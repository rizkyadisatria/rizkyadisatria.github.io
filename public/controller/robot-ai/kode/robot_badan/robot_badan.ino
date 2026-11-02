/*
  ROBOT AI 4WD - BADAN (ESP32 DevKit V1)
  ---------------------------------------
  Mengurus: 4 motor TT lewat TB6612FNG, servo pan-tilt, 2 sensor jarak HC-SR04,
  MPU6050 (kemiringan), BME280 atau DHT11 (cuaca), tombol STOP, dan tegangan baterai.
  Menerima perintah teks dari ESP32-S3 (otak) lewat UART2, atau dari Serial Monitor.

  Board di Arduino IDE : ESP32 Dev Module
  Library             : Adafruit BME280 Library, DHT sensor library (Adafruit)
                        (keduanya otomatis meminta "Adafruit Unified Sensor" dan "Adafruit BusIO", pilih Install All)

  PERINTAH (satu baris, diakhiri Enter / newline):
    M <kiri> <kanan> [ms]   gerak. kiri/kanan -255..255. ms = lama (default 600, maks 3000)
    S                       berhenti
    P <pan> <tilt>          arahkan kepala, derajat 0..180
    Q                       minta data sekarang
    PING                    balas PONG
  BALASAN otomatis tiap 250 ms:
    D <depan_cm> <belakang_cm> <baterai_V> <suhu_C> <lembap_%> <pitch> <roll> <flag>
    flag: 1=STOP ditekan, 2=baterai lemah, 4=halangan depan, 8=halangan belakang, 16=miring
    Nilai -1 = sensor tidak terpasang / gagal baca.

  Aturan keselamatan ada DI SINI, bukan di otak:
    - Setiap gerakan berhenti sendiri setelah waktunya habis (otak hang = robot berhenti).
    - Halangan < JARAK_STOP_DEPAN cm: tidak boleh maju. Halangan belakang: tidak boleh mundur.
    - Tombol STOP: tekan sekali = kunci berhenti, tekan lagi = lepas.
    - Baterai di bawah BATERAI_MIN: motor dimatikan sampai board di-restart.
    - Miring lebih dari 40 derajat (terbalik/terangkat miring): motor berhenti.
*/

#include <Wire.h>
#include <Adafruit_BME280.h>
#include <DHT.h>

// ================== PENGATURAN (boleh diubah) ==================
const int   JARAK_STOP_DEPAN    = 20;    // cm
const int   JARAK_STOP_BELAKANG = 15;    // cm
const float BATERAI_MIN         = 6.4;   // volt total 2 sel (3,2 V per sel)
float       BATERAI_FAKTOR      = 3.0;   // pembagi 20k/10k. Kalibrasi: lihat panduan Tahap 6
const bool  MOTOR_KIRI_TERBALIK  = false; // ubah ke true kalau roda kiri berputar terbalik
const bool  MOTOR_KANAN_TERBALIK = false;
const int   SERVO_MIN_US = 600;          // lebar pulsa sudut 0
const int   SERVO_MAX_US = 2400;         // lebar pulsa sudut 180
const int   TILT_MIN = 40, TILT_MAX = 150; // batasi supaya kepala tidak membentur bracket

// ================== PIN (sesuai panduan) ==================
#define PIN_PWMA   25   // TB6612 kanal A = motor kiri
#define PIN_AIN1   26
#define PIN_AIN2   27
#define PIN_PWMB   13   // TB6612 kanal B = motor kanan
#define PIN_BIN1   32
#define PIN_BIN2   33
#define PIN_PAN    23   // servo pan (kiri-kanan)
#define PIN_TILT    4   // servo tilt (atas-bawah)
#define PIN_TRIG_D 18   // HC-SR04 depan
#define PIN_ECHO_D 19   // lewat pembagi 1k/2k!
#define PIN_TRIG_B 14   // HC-SR04 belakang
#define PIN_ECHO_B 35   // lewat pembagi 1k/2k! (GPIO 35 hanya input, cocok untuk ECHO)
#define PIN_TOMBOL 39   // tombol STOP ke GND + resistor 10k ke 3V3 (GPIO 39 tidak punya pull-up internal)
#define PIN_BATT   34   // pembagi 20k (ke baterai) / 10k (ke GND)
#define PIN_DHT     5   // DHT11, hanya dipakai kalau BME280 tidak ditemukan
#define PIN_LED     2   // LED biru bawaan DevKit
#define PIN_RX2    16   // dari TX ESP32-S3 (GPIO 17)
#define PIN_TX2    17   // ke RX ESP32-S3 (GPIO 18)

