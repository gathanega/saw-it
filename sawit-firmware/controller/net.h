// net.h — WiFi (mode Internet), kirim HTTP POST /api/ingest atau MQTT.
#pragma once
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#if USE_MQTT
#include <PubSubClient.h>
#endif
#include "config.h"
#include "protocol.h"
#include "settings.h"
#include "hw.h"
#include "storage.h"

enum SendResult { SEND_OK, SEND_FAIL, SEND_AUTH, SEND_REJECT };
String g_lastNetErr;

bool wifiConnect(uint32_t timeoutMs = WIFI_TIMEOUT_MS) {
  if (cfg.ssid.isEmpty()) return false;
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);
  WiFi.begin(cfg.ssid.c_str(), cfg.pass.c_str());
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < timeoutMs) delay(200);
  bool ok = WiFi.status() == WL_CONNECTED;
  Serial.println(ok ? "[wifi] tersambung " + WiFi.localIP().toString() : "[wifi] gagal tersambung");
  return ok;
}
void wifiOff() { WiFi.disconnect(true, false); WiFi.mode(WIFI_OFF); }

// ---------- Payload sesuai format web airtracking ----------
// Header: id, key, loc, fw, bat, cal_ph, cal_ec, sd_pending (objek TERBUKA, tanpa '}').
int g_batPct = -1;
String payloadHeader(uint32_t sdPending) {
  String h = "{" + jStr("id", cfg.devId) + "," + jStr("key", cfg.key);
  if (cfg.loc.length()) h += "," + jStr("loc", cfg.loc);
  h += "," + jStr("fw", FW_VERSION);
  if (g_batPct >= 0) h += "," + jInt("bat", g_batPct);
  if (cfg.calPh.length()) h += "," + jStr("cal_ph", cfg.calPh);
  if (cfg.calEc.length()) h += "," + jStr("cal_ec", cfg.calEc);
  h += "," + jInt("sd_pending", (long)sdPending);
  return h;
}
// 1 data -> format datar ({"id":..,"ph":..}); banyak data -> "records":[...]
String buildPayload(const String* lines, int n, uint32_t sdPending) {
  String h = payloadHeader(sdPending);
  if (n == 1) return h + "," + lines[0].substring(1);             // gabung ke objek yang sama
  h += ",\"records\":[";
  for (int i = 0; i < n; i++) { if (i) h += ","; h += lines[i]; }
  return h + "]}";
}

String httpUrl() {
  String u = cfg.server; u.trim();
  if (!u.startsWith("http://") && !u.startsWith("https://")) u = "http://" + u;
  int schemeEnd = u.indexOf("://") + 3;
  if (u.indexOf('/', schemeEnd) < 0) u += API_PATH;
  return u;
}

SendResult httpSend(const String& body) {
  String url = httpUrl();
  bool https = url.startsWith("https://");
  WiFiClient plain;
  WiFiClientSecure secure;
  if (https) { if (strlen(ROOT_CA)) secure.setCACert(ROOT_CA); else secure.setInsecure(); }
  HTTPClient http;
  http.setTimeout(12000);
  if (!http.begin(https ? (WiFiClient&)secure : plain, url)) { g_lastNetErr = "URL server tidak valid"; return SEND_FAIL; }
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(body);
  String resp = code > 0 ? http.getString() : "";
  http.end();
  Serial.printf("[http] POST %s -> %d %s\n", url.c_str(), code, resp.substring(0, 120).c_str());
  if (code >= 200 && code < 300) return SEND_OK;
  if (code == 401) { g_lastNetErr = "ID alat/credential ditolak server (401)"; return SEND_AUTH; }
  if (code == 422) { g_lastNetErr = "Nilai ditolak server (422)"; return SEND_REJECT; }
  g_lastNetErr = code > 0 ? "HTTP " + String(code) : String("Server tidak terjangkau");
  return SEND_FAIL;
}

#if USE_MQTT
SendResult mqttSend(const String* lines, int n, uint32_t sdPending) {
  String host = cfg.server; host.trim();
  bool tls = false;
  if (host.startsWith("mqtts://")) { tls = true; host = host.substring(8); }
  else if (host.startsWith("mqtt://")) host = host.substring(7);
  int slash = host.indexOf('/'); if (slash >= 0) host = host.substring(0, slash);
  uint16_t port = tls ? 8883 : 1883;
  int colon = host.lastIndexOf(':');
  if (colon > 0) { port = host.substring(colon + 1).toInt(); host = host.substring(0, colon); }

  WiFiClient plain; WiFiClientSecure secure;
  if (tls) { if (strlen(ROOT_CA)) secure.setCACert(ROOT_CA); else secure.setInsecure(); }
  PubSubClient mqtt(tls ? (Client&)secure : (Client&)plain);
  mqtt.setServer(host.c_str(), port);
  mqtt.setBufferSize(1024);
  mqtt.setSocketTimeout(10);
  // username = ID alat, password = credential (sesuaikan dengan konfigurasi broker)
  if (!mqtt.connect(("airtrack-" + cfg.devId).c_str(), cfg.devId.c_str(), cfg.key.c_str())) {
    g_lastNetErr = "MQTT gagal tersambung (rc=" + String(mqtt.state()) + ")";
    return SEND_FAIL;
  }
  char topic[64]; snprintf(topic, sizeof(topic), MQTT_TOPIC_FMT, cfg.devId.c_str());
  SendResult res = SEND_OK;
  for (int i = 0; i < n; i++) {
    String p = buildPayload(&lines[i], 1, sdPending + (n - 1 - i));
    if (!mqtt.publish(topic, p.c_str())) { g_lastNetErr = "MQTT publish gagal"; res = SEND_FAIL; break; }
    mqtt.loop();
  }
  mqtt.disconnect();
  return res;
}
#endif

SendResult sendChunk(const String* lines, int n, uint32_t sdPendingAfter) {
#if USE_MQTT
  if (cfg.proto == "mqtt") return mqttSend(lines, n, sdPendingAfter);
#endif
  return httpSend(buildPayload(lines, n, sdPendingAfter));
}

// Kirim seluruh antrean SD (atau satu data langsung bila tanpa SD). Return jumlah baris terkirim.
uint32_t flushPending(const String* directLine = nullptr) {
  if (!sdOk) {
    if (directLine) { SendResult r = sendChunk(directLine, 1, 0); return r == SEND_OK ? 1 : 0; }
    return 0;
  }
  if (!SD.exists(PENDING_FILE)) return 0;
  File f = SD.open(PENDING_FILE, FILE_READ);
  if (!f) return 0;
  uint32_t total = 0, sent = 0, consumed = 0;
  while (f.available()) { String l = f.readStringUntil('\n'); l.trim(); if (l.length() > 2) total++; }
  f.seek(0);

  String chunk[SEND_CHUNK];
  while (f.available()) {
    int n = 0;
    while (n < SEND_CHUNK && f.available()) {
      String l = f.readStringUntil('\n'); l.trim();
      if (l.length() > 2 && l[0] == '{') chunk[n++] = l;
    }
    uint32_t pos = f.position();
    if (n == 0) { consumed = pos; break; }
    SendResult r = sendChunk(chunk, n, total - sent - n);
    if (r == SEND_OK) { sent += n; consumed = pos; }
    else if (r == SEND_REJECT) {                                  // jangan biarkan data buruk menyumbat antrean
      for (int i = 0; i < n; i++) sdAppendLine(REJECT_FILE, chunk[i]);
      sent += n; consumed = pos;
    } else break;                                                  // gagal/401: sisakan di SD, coba lagi nanti
  }
  f.close();
  pendingConsume(consumed);
  return sent;
}
