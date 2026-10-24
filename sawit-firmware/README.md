# AirTracking — Firmware ESP32 (controller + sensor node)

Firmware Arduino untuk alat portable AirTracking: **dua ESP32 DevKit 38-pin** (controller dan sensor node),
mengirim data ke server AirTracking (Laravel) dengan format JSON yang sama persis dengan `POST /api/ingest`.
Pin mengikuti sheet **Pinout (usulan)** di `Komponen_Spesifik_Airtracking.xlsx`.

> **Status:** kode ditulis dan dicek sintaksnya (kompilasi percobaan dengan header tiruan), tetapi **belum diunggah ke
> hardware dan belum dikompilasi dengan toolchain ESP32 asli**. Siapkan waktu uji bench pertama (lihat bagian *Uji pertama*).

## Isi folder

```
ap_page.html    SUMBER halaman mode AP (1 file HTML). Buka langsung di browser + ?demo untuk melihat tampilan tanpa alat.
controller/     ESP32 #1: jadwal, rail daya, SD, RTC, LCD, WiFi, kirim ke server, MODE AP
  controller.ino  config.h  settings.h  hw.h  state.h  node_link.h  storage.h  net.h  lcd_ui.h  ap_mode.h  ap_page.h  protocol.h
sensor_node/    ESP32 #2: ADS1115 (pH, EC, kekeruhan), DS18B20, DO RS485 (fase 2), kalibrasi
  sensor_node.ino  config.h  protocol.h
```

Buka `controller/controller.ino` dan `sensor_node/sensor_node.ino` sebagai dua sketch terpisah di Arduino IDE
(board **ESP32 Dev Module**, core *esp32 by Espressif* 2.0.x atau 3.x). `protocol.h` sama di kedua folder.

**Library** (Library Manager):

| Sketch | Library |
|---|---|
| controller | `RTClib` (Adafruit), `LiquidCrystal_I2C` (Frank de Brabander), `PubSubClient` (hanya bila `USE_MQTT 1`) |
| sensor_node | `Adafruit ADS1X15`, `OneWire`, `DallasTemperature` |

WiFi, WebServer, DNSServer, HTTPClient, SD, Preferences sudah bawaan core ESP32. Tidak memakai ArduinoJson.

**Saat flashing via USB:** lepas catu eksternal ke pin 3V3 (catatan BOM no. 1).

## Cara kerja

```
timer / tombol ──► CONTROLLER (bangun)
                    1. EN 5V (GPIO26) ON  [+ EN 12V (GPIO27) bila DO]
                    2. pulsa WAKE (GPIO25) ──► SENSOR NODE bangun (ext0, GPIO33)
                    3. UART 115200: {"c":"measure"} ──► node warm-up ±15 dtk, baca sensor
                       node ──► {"ok":1,"ph":..,"ec":..,"tds":..,"do":null,"temp":..,"turb":..}
                    4. node tidur, rail OFF
                    5. data ditulis ke microSD (/pending.ndjson) + arsip /data.csv
                    6. WiFi → (NTP bila jam belum benar) → POST /api/ingest (antrean SD dikirim per 20 data)
                    7. deep sleep sampai jadwal berikutnya (jam DS3231, tidak bergeser oleh tombol)
```

- Waktu dikirim sebagai **UTC** (`ts`, format `2026-10-19T02:30:00Z`) dari RTC DS3231. Bila RTC belum benar, alat sinkron NTP; bila
  tetap gagal, `ts` dikosongkan dan server memakai waktunya sendiri.
- Bila WiFi/server putus, data **tetap di microSD** dan ikut terkirim pada siklus berikutnya (`records`, field `sd_pending` terisi).
  Tanpa microSD, data hanya dikirim langsung (tidak ada buffer).
- Data yang ditolak server dengan 422 (nilai di luar batas fisik) dipindah ke `/rejected.ndjson` agar tidak menyumbat antrean.
  Jawaban 401 (credential salah) membiarkan data tetap di SD.

## Format data (sama dengan web)

```json
{"id":"AIR-01","key":"<credential>","loc":"LOK-01","fw":"1.0.0","bat":87,"cal_ph":"2026-10-19","cal_ec":"2026-10-19","sd_pending":0,
 "ts":"2026-10-19T02:30:00Z","ph":7.12,"ec":420.4,"tds":210.2,"do":null,"temp":27.50,"turb":3.2}
```

Banyak data (antrean SD): sama, tetapi nilai sensor diganti `"records":[{"ts":..,"ph":..}, ...]` (maks 20 per paket).
`do` bernilai `null` selama fase 1 (belum ada sensor DO). Dokumen perencanaan menyebut protokol **MQTT**; sudah tersedia
(`proto = mqtt`, topik `airtracking/<ID>/data`, username = ID alat, password = credential), tetapi **penerima MQTT di server belum dibuat**
(lihat README web), jadi pakai **HTTP** dulu.

