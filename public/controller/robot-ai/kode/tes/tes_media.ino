// TES MEDIA: menu utama di layar sentuh dengan dua pilihan, PUTAR MUSIK dan LIHAT GAMBAR.
// Semua file dibaca dari SD card (folder utama). Suara lewat MAX98357A dan speaker.
// Library: LovyanGFX dan ESP32-audioI2S (schreibfaul1, pasang dari ZIP GitHub seperti di panduan).
// Siapkan SD card FAT32: lagu .mp3 / .wav / .flac dan gambar .jpg / .jpeg / .png / .bmp, nama pendek tanpa spasi.
// Jalankan tes_layar dulu: arah layar dan kalibrasi sentuh dipakai dari sana.
//
// MUSIK  : ketuk nama lagu = putar. Segitiga kanan = geser daftar. SEBELUM, JEDA, BERIKUT, VOL -, VOL +.
//          Tombol MENU di pojok kiri atas kembali ke menu; lagu tetap berjalan.
// GAMBAR : SEBELUM, BERIKUT, OTOMATIS (ganti gambar tiap 5 detik), MENU. Ketuk sisi kiri atau kanan gambar = sebelum/berikut.
//          Musik berhenti saat masuk ke gambar (membuka gambar besar menahan pemutar suara beberapa detik).
// Serial Monitor (115200), ketik huruf lalu Enter: n = berikutnya, b = sebelumnya, p = jeda, + / - = volume, m = menu.
#include <SPI.h>
#include <SD.h>                            // SD.h harus dipasang SEBELUM LovyanGFX supaya gambar bisa dibaca dari SD
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
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
  bool ic = true;
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
  void pakaiIC(bool v) { _panel.ic = v; }
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
const int BARIS_Y = 28, BARIS_H = 30, BARIS_N = 5;      // musik: 5 baris lagu terlihat
const int DAFTAR_W = 262;                               // lebar daftar; di kanannya tombol geser
const int NAMA_Y = 181, BAR_Y = 200, BAR_H = 6;
const int TOMBOL_Y = 208, TOMBOL_H = 32;                // tombol bawah (musik 5 buah, gambar 4 buah)
const int GAMBAR_H = 188;                               // area gambar: 320 x 188, lalu baris keterangan
const int KET_Y = 190;
#define RGB565(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))
const uint16_t C_KEPALA = RGB565(0, 40, 110), C_BARIS = RGB565(32, 32, 36), C_MAIN = RGB565(0, 110, 50);
const uint16_t C_TOMBOL = RGB565(50, 60, 90), C_BAR = RGB565(60, 60, 60), C_ISI = RGB565(0, 220, 90);
const uint16_t C_AKTIF = RGB565(160, 90, 0), C_MATI = RGB565(40, 40, 44);

enum Layar { L_MENU, L_MUSIK, L_GAMBAR };
Layar layar = L_MENU;

const int MAKS_LAGU = 40, MAKS_GAMBAR = 60;
String lagu[MAKS_LAGU], gambar[MAKS_GAMBAR];
int jumlah = 0, now = 0, awal = 0, volume = 12, gagalBeruntun = 0;     // lagu
int nGambar = 0, noGambar = 0;                                         // gambar
bool siap = false, jeda = false, sedangMain = false, otomatis = false;
unsigned long tGeserGambar = 0;

// ---------------- bantuan ----------------
String potong(const String& s, unsigned n) { return s.length() <= n ? s : s.substring(0, n - 1) + "~"; }

bool berakhir(const String& n, const char* a, const char* b = nullptr, const char* c = nullptr, const char* d = nullptr) {
  String k = n; k.toLowerCase();
  return k.endsWith(a) || (b && k.endsWith(b)) || (c && k.endsWith(c)) || (d && k.endsWith(d));
}
bool sampah(const String& n) { return n.startsWith(".") || n.startsWith("_"); }    // file sampah dari Mac/Windows
bool lagunya(const String& n)   { return !sampah(n) && berakhir(n, ".mp3", ".wav", ".flac"); }
bool gambarnya(const String& n) { return !sampah(n) && berakhir(n, ".jpg", ".jpeg", ".png", ".bmp"); }

