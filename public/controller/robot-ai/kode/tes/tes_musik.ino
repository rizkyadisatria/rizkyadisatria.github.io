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

// Modul merah 320 x 240 biasanya memakai chip ILI9342C. Library mengirim pengaturan daya dan gamma
// ILI9341 kalau kita pilih panel ILI9341, dan itu membuat warna pucat. Panel ini mengirim pengaturan ILI9342C.
// ic = true: pengaturan ILI9342C, ic = false: pengaturan ILI9341 asli. Arah layar dan sentuhan tidak berubah.
struct PanelKu : public lgfx::Panel_ILI9341 {
  bool ic = true, fh = false, fv = true;   // ic: chip ILI9342C; fh/fv: balik layar kiri-kanan / atas-bawah
protected:
  const uint8_t* getInitCommands(uint8_t listno) const override {
    if (!ic) return lgfx::Panel_ILI9341::getInitCommands(listno);
    static constexpr uint8_t list0[] = {
      0xC8, 3, 0xFF,0x93,0x42,
      0xC0, 2, 0x12,0x12,
      0xC1, 1, 0x03,
      0xC5, 1, 0xF2,
      0xB0, 1, 0xE0,
      0xF6, 3, 0x01,0x00,0x00,
      0xE0,15, 0x00,0x0C,0x11,0x04,0x11,0x08,0x37,0x89,0x4C,0x06,0x0C,0x0A,0x2E,0x34,0x0F,
      0xE1,15, 0x00,0x0B,0x11,0x05,0x13,0x09,0x33,0x67,0x48,0x07,0x0E,0x0B,0x2E,0x33,0x0F,
      0xB6, 4, 0x08,0x82,0x1D,0x04,
      0x38, 0,
      0x29, 0,
      0x11, 0 + CMD_INIT_DELAY, 120,
      0xFF,0xFF,
    };
    return listno == 0 ? list0 : nullptr;
  }
  // Pembalikan arah dan urutan warna diatur di sini, supaya nomor arah (rotasi) dan sentuhan tidak ikut berubah.
  void update_madctl(void) override {
    if (_bus == nullptr) return;
    uint8_t m = getMadCtl(_internal_rotation);
    if (fh) m ^= (m & MAD_MV) ? (MAD_MY | MAD_ML) : (MAD_MX | MAD_MH);   // balik kiri-kanan layar
    if (fv) m ^= (m & MAD_MV) ? (MAD_MX | MAD_MH) : (MAD_MY | MAD_ML);   // balik atas-bawah layar
    m |= (_cfg.rgb_order == ic) ? MAD_BGR : MAD_RGB;                     // ILI9342C: urutan warna kebalikan dari ILI9341
    startWrite();
    write_command(CMD_COLMOD); writeData(getColMod(_write_bits), 1);
    write_command(CMD_MADCTL); writeData(m, 1);
    _bus->flush();
    endWrite();
  }
};

class LGFX : public lgfx::LGFX_Device {
  PanelKu _panel;
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
  void pakaiLayar(bool ic, bool fh, bool fv) { _panel.ic = ic; _panel.fh = fh; _panel.fv = fv; }
  void balikInvert(bool v) { auto c = _panel.config(); c.invert = v; _panel.config(c); }
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
const int TOMBOL_Y = 208, TOMBOL_H = 32, TOMBOL_W = 64;
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
  if (y >= TOMBOL_Y - 4) {                              // zona tombol sedikit lebih tinggi dari gambarnya
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

void titikSentuh(bool nyala) {            // titik kuning di header: tanda bahwa sentuhan terbaca
  static bool lalu = false;
  if (nyala == lalu) return;
  lalu = nyala;
  lcd.fillCircle(W / 2, KEPALA_H / 2, 6, nyala ? TFT_YELLOW : C_KEPALA);
}

void tanganiSentuh() {
  static unsigned long tPanggil = 0, tCetak = 0, terlama = 0;   // ukur: seberapa jarang sentuhan dicek
  unsigned long kini = millis();
  if (tPanggil && kini - tPanggil > terlama) terlama = kini - tPanggil;
  tPanggil = kini;
  if (kini - tCetak > 5000) {
    tCetak = kini;
    if (terlama > 60) Serial.printf("info: jeda terpanjang antar cek sentuh %lu ms (ketukan lebih singkat dari ini bisa terlewat)\n", terlama);
    terlama = 0;
  }
  static bool tekan = false, ulang = false;
  static unsigned long tTekan = 0, tUlang = 0, tLepas = 0;
  int32_t x, y;
  bool ada = lcd.getTouch(&x, &y) > 0;                // LovyanGFX sudah mengambil nilai tengah dari 7 bacaan
  if (ada) {
    tLepas = millis();
    titikSentuh(true);
    if (!tekan) {
      tekan = true; tTekan = tUlang = millis();
      Serial.printf("sentuh x=%d y=%d\n", (int)x, (int)y);
      ulang = aksiSentuh(x, y);
    } else if (ulang && millis() - tTekan > 400 && millis() - tUlang > 150) { tUlang = millis(); aksiSentuh(x, y); }
  } else if (tekan && millis() - tLepas > 150) { tekan = false; titikSentuh(false); }   // 0,15 detik tanpa sentuhan = jari diangkat
}

// Baca pilihan layar tersimpan: jenis chip, balik layar kiri-kanan / atas-bawah, dan warna negatif (invert).
// Bawaan untuk modul merahmu (ILI9342C): layar dibalik atas-bawah dan invert aktif. Ubah lewat tombol Serial di tes_media.
void muatLayar() {
  Preferences p; p.begin("robot", true);
  bool ic = p.getUChar("ic", 1) != 0;
  lcd.pakaiLayar(ic, p.getUChar("fh", 0) != 0, p.getUChar("fv", ic ? 1 : 0) != 0);
  lcd.balikInvert(p.getUChar("inv", ic ? 1 : 0) != 0);
  p.end();
}

void setup() {
  Serial.begin(115200);
  delay(1500);   // beri waktu Serial Monitor tersambung
  Serial.println("\n=== TES MUSIK ===");

  // layar: arah, jenis panel, dan kalibrasi sentuh dari tes_layar
  Preferences pref; pref.begin("robot", false);
  uint8_t arah = pref.getUChar("rot", 6), geo = pref.getUChar("geo", 1), rgb = pref.getUChar("rgb", 1);
  float aff[6]; bool adaKal = pref.getBytesLength("aff") == sizeof(aff);
  if (adaKal) pref.getBytes("aff", aff, sizeof(aff));
  pref.end();
  lcd.aturPanel(geo != 0, rgb != 0);
  muatLayar();
  lcd.init(); lcd.setRotation(arah > 7 ? 6 : arah);
  if (lcd.width() < lcd.height()) lcd.setRotation(geo ? 6 : 1);    // pemutar ini untuk layar mendatar 320 x 240
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
  if (millis() - tSentuh >= 4) { tSentuh = millis(); tanganiSentuh(); }

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