## Mode AP (setup): satu halaman untuk dua ESP32

**Dibuka dengan SWITCH AP** (slide switch panel-mount, GPIO13 controller):

```
3V3 ──[ switch ]──┬── GPIO13
                  └── 10 kΩ ── GND        ON = HIGH = mode AP, OFF = mode Internet
```

- **Switch ON** (alat tidur pun akan bangun): controller memancarkan WiFi **`AirTrack-<ID>`**, menyalakan rail 5V, dan **menahan sensor node tetap bangun**.
  LCD (bila rail 5V siap) menampilkan `MODE AP`. Selama switch ON alat tidak mengukur terjadwal dan tidak mengirim data.
- **Switch OFF**: AP ditutup, node ditidurkan, lalu controller **langsung ukur + kirim** ke server (cek “Sinkron terakhir” di dashboard), kemudian tidur seperti biasa.
- Bila switch dibiarkan ON dan halaman tidak dipakai **30 menit** (`AP_SWITCH_IDLE_MS`), AP ditutup sendiri untuk menghemat baterai. Untuk membuka lagi: OFF-kan lalu ON-kan switch.
- Tanpa switch pun, alat yang **belum dikonfigurasi** tetap masuk AP, dan AP otomatis terbuka 5 menit setelah WiFi gagal 3 siklus berturut-turut.
  Cadangan lama (tombol UKUR ditahan 3 detik) bisa dihidupkan dengan `USE_AP_BTN_HOLD 1` di `config.h`.

**Satu HTML, dua ESP32.** Sensor node tidak punya WiFi; halaman dilayani controller, dan perintah ke node diteruskan lewat kabel UART yang sudah ada:

| Panel di halaman | ESP32 | Endpoint controller |
|---|---|---|
| Status Controller (ID, jam, baterai, SD, antrean) | #1 controller | `GET /api/status` |
| Status Sensor node (firmware, ADS1115, nilai kalibrasi) | #2 node, via UART | `GET /api/node` |
| Pembacaan langsung (segar tiap 3 dtk), kedua ESP dalam 1 panggilan | #1 + #2 | `GET /api/live` |
| Pengaturan WiFi/ID/server/interval | #1 | `GET/POST /api/config`, `GET /api/scan` |
| Ukur penuh & kalibrasi pH/EC/kekeruhan | #2 via #1 | `POST /api/measure`, `POST /api/cal` |

Langkah setup:

1. **Daftarkan alat dulu di web:** `/admin/devices` → isi ID alat, nama, kode lokasi → catat **credential** (hanya tampil sekali).
2. Geser **switch AP ke ON**. Sambungkan HP ke WiFi **`AirTrack-<ID>`** (sandi di `config.h`: `AP_PASSWORD`, default `airtrack123`; **ganti** sebelum dipasang).
3. Halaman terbuka sendiri (captive portal); bila tidak, buka `http://192.168.4.1`. Kedua kartu (Controller dan Sensor node) harus **Online**.
4. **Pindai WiFi** → pilih WiFi lokasi → isi sandi. Isi ID alat, kode lokasi, credential, alamat server (mis. `https://domain-anda/api/ingest`), interval.
   Atau tempel JSON dari `/admin/wifi` lalu **Isi form dari konfigurasi**. Tekan **Simpan pengaturan** (alat tidak restart).
5. Kalibrasi sambil memantau **Pembacaan langsung** (lihat bagian Kalibrasi).
6. Geser **switch AP ke OFF**. Alat masuk mode Internet dan mengirim data.

WiFi dan credential disimpan di NVS alat; halaman tidak pernah menampilkannya kembali.
AP tidak punya internet, jadi font Outfit hanya dipakai bila terpasang di HP (selain itu font sistem).
Halaman diubah di `ap_page.html`, lalu isinya disalin ke dalam `R"rawliteral(` … `)rawliteral"` pada `controller/ap_page.h`.

## Tombol di lapangan

| Tombol | Aksi |
|---|---|
| SWITCH AP (GPIO13) | ON = mode AP (satu halaman untuk controller + sensor node) · OFF = mode Internet |
| UKUR (GPIO32) | tekan = ukur & kirim sekarang |
| LCD (GPIO33) | tekan = LCD menyala 30 detik (rail 5V ON); tekan lagi = halaman berikut (nilai · turbidity/DO · baterai/SD/WiFi · jam WIB). Saat LCD menyala, tombol UKUR = ukur dengan layar aktif |

## Kalibrasi (lewat halaman AP)