void urut(String* a, int n) {                                        // urutkan nama A ke Z
  for (int i = 1; i < n; i++) {
    String k = a[i]; int j = i - 1;
    while (j >= 0 && a[j].compareTo(k) > 0) { a[j + 1] = a[j]; j--; }
    a[j + 1] = k;
  }
}

void daftar() {
  Serial.printf("\nDaftar lagu (%d):\n", jumlah);
  for (int i = 0; i < jumlah; i++) Serial.printf("  %d. %s%s\n", i + 1, lagu[i].c_str(), i == now ? "   <- sedang diputar" : "");
  Serial.printf("Daftar gambar (%d):\n", nGambar);
  for (int i = 0; i < nGambar; i++) Serial.printf("  %d. %s\n", i + 1, gambar[i].c_str());
}

void berhenti(const char* sebab) {        // tes tidak bisa jalan: tulis di layar dan Serial Monitor, ulangi pesannya
  lcd.fillScreen(TFT_BLACK);
  lcd.setFont(&fonts::Font2); lcd.setTextColor(TFT_RED, TFT_BLACK); lcd.setTextDatum(top_left);
  lcd.setTextWrap(true, true); lcd.setCursor(8, 8); lcd.print("TES MEDIA BERHENTI"); lcd.setCursor(8, 36);
  lcd.setTextColor(TFT_WHITE, TFT_BLACK); lcd.print(sebab);
  while (true) { Serial.println(sebab); delay(3000); }
}

void tombolBawah(int n, const char* const* nama, int aktif = -1) {
  int w = W / n;
  lcd.setFont(&fonts::Font2); lcd.setTextDatum(middle_center);
  lcd.fillRect(0, TOMBOL_Y - 2, W, H - TOMBOL_Y + 2, TFT_BLACK);
  for (int i = 0; i < n; i++) {
    uint16_t c = (i == aktif) ? C_AKTIF : C_TOMBOL;
    lcd.fillRoundRect(i * w + 2, TOMBOL_Y, w - 4, TOMBOL_H, 6, c);
    lcd.setTextColor(TFT_WHITE, c);
    lcd.drawString(nama[i], i * w + w / 2, TOMBOL_Y + TOMBOL_H / 2);
  }
}

// ---------------- MENU ----------------
const int MENU_X = 16, MENU_W = 288, MENU_H = 80, MENU_Y1 = 40, MENU_Y2 = 132;

void gambarIkonMusik(int x, int y) {
  lcd.fillCircle(x + 24, y + 54, 10, TFT_WHITE);
  lcd.fillRect(x + 32, y + 12, 5, 42, TFT_WHITE);
  lcd.fillRect(x + 32, y + 12, 22, 7, TFT_WHITE);
}
void gambarIkonGambar(int x, int y) {
  lcd.drawRect(x + 6, y + 14, 56, 46, TFT_WHITE); lcd.drawRect(x + 7, y + 15, 54, 44, TFT_WHITE);
  lcd.fillCircle(x + 48, y + 28, 5, TFT_YELLOW);
  lcd.fillTriangle(x + 10, y + 56, x + 28, y + 32, x + 46, y + 56, TFT_WHITE);
  lcd.fillTriangle(x + 34, y + 56, x + 46, y + 42, x + 58, y + 56, TFT_LIGHTGREY);
}

void gambarMenuTombol(int y, bool ada, const char* judul, int jml, const char* satuan, bool musik) {
  uint16_t c = ada ? C_TOMBOL : C_MATI;
  char s[40];
  lcd.fillRoundRect(MENU_X, y, MENU_W, MENU_H, 10, c);
  if (musik) gambarIkonMusik(MENU_X + 10, y + 6); else gambarIkonGambar(MENU_X + 10, y + 6);
  lcd.setTextDatum(middle_left); lcd.setTextColor(ada ? TFT_WHITE : TFT_DARKGREY, c);
  lcd.setFont(&fonts::Font4); lcd.drawString(judul, MENU_X + 84, y + 28);
  lcd.setFont(&fonts::Font2);
  if (ada) snprintf(s, sizeof(s), "%d %s di SD", jml, satuan); else snprintf(s, sizeof(s), "tidak ada %s di SD", satuan);
  lcd.drawString(s, MENU_X + 84, y + 56);
}

