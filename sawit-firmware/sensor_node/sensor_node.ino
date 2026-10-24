/*
 * AirTracking — Firmware SENSOR NODE (ESP32 #2)
 * ---------------------------------------------
 * Tugas: bangun dari deep sleep saat jalur WAKE dipulsa controller, baca pH, EC/TDS,
 * suhu, kekeruhan (dan DO di fase 2), kirim hasilnya ke controller lewat UART, tidur lagi.
 * Node TIDAK memakai WiFi.
 *
 * Library (Library Manager Arduino IDE): "Adafruit ADS1X15", "OneWire", "DallasTemperature".
 * Board: "ESP32 Dev Module" (esp32 by Espressif, 2.0.x atau 3.x).
 */
#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_ADS1X15.h>
#include "driver/gpio.h"
#include "esp_sleep.h"
#include "config.h"
#include "protocol.h"

Adafruit_ADS1115 ads;
OneWire oneWire(PIN_ONEWIRE);
DallasTemperature ds(&oneWire);
Preferences prefs;
bool adsReady = false;
bool holdMode = false;      // true selama controller dalam mode AP (node tetap bangun)

// ---------- Kalibrasi (disimpan di NVS node) ----------
struct Cal {
  float v7 = PH_V7_DEFAULT, v4 = PH_V4_DEFAULT, tcal = 25.0f;
  float kec = 1.0f;
  float turbV0 = TURB_V0_DEFAULT;
} cal;

void calLoad() {
  prefs.begin("cal", true);
  cal.v7 = prefs.getFloat("v7", PH_V7_DEFAULT);
  cal.v4 = prefs.getFloat("v4", PH_V4_DEFAULT);
  cal.tcal = prefs.getFloat("tcal", 25.0f);
  cal.kec = prefs.getFloat("kec", 1.0f);
  cal.turbV0 = prefs.getFloat("tv0", TURB_V0_DEFAULT);
  prefs.end();
}
void calSave() {
  prefs.begin("cal", false);
  prefs.putFloat("v7", cal.v7);
  prefs.putFloat("v4", cal.v4);
  prefs.putFloat("tcal", cal.tcal);
  prefs.putFloat("kec", cal.kec);
  prefs.putFloat("tv0", cal.turbV0);
  prefs.end();
}
void calReset() {
  prefs.begin("cal", false);
  prefs.clear();
  prefs.end();
  cal = Cal();
}

// ---------- Link ke controller ----------
void sendLine(const String& s) {
  Serial1.print(s);
  Serial1.print('\n');
  Serial1.flush();
}
void sendErr(const char* msg) { sendLine(String("{\"ok\":0,") + jStr("err", msg) + "}"); }

bool readLine(String& out, uint32_t timeoutMs) {
  static String buf;
  uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    while (Serial1.available()) {
      char c = (char)Serial1.read();
      if (c == '\n') { out = buf; buf = ""; out.trim(); if (out.length()) return true; }
      else if (c != '\r' && buf.length() < 200) buf += c;
    }
    delay(2);
  }
  return false;
}

// ---------- Sensor ----------
bool initSensors() {
  if (adsReady) return true;
  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(100000);
  if (!ads.begin(ADS_ADDR, &Wire)) return false;
  ads.setGain(GAIN_ONE);                    // ±4.096 V; maks input aman 3.3 V
  ads.setDataRate(RATE_ADS1115_128SPS);
  ds.begin();
  ds.setResolution(12);
  adsReady = true;
  return true;
}

// Rata-rata bagian tengah dari N sampel terurut (membuang pencilan), hasil dalam mV.
float readMv(uint8_t ch) {
  float s[N_SAMPLES];
  for (int i = 0; i < N_SAMPLES; i++) {
    s[i] = ads.computeVolts(ads.readADC_SingleEnded(ch)) * 1000.0f;
    delay(15);
  }
  for (int i = 1; i < N_SAMPLES; i++) {                // insertion sort
    float v = s[i]; int j = i - 1;
    while (j >= 0 && s[j] > v) { s[j + 1] = s[j]; j--; }
    s[j + 1] = v;
  }
  int a = N_SAMPLES / 5, b = N_SAMPLES - a;
  float sum = 0; for (int i = a; i < b; i++) sum += s[i];
  return sum / (b - a);
}

