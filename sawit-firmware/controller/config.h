// config.h — CONTROLLER (ESP32 #1). Pin mengikuti sheet "Pinout (usulan)".
#pragma once

// ---------- Pin ----------
#define PIN_SDA        21     // I2C: DS3231 langsung; LCD lewat level shifter BSS138
#define PIN_SCL        22
#define PIN_SD_CS      5      // microSD (VSPI)
#define PIN_SD_SCK     18
#define PIN_SD_MISO    19
#define PIN_SD_MOSI    23
#define PIN_LINK_TX    17     // TX2 -> RX1 node (GPIO26)
#define PIN_LINK_RX    16     // RX2 <- TX1 node (GPIO27)
#define PIN_WAKE       25     // pulsa HIGH membangunkan sensor node (GPIO33 node)
#define PIN_EN_5V      26     // HIGH = rail 5V aktif (LCD, turbidity, LDO analog)
#define PIN_EN_12V     27     // HIGH = rail 12V aktif (DO, fase 2)
#define PIN_BTN_MEAS   32     // tombol ukur manual (aktif HIGH, pull-down 10k) — tahan 3 dtk = mode AP
#define PIN_BTN_LCD    33     // tombol LCD (aktif HIGH, pull-down 10k)
#define PIN_VBAT       34     // pembagi 100k/100k dari VBAT
#define PIN_SW_AP      13     // switch MODE AP (slide switch ke 3V3 + pull-down 10k): ON = HIGH = mode AP

// ---------- Identitas & jaringan ----------
#define AP_PASSWORD    "airtrack123"   // GANTI sebelum alat dipasang (minimal 8 karakter)
#define DEFAULT_INTERVAL_S 900         // interval ukur+kirim awal (15 menit, sesuai Anggaran Daya)
#define MIN_INTERVAL_S 60              // sensor perlu ±45 detik aktif per siklus
#define MAX_INTERVAL_S 86400
#define API_PATH       "/api/ingest"   // dilengkapi otomatis bila alamat server hanya domain
#define ROOT_CA        ""              // PEM root CA server; kosong = HTTPS tanpa verifikasi sertifikat
#define USE_MQTT       1               // 0 = buang dukungan MQTT (tidak perlu library PubSubClient)
#define MQTT_TOPIC_FMT "airtracking/%s/data"

// ---------- Fase hardware ----------
#define USE_DO         0     // samakan dengan sensor_node/config.h (1 = nyalakan rail 12V saat ukur)

// ---------- Waktu (ms) ----------
#define WIFI_TIMEOUT_MS   20000
#define NODE_TIMEOUT_MS   60000    // tunggu hasil ukur dari node (otomatis +40 dtk bila USE_DO)
#define RAIL_SETTLE_MS    400
#define LCD_ON_MS         30000    // layar aktif setelah tombol LCD ditekan
#define LCD_PAGE_MS       3000
#define AP_IDLE_MS        (10UL * 60UL * 1000UL)   // AP ditutup bila 10 menit tanpa aktivitas
#define AP_FALLBACK_FAILS 3        // gagal WiFi berturut-turut sebelum AP dibuka otomatis
#define AP_FALLBACK_MS    (5UL * 60UL * 1000UL)    // lama AP otomatis (hemat baterai)
#define AP_SWITCH_IDLE_MS (30UL * 60UL * 1000UL)   // switch AP masih ON tapi 30 menit tanpa aktivitas: AP ditutup (hemat baterai)
#define USE_AP_BTN_HOLD   0        // 1 = tombol UKUR ditahan BTN_AP_HOLD_MS juga membuka AP (cadangan bila switch belum terpasang)
#define BTN_AP_HOLD_MS    3000
#define NTP_RESYNC_S      (7UL * 86400UL)

// ---------- Lain-lain ----------
#define LCD_TZ_OFFSET_H   7        // WIB; hanya untuk tampilan LCD (data dikirim dalam UTC)
#define SEND_CHUNK        20       // jumlah data per POST saat mengirim antrean SD
#define VBAT_SCALE        1.0f     // koreksi pembagi tegangan (ukur dengan multimeter)
#define MIN_VALID_EPOCH   1735689600UL   // 2025-01-01; di bawah ini jam dianggap belum benar