void tampilkanMenu() {
  lcd.fillScreen(TFT_BLACK);
  lcd.fillRect(0, 0, W, KEPALA_H, C_KEPALA);
  lcd.setFont(&fonts::Font2); lcd.setTextColor(TFT_WHITE, C_KEPALA); lcd.setTextDatum(middle_left);
  lcd.drawString("MENU UTAMA", 8, KEPALA_H / 2);
  gambarMenuTombol(MENU_Y1, jumlah > 0, "PUTAR MUSIK", jumlah, "lagu", true);
  gambarMenuTombol(MENU_Y2, nGambar > 0, "LIHAT GAMBAR", nGambar, "gambar", false);
}

// ---------------- MUSIK ----------------
void gambarKepalaMusik() {
  char s[40];
  lcd.fillRect(0, 0, W, KEPALA_H, C_KEPALA);
  lcd.fillRoundRect(3, 3, 56, KEPALA_H - 6, 5, C_TOMBOL);
  lcd.setFont(&fonts::Font2); lcd.setTextColor(TFT_WHITE, C_TOMBOL); lcd.setTextDatum(middle_center);
  lcd.drawString("MENU", 31, KEPALA_H / 2);
  lcd.setTextColor(TFT_WHITE, C_KEPALA); lcd.setTextDatum(middle_left);
  snprintf(s, sizeof(s), "%d lagu", jumlah); lcd.drawString(s, 68, KEPALA_H / 2);
  lcd.setTextDatum(middle_right);
  snprintf(s, sizeof(s), "VOL %d / 21", volume); lcd.drawString(s, W - 8, KEPALA_H / 2);
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

void gambarTombolMusik() {
  const char* nama[5] = {"SEBELUM", jeda ? "LANJUT" : "JEDA", "BERIKUT", "VOL -", "VOL +"};
  tombolBawah(5, nama, jeda ? 1 : -1);
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

void tampilkanMusik() {
  lcd.fillScreen(TFT_BLACK);
  gambarKepalaMusik(); gambarDaftar(); gambarGeser(); gambarWaktu(); gambarTombolMusik();
}

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
  if (layar == L_MUSIK) { gambarDaftar(); gambarGeser(); gambarWaktu(); gambarTombolMusik(); }
}

void jedaLanjut() {
  if (audio.pauseResume()) {
    jeda = !jeda; Serial.println(jeda ? "Jeda" : "Lanjut");
    if (layar == L_MUSIK) { gambarTombolMusik(); gambarWaktu(); }
  }
}

void ubahVolume(int d) {
  volume = constrain(volume + d, 0, 21);
  audio.setVolume(volume);
  Serial.printf("Volume %d\n", volume);
  if (layar == L_MUSIK) gambarKepalaMusik();
}

void geser(int d) {
  int baru = max(0, min(awal + d, max(0, jumlah - BARIS_N)));
  if (baru == awal) return;
  awal = baru; gambarDaftar(); gambarGeser();
}

// ---------------- GAMBAR ----------------
// Membaca ukuran dan jenis dari kepala file. jenis: 1 = jpg, 2 = png, 3 = bmp.
bool ukuranGambar(const String& nama, int& w, int& h, int& jenis, bool& progresif) {
  File f = SD.open("/" + nama);
  if (!f) return false;
  uint8_t b[32];
  bool ok = false; progresif = false;
  if (berakhir(nama, ".png")) {
    jenis = 2;
    if (f.read(b, 24) == 24) {
      w = ((int)b[16] << 24) | (b[17] << 16) | (b[18] << 8) | b[19];
      h = ((int)b[20] << 24) | (b[21] << 16) | (b[22] << 8) | b[23];
      ok = w > 0 && h > 0;
    }
  } else if (berakhir(nama, ".bmp")) {
    jenis = 3;
    if (f.read(b, 26) == 26) {
      w = (int32_t)((uint32_t)b[18] | (b[19] << 8) | (b[20] << 16) | ((uint32_t)b[21] << 24));
      h = (int32_t)((uint32_t)b[22] | (b[23] << 8) | (b[24] << 16) | ((uint32_t)b[25] << 24));
      if (h < 0) h = -h;
      ok = w > 0 && h > 0;
    }
  } else {
    jenis = 1;
    if (f.read(b, 2) == 2 && b[0] == 0xFF && b[1] == 0xD8) {
      while (f.available()) {
        int c = f.read(); if (c != 0xFF) continue;
        int m = f.read(); while (m == 0xFF) m = f.read();
        if (m < 0) break;
        if (m == 0xD8 || m == 0x01 || (m >= 0xD0 && m <= 0xD7)) continue;      // penanda tanpa panjang
        if (f.read(b, 2) != 2) break;
        int len = (b[0] << 8) | b[1];
        if (m >= 0xC0 && m <= 0xCF && m != 0xC4 && m != 0xC8 && m != 0xCC) {   // SOF: berisi ukuran
          if (f.read(b, 5) == 5) { h = (b[1] << 8) | b[2]; w = (b[3] << 8) | b[4]; progresif = (m != 0xC0 && m != 0xC1); ok = w > 0 && h > 0; }
          break;
        }
        f.seek(f.position() + len - 2);
      }
    }
  }
  f.close();
  return ok;
}

void tulisKeterangan(const String& teks, uint16_t warna = TFT_WHITE) {
  lcd.fillRect(0, KET_Y, W, TOMBOL_Y - KET_Y - 2, TFT_BLACK);
  lcd.setFont(&fonts::Font2); lcd.setTextColor(warna, TFT_BLACK); lcd.setTextDatum(middle_left);
  lcd.drawString(teks, 6, KET_Y + 9);
}

void gambarTombolGambar() {
  const char* nama[4] = {"SEBELUM", "OTOMATIS", "BERIKUT", "MENU"};
  tombolBawah(4, nama, otomatis ? 1 : -1);
}

void pesanGambar(const char* a, const char* b) {
  lcd.fillRect(0, 0, W, GAMBAR_H, TFT_BLACK);
  lcd.setFont(&fonts::Font2); lcd.setTextDatum(middle_center);
  lcd.setTextColor(TFT_RED, TFT_BLACK);   lcd.drawString(a, W / 2, GAMBAR_H / 2 - 10);
  lcd.setTextColor(TFT_WHITE, TFT_BLACK); lcd.drawString(b, W / 2, GAMBAR_H / 2 + 12);
}

void tampilGambar(int i) {
  noGambar = (i % nGambar + nGambar) % nGambar;
  const String& nama = gambar[noGambar];
  char s[24]; snprintf(s, sizeof(s), "%d/%d  ", noGambar + 1, nGambar);
  tulisKeterangan(String(s) + potong(nama, 26) + "  memuat...", TFT_YELLOW);
  Serial.printf("Gambar %d/%d: %s\n", noGambar + 1, nGambar, nama.c_str());
  lcd.fillRect(0, 0, W, GAMBAR_H, TFT_BLACK);

  int w = 0, h = 0, jenis = 0; bool progresif = false;
  if (!ukuranGambar(nama, w, h, jenis, progresif)) {
    pesanGambar("Gambar tidak bisa dibaca", "file rusak atau bukan jpg, png, bmp");
    tulisKeterangan(String(s) + potong(nama, 26), TFT_RED); return;
  }
  if (progresif) {
    pesanGambar("JPEG progresif tidak didukung", "simpan ulang sebagai JPEG biasa (baseline)");
    tulisKeterangan(String(s) + potong(nama, 20) + "  " + w + "x" + h, TFT_RED); return;
  }
  float zoom = min((float)W / w, (float)GAMBAR_H / h);
  if (zoom > 1.0f) zoom = 1.0f;                                       // gambar kecil tidak diperbesar
  int tw = max(1, (int)(w * zoom)), th = max(1, (int)(h * zoom));
  int x0 = (W - tw) / 2, y0 = (GAMBAR_H - th) / 2;
  String path = "/" + nama;
  bool ok = false;
  if (jenis == 1)      ok = lcd.drawJpgFile(SD, path.c_str(), x0, y0, tw + 1, th + 1, 0, 0, zoom, zoom);
  else if (jenis == 2) ok = lcd.drawPngFile(SD, path.c_str(), x0, y0, tw + 1, th + 1, 0, 0, zoom, zoom);
  else                 ok = lcd.drawBmpFile(SD, path.c_str(), x0, y0, tw + 1, th + 1, 0, 0, zoom, zoom);
  if (!ok) {
    pesanGambar("Gambar gagal dibuka", "terlalu besar, atau formatnya tidak biasa");
    tulisKeterangan(String(s) + potong(nama, 20) + "  " + w + "x" + h, TFT_RED);
    Serial.println("  GAGAL menggambar. Coba kecilkan gambar (mis. lebar 1280 piksel) dan simpan sebagai JPEG biasa.");
    return;
  }
  tulisKeterangan(String(s) + potong(nama, 20) + "  " + w + "x" + h);
  tGeserGambar = millis();
}

void tampilkanGambar() {
  lcd.fillScreen(TFT_BLACK);
  gambarTombolGambar();
  tampilGambar(noGambar);
}

// ---------------- pindah layar ----------------
void keLayar(Layar baru) {
  if (baru == L_MUSIK && jumlah == 0) { Serial.println("Tidak ada lagu di SD."); return; }
  if (baru == L_GAMBAR && nGambar == 0) { Serial.println("Tidak ada gambar di SD."); return; }
  if (baru == L_GAMBAR && layar != L_GAMBAR) {                         // musik berhenti saat melihat gambar
    audio.stopSong(); sedangMain = false; jeda = false;
  }
  layar = baru;
  if (layar == L_MENU) tampilkanMenu();
  else if (layar == L_MUSIK) { tampilkanMusik(); if (!sedangMain && !jeda) putar(now); }
  else { otomatis = false; tampilkanGambar(); }
}

// ---------------- sentuhan ----------------
bool aksiMenu(int x, int y) {
  if (x >= MENU_X && x < MENU_X + MENU_W) {
    if (y >= MENU_Y1 && y < MENU_Y1 + MENU_H) keLayar(L_MUSIK);
    else if (y >= MENU_Y2 && y < MENU_Y2 + MENU_H) keLayar(L_GAMBAR);
  }
  return false;
}

// Mengembalikan true untuk tombol yang boleh diulang kalau ditahan (volume).
bool aksiMusik(int x, int y) {
  if (y < KEPALA_H + 4) { if (x < 64) keLayar(L_MENU); return false; }
  if (y >= BARIS_Y && y < BARIS_Y + BARIS_N * BARIS_H) {
    if (x < DAFTAR_W) { int i = awal + (y - BARIS_Y) / BARIS_H; if (i < jumlah) putar(i); }
    else if (y < BARIS_Y + 75) geser(-BARIS_N);
    else geser(BARIS_N);
    return false;
  }
  if (y >= TOMBOL_Y - 4) {                              // zona tombol sedikit lebih tinggi dari gambarnya
    switch (constrain(x / (W / 5), 0, 4)) {
      case 0: putar(now - 1); break;
      case 1: jedaLanjut(); break;
      case 2: putar(now + 1); break;
      case 3: ubahVolume(-1); return true;
      case 4: ubahVolume(+1); return true;
    }
  }
  return false;
}

bool aksiGambar(int x, int y) {
  if (y >= TOMBOL_Y - 4) {
    switch (constrain(x / (W / 4), 0, 3)) {
      case 0: tampilGambar(noGambar - 1); break;
      case 1: otomatis = !otomatis; tGeserGambar = millis(); gambarTombolGambar(); break;
      case 2: tampilGambar(noGambar + 1); break;
      case 3: keLayar(L_MENU); break;
    }
  } else if (y < GAMBAR_H) {                            // ketuk sisi gambar
    if (x < W / 4) tampilGambar(noGambar - 1);
    else if (x >= W - W / 4) tampilGambar(noGambar + 1);
  }
  return false;
}

void titikSentuh(bool nyala) {            // titik kuning di header: tanda bahwa sentuhan terbaca
  static bool lalu = false;
  if (nyala == lalu) return;
  lalu = nyala;
  if (layar == L_GAMBAR) return;
  lcd.fillCircle(W / 2, KEPALA_H / 2, 6, nyala ? TFT_YELLOW : C_KEPALA);
}

void tanganiSentuh() {
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
      if (layar == L_MENU) ulang = aksiMenu(x, y);
      else if (layar == L_MUSIK) ulang = aksiMusik(x, y);
      else ulang = aksiGambar(x, y);
    } else if (ulang && layar == L_MUSIK && millis() - tTekan > 400 && millis() - tUlang > 150) { tUlang = millis(); aksiMusik(x, y); }
  } else if (tekan && millis() - tLepas > 150) { tekan = false; titikSentuh(false); }   // 0,15 detik tanpa sentuhan = jari diangkat
}

