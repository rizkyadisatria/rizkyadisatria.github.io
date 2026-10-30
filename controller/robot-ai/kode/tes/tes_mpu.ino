// TES MPU6050 saja (tanpa sensor cuaca).
// Sambungan: MPU6050 VCC -> VCC, GND -> GND, SCL -> D22, SDA -> D21 (header I2C papan 30P).
// Tidak butuh library tambahan. Buka Serial Monitor 115200.
#include <Wire.h>

const uint8_t MPU = 0x68;   // 0x69 kalau pin AD0 disambung ke VCC
bool adaMPU = false;

void tulisReg(uint8_t reg, uint8_t nilai) {
  Wire.beginTransmission(MPU); Wire.write(reg); Wire.write(nilai); Wire.endTransmission();
}

void setup() {
  Serial.begin(115200);
  delay(1500);   // beri waktu Serial Monitor tersambung
  Serial.println("\n=== TES MPU6050 ===");
  Wire.begin(21, 22);   // SDA, SCL

  Serial.println("Mencari perangkat I2C...");
  int jumlah = 0;
  for (byte a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) { Serial.printf("  ditemukan di 0x%02X\n", a); jumlah++; }
  }
  if (jumlah == 0) Serial.println("  tidak ada. Cek VCC, GND, dan SDA/SCL (mungkin tertukar).");

  Wire.beginTransmission(MPU);
  adaMPU = Wire.endTransmission() == 0;
  if (!adaMPU) { Serial.println("MPU6050 TIDAK ditemukan di 0x68."); return; }

  // WHO_AM_I: MPU6050 asli menjawab 0x68. Modul tiruan kadang menjawab angka lain tapi tetap bisa dipakai.
  Wire.beginTransmission(MPU); Wire.write(0x75); Wire.endTransmission(false);
  Wire.requestFrom(MPU, (uint8_t)1);
  Serial.printf("MPU6050 ditemukan. WHO_AM_I = 0x%02X\n", Wire.read());

  tulisReg(0x6B, 0x00);   // bangunkan dari mode tidur
  tulisReg(0x1C, 0x00);   // akselerometer +-2 g   (16384 = 1 g)
  tulisReg(0x1B, 0x00);   // giroskop +-250 deg/s  (131 = 1 deg/s)
  Serial.println("Taruh datar dulu, lalu miringkan ke depan-belakang dan kiri-kanan.\n");
}

void loop() {
  if (!adaMPU) {   // terus beri kabar supaya Serial Monitor tidak pernah kosong, dan coba cari lagi
    Wire.beginTransmission(MPU);
    if (Wire.endTransmission() == 0) { Serial.println("MPU6050 sekarang ditemukan, mulai ulang..."); delay(200); ESP.restart(); }
    Serial.println("MPU6050 belum ditemukan di 0x68. Cek SDA->D21, SCL->D22, VCC, GND. Tekan tombol EN untuk mengulang pencarian.");
    delay(2000); return;
  }

  Wire.beginTransmission(MPU); Wire.write(0x3B); Wire.endTransmission(false);
  if (Wire.requestFrom(MPU, (uint8_t)14) != 14) { Serial.println("gagal membaca, cek kabel"); delay(500); return; }
  int16_t ax = (Wire.read() << 8) | Wire.read();
  int16_t ay = (Wire.read() << 8) | Wire.read();
  int16_t az = (Wire.read() << 8) | Wire.read();
  int16_t tr = (Wire.read() << 8) | Wire.read();
  int16_t gx = (Wire.read() << 8) | Wire.read();
  int16_t gy = (Wire.read() << 8) | Wire.read();
  int16_t gz = (Wire.read() << 8) | Wire.read();

  float axg = ax / 16384.0, ayg = ay / 16384.0, azg = az / 16384.0;
  float pitch = atan2(-axg, sqrt(ayg * ayg + azg * azg)) * 57.3;
  float roll  = atan2(ayg, azg) * 57.3;
  float suhuChip = tr / 340.0 + 36.53;   // suhu chip, bukan suhu ruangan

  Serial.printf("pitch %6.1f  roll %6.1f  |  aks (g) x %5.2f y %5.2f z %5.2f  |  gyro (deg/s) %6.1f %6.1f %6.1f  |  chip %.1f C",
                pitch, roll, axg, ayg, azg, gx / 131.0, gy / 131.0, gz / 131.0, suhuChip);
  if (abs(pitch) > 40 || abs(roll) > 40) Serial.print("  <- MIRING (badan robot akan berhenti)");
  Serial.println();
  delay(300);
}