float readTemp() {
  for (int i = 0; i < 3; i++) {
    ds.requestTemperatures();
    float t = ds.getTempCByIndex(0);
    if (t > -55.0f && t < 125.0f && t != 85.0f) return t;   // 85.0 = nilai awal sebelum konversi
    delay(100);
  }
  return NAN;
}

float phFromMv(float mv, float t) {
  float slope = (cal.v4 - cal.v7) / (4.0f - 7.0f);          // mV per pH (negatif)
  if (fabsf(slope) < 1.0f) return NAN;
  float tk = isnan(t) ? cal.tcal : t;
  float comp = (cal.tcal + 273.15f) / (tk + 273.15f);       // kemiringan Nernst ~ suhu mutlak
  return constrain(7.0f + (mv - cal.v7) / slope * comp, 0.0f, 14.0f);
}

float ecRawMs(float mv) { return 1000.0f * mv / EC_RES2 / EC_REF; }   // mS/cm sebelum K

float ecFromMv(float mv, float t) {                         // hasil µS/cm pada 25 °C
  float tt = isnan(t) ? 25.0f : t;
  float ms = ecRawMs(mv) * cal.kec / (1.0f + EC_TEMP_COEF * (tt - 25.0f));
  return max(0.0f, ms * 1000.0f);
}

float turbVolt(float mvAdc) { return mvAdc / 1000.0f * TURB_DIV_RATIO + (TURB_V0_DEFAULT - cal.turbV0); }

float turbFromVolt(float v) {                               // NTU (indikatif) — rumus wiki DFRobot SEN0189
  if (v < 2.5f) return 3000.0f;
  float ntu = -1120.4f * v * v + 5742.3f * v - 4352.9f;
  return constrain(ntu, 0.0f, 3000.0f);
}

#if USE_DO
uint16_t crc16(const uint8_t* d, size_t n) {
  uint16_t c = 0xFFFF;
  for (size_t i = 0; i < n; i++) {
    c ^= d[i];
    for (int b = 0; b < 8; b++) c = (c & 1) ? (c >> 1) ^ 0xA001 : (c >> 1);
  }
  return c;
}
bool modbusRead(uint8_t addr, uint16_t reg, uint16_t n, uint16_t* out) {
  uint8_t req[8] = {addr, 0x03, (uint8_t)(reg >> 8), (uint8_t)reg, (uint8_t)(n >> 8), (uint8_t)n, 0, 0};
  uint16_t c = crc16(req, 6); req[6] = c & 0xFF; req[7] = c >> 8;
  while (Serial2.available()) Serial2.read();
  digitalWrite(PIN_RS485_DE, HIGH); delayMicroseconds(100);
  Serial2.write(req, 8); Serial2.flush();
  digitalWrite(PIN_RS485_DE, LOW);
  size_t need = 5 + 2 * n, got = 0; uint8_t rx[32];
  uint32_t t0 = millis();
  while (millis() - t0 < 600 && got < need && got < sizeof(rx)) {
    if (Serial2.available()) rx[got++] = Serial2.read(); else delay(1);
  }
  if (got < need || rx[0] != addr || rx[1] != 0x03 || rx[2] != 2 * n) return false;
  if (crc16(rx, need - 2) != (uint16_t)(rx[need - 2] | (rx[need - 1] << 8))) return false;
  for (int i = 0; i < n; i++) out[i] = (rx[3 + 2 * i] << 8) | rx[4 + 2 * i];
  return true;
}
float readDO() {
  uint16_t r[DO_MB_COUNT];
  for (int i = 0; i < 5; i++) {
    if (modbusRead(DO_MB_ADDR, DO_MB_REG, DO_MB_COUNT, r)) return r[DO_MB_IDX] / DO_MB_SCALE;
    delay(500);
  }
  return NAN;
}
#endif

