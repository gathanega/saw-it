// settings.h — konfigurasi alat (NVS), diatur lewat halaman mode AP.
#pragma once
#include <Preferences.h>
#include "config.h"

struct Settings {
  String ssid, pass;
  String devId, loc, key;
  String proto = "http";        // http | mqtt
  String server;                // https://domain/api/ingest  atau  host[:port] untuk MQTT
  uint32_t intervalS = DEFAULT_INTERVAL_S;
  String calPh, calEc;          // tanggal kalibrasi terakhir (YYYY-MM-DD), dikirim sebagai cal_ph/cal_ec
} cfg;

void cfgLoad() {
  Preferences p;
  p.begin("airtrack", true);
  cfg.ssid = p.getString("ssid", "");
  cfg.pass = p.getString("pass", "");
  cfg.devId = p.getString("id", "");
  cfg.loc = p.getString("loc", "");
  cfg.key = p.getString("key", "");
  cfg.proto = p.getString("proto", "http");
  cfg.server = p.getString("server", "");
  cfg.intervalS = p.getUInt("iv", DEFAULT_INTERVAL_S);
  cfg.calPh = p.getString("calph", "");
  cfg.calEc = p.getString("calec", "");
  p.end();
  cfg.intervalS = constrain(cfg.intervalS, (uint32_t)MIN_INTERVAL_S, (uint32_t)MAX_INTERVAL_S);
}

void cfgSave() {
  Preferences p;
  p.begin("airtrack", false);
  p.putString("ssid", cfg.ssid);
  p.putString("pass", cfg.pass);
  p.putString("id", cfg.devId);
  p.putString("loc", cfg.loc);
  p.putString("key", cfg.key);
  p.putString("proto", cfg.proto);
  p.putString("server", cfg.server);
  p.putUInt("iv", cfg.intervalS);
  p.putString("calph", cfg.calPh);
  p.putString("calec", cfg.calEc);
  p.end();
}

bool cfgConfigured() {
  return cfg.ssid.length() && cfg.devId.length() && cfg.key.length() && cfg.server.length();
}
