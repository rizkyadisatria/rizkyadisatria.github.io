// TES SENSOR JARAK: cetak jarak depan dan belakang dalam cm.
#define TRIG_D 18
#define ECHO_D 19
#define TRIG_B 14
#define ECHO_B 35

int jarak(int trig, int echo) {
  digitalWrite(trig, LOW);  delayMicroseconds(3);
  digitalWrite(trig, HIGH); delayMicroseconds(10);
  digitalWrite(trig, LOW);
  unsigned long d = pulseIn(echo, HIGH, 25000UL);   // tunggu maks 25 ms (~4 m)
  return d ? d / 58 : -1;                            // -1 = tidak ada pantulan
}

void setup() {
  Serial.begin(115200);
  delay(1500);   // beri waktu Serial Monitor tersambung
  Serial.println("\n=== TES JARAK ===");
  pinMode(TRIG_D, OUTPUT); pinMode(ECHO_D, INPUT);
  pinMode(TRIG_B, OUTPUT); pinMode(ECHO_B, INPUT);
}

void loop() {
  int d = jarak(TRIG_D, ECHO_D);
  delay(40);                                         // jeda supaya gema sensor depan tidak terbaca sensor belakang
  int b = jarak(TRIG_B, ECHO_B);
  Serial.printf("depan: %4d cm   belakang: %4d cm\n", d, b);
  delay(300);
}
