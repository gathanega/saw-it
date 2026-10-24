/*
 * AirTracking — Firmware CONTROLLER (ESP32 #1)
 * --------------------------------------------
 * Tugas: jadwal ukur (deep sleep + timer), nyalakan rail daya, bangunkan sensor node,
 * terima data, simpan ke microSD (buffer), sambung WiFi lalu kirim ke server AirTracking
 * (HTTP POST /api/ingest, atau MQTT), tampilkan di LCD, dan sediakan MODE AP untuk setup.
 *
 * Library (Library Manager Arduino IDE):
 *   "RTClib" (Adafruit), "LiquidCrystal_I2C" (Frank de Brabander), "PubSubClient" (Nick O'Leary;
 *   tidak perlu bila USE_MQTT 0). Sisanya bawaan core ESP32 (WiFi, WebServer, DNSServer, HTTPClient, SD, Preferences).
 * Board: "ESP32 Dev Module" (esp32 by Espressif, 2.0.x atau 3.x).
 *
 * Cara pakai di lapangan:
 *   - SWITCH AP (GPIO13): ON = mode AP (WiFi "AirTrack-<ID>", buka 192.168.4.1; satu halaman untuk Controller
 *     DAN Sensor node). OFF = kembali ke mode Internet (langsung ukur & kirim, lalu tidur).
 *   - Tombol UKUR (GPIO32): tekan = ukur & kirim sekarang.
 *   - Tombol LCD  (GPIO33): tekan = layar menyala 30 detik; tekan lagi = halaman berikut.
 *   - Alat yang belum dikonfigurasi langsung masuk mode AP walau switch OFF; simpan = restart.
 */
#include <Arduino.h>
#include <Wire.h>
#include "esp_sleep.h"
#include "config.h"
#include "protocol.h"
#include "state.h"
#include "settings.h"
#include "hw.h"
#include "node_link.h"
#include "storage.h"
#include "net.h"
#include "lcd_ui.h"
#include "ap_page.h"
#include "ap_mode.h"

static float clampf(float v, float lo, float hi) { return isnan(v) ? v : (v < lo ? lo : (v > hi ? hi : v)); }

static bool syncClock() {
  if (ntpSync()) { g_lastNtp = nowEpoch(); return true; }
  return false;
}

// Satu rekaman JSON dalam format web: {"ts":..,"ph":..,"ec":..,"tds":..,"do":..,"temp":..,"turb":..}
static String makeRecord(uint32_t ts, const NodeReading& r) {
  String s = "{";
  if (ts) s += jStr("ts", isoUtc(ts)) + ",";
  s += jNum("ph", clampf(r.ph, 0, 14), 2) + ",";
  s += jNum("ec", clampf(r.ec, 0, 100000), 1) + ",";
  s += jNum("tds", clampf(r.tds, 0, 100000), 1) + ",";
  s += jNum("do", clampf(r.doo, 0, 60), 2) + ",";
  s += jNum("temp", clampf(r.temp, -5, 100), 2) + ",";
  s += jNum("turb", clampf(r.turb, 0, 10000), 1) + "}";
  return s;
}

// Satu siklus: ukur -> simpan -> kirim. manual = dipicu tombol/halaman AP.
void runCycle(bool manual) {
  uint32_t startMs = millis();
  Serial.printf("[cycle] mulai (%s)\n", manual ? "manual" : "terjadwal");

  bool wifiUp = false;
  if (cfgConfigured() && !timeValid()) {            // jam belum benar (RTC baru/baterai habis): sinkron dulu
    wifiUp = wifiConnect();
    if (wifiUp) syncClock();
  }

  float vb = readVbat();
  g_batPct = vbatToPct(vb);

  NodeReading r; String err;
  String rec; bool have = false;
  if (takeMeasurement(r, err)) {
    uint32_t ts = nowEpoch();
    rec = makeRecord(ts, r);
    have = true;
    g_last = {true, ts, r.ph, r.ec, r.tds, r.doo, r.temp, r.turb, g_batPct, vb};
    archiveCsv(ts, r.ph, r.ec, r.tds, r.doo, r.temp, r.turb);
    if (sdOk) sdAppendLine(PENDING_FILE, rec);      // tulis dulu ke SD, baru kirim (aman bila daya/WiFi putus)
    Serial.println("[cycle] data: " + rec);
  } else {
    Serial.println("[cycle] ukur GAGAL: " + err);
  }

  if (cfgConfigured()) {
    if (!wifiUp) wifiUp = wifiConnect();
    if (wifiUp) {
      g_lastWifiOk = true; g_failCount = 0;
      if (timeValid() && (g_lastNtp == 0 || nowEpoch() - g_lastNtp > NTP_RESYNC_S)) syncClock();
      uint32_t sent = flushPending((have && !sdOk) ? &rec : nullptr);
      Serial.printf("[cycle] terkirim: %u data\n", (unsigned)sent);
      if (g_lastNetErr.length()) { Serial.println("[cycle] catatan: " + g_lastNetErr); g_lastNetErr = ""; }
      wifiOff();
    } else {
      g_lastWifiOk = false;
      if (g_failCount < 255) g_failCount++;
      Serial.printf("[cycle] WiFi gagal (%u kali berturut-turut); data tetap di SD\n", g_failCount);
    }
  }
  g_pendingCache = pendingCount();

  uint32_t e = nowEpoch();                          // jadwal berikutnya dihitung dari awal siklus
  g_nextDue = e ? e - (millis() - startMs) / 1000 + cfg.intervalS : 0;
}

