// TES MUSIK: pemutar lagu di layar sentuh. Lagu dari SD card, suara lewat MAX98357A dan speaker.
// Library: LovyanGFX dan ESP32-audioI2S (schreibfaul1, pasang dari ZIP GitHub seperti di panduan).
// Siapkan: SD card FAT32 berisi file .mp3, .wav, atau .flac di folder utama (nama pendek tanpa spasi).
// Jalankan tes_layar dulu: arah layar dan kalibrasi sentuh dipakai dari sana.
//
// Layar:   ketuk nama lagu = putar lagu itu.   Tombol segitiga kanan = geser daftar.
//          SEBELUM / JEDA / BERIKUT di bawah, VOL- dan VOL+ (tahan untuk mengulang).
// Serial Monitor (115200), ketik huruf lalu Enter, sama seperti tombolnya:
//          n = berikutnya   b = sebelumnya   p = jeda   + / - = volume   l = daftar lagu
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <SPI.h>
#include <SD.h>
#include <Audio.h>
#include <Preferences.h>

#define TFT_SCK   12
#define TFT_MOSI  11
#define TFT_MISO  13
#define TFT_CS    10
#define TFT_DC     9
#define TFT_RST    8
#define TOUCH_CS  14
#define SD_CS     21
#define AMP_BCLK   4
#define AMP_LRC    5
#define AMP_DIN    7

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9341 _panel;
  lgfx::Bus_SPI _bus;
  lgfx::Touch_XPT2046 _touch;
public:
  LGFX() {
    { auto c = _bus.config();
      c.spi_host = SPI2_HOST; c.spi_mode = 0; c.freq_write = 27000000; c.freq_read = 16000000;
      c.spi_3wire = false; c.use_lock = true; c.dma_channel = SPI_DMA_CH_AUTO;
      c.pin_sclk = TFT_SCK; c.pin_mosi = TFT_MOSI; c.pin_miso = TFT_MISO; c.pin_dc = TFT_DC;
      _bus.config(c); _panel.setBus(&_bus); }
    { auto c = _panel.config();
      c.pin_cs = TFT_CS; c.pin_rst = TFT_RST; c.pin_busy = -1;
      c.panel_width = 240; c.panel_height = 320;
      c.readable = false; c.invert = false;   // readable=false: pin SDO(MISO) layar sengaja TIDAK disambung
      c.rgb_order = false; c.dlen_16bit = false;
      c.bus_shared = true;                  // SD card dan sentuhan memakai kabel SPI yang sama
      _panel.config(c); }
    { auto c = _touch.config();
      c.x_min = 300; c.x_max = 3900; c.y_min = 300; c.y_max = 3900;
      c.pin_int = -1; c.bus_shared = true; c.offset_rotation = 0;
      c.spi_host = SPI2_HOST; c.freq = 1000000;
      c.pin_sclk = TFT_SCK; c.pin_mosi = TFT_MOSI; c.pin_miso = TFT_MISO; c.pin_cs = TOUCH_CS;
      _touch.config(c); _panel.setTouch(&_touch); }
    setPanel(&_panel);
  }
  // lebar320 = true untuk panel mendatar 320 x 240 (modul merahmu). rgb = urutan warna.
  void aturPanel(bool lebar320, bool rgb) {
    auto c = _panel.config();
    c.panel_width  = c.memory_width  = lebar320 ? 320 : 240;
    c.panel_height = c.memory_height = lebar320 ? 240 : 320;
    c.rgb_order = rgb;
    _panel.config(c);
  }
};

LGFX lcd;
Audio audio;

// ---------------- tata letak layar 320 x 240 ----------------
const int W = 320, H = 240;
const int KEPALA_H = 26;
const int BARIS_Y = 28, BARIS_H = 30, BARIS_N = 5;      // 5 baris lagu terlihat
const int DAFTAR_W = 262;                               // lebar daftar; di kanannya tombol geser
const int NAMA_Y = 181, BAR_Y = 200, BAR_H = 6;
const int TOMBOL_Y = 210, TOMBOL_H = 28, TOMBOL_W = 64;
#define RGB565(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))
const uint16_t C_KEPALA = RGB565(0, 40, 110), C_BARIS = RGB565(32, 32, 36), C_MAIN = RGB565(0, 110, 50);
const uint16_t C_TOMBOL = RGB565(50, 60, 90), C_BAR = RGB565(60, 60, 60), C_ISI = RGB565(0, 220, 90);

const int MAKS = 40;
String lagu[MAKS];
int jumlah = 0, now = 0, awal = 0, volume = 12, gagalBeruntun = 0;
bool siap = false, jeda = false, sedangMain = false;