// Layar uji warna (Serial 't'): petak warna yang kamu tahu seharusnya seperti apa, plus gradasi abu-abu dan merah/hijau/biru.
// Bandingkan dengan keterangan di layar. Serial 'i' membalik pengaturan invert panel, 'm' kembali ke menu.
void layarUji() {
  lcd.fillScreen(TFT_BLACK);
  lcd.setTextColor(TFT_WHITE, TFT_BLACK); lcd.setTextSize(1);
  struct P { uint8_t r, g, b; const char* nama; };
  const P petak[8] = {
    {230, 120, 40, "ORANYE"}, {224, 172, 140, "KULIT"}, {70, 150, 50, "HIJAU RUMPUT"}, {100, 160, 230, "BIRU LANGIT"},
    {120, 70, 40, "COKELAT"}, {255, 220, 0, "KUNING"}, {160, 60, 160, "UNGU"}, {128, 128, 128, "ABU 50%"} };
  for (int k = 0; k < 8; k++) {
    int x = 8 + (k % 4) * 78, y = 6 + (k / 4) * 50;
    lcd.fillRect(x, y, 72, 36, lcd.color565(petak[k].r, petak[k].g, petak[k].b));
    lcd.setCursor(x, y + 39); lcd.print(petak[k].nama);
  }
  for (int k = 0; k < 16; k++) {                       // gradasi abu-abu: 16 langkah hitam ke putih
    uint8_t v = k * 17; lcd.fillRect(8 + k * 19, 112, 19, 28, lcd.color565(v, v, v));
  }
  for (int k = 0; k < 64; k++) {                       // gradasi merah, hijau, biru
    uint8_t v = k * 4 + 3;
    lcd.fillRect(8 + k * 4, 146, 4, 18, lcd.color565(v, 0, 0));
    lcd.fillRect(8 + k * 4, 166, 4, 18, lcd.color565(0, v, 0));
    lcd.fillRect(8 + k * 4, 186, 4, 18, lcd.color565(0, 0, v));
  }
  lcd.setCursor(8, 212); lcd.print("Serial: c = tukar chip, i = invert, m = menu");
}

