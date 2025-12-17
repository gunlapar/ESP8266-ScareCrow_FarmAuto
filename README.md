# ESP8266-ScareCrow_FarmAuto
ESP8266 Smart Scarecrow (Blynk + OLED) — sistem deteksi hama berbasis PIR &amp; vibration dengan anti-overlap lock, alarm multi-frequency buzzer, monitoring DHT11 + soil moisture, serta notifikasi &amp; dashboard via Blynk IoT, ditampilkan juga ke OLED SSD1306.

# Smart Scarecrow (ESP8266 + Blynk + OLED)

Sistem “Smart Scarecrow” berbasis **ESP8266** untuk mendeteksi gangguan/hama menggunakan **PIR** dan **vibration sensor**, lalu menjalankan **buzzer multi-frequency** sebagai deterrent. Sistem ini memiliki **anti-overlap/lock** agar trigger sensor tidak saling tumpang tindih, mengirim status ke **Blynk IoT**, dan menampilkan informasi real-time di **OLED SSD1306 (I2C)**.

## Fitur Utama
- Deteksi hama berbasis:
  - **Vibration sensor** (priority 1)
  - **PIR motion** (priority 2)
- **Anti-overlap state machine** (lock durasi tertentu agar pembacaan rapi)
- **Multi-frequency buzzer pattern** (frekuensi berganti untuk deterrent)
- Monitoring lingkungan:
  - **DHT11**: suhu & kelembapan
  - **Soil moisture** via **A0**
- **OLED SSD1306**:
  - Boot screen
  - Status sistem & sensor
  - Alert “PEST DETECTED!” dengan blink
- **Blynk IoT**:
  - Virtual pin telemetry (V0–V9)
  - Event log (`pest_detected`)
  - Tombol manual test buzzer dan reset counter

## Hardware
Disarankan:
- ESP8266: NodeMCU / Wemos D1 Mini
- OLED SSD1306 I2C 128x64 (alamat umum: `0x3C` / `0x3D`)
- PIR sensor (HC-SR501 / sejenis)
- Vibration sensor module (SW-420 / sejenis)
- Buzzer aktif/pasif (kode menggunakan `tone()`)
- DHT11 + pull-up sesuai modul
- Soil moisture sensor analog ke A0

## Pin Mapping (ESP8266)
| Komponen | Pin Board | Catatan |
|---|---|---|
| OLED SDA | D2 | I2C default |
| OLED SCL | D1 | I2C default |
| PIR | D6 | input |
| Vibration | D7 | input |
| Buzzer | D3 | output (`tone/noTone`) |
| DHT11 | D5 | input |
| Soil Moisture | A0 | analog input |

## Blynk Virtual Pins
| Data/Control | Virtual Pin |
|---|---|
| Temperature | V0 |
| Humidity | V1 |
| Soil Moisture (%) | V2 |
| Daily Pest Count | V3 |
| Pest Detected Flag (0/1) | V4 |
| Connection/Status Text | V5 |
| Manual Buzzer Trigger | V6 |
| Reset Count | V7 |
| Vibration Status (0/1) | V8 |
| PIR Status (0/1) | V9 |

> Pastikan widget di Blynk diset sesuai Virtual Pin di atas.

## Dependensi Library (Arduino IDE)
Install via Library Manager:
- **Blynk** (Blynk IoT)
- **DHT sensor library**
- **Adafruit SSD1306**
- **Adafruit GFX Library**

Board:
- ESP8266 board package (Board Manager)

## Cara Setup
1. Clone repo ini.
2. Buka project di **Arduino IDE** / **PlatformIO**.
3. Isi kredensial Blynk:
   ```cpp
   #define BLYNK_TEMPLATE_ID ""
   #define BLYNK_TEMPLATE_NAME ""
   #define BLYNK_AUTH_TOKEN ""