static void deepSleepNow() {
  uint32_t sleepS = cfg.intervalS;
  uint32_t e = nowEpoch();
  if (e && g_nextDue) sleepS = g_nextDue > e ? g_nextDue - e : 5;
  sleepS = constrain(sleepS, 5UL, (uint32_t)cfg.intervalS);

  Serial.printf("[sleep] tidur %u detik\n", (unsigned)sleepS);
  Serial.flush();
  wifiOff();
  g_keep5v = false;
  railsOff();
  rail5v(false);
  if (sdOk) SD.end();
  waitRelease(PIN_BTN_MEAS);                        // tombol yang masih ditekan akan membangunkan seketika
  waitRelease(PIN_BTN_LCD);
  holdPinsForSleep();
  esp_sleep_enable_timer_wakeup((uint64_t)sleepS * 1000000ULL);
  uint64_t wakeMask = (1ULL << PIN_BTN_MEAS) | (1ULL << PIN_BTN_LCD);
  if (!btn(PIN_SW_AP)) wakeMask |= (1ULL << PIN_SW_AP);   // switch AP di-ON-kan saat tidur = bangun & buka AP (bila sudah ON, jangan dipasang: bangun terus)
  esp_sleep_enable_ext1_wakeup(wakeMask, ESP_EXT1_WAKEUP_ANY_HIGH);
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  delay(50);
  pinsInit();
  Wire.begin(PIN_SDA, PIN_SCL);
  rtcOk = rtc.begin();
  cfgLoad();
  sdInit();
  linkBegin();

  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  bool ext1 = cause == ESP_SLEEP_WAKEUP_EXT1;
  uint64_t mask = ext1 ? esp_sleep_get_ext1_wakeup_status() : 0;
  bool btnMeas = mask & (1ULL << PIN_BTN_MEAS);
  bool btnLcd = mask & (1ULL << PIN_BTN_LCD);
  bool coldBoot = (cause == ESP_SLEEP_WAKEUP_UNDEFINED);
  (void)coldBoot;                      // hanya dipakai bila USE_AP_BTN_HOLD 1
  Serial.printf("[boot] AirTracking controller fw %s, wake=%d, rtc=%d, sd=%d, cfg=%d\n",
                FW_VERSION, (int)cause, rtcOk, sdOk, cfgConfigured());

  // ----- Mode AP? -----
  bool apRan = false;
  bool afterSwitchAp = false;                                 // AP switch baru selesai: langsung ukur & kirim
  bool sw = apSwitchOn();
  if (!sw) g_apSwLatch = false;                               // switch sudah OFF: AP switch boleh dibuka lagi

  if (sw && !g_apSwLatch) {
    bool off = runApMode(AP_SWITCH_IDLE_MS, false, true);     // AP selama switch ON
    if (!off) g_apSwLatch = true;                             // idle, switch masih ON: jangan buka AP lagi tiap bangun
    apRan = afterSwitchAp = true;
  } else if (!cfgConfigured()) {
    runApMode(AP_IDLE_MS, true);                              // belum disetel: tetap AP sampai disimpan (lalu restart)
    apRan = true;
#if USE_AP_BTN_HOLD
  } else if ((btnMeas || (coldBoot && btn(PIN_BTN_MEAS))) && heldFor(PIN_BTN_MEAS, BTN_AP_HOLD_MS)) {
    runApMode(AP_IDLE_MS, false);                             // cadangan: tombol ukur ditahan 3 detik
    apRan = true;
#endif
  } else if (g_failCount >= AP_FALLBACK_FAILS) {
    runApMode(AP_FALLBACK_MS, false);                         // WiFi gagal berulang: beri kesempatan setup ulang
    g_failCount = 0;
    apRan = true;
  }

  if (btnLcd && !apRan) lcdSession();

  uint32_t now = nowEpoch();
  bool due = !ext1 || btnMeas || (now && g_nextDue && now >= g_nextDue) || afterSwitchAp;
  if (due && (!apRan || afterSwitchAp)) runCycle(btnMeas || afterSwitchAp);

  // Switch AP di-ON-kan saat siklus ukur/kirim berjalan: buka AP sekarang, bukan menunggu bangun berikutnya
  if (!g_apSwLatch && apSwitchOn()) {
    bool off = runApMode(AP_SWITCH_IDLE_MS, false, true);
    if (!off) g_apSwLatch = true;
    else if (cfgConfigured()) runCycle(true);
  }

  deepSleepNow();
}

void loop() {}