void setup() {
  Serial.begin(115200);
  delay(1500);   // beri waktu Serial Monitor tersambung
  Serial.println("\n=== TES MEDIA ===");

  // layar: arah, jenis panel, dan kalibrasi sentuh dari tes_layar
  Preferences pref; pref.begin("robot", false);
  uint8_t arah = pref.getUChar("rot", 4), geo = pref.getUChar("geo", 1), rgb = pref.getUChar("rgb", 1);
  float aff[6]; bool adaKal = pref.getBytesLength("aff") == sizeof(aff);
  if (adaKal) pref.getBytes("aff", aff, sizeof(aff));
  bool icSimpan = pref.getUChar("ic", 1) != 0;       // 1 = pengaturan ILI9342C (bawaan), 0 = ILI9341
  pref.end();
  lcd.aturPanel(geo != 0, rgb != 0);
  lcd.pakaiIC(icSimpan);
  lcd.init(); lcd.setRotation(arah > 7 ? 4 : arah);
  if (lcd.width() < lcd.height()) lcd.setRotation(geo ? 4 : 1);    // program ini untuk layar mendatar 320 x 240
  lcd.setBrightness(255); lcd.fillScreen(TFT_BLACK);
  if (adaKal) lcd.panel()->setCalibrateAffine(aff);
  else Serial.println("PERINGATAN: kalibrasi sentuh belum ada. Jalankan tes_layar dulu, kalau tidak ketukan bisa meleset.");

  // SD card (sesudah layar, karena memakai kabel SPI yang sama)
  SPI.begin(TFT_SCK, TFT_MISO, TFT_MOSI);
  if (!SD.begin(SD_CS, SPI, 20000000))
    berhenti("SD GAGAL. Cek kabel SD_CS ke IO21, SD_SCK ke d2, SD_MOSI ke d5, SD_MISO ke d8, dan format kartu FAT32.");
  Serial.printf("SD terbaca, ukuran %llu MB\n", SD.cardSize() / 1048576);

  File root = SD.open("/");
  for (File f = root.openNextFile(); f; f = root.openNextFile()) {
    if (f.isDirectory()) continue;
    String n = String(f.name());
    if (lagunya(n) && jumlah < MAKS_LAGU) lagu[jumlah++] = n;
    else if (gambarnya(n) && nGambar < MAKS_GAMBAR) gambar[nGambar++] = n;
  }
  urut(lagu, jumlah); urut(gambar, nGambar);
  daftar();
  if (jumlah == 0 && nGambar == 0)
    berhenti("Tidak ada lagu (.mp3 .wav .flac) maupun gambar (.jpg .png .bmp) di folder utama SD. Salin beberapa file ke kartu, lalu tekan RST.");

  if (!audio.setPinout(AMP_BCLK, AMP_LRC, AMP_DIN))
    berhenti("Pin speaker gagal dipasang. Cek BCLK=IO4, LRC=IO5, DIN=IO7 ke MAX98357A.");
  audio.setVolume(volume);                 // 0 sampai 21
  siap = true;
  tampilkanMenu();
}