// ---------------- bantuan ----------------
String potong(const String& s, unsigned n) { return s.length() <= n ? s : s.substring(0, n - 1) + "~"; }

bool lagunya(const String& n) {
  String k = n; k.toLowerCase();
  if (n.startsWith(".") || n.startsWith("_")) return false;                  // file sampah dari Mac/Windows
  return k.endsWith(".mp3") || k.endsWith(".wav") || k.endsWith(".flac");
}

void daftar() {
  Serial.printf("\nDaftar lagu (%d):\n", jumlah);
  for (int i = 0; i < jumlah; i++) Serial.printf("  %d. %s%s\n", i + 1, lagu[i].c_str(), i == now ? "   <- sedang diputar" : "");
}

void berhenti(const char* sebab) {        // tes tidak bisa jalan: tulis di layar dan Serial Monitor, ulangi pesannya
  lcd.fillScreen(TFT_BLACK);
  lcd.setFont(&fonts::Font2); lcd.setTextColor(TFT_RED, TFT_BLACK); lcd.setTextDatum(top_left);
  lcd.setTextWrap(true, true); lcd.setCursor(8, 8); lcd.print("TES MUSIK BERHENTI"); lcd.setCursor(8, 36);
  lcd.setTextColor(TFT_WHITE, TFT_BLACK); lcd.print(sebab);
  while (true) { Serial.println(sebab); delay(3000); }
}

// ---------------- gambar ----------------
void gambarKepala() {
  char s[40];
  lcd.fillRect(0, 0, W, KEPALA_H, C_KEPALA);
  lcd.setFont(&fonts::Font2); lcd.setTextColor(TFT_WHITE, C_KEPALA); lcd.setTextDatum(middle_left);
  snprintf(s, sizeof(s), "MUSIK   %d lagu", jumlah); lcd.drawString(s, 8, KEPALA_H / 2);
  lcd.setTextDatum(middle_right);
  snprintf(s, sizeof(s), "VOLUME %d / 21", volume); lcd.drawString(s, W - 8, KEPALA_H / 2);
}

void gambarBaris(int r) {
  int i = awal + r, y = BARIS_Y + r * BARIS_H;
  if (i >= jumlah) { lcd.fillRect(0, y, DAFTAR_W, BARIS_H - 2, TFT_BLACK); return; }
  uint16_t bg = (i == now) ? C_MAIN : C_BARIS;
  char s[16]; snprintf(s, sizeof(s), "%d. ", i + 1);
  lcd.fillRect(0, y, DAFTAR_W, BARIS_H - 2, bg);
  lcd.setFont(&fonts::Font2); lcd.setTextColor(TFT_WHITE, bg); lcd.setTextDatum(middle_left);
  lcd.drawString(String(s) + potong(lagu[i], 28), 8, y + BARIS_H / 2 - 1);
}

void gambarDaftar() { for (int r = 0; r < BARIS_N; r++) gambarBaris(r); }

void gambarGeser() {
  int x = DAFTAR_W + 4, w = W - x - 2;
  bool naik = awal > 0, turun = awal + BARIS_N < jumlah;
  lcd.fillRoundRect(x, BARIS_Y, w, 70, 6, naik ? C_TOMBOL : C_BARIS);
  lcd.fillRoundRect(x, BARIS_Y + 80, w, 70, 6, turun ? C_TOMBOL : C_BARIS);
  int cx = x + w / 2;
  lcd.fillTriangle(cx, BARIS_Y + 22, cx - 12, BARIS_Y + 48, cx + 12, BARIS_Y + 48, naik ? TFT_WHITE : TFT_DARKGREY);
  lcd.fillTriangle(cx, BARIS_Y + 128, cx - 12, BARIS_Y + 102, cx + 12, BARIS_Y + 102, turun ? TFT_WHITE : TFT_DARKGREY);
}

void gambarTombol() {
  const char* nama[5] = {"SEBELUM", jeda ? "LANJUT" : "JEDA", "BERIKUT", "VOL -", "VOL +"};
  lcd.setFont(&fonts::Font2); lcd.setTextDatum(middle_center);
  for (int i = 0; i < 5; i++) {
    uint16_t c = (i == 1 && jeda) ? RGB565(160, 90, 0) : C_TOMBOL;
    lcd.fillRoundRect(i * TOMBOL_W + 2, TOMBOL_Y, TOMBOL_W - 4, TOMBOL_H, 6, c);
    lcd.setTextColor(TFT_WHITE, c);
    lcd.drawString(nama[i], i * TOMBOL_W + TOMBOL_W / 2, TOMBOL_Y + TOMBOL_H / 2);
  }
}

