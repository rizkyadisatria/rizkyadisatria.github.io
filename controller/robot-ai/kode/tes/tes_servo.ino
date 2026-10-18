// TES SERVO: kepala menoleh kiri-tengah-kanan, lalu mengangguk.
// Ketik dua angka di Serial Monitor, misalnya "90 90" lalu Enter, untuk mengatur sendiri.
#define PAN  23
#define TILT 4

void sudut(int pin, int derajat) {
  int us = map(derajat, 0, 180, 600, 2400);        // lebar pulsa 0,6 - 2,4 ms
  ledcWrite(pin, (uint32_t)us * 16383 / 20000);    // 50 Hz, resolusi 14 bit
}

void setup() {
  Serial.begin(115200);
  ledcAttach(PAN, 50, 14);
  ledcAttach(TILT, 50, 14);
  sudut(PAN, 90); sudut(TILT, 90);
  delay(1000);
  int urutan[] = {90, 45, 90, 135, 90};
  for (int s : urutan) { Serial.printf("pan %d\n", s); sudut(PAN, s); delay(800); }
  int angguk[] = {90, 60, 110, 90};
  for (int s : angguk) { Serial.printf("tilt %d\n", s); sudut(TILT, s); delay(800); }
  Serial.println("Selesai. Ketik: pan tilt  (contoh: 90 70)");
}

void loop() {
  if (Serial.available()) {
    int p = Serial.parseInt(), t = Serial.parseInt();
    p = constrain(p, 0, 180); t = constrain(t, 40, 150);
    sudut(PAN, p); sudut(TILT, t);
    Serial.printf("pan %d  tilt %d\n", p, t);
    while (Serial.available()) Serial.read();
  }
}