// ================== VARIABEL ==================
Adafruit_BME280 bme;
DHT dht(PIN_DHT, DHT11);
bool adaBME = false, adaMPU = false;

int  targetKiri = 0, targetKanan = 0;     // kecepatan yang diminta
int  nowKiri = 0, nowKanan = 0;           // kecepatan sekarang (naik pelan-pelan)
unsigned long gerakSampai = 0;            // millis() kapan gerakan habis

int  panTarget = 90, tiltTarget = 90;
float panNow = 90, tiltNow = 90;

int   jarakDepan = -1, jarakBelakang = -1;
float baterai = -1, suhu = -1, lembap = -1, pitch = 0, roll = 0;
bool  kunciStop = false, bateraiLemah = false;

String bufUSB, bufUART;

// ================== MOTOR ==================
void tulisMotor(int pwmPin, int in1, int in2, int v) {
  v = constrain(v, -255, 255);
  if (v > 0)      { digitalWrite(in1, HIGH); digitalWrite(in2, LOW);  }
  else if (v < 0) { digitalWrite(in1, LOW);  digitalWrite(in2, HIGH); }
  else            { digitalWrite(in1, HIGH); digitalWrite(in2, HIGH); } // rem pendek
  ledcWrite(pwmPin, abs(v));
}

void berhenti() {
  targetKiri = targetKanan = 0;
  gerakSampai = 0;
}

bool bolehGerak(int kiri, int kanan) {
  if (kunciStop || bateraiLemah) return false;
  if (adaMPU && (abs(pitch) > 40 || abs(roll) > 40)) return false;
  bool maju = (kiri + kanan) > 0, mundur = (kiri + kanan) < 0;
  if (maju && jarakDepan > 0 && jarakDepan < JARAK_STOP_DEPAN) return false;
  if (mundur && jarakBelakang > 0 && jarakBelakang < JARAK_STOP_BELAKANG) return false;
  return true;
}

void updateMotor() {
  static unsigned long t = 0;
  if (millis() - t < 10) return;
  t = millis();
  if (gerakSampai && millis() > gerakSampai) berhenti();      // waktu habis
  if (!bolehGerak(targetKiri, targetKanan)) { targetKiri = targetKanan = 0; }
  // naik pelan supaya arus awal tidak membuat tegangan drop; turun langsung (lebih aman)
  auto dekati = [](int now, int target) {
    if (target == 0) return 0;
    if (abs(target - now) <= 20) return target;
    return now + (target > now ? 20 : -20);
  };
  nowKiri = dekati(nowKiri, targetKiri);
  nowKanan = dekati(nowKanan, targetKanan);
  tulisMotor(PIN_PWMA, PIN_AIN1, PIN_AIN2, MOTOR_KIRI_TERBALIK ? -nowKiri : nowKiri);
  tulisMotor(PIN_PWMB, PIN_BIN1, PIN_BIN2, MOTOR_KANAN_TERBALIK ? -nowKanan : nowKanan);
}

// ================== SERVO ==================
void tulisServo(int pin, float sudut) {
  int us = map((int)(sudut * 10), 0, 1800, SERVO_MIN_US, SERVO_MAX_US);
  ledcWrite(pin, (uint32_t)us * 16383UL / 20000UL);   // 50 Hz, 14 bit
}

void updateServo() {
  static unsigned long t = 0;
  if (millis() - t < 15) return;
  t = millis();
  // gerak 3 derajat per 15 ms: halus dan tidak menarik arus besar sekaligus
  if (abs(panTarget - panNow) > 3) panNow += (panTarget > panNow ? 3 : -3); else panNow = panTarget;
  if (abs(tiltTarget - tiltNow) > 3) tiltNow += (tiltTarget > tiltNow ? 3 : -3); else tiltNow = tiltTarget;
  tulisServo(PIN_PAN, panNow);
  tulisServo(PIN_TILT, tiltNow);
}

