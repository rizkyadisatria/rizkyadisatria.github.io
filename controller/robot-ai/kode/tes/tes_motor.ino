// TES MOTOR: roda kiri maju, kiri mundur, kanan maju, kanan mundur. Roda harus TERANGKAT.
#define PWMA 25
#define AIN1 26
#define AIN2 27
#define PWMB 13
#define BIN1 32
#define BIN2 33

void motor(int pwm, int in1, int in2, int v) {
  digitalWrite(in1, v > 0); digitalWrite(in2, v < 0);
  ledcWrite(pwm, abs(v));
}

void setup() {
  Serial.begin(115200);
  delay(1500);   // beri waktu Serial Monitor tersambung
  Serial.println("\n=== TES MOTOR ===");
  pinMode(AIN1, OUTPUT); pinMode(AIN2, OUTPUT); pinMode(BIN1, OUTPUT); pinMode(BIN2, OUTPUT);
  ledcAttach(PWMA, 20000, 8);
  ledcAttach(PWMB, 20000, 8);
}

void loop() {
  Serial.println("KIRI maju");    motor(PWMA, AIN1, AIN2, 150);  delay(1500); motor(PWMA, AIN1, AIN2, 0); delay(700);
  Serial.println("KIRI mundur");  motor(PWMA, AIN1, AIN2, -150); delay(1500); motor(PWMA, AIN1, AIN2, 0); delay(700);
  Serial.println("KANAN maju");   motor(PWMB, BIN1, BIN2, 150);  delay(1500); motor(PWMB, BIN1, BIN2, 0); delay(700);
  Serial.println("KANAN mundur"); motor(PWMB, BIN1, BIN2, -150); delay(1500); motor(PWMB, BIN1, BIN2, 0);
  Serial.println("--- jeda 3 detik ---"); delay(3000);
}
