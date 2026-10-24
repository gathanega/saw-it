// hw.h — rail daya, tombol, baterai, jam (DS3231 + NTP).
#pragma once
#include <Wire.h>
#include <RTClib.h>
#include <time.h>
#include "driver/gpio.h"
#include "config.h"
#include "settings.h"

// ---------- Rail daya ----------
bool g_keep5v = false;     // true selama sesi LCD berjalan (LCD memakai rail 5V)

void pinsInit() {
  const gpio_num_t held[] = {(gpio_num_t)PIN_WAKE, (gpio_num_t)PIN_EN_5V, (gpio_num_t)PIN_EN_12V};
  for (auto p : held) gpio_hold_dis(p);          // lepas penahan dari deep sleep sebelumnya
  pinMode(PIN_WAKE, OUTPUT);   digitalWrite(PIN_WAKE, LOW);
  pinMode(PIN_EN_5V, OUTPUT);  digitalWrite(PIN_EN_5V, LOW);
  pinMode(PIN_EN_12V, OUTPUT); digitalWrite(PIN_EN_12V, LOW);
  pinMode(PIN_BTN_MEAS, INPUT);
  pinMode(PIN_BTN_LCD, INPUT);
  pinMode(PIN_SW_AP, INPUT);
  analogSetPinAttenuation(PIN_VBAT, ADC_11db);
}

void rail5v(bool on) { digitalWrite(PIN_EN_5V, on ? HIGH : LOW); }
void rail12v(bool on) { digitalWrite(PIN_EN_12V, on ? HIGH : LOW); }

void railsOn(bool withDo) {
  rail5v(true);
  if (withDo) { delay(100); rail12v(true); }
  delay(RAIL_SETTLE_MS + (withDo ? 500 : 0));
}
void railsOff() {
  rail12v(false);
  if (!g_keep5v) rail5v(false);
}

// Tahan level semua pin kendali selama deep sleep (rail harus tetap mati).
void holdPinsForSleep() {
  digitalWrite(PIN_WAKE, LOW); digitalWrite(PIN_EN_5V, LOW); digitalWrite(PIN_EN_12V, LOW);
  gpio_hold_en((gpio_num_t)PIN_WAKE);
  gpio_hold_en((gpio_num_t)PIN_EN_5V);
  gpio_hold_en((gpio_num_t)PIN_EN_12V);
  gpio_deep_sleep_hold_en();
}

// ---------- Tombol ----------
bool btn(int pin) { return digitalRead(pin) == HIGH; }
void waitRelease(int pin, uint32_t maxMs = 4000) {
  uint32_t t0 = millis();
  while (btn(pin) && millis() - t0 < maxMs) delay(10);
  delay(40);
}
// Switch MODE AP (aktif HIGH). Stabil 60 ms agar tidak terpicu pantulan kontak.
bool apSwitchOn() {
  if (!btn(PIN_SW_AP)) return false;
  delay(60);
  return btn(PIN_SW_AP);
}
bool heldFor(int pin, uint32_t ms) {            // true bila tombol ditahan terus selama ms
  uint32_t t0 = millis();
  while (btn(pin)) { if (millis() - t0 >= ms) return true; delay(10); }
  return false;
}

// ---------- Baterai ----------
float readVbat() {
  uint32_t s = 0;
  for (int i = 0; i < 16; i++) { s += analogReadMilliVolts(PIN_VBAT); delay(2); }
  return (s / 16.0f) * 2.0f * VBAT_SCALE / 1000.0f;     // pembagi 1:2
}
int vbatToPct(float v) {
  static const float V[] = {3.30f, 3.50f, 3.60f, 3.70f, 3.80f, 3.90f, 4.00f, 4.10f, 4.20f};
  static const float P[] = {0, 5, 15, 35, 55, 70, 82, 92, 100};
  if (v <= V[0]) return 0;
  if (v >= V[8]) return 100;
  for (int i = 1; i < 9; i++)
    if (v <= V[i]) return (int)(P[i - 1] + (P[i] - P[i - 1]) * (v - V[i - 1]) / (V[i] - V[i - 1]) + 0.5f);
  return 100;
}

// ---------- Jam ----------
RTC_DS3231 rtc;
bool rtcOk = false;

uint32_t nowEpoch() {
  if (rtcOk) {
    uint32_t e = rtc.now().unixtime();
    if (e >= MIN_VALID_EPOCH) return e;
  }
  time_t t = time(nullptr);                      // jam sistem (NTP / dijaga RTC internal saat deep sleep)
  return (uint32_t)t >= MIN_VALID_EPOCH ? (uint32_t)t : 0;
}
bool timeValid() { return nowEpoch() != 0; }

String isoUtc(uint32_t e) {                      // 2026-10-19T09:30:00Z
  time_t t = e; struct tm tm; gmtime_r(&t, &tm);
  char b[24]; strftime(b, sizeof(b), "%Y-%m-%dT%H:%M:%SZ", &tm);
  return String(b);
}
String dateUtc(uint32_t e) {                     // 2026-10-19
  time_t t = e; struct tm tm; gmtime_r(&t, &tm);
  char b[12]; strftime(b, sizeof(b), "%Y-%m-%d", &tm);
  return String(b);
}

bool ntpSync(uint32_t timeoutMs = 8000) {        // butuh WiFi tersambung
  configTime(0, 0, "pool.ntp.org", "time.google.com", "id.pool.ntp.org");
  uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    time_t t = time(nullptr);
    if ((uint32_t)t >= MIN_VALID_EPOCH) {
      if (rtcOk) rtc.adjust(DateTime((uint32_t)t));
      Serial.println("[ntp] sinkron: " + isoUtc((uint32_t)t));
      return true;
    }
    delay(200);
  }
  return false;
}
