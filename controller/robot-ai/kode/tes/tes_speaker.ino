// TES SPEAKER: bunyi nada naik-turun lewat MAX98357A. Tanpa library tambahan.
#include <ESP_I2S.h>
#define AMP_BCLK 4
#define AMP_LRC  5
#define AMP_DIN  7

I2SClass amp;
const int RATE = 16000;

void nada(float hz, int ms, float keras = 0.25) {
  int16_t buf[256];
  int total = RATE * ms / 1000;
  static float fase = 0;
  for (int n = 0; n < total; n += 128) {
    for (int i = 0; i < 128; i++) {
      int16_t v = (int16_t)(sin(fase) * 32767 * keras);
      fase += 2 * PI * hz / RATE; if (fase > 2 * PI) fase -= 2 * PI;
      buf[i * 2] = v; buf[i * 2 + 1] = v;          // kiri dan kanan sama
    }
    amp.write((uint8_t*)buf, sizeof(buf));
  }
}

void setup() {
  Serial.begin(115200);
  amp.setPins(AMP_BCLK, AMP_LRC, AMP_DIN);
  if (!amp.begin(I2S_MODE_STD, RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) Serial.println("I2S gagal");
}

void loop() {
  Serial.println("do re mi fa sol");
  float not_[] = {523, 587, 659, 698, 784};
  for (float f : not_) nada(f, 300);
  nada(0, 1000, 0);                                  // diam 1 detik
}
