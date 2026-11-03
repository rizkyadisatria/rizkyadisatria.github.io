// TES MUSIK: memutar lagu dari SD card lewat MAX98357A dan speaker.
// Library: ESP32-audioI2S (schreibfaul1), pasang dari ZIP GitHub seperti di panduan.
// Siapkan: SD card FAT32 berisi file .mp3 atau .wav di folder utama (nama pendek tanpa spasi, mis. lagu1.mp3).
// Perintah di Serial Monitor (115200), ketik huruf lalu Enter:
//   n = lagu berikutnya   b = lagu sebelumnya   p = jeda / lanjut
//   + = volume naik       - = volume turun      l = daftar lagu
// Tes ini hanya memakai SD dan speaker. Layar tidak dinyalakan (kabelnya boleh tetap terpasang).
#include <SPI.h>
#include <SD.h>
#include <Audio.h>

#define TFT_SCK   12
#define TFT_MOSI  11
#define TFT_MISO  13
#define TFT_CS    10
#define TOUCH_CS  14
#define SD_CS     21
#define AMP_BCLK   4
#define AMP_LRC    5
#define AMP_DIN    7

Audio audio;
const int MAKS = 40;
String lagu[MAKS];
int jumlah = 0, now = 0, volume = 12, gagalBeruntun = 0;
bool siap = false, jeda = false;

bool lagunya(const String& n) {           // true kalau nama file seperti lagu
  String k = n; k.toLowerCase();
  if (n.startsWith(".") || n.startsWith("_")) return false;                  // file sampah dari Mac/Windows
  return k.endsWith(".mp3") || k.endsWith(".wav");
}

void daftar() {
  Serial.printf("\nDaftar lagu (%d):\n", jumlah);
  for (int i = 0; i < jumlah; i++) Serial.printf("  %d. %s%s\n", i + 1, lagu[i].c_str(), i == now ? "   <- sedang diputar" : "");
}

void putar(int i) {
  now = (i % jumlah + jumlah) % jumlah;
  jeda = false;
  audio.stopSong();
  Serial.printf("\nMemutar %d/%d: %s\n", now + 1, jumlah, lagu[now].c_str());
  if (audio.connecttoFS(SD, ("/" + lagu[now]).c_str())) {
    gagalBeruntun = 0;
  } else {
    Serial.println("  GAGAL membuka file ini. Coba format lain (mp3 128 kbps paling aman), atau salin ulang filenya.");
    gagalBeruntun++;
  }
}

void berhenti(const char* sebab) {        // tes tidak jalan: ulangi pesannya supaya terbaca walau Serial Monitor telat dibuka
  while (true) { Serial.println(sebab); delay(3000); }
}

void setup() {
  Serial.begin(115200);
  delay(1500);   // beri waktu Serial Monitor tersambung
  Serial.println("\n=== TES MUSIK ===");

  pinMode(TFT_CS, OUTPUT);   digitalWrite(TFT_CS, HIGH);     // layar dan sentuhan "diam" supaya tidak ikut
  pinMode(TOUCH_CS, OUTPUT); digitalWrite(TOUCH_CS, HIGH);   // mendengar di kabel SPI yang dipakai SD

  SPI.begin(TFT_SCK, TFT_MISO, TFT_MOSI);
  if (!SD.begin(SD_CS, SPI, 20000000))
    berhenti("SD GAGAL. Cek kabel SD_CS ke IO21, SD_SCK ke d2, SD_MOSI ke d5, SD_MISO ke d8, dan format kartu FAT32.");
  Serial.printf("SD terbaca, ukuran %llu MB\n", SD.cardSize() / 1048576);

  File root = SD.open("/");
  for (File f = root.openNextFile(); f && jumlah < MAKS; f = root.openNextFile()) {
    if (!f.isDirectory() && lagunya(String(f.name()))) lagu[jumlah++] = String(f.name());
  }
  if (jumlah == 0)
    berhenti("Tidak ada file .mp3 atau .wav di folder utama SD. Salin satu lagu (mis. lagu1.mp3) ke kartu, lalu tekan RST.");
  daftar();

  if (!audio.setPinout(AMP_BCLK, AMP_LRC, AMP_DIN))
    berhenti("Pin speaker gagal dipasang. Cek BCLK=IO4, LRC=IO5, DIN=IO7 ke MAX98357A.");
  audio.setVolume(volume);                 // 0 sampai 21
  Serial.printf("Volume %d dari 21. Ketik n, b, p, +, -, l lalu Enter.\n", volume);
  siap = true;
  putar(0);
}

void loop() {
  audio.loop();
  if (!siap) return;

  static unsigned long tInfo = 0;
  if (audio.isRunning() && millis() - tInfo > 5000) {
    tInfo = millis();
    Serial.printf("  %s  %u / %u detik\n", lagu[now].c_str(), (unsigned)audio.getAudioCurrentTime(), (unsigned)audio.getAudioFileDuration());
  }

  static bool pernahJalan = false;
  if (audio.isRunning()) pernahJalan = true;
  else if (pernahJalan && !jeda) {   // lagu habis: lanjut ke berikutnya
    pernahJalan = false;
    if (gagalBeruntun >= jumlah) berhenti("Semua file gagal dibuka. Coba lagu mp3 lain, atau format ulang kartu SD sebagai FAT32.");
    putar(now + 1);
  }

  while (Serial.available()) {
    int c = Serial.read();
    if (c == 'n') putar(now + 1);
    else if (c == 'b') putar(now - 1);
    else if (c == 'p') { if (audio.pauseResume()) { jeda = !jeda; Serial.println(jeda ? "Jeda" : "Lanjut"); } }
    else if (c == '+') { volume = min(21, volume + 2); audio.setVolume(volume); Serial.printf("Volume %d\n", volume); }
    else if (c == '-') { volume = max(0, volume - 2);  audio.setVolume(volume); Serial.printf("Volume %d\n", volume); }
    else if (c == 'l') daftar();
  }
}
