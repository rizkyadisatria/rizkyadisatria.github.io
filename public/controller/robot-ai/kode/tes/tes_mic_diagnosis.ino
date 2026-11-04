// DIAGNOSIS MIKROFON INMP441: baca jalur KIRI dan KANAN sekaligus, lalu beri kesimpulan.
// Upload, buka Serial Monitor (115200). Diam 3 detik, lalu tepuk tangan dekat mikrofon.
#include <ESP_I2S.h>
#define MIC_SCK 15
#define MIC_WS  16
#define MIC_SD   6

I2SClass mic;
bool micOk = false;

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("\n=== DIAGNOSIS MIC ===");
  mic.setPins(MIC_SCK, MIC_WS, -1, MIC_SD);
  micOk = mic.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO);
  if (!micOk) Serial.println("I2S gagal mulai. Cek apakah pin 15, 16, 6 dipakai program lain.");
  Serial.println("Kolom: KIRI = jalur kiri, KANAN = jalur kanan, nol = persen sampel yang nol persis.");
}

void loop() {
  if (!micOk) { delay(2000); return; }
  static int32_t buf[1024];                      // kiri dan kanan bergantian
  size_t n = mic.readBytes((char*)buf, sizeof(buf)) / 8;
  if (n == 0) { Serial.println("tidak ada data sama sekali: cek kabel SCK ke IO15 dan WS ke IO16"); delay(500); return; }

  double jk = 0, jn = 0, rk = 0, rn = 0;          // jumlah kuadrat, rata-rata
  int nolK = 0, nolN = 0;
  for (size_t i = 0; i < n; i++) {
    double k = buf[2 * i] >> 14, a = buf[2 * i + 1] >> 14;
    rk += k; rn += a;
    if (buf[2 * i] == 0) nolK++;
    if (buf[2 * i + 1] == 0) nolN++;
  }
  rk /= n; rn /= n;
  for (size_t i = 0; i < n; i++) {
    double k = (buf[2 * i] >> 14) - rk, a = (buf[2 * i + 1] >> 14) - rn;
    jk += k * k; jn += a * a;
  }
  int kiri = sqrt(jk / n), kanan = sqrt(jn / n);
  int pk = nolK * 100 / n, pn = nolN * 100 / n;

  const char* kes;
  if (pk < 10)                 kes = "jalur kiri hidup: L/R benar. Angka KIRI harus kecil saat diam, besar saat tepuk";
  else if (pk > 90 && pn < 10) kes = "suara ada di jalur KANAN: L/R tidak ke GND (lepas atau ke 3,3 V)";
  else if (pk > 90 && pn > 90) kes = "dua jalur nol: mikrofon tidak mengirim. Cek VDD 3,3 V, GND, dan kabel SD ke IO6";
  else                         kes = "data putus-putus: kabel SD, WS, atau GND mikrofon longgar";

  static unsigned long t = 0;
  if (millis() - t > 300) {
    t = millis();
    Serial.printf("KIRI %6d (nol %3d%%)   KANAN %6d (nol %3d%%)   -> %s\n", kiri, pk, kanan, pn, kes);
  }
}
