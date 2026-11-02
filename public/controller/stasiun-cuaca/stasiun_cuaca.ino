/*
  ==========================================================
   STASIUN CUACA KAMAR + BOT TELEGRAM
   ESP32 DevKit (ESP-WROOM-32) | DHT11 | LDR | LCD 16x2 | LED | Buzzer
  ==========================================================

  Library (Arduino IDE > Tools > Manage Libraries):
    - "DHT sensor library" oleh Adafruit  (ikut install "Adafruit Unified Sensor")
    - "UniversalTelegramBot" oleh Brian Lough
    - "ArduinoJson" oleh Benoit Blanchon
    - LiquidCrystal (bawaan Arduino)
  Board: Tools > Board > esp32 > "ESP32 Dev Module"

  Perintah bot:
    /status        -> suhu, kelembapan, heat index, cahaya
    /setmax 31     -> ubah batas suhu peringatan (15-45 C)
    /alarm_on      -> aktifkan peringatan
    /alarm_off     -> matikan peringatan
    /help          -> daftar perintah
*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <LiquidCrystal.h>
#include <time.h>

// =================== ISI BAGIAN INI ===================
const char* WIFI_SSID = "NAMA_WIFI";
const char* WIFI_PASS = "PASSWORD_WIFI";
const char* BOT_TOKEN = "123456789:ISI_TOKEN_DARI_BOTFATHER";
const char* CHAT_ID   = "123456789";   // chat ID kamu (dari @userinfobot)

#define USE_LCD         0   // ganti 1 kalau LCD sudah dipasang (Tahap 7)
#define BUZZER_PASSIVE  0   // ganti 1 kalau buzzer-mu pasif
// ======================================================

// ---------- Pin ----------
const int PIN_DHT    = 4;
const int PIN_LDR    = 34;   // ADC1: tetap bisa dibaca saat WiFi aktif
const int PIN_LED    = 25;
const int PIN_BUZZER = 26;
// LCD mode 4-bit: RS, E, D4, D5, D6, D7  (RW LCD disambung ke GND)
const int LCD_RS = 19, LCD_E = 23, LCD_D4 = 18, LCD_D5 = 17, LCD_D6 = 16, LCD_D7 = 27;

// ---------- Objek ----------
DHT dht(PIN_DHT, DHT11);
WiFiClientSecure secured;
UniversalTelegramBot bot(BOT_TOKEN, secured);
#if USE_LCD
LiquidCrystal lcd(LCD_RS, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
#endif

// ---------- Data & pengaturan ----------
float suhu = NAN, lembap = NAN, heatIndex = NAN;
int   cahaya = -1;              // persen relatif, BUKAN lux
float batasSuhu = 30.0;         // batas peringatan awal (C)
const float HISTERESIS = 1.0;   // peringatan reset setelah turun 1 C di bawah batas
bool  alarmAktif = true;
bool  sudahPeringatan = false;

// ---------- Timing non-blocking ----------
unsigned long tSensor = 0, tBot = 0, tWifi = 0;
const unsigned long INTERVAL_SENSOR = 2000;   // DHT11 maksimal 1x per detik
const unsigned long INTERVAL_BOT    = 3000;
const unsigned long INTERVAL_WIFI   = 10000;

// ======================================================

void bunyi(int ms) {
#if BUZZER_PASSIVE
  tone(PIN_BUZZER, 2000, ms);
#else
  digitalWrite(PIN_BUZZER, HIGH);
  delay(ms);
  digitalWrite(PIN_BUZZER, LOW);
#endif
}

#if USE_LCD
void cetakBaris(int baris, const char* teks) {
  char buf[17];
  snprintf(buf, sizeof(buf), "%-16s", teks);   // isi sisa baris dengan spasi
  lcd.setCursor(0, baris);
  lcd.print(buf);
}
#endif

String waktuSekarang() {
  struct tm info;
  if (!getLocalTime(&info, 100)) return "";
  char buf[24];
  strftime(buf, sizeof(buf), " (%d/%m %H:%M)", &info);
  return String(buf);
}

int bacaCahayaPersen() {
  long total = 0;
  for (int i = 0; i < 10; i++) {      // rata-rata 10 sampel untuk meredam noise
    total += analogRead(PIN_LDR);
    delay(2);
  }
  return map(total / 10, 0, 4095, 0, 100);
}

void bacaSensor() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (isnan(t) || isnan(h)) {
    Serial.println("Gagal baca DHT11 - cek kabel dan resistor pull-up 10k");
  } else {
    suhu = t;
    lembap = h;
    heatIndex = dht.computeHeatIndex(t, h, false);   // false = Celsius
  }
  cahaya = bacaCahayaPersen();

  Serial.printf("Suhu %.1f C | Lembap %.0f %% | HI %.1f C | Cahaya %d %%\n",
                suhu, lembap, heatIndex, cahaya);
}

void tampilLCD() {
#if USE_LCD
  char b1[17], b2[17];
  if (isnan(suhu)) {
    snprintf(b1, sizeof(b1), "Sensor error");
    snprintf(b2, sizeof(b2), "Cek kabel DHT11");
  } else {
    snprintf(b1, sizeof(b1), "T:%4.1fC H:%3.0f%%", suhu, lembap);
    snprintf(b2, sizeof(b2), "HI:%4.1fC L:%3d%%%c", heatIndex, cahaya,
             WiFi.status() == WL_CONNECTED ? '*' : ' ');   // * = WiFi tersambung
  }
  cetakBaris(0, b1);
  cetakBaris(1, b2);
#endif
}

String teksStatus() {
  if (isnan(suhu)) return "Sensor DHT11 belum terbaca. Cek kabel dan resistor pull-up.";
  String s = "Kondisi kamar" + waktuSekarang();
  s += "\nSuhu: " + String(suhu, 1) + " C";
  s += "\nKelembapan: " + String(lembap, 0) + " %";
  s += "\nTerasa seperti: " + String(heatIndex, 1) + " C";
  s += "\nCahaya: " + String(cahaya) + " % (relatif)";
  s += "\nBatas peringatan: " + String(batasSuhu, 1) + " C (";
  s += alarmAktif ? "aktif)" : "mati)";
  return s;
}

String teksBantuan() {
  return "Perintah:\n"
         "/status - kondisi kamar sekarang\n"
         "/setmax 31 - ubah batas suhu peringatan\n"
         "/alarm_on - aktifkan peringatan\n"
         "/alarm_off - matikan peringatan";
}

void tanganiPesan(int jumlah) {
  for (int i = 0; i < jumlah; i++) {
    String chatId = bot.messages[i].chat_id;
    String teks   = bot.messages[i].text;
    teks.trim();

    // Hanya kamu yang boleh memberi perintah
    if (chatId != CHAT_ID) {
      bot.sendMessage(chatId, "Akses ditolak.", "");
      continue;
    }

    if (teks.startsWith("/start") || teks == "/help") {
      bot.sendMessage(chatId, teksBantuan(), "");
    } else if (teks == "/status") {
      bot.sendMessage(chatId, teksStatus(), "");
    } else if (teks.startsWith("/setmax")) {
      float v = teks.substring(7).toFloat();
      if (v >= 15 && v <= 45) {
        batasSuhu = v;
        sudahPeringatan = false;
        digitalWrite(PIN_LED, LOW);
        bot.sendMessage(chatId, "Batas suhu diubah ke " + String(batasSuhu, 1) + " C.", "");
      } else {
        bot.sendMessage(chatId, "Format: /setmax 31  (rentang 15-45)", "");
      }
    } else if (teks == "/alarm_on") {
      alarmAktif = true;
      sudahPeringatan = false;
      bot.sendMessage(chatId, "Peringatan suhu diaktifkan.", "");
    } else if (teks == "/alarm_off") {
      alarmAktif = false;
      digitalWrite(PIN_LED, LOW);
      bot.sendMessage(chatId, "Peringatan suhu dimatikan.", "");
    } else {
      bot.sendMessage(chatId, "Perintah tidak dikenal.\n\n" + teksBantuan(), "");
    }
  }
}

void cekAlarm() {
  if (isnan(suhu) || !alarmAktif) {
    digitalWrite(PIN_LED, LOW);
    return;
  }
  if (!sudahPeringatan && suhu >= batasSuhu) {
    sudahPeringatan = true;
    digitalWrite(PIN_LED, HIGH);
    bunyi(300);
    if (WiFi.status() == WL_CONNECTED) {
      bot.sendMessage(CHAT_ID, "PERINGATAN: suhu kamar " + String(suhu, 1) +
                      " C, melewati batas " + String(batasSuhu, 1) + " C.", "");
    }
  } else if (sudahPeringatan && suhu <= batasSuhu - HISTERESIS) {
    sudahPeringatan = false;
    digitalWrite(PIN_LED, LOW);
    if (WiFi.status() == WL_CONNECTED) {
      bot.sendMessage(CHAT_ID, "Suhu kembali normal: " + String(suhu, 1) + " C.", "");
    }
  }
}

void sambungWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Menyambung WiFi");
  unsigned long mulai = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - mulai < 15000) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(" OK, IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(" gagal, akan dicoba lagi otomatis");
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  dht.begin();

#if USE_LCD
  lcd.begin(16, 2);
  cetakBaris(0, "Stasiun Cuaca");
  cetakBaris(1, "Menyambung WiFi");
#endif

  sambungWiFi();
  configTime(8 * 3600, 0, "pool.ntp.org", "time.google.com");   // WITA (UTC+8)
  secured.setCACert(TELEGRAM_CERTIFICATE_ROOT);                  // HTTPS ke api.telegram.org

  if (WiFi.status() == WL_CONNECTED) {
    bot.setMyCommands(F("[{\"command\":\"status\",\"description\":\"Kondisi kamar sekarang\"},"
                        "{\"command\":\"setmax\",\"description\":\"Ubah batas suhu, contoh: /setmax 31\"},"
                        "{\"command\":\"alarm_on\",\"description\":\"Aktifkan peringatan\"},"
                        "{\"command\":\"alarm_off\",\"description\":\"Matikan peringatan\"}]"));
    bot.sendMessage(CHAT_ID, "Stasiun cuaca menyala. Ketik /help untuk daftar perintah.", "");
  }

  delay(2000);   // beri waktu DHT11 stabil
  bacaSensor();
  tampilLCD();
}

void loop() {
  unsigned long sekarang = millis();

  if (sekarang - tSensor >= INTERVAL_SENSOR) {
    tSensor = sekarang;
    bacaSensor();
    tampilLCD();
    cekAlarm();
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (sekarang - tBot >= INTERVAL_BOT) {
      tBot = sekarang;
      int n = bot.getUpdates(bot.last_message_received + 1);
      while (n) {
        tanganiPesan(n);
        n = bot.getUpdates(bot.last_message_received + 1);
      }
    }
  } else if (sekarang - tWifi >= INTERVAL_WIFI) {
    tWifi = sekarang;
    Serial.println("WiFi terputus, mencoba menyambung ulang...");
    WiFi.reconnect();
  }
}