void gambarWaktu() {                      // nama lagu, waktu, dan garis kemajuan
  uint32_t t = audio.getAudioCurrentTime(), d = audio.getAudioFileDuration();
  char s[40];
  lcd.fillRect(0, NAMA_Y - 1, W, 18, TFT_BLACK);
  lcd.setFont(&fonts::Font2); lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  lcd.setTextDatum(middle_left);  lcd.drawString(sedangMain || jeda ? potong(lagu[now], 26) : "(berhenti)", 6, NAMA_Y + 8);
  lcd.setTextDatum(middle_right);
  if (d) snprintf(s, sizeof(s), "%u:%02u / %u:%02u", (unsigned)(t / 60), (unsigned)(t % 60), (unsigned)(d / 60), (unsigned)(d % 60));
  else   snprintf(s, sizeof(s), "%u:%02u", (unsigned)(t / 60), (unsigned)(t % 60));
  lcd.drawString(s, W - 6, NAMA_Y + 8);
  lcd.fillRect(6, BAR_Y, W - 12, BAR_H, C_BAR);
  if (d) lcd.fillRect(6, BAR_Y, (int)((W - 12) * (float)min(t, d) / d), BAR_H, C_ISI);
}

void tampilkanSemua() {
  lcd.fillScreen(TFT_BLACK);
  gambarKepala(); gambarDaftar(); gambarGeser(); gambarWaktu(); gambarTombol();
}

// ---------------- pemutar ----------------
void putar(int i) {
  now = (i % jumlah + jumlah) % jumlah;
  jeda = false;
  audio.stopSong();
  Serial.printf("\nMemutar %d/%d: %s\n", now + 1, jumlah, lagu[now].c_str());
  sedangMain = audio.connecttoFS(SD, ("/" + lagu[now]).c_str());
  if (sedangMain) gagalBeruntun = 0;
  else {
    Serial.println("  GAGAL membuka file ini. FLAC hi-res (di atas 48 kHz atau 24 bit) sering terlalu berat: ubah ke mp3 atau FLAC 16 bit 44,1 kHz. Atau salin ulang filenya.");
    gagalBeruntun++;
  }
  if (now < awal || now >= awal + BARIS_N) awal = max(0, min(now, jumlah - BARIS_N));   // lagu harus terlihat
  gambarDaftar(); gambarGeser(); gambarWaktu(); gambarTombol();
}

void jedaLanjut() {
  if (audio.pauseResume()) { jeda = !jeda; Serial.println(jeda ? "Jeda" : "Lanjut"); gambarTombol(); gambarWaktu(); }
}

void ubahVolume(int d) {
  volume = constrain(volume + d, 0, 21);
  audio.setVolume(volume);
  Serial.printf("Volume %d\n", volume);
  gambarKepala();
}

void geser(int d) {
  int baru = max(0, min(awal + d, max(0, jumlah - BARIS_N)));
  if (baru == awal) return;
  awal = baru; gambarDaftar(); gambarGeser();
}

// Aksi dari sentuhan. Mengembalikan true untuk tombol yang boleh diulang kalau ditahan (volume).
bool aksiSentuh(int x, int y) {
  if (y >= BARIS_Y && y < BARIS_Y + BARIS_N * BARIS_H) {
    if (x < DAFTAR_W) { int i = awal + (y - BARIS_Y) / BARIS_H; if (i < jumlah) putar(i); }
    else if (y < BARIS_Y + 75) geser(-BARIS_N);
    else geser(BARIS_N);
    return false;
  }
  if (y >= TOMBOL_Y) {
    switch (constrain(x / TOMBOL_W, 0, 4)) {
      case 0: putar(now - 1); break;
      case 1: jedaLanjut(); break;
      case 2: putar(now + 1); break;
      case 3: ubahVolume(-1); return true;
      case 4: ubahVolume(+1); return true;
    }
  }
  return false;
}

void tanganiSentuh() {
  static bool tekan = false, ulang = false;
  static unsigned long tTekan = 0, tUlang = 0, tLepas = 0;
  int32_t x1, y1, x2, y2;
  bool ada = lcd.getTouch(&x1, &y1) > 0;
  if (ada) { delay(3); ada = lcd.getTouch(&x2, &y2) > 0 && abs(x1 - x2) < 10 && abs(y1 - y2) < 10; }   // dua bacaan harus sepakat
  if (ada) {
    tLepas = millis();
    int x = (x1 + x2) / 2, y = (y1 + y2) / 2;
    if (!tekan) { tekan = true; tTekan = tUlang = millis(); ulang = aksiSentuh(x, y); }
    else if (ulang && millis() - tTekan > 400 && millis() - tUlang > 150) { tUlang = millis(); aksiSentuh(x, y); }
  } else if (tekan && millis() - tLepas > 60) tekan = false;     // jari sudah diangkat
}

