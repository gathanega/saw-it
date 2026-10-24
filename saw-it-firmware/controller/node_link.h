// node_link.h — komunikasi UART + pembangunan sensor node.
#pragma once
#include "config.h"
#include "protocol.h"
#include "hw.h"

struct NodeReading { float ph = NAN, ec = NAN, tds = NAN, doo = NAN, temp = NAN, turb = NAN; };
String g_nodeReply;     // balasan mentah terakhir dari node (dipakai halaman AP)
bool g_nodeHold = false; // true selama mode AP: node ditahan bangun, rail 5V tetap menyala

void linkBegin() { Serial2.begin(LINK_BAUD, SERIAL_8N1, PIN_LINK_RX, PIN_LINK_TX); }

bool linkReadLine(String& out, uint32_t timeoutMs) {
  static String buf;
  uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    while (Serial2.available()) {
      char c = (char)Serial2.read();
      if (c == '\n') { out = buf; buf = ""; out.trim(); if (out.length()) return true; }
      else if (c != '\r' && buf.length() < 300) buf += c;
    }
    delay(2);
  }
  return false;
}
void linkSend(const String& s) { Serial2.print(s); Serial2.print('\n'); Serial2.flush(); }
void linkDrain() { while (Serial2.available()) Serial2.read(); }

// Kirim perintah, tunggu balasan yang memuat "ok". Balasan lain (mis. rdy) diabaikan.
bool nodeRequest(const String& cmd, String& reply, uint32_t timeoutMs) {
  linkDrain();
  linkSend(cmd);
  uint32_t t0 = millis();
  String line;
  while (millis() - t0 < timeoutMs) {
    if (linkReadLine(line, 200)) {
      float tmp;
      if (jGetNum(line, "ok", tmp)) { reply = line; return true; }
    }
  }
  return false;
}

// Pastikan node bangun: coba ping dulu (mungkin sudah bangun), lalu pulsa WAKE.
bool nodeWake() {
  String r;
  if (nodeRequest("{\"c\":\"ping\"}", r, 300)) return true;
  for (int i = 0; i < 3; i++) {
    linkDrain();
    digitalWrite(PIN_WAKE, HIGH); delay(120); digitalWrite(PIN_WAKE, LOW);
    String line; uint32_t t0 = millis();
    while (millis() - t0 < 2500) {
      if (linkReadLine(line, 100)) {
        float v;
        if (jGetNum(line, "rdy", v) || jGetNum(line, "ok", v)) return true;
      }
    }
  }
  return false;
}

void nodeSleep() { String r; nodeRequest("{\"c\":\"sleep\"}", r, 500); }

// Satu sesi perintah ke node dengan rail dinyalakan. cmd = JSON perintah.
// Saat g_nodeHold (mode AP) node tidak ditidurkan setelah perintah selesai.
bool nodeSession(const String& cmd, String& reply, uint32_t timeoutMs, bool withDo, String& err) {
  railsOn(withDo);
  bool ok = false;
  if (!nodeWake()) {
    err = "Sensor node tidak merespons (cek kabel WAKE/TX/RX dan catu node)";
  } else {
    if (g_nodeHold) { String h; nodeRequest("{\"c\":\"hold\",\"v\":1}", h, 500); }
    if (!nodeRequest(cmd, reply, timeoutMs)) {
      err = "Sensor node tidak mengirim hasil (timeout)";
    } else if (!jGetOk(reply)) {
      if (!jGetStr(reply, "err", err)) err = "Node melaporkan kegagalan";
    } else {
      ok = true;
    }
  }
  g_nodeReply = reply;
  if (!g_nodeHold) nodeSleep();
  railsOff();
  return ok;
}

// Perintah cepat (status/live) untuk mode AP: tanpa mengubah rail, bangunkan node bila perlu.
bool nodeQuick(const char* cmd, String& reply, String& err, uint32_t timeoutMs = 8000) {
  String r;
  if (!nodeRequest("{\"c\":\"ping\"}", r, 400)) {
    if (!nodeWake()) { err = "Sensor node tidak merespons (cek kabel WAKE/TX/RX dan catu node)"; return false; }
    nodeRequest("{\"c\":\"hold\",\"v\":1}", r, 500);
  }
  if (!nodeRequest(String("{\"c\":\"") + cmd + "\"}", reply, timeoutMs)) { err = "Sensor node tidak mengirim hasil (timeout)"; return false; }
  if (!jGetOk(reply)) { if (!jGetStr(reply, "err", err)) err = "Node melaporkan kegagalan"; return false; }
  return true;
}

bool takeMeasurement(NodeReading& r, String& err) {
  String reply;
  uint32_t to = NODE_TIMEOUT_MS + (USE_DO ? 40000 : 0);
  String cmd = String("{\"c\":\"measure\",\"do\":") + (USE_DO ? 1 : 0) + "}";
  if (!nodeSession(cmd, reply, to, USE_DO, err)) return false;
  jGetNum(reply, "ph", r.ph);
  jGetNum(reply, "ec", r.ec);
  jGetNum(reply, "tds", r.tds);
  jGetNum(reply, "do", r.doo);
  jGetNum(reply, "temp", r.temp);
  jGetNum(reply, "turb", r.turb);
  return true;
}