// ================== SENSOR ==================
int bacaJarak(int trig, int echo) {
  digitalWrite(trig, LOW);  delayMicroseconds(3);
  digitalWrite(trig, HIGH); delayMicroseconds(10);
  digitalWrite(trig, LOW);
  unsigned long d = pulseIn(echo, HIGH, 25000UL);   // maks ~4 m
  if (d == 0) return -1;                            // tidak ada pantulan / tidak terpasang
  return (int)(d / 58);
}

void updateJarak() {
  static unsigned long t = 0;
  static bool giliranDepan = true;
  if (millis() - t < 40) return;                    // gantian supaya gema tidak saling ganggu
  t = millis();
  if (giliranDepan) jarakDepan = bacaJarak(PIN_TRIG_D, PIN_ECHO_D);
  else              jarakBelakang = bacaJarak(PIN_TRIG_B, PIN_ECHO_B);
  giliranDepan = !giliranDepan;
}

bool mulaiMPU() {
  Wire.beginTransmission(0x68);
  if (Wire.endTransmission() != 0) return false;
  Wire.beginTransmission(0x68); Wire.write(0x6B); Wire.write(0); Wire.endTransmission(); // bangunkan
  return true;
}

void updateMPU() {
  static unsigned long t = 0;
  if (!adaMPU || millis() - t < 50) return;
  t = millis();
  Wire.beginTransmission(0x68); Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) return;
  if (Wire.requestFrom(0x68, 6) != 6) return;
  int16_t ax = (Wire.read() << 8) | Wire.read();
  int16_t ay = (Wire.read() << 8) | Wire.read();
  int16_t az = (Wire.read() << 8) | Wire.read();
  pitch = atan2(-ax, sqrt((float)ay * ay + (float)az * az)) * 57.3;
  roll  = atan2(ay, az) * 57.3;
}

void updateCuaca() {
  static unsigned long t = 0;
  if (millis() - t < 2000) return;
  t = millis();
  if (adaBME) { suhu = bme.readTemperature(); lembap = bme.readHumidity(); }
  else {
    float s = dht.readTemperature(), h = dht.readHumidity();
    suhu = isnan(s) ? -1 : s; lembap = isnan(h) ? -1 : h;
  }
}

void updateBaterai() {
  static unsigned long t = 0;
  static int hitungLemah = 0;
  if (millis() - t < 200) return;
  t = millis();
  uint32_t mv = 0;
  for (int i = 0; i < 8; i++) mv += analogReadMilliVolts(PIN_BATT);
  float v = (mv / 8.0) / 1000.0 * BATERAI_FAKTOR;
  baterai = (baterai < 0) ? v : baterai * 0.8 + v * 0.2;   // dihaluskan
  if (baterai < 1.0) { baterai = -1; return; }              // pembagi belum dipasang
  // lemah harus terbaca 3 detik berturut-turut (lonjakan saat motor start tidak dihitung)
  hitungLemah = (baterai < BATERAI_MIN) ? hitungLemah + 1 : 0;
  if (hitungLemah > 15 && !bateraiLemah) { bateraiLemah = true; berhenti(); kirimSemua("E BATERAI_LEMAH"); }
}

void updateTombol() {
  static bool lalu = HIGH;
  static unsigned long tUbah = 0;
  bool now = digitalRead(PIN_TOMBOL);
  if (now != lalu && millis() - tUbah > 40) {        // debounce 40 ms
    tUbah = millis();
    lalu = now;
    if (now == LOW) {                                 // baru ditekan
      kunciStop = !kunciStop;
      berhenti();
      kirimSemua(kunciStop ? "E STOP_DIKUNCI" : "E STOP_DILEPAS");
    }
  }
}

// ================== KOMUNIKASI ==================
void kirimSemua(const String &s) { Serial.println(s); Serial2.println(s); }

int flagSekarang() {
  int f = 0;
  if (kunciStop) f |= 1;
  if (bateraiLemah) f |= 2;
  if (jarakDepan > 0 && jarakDepan < JARAK_STOP_DEPAN) f |= 4;
  if (jarakBelakang > 0 && jarakBelakang < JARAK_STOP_BELAKANG) f |= 8;
  if (adaMPU && (abs(pitch) > 40 || abs(roll) > 40)) f |= 16;
  return f;
}

String dataSekarang() {
  char b[96];
  snprintf(b, sizeof(b), "D %d %d %.2f %.1f %.0f %.0f %.0f %d",
           jarakDepan, jarakBelakang, baterai, suhu, lembap,
           adaMPU ? pitch : -1.0f, adaMPU ? roll : -1.0f, flagSekarang());
  return String(b);
}

