// protocol.h — identik di controller/ dan sensor_node/ (salin bila diubah).
// Helper JSON datar kecil (tanpa library) + definisi jalur UART antar-ESP32.
//
// Jalur UART (115200 8N1, satu baris JSON per pesan, diakhiri '\n'):
//   controller -> node : {"c":"ping"} {"c":"measure","do":0|1} {"c":"cal_ph","v":4|7}
//                        {"c":"cal_ec","v":1413} {"c":"cal_turb"} {"c":"cal_reset"} {"c":"sleep"}
//                        {"c":"status"} {"c":"live"} {"c":"hold","v":1|0}   (dipakai mode AP)
//   node -> controller : {"rdy":1,"fw":"1.0.0"}   (sesaat setelah bangun)
//                        {"ok":1,"ph":7.1,"ec":420,"tds":210,"do":null,"temp":27.5,"turb":3.2,"mv_ph":..}
//                        {"ok":0,"err":"pesan"}
#pragma once
#include <Arduino.h>
#include <math.h>

#define FW_VERSION "1.1.0"
#define LINK_BAUD  115200

// ---------- membuat JSON ----------
inline String jEsc(const String& s) {
  String o; o.reserve(s.length() + 4);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if (c == '\n') o += "\\n";
    else if (c == '\r') o += "\\r";
    else if (c == '\t') o += "\\t";
    else if ((uint8_t)c < 0x20) { /* buang karakter kontrol */ }
    else o += c;
  }
  return o;
}
inline String jStr(const char* k, const String& v) { return String("\"") + k + "\":\"" + jEsc(v) + "\""; }
inline String jNum(const char* k, float v, int dec = 2) {
  if (isnan(v) || isinf(v)) return String("\"") + k + "\":null";
  return String("\"") + k + "\":" + String(v, dec);
}
inline String jInt(const char* k, long v) { return String("\"") + k + "\":" + String(v); }

// ---------- membaca JSON datar ----------
inline bool jFind(const String& js, const char* key, int& pos) {
  String pat = String("\"") + key + "\"";
  int from = 0;
  while (true) {
    int i = js.indexOf(pat, from);
    if (i < 0) return false;
    int p = i + pat.length();
    while (p < (int)js.length() && js[p] == ' ') p++;
    if (p < (int)js.length() && js[p] == ':') {
      p++;
      while (p < (int)js.length() && js[p] == ' ') p++;
      pos = p;
      return true;
    }
    from = i + 1;
  }
}
inline bool jGetNum(const String& js, const char* key, float& out) {
  int p;
  if (!jFind(js, key, p) || p >= (int)js.length()) return false;
  char c = js[p];
  if (!(isdigit((unsigned char)c) || c == '-' || c == '+' || c == '.')) return false;  // null / string
  out = js.substring(p).toFloat();
  return true;
}
inline bool jGetStr(const String& js, const char* key, String& out) {
  int p;
  if (!jFind(js, key, p) || p >= (int)js.length() || js[p] != '"') return false;
  out = "";
  for (p++; p < (int)js.length(); p++) {
    char c = js[p];
    if (c == '\\' && p + 1 < (int)js.length()) { p++; out += js[p]; }
    else if (c == '"') return true;
    else out += c;
  }
  return false;
}
inline bool jGetOk(const String& js) { float v; return jGetNum(js, "ok", v) && v > 0.5f; }