void loop() {
  audio.loop();                            // harus dipanggil sesering mungkin
  if (!siap) return;

  static bool pernahJalan = false;
  if (audio.isRunning()) pernahJalan = true;
  else if (pernahJalan && !jeda) {         // lagu habis: lanjut ke berikutnya
    pernahJalan = false; sedangMain = false;
    if (gagalBeruntun >= jumlah) { Serial.println("Semua file lagu gagal dibuka."); gagalBeruntun = 0; }
    else if (layar != L_GAMBAR) putar(now + 1);
  }

  static unsigned long tWaktu = 0, tSentuh = 0;
  if (layar == L_MUSIK && millis() - tWaktu > 1000) { tWaktu = millis(); gambarWaktu(); }
  if (millis() - tSentuh >= 4) { tSentuh = millis(); tanganiSentuh(); }
  if (layar == L_GAMBAR && otomatis && millis() - tGeserGambar > 5000) { tGeserGambar = millis(); tampilGambar(noGambar + 1); }

  while (Serial.available()) {
    int c = Serial.read();
    if (c == 'm') keLayar(L_MENU);
    else if (c == 'n') { if (layar == L_GAMBAR) tampilGambar(noGambar + 1); else if (jumlah) putar(now + 1); }
    else if (c == 'b') { if (layar == L_GAMBAR) tampilGambar(noGambar - 1); else if (jumlah) putar(now - 1); }
    else if (c == 'p' && jumlah) jedaLanjut();
    else if (c == '+') ubahVolume(+2);
    else if (c == '-') ubahVolume(-2);
    else if (c == 'l') daftar();
    else if (c == 't') layarUji();
    else if (c == 'c') {                                  // tukar jenis chip lalu mulai ulang
      Preferences pr; pr.begin("robot", false);
      uint8_t baru = pr.getUChar("ic", 1) ? 0 : 1; pr.putUChar("ic", baru); pr.end();
      Serial.printf("Pengaturan layar: %s. Mulai ulang...\n", baru ? "ILI9342C" : "ILI9341"); delay(300); ESP.restart();
    }
    else if (c == 'i') {
      static bool terbalik = false; terbalik = !terbalik; lcd.invertDisplay(terbalik);
      Serial.printf("invert layar: %s\n", terbalik ? "AKTIF" : "mati");
    }
  }
}
