// storage.h — microSD: antrean data (write-ahead) + arsip CSV.
#pragma once
#include <SPI.h>
#include <SD.h>
#include "config.h"
#include "protocol.h"
#include "hw.h"

bool sdOk = false;
static const char* PENDING_FILE = "/pending.ndjson";   // satu objek JSON per baris, belum terkirim
static const char* PENDING_TMP  = "/pending.tmp";
static const char* ARCHIVE_FILE = "/data.csv";         // arsip semua pengukuran
static const char* REJECT_FILE  = "/rejected.ndjson";  // data yang ditolak server (422)

bool sdInit() {
  SPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
  sdOk = SD.begin(PIN_SD_CS, SPI, 4000000);
  Serial.println(sdOk ? "[sd] siap" : "[sd] TIDAK terdeteksi (data tidak di-buffer)");
  return sdOk;
}

bool sdAppendLine(const char* path, const String& line) {
  if (!sdOk) return false;
  File f = SD.open(path, FILE_APPEND);
  if (!f) return false;
  f.println(line);
  f.close();
  return true;
}

uint32_t pendingCount() {
  if (!sdOk || !SD.exists(PENDING_FILE)) return 0;
  File f = SD.open(PENDING_FILE, FILE_READ);
  if (!f) return 0;
  uint32_t n = 0;
  while (f.available()) { String l = f.readStringUntil('\n'); l.trim(); if (l.length() > 2) n++; }
  f.close();
  return n;
}

void archiveCsv(uint32_t ts, float ph, float ec, float tds, float doo, float temp, float turb) {
  if (!sdOk) return;
  bool fresh = !SD.exists(ARCHIVE_FILE);
  File f = SD.open(ARCHIVE_FILE, FILE_APPEND);
  if (!f) return;
  if (fresh) f.println("ts,ph,ec_us_cm,tds_ppm,do_mg_l,temp_c,turb_ntu");
  auto col = [&](float v, int d) { return isnan(v) ? String("") : String(v, d); };
  f.println((ts ? isoUtc(ts) : String("")) + "," + col(ph, 2) + "," + col(ec, 1) + "," + col(tds, 1) + "," +
            col(doo, 2) + "," + col(temp, 2) + "," + col(turb, 1));
  f.close();
}

// Buang baris [0, consumedBytes) dari antrean; sisanya disalin ulang.
void pendingConsume(uint32_t consumedBytes) {
  if (!sdOk || !SD.exists(PENDING_FILE) || consumedBytes == 0) return;
  File src = SD.open(PENDING_FILE, FILE_READ);
  if (!src) return;
  if (consumedBytes >= src.size()) { src.close(); SD.remove(PENDING_FILE); return; }
  File dst = SD.open(PENDING_TMP, FILE_WRITE);
  if (!dst) { src.close(); return; }
  src.seek(consumedBytes);
  uint8_t buf[256];
  while (src.available()) { int n = src.read(buf, sizeof(buf)); if (n <= 0) break; dst.write(buf, n); }
  src.close(); dst.close();
  SD.remove(PENDING_FILE);
  SD.rename(PENDING_TMP, PENDING_FILE);
}