void setup() {
  Serial.begin(115200);
  delay(1500);   // beri waktu Serial Monitor tersambung
  Serial.println("\n=== TES MUSIK ===");

  // layar: arah, jenis panel, dan kalibrasi sentuh dari tes_layar
  Preferences pref; pref.begin("robot", false);
  uint8_t arah = pref.getUChar("rot", 4), geo = pref.getUChar("geo", 1), rgb = pref.getUChar("rgb", 1);
  float aff[6]; bool adaKal = pref.getBytesLength("aff") == sizeof(aff);
  if (adaKal) pref.getBytes("aff", aff, sizeof(aff));
  pref.end();
  lcd.aturPanel(geo != 0, rgb != 0);
  lcd.init(); lcd.setRotation(arah > 7 ? 4 : arah);
  if (lcd.width() < lcd.height()) lcd.setRotation(geo ? 4 : 1);    // pemutar ini untuk layar mendatar 320 x 240
  lcd.setBrightness(255); lcd.fillScreen(TFT_BLACK);
  if (adaKal) lcd.panel()->setCalibrateAffine(aff);
  else Serial.println("PERINGATAN: kalibrasi sentuh belum ada. Jalankan tes_layar dulu, kalau tidak ketukan bisa meleset.");

  // SD card (sesudah layar, karena memakai kabel SPI yang sama)
  SPI.begin(TFT_SCK, TFT_MISO, TFT_MOSI);
  if (!SD.begin(SD_CS, SPI, 20000000))
    berhenti("SD GAGAL. Cek kabel SD_CS ke IO21, SD_SCK ke d2, SD_MOSI ke d5, SD_MISO ke d8, dan format kartu FAT32.");
  Serial.printf("SD terbaca, ukuran %llu MB\n", SD.cardSize() / 1048576);

  File root = SD.open("/");
  for (File f = root.openNextFile(); f && jumlah < MAKS; f = root.openNextFile()) {
    if (!f.isDirectory() && lagunya(String(f.name()))) lagu[jumlah++] = String(f.name());
  }
  if (jumlah == 0)
    berhenti("Tidak ada file .mp3, .wav, atau .flac di folder utama SD. Salin satu lagu ke kartu, lalu tekan RST.");
  for (int i = 1; i < jumlah; i++) {                                // urutkan nama A ke Z
    String k = lagu[i]; int j = i - 1;
    while (j >= 0 && lagu[j].compareTo(k) > 0) { lagu[j + 1] = lagu[j]; j--; }
    lagu[j + 1] = k;
  }
  daftar();

  if (!audio.setPinout(AMP_BCLK, AMP_LRC, AMP_DIN))
    berhenti("Pin speaker gagal dipasang. Cek BCLK=IO4, LRC=IO5, DIN=IO7 ke MAX98357A.");
  audio.setVolume(volume);                 // 0 sampai 21
  siap = true;
  tampilkanSemua();
  putar(0);
}

void loop() {
  audio.loop();                            // harus dipanggil sesering mungkin
  if (!siap) return;

  static bool pernahJalan = false;
  if (audio.isRunning()) pernahJalan = true;
  else if (pernahJalan && !jeda) {         // lagu habis: lanjut ke berikutnya
    pernahJalan = false; sedangMain = false;
    if (gagalBeruntun >= jumlah) berhenti("Semua file gagal dibuka. Coba lagu mp3 lain, atau format ulang kartu SD sebagai FAT32.");
    putar(now + 1);
  }

  static unsigned long tWaktu = 0, tSentuh = 0;
  if (millis() - tWaktu > 1000) { tWaktu = millis(); gambarWaktu(); }
  if (millis() - tSentuh >= 25) { tSentuh = millis(); tanganiSentuh(); }

  while (Serial.available()) {
    int c = Serial.read();
    if (c == 'n') putar(now + 1);
    else if (c == 'b') putar(now - 1);
    else if (c == 'p') jedaLanjut();
    else if (c == '+') ubahVolume(+2);
    else if (c == '-') ubahVolume(-2);
    else if (c == 'l') daftar();
  }
}
