// TES I2C: cari alamat perangkat, lalu baca MPU6050 (kemiringan) dan BME280 atau DHT11 (cuaca).
// Alamat yang benar: 0x68 = MPU6050, 0x76 atau 0x77 = BME280.
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <DHT.h>

Adafruit_BME280 bme;
DHT dht(5, DHT11);
bool adaBME = false, adaMPU = false;

void setup() {
  Serial.begin(115200);
  delay(500);
  Wire.begin(21, 22);
  Serial.println("Mencari perangkat I2C...");
  for (byte a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) Serial.printf("  ditemukan di 0x%02X\n", a);
  }
  Wire.beginTransmission(0x68);
  adaMPU = Wire.endTransmission() == 0;
  if (adaMPU) { Wire.beginTransmission(0x68); Wire.write(0x6B); Wire.write(0); Wire.endTransmission(); }
  adaBME = bme.begin(0x76) || bme.begin(0x77);
  if (!adaBME) dht.begin();
  Serial.printf("MPU6050: %s   BME280: %s\n", adaMPU ? "ada" : "TIDAK", adaBME ? "ada" : "TIDAK (pakai DHT11 di GPIO 5)");
}

void loop() {
  if (adaMPU) {
    Wire.beginTransmission(0x68); Wire.write(0x3B); Wire.endTransmission(false);
    Wire.requestFrom(0x68, 6);
    int16_t ax = (Wire.read() << 8) | Wire.read();
    int16_t ay = (Wire.read() << 8) | Wire.read();
    int16_t az = (Wire.read() << 8) | Wire.read();
    float pitch = atan2(-ax, sqrt((float)ay * ay + (float)az * az)) * 57.3;
    float roll  = atan2(ay, az) * 57.3;
    Serial.printf("pitch %6.1f  roll %6.1f   ", pitch, roll);
  }
  if (adaBME) Serial.printf("suhu %.1f C  lembap %.0f %%  tekanan %.0f hPa\n", bme.readTemperature(), bme.readHumidity(), bme.readPressure() / 100);
  else Serial.printf("suhu %.1f C  lembap %.0f %%\n", dht.readTemperature(), dht.readHumidity());
  delay(1000);
}
