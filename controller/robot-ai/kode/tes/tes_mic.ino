// TES MIKROFON: tampilkan kerasnya suara. Buka Tools > Serial Plotter, lalu tepuk tangan.
#include <ESP_I2S.h>
#define MIC_SCK 15
#define MIC_WS  16
#define MIC_SD   6

I2SClass mic;

void setup() {
  Serial.begin(115200);
  mic.setPins(MIC_SCK, MIC_WS, -1, MIC_SD);
  if (!mic.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT))
    Serial.println("Mikrofon gagal mulai");
}

void loop() {
  int32_t buf[512];
  size_t n = mic.readBytes((char*)buf, sizeof(buf)) / 4;
  if (n == 0) return;
  double jumlah = 0;
  for (size_t i = 0; i < n; i++) { double v = buf[i] >> 14; jumlah += v * v; }
  int keras = sqrt(jumlah / n);
  Serial.printf("keras:%d\n", keras);                // diam: puluhan sampai ratusan, tepuk: ribuan
}
