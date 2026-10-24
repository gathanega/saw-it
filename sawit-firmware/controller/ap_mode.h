// ap_mode.h — MODE AP: alat memancarkan WiFi "AirTrack-<ID>", halaman setup di 192.168.4.1.
// Satu halaman HTML mengakses DUA ESP32: Controller (pengaturan, status) dan Sensor node (pembacaan langsung,
// ukur, kalibrasi) — perintah ke node diteruskan Controller lewat UART. Dibuka dengan switch AP (PIN_SW_AP).
#pragma once
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "config.h"
#include "protocol.h"
#include "settings.h"
#include "hw.h"
#include "state.h"
#include "node_link.h"
#include "storage.h"
#include "lcd_ui.h"
#include "ap_page.h"

static WebServer apServer(80);
static DNSServer apDns;
static uint32_t apLastActivity = 0;
static uint32_t apRebootAt = 0;
static bool apBySwitch = false;      // true: AP dibuka lewat switch, ditutup saat switch OFF (tanpa restart)
static bool apRoutesSet = false;

static void apTouch() { apLastActivity = millis(); }
static void apJson(int code, const String& body) { apTouch(); apServer.send(code, "application/json", body); }

static String apSsid() {
  String id = cfg.devId;
  if (id.isEmpty()) { id = WiFi.macAddress(); id.replace(":", ""); id = id.substring(id.length() - 4); }
  return "AirTrack-" + id;
}

static void apHandleRoot() { apTouch(); apServer.send_P(200, "text/html; charset=utf-8", AP_PAGE); }

// Status Controller. full=false: tanpa hitung antrean SD (dipakai polling cepat /api/live).
static String ctrlStatusJson(bool full) {
  float vb = readVbat();
  uint32_t e = nowEpoch();
  String o = "{" + jStr("id", cfg.devId) + "," + jStr("fw", FW_VERSION) + "," + jStr("time", e ? isoUtc(e) : "") + ",";
  o += jInt("bat", vbatToPct(vb)) + "," + jNum("vbat", vb, 2) + ",";
  o += String("\"sw\":") + (btn(PIN_SW_AP) ? "true" : "false") + ",";
  if (full) {
    o += String("\"sd\":") + (sdOk ? "true" : "false") + "," + jInt("pending", pendingCount()) + ",";
    o += jStr("cal_ph", cfg.calPh) + "," + jStr("cal_ec", cfg.calEc) + ",";
    o += String("\"configured\":") + (cfgConfigured() ? "true" : "false") + ",";
  }
  o += String("\"ap_switch\":") + (apBySwitch ? "true" : "false") + "}";
  return o;
}

static void apHandleStatus() { apJson(200, ctrlStatusJson(true)); }

// Status Sensor node (versi, ADS1115, nilai kalibrasi).
static void apHandleNode() {
  apTouch();
  String reply, err;
  if (nodeQuick("status", reply, err, 3000)) apServer.send(200, "application/json", reply);
  else apServer.send(200, "application/json", "{\"ok\":0," + jStr("err", err) + "}");
  apTouch();
}

// Pembacaan langsung: status Controller + pembacaan Sensor node dalam satu panggilan.
static void apHandleLive() {
  apTouch();
  String reply, err;
  bool ok = nodeQuick("live", reply, err, 8000);
  apTouch();
  String o = "{\"ctrl\":" + ctrlStatusJson(false) + ",\"node\":";
  o += ok ? reply : "{\"ok\":0," + jStr("err", err) + "}";
  o += "}";
  apServer.send(200, "application/json", o);
}

static void apHandleScan() {
  apTouch();
  int n = WiFi.scanNetworks(false, false);
  String o = "[";
  bool first = true;
  for (int i = 0; i < n; i++) {
    String s = WiFi.SSID(i);
    if (s.isEmpty()) continue;
    bool dup = false;
    for (int j = 0; j < i; j++) if (WiFi.SSID(j) == s) { dup = true; break; }
    if (dup) continue;
    if (!first) o += ",";
    first = false;
    o += "{" + jStr("ssid", s) + "," + jInt("rssi", WiFi.RSSI(i)) + ",\"enc\":" + (WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false") + "}";
  }
  WiFi.scanDelete();
  apJson(200, o + "]");
}