void jalankan(String c) {
  c.trim();
  if (c.length() == 0) return;
  char cmd[8] = {0};
  int a = 0, b = 0, d = 0;
  int n = sscanf(c.c_str(), "%7s %d %d %d", cmd, &a, &b, &d);
  String k = String(cmd); k.toUpperCase();
  if (k == "M" && n >= 3) {
    if (!bolehGerak(a, b)) { kirimSemua("E DITOLAK " + String(flagSekarang())); berhenti(); return; }
    targetKiri = constrain(a, -255, 255);
    targetKanan = constrain(b, -255, 255);
    int ms = (n >= 4) ? constrain(d, 50, 3000) : 600;
    gerakSampai = millis() + ms;
  } else if (k == "S") {
    berhenti();
  } else if (k == "P" && n >= 3) {
    panTarget = constrain(a, 0, 180);
    tiltTarget = constrain(b, TILT_MIN, TILT_MAX);
  } else if (k == "Q") {
    kirimSemua(dataSekarang());
  } else if (k == "PING") {
    kirimSemua("PONG");
  } else {
    kirimSemua("E TIDAK_DIKENAL " + c);
  }
}

void bacaBaris(Stream &s, String &buf) {
  while (s.available()) {
    char ch = s.read();
    if (ch == '\n' || ch == '\r') { if (buf.length()) jalankan(buf); buf = ""; }
    else if (buf.length() < 60) buf += ch;
  }
}

// ================== SETUP & LOOP ==================
void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, PIN_RX2, PIN_TX2);

  pinMode(PIN_AIN1, OUTPUT); pinMode(PIN_AIN2, OUTPUT);
  pinMode(PIN_BIN1, OUTPUT); pinMode(PIN_BIN2, OUTPUT);
  ledcAttach(PIN_PWMA, 20000, 8);   // 20 kHz: tidak berdenging
  ledcAttach(PIN_PWMB, 20000, 8);
  tulisMotor(PIN_PWMA, PIN_AIN1, PIN_AIN2, 0);
  tulisMotor(PIN_PWMB, PIN_BIN1, PIN_BIN2, 0);

  ledcAttach(PIN_PAN, 50, 14);
  ledcAttach(PIN_TILT, 50, 14);
  tulisServo(PIN_PAN, 90); tulisServo(PIN_TILT, 90);

  pinMode(PIN_TRIG_D, OUTPUT); pinMode(PIN_ECHO_D, INPUT);
  pinMode(PIN_TRIG_B, OUTPUT); pinMode(PIN_ECHO_B, INPUT);
  pinMode(PIN_TOMBOL, INPUT);       // pull-up 10k eksternal
  pinMode(PIN_LED, OUTPUT);
  analogSetPinAttenuation(PIN_BATT, ADC_11db);

  Wire.begin(21, 22);
  adaMPU = mulaiMPU();
  adaBME = bme.begin(0x76) || bme.begin(0x77);
  if (!adaBME) dht.begin();

  Serial.println();
  Serial.println("=== ROBOT BADAN SIAP ===");
  Serial.printf("MPU6050: %s | BME280: %s (kalau tidak, pakai DHT11 di GPIO %d)\n",
                adaMPU ? "ada" : "TIDAK ADA", adaBME ? "ada" : "TIDAK ADA", PIN_DHT);
  Serial.println("Coba ketik: M 150 150 1000   lalu Enter");
  kirimSemua("E SIAP");
}

void loop() {
  bacaBaris(Serial, bufUSB);
  bacaBaris(Serial2, bufUART);
  updateTombol();
  updateJarak();
  updateMPU();
  updateCuaca();
  updateBaterai();
  updateMotor();
  updateServo();

  static unsigned long tKirim = 0;
  if (millis() - tKirim > 250) { tKirim = millis(); Serial2.println(dataSekarang()); }

  static unsigned long tCetak = 0;                    // ke Serial Monitor lebih jarang
  if (millis() - tCetak > 1000) { tCetak = millis(); Serial.println(dataSekarang()); }

  // LED: menyala = siap, berkedip cepat = STOP dikunci / baterai lemah
  digitalWrite(PIN_LED, (kunciStop || bateraiLemah) ? (millis() / 150) % 2 : HIGH);
}
