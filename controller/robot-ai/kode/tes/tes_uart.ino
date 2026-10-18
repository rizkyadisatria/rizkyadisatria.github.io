// TES SAMBUNGAN OTAK-BADAN (upload ke ESP32-S3; badan sudah berisi robot_badan.ino)
// Ketik perintah di Serial Monitor S3, misalnya PING atau M 150 150 800, lalu Enter.
#define BADAN_TX 17
#define BADAN_RX 18

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, BADAN_RX, BADAN_TX);
  delay(500);
  Serial.println("Mengirim PING ke badan...");
  Serial1.println("PING");
}

void loop() {
  while (Serial.available()) Serial1.write(Serial.read());   // dari laptop ke badan
  static String baris;
  while (Serial1.available()) {                              // dari badan ke laptop
    char c = Serial1.read();
    if (c == '\n') {
      if (!baris.startsWith("D ") || millis() % 2000 < 260) Serial.println("badan: " + baris);  // data D dicetak jarang
      baris = "";
    } else if (c != '\r') baris += c;
  }
}