static void apHandleGetConfig() {
  String o = "{" + jStr("ssid", cfg.ssid) + "," + jStr("id", cfg.devId) + "," + jStr("loc", cfg.loc) + ",";
  o += jStr("proto", cfg.proto) + "," + jStr("server", cfg.server) + "," + jInt("interval", cfg.intervalS) + ",";
  o += String("\"has_pass\":") + (cfg.pass.length() ? "true" : "false") + ",";
  o += String("\"has_key\":") + (cfg.key.length() ? "true" : "false") + "}";
  apJson(200, o);          // pass dan key TIDAK pernah dikembalikan
}

static void apHandleSetConfig() {
  String ssid = apServer.arg("ssid"), id = apServer.arg("id"), server = apServer.arg("server");
  String proto = apServer.arg("proto"), pass = apServer.arg("pass"), key = apServer.arg("key"), loc = apServer.arg("loc");
  ssid.trim(); id.trim(); server.trim(); loc.trim();
  if (ssid.isEmpty() || id.isEmpty() || server.isEmpty()) { apJson(400, "{\"ok\":false,\"err\":\"SSID, ID alat, dan server wajib diisi\"}"); return; }
  if (id.length() > 32 || loc.length() > 32 || key.length() > 128) { apJson(400, "{\"ok\":false,\"err\":\"Isian terlalu panjang\"}"); return; }
  if (key.isEmpty() && cfg.key.isEmpty()) { apJson(400, "{\"ok\":false,\"err\":\"Credential wajib diisi\"}"); return; }

  if (pass.isEmpty() && ssid != cfg.ssid) cfg.pass = "";     // WiFi baru tanpa sandi = jaringan terbuka
  if (pass.length()) cfg.pass = pass;
  cfg.ssid = ssid;
  cfg.devId = id;
  cfg.loc = loc;
  if (key.length()) cfg.key = key;
  cfg.proto = (proto == "mqtt") ? "mqtt" : "http";
  cfg.server = server;
  cfg.intervalS = constrain((uint32_t)apServer.arg("interval").toInt(), (uint32_t)MIN_INTERVAL_S, (uint32_t)MAX_INTERVAL_S);
  cfgSave();
  g_nextDue = 0; g_failCount = 0;
  if (apBySwitch) {
    apJson(200, "{\"ok\":true,\"reboot\":false}");   // tetap di mode AP; mode Internet mulai saat switch di-OFF-kan
  } else {
    apJson(200, "{\"ok\":true,\"reboot\":true}");
    apRebootAt = millis() + 1500;
  }
}

static void apHandleMeasure() {
  apTouch();
  NodeReading r; String err;
  bool ok = takeMeasurement(r, err);
  apTouch();
  if (ok) apServer.send(200, "application/json", g_nodeReply);
  else apServer.send(200, "application/json", "{\"ok\":0," + jStr("err", err) + "}");
}

static void apHandleCal() {
  apTouch();
  String t = apServer.arg("t"), cmd;
  if (t == "ph7") cmd = "{\"c\":\"cal_ph\",\"v\":7}";
  else if (t == "ph4") cmd = "{\"c\":\"cal_ph\",\"v\":4}";
  else if (t == "ec") cmd = "{\"c\":\"cal_ec\",\"v\":1413}";
  else if (t == "turb") cmd = "{\"c\":\"cal_turb\"}";
  else if (t == "reset") cmd = "{\"c\":\"cal_reset\"}";
  else { apJson(400, "{\"ok\":0,\"err\":\"jenis kalibrasi tidak dikenal\"}"); return; }

  String reply, err;
  bool ok = nodeSession(cmd, reply, 30000, false, err);
  apTouch();
  if (!ok) { apServer.send(200, "application/json", "{\"ok\":0," + jStr("err", err) + "}"); return; }

  uint32_t e = nowEpoch();
  String today = e ? dateUtc(e) : "";
  String detail; float v;
  if (t == "ph7" || t == "ph4") { if (today.length()) cfg.calPh = today; if (jGetNum(reply, "mv", v)) detail = String(v, 0) + " mV"; }
  if (t == "ec")   { if (today.length()) cfg.calEc = today; if (jGetNum(reply, "k", v)) detail = "K = " + String(v, 3); }
  if (t == "turb" && jGetNum(reply, "v0", v)) detail = "V0 = " + String(v, 2) + " V";
  if (t == "reset") { cfg.calPh = ""; cfg.calEc = ""; }
  cfgSave();
  apServer.send(200, "application/json", "{\"ok\":1," + jStr("detail", detail) + "}");
}

