// TES BATERAI DAN TOMBOL STOP
// Baterai lewat pembagi 20k/10k ke GPIO 34. Tombol ke GPIO 39 (+ resistor 10k ke 3V3).
float FAKTOR = 3.0;      // ubah setelah dibandingkan dengan multimeter (lihat panduan)

void setup() {
  Serial.begin(115200);
  pinMode(39, INPUT);
  analogSetPinAttenuation(34, ADC_11db);
}

void loop() {
  uint32_t mv = 0;
  for (int i = 0; i < 16; i++) mv += analogReadMilliVolts(34);
  mv /= 16;
  Serial.printf("pin 34: %4u mV  ->  baterai: %.2f V   tombol: %s\n",
                mv, mv / 1000.0 * FAKTOR, digitalRead(39) == LOW ? "DITEKAN" : "lepas");
  delay(500);
}