// ---------- Perintah ----------
// live=true: pembacaan cepat untuk halaman mode AP (tanpa pemanasan, probe dianggap sudah basah/stabil).
void cmdMeasure(bool withDo, bool live = false) {
  if (!initSensors()) { sendErr("ADS1115 tidak terdeteksi (cek rail 5V/LDO dan I2C)"); return; }
  if (!live) delay(WARMUP_MS + (withDo ? WARMUP_DO_MS : 0));

  float t = readTemp();
  float mvPh = readMv(ADS_CH_PH);
  float mvEc = readMv(ADS_CH_EC);
  float mvTb = readMv(ADS_CH_TURB);
  float doV = NAN;
#if USE_DO
  if (withDo) doV = readDO();
#endif
  float ec = ecFromMv(mvEc, t);

  String o = "{\"ok\":1,";
  o += jNum("ph", phFromMv(mvPh, t), 2) + ",";
  o += jNum("ec", ec, 1) + ",";
  o += jNum("tds", ec * TDS_FACTOR, 1) + ",";
  o += jNum("do", doV, 2) + ",";
  o += jNum("temp", t, 2) + ",";
  o += jNum("turb", turbFromVolt(turbVolt(mvTb)), 1) + ",";
  o += jNum("mv_ph", mvPh, 1) + "," + jNum("mv_ec", mvEc, 1) + "," + jNum("mv_turb", mvTb, 1);
  o += "}";
  sendLine(o);
}

void cmdCalPh(float buffer) {
  if (!initSensors()) { sendErr("ADS1115 tidak terdeteksi"); return; }
  delay(WARMUP_CAL_MS);
  float t = readTemp();
  float mv = readMv(ADS_CH_PH);
  if (buffer == 7.0f) {
    if (mv < 1100 || mv > 1900) { sendErr("Tegangan di luar rentang pH 7 (probe di buffer 7?)"); return; }
    cal.v7 = mv;
  } else if (buffer == 4.0f) {
    if (mv < 1600 || mv > 2500) { sendErr("Tegangan di luar rentang pH 4 (probe di buffer 4?)"); return; }
    cal.v4 = mv;
  } else { sendErr("Buffer pH hanya 4 atau 7"); return; }
  if (!isnan(t)) cal.tcal = t;
  calSave();
  sendLine(String("{\"ok\":1,") + jNum("mv", mv, 1) + "," + jNum("v7", cal.v7, 1) + "," + jNum("v4", cal.v4, 1) + "}");
}

void cmdCalEc(float uS) {
  if (!initSensors()) { sendErr("ADS1115 tidak terdeteksi"); return; }
  if (uS < 100) uS = 1413;
  delay(WARMUP_CAL_MS);
  float t = readTemp();
  float tt = isnan(t) ? 25.0f : t;
  float raw = ecRawMs(readMv(ADS_CH_EC));
  if (raw < 0.05f) { sendErr("Sinyal EC terlalu kecil (probe di larutan 1413?)"); return; }
  float targetMs = (uS / 1000.0f) * (1.0f + EC_TEMP_COEF * (tt - 25.0f));
  float k = targetMs / raw;
  if (k < 0.2f || k > 5.0f) { sendErr("Faktor K di luar wajar (0.2-5); cek probe/larutan"); return; }
  cal.kec = k;
  calSave();
  sendLine(String("{\"ok\":1,") + jNum("k", k, 4) + "}");
}