static void apHandleReboot() { apJson(200, "{\"ok\":true}"); apRebootAt = millis() + 800; }

static void apNotFound() {                 // captive portal: arahkan semua ke halaman setup
  apTouch();
  apServer.sendHeader("Location", "http://192.168.4.1/", true);
  apServer.send(302, "text/plain", "");
}

// Jalankan AP.
//  bySwitch=true : terbuka selama switch ON; ditutup saat switch OFF (return true) atau idleMs tanpa aktivitas (return false).
//  bySwitch=false: tanpa switch. forever=true tidak pernah timeout (alat belum dikonfigurasi, simpan = restart).
bool runApMode(uint32_t idleMs, bool forever, bool bySwitch = false) {
  apBySwitch = bySwitch;
  apRebootAt = 0;
  Serial.println(String("[ap] mode AP") + (bySwitch ? " (switch): " : ": ") + apSsid());
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(apSsid().c_str(), AP_PASSWORD);
  delay(200);
  apDns.start(53, "*", WiFi.softAPIP());

  // Rail 5V ON + Sensor node ditahan bangun selama AP, supaya halaman bisa membaca/mengkalibrasi tanpa menunggu
  g_keep5v = true; g_nodeHold = true;
  rail5v(true);
  delay(RAIL_SETTLE_MS);
  if (lcdBegin()) lcdMsg(bySwitch ? "MODE AP (switch)" : "MODE AP", apSsid());
  { String r, e; nodeQuick("status", r, e, 3000); }       // bangunkan node; bila gagal halaman menampilkan Offline

  if (!apRoutesSet) {
    apServer.on("/", HTTP_GET, apHandleRoot);
    apServer.on("/api/status", HTTP_GET, apHandleStatus);
    apServer.on("/api/node", HTTP_GET, apHandleNode);
    apServer.on("/api/live", HTTP_GET, apHandleLive);
    apServer.on("/api/scan", HTTP_GET, apHandleScan);
    apServer.on("/api/config", HTTP_GET, apHandleGetConfig);
    apServer.on("/api/config", HTTP_POST, apHandleSetConfig);
    apServer.on("/api/measure", HTTP_POST, apHandleMeasure);
    apServer.on("/api/cal", HTTP_POST, apHandleCal);
    apServer.on("/api/reboot", HTTP_POST, apHandleReboot);
    apServer.onNotFound(apNotFound);
    apRoutesSet = true;
  }
  apServer.begin();

  apTouch();
  bool switchOff = false;
  uint32_t offSince = 0;
  while (true) {
    if (bySwitch) {
      if (!btn(PIN_SW_AP)) {                                  // switch OFF stabil 300 ms = keluar dari mode AP
        if (!offSince) offSince = millis();
        if (millis() - offSince > 300) { switchOff = true; break; }
      } else offSince = 0;
      if (millis() - apLastActivity >= idleMs) break;
    } else if (!forever && millis() - apLastActivity >= idleMs) break;
    apDns.processNextRequest();
    apServer.handleClient();
    if (apRebootAt && (int32_t)(millis() - apRebootAt) >= 0) { delay(200); ESP.restart(); }
    delay(2);
  }
  apServer.stop();
  apDns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  nodeSleep();                                                // node tidur lagi, rail dimatikan
  g_nodeHold = false; g_keep5v = false;
  railsOff();
  rail5v(false);
  if (lcd) { lcd->clear(); lcd->noBacklight(); }
  Serial.println(switchOff ? "[ap] ditutup (switch OFF)" : "[ap] ditutup (tidak ada aktivitas)");
  return switchOff;
}
