// config.h — SENSOR NODE (ESP32 #2). Pin mengikuti sheet "Pinout (usulan)".
#pragma once

// ---------- Pin ----------
#define PIN_SDA        21     // I2C ADS1115 (0x48)
#define PIN_SCL        22
#define PIN_ONEWIRE    4      // DS18B20 DQ (+ pull-up 4.7k ke 3V3)
#define PIN_LINK_RX    26     // UART1 dari controller (TX2 = GPIO17 controller)
#define PIN_LINK_TX    27     // UART1 ke controller   (RX2 = GPIO16 controller)
#define PIN_WAKE       33     // WAKE in dari GPIO25 controller (ext0, aktif HIGH)
#define PIN_RS485_RX   16     // Fase 2 (DO): RO modul SP3485
#define PIN_RS485_TX   17     // Fase 2 (DO): DI modul SP3485
#define PIN_RS485_DE   19     // Fase 2 (DO): DE+RE disatukan, HIGH = kirim

// ---------- Kanal ADS1115 ----------
#define ADS_ADDR       0x48
#define ADS_CH_PH      0
#define ADS_CH_EC      1
#define ADS_CH_TURB    2      // lewat pembagi 10k/20k
#define TURB_DIV_RATIO 1.5f   // Vsensor = Vadc * (10k+20k)/20k

// ---------- Sensor DO (FASE 2) ----------
// 0 = fase 1 (field "do" dikirim null). 1 = baca DO lewat RS485 Modbus-RTU.
#define USE_DO         0
// VERIFIKASI semua nilai ini di datasheet SN-3002-LDO sebelum USE_DO = 1.
#define DO_MB_ADDR     1
#define DO_MB_BAUD     9600
#define DO_MB_REG      0x0000   // register awal
#define DO_MB_COUNT    2        // jumlah register dibaca
#define DO_MB_IDX      0        // indeks register yang berisi DO
#define DO_MB_SCALE    100.0f   // nilai register / SCALE = mg/L

// ---------- Waktu (ms) ----------
#define WARMUP_MS      15000    // stabilisasi probe sebelum sampling (pH, EC, turbidity)
#define WARMUP_DO_MS   30000    // tambahan bila DO dipakai (respons DO <= 60 detik)
#define WARMUP_CAL_MS  3000     // saat kalibrasi (probe sudah di larutan)
#define IDLE_TIMEOUT_MS 20000   // tidur bila tidak ada perintah selama ini
#define HOLD_IDLE_MS   600000   // mode AP (perintah "hold"): tetap bangun, tidur bila 10 menit tanpa perintah
#define N_SAMPLES      15       // sampel per kanal (median dari bagian tengah)

// ---------- Rumus sensor ----------
#define EC_RES2        (7500.0f / 0.66f)   // rangkaian DFRobot DFR0300
#define EC_REF         20.0f
#define EC_TEMP_COEF   0.0185f             // kompensasi suhu 1.85 %/°C terhadap 25 °C
#define TDS_FACTOR     0.5f                // TDS (ppm) = EC (µS/cm) * faktor (0.5 – 0.7)
// Default sebelum kalibrasi (DFRobot SEN0161-V2 @3.3V): pH7 = 1500 mV, pH4 = 2032 mV
#define PH_V7_DEFAULT  1500.0f
#define PH_V4_DEFAULT  2032.44f
#define TURB_V0_DEFAULT 4.2f               // tegangan air jernih (V, di sisi sensor)