Pantau panel **Pembacaan langsung** (mV mentah ikut tampil) sampai stabil sebelum menekan tombol kalibrasi.

1. **pH 2 titik:** bilas probe, celup ke buffer **7.00**, tunggu ±1 menit, tekan *pH 7.00*; lalu buffer **4.00**, tekan *pH 4.00*.
   Kompensasi suhu (Nernst) memakai DS18B20. Sebelum dikalibrasi dipakai nilai bawaan DFRobot (1500 mV = pH 7, 2032 mV = pH 4).
2. **EC:** celup ke larutan **1413 µS/cm**, tekan *EC 1413*. Menyimpan faktor K (wajib; tanpa kalibrasi EC belum akurat). TDS = EC × 0,5 (`TDS_FACTOR`, ubah 0,5–0,7).
3. **Kekeruhan:** celup ke aquades, tekan *Kekeruhan 0*. Hasil NTU **indikatif** (rumus SEN0189).
4. Tanggal kalibrasi pH/EC otomatis ikut terkirim sebagai `cal_ph` / `cal_ec` (butuh jam valid).

Nilai kalibrasi disimpan di NVS **sensor node**; tanggalnya di controller.

## Fase 2: sensor DO (SN-3002-LDO, RS485)

Set `USE_DO 1` di **kedua** `config.h`. Controller akan menyalakan rail 12V dan node membaca Modbus-RTU lewat RS485 (GPIO16/17, DE/RE GPIO19).
**Verifikasi dulu di datasheet** `DO_MB_ADDR`, `DO_MB_BAUD`, `DO_MB_REG`, `DO_MB_IDX`, `DO_MB_SCALE` — nilai di `config.h` hanya tebakan umum untuk sensor sejenis.

## Hal yang perlu diuji / diputuskan

- **Uji pertama (bench):** (1) node saja: buka Serial Monitor, ketik `{"c":"ping"}` + Enter lalu `{"c":"measure"}`; (2) controller + node tanpa WiFi: cek `[cycle] data:` di Serial;
  (3) mode AP (switch ON → kedua kartu Online, pembacaan langsung bergerak; switch OFF → alat ukur+kirim); (4) kirim ke server lokal (`php artisan serve --host=0.0.0.0`, alamat `http://IP-laptop:8000/api/ingest`).
- **Interval minimum 60 detik** (web admin mengizinkan 10) karena sensor perlu ±45 detik aktif; default 900 detik (15 menit, sama dengan sheet Anggaran Daya).
- **Switch AP:** tambahan hardware baru (belum ada di BOM/Pinout): 1 slide switch + resistor pull-down 10 kΩ di GPIO13 controller. Tombol ukur tidak lagi dipakai untuk masuk AP.
- **Mode AP lebih boros daya:** WiFi AP, rail 5V, dan sensor node menyala terus selama switch ON. Jangan dibiarkan ON di lapangan.
- **DO:** dokumen perencanaan menyebut RS232, daftar komponen RS485 — firmware mengikuti RS485.
- **HTTPS:** bila `ROOT_CA` di `config.h` kosong, sertifikat server **tidak diverifikasi**. Isi PEM root CA server untuk produksi.
- Credential dan sandi WiFi tersimpan polos di NVS (tidak terenkripsi); aktifkan flash encryption bila alat bisa jatuh ke tangan lain.
- Rumus EC (DFR0300), pH, dan kekeruhan memakai konstanta DFRobot; akurasi final tergantung kalibrasi dan kondisi probe.
- Persentase baterai dari kurva Li-ion kasar; sesuaikan `VBAT_SCALE` dengan multimeter.

## Pemecahan masalah

| Gejala | Penyebab umum |
|---|---|
| `Sensor node tidak merespons` | Kabel TX/RX harus silang (17→26, 16→27), GND bersama, WAKE GPIO25→GPIO33, catu 3V3 node hidup |
| `ADS1115 tidak terdeteksi` | Rail 5V/LDO analog belum menyala, alamat I2C bukan 0x48 (ADDR ke GND), SDA/SCL tertukar |
| `401` | ID alat/credential tidak cocok dengan yang diregistrasi (ganti credential di `/admin/devices`, isi ulang lewat AP) |
| `422` | Nilai di luar batas fisik (data pindah ke `/rejected.ndjson`) |
| Jam tidak benar | Baterai CR2032 DS3231 habis / modul ZS-042 — alat otomatis sinkron NTP bila ada WiFi |
| Alat bangun terus | Tombol ditekan terus atau pull-down 10 kΩ tidak terpasang pada GPIO32/33 |
| LCD kosong | Rail 5V mati, level shifter belum terpasang, atau alamat bukan 0x27/0x3F |