void cmdCalTurb() {
  if (!initSensors()) { sendErr("ADS1115 tidak terdeteksi"); return; }
  delay(WARMUP_CAL_MS);
  float v = readMv(ADS_CH_TURB) / 1000.0f * TURB_DIV_RATIO;
  if (v < 3.0f || v > 5.2f) { sendErr("Tegangan air jernih di luar 3.0-5.2 V (probe di aquades?)"); return; }
  cal.turbV0 = v;
  calSave();
  sendLine(String("{\"ok\":1,") + jNum("v0", v, 3) + "}");
}

void cmdStatus() {
  bool ads = initSensors();
  String o = String("{\"ok\":1,") + jStr("fw", FW_VERSION) + "," + jInt("ads", ads ? 1 : 0) + ",";
  o += jNum("v7", cal.v7, 1) + "," + jNum("v4", cal.v4, 1) + "," + jNum("k", cal.kec, 4) + "," + jNum("v0", cal.turbV0, 3) + ",";
  o += jInt("hold", holdMode ? 1 : 0) + "}";
  sendLine(o);
}

// return true bila node boleh tidur
bool handle(const String& line) {
  String c;
  if (!jGetStr(line, "c", c)) { sendErr("perintah tidak dikenal"); return false; }
  float v = 0;
  if (c == "ping")           sendLine(String("{\"ok\":1,\"pong\":1,") + jStr("fw", FW_VERSION) + "}");
  else if (c == "measure")   { float d = 0; jGetNum(line, "do", d); cmdMeasure(d > 0.5f); }
  else if (c == "live")      cmdMeasure(false, true);
  else if (c == "status")    cmdStatus();
  else if (c == "hold")      { jGetNum(line, "v", v); holdMode = v > 0.5f; sendLine(String("{\"ok\":1,") + jInt("hold", holdMode ? 1 : 0) + "}"); }
  else if (c == "cal_ph")    { jGetNum(line, "v", v); cmdCalPh(v); }
  else if (c == "cal_ec")    { jGetNum(line, "v", v); cmdCalEc(v); }
  else if (c == "cal_turb")  cmdCalTurb();
  else if (c == "cal_reset") { calReset(); sendLine("{\"ok\":1}"); }
  else if (c == "sleep")     { sendLine("{\"ok\":1}"); return true; }
  else sendErr("perintah tidak dikenal");
  return false;
}

void goToSleep() {
  Serial.println("[node] tidur");
  Serial.flush();
  uint32_t t0 = millis();
  while (digitalRead(PIN_WAKE) && millis() - t0 < 1500) delay(5);   // jangan tidur saat WAKE masih HIGH
#if USE_DO
  digitalWrite(PIN_RS485_DE, LOW);
  gpio_hold_en((gpio_num_t)PIN_RS485_DE);
  gpio_deep_sleep_hold_en();
#endif
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_WAKE, 1);
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_WAKE, INPUT_PULLDOWN);
#if USE_DO
  gpio_hold_dis((gpio_num_t)PIN_RS485_DE);
  pinMode(PIN_RS485_DE, OUTPUT);
  digitalWrite(PIN_RS485_DE, LOW);
  Serial2.begin(DO_MB_BAUD, SERIAL_8N1, PIN_RS485_RX, PIN_RS485_TX);
#endif
  Serial1.begin(LINK_BAUD, SERIAL_8N1, PIN_LINK_RX, PIN_LINK_TX);
  calLoad();
  Serial.printf("[node] AirTracking sensor node fw %s, wake=%d\n", FW_VERSION, (int)esp_sleep_get_wakeup_cause());

  sendLine(String("{\"rdy\":1,") + jStr("fw", FW_VERSION) + "}");

  uint32_t last = millis();
  String line;
  while (millis() - last < (holdMode ? HOLD_IDLE_MS : IDLE_TIMEOUT_MS)) {
    if (readLine(line, 100)) {
      Serial.println("[node] <- " + line);
      if (handle(line)) break;
      last = millis();
    }
  }
  goToSleep();
}

void loop() {}
