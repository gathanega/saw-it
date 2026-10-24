// lcd_ui.h — LCD 16x2 I2C (aktif 30 detik setelah tombol LCD ditekan).
#pragma once
#include <LiquidCrystal_I2C.h>
#include "config.h"
#include "hw.h"
#include "state.h"
#include "settings.h"

void runCycle(bool manual);     // didefinisikan di controller.ino

LiquidCrystal_I2C* lcd = nullptr;

bool lcdBegin() {
  uint8_t addr = 0;
  for (uint8_t a : {0x27, 0x3F}) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) { addr = a; break; }
  }
  if (!addr) return false;
  if (!lcd) lcd = new LiquidCrystal_I2C(addr, 16, 2);
  lcd->init();
  lcd->backlight();
  lcd->clear();
  return true;
}

void lcdRow(uint8_t row, const String& s) {
  if (!lcd) return;
  char b[17]; snprintf(b, sizeof(b), "%-16.16s", s.c_str());
  lcd->setCursor(0, row);
  lcd->print(b);
}
void lcdMsg(const String& a, const String& b = "") { lcdRow(0, a); lcdRow(1, b); }

String fmtVal(float v, int d) { return isnan(v) ? String("--") : String(v, d); }

#define LCD_PAGES 4
void lcdPage(int page) {
  if (!lcd) return;
  const LastReading& r = g_last;
  switch (page) {
    case 0:
      if (!r.valid) { lcdMsg("Belum ada data", "Tekan tombol ukur"); break; }
      lcdRow(0, "pH " + fmtVal(r.ph, 2) + " T" + fmtVal(r.temp, 1) + "C");
      lcdRow(1, "EC" + fmtVal(r.ec, 0) + " TDS" + fmtVal(r.tds, 0));
      break;
    case 1:
      if (!r.valid) { lcdMsg("Belum ada data", ""); break; }
      lcdRow(0, "Turb " + fmtVal(r.turb, 1) + " NTU");
      lcdRow(1, "DO " + fmtVal(r.doo, 2) + " mg/L");
      break;
    case 2: {
      float vb = readVbat();
      lcdRow(0, "Bat " + String(vbatToPct(vb)) + "% " + String(vb, 2) + "V");
      lcdRow(1, "SD:" + String(g_pendingCache) + " WiFi:" + (g_lastWifiOk ? "OK" : "--"));
      break;
    }
    default: {
      uint32_t e = nowEpoch();
      if (!e) { lcdMsg("Jam belum valid", cfg.devId); break; }
      time_t t = e + LCD_TZ_OFFSET_H * 3600; struct tm tm; gmtime_r(&t, &tm);
      char b[20]; strftime(b, sizeof(b), "%d/%m/%y %H:%M", &tm);
      lcdRow(0, b);
      lcdRow(1, cfg.devId + " " + cfg.loc);
    }
  }
}

// Sesi LCD: tombol LCD = halaman berikut; tombol ukur = ukur sekarang.
void lcdSession() {
  g_keep5v = true;
  rail5v(true);
  delay(RAIL_SETTLE_MS);
  if (!lcdBegin()) { g_keep5v = false; rail5v(false); Serial.println("[lcd] tidak terdeteksi"); return; }

  waitRelease(PIN_BTN_LCD);
  int page = 0;
  lcdPage(page);
  uint32_t start = millis(), lastPage = millis();
  while (millis() - start < LCD_ON_MS) {
    if (btn(PIN_BTN_LCD)) {
      waitRelease(PIN_BTN_LCD);
      page = (page + 1) % LCD_PAGES; lcdPage(page);
      start = lastPage = millis();
    }
    if (btn(PIN_BTN_MEAS)) {
      waitRelease(PIN_BTN_MEAS);
      lcdMsg("Mengukur...", "tunggu +-1 menit");
      runCycle(true);
      page = 0; lcdPage(page);
      start = lastPage = millis();
    }
    if (millis() - lastPage > LCD_PAGE_MS) {
      page = (page + 1) % LCD_PAGES; lcdPage(page); lastPage = millis();
    }
    delay(20);
  }
  lcd->clear(); lcd->noBacklight();
  g_keep5v = false;
  rail5v(false);
}
