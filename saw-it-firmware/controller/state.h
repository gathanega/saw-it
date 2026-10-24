// state.h — data yang bertahan melewati deep sleep (RTC memory; hilang bila daya putus).
#pragma once
#include <Arduino.h>

struct LastReading {
  bool valid;
  uint32_t ts;
  float ph, ec, tds, doo, temp, turb;
  int bat;
  float vbat;
};

RTC_DATA_ATTR LastReading g_last = {false, 0, NAN, NAN, NAN, NAN, NAN, NAN, -1, 0};
RTC_DATA_ATTR uint32_t g_nextDue = 0;       // epoch jadwal ukur berikutnya (0 = segera)
RTC_DATA_ATTR uint8_t  g_failCount = 0;     // gagal WiFi berturut-turut
RTC_DATA_ATTR uint32_t g_lastNtp = 0;       // epoch sinkron NTP terakhir
RTC_DATA_ATTR bool     g_lastWifiOk = false;
RTC_DATA_ATTR bool     g_apSwLatch = false;   // AP switch ditutup karena idle sementara switch masih ON; aktif lagi setelah switch di-OFF-kan
RTC_DATA_ATTR uint32_t g_pendingCache = 0;  // jumlah data belum terkirim (untuk LCD)
