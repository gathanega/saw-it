# Modul Tanaman Saw-IT

Dashboard pemantau tanaman berbasis IoT, ditambahkan ke aplikasi Saw-IT (Laravel 13, Blade, SQLite/MySQL). Modul kualitas air tetap utuh di `/air`; `/` sekarang dashboard tanaman. Login, peran admin/user, tabel `devices`, dan antrean memakai yang sudah ada.

> **Belum dijalankan.** Lingkungan penyusun tidak punya PHP, jadi migrasi, tes, dan tampilan belum dieksekusi. Ikuti langkah pemasangan; kirim pesan galatnya bila ada.

## Stack
Laravel 13 / PHP 8.3 (bukan Laravel 11 dari spesifikasi, karena proyek dasar sudah Laravel 13), Blade + CSS biasa (bukan Tailwind/Alpine, mengikuti proyek dasar), Chart.js dari CDN, queue database + scheduler, auth sesi untuk pengguna dan kunci per perangkat.

## Pemasangan
```bash
cd airtracking && composer install && php artisan migrate --seed   # seeder: struktur ambang (KOSONG) + rekomendasi awal
php artisan airtracking:make-admin admin@contoh.com --name=Admin
php artisan serve
php artisan queue:work            # kirim notifikasi (Telegram/email)
# cron tiap menit:  * * * * * cd /path/airtracking && php artisan schedule:run >> /dev/null 2>&1
```
`.env`: `APP_TIMEZONE=UTC` (jangan diubah; tampilan memakai WIB), `QUEUE_CONNECTION=database`, `TELEGRAM_BOT_TOKEN`, `TELEGRAM_CHAT_ID`, `PLANT_ALERT_MAIL` (+ pengaturan `MAIL_*` bila email dipakai).

Alur pertama: Admin → **Alat tanaman** (buat perangkat, salin kunci) → **Ambang batas** (isi bersama penyuluh) → **Kalibrasi** → **Kontrol** (aktuator + aturan).

## Kontrak perangkat
Header `X-Device-Id`, `X-Device-Key` (atau `id` + `key` di body JSON).

`POST /api/plant/ingest`
```json
{"ts":"2026-10-24T08:30:00Z","fw":"1.0","bat":88,
 "data":{"soil_moisture":512,"air_temp":28.1,"air_rh":71,"light":18000,"soil_temp":26.4}}
```
Batch tertunda: `{"records":[{"ts":"...","data":{...}}, ...]}` (maks 500). Respons `{"ok":true,"saved":n,"duplicates":n,"ignored":n}`; kirim ulang aman. Tanpa `ts`, waktu server dipakai. Kunci sensor ada di `config/plant.php` (`soil_moisture, air_temp, air_rh, light, soil_temp, water_level, soil_ph, rain, soil_ec, soil_n/p/k, leaf_wet, water_ph, water_ec, water_temp, tank_level, flow, co2, uv`). VPD dihitung server.

`GET /api/plant/commands` → `{"server_time":"...","commands":[{"id":7,"actuator":"pump","action":"on","duration_sec":300}]}`
`POST /api/plant/commands/{id}/result` → `{"status":"done|failed","message":"..."}`
Perangkat wajib mematikan aktuator sendiri setelah `duration_sec`. Perintah pending > 10 menit kedaluwarsa; perintah `sent` tanpa laporan > 30 menit ditandai gagal.

## Pemetaan ke 9 fitur
| # | Fitur | Lokasi |
|---|---|---|
| 1 | Ambang per komoditas/parameter/fase, bisa diubah | tabel `plant_thresholds`, `Admin/ThresholdController`; seeder hanya struktur kosong + "perlu diverifikasi" |
| 2 | Kartu status + penanda data usang | `/` (`PlantSnapshot`, `Status`); ikon + teks + warna |
| 3 | Alert, kanal, queue, cooldown, log | `AlertEngine`, `Jobs/SendAlertNotification`, `/peringatan` |
| 4 | Kontrol otomatis, Auto/Manual, batas durasi | `ControlEngine`, `Plant/ControlController`, `/kontrol` |
| 5 | Grafik, riwayat, filter tanggal, CSV | `/grafik`, `/tanam/api/series`, `/tanam/api/export.csv` |
| 6 | VPD, indeks stres, kebutuhan air | `Services/Plant/Derived.php`, `/bantuan` |
| 7 | Rekomendasi dapat diedit | `plant_recommendation_rules`, Admin → Rekomendasi |
| 8 | Kalibrasi dua titik per sensor | `Admin/CalibrationController` |
| 9 | Ketahanan | unik `(device, sensor, measured_at)`, batch, `measured_at` + `received_at`, status sensor error, `plant:watch` (offline) |

## Asumsi dan catatan
- **Ambang batas tidak diisi** (sengaja). Tanpa ambang, kartu berstatus "Ambang belum diatur" dan tidak ada alert ambang.
- **Indeks stres** adalah asumsi (tidak ada rumus baku); **Kc** wajib diisi pengguna; **ET₀** memakai Hargreaves-Samani (perkiraan kasar) dan butuh koordinat alat.
- **Batas fisik sensor** di `config/plant.php` adalah batas umum, sesuaikan dengan datasheet.
- Kelembapan tanah wajib dikalibrasi; sebelum itu statusnya "Belum dikalibrasi", bukan error.
- Data lebih tua dari 10 menit (paket tertunda) disimpan tetapi tidak memicu alert/kontrol.
- Firmware ESP32 bawaan zip untuk air tidak diubah dan tidak mengirim data tanaman; perangkat tanaman perlu mengikuti kontrak di atas.
- Dashboard air (`/api/public/devices`) masih menampilkan semua baris `devices`, termasuk perangkat tanaman (tanpa data air). Filter `kind='water'` di `PublicController` bila perlu.
- Telegram/email tidak bisa diuji tanpa token dan SMTP Anda. Tes ada di `tests/Feature/PlantTest.php` (`php artisan test`).
