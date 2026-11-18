/*
  ROBOT AI 4WD - OTAK (ESP32-S3 N16R8)
  -------------------------------------
  Layar sentuh + wajah robot, speaker, mikrofon, SD card, bot Telegram,
  AI Gemini dengan cadangan Groq (ngobrol, menggerakkan badan, melihat lewat kamera), dan perintah suara.
  Badan (DevKit V1) dikendalikan lewat UART. Kamera (ESP32-CAM) diambil lewat WiFi.

  PENGATURAN BOARD di Arduino IDE (Tools):
    Board            : ESP32S3 Dev Module
    Flash Size       : 16MB (128Mb)
    PSRAM            : OPI PSRAM            <- wajib, tanpa ini audio dan foto gagal
    Partition Scheme : 16M Flash (3MB APP/9.9MB FATFS)   <- wajib untuk upload lewat WiFi (OTA)
    USB CDC On Boot  : Enabled kalau log upload menulis "USB mode: USB-Serial/JTAG" (port USB bawaan chip),
                       Disabled kalau menulis "USB mode: UART"
  LIBRARY (Tools > Manage Libraries):
    LovyanGFX (lovyan03), UniversalTelegramBot (Brian Lough), ArduinoJson (Benoit Blanchon)
    ESP32-audioI2S (schreibfaul1) -> tidak ada di Library Manager, pasang dari ZIP GitHub (lihat panduan)

  SUARA COWOK: SUARA_GEMINI memakai Gemini TTS (kunci Gemini yang sama, gratis). Kosong = suara Google.

  Isi bagian KONFIGURASI di bawah. Jangan bagikan file yang sudah berisi token/API key.

  UPLOAD LEWAT WIFI (OTA): setelah sekali upload lewat USB dengan Partition Scheme di atas, robot muncul
  di Tools > Port sebagai port jaringan (nama robot + IP). Pilih port itu lalu Upload seperti biasa.
*/

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <WiFi.h>
#include <time.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <WebServer.h>
#include <SPI.h>
#include <SD.h>
#include <FFat.h>
#include <Preferences.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <base64.h>
#include "mbedtls/base64.h"
#include "esp_heap_caps.h"
#include "driver/gpio.h"
#include <ESP_I2S.h>
#include "Audio.h"

// ======================= KONFIGURASI =======================
const char* WIFI_NAMA       = "NAMA_WIFI";            // WiFi 2.4 GHz
const char* WIFI_PASSWORD   = "PASSWORD_WIFI";
const char* TELEGRAM_TOKEN  = "ISI_TOKEN_DARI_BOTFATHER";
const char* TELEGRAM_CHATID = "ISI_ANGKA_ID_KAMU";    // dari @userinfobot. Bot hanya menurut ke ID ini
const char* GEMINI_API_KEY  = "";                     // dari aistudio.google.com (gratis), diawali AIza...
const char* GEMINI_MODEL    = "gemini-3.5-flash-lite"; // AI utama, bisa melihat foto (jatah gratis 500/hari, Nov 2026)
const char* GEMMA_MODEL     = "gemma-4-31b-it";       // cadangan pertama, kunci yang sama (14.400/hari). Kosongkan untuk mematikan
const char* GROQ_API_KEY    = "";                     // gsk_...: suara ke teks, dan cadangan AI kalau jatah Gemini habis
const char* GROQ_MODEL      = "qwen/qwen3.8-27b";     // model Groq yang bisa melihat foto (cadangan)
const char* ANTHROPIC_API_KEY = "";                   // sk-ant-... dari console.anthropic.com (BERBAYAR). Kosong = Haiku mati
const char* HAIKU_MODEL     = "claude-haiku-5-5";     // Claude Haiku: cepat, bisa melihat foto, bayar per pemakaian
const char* KAMERA_ALAMAT   = "";                     // IP saja, contoh "192.168.1.50". Kosong = cari robotcam.local
const char* NAMA_ROBOT      = "Zond";                 // nama robot; perintah suara harus menyebut nama ini
const uint8_t ARAH_LAYAR    = 6;                      // 6 = slot SD di atas, 4 = slot SD di bawah. Tampilan terbalik? ganti 6 <-> 4
#define LAYAR_MHZ        27                       // kecepatan kirim ke layar. Layar putih/bergaris dengan kabel panjang? coba 10
#define ZONA_WAKTU       "WITA-8"                 // jam di layar: WIB-7 (Jawa, Sumatra), WITA-8 (Bali, NTB, Sulawesi), WIT-9 (Papua)
#define OTA_SANDI        ""                       // kata sandi upload lewat WiFi. Kosong = tanpa sandi (cukup untuk WiFi pribadi)
#define CUACA_KOTA       "Mataram"                // lokasi cuaca di layar. Angka lintang/bujur dari Google Maps (klik kanan peta)
#define CUACA_LAT        "-8.583"
#define CUACA_LON        "116.116"
#define SUARA_GEMINI     "Puck"                   // suara cowok Gemini TTS: Puck (ceria), Charon (tenang), Orus (tegas), Fenrir. Kosong = Google
#define SUARA_MODEL      "gemini-3.8-flash-tts"   // lebih cepat tapi sedikit kurang natural: gemini-3.8-flash-lite-tts
#define SUARA_GAYA       "santai dan ramah"       // gaya bicara (bahasa bebas). Kosong = biasa
#define LAYAR_LED        -1                       // isi 38 kalau kabel LED layar dipindah ke IO38 (kecerahan bisa diatur)
#define SHOLAT_KOREKSI   ""                       // koreksi menit jadwal sholat, misalnya "2,2,-2,2,2,2,0,2,0" (lihat panduan)
const char* NAMA_PEMILIK    = "Rizky";
const int   VOLUME_AWAL     = 12;                     // 0..21
// Stasiun untuk tombol RADIO di layar (ketuk lagi = stasiun berikutnya). Alamat stream MP3/AAC,
// cari di radio-browser.info (salin "Stream URL"). Tambah sebanyak yang kamu mau.
const char* DAFTAR_RADIO[] = {
  "http://stream.radioparadise.com/mp3-128",           // Radio Paradise (musik campuran, tanpa iklan)
};

// ======================= PIN (sesuai panduan) =======================
#define TFT_SCK   12
#define TFT_MOSI  11
#define TFT_MISO  13
#define TFT_CS    10
#define TFT_DC     9
#define TFT_RST    8
#define TOUCH_CS  14
#define SD_CS     21
#define SD_SCK    39    // SD card di jalur SPI sendiri (4 pad SD_ di tepi layar), terpisah dari layar dan sentuhan.
#define SD_MOSI   40    // Isi SD_SCK dengan -1 kalau SD masih ikut jalur layar (IO12/11/13, cara lama).
#define SD_MISO   41
#define AMP_BCLK   4    // MAX98357A
#define AMP_LRC    5
#define AMP_DIN    7
#define MIC_SCK   15    // INMP441
#define MIC_WS    16
#define MIC_SD     6
#define BADAN_TX  17    // ke RX2 DevKit (GPIO 16)
#define BADAN_RX  18    // dari TX2 DevKit (GPIO 17)

// ======================= LAYAR =======================
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

#ifndef LAYAR_MHZ                                     // kalau baris LAYAR_MHZ tidak ada di pengaturan atas
#define LAYAR_MHZ 27
#endif
#ifndef ZONA_WAKTU
#define ZONA_WAKTU "WITA-8"
#endif
#ifndef OTA_SANDI
#define OTA_SANDI ""
#endif
#ifndef CUACA_KOTA
#define CUACA_KOTA "Mataram"
#define CUACA_LAT "-8.583"
#define CUACA_LON "116.116"
#endif
#ifndef SUARA_GEMINI
#define SUARA_GEMINI "Puck"
#endif
#ifndef SUARA_MODEL
#define SUARA_MODEL "gemini-3.8-flash-tts"
#endif
#ifndef SUARA_GAYA
#define SUARA_GAYA ""
#endif
#ifndef LAYAR_LED
#define LAYAR_LED -1
#endif
#ifndef SHOLAT_KOREKSI
#define SHOLAT_KOREKSI ""
#endif

class LGFX : public lgfx::LGFX_Device {
  PanelKu _panel;
  lgfx::Bus_SPI _bus;
  lgfx::Touch_XPT2046 _touch;
  lgfx::Light_PWM _light;
public:
  LGFX() {
    { auto c = _bus.config();
      c.spi_host = SPI2_HOST; c.spi_mode = 0; c.freq_write = LAYAR_MHZ * 1000000UL; c.freq_read = 16000000;
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
    if (LAYAR_LED >= 0) {                   // lampu layar lewat PWM (kalau kabel LED ke GPIO)
      auto c = _light.config();
      c.pin_bl = LAYAR_LED; c.invert = false; c.freq = 12000; c.pwm_channel = 7;
      _light.config(c); _panel.setLight(&_light);
    }
    setPanel(&_panel);
  }
  void pakaiLayar(bool ic, bool fh, bool fv) { _panel.ic = ic; _panel.fh = fh; _panel.fv = fv; }
  void balikInvert(bool v) { auto c = _panel.config(); c.invert = v; _panel.config(c); }
  // Jenis panel. Modul merahmu ternyata panel MENDATAR asli (320 x 240): dengan setelan 240 x 320
  // hanya 240 x 240 piksel yang tergambar. lebar320 = true untuk modul seperti itu.
  // rgb = urutan warna; modul merahmu butuh true (kalau false, merah dan biru tertukar).
  // Berlaku setelah setRotation() berikutnya.
  void aturPanel(bool lebar320, bool rgb) {
    auto c = _panel.config();
    c.panel_width  = c.memory_width  = lebar320 ? 320 : 240;
    c.panel_height = c.memory_height = lebar320 ? 240 : 320;
    c.rgb_order = rgb;
    _panel.config(c);
  }
};

LGFX lcd;

Audio audio;                 // memakai I2S_NUM_0 untuk speaker
I2SClass mic;                // otomatis memakai I2S_NUM_1
Preferences pref;
WiFiClientSecure tgClient;
UniversalTelegramBot bot(TELEGRAM_TOKEN, tgClient);

SemaphoreHandle_t kunciSPI, kunciUART, kunciFoto, kunciInfo = nullptr;   // kunciFoto: satu pemakai foto dalam satu waktu
QueueHandle_t antrianAudio, antrianTugas, antrianKirim;   // antrianKirim: pesan Telegram dari tugas lain
SemaphoreHandle_t kunciAlarm;

// ======================= STATUS BERSAMA =======================
struct DataBadan { int depan = -1, belakang = -1; float baterai = -1, suhu = -1, lembap = -1; int pitch = 0, roll = 0, flag = 0, putar = 0; unsigned long terakhir = 0; };
volatile DataBadan badan;                  // putar: laju putar badan dari giroskop, derajat per detik
enum Ekspresi { NORMAL, DENGAR, PIKIR, BICARA, KAGET, LEMAH, NGANTUK, PUSING };
volatile Ekspresi ekspresi = NORMAL;
char teksStatus[96] = "Menyala...";
volatile int arahMata = 0;          // -1 kiri, 0 tengah, 1 kanan
volatile bool sedangBicara = false;
volatile bool audioJalan = false;          // speaker sedang memutar sesuatu (bicara, musik, radio)
volatile bool adaSD = false;
String alamatKamera;
volatile int pesanBadan = 0;        // 1 = baterai lemah, 2 = stop dikunci (diteruskan ke Telegram)
enum Mode { M_BIASA, M_IKUTI, M_JAGA, M_JELAJAH };
volatile Mode mode = M_BIASA;
volatile bool alarmJaga = false;    // diisi mode jaga, dikirim ke Telegram oleh tugas jaringan
volatile unsigned long ekspresiSampai = 0;   // ekspresi sementara (kaget, pusing) kembali normal setelah ini
volatile unsigned long tAktif = 0;           // terakhir ada kegiatan (bicara, perintah, sentuhan): untuk ekspresi mengantuk
volatile unsigned long tGerak = 0;           // terakhir badan diperintah bergerak: angkat/putar oleh tangan dibedakan dari gerak sendiri
volatile int putarSelesai = 0, putarHasil = 0;   // hasil perintah belok: 1 tercapai, 2 gagal/macet, 3 tanpa MPU, 4 ditolak
volatile bool otaJalan = false; volatile int otaPersen = 0;
volatile bool fiturKamera = false;           // "lihat sekeliling" dan "suit": harus diaktifkan dulu di panel web
volatile int aksiTertunda = 0;               // 1 = lihat sekeliling, 2 = suit (dijalankan tugas jaringan)
int skorKamu = 0, skorZond = 0;              // skor suit sejak menyala
volatile bool adaFFat = false;               // penyimpanan di flash (FATFS): tempat file suara Gemini
volatile bool pakaiSuaraAI = true;           // suara bicara: Gemini TTS (kalau SUARA_GEMINI diisi) atau Google
volatile int suaraGalat = 0;                 // kode galat Gemini TTS terakhir, 0 = belum pernah gagal
volatile uint32_t suaraHariIni = 0;          // jumlah kalimat Gemini TTS hari ini
unsigned long suaraIstirahat = 0;            // Gemini TTS tidak dicoba lagi sampai waktu ini (setelah ditolak)
String suaraPesanGalat;                      // potongan pesan galat dari Google, untuk Telegram dan Serial Monitor
volatile int persenBat = -1, sisaMenitBat = -1;   // isi baterai (%) dan perkiraan sisa waktu (menit)
float batHalus = -1;                         // tegangan baterai yang dihaluskan
volatile unsigned long jelajahSampai = 0;    // mode jelajah berhenti sendiri pada waktu ini

// ======================= AUDIO (task sendiri) =======================
enum JenisAudio { A_FILE, A_URL, A_TTS, A_STOP, A_VOL, A_JEDA };
char laguFile[100] = "";                       // lagu SD yang sedang/terakhir diputar, misalnya "/lagu1.mp3"
volatile bool musikJeda = false, laguHabis = false;   // laguHabis: lagu SD selesai sendiri (untuk lagu berikutnya)
volatile uint32_t laguDetik = 0, laguDurasi = 0;
int volSekarang = VOLUME_AWAL;   // volume yang sedang dipakai, 0..21
struct PerintahAudio { JenisAudio jenis; char isi[320]; };

// Semua jenis data (enum/struct) harus dibuat sebelum fungsi pertama: Arduino IDE menaruh deklarasi
// fungsi otomatis tepat sebelum fungsi pertama. Fungsi dengan nilai bawaan dideklarasikan sendiri di sini.
void kirimAudio(JenisAudio j, const String& isi = "");
// Fungsi yang dipakai sebelum ditulis. Dideklarasikan di sini supaya tidak bergantung pada prototipe otomatis Arduino.
bool jamSiap(struct tm& t);
bool pasangSD();
int mintaSuaraGemini(const String& teks, int tingkat);
void tambahPesan(const String& dari, const String& isi);
String namaHost();

void setStatus(const String& s) { strlcpy(teksStatus, s.c_str(), sizeof(teksStatus)); Serial.println("[status] " + s); tAktif = millis(); }

// Kirim pesan Telegram dari tugas mana saja; dikirim oleh tugas jaringan (bot Telegram hanya dipakai satu tugas).
struct PesanKirim { char isi[300]; };
void kirimTelegram(const String& t) {
  PesanKirim p; strlcpy(p.isi, t.c_str(), sizeof(p.isi));
  if (antrianKirim) xQueueSend(antrianKirim, &p, 0);
}

// Ekspresi sebentar (kaget, pusing), lalu kembali ke ekspresi dasar.
void ekspresiSebentar(Ekspresi e, unsigned long ms) { ekspresi = e; ekspresiSampai = millis() + ms; }

void kirimAudio(JenisAudio j, const String& isi) {
  PerintahAudio p; p.jenis = j; strlcpy(p.isi, isi.c_str(), sizeof(p.isi));
  xQueueSend(antrianAudio, &p, pdMS_TO_TICKS(100));
}

// Ambil potongan pertama teks (paling panjang maks huruf), dipotong di akhir kalimat atau di spasi kalau bisa.
String potongDepan(String& t, int maks) {
  t.trim();
  int potong = t.length();
  if (potong > maks) {
    potong = maks;
    for (int i = maks; i > maks * 2 / 5; i--) if (t[i] == '.' || t[i] == ',' || t[i] == '?' || t[i] == '!') { potong = i + 1; break; }
    if (potong == maks) for (int i = maks; i > maks * 2 / 5; i--) if (t[i] == ' ') { potong = i; break; }
  }
  String depan = t.substring(0, potong);
  t = t.substring(potong); t.trim();
  return depan;
}

// Suara Gemini dipakai kalau dipilih, nama suaranya diisi, penyimpanan flash siap, dan tidak sedang diistirahatkan.
bool suaraAIAktif() {
  return pakaiSuaraAI && strlen(SUARA_GEMINI) > 0 && strlen(GEMINI_API_KEY) > 0 && adaFFat &&
         (!suaraIstirahat || (long)(millis() - suaraIstirahat) >= 0);
}

// Kalimat dipecah: Google TTS hanya mau potongan pendek (<= 150 huruf). Gemini boleh lebih panjang: lebih sedikit
// permintaan (hemat jatah) dan lebih sedikit jeda.
void ucapkan(const String& teks) {
  String t = teks; t.trim();
  int maks = suaraAIAktif() ? 300 : 150;
  while (t.length()) kirimAudio(A_TTS, potongDepan(t, maks));
}

// ======================= SUARA GEMINI TTS (cowok) =======================
// Teks dikirim ke Gemini TTS (kunci GEMINI_API_KEY yang sama). Jawabannya JSON berisi audio WAV dalam base64.
// Audio itu disimpan di flash (/suara.wav) lalu diputar. Kalau gagal (jatah habis, internet putus), kalimat itu
// diucapkan suara Google.
const int SUARA_RATE = 16000;                       // 16 kHz cukup untuk speaker kecil, file lebih kecil dari 24 kHz

// Penampung jawaban di PSRAM (jawaban bisa ratusan KB, terlalu besar untuk RAM biasa).
class PenampungPSRAM : public Stream {
public:
  uint8_t* buf = nullptr; size_t len = 0, kap = 0;
  ~PenampungPSRAM() { if (buf) free(buf); }
  size_t write(uint8_t c) override { return write(&c, 1); }
  size_t write(const uint8_t* d, size_t n) override {
    if (len + n + 1 > kap) {
      size_t baru = max(kap * 2, len + n + 65536);
      uint8_t* b = (uint8_t*)ps_realloc(buf, baru);
      if (!b) return 0;
      buf = b; kap = baru;
    }
    memcpy(buf + len, d, n); len += n; buf[len] = 0;
    return n;
  }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
};

void catatSuaraHariIni() {
  struct tm j;
  uint32_t hari = jamSiap(j) ? (j.tm_year + 1900) * 10000 + (j.tm_mon + 1) * 100 + j.tm_mday : 0;
  Preferences q; q.begin("robot", false);
  if (hari && q.getUInt("tsH", 0) != hari) { q.putUInt("tsH", hari); q.putUInt("tsN", 0); }
  suaraHariIni = q.getUInt("tsN", 0) + 1;
  q.putUInt("tsN", suaraHariIni);
  q.end();
}

// Header WAV 44 byte untuk audio PCM 16-bit mono.
void tulisHeaderWav(File& f, uint32_t pcmByte, uint32_t rate) {
  uint8_t h[44] = {'R','I','F','F',0,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0,1,0,1,0,
                   0,0,0,0,0,0,0,0,2,0,16,0,'d','a','t','a',0,0,0,0};
  auto isi32 = [&](int i, uint32_t v) { h[i] = v; h[i + 1] = v >> 8; h[i + 2] = v >> 16; h[i + 3] = v >> 24; };
  isi32(4, 36 + pcmByte); isi32(24, rate); isi32(28, rate * 2); isi32(40, pcmByte);
  f.write(h, 44);
}

// Bentuk permintaan: 0 = PCM 16 kHz + gaya bicara (paling kecil), 1 = WAV bawaan 24 kHz + gaya, 2 = WAV bawaan tanpa gaya.
// Kalau Google menolak (kode 400), bentuk berikutnya dicoba dan yang berhasil dipakai terus sampai robot menyala ulang.
int tingkatSuara = 0;

// 0 = berhasil. Positif = kode HTTP dari Google. -1 sampai -11 = sambungan gagal (kode HTTPClient).
// -101 dan seterusnya = jawaban tidak berisi audio atau penyimpanan gagal.
int unduhSuaraGemini(const String& teks) {
  bool sudahUlang = false;
  for (;;) {
    int h = mintaSuaraGemini(teks, tingkatSuara);
    if (h == 400 && tingkatSuara < 2) {
      tingkatSuara++;
      Serial.printf("[suara] ditolak (400), coba bentuk permintaan %d\n", tingkatSuara);
      continue;
    }
    if (h < 0 && h > -100 && !sudahUlang) {             // sambungan gagal: coba sekali lagi
      sudahUlang = true;
      Serial.printf("[suara] sambungan gagal (%s), coba lagi\n", suaraPesanGalat.c_str());
      continue;
    }
    return h;
  }
}

int mintaSuaraGemini(const String& teks, int tingkat) {
  WiFiClientSecure klien;                              // sambungan baru tiap kalimat (sambungan lama bisa diputus Google)
  klien.setInsecure();
  HTTPClient http;
  if (!http.begin(klien, "https://generativelanguage.googleapis.com/v1beta/interactions")) { suaraPesanGalat = "begin gagal"; return -1; }
  http.setTimeout(40000);                              // membuat suara bisa makan belasan detik untuk kalimat panjang
  http.addHeader("x-goog-api-key", GEMINI_API_KEY);
  http.addHeader("Content-Type", "application/json");
  JsonDocument q;
  q["model"] = SUARA_MODEL;
  JsonObject isi = q["input"].add<JsonObject>();
  isi["type"] = "user_input";
  JsonObject teksObj = isi["content"].add<JsonObject>();
  teksObj["type"] = "text"; teksObj["text"] = teks;
  if (strlen(SUARA_GAYA) && tingkat < 2) {
    JsonObject g = teksObj["annotations"].add<JsonObject>(); g["type"] = "speech_metadata"; g["style"] = SUARA_GAYA;
  }
  q["response_format"]["type"] = "audio";
  if (tingkat == 0) { q["response_format"]["mime_type"] = "audio/l16"; q["response_format"]["sample_rate"] = SUARA_RATE; }
  q["generation_config"]["speech_config"].add<JsonObject>()["voice"] = SUARA_GEMINI;
  String permintaan; serializeJson(q, permintaan);
  unsigned long t0 = millis();
  int kode = http.POST(permintaan);
  if (kode <= 0) {
    suaraPesanGalat = http.errorToString(kode) + " setelah " + String((millis() - t0) / 1000) + " detik, RAM bebas " +
                      String(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) / 1024) + " KB";
    http.end();
    return kode < 0 ? kode : -1;
  }
  PenampungPSRAM jaw;
  int terima = http.writeToStream(&jaw);
  http.end();
  if (kode != 200) {
    suaraPesanGalat = jaw.buf ? String((const char*)jaw.buf).substring(0, 240) : String();
    return kode;
  }
  if (!jaw.buf) { suaraPesanGalat = "jawaban kosong (" + http.errorToString(terima) + ")"; return -101; }
  // Cari string "data" terpanjang di JSON: itu audionya (base64). Tidak memakai ArduinoJson karena ukurannya besar.
  char* awal = nullptr; size_t panjang = 0;
  for (char* p = strstr((char*)jaw.buf, "\"data\""); p; p = strstr(p + 6, "\"data\"")) {
    char* v = p + 6;
    while (*v == ' ' || *v == ':' || *v == '\n' || *v == '\r' || *v == '\t') v++;
    if (*v != '"') continue;
    char* akhir = strchr(v + 1, '"');
    if (akhir && (size_t)(akhir - v - 1) > panjang) { awal = v + 1; panjang = akhir - v - 1; }
  }
  if (!awal || panjang < 100) { suaraPesanGalat = String((const char*)jaw.buf).substring(0, 240); return -102; }
  size_t n = 0;                                        // buang garis miring terbalik (escape JSON) kalau ada
  for (size_t i = 0; i < panjang; i++) if (awal[i] != '\\') awal[n++] = awal[i];
  size_t ukuran = n / 4 * 3 + 4, hasil = 0;
  uint8_t* audioBuf = (uint8_t*)ps_malloc(ukuran);
  if (!audioBuf) { suaraPesanGalat = "PSRAM penuh"; return -103; }
  if (mbedtls_base64_decode(audioBuf, ukuran, &hasil, (const uint8_t*)awal, n) != 0 || hasil < 100) {
    free(audioBuf); suaraPesanGalat = "audio base64 rusak"; return -104;
  }
  File f = FFat.open("/suara.wav", FILE_WRITE);
  if (!f) { free(audioBuf); suaraPesanGalat = "tidak bisa menulis ke flash"; return -105; }
  if (memcmp(audioBuf, "RIFF", 4) != 0) tulisHeaderWav(f, hasil, tingkat == 0 ? SUARA_RATE : 24000);   // PCM tanpa header
  size_t tertulis = f.write(audioBuf, hasil);
  f.close();
  free(audioBuf);
  if (tertulis != hasil) { suaraPesanGalat = "flash penuh"; return -106; }
  return 0;
}

// Dipanggil tugas audio. true = sedang diputar dengan suara Gemini, false = pakai Google.
bool bicaraSuaraAI(const String& teks) {
  if (!suaraAIAktif() || WiFi.status() != WL_CONNECTED) return false;
  unsigned long t0 = millis();
  int h = unduhSuaraGemini(teks);
  if (h != 0) {
    Serial.printf("[suara] Gemini TTS gagal (%d, bentuk %d), kalimat ini memakai suara Google. %s\n", h, tingkatSuara, suaraPesanGalat.c_str());
    if (h != suaraGalat)                                 // satu kali per jenis galat
      kirimTelegram("Suara Gemini gagal (kode " + String(h) + (h == 429 ? ": jatah habis atau terlalu sering" : "") +
                    ", " SUARA_MODEL ", suara " SUARA_GEMINI ", bentuk " + String(tingkatSuara) + "). Sementara memakai suara Google. " +
                    suaraPesanGalat);
    suaraGalat = h;
    if (h == 429) suaraIstirahat = millis() + 300000UL;                          // jatah: coba lagi 5 menit kemudian
    else if (h == 400 || h == 401 || h == 403 || h == 404) suaraIstirahat = millis() + 600000UL;
    else if (h < 0 && h > -100) suaraIstirahat = millis() + 120000UL;           // sambungan gagal: pakai Google 2 menit dulu
    return false;
  }
  suaraGalat = 0;
  catatSuaraHariIni();
  Serial.printf("[suara] Gemini %u ms (bentuk %d), kalimat ke-%u hari ini\n", (unsigned)(millis() - t0), tingkatSuara, (unsigned)suaraHariIni);
  return audio.connecttoFS(FFat, "/suara.wav");
}

void aturSuara(bool ai) {
  pakaiSuaraAI = ai;
  if (ai) suaraIstirahat = 0;                           // dipilih lagi: langsung dicoba
  Preferences q; q.begin("robot", false); q.putUChar("suara", ai ? 1 : 0); q.end();
}

String teksSuara() {
  if (!strlen(SUARA_GEMINI)) return "Suara: Google (SUARA_GEMINI di kode masih kosong).";
  if (!strlen(GEMINI_API_KEY)) return "Suara: Google (GEMINI_API_KEY masih kosong).";
  if (!adaFFat) return "Suara: Google (penyimpanan flash tidak siap. Partition Scheme harus 16M Flash (3MB APP/9.9MB FATFS)).";
  String t = String("Suara: ") + (pakaiSuaraAI ? "Gemini " SUARA_GEMINI : "Google") + ". Suara Gemini hari ini " +
             String((uint32_t)suaraHariIni) + " kalimat.";
  if (suaraGalat) t += " Galat terakhir: kode " + String(suaraGalat) + ".";
  return t;
}

// Pesan dari library Audio (format lagu, kesalahan baca, akhir lagu) dicetak ke Serial Monitor.
void infoAudio(Audio::msg_t m) {
  if (m.e == Audio::evt_vu || m.e == Audio::evt_spectrum) return;   // data meter, terlalu sering
  Serial.printf("[audio] %s: %s\n", m.s ? m.s : "", m.msg ? m.msg : "");
}

// SD di jalur sendiri tidak perlu bergantian dengan layar. Kalau masih ikut jalur layar, wajib lewat kunciSPI.
const bool SD_BERBAGI = SD_SCK < 0;
SPIClass spiSD(HSPI);                              // jalur SPI kedua S3, khusus SD card
void kunciSD() { if (SD_BERBAGI) xSemaphoreTake(kunciSPI, portMAX_DELAY); }
void lepasSD() { if (SD_BERBAGI) xSemaphoreGive(kunciSPI); }

void tugasAudio(void*) {
  std::vector<String> antreTTS;
  bool pakaiSD = false;          // sedang memutar dari SD
  String laguSekarang;           // untuk menyambung ulang kalau pembacaan SD macet
  uint32_t posLalu = 0; unsigned long tPosBerubah = 0, tPosisi = 0;
  bool sengaja = true, jalanLalu = false;   // sengaja: lagu dihentikan orang (bukan selesai sendiri)
  for (;;) {
    PerintahAudio p;
    while (xQueueReceive(antrianAudio, &p, 0) == pdTRUE) {
      kunciSD();
      switch (p.jenis) {
        case A_FILE: antreTTS.clear(); pakaiSD = true; laguSekarang = p.isi; strlcpy(laguFile, p.isi, sizeof(laguFile));
                     musikJeda = false; sengaja = false; laguDetik = laguDurasi = 0; audio.connecttoFS(SD, p.isi); break;
        case A_URL:  antreTTS.clear(); pakaiSD = false; sengaja = true; musikJeda = false; laguFile[0] = 0; audio.connecttohost(p.isi); break;
        case A_TTS:  if ((audio.isRunning() || musikJeda) && !sedangBicara) { audio.stopSong(); sengaja = true; musikJeda = false; }   // bicara memotong musik
                     antreTTS.push_back(String(p.isi)); break;
        case A_STOP: antreTTS.clear(); audio.stopSong(); sengaja = true; musikJeda = false; break;
        case A_VOL:  volSekarang = constrain(atoi(p.isi), 0, 21); audio.setVolume(volSekarang); break;
        case A_JEDA: if (pakaiSD && laguFile[0]) { audio.pauseResume(); musikJeda = !musikJeda; } break;
      }
      lepasSD();
    }
    if (!audio.isRunning() && !antreTTS.empty()) {
      String s = antreTTS.front(); antreTTS.erase(antreTTS.begin());
      pakaiSD = false; sedangBicara = true; audioJalan = true;
      if (!bicaraSuaraAI(s)) {
        if (s.length() > 150) {                          // Google hanya mau potongan pendek: pecah lagi
          std::vector<String> pecahan;
          while (s.length()) pecahan.push_back(potongDepan(s, 150));
          antreTTS.insert(antreTTS.begin(), pecahan.begin() + 1, pecahan.end());
          s = pecahan[0];
        }
        audio.connecttospeech(s.c_str(), "id");
      }
    }
    if (!audio.isRunning() && antreTTS.empty()) sedangBicara = false;
    audioJalan = audio.isRunning() || !antreTTS.empty();
    if (pakaiSD) kunciSD();
    audio.loop();
    if (pakaiSD) lepasSD();
    bool jalanSD = pakaiSD && audio.isRunning();
    if (jalanLalu && !jalanSD && !musikJeda && !sengaja) { laguHabis = true; sengaja = true; }   // lagu selesai sendiri
    jalanLalu = jalanSD;
    if (jalanSD && millis() - tPosisi > 500) { tPosisi = millis(); laguDetik = audio.getAudioCurrentTime(); laguDurasi = audio.getAudioFileDuration(); }
    // Pembacaan SD kadang macet di tengah lagu (gangguan di kabel SPI): posisi file berhenti naik.
    // Pasang ulang SD lalu lanjutkan lagu dari detik terakhir.
    if (pakaiSD && audio.isRunning()) {
      uint32_t pos = audio.getAudioFilePosition();
      if (pos != posLalu) { posLalu = pos; tPosBerubah = millis(); }
      else if (millis() - tPosBerubah > 3000 && laguSekarang.length()) {
        uint32_t detik = audio.getAudioCurrentTime();
        Serial.printf("[audio] SD macet di detik %u, pasang ulang SD dan lanjutkan\n", (unsigned)detik);
        audio.stopSong();
        adaSD = false; pasangSD();
        if (adaSD) { kunciSD(); audio.connecttoFS(SD, laguSekarang.c_str(), detik); lepasSD(); }
        else { pakaiSD = false; sengaja = true; setStatus("SD card terlepas"); }
        posLalu = 0; tPosBerubah = millis();
      }
    }
    static unsigned long tLapor = 0;                     // laporan tiap 3 detik saat memutar lagu dari SD
    if (pakaiSD && millis() - tLapor > 3000) {
      tLapor = millis();
      if (audio.isRunning())
        Serial.printf("[audio] detik %u, posisi %u, buffer %u, PSRAM bebas %u\n", (unsigned)audio.getAudioCurrentTime(),
                      (unsigned)audio.getAudioFilePosition(), (unsigned)audio.inBufferFilled(), (unsigned)ESP.getFreePsram());
    }
    vTaskDelay(1);
  }
}

// ======================= BADAN (UART) =======================
void kirimBadan(const String& s) {
  xSemaphoreTake(kunciUART, portMAX_DELAY);
  Serial1.println(s);
  xSemaphoreGive(kunciUART);
}

void gerak(int kiri, int kanan, int ms) {
  kirimBadan("M " + String(kiri) + " " + String(kanan) + " " + String(ms));
  arahMata = (kiri > kanan + 40) ? 1 : (kanan > kiri + 40) ? -1 : 0;
  tGerak = millis() + ms;
}

// Belok presisi pakai giroskop di badan (perintah T). derajat + = kanan, - = kiri.
// tungguMs = 0: langsung kembali. Selain itu tunggu hasilnya: 1 tercapai, 2 macet, 3 badan tanpa MPU, 4 ditolak, 0 tidak ada jawaban.
// Kalau badan belum punya giroskop (kode badan lama), belok memakai waktu seperti dulu.
int belok(int derajat, int tungguMs) {
  derajat = constrain(derajat, -720, 720);
  if (derajat == 0) return 1;
  putarSelesai = 0;
  arahMata = derajat > 0 ? 1 : -1;
  tGerak = millis() + 3000 + abs(derajat) * 15;
  kirimBadan("T " + String(derajat) + " 170");
  if (tungguMs <= 0) return 0;
  unsigned long t0 = millis();
  while (!putarSelesai && millis() - t0 < (unsigned long)tungguMs) vTaskDelay(pdMS_TO_TICKS(20));
  int h = putarSelesai;
  if (h == 3) {                                       // tanpa giroskop: kira-kira 4,5 ms per derajat pada kecepatan 160
    int ms = constrain(abs(derajat) * 5, 100, 3000);
    gerak(derajat > 0 ? 160 : -160, derajat > 0 ? -160 : 160, ms);
    vTaskDelay(pdMS_TO_TICKS(ms + 150));
  }
  arahMata = 0;
  return h;
}

void bacaBadan() {
  static String buf;
  while (Serial1.available()) {
    char c = Serial1.read();
    if (c == '\n') {
      buf.trim();
      if (buf.startsWith("D ")) {
        int d, b, p, r, f, g = 0; float v, s, h;
        // kolom ke-9 (laju putar) hanya dikirim kode badan baru; kode badan lama mengirim 8 kolom
        if (sscanf(buf.c_str(), "D %d %d %f %f %f %d %d %d %d", &d, &b, &v, &s, &h, &p, &r, &f, &g) >= 8) {
          badan.depan = d; badan.belakang = b; badan.baterai = v; badan.suhu = s; badan.lembap = h;
          badan.pitch = p; badan.roll = r; badan.flag = f; badan.putar = g; badan.terakhir = millis();
        }
      } else if (buf.startsWith("OK T ")) {
        putarHasil = buf.substring(5).toInt(); putarSelesai = 1;
        Serial.println("[badan] " + buf);
      } else if (buf.startsWith("E ")) {
        Serial.println("[badan] " + buf);
        if (buf.indexOf("PUTAR_GAGAL") > 0) { putarHasil = buf.substring(buf.lastIndexOf(' ') + 1).toInt(); putarSelesai = 2; }
        if (buf.indexOf("TANPA_MPU") > 0) putarSelesai = 3;
        if (buf.indexOf("TIDAK_DIKENAL T ") > 0) putarSelesai = 3;   // badan masih kode lama: belum kenal perintah T
        if (buf.indexOf("DITOLAK") > 0) { if (!putarSelesai) putarSelesai = 4; ekspresiSebentar(KAGET, 1500); setStatus("Ada halangan!"); }
        if (buf.indexOf("BATERAI_LEMAH") > 0) { ekspresi = LEMAH; pesanBadan = 1; }
        if (buf.indexOf("STOP_DIKUNCI") > 0) pesanBadan = 2;
      }
      buf = "";
    } else if (c != '\r' && buf.length() < 100) buf += c;
  }
}

// Sensor suhu/lembap tidak wajib: badan mengirim -1 kalau tidak ada.
bool adaSensorSuhu() { return badan.suhu > -40 && fabsf(badan.suhu + 1) > 0.05f; }

String ringkasSensor() {
  char b[260];
  bool hidup = badan.terakhir && millis() - badan.terakhir < 2000;
  if (!hidup) return "badan tidak terhubung";
  char isi[16] = "";
  if (persenBat >= 0) snprintf(isi, sizeof(isi), " (%d%%)", (int)persenBat);
  char cuaca[40] = "";
  if (adaSensorSuhu()) snprintf(cuaca, sizeof(cuaca), ", suhu %.1f C, lembap %.0f%%", badan.suhu, badan.lembap);
  snprintf(b, sizeof(b), "jarak depan %d cm, belakang %d cm, baterai %.2f V%s%s, miring %d/%d derajat%s",
           badan.depan, badan.belakang, badan.baterai, isi, cuaca, badan.pitch, badan.roll,
           (badan.flag & 1) ? ", TOMBOL STOP DIKUNCI" : "");
  return String(b);
}

// ======================= JAM (dari internet, NTP) =======================
const char* NAMA_HARI[]  = {"Minggu", "Senin", "Selasa", "Rabu", "Kamis", "Jumat", "Sabtu"};
const char* NAMA_BULAN[] = {"Januari", "Februari", "Maret", "April", "Mei", "Juni", "Juli",
                            "Agustus", "September", "Oktober", "November", "Desember"};

bool jamSiap(struct tm& t) {
  time_t s = time(nullptr);
  if (s < 1700000000) return false;            // belum tersinkron dari internet
  localtime_r(&s, &t);
  return true;
}

// "Minggu, 15 November 2026" dan "19:04"; kosong kalau jam belum tersinkron.
String teksTanggal() {
  struct tm t; if (!jamSiap(t)) return "";
  char b[40]; snprintf(b, sizeof(b), "%s, %d %s %d", NAMA_HARI[t.tm_wday], t.tm_mday, NAMA_BULAN[t.tm_mon], t.tm_year + 1900);
  return b;
}
String teksJam() {
  struct tm t; if (!jamSiap(t)) return "";
  char b[8]; snprintf(b, sizeof(b), "%02d:%02d", t.tm_hour, t.tm_min);
  return b;
}

// ======================= CUACA (Open-Meteo, gratis tanpa API key) =======================
struct Cuaca {
  bool ada = false; float suhu = 0, tertinggi = 0, terendah = 0; int lembap = 0, kode = 0, hujan = 0;
  int hariKode[4] = {0}, hariHujan[4] = {0}; float hariMaks[4] = {0}, hariMin[4] = {0};   // [0] hari ini, [1] besok, ...
  int nJam = 0, jamAwal = 0, jamHujan[12] = {0}; float jamSuhu[12] = {0};               // 12 jam ke depan
};
Cuaca cuaca;                                 // diisi tugas jaringan tiap 30 menit

// Kode cuaca WMO dari Open-Meteo -> kata Indonesia.
const char* namaCuaca(int k) {
  if (k == 0) return "cerah";
  if (k == 1) return "cerah berawan";
  if (k == 2) return "berawan";
  if (k == 3) return "mendung";
  if (k == 45 || k == 48) return "berkabut";
  if (k >= 51 && k <= 57) return "gerimis";
  if (k == 61 || k == 80) return "hujan ringan";
  if (k == 63 || k == 81) return "hujan sedang";
  if (k == 65 || k == 82 || k == 66 || k == 67) return "hujan lebat";
  if (k >= 71 && k <= 77) return "bersalju";
  if (k >= 95) return "badai petir";
  return "tidak diketahui";
}

bool ambilCuaca() {
  WiFiClientSecure c; c.setInsecure();
  HTTPClient http;
  http.begin(c, String("https://api.open-meteo.com/v1/forecast?latitude=") + CUACA_LAT + "&longitude=" + CUACA_LON +
                "&current=temperature_2m,relative_humidity_2m,weather_code"
                "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max&timezone=auto&forecast_days=4"
                "&hourly=temperature_2m,precipitation_probability&forecast_hours=12");
  http.setTimeout(10000);
  int kode = http.GET();
  String resp = kode == 200 ? http.getString() : String();
  http.end();
  if (kode != 200) { Serial.printf("[cuaca] kode %d\n", kode); return false; }
  JsonDocument r;
  if (deserializeJson(r, resp)) return false;
  Cuaca c2;
  c2.suhu = r["current"]["temperature_2m"] | 0.0f;
  c2.lembap = r["current"]["relative_humidity_2m"] | 0;
  c2.kode = r["current"]["weather_code"] | -1;
  c2.tertinggi = r["daily"]["temperature_2m_max"][0] | 0.0f;
  c2.terendah = r["daily"]["temperature_2m_min"][0] | 0.0f;
  c2.hujan = r["daily"]["precipitation_probability_max"][0] | 0;
  for (int i = 0; i < 4; i++) {
    c2.hariKode[i] = r["daily"]["weather_code"][i] | 0;
    c2.hariMaks[i] = r["daily"]["temperature_2m_max"][i] | 0.0f;
    c2.hariMin[i] = r["daily"]["temperature_2m_min"][i] | 0.0f;
    c2.hariHujan[i] = r["daily"]["precipitation_probability_max"][i] | 0;
  }
  const char* jam0 = r["hourly"]["time"][0] | "";             // "2026-11-18T18:00"
  c2.jamAwal = strlen(jam0) >= 13 ? atoi(jam0 + 11) : 0;
  for (int i = 0; i < 12; i++) {
    if (r["hourly"]["temperature_2m"][i].isNull()) break;
    c2.jamSuhu[i] = r["hourly"]["temperature_2m"][i] | 0.0f;
    c2.jamHujan[i] = r["hourly"]["precipitation_probability"][i] | 0;
    c2.nJam = i + 1;
  }
  c2.ada = c2.kode >= 0;
  if (kunciInfo) xSemaphoreTake(kunciInfo, portMAX_DELAY);
  cuaca = c2;
  if (kunciInfo) xSemaphoreGive(kunciInfo);
  Serial.printf("[cuaca] %s %.0f C, %s\n", CUACA_KOTA, c2.suhu, namaCuaca(c2.kode));
  return c2.ada;
}

// "28C berawan" untuk layar, atau kalimat lengkap untuk diucapkan / AI. Kosong kalau belum ada data.
String teksCuaca(bool lengkap) {
  if (!cuaca.ada) return "";
  char b[180];
  if (!lengkap) snprintf(b, sizeof(b), "%.0fC %s", cuaca.suhu, namaCuaca(cuaca.kode));
  else snprintf(b, sizeof(b), "Cuaca di %s sekarang %s, %.0f derajat, kelembapan %d persen. Hari ini %.0f sampai %.0f derajat, peluang hujan %d persen.",
                CUACA_KOTA, namaCuaca(cuaca.kode), cuaca.suhu, cuaca.lembap, cuaca.terendah, cuaca.tertinggi, cuaca.hujan);
  return String(b);
}

// ======================= ALARM DAN PENGINGAT =======================
// Disimpan di memori S3 (tetap ada walau robot dimatikan). hari: bit 0 = Minggu ... bit 6 = Sabtu.
// hari = 0 berarti sekali saja pada tanggal (yyyymmdd), lalu dihapus sendiri setelah berbunyi.
// bunyi: 0 = suara saja, 1 = suara lalu radio pertama, 2 = suara lalu lagu SD berikutnya.
struct Alarm { uint8_t jam = 0, menit = 0, hari = 0, bunyi = 0; uint32_t tanggal = 0, terakhir = 0; char pesan[80] = ""; };
const int ALARM_MAKS = 12;
Alarm daftarAlarm[ALARM_MAKS];
int jumlahAlarm = 0;
volatile int bunyiTertunda = 0; volatile unsigned long tBunyiTertunda = 0;   // radio/lagu diputar setelah pengingat diucapkan

uint32_t angkaTanggal(int tahun, int bulan, int hari) { return (uint32_t)tahun * 10000 + bulan * 100 + hari; }

void simpanAlarm() {                        // dipanggil saat kunciAlarm sedang dipegang
  JsonDocument d;
  JsonArray a = d.to<JsonArray>();
  for (int i = 0; i < jumlahAlarm; i++) {
    JsonObject o = a.add<JsonObject>();
    o["j"] = daftarAlarm[i].jam; o["m"] = daftarAlarm[i].menit; o["h"] = daftarAlarm[i].hari;
    o["b"] = daftarAlarm[i].bunyi; o["t"] = daftarAlarm[i].tanggal; o["p"] = (const char*)daftarAlarm[i].pesan;
  }
  String isi; serializeJson(d, isi);
  Preferences q; q.begin("robot", false); q.putString("alarm", isi); q.end();
}

void muatAlarm() {
  Preferences q; q.begin("robot", true); String isi = q.getString("alarm", "[]"); q.end();
  JsonDocument d;
  if (deserializeJson(d, isi)) return;
  jumlahAlarm = 0;
  for (JsonObject o : d.as<JsonArray>()) {
    if (jumlahAlarm >= ALARM_MAKS) break;
    Alarm& a = daftarAlarm[jumlahAlarm++];
    a.jam = o["j"] | 0; a.menit = o["m"] | 0; a.hari = o["h"] | 0; a.bunyi = o["b"] | 0;
    a.tanggal = o["t"] | 0; a.terakhir = 0; strlcpy(a.pesan, o["p"] | "", sizeof(a.pesan));
  }
}

// Tambah alarm. hari = 0: sekali, pada kemunculan jam itu berikutnya (hari ini, atau besok kalau sudah lewat).
// Kembalikan "" kalau berhasil, atau alasan gagalnya.
String tambahAlarm(int jam, int menit, int hari, const String& pesan, int bunyi) {
  if (jam < 0 || jam > 23 || menit < 0 || menit > 59) return "jamnya tidak valid";
  struct tm t;
  if (!jamSiap(t)) return "jam robot belum tersinkron dari internet";
  uint32_t tanggal = 0;
  if (hari == 0) {
    time_t s = time(nullptr);
    if (jam * 60 + menit <= t.tm_hour * 60 + t.tm_min) s += 86400;   // sudah lewat hari ini: besok
    struct tm b; localtime_r(&s, &b);
    tanggal = angkaTanggal(b.tm_year + 1900, b.tm_mon + 1, b.tm_mday);
  }
  String galat;
  xSemaphoreTake(kunciAlarm, portMAX_DELAY);
  if (jumlahAlarm >= ALARM_MAKS) galat = "alarm sudah penuh (paling banyak 12)";
  else {
    Alarm& a = daftarAlarm[jumlahAlarm++];
    a.jam = jam; a.menit = menit; a.hari = hari & 0x7F; a.bunyi = constrain(bunyi, 0, 2);
    a.tanggal = tanggal; a.terakhir = 0;
    strlcpy(a.pesan, pesan.length() ? pesan.c_str() : "Waktunya!", sizeof(a.pesan));
    simpanAlarm();
  }
  xSemaphoreGive(kunciAlarm);
  return galat;
}

bool hapusAlarm(int i) {
  xSemaphoreTake(kunciAlarm, portMAX_DELAY);
  bool ok = i >= 0 && i < jumlahAlarm;
  if (ok) {
    for (int k = i; k < jumlahAlarm - 1; k++) daftarAlarm[k] = daftarAlarm[k + 1];
    jumlahAlarm--;
    simpanAlarm();
  }
  xSemaphoreGive(kunciAlarm);
  return ok;
}

String teksHariAlarm(int hari, uint32_t tanggal) {
  if (hari == 0) {
    char b[28]; snprintf(b, sizeof(b), "sekali, %u/%u/%u", (unsigned)(tanggal % 100), (unsigned)(tanggal / 100 % 100), (unsigned)(tanggal / 10000));
    return String(b);
  }
  if (hari == 0x7F) return "setiap hari";
  if (hari == 0x3E) return "Senin-Jumat";
  if (hari == 0x41) return "Sabtu-Minggu";
  String s;
  for (int i = 0; i < 7; i++) if (hari & (1 << i)) { if (s.length()) s += ", "; s += NAMA_HARI[i]; }
  return s;
}

String daftarAlarmTeks() {
  String s;
  xSemaphoreTake(kunciAlarm, portMAX_DELAY);
  for (int i = 0; i < jumlahAlarm; i++) {
    char b[24]; snprintf(b, sizeof(b), "%d. %02d:%02d ", i + 1, daftarAlarm[i].jam, daftarAlarm[i].menit);
    s += String(b) + daftarAlarm[i].pesan + " (" + teksHariAlarm(daftarAlarm[i].hari, daftarAlarm[i].tanggal) + ")\n";
  }
  xSemaphoreGive(kunciAlarm);
  return s.length() ? s : String("Belum ada alarm.\n");
}

// Dipanggil terus dari loop(); memeriksa tiap detik. Alarm yang cocok dengan jam sekarang berbunyi sekali per menit itu.
void cekAlarm() {
  static unsigned long t = 0;
  if (millis() - t < 1000) return;
  t = millis();
  struct tm j;
  if (!jamSiap(j)) return;
  uint32_t hariIni = angkaTanggal(j.tm_year + 1900, j.tm_mon + 1, j.tm_mday);
  uint32_t cap = (uint32_t)(j.tm_mday * 10000 + j.tm_hour * 100 + j.tm_min);   // penanda menit ini
  String bunyikan; int bunyi = 0;
  xSemaphoreTake(kunciAlarm, portMAX_DELAY);
  for (int i = 0; i < jumlahAlarm; i++) {
    Alarm& a = daftarAlarm[i];
    if (a.jam != j.tm_hour || a.menit != j.tm_min || a.terakhir == cap) continue;
    bool cocok = a.hari ? ((a.hari >> j.tm_wday) & 1) : (a.tanggal == hariIni);
    if (!cocok) continue;
    a.terakhir = cap;
    bunyikan = a.pesan; bunyi = a.bunyi;
    if (a.hari == 0) {                                  // sekali: hapus
      for (int k = i; k < jumlahAlarm - 1; k++) daftarAlarm[k] = daftarAlarm[k + 1];
      jumlahAlarm--;
      simpanAlarm();
    }
    break;                                              // satu alarm per detik; yang lain menyusul detik berikutnya
  }
  xSemaphoreGive(kunciAlarm);
  if (!bunyikan.length()) return;
  String ucap = "Pengingat: " + bunyikan + ".";
  if (j.tm_hour < 11 && cuaca.ada) ucap += " " + teksCuaca(true);   // pengingat pagi sekalian menyebut cuaca
  ekspresiSebentar(KAGET, 2000);
  setStatus("Pengingat: " + bunyikan);
  ucapkan(ucap);
  char jm[8]; snprintf(jm, sizeof(jm), "%02d:%02d", j.tm_hour, j.tm_min);
  kirimTelegram(String("Pengingat ") + jm + ": " + bunyikan);
  if (bunyi) { bunyiTertunda = bunyi; tBunyiTertunda = millis(); }
}

// ======================= EKSPRESI OTOMATIS =======================
// Ekspresi dasar mengikuti keadaan: lesu kalau baterai lemah, mengantuk kalau malam dan lama tidak ada kegiatan.
// Kaget kalau diangkat, pusing kalau diputar-putar dengan tangan (dari MPU6050 di badan).
void ekspresiOtomatis() {
  static unsigned long t = 0, tPutarMulai = 0, tKomentar = 0;
  static int pitchLalu = 0, rollLalu = 0;
  if (millis() - t < 250) return;
  t = millis();
  if (otaJalan) return;
  bool hidup = badan.terakhir && millis() - badan.terakhir < 2000;
  bool gerakSendiri = (long)(millis() - tGerak) < 1500;   // badan sedang/baru saja diperintah bergerak

  if (hidup && !gerakSendiri) {
    // diangkat: kemiringan berubah tiba-tiba lebih dari 15 derajat dalam seperempat detik
    if (abs(badan.pitch - pitchLalu) > 15 || abs(badan.roll - rollLalu) > 15) {
      ekspresiSebentar(KAGET, 2000);
      if (millis() - tKomentar > 30000 && !audioJalan) { tKomentar = millis(); ucapkan("Eh, mau dibawa ke mana?"); }
    }
    // diputar tangan: lebih dari 150 derajat per detik selama 1 detik
    if (abs(badan.putar) > 150) { if (!tPutarMulai) tPutarMulai = millis(); }
    else if (tPutarMulai) {
      if (millis() - tPutarMulai > 1000) {
        ekspresiSebentar(PUSING, 3500);
        if (millis() - tKomentar > 30000 && !audioJalan) { tKomentar = millis(); ucapkan("Aduh, pusing!"); }
      }
      tPutarMulai = 0;
    }
  } else tPutarMulai = 0;
  pitchLalu = badan.pitch; rollLalu = badan.roll;

  if (ekspresiSampai && (long)(millis() - ekspresiSampai) > 0) {   // ekspresi sementara habis
    ekspresiSampai = 0;
    if (ekspresi == KAGET || ekspresi == PUSING) ekspresi = NORMAL;
  }
  // ekspresi dasar hanya mengganti NORMAL/LEMAH/NGANTUK, tidak mengganggu DENGAR, PIKIR, KAGET, PUSING
  if (ekspresi == NORMAL || ekspresi == LEMAH || ekspresi == NGANTUK) {
    struct tm j;
    bool malam = jamSiap(j) && (j.tm_hour >= 22 || j.tm_hour < 5);
    bool lemah = hidup && ((badan.flag & 2) || (badan.baterai > 1 && badan.baterai < 6.8));
    bool sepi = millis() - tAktif > 120000 && !audioJalan;
    ekspresi = lemah ? LEMAH : (malam && sepi) ? NGANTUK : NORMAL;
  }
}

// ======================= INFO: JADWAL SHOLAT, BERITA, KURS, WIKIPEDIA, TANGGAL MERAH =======================
// Data diambil tugas jaringan (inti 0) dan dibaca layar (inti 1), jadi penulisan dan pembacaan memakai kunciInfo.
volatile uint8_t mintaInfo = 0;                     // permintaan dari layar, dikerjakan tugas jaringan
volatile bool infoBaru = false;                     // ada data baru: layar digambar ulang
enum { INFO_CUACA = 1, INFO_SHOLAT = 2, INFO_BERITA = 4, INFO_KURS = 8 };
String galatInfo;                                   // pesan galat terakhir untuk ditampilkan (baca/tulis lewat kunciInfo)
void aturGalat(const String& g) { xSemaphoreTake(kunciInfo, portMAX_DELAY); galatInfo = g; xSemaphoreGive(kunciInfo); if (g.length()) Serial.println("[info] " + g); }

// ---- jadwal sholat (api.aladhan.com, metode 20 = Kementerian Agama RI, lokasi = CUACA_LAT/LON) ----
struct JadwalSholat { bool ada = false; uint32_t tanggal = 0; char waktu[7][6]; char hijri[40]; };
JadwalSholat sholat;
const char* NAMA_SHOLAT[7] = {"Imsak", "Subuh", "Terbit", "Dzuhur", "Ashar", "Maghrib", "Isya"};
const char* KUNCI_SHOLAT[7] = {"Imsak", "Fajr", "Sunrise", "Dhuhr", "Asr", "Maghrib", "Isha"};
const char* BULAN_HIJRIAH[12] = {"Muharram", "Safar", "Rabiul Awal", "Rabiul Akhir", "Jumadil Awal", "Jumadil Akhir",
                                 "Rajab", "Syakban", "Ramadan", "Syawal", "Zulkaidah", "Zulhijah"};
uint8_t pengingatSholat = 1;                        // 0 mati, 1 suara, 2 suara lalu adzan dari SD (/adzan.mp3)

uint32_t tanggalHariIni() { struct tm t; return jamSiap(t) ? angkaTanggal(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday) : 0; }

bool ambilSholat() {
  struct tm t;
  if (!jamSiap(t)) return false;
  char url[220];
  snprintf(url, sizeof(url), "https://api.aladhan.com/v1/timings/%02d-%02d-%04d?latitude=%s&longitude=%s&method=20%s%s",
           t.tm_mday, t.tm_mon + 1, t.tm_year + 1900, CUACA_LAT, CUACA_LON, strlen(SHOLAT_KOREKSI) ? "&tune=" : "", SHOLAT_KOREKSI);
  WiFiClientSecure c; c.setInsecure();
  HTTPClient http;
  http.begin(c, url);
  http.setTimeout(10000);
  int kode = http.GET();
  String resp = kode == 200 ? http.getString() : String();
  http.end();
  if (kode != 200) { Serial.printf("[sholat] kode %d\n", kode); aturGalat("Jadwal sholat gagal diambil (kode " + String(kode) + ")"); return false; }
  JsonDocument saring;
  for (int i = 0; i < 7; i++) saring["data"]["timings"][KUNCI_SHOLAT[i]] = true;
  saring["data"]["date"]["hijri"]["day"] = true;
  saring["data"]["date"]["hijri"]["month"]["number"] = true;
  saring["data"]["date"]["hijri"]["year"] = true;
  JsonDocument r;
  if (deserializeJson(r, resp, DeserializationOption::Filter(saring))) return false;
  JadwalSholat j;
  for (int i = 0; i < 7; i++) {
    const char* w = r["data"]["timings"][KUNCI_SHOLAT[i]] | "--:--";
    strlcpy(j.waktu[i], w, 6);                         // "04:12 (WITA)" -> "04:12"
  }
  int bln = r["data"]["date"]["hijri"]["month"]["number"] | 0;
  snprintf(j.hijri, sizeof(j.hijri), "%s %s %s H", (const char*)(r["data"]["date"]["hijri"]["day"] | ""),
           bln >= 1 && bln <= 12 ? BULAN_HIJRIAH[bln - 1] : "", (const char*)(r["data"]["date"]["hijri"]["year"] | ""));
  j.tanggal = angkaTanggal(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
  j.ada = true;
  xSemaphoreTake(kunciInfo, portMAX_DELAY); sholat = j; xSemaphoreGive(kunciInfo);
  Serial.printf("[sholat] Subuh %s, Dzuhur %s, Maghrib %s\n", j.waktu[1], j.waktu[3], j.waktu[5]);
  return true;
}

// Menit sejak tengah malam dari "HH:MM"; -1 kalau tidak valid.
int menitDari(const char* w) { int h, m; return sscanf(w, "%d:%d", &h, &m) == 2 ? h * 60 + m : -1; }

// Sholat berikutnya (1 Subuh, 3 Dzuhur, 4 Ashar, 5 Maghrib, 6 Isya; Imsak dan Terbit dilewati). -1 kalau sudah lewat Isya.
int sholatBerikut(int menitSekarang) {
  const int URUT[5] = {1, 3, 4, 5, 6};
  for (int k : URUT) if (menitDari(sholat.waktu[k]) > menitSekarang) return k;
  return -1;
}

String teksSholat() {
  if (!sholat.ada) return "Jadwal sholat belum ada (butuh WiFi dan jam).";
  String t = String("Jadwal sholat ") + CUACA_KOTA + " hari ini (" + sholat.hijri + "):";
  for (int i = 0; i < 7; i++) t += String("\n") + NAMA_SHOLAT[i] + "  " + sholat.waktu[i];
  return t;
}

// Dipanggil terus dari loop(): ucapkan masuknya waktu sholat, lalu adzan kalau dipilih.
void cekSholat() {
  static unsigned long t = 0;
  static uint32_t sudahTanggal = 0; static int sudahKe = -1;
  if (millis() - t < 1000) return;
  t = millis();
  struct tm j;
  if (!pengingatSholat || !sholat.ada || !jamSiap(j)) return;
  uint32_t hariIni = angkaTanggal(j.tm_year + 1900, j.tm_mon + 1, j.tm_mday);
  if (sholat.tanggal != hariIni) return;
  int sekarang = j.tm_hour * 60 + j.tm_min;
  const int URUT[5] = {1, 3, 4, 5, 6};
  for (int k : URUT) {
    if (menitDari(sholat.waktu[k]) != sekarang || (sudahTanggal == hariIni && sudahKe == k)) continue;
    sudahTanggal = hariIni; sudahKe = k;
    kirimAudio(A_STOP);
    ucapkan(String("Sudah masuk waktu ") + NAMA_SHOLAT[k] + ".");
    if (pengingatSholat == 2) { bunyiTertunda = k == 1 ? 4 : 3; tBunyiTertunda = millis(); }   // adzan setelah kalimat selesai
    setStatus(String("Waktu ") + NAMA_SHOLAT[k] + " " + sholat.waktu[k]);
  }
}

// ---- berita (RSS) ----
struct Berita { String judul, isi, waktu; };
std::vector<Berita> berita;
int sumberBerita = 0; unsigned long tBerita = 0;
const char* SUMBER_BERITA[][2] = {
  {"Antara", "https://www.antaranews.com/rss/terkini.xml"},
  {"CNN Indonesia", "https://www.cnnindonesia.com/nasional/rss"},
  {"Teknologi", "https://www.antaranews.com/rss/tekno.xml"},
};
const int JUMLAH_SUMBER_BERITA = sizeof(SUMBER_BERITA) / sizeof(SUMBER_BERITA[0]);

// Isi tag XML pertama antara a dan b, misalnya <title>...</title>. Kosong kalau tidak ada.
String ambilTag(const char* a, const char* b, const char* tag) {
  size_t n = strlen(tag);
  for (const char* p = a; p && p < b; p++) {
    p = strstr(p, "<"); if (!p || p >= b) break;
    if (strncmp(p + 1, tag, n) == 0 && (p[n + 1] == '>' || p[n + 1] == ' ')) {
      const char* isi = strchr(p, '>'); if (!isi || isi >= b) break;
      isi++;
      char tutup[24]; snprintf(tutup, sizeof(tutup), "</%s>", tag);
      const char* akhir = strstr(isi, tutup); if (!akhir || akhir > b) break;
      String s; s.reserve(akhir - isi);
      for (const char* q = isi; q < akhir; q++) s += *q;
      return s;
    }
  }
  return String();
}

// Buang CDATA, tag HTML, dan ubah entitas (&amp; &#8220; ...) jadi huruf biasa.
String bersihkanHTML(const String& masuk) {
  String t = masuk;
  t.replace("<![CDATA[", ""); t.replace("]]>", "");
  String o; o.reserve(t.length());
  bool dalamTag = false;
  for (unsigned int i = 0; i < t.length(); i++) {
    char c = t[i];
    if (c == '<') { dalamTag = true; continue; }
    if (c == '>') { dalamTag = false; o += ' '; continue; }
    if (dalamTag) continue;
    if (c == '&') {
      int titik = t.indexOf(';', i);
      if (titik > 0 && titik - (int)i <= 8) {
        String e = t.substring(i + 1, titik), ganti;
        int kode = -1;
        if (e.startsWith("#x") || e.startsWith("#X")) kode = strtol(e.c_str() + 2, nullptr, 16);
        else if (e.startsWith("#")) kode = e.substring(1).toInt();
        if (e == "amp") ganti = "&"; else if (e == "quot" || e == "ldquo" || e == "rdquo") ganti = "\"";
        else if (e == "apos" || e == "lsquo" || e == "rsquo") ganti = "'"; else if (e == "lt") ganti = "<";
        else if (e == "gt") ganti = ">"; else if (e == "nbsp") ganti = " "; else if (e == "hellip") ganti = "...";
        else if (e == "ndash" || e == "mdash") ganti = "-";
        else if (kode == 8216 || kode == 8217) ganti = "'"; else if (kode == 8220 || kode == 8221) ganti = "\"";
        else if (kode == 8211 || kode == 8212) ganti = "-"; else if (kode == 8230) ganti = "...";
        else if (kode > 0 && kode < 128) ganti = String((char)kode); else if (kode >= 128) ganti = " ";
        if (ganti.length()) { o += ganti; i = titik; continue; }
      }
    }
    o += (c == '\n' || c == '\r' || c == '\t') ? ' ' : c;
  }
  while (o.indexOf("  ") >= 0) o.replace("  ", " ");
  o.trim();
  return o;
}

bool ambilBerita() {
  WiFiClientSecure c; c.setInsecure();
  HTTPClient http;
  http.begin(c, SUMBER_BERITA[sumberBerita][1]);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setUserAgent("Mozilla/5.0 (Zond robot)");
  http.setTimeout(15000);
  int kode = http.GET();
  PenampungPSRAM jaw;
  if (kode == 200) http.writeToStream(&jaw);
  http.end();
  if (kode != 200 || !jaw.buf) { aturGalat("Berita gagal diambil (kode " + String(kode) + ")"); return false; }
  std::vector<Berita> baru;
  char* p = (char*)jaw.buf;
  while (baru.size() < 20 && (p = strstr(p, "<item")) != nullptr) {
    char* akhir = strstr(p, "</item>"); if (!akhir) break;
    Berita b;
    b.judul = bersihkanHTML(ambilTag(p, akhir, "title"));
    b.isi = bersihkanHTML(ambilTag(p, akhir, "description"));
    String w = ambilTag(p, akhir, "pubDate");              // "Tue, 17 Nov 2026 10:08:33 +0700"
    b.waktu = w.length() > 22 ? w.substring(5, 22) : w;
    if (b.isi.startsWith(b.judul)) b.isi = b.isi.substring(b.judul.length());
    b.isi.trim();
    if (b.judul.length()) baru.push_back(b);
    p = akhir + 7;
  }
  xSemaphoreTake(kunciInfo, portMAX_DELAY); berita.swap(baru); tBerita = millis(); xSemaphoreGive(kunciInfo);
  Serial.printf("[berita] %s: %u berita\n", SUMBER_BERITA[sumberBerita][0], (unsigned)berita.size());
  return true;
}

// ---- kurs rupiah (open.er-api.com, gratis tanpa kunci, diperbarui sekali sehari) ----
const char* KODE_KURS[8] = {"USD", "EUR", "SGD", "MYR", "SAR", "JPY", "CNY", "AUD"};
struct Kurs { bool ada = false; double idr[8]; uint32_t diperbarui = 0; unsigned long t = 0; };
Kurs kurs;

bool ambilKurs() {
  WiFiClientSecure c; c.setInsecure();
  HTTPClient http;
  http.begin(c, "https://open.er-api.com/v6/latest/USD");
  http.setTimeout(10000);
  int kode = http.GET();
  String resp = kode == 200 ? http.getString() : String();
  http.end();
  if (kode != 200) { aturGalat("Kurs gagal diambil (kode " + String(kode) + ")"); return false; }
  JsonDocument saring;
  saring["time_last_update_unix"] = true;
  saring["rates"]["IDR"] = true;
  for (int i = 0; i < 8; i++) saring["rates"][KODE_KURS[i]] = true;
  JsonDocument r;
  if (deserializeJson(r, resp, DeserializationOption::Filter(saring))) return false;
  double idr = r["rates"]["IDR"] | 0.0;
  if (idr <= 0) return false;
  Kurs k;
  for (int i = 0; i < 8; i++) { double v = r["rates"][KODE_KURS[i]] | 0.0; k.idr[i] = v > 0 ? idr / v : 0; }
  k.diperbarui = r["time_last_update_unix"] | 0;
  k.ada = true; k.t = millis();
  xSemaphoreTake(kunciInfo, portMAX_DELAY); kurs = k; xSemaphoreGive(kunciInfo);
  return true;
}

// 18273.2 -> "18.273"
String angkaRibuan(double v) {
  char b[24]; snprintf(b, sizeof(b), "%.0f", v);
  String s = b, o;
  int n = s.length();
  for (int i = 0; i < n; i++) { o += s[i]; if ((n - 1 - i) % 3 == 0 && i < n - 1) o += '.'; }
  return o;
}

String teksKurs() {
  if (!kurs.ada) return "Kurs belum ada.";
  String t = "Kurs rupiah (ExchangeRate-API):";
  for (int i = 0; i < 8; i++) t += String("\n") + (i == 5 ? "100 " : "1 ") + KODE_KURS[i] + " = Rp" + angkaRibuan(kurs.idr[i] * (i == 5 ? 100 : 1));
  return t;
}

// ---- Wikipedia di layar: hasil pencarian terakhir (dari suara, /wiki, atau AI) ----
String wikiJudul, wikiIsi;
volatile int tujuanDengar = 0;                      // 1 = kalimat berikutnya dipakai untuk mencari di Wikipedia

// hasil dari cariWikipedia(): "Judul (Wikipedia id): isi"
void simpanWiki(const String& cari, const String& hasil) {
  xSemaphoreTake(kunciInfo, portMAX_DELAY);
  int a = hasil.indexOf("): ");
  if (!hasil.length()) { wikiJudul = cari; wikiIsi = "Tidak ditemukan di Wikipedia."; }
  else if (a > 0) { wikiJudul = hasil.substring(0, hasil.lastIndexOf(" (", a)); wikiIsi = hasil.substring(a + 3); }
  else { wikiJudul = cari; wikiIsi = hasil; }
  xSemaphoreGive(kunciInfo);
  infoBaru = true;
}

// ---- tanggal merah: SKB 3 Menteri (setneg.go.id). Tambahkan tahun berikutnya setelah SKB-nya terbit. ----
struct Libur { uint32_t tanggal; const char* nama; bool cuti; };
const Libur DAFTAR_LIBUR[] = {
  {20260101, "Tahun Baru 2026 Masehi", false}, {20260116, "Isra Mikraj", false}, {20260216, "Cuti bersama Imlek", true},
  {20260217, "Tahun Baru Imlek", false}, {20260318, "Cuti bersama Nyepi", true}, {20260319, "Hari Suci Nyepi", false},
  {20260320, "Cuti bersama Idulfitri", true}, {20260321, "Idulfitri 1447 H", false}, {20260322, "Idulfitri 1447 H", false},
  {20260323, "Cuti bersama Idulfitri", true}, {20260324, "Cuti bersama Idulfitri", true}, {20260403, "Wafat Yesus Kristus", false},
  {20260405, "Paskah", false}, {20260501, "Hari Buruh", false}, {20260514, "Kenaikan Yesus Kristus", false},
  {20260515, "Cuti bersama Kenaikan", true}, {20260527, "Iduladha 1447 H", false}, {20260528, "Cuti bersama Iduladha", true},
  {20260531, "Waisak", false}, {20260601, "Hari Lahir Pancasila", false}, {20260616, "Tahun Baru Islam 1448 H", false},
  {20260817, "Proklamasi Kemerdekaan", false}, {20260825, "Maulid Nabi", false}, {20261224, "Cuti bersama Natal", true},
  {20261225, "Natal", false},
  {20270101, "Tahun Baru 2027 Masehi", false}, {20270105, "Isra Mikraj", false}, {20270205, "Cuti bersama Imlek", true},
  {20270206, "Tahun Baru Imlek", false}, {20270308, "Hari Suci Nyepi", false}, {20270309, "Cuti bersama Idulfitri", true},
  {20270310, "Idulfitri 1448 H", false}, {20270311, "Idulfitri 1448 H", false}, {20270312, "Cuti bersama Idulfitri", true},
  {20270315, "Cuti bersama Idulfitri", true}, {20270325, "Cuti bersama Wafat Yesus", true}, {20270326, "Wafat Yesus Kristus", false},
  {20270328, "Paskah", false}, {20270501, "Hari Buruh", false}, {20270506, "Kenaikan Yesus Kristus", false},
  {20270517, "Iduladha 1448 H", false}, {20270518, "Cuti bersama Iduladha", true}, {20270519, "Cuti bersama Waisak", true},
  {20270520, "Waisak", false}, {20270601, "Hari Lahir Pancasila", false}, {20270606, "Tahun Baru Islam 1449 H", false},
  {20270815, "Maulid Nabi", false}, {20270817, "Proklamasi Kemerdekaan", false}, {20271224, "Cuti bersama Natal", true},
  {20271225, "Natal", false}, {20271226, "Isra Mikraj", false},
};
const int JUMLAH_LIBUR = sizeof(DAFTAR_LIBUR) / sizeof(DAFTAR_LIBUR[0]);
const int TAHUN_LIBUR_TERAKHIR = 2027;

// 0 = bukan libur, 1 = libur nasional, 2 = cuti bersama
int jenisLibur(uint32_t tgl) {
  for (int i = 0; i < JUMLAH_LIBUR; i++) if (DAFTAR_LIBUR[i].tanggal == tgl) return DAFTAR_LIBUR[i].cuti ? 2 : 1;
  return 0;
}

// ======================= RIWAYAT BATERAI =======================
// Tegangan rata-rata tiap menit disimpan 6 jam terakhir (hilang kalau robot dimatikan). Dari situ dihitung
// isi baterai (%) dan perkiraan sisa waktu. Peringatan ke Telegram saat tinggal 20% dan 10%.
const int SEL_BATERAI = 2;                         // 2 sel 18650 seri
const int RIWAYAT_BAT = 360;                       // 360 menit = 6 jam
uint16_t riwayatBat[RIWAYAT_BAT];                  // tegangan x 100, urut melingkar
int jumlahRiwayat = 0, ujungRiwayat = 0;           // ujung = tempat titik berikutnya
portMUX_TYPE kunciBat = portMUX_INITIALIZER_UNLOCKED;

// Perkiraan isi dari tegangan per sel (Li-ion). 3,2 V per sel = 0% karena badan memutus motor di 6,4 V.
int persenDariVolt(float v) {
  static const float V[] = {3.20, 3.30, 3.40, 3.50, 3.60, 3.70, 3.80, 3.90, 4.00, 4.10, 4.20};
  static const int   P[] = {   0,    2,    6,   13,   25,   40,   55,   68,   79,   90,  100};
  float s = v / SEL_BATERAI;
  if (s <= V[0]) return 0;
  for (int i = 1; i < 11; i++)
    if (s <= V[i]) return P[i - 1] + (int)roundf((s - V[i - 1]) / (V[i] - V[i - 1]) * (P[i] - P[i - 1]));
  return 100;
}

String teksDurasi(int menit) {
  if (menit < 60) return String(menit) + " menit";
  return String(menit / 60) + " jam" + (menit % 60 ? " " + String(menit % 60) + " menit" : String());
}

// Garis lurus lewat isi baterai 30 menit terakhir: seberapa cepat turun, lalu berapa lama lagi sampai 0%.
void hitungSisaBaterai() {
  int n = min(jumlahRiwayat, 30);
  if (n < 10) { sisaMenitBat = -1; return; }
  float sx = 0, sy = 0, sxx = 0, sxy = 0;
  for (int i = 0; i < n; i++) {
    float y = persenDariVolt(riwayatBat[(ujungRiwayat - n + i + RIWAYAT_BAT) % RIWAYAT_BAT] / 100.0f);
    sx += i; sy += y; sxx += (float)i * i; sxy += i * y;
  }
  float turun = (n * sxy - sx * sy) / (n * sxx - sx * sx);   // persen per menit (negatif = berkurang)
  sisaMenitBat = (turun < -0.02f) ? (int)min(1440.0f, persenBat / -turun) : -1;
}

String teksBaterai() {
  if (persenBat < 0) return "Data baterai belum ada (badan belum terhubung).";
  String t = "Baterai " + String((int)persenBat) + "% (" + String(batHalus, 2) + " V).";
  if (sisaMenitBat >= 0) t += " Perkiraan sisa " + teksDurasi(sisaMenitBat) + " kalau pemakaiannya seperti setengah jam terakhir.";
  else t += " Perkiraan sisa waktu muncul setelah sekitar 10 menit menyala.";
  return t;
}

void catatBaterai() {
  static unsigned long t = 0, tMenit = 0, tAwal = 0;
  static float jumlah = 0; static int n = 0;
  static bool sudah20 = false, sudah10 = false;
  if (millis() - t < 1000) return;
  t = millis();
  bool hidup = badan.terakhir && millis() - badan.terakhir < 2000;
  float v = badan.baterai;
  if (!hidup || v < 3) return;                      // badan belum terhubung atau tanpa pengukur baterai
  if (!tAwal) { tAwal = tMenit = millis(); batHalus = v; }
  batHalus += (v - batHalus) * 0.05f;               // dihaluskan: tegangan turun sesaat saat motor jalan
  persenBat = persenDariVolt(batHalus);
  jumlah += v; n++;
  if (millis() - tMenit >= 60000) {
    tMenit = millis();
    uint16_t rata = (uint16_t)roundf(jumlah / n * 100);
    jumlah = 0; n = 0;
    portENTER_CRITICAL(&kunciBat);
    riwayatBat[ujungRiwayat] = rata;
    ujungRiwayat = (ujungRiwayat + 1) % RIWAYAT_BAT;
    if (jumlahRiwayat < RIWAYAT_BAT) jumlahRiwayat++;
    portEXIT_CRITICAL(&kunciBat);
    hitungSisaBaterai();
  }
  if (millis() - tAwal < 120000) return;            // 2 menit pertama: angkanya belum tenang
  if (persenBat >= 35) sudah20 = sudah10 = false;   // baterai sudah diisi
  if ((persenBat <= 20 && !sudah20) || (persenBat <= 10 && !sudah10)) {
    if (persenBat <= 10) sudah10 = true;
    sudah20 = true;
    String p = "Baterai tinggal " + String((int)persenBat) + " persen";
    ucapkan(p + ". Tolong isi ulang ya.");
    kirimTelegram(p + " (" + String(batHalus, 2) + " V)." +
                  (sisaMenitBat >= 0 ? " Perkiraan sisa " + teksDurasi(sisaMenitBat) + "." : String()) + " Segera isi ulang.");
  }
}

// ======================= MODE IKUTI, JAGA & JELAJAH =======================
// IKUTI: jaga jarak 35-70 cm ke benda di depan (sensor ultrasonik tidak bisa membedakan orang dan benda).
// JAGA : robot diam; kalau jarak depan berubah jauh (ada yang lewat/mendekat), kirim foto ke Telegram.
// JELAJAH: maju sampai ada halangan, lalu belok dengan giroskop ke arah acak. Berhenti sendiri kalau waktunya habis,
//          baterai hampir habis, tombol STOP ditekan, atau robot miring/terangkat.
int dasarJaga = -1; unsigned long mulaiJaga = 0;
const int JELAJAH_JARAK = 40;                      // cm: mulai belok sebelum badan menolak maju (20 cm)
const int JELAJAH_LAJU  = 140;
enum { J_MAJU, J_BELOK, J_MUNDUR, J_TUNGGU };
int jTahap = J_MAJU, jArah = 1, jBuntu = 0, jJarakLalu = -1;
unsigned long jSampai = 0, jJarakSama = 0, jMajuMulai = 0;

void gantiMode(Mode m) {
  mode = m;
  kirimBadan("S");
  if (m == M_JAGA) { dasarJaga = -1; mulaiJaga = millis(); setStatus("Mode jaga"); }
  else if (m == M_IKUTI) setStatus("Mode ikuti");
  else if (m == M_JELAJAH) {
    jTahap = J_MAJU; jBuntu = 0; jJarakLalu = -1; jJarakSama = jMajuMulai = millis();
    kirimBadan("P 90 90"); arahMata = 0;
    setStatus("Menjelajah");
  }
  else setStatus("Mode biasa");
}

// menit <= 0: berhenti menjelajah.
void jelajah(int menit) {
  if (menit <= 0) { if (mode == M_JELAJAH) gantiMode(M_BIASA); return; }
  jelajahSampai = millis() + (unsigned long)constrain(menit, 1, 60) * 60000UL;   // diisi dulu sebelum mode berganti
  gantiMode(M_JELAJAH);
  ucapkan("Aku jalan-jalan dulu ya.");
}

void selesaiJelajah(const String& alasan) {
  gantiMode(M_BIASA);
  setStatus("Jelajah selesai: " + alasan);
  ucapkan("Selesai jalan-jalan, " + alasan + ".");
  kirimTelegram("Jelajah selesai: " + alasan + ".");
}

void belokJelajah(bool besar) {
  int sudut = besar ? 130 + esp_random() % 51 : 70 + esp_random() % 61;   // 130-180 atau 70-130 derajat
  if (esp_random() % 100 < 30) jArah = -jArah;           // biasanya searah belokan sebelumnya: tidak bolak-balik di pojok
  belok(jArah * sudut, 0);
  jTahap = J_BELOK; jSampai = millis() + 3500 + sudut * 15;
}

void jalankanJelajah(int d) {
  bool hidup = badan.terakhir && millis() - badan.terakhir < 1500;
  if (!hidup) { selesaiJelajah("badan tidak menjawab"); return; }
  if (badan.flag & 1) { selesaiJelajah("tombol STOP ditekan"); return; }
  if ((badan.flag & 2) || (persenBat >= 0 && persenBat <= 10)) { selesaiJelajah("baterai hampir habis"); return; }
  if (badan.flag & 16) { selesaiJelajah("aku miring atau terangkat"); return; }
  if ((long)(millis() - jelajahSampai) > 0) { selesaiJelajah("waktunya habis"); return; }
  switch (jTahap) {
    case J_MAJU: {
      bool halangan = (d > 0 && d < JELAJAH_JARAK) || (badan.flag & 4);
      // tersangkut: jarak depan tidak berubah 5 detik padahal sedang maju (misalnya roda tertahan kaki kursi)
      if (d > 0 && d < 250) { if (jJarakLalu < 0 || abs(d - jJarakLalu) > 4) { jJarakLalu = d; jJarakSama = millis(); } }
      else { jJarakLalu = -1; jJarakSama = millis(); }
      bool macet = millis() - jJarakSama > 5000;
      if (halangan || macet) {
        kirimBadan("S");
        jBuntu = (millis() - jMajuMulai < 1500) ? jBuntu + 1 : 0;   // baru selesai belok sudah ketemu halangan lagi
        if (macet || jBuntu >= 2) {                     // mundur dulu, lalu belok besar
          jBuntu = 0;
          gerak(-130, -130, 600);
          jTahap = J_MUNDUR; jSampai = millis() + 800;
        } else belokJelajah(false);
        return;
      }
      gerak(JELAJAH_LAJU, JELAJAH_LAJU, 400);          // dikirim ulang tiap 150 ms; berhenti sendiri kalau otak macet
      tAktif = millis();                                // sedang sibuk: jangan pasang wajah mengantuk
      break;
    }
    case J_MUNDUR:
      if ((long)(millis() - jSampai) > 0) belokJelajah(true);
      break;
    case J_BELOK:
      if (!putarSelesai && (long)(millis() - jSampai) < 0) break;
      if (putarSelesai == 3) {                          // kode badan lama tanpa giroskop: belok memakai waktu
        gerak(jArah * 160, -jArah * 160, 450);
        jTahap = J_TUNGGU; jSampai = millis() + 600;
        break;
      }
      jTahap = J_MAJU; jJarakLalu = -1; jJarakSama = jMajuMulai = millis();
      break;
    case J_TUNGGU:
      if ((long)(millis() - jSampai) > 0) { jTahap = J_MAJU; jJarakLalu = -1; jJarakSama = jMajuMulai = millis(); }
      break;
  }
}

void jalankanMode() {
  static unsigned long t = 0; static int beda = 0; static unsigned long tAlarm = 0;
  if (millis() - t < 150) return;
  t = millis();
  int d = badan.depan;
  if (mode == M_IKUTI) {
    if (d < 0 || d > 150) return;                       // tidak ada yang diikuti: diam
    if (d > 70) gerak(110, 110, 300);
    else if (d < 35) gerak(-100, -100, 300);
  } else if (mode == M_JELAJAH) {
    jalankanJelajah(d);
  } else if (mode == M_JAGA) {
    if (millis() - mulaiJaga < 4000) return;            // beri waktu pemilik menjauh
    if (dasarJaga < 0) { dasarJaga = (d < 0) ? 400 : d; return; }
    int now = (d < 0) ? 400 : d;
    beda = (abs(now - dasarJaga) > 25) ? beda + 1 : 0;
    if (beda >= 2 && millis() - tAlarm > 30000) {       // jeda 30 detik antar-alarm
      tAlarm = millis(); beda = 0;
      ekspresiSebentar(KAGET, 1500); ucapkan("Siapa itu?");
      alarmJaga = true;
    }
  }
}

// ======================= ALASAN RESTART =======================
// Dikirim bersama pesan "menyala" supaya restart yang tidak disengaja langsung ketahuan penyebabnya.
String alasanRestart() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: case ESP_RST_SW: case ESP_RST_USB: return "";   // dinyalakan, diupload, atau tombol RST
    case ESP_RST_EXT:      return "";
    case ESP_RST_BROWNOUT: return "\n(Restart: tegangan 5 V turun / brownout. Cek kabel 5 V dan GND papan S3.)";
    case ESP_RST_PANIC:    return "\n(Restart: program crash. Lihat Serial Monitor S3 untuk detailnya.)";
    case ESP_RST_INT_WDT: case ESP_RST_TASK_WDT: case ESP_RST_WDT: return "\n(Restart: program macet / watchdog.)";
    default:               return "\n(Restart: kode " + String((int)esp_reset_reason()) + ")";
  }
}

// ======================= KAMERA =======================
uint8_t* fotoBuf = nullptr; size_t fotoLen = 0, fotoPos = 0;

String alasanFoto;                  // kenapa foto terakhir gagal (dikirim ke Telegram)

bool ambilFoto(const char* res) {
  alasanFoto = "";
  if (alamatKamera.isEmpty()) {
    IPAddress ip = MDNS.queryHost("robotcam", 2000);
    if (ip == IPAddress()) { alasanFoto = "robotcam.local tidak ditemukan. Isi KAMERA_ALAMAT dengan IP kamera."; return false; }
    alamatKamera = ip.toString();
  }
  HTTPClient http;
  http.begin("http://" + alamatKamera + "/capture?res=" + res);
  http.setTimeout(8000);
  int code = http.GET();
  if (code != 200) {
    alasanFoto = code < 0 ? "tidak bisa tersambung ke " + alamatKamera + " (" + http.errorToString(code) + "). Kamera menyala, satu WiFi, dan tidak ada tab /stream yang terbuka?"
                          : "kamera menjawab kode " + String(code);
    http.end(); return false;
  }
  int len = http.getSize();
  if (len <= 0 || len > 400000) { alasanFoto = "ukuran foto aneh: " + String(len); http.end(); return false; }
  if (fotoBuf) free(fotoBuf);
  fotoBuf = (uint8_t*)ps_malloc(len);
  if (!fotoBuf) { alasanFoto = "PSRAM tidak ada. Tools > PSRAM harus OPI PSRAM, lalu upload ulang."; http.end(); return false; }
  WiFiClient* s = http.getStreamPtr();
  size_t got = 0; unsigned long t0 = millis();
  while (got < (size_t)len && millis() - t0 < 8000) {
    size_t n = s->available();
    if (n) got += s->readBytes(fotoBuf + got, min(n, (size_t)len - got)); else delay(2);
  }
  http.end();
  fotoLen = got; fotoPos = 0;
  if (got != (size_t)len) alasanFoto = "foto terputus: " + String(got) + " dari " + String(len) + " byte. Sinyal WiFi kamera lemah?";
  Serial.println(alasanFoto.length() ? "[kamera] gagal: " + alasanFoto : "[kamera] foto " + String(len) + " byte");
  return got == (size_t)len;
}

// Library Telegram 1.3.0 hanya memakai jalur "per potongan" kalau fungsi per-byte diisi nullptr
// (logikanya terbalik di library). Urutan panggilannya: fotoMasihAda(), fotoBufferBerikut(), fotoPanjangBerikut().
size_t potAwal = 0, potLen = 0;
bool fotoMasihAda() { fotoPos = potAwal + potLen; potAwal = fotoPos; potLen = min((size_t)1024, fotoLen - fotoPos); return potLen > 0; }
byte* fotoBufferBerikut() { return fotoBuf + potAwal; }
int fotoPanjangBerikut() { return potLen; }

bool kirimFotoTelegram(const String& chat) {
  if (!fotoBuf || !fotoLen) return false;
  fotoPos = 0; potAwal = 0; potLen = 0;
  String r = bot.sendPhotoByBinary(chat, "image/jpeg", fotoLen, fotoMasihAda, nullptr, fotoBufferBerikut, fotoPanjangBerikut);
  // Library menunggu balasan dengan loop tanpa jeda (bawaan 1,5 detik). Jangan dinaikkan: di atas 5 detik
  // watchdog me-restart S3. Balasan kosong = Telegram belum sempat menjawab, fotonya biasanya tetap sampai.
  if (r.length() == 0) { Serial.println("[telegram] foto terkirim, balasan Telegram belum datang"); return true; }
  if (r.indexOf("\"ok\":true") < 0) Serial.println("[telegram] kirim foto gagal: " + r.substring(0, 200));
  return r.indexOf("\"ok\":true") >= 0;
}

// ======================= MIKROFON + SUARA KE TEKS =======================
const int MIC_RATE = 16000;
uint8_t* wavBuf = nullptr; size_t wavLen = 0;

void tulisHeaderWav(uint8_t* h, uint32_t dataLen) {
  uint32_t v;
  memcpy(h, "RIFF", 4); v = dataLen + 36; memcpy(h + 4, &v, 4); memcpy(h + 8, "WAVEfmt ", 8);
  v = 16; memcpy(h + 16, &v, 4); uint16_t w = 1; memcpy(h + 20, &w, 2); w = 1; memcpy(h + 22, &w, 2);
  v = MIC_RATE; memcpy(h + 24, &v, 4); v = MIC_RATE * 2; memcpy(h + 28, &v, 4);
  w = 2; memcpy(h + 32, &w, 2); w = 16; memcpy(h + 34, &w, 2); memcpy(h + 36, "data", 4); memcpy(h + 40, &dataLen, 4);
}

// ---- Mendengar terus: potong tiap kalimat berdasarkan keras-pelannya suara ----
// Satu kalimat (maksimal 6 detik) disimpan di wavBuf, lalu tugas jaringan mengubahnya jadi teks.
// Robot tidak mendengar saat speaker berbunyi, supaya tidak menanggapi suaranya sendiri.
const int FRAME = 320;                     // 20 milidetik pada 16 kHz
const int PRAROL = 15;                     // simpan 0,3 detik sebelum suara mulai, supaya awal kata tidak terpotong
const int MAKS_DETIK = 6;
const int AMBANG_MIN = 300;                // suara di bawah ini selalu dianggap diam
volatile bool mintaDengar = false;         // tombol BICARA ditekan
volatile bool dengarTerus = false;         // true = mendengar terus dan menanggapi kalau namanya disebut.
                                           // Boros: setiap suara di ruangan dikirim ke Groq. Diatur dari panel web
                                           // atau /dengar di Telegram, tersimpan di memori. Bawaan: mati (tekan BICARA).

void aturDengarTerus(bool nyala) {
  dengarTerus = nyala;
  pref.begin("robot", false); pref.putUChar("dengar", nyala ? 1 : 0); pref.end();
  setStatus(nyala ? String("Dengar terus: sebut \"") + NAMA_ROBOT + "\"" : "Tekan BICARA untuk bicara");
}

volatile bool suaraSiap = false;           // satu kalimat sudah ada di wavBuf
volatile bool suaraButuhNama = true;       // kalimat itu harus menyebut nama robot supaya ditanggapi
volatile unsigned long jendelaSampai = 0;  // sampai kapan boleh bicara tanpa menyebut nama

void tugasDengar(void*) {
  static int32_t blok[FRAME * 2];
  static int16_t cincin[PRAROL * FRAME];
  const size_t maks = MIC_RATE * MAKS_DETIK;
  wavBuf = (uint8_t*)ps_malloc(44 + maks * 2);
  if (!wavBuf) { Serial.println("[dengar] memori PSRAM tidak cukup"); vTaskDelete(NULL); }
  int16_t* out = (int16_t*)(wavBuf + 44);
  float hpX = 0, hpY = 0, lantai = 200;     // lantai = perkiraan bising ruangan
  int posCincin = 0, atas = 0, bawah = 0, bersuara = 0;
  bool merekam = false; size_t n = 0;
  unsigned long tSibuk = 0;
  for (;;) {
    size_t k = mic.readBytes((char*)blok, sizeof(blok)) / 8;
    if (k == 0) { vTaskDelay(1); continue; }
    if (k > FRAME) k = FRAME;
    int16_t f[FRAME]; double jum = 0;
    for (size_t i = 0; i < k; i++) {
      float x = blok[2 * i] >> 14;                       // jalur kiri, 24 bit -> 16 bit dengan sedikit penguatan
      float y = x - hpX + 0.995f * hpY;                  // buang geseran nol dan dengung rendah
      hpX = x; hpY = y;
      f[i] = (int16_t)constrain((int)y, -32768, 32767); jum += (double)y * y;
    }
    int rms = sqrt(jum / k);

    if (suaraSiap || audioJalan || sedangBicara) { tSibuk = millis(); merekam = false; atas = 0; continue; }
    if (millis() - tSibuk < 1000) continue;              // tunggu gema speaker hilang
    if (mintaDengar) {
      mintaDengar = false; jendelaSampai = millis() + 6000;
      ekspresi = DENGAR; setStatus("Mendengarkan...");
    }
    bool bolehDengar = dengarTerus || millis() < jendelaSampai;
    if (!merekam) {
      if (jendelaSampai && millis() > jendelaSampai) {   // waktu menunggu perintah habis
        jendelaSampai = 0;
        if (ekspresi == DENGAR) { ekspresi = NORMAL; setStatus(""); }
      }
      memcpy(cincin + (posCincin % PRAROL) * FRAME, f, k * 2); posCincin++;
      int ambang = max(AMBANG_MIN, (int)(lantai * 3));
      if (rms > ambang) atas++;
      else { atas = 0; lantai += (rms - lantai) * 0.02f; }
      if (atas >= 3 && bolehDengar) {                   // 60 milidetik suara keras: kalimat dimulai
        n = 0;
        int ada = min(posCincin, PRAROL);
        for (int r = posCincin - ada; r < posCincin; r++) { memcpy(out + n, cincin + (r % PRAROL) * FRAME, FRAME * 2); n += FRAME; }
        merekam = true; bersuara = atas; bawah = 0;
      }
    } else {
      if (n + k <= maks) { memcpy(out + n, f, k * 2); n += k; }
      if (rms > max(AMBANG_MIN / 2, (int)(lantai * 2))) { bersuara++; bawah = 0; } else bawah++;
      if (bawah >= 35 || n + FRAME > maks) {             // 0,7 detik diam, atau sudah 6 detik: kalimat selesai
        merekam = false; atas = 0;
        if (bersuara >= 15) {                            // minimal 0,3 detik bersuara, selain itu cuma bunyi singkat
          wavLen = 44 + n * 2; tulisHeaderWav(wavBuf, n * 2);
          suaraButuhNama = millis() > jendelaSampai;
          suaraSiap = true;
        }
      }
    }
  }
}

// Cari nama robot di tiga kata pertama. Whisper kadang menulis nama sedikit berbeda (Zon, Zone, Son), jadi dicocokkan
// dengan toleransi satu huruf. sisa = kalimat tanpa nama dan kata sapaan; kosong kalau hanya memanggil nama.
int jarakHuruf(const String& a, const String& b) {
  int m = a.length(), n = b.length(); if (abs(m - n) > 1) return 9;
  int d[12][12]; if (m > 10 || n > 10) return 9;
  for (int i = 0; i <= m; i++) d[i][0] = i;
  for (int j = 0; j <= n; j++) d[0][j] = j;
  for (int i = 1; i <= m; i++) for (int j = 1; j <= n; j++)
    d[i][j] = min(min(d[i - 1][j] + 1, d[i][j - 1] + 1), d[i - 1][j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1));
  return d[m][n];
}
String miripkan(String w) { w.replace("z", "s"); return w; }   // z dan s sering tertukar

bool cariNama(const String& kalimat, String& sisa) {
  String nama = NAMA_ROBOT; nama.toLowerCase(); nama = miripkan(nama);
  String t = kalimat; t.toLowerCase();
  for (unsigned i = 0; i < t.length(); i++) if (!isalnum((unsigned char)t[i])) t[i] = ' ';
  bool ketemu = false; int kata = 0; sisa = "";
  const char* sapaan[] = {"hai", "hei", "hey", "halo", "hallo", "oke", "ok", "eh", "ya", "hi"};
  int i = 0;
  while (i < (int)t.length()) {
    while (i < (int)t.length() && t[i] == ' ') i++;
    int j = i; while (j < (int)t.length() && t[j] != ' ') j++;
    if (j > i) {
      String w = t.substring(i, j); kata++;
      bool iniNama = !ketemu && kata <= 3 && jarakHuruf(miripkan(w), nama) <= 1;   // nama biasanya di awal kalimat
      bool iniSapaan = false;
      for (const char* s : sapaan) if (w == s) iniSapaan = true;
      if (iniNama) ketemu = true;
      else if (!(iniSapaan && sisa.length() == 0)) sisa += (sisa.length() ? " " : "") + w;
    }
    i = j;
  }
  return ketemu;
}

// Whisper kadang "mengarang" kalimat dari bising: mengulang petunjuknya, atau kalimat penutup video.
bool karanganWhisper(String t) {
  t.toLowerCase();
  const char* karangan[] = {"percakapan dengan robot", "terima kasih telah menonton", "terima kasih sudah menonton",
                            "jangan lupa subscribe", "subtitle"};
  for (const char* k : karangan) if (t.indexOf(k) >= 0) return true;
  return false;
}

// Batasi kiriman suara ke Groq saat robot mendengar terus (obrolan orang lain, TV), supaya jatah harian tidak habis.
bool izinSuaraKeTeks() {
  static unsigned long awalMenit = 0; static int jumlah = 0;
  if (millis() - awalMenit > 60000) { awalMenit = millis(); jumlah = 0; }
  return ++jumlah <= 10;
}

String suaraKeTeks() {
  if (strlen(GROQ_API_KEY) == 0) return "";
  const char* batas = "----robotbatas";
  String awal = String("--") + batas + "\r\nContent-Disposition: form-data; name=\"model\"\r\n\r\nwhisper-large-v3-turbo\r\n" +
                "--" + batas + "\r\nContent-Disposition: form-data; name=\"language\"\r\n\r\nid\r\n" +
                "--" + batas + "\r\nContent-Disposition: form-data; name=\"response_format\"\r\n\r\ntext\r\n" +
                "--" + batas + "\r\nContent-Disposition: form-data; name=\"prompt\"\r\n\r\nPercakapan dengan robot bernama " + NAMA_ROBOT + ".\r\n" +
                "--" + batas + "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"suara.wav\"\r\nContent-Type: audio/wav\r\n\r\n";
  String akhir = String("\r\n--") + batas + "--\r\n";
  size_t total = awal.length() + wavLen + akhir.length();
  uint8_t* body = (uint8_t*)ps_malloc(total);
  if (!body) return "";
  memcpy(body, awal.c_str(), awal.length());
  memcpy(body + awal.length(), wavBuf, wavLen);
  memcpy(body + awal.length() + wavLen, akhir.c_str(), akhir.length());
  WiFiClientSecure c; c.setInsecure();
  HTTPClient http;
  http.begin(c, "https://api.groq.com/openai/v1/audio/transcriptions");
  http.addHeader("Authorization", String("Bearer ") + GROQ_API_KEY);
  http.addHeader("Content-Type", String("multipart/form-data; boundary=") + batas);
  http.setTimeout(20000);
  int code = http.sendRequest("POST", body, total);
  String hasil = (code == 200) ? http.getString() : "";
  if (code != 200) Serial.printf("[groq] gagal, kode %d: %s\n", code, http.getString().c_str());
  http.end(); free(body);
  hasil.trim();
  return hasil;
}

// ======================= AI (Gemini, cadangan Groq) =======================
const int RIWAYAT_CHAT = 20;                       // 10 tanya-jawab terakhir diingat (hilang kalau robot dimatikan)
String riwayatPeran[RIWAYAT_CHAT], riwayatIsi[RIWAYAT_CHAT]; int riwayatN = 0;

void ingat(const String& peran, const String& isi) {
  if (riwayatN == RIWAYAT_CHAT) {
    for (int i = 0; i < RIWAYAT_CHAT - 2; i++) { riwayatPeran[i] = riwayatPeran[i + 2]; riwayatIsi[i] = riwayatIsi[i + 2]; }
    riwayatN = RIWAYAT_CHAT - 2;
  }
  riwayatPeran[riwayatN] = peran;
  riwayatIsi[riwayatN] = isi.length() > 1500 ? isi.substring(0, 1500) : isi;   // batasi supaya RAM tidak habis
  riwayatN++;
}

String promptSistem() {
  return String("Kamu adalah ") + NAMA_ROBOT + ", robot beroda empat milik " + NAMA_PEMILIK + " di Lombok. "
    "Namamu " + String(NAMA_ROBOT) + "; pemilikmu memanggil namamu sebelum memberi perintah, jadi abaikan panggilan itu dan tanggapi permintaannya. "
    "Bicara Bahasa Indonesia santai dan ramah. Untuk obrolan biasa dan perintah, jawab singkat (1-2 kalimat). "
    "Kalau diminta menjelaskan, bercerita, memberi langkah, resep, atau saran, atau ditanya kenapa/bagaimana, jawab lengkap "
    "dan pakai pengetahuanmu sepenuhnya, sampai sekitar 6 kalimat yang padat. Jawabanmu diucapkan lewat speaker, jadi "
    "tulis kalimat biasa tanpa daftar bernomor, simbol, atau markdown. "
    "Jujur: kalau tidak tahu, bilang tidak tahu. Pengetahuanmu punya batas waktu; untuk hal yang bisa berubah "
    "(harga, jabatan, berita), bilang bahwa infomu mungkin sudah lama. Kamu punya badan yang bisa bergerak. "
    "Jawab HANYA dengan satu objek JSON tanpa teks lain, formatnya: "
    "{\"bicara\":\"kalimatmu\",\"aksi\":[...]} . Aksi yang tersedia: "
    "{\"m\":[kiri,kanan,ms]} maju atau mundur, kiri/kanan -200..200, ms 100..2000 (maju [150,150,800], mundur [-150,-150,800]); "
    "{\"belok\":derajat} berputar di tempat dengan tepat, positif = kanan, negatif = kiri (90 = seperempat putaran, 180 = balik arah); "
    "{\"kepala\":[pan,tilt]} arah kepala dalam derajat, 90,90 = lurus ke depan, pan 30=kanan 150=kiri; "
    "{\"foto\":1} ambil foto dari kameramu, HANYA kalau diminta melihat sesuatu; "
    "{\"cari\":\"kata kunci\"} cari di Wikipedia kalau ditanya fakta (tokoh, tempat, sejarah, sains, istilah) yang tidak "
    "kamu yakini; kata kunci singkat, isi bicara dengan kalimat pendek seperti \"Sebentar, aku cari dulu.\"; jangan dipakai "
    "untuk obrolan, perintah gerak, cuaca, atau berita hari ini; "
    "{\"diam\":1} hentikan musik; {\"mode\":\"ikuti\"} ikuti benda di depanmu (misalnya kaki pemilik), "
    "{\"mode\":\"jaga\"} diam dan berjaga, {\"mode\":\"jelajah\"} jalan-jalan sendiri keliling ruangan selama 5 menit, "
    "{\"mode\":\"biasa\"} kembali normal (juga menghentikan jelajah); "
    "{\"ingatkan\":[\"HH:MM\",\"pesan\",\"sekali\" atau \"harian\"]} pasang alarm/pengingat (jam 24 jam; untuk \"10 menit lagi\" hitung sendiri jamnya dari waktu sekarang). "
    + String(fiturKamera ? "{\"sekeliling\":1} menoleh kiri, depan, kanan lalu menceritakan sekitarmu; {\"suit\":1} main suit (batu-gunting-kertas) lewat kamera. " : "") +
    "Kosongkan aksi kalau tidak perlu. "
    "Badanmu otomatis menolak maju kalau ada halangan dekat, jadi tidak perlu khawatir menabrak. "
    "Data sensor terbaru: " + ringkasSensor() + ". " + teksCuaca(true) +
    (sholat.ada ? String(" Jadwal sholat hari ini: Subuh ") + sholat.waktu[1] + ", Dzuhur " + sholat.waktu[3] + ", Ashar " +
                  sholat.waktu[4] + ", Maghrib " + sholat.waktu[5] + ", Isya " + sholat.waktu[6] + "." : String()) +
    (teksJam().length() ? String(" Sekarang ") + teksTanggal() + ", jam " + teksJam() + "." : String());
}

// Buang bagian "berpikir" <think>...</think> yang kadang ditulis model cadangan, lalu rapikan.
String bersihkan(String t) {
  int a = t.indexOf("<think>"), b = t.indexOf("</think>");
  if (a >= 0 && b > a) t.remove(a, b + 8 - a);
  t.trim();
  return t;
}

// Beberapa foto boleh dikirim sekaligus dalam satu String, dipisah koma (koma tidak pernah muncul di base64).
// Ambil foto ke-n (mulai 0); kosong kalau tidak ada.
String fotoKe(const String& semua, int n) {
  int a = 0;
  for (int i = 0; i < n; i++) { a = semua.indexOf(',', a); if (a < 0) return String(); a++; }
  int b = semua.indexOf(',', a);
  return semua.substring(a, b < 0 ? semua.length() : b);
}

// Gemini atau Gemma lewat Google AI Studio. kode: 200 = berhasil, 429 = jatah habis atau terlalu sering.
// gemma = true: model Gemma belum tentu mendukung mode JSON, jadi JSON hanya diminta lewat aturan di prompt.
String tanyaGemini(const char* model, bool gemma, const String& pertanyaan, const String& fotoBase64, int& kode) {
  JsonDocument doc;
  doc["system_instruction"]["parts"][0]["text"] = promptSistem();
  JsonArray isi = doc["contents"].to<JsonArray>();
  for (int i = 0; i < riwayatN; i++) {
    JsonObject m = isi.add<JsonObject>();
    m["role"] = riwayatPeran[i] == "user" ? "user" : "model";
    m["parts"][0]["text"] = riwayatIsi[i];
  }
  JsonObject u = isi.add<JsonObject>();
  u["role"] = "user";
  JsonArray bagian = u["parts"].to<JsonArray>();
  for (int n = 0; fotoBase64.length(); n++) {
    String f = fotoKe(fotoBase64, n); if (!f.length()) break;
    JsonObject img = bagian.add<JsonObject>();
    img["inline_data"]["mime_type"] = "image/jpeg"; img["inline_data"]["data"] = f;
  }
  bagian.add<JsonObject>()["text"] = pertanyaan;
  if (gemma) doc["generationConfig"]["thinkingConfig"]["thinkingLevel"] = "minimal";   // tanpa berpikir panjang, supaya cepat
  else doc["generationConfig"]["responseMimeType"] = "application/json";              // minta jawaban JSON murni
  String body; serializeJson(doc, body);
  doc.clear();

  WiFiClientSecure c; c.setInsecure();
  HTTPClient http;
  http.begin(c, String("https://generativelanguage.googleapis.com/v1beta/models/") + model + ":generateContent");
  http.addHeader("content-type", "application/json");
  http.addHeader("x-goog-api-key", GEMINI_API_KEY);
  http.setTimeout(30000);
  kode = http.POST(body);
  String resp = http.getString();
  http.end();
  if (kode != 200) { Serial.printf("[%s] kode %d: %s\n", model, kode, resp.c_str()); return ""; }
  JsonDocument r;
  if (deserializeJson(r, resp)) { kode = -2; return ""; }
  String teks;
  for (JsonObject p : r["candidates"][0]["content"]["parts"].as<JsonArray>())
    if (!(p["thought"] | false)) teks += p["text"] | "";
  if (teks.length() == 0) { Serial.printf("[%s] jawaban kosong: %s\n", model, resp.c_str()); kode = -3; }
  return bersihkan(teks);
}

// Groq (cadangan), format OpenAI.
String tanyaGroq(const String& pertanyaan, const String& fotoBase64, int& kode) {
  JsonDocument doc;
  doc["model"] = GROQ_MODEL;
  doc["max_completion_tokens"] = 1024;
  JsonArray msgs = doc["messages"].to<JsonArray>();
  JsonObject sis = msgs.add<JsonObject>(); sis["role"] = "system"; sis["content"] = promptSistem();
  for (int i = 0; i < riwayatN; i++) { JsonObject m = msgs.add<JsonObject>(); m["role"] = riwayatPeran[i]; m["content"] = riwayatIsi[i]; }
  JsonObject u = msgs.add<JsonObject>();
  u["role"] = "user";
  if (fotoBase64.length()) {
    JsonArray isi = u["content"].to<JsonArray>();
    JsonObject txt = isi.add<JsonObject>(); txt["type"] = "text"; txt["text"] = pertanyaan;
    for (int n = 0; ; n++) {
      String f = fotoKe(fotoBase64, n); if (!f.length()) break;
      JsonObject img = isi.add<JsonObject>(); img["type"] = "image_url";
      img["image_url"]["url"] = "data:image/jpeg;base64," + f;
    }
  } else {
    u["content"] = pertanyaan;
  }
  String body; serializeJson(doc, body);
  doc.clear();

  WiFiClientSecure c; c.setInsecure();
  HTTPClient http;
  http.begin(c, "https://api.groq.com/openai/v1/chat/completions");
  http.addHeader("content-type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + GROQ_API_KEY);
  http.setTimeout(30000);
  kode = http.POST(body);
  String resp = http.getString();
  http.end();
  if (kode != 200) { Serial.printf("[groq] kode %d: %s\n", kode, resp.c_str()); return ""; }
  JsonDocument r;
  if (deserializeJson(r, resp)) { kode = -2; return ""; }
  return bersihkan(r["choices"][0]["message"]["content"] | "");
}

// Claude Haiku (Anthropic, berbayar). Format Messages API.
String tanyaClaude(const String& pertanyaan, const String& fotoBase64, int& kode) {
  JsonDocument doc;
  doc["model"] = HAIKU_MODEL;
  doc["max_tokens"] = 1024;
  doc["system"] = promptSistem();
  JsonArray msgs = doc["messages"].to<JsonArray>();
  for (int i = 0; i < riwayatN; i++) { JsonObject m = msgs.add<JsonObject>(); m["role"] = riwayatPeran[i]; m["content"] = riwayatIsi[i]; }
  JsonObject u = msgs.add<JsonObject>();
  u["role"] = "user";
  JsonArray isi = u["content"].to<JsonArray>();
  for (int n = 0; fotoBase64.length(); n++) {
    String f = fotoKe(fotoBase64, n); if (!f.length()) break;
    JsonObject img = isi.add<JsonObject>(); img["type"] = "image";
    img["source"]["type"] = "base64"; img["source"]["media_type"] = "image/jpeg"; img["source"]["data"] = f;
  }
  JsonObject txt = isi.add<JsonObject>(); txt["type"] = "text"; txt["text"] = pertanyaan;
  String body; serializeJson(doc, body);
  doc.clear();

  WiFiClientSecure c; c.setInsecure();
  HTTPClient http;
  http.begin(c, "https://api.anthropic.com/v1/messages");
  http.addHeader("content-type", "application/json");
  http.addHeader("x-api-key", ANTHROPIC_API_KEY);
  http.addHeader("anthropic-version", "2023-06-01");
  http.setTimeout(30000);
  kode = http.POST(body);
  String resp = http.getString();
  http.end();
  if (kode != 200) { Serial.printf("[haiku] kode %d: %s\n", kode, resp.c_str()); return ""; }
  JsonDocument r;
  if (deserializeJson(r, resp)) { kode = -2; return ""; }
  String teks;
  for (JsonObject p : r["content"].as<JsonArray>()) if (String(p["type"] | "") == "text") teks += p["text"] | "";
  return bersihkan(teks);
}

// Pilihan otak AI (dari panel web atau /ai di Telegram), disimpan di memori S3.
// OTOMATIS = urutan gratis: Gemini, Gemma, Groq. Pilihan lain dicoba duluan; kalau gagal, lanjut ke urutan gratis.
enum PilihanAI { AI_OTOMATIS, AI_GEMINI, AI_GEMMA, AI_GROQ, AI_HAIKU, AI_JUMLAH };
const char* namaAI[AI_JUMLAH] = {"otomatis", "gemini", "gemma", "groq", "haiku"};
volatile int pilihanAI = AI_OTOMATIS;
String aiTerakhir;                         // model yang terakhir benar-benar menjawab

bool aiTersedia(int p) {
  switch (p) {
    case AI_GEMINI: return strlen(GEMINI_API_KEY) > 0;
    case AI_GEMMA:  return strlen(GEMINI_API_KEY) > 0 && strlen(GEMMA_MODEL) > 0;
    case AI_GROQ:   return strlen(GROQ_API_KEY) > 0;
    case AI_HAIKU:  return strlen(ANTHROPIC_API_KEY) > 0;
    default:        return strlen(GEMINI_API_KEY) > 0 || strlen(GROQ_API_KEY) > 0 || strlen(ANTHROPIC_API_KEY) > 0;
  }
}

bool pilihAI(const String& nama) {
  for (int i = 0; i < AI_JUMLAH; i++) if (nama.equalsIgnoreCase(namaAI[i]) && aiTersedia(i)) {
    pilihanAI = i;
    pref.begin("robot", false); pref.putUChar("ai", i); pref.end();
    return true;
  }
  return false;
}

String tanyaSatu(int p, const String& q, const String& foto, int& kode) {
  kode = 0;
  if (!aiTersedia(p)) return "";
  String t;
  switch (p) {
    case AI_GEMINI: t = tanyaGemini(GEMINI_MODEL, false, q, foto, kode); break;
    case AI_GEMMA:  t = tanyaGemini(GEMMA_MODEL, true, q, foto, kode); break;
    case AI_GROQ:   t = tanyaGroq(q, foto, kode); break;
    case AI_HAIKU:  t = tanyaClaude(q, foto, kode); break;
  }
  if (kode == 200 && t.length()) aiTerakhir = namaAI[p];
  else Serial.printf("[ai] %s gagal (kode %d)\n", namaAI[p], kode);
  return t;
}

// fotoBase64 kosong = hanya teks.
String tanyaAI(const String& pertanyaan, const String& fotoBase64 = "", bool simpan = true) {
  if (!aiTersedia(AI_OTOMATIS)) return "{\"bicara\":\"API key AI belum diisi di kode.\",\"aksi\":[]}";
  int kode = 0; String teks;
  int urutan[] = {pilihanAI, AI_GEMINI, AI_GEMMA, AI_GROQ};
  for (int p : urutan) {
    if (p == AI_OTOMATIS) continue;
    teks = tanyaSatu(p, pertanyaan, fotoBase64, kode);
    if (kode == 200 && teks.length()) break;
  }
  if (kode != 200 || teks.length() == 0) {
    if (kode == 429) return "{\"bicara\":\"Jatah AI gratis sedang habis. Coba lagi nanti.\",\"aksi\":[]}";
    return "{\"bicara\":\"Maaf, aku gagal menghubungi otak AI (kode " + String(kode) + ").\",\"aksi\":[]}";
  }
  if (simpan) {                              // permainan (suit) tidak perlu masuk riwayat obrolan
    ingat("user", pertanyaan);               // foto tidak disimpan di riwayat, terlalu besar
    ingat("assistant", teks);
  }
  return teks;
}

// ======================= CARI DI WIKIPEDIA (gratis, tanpa kunci) =======================
// Dipakai AI lewat aksi {"cari":"kata kunci"}: ringkasan artikel teratas dikirim balik ke AI untuk menjawab.
String kodeUrl(const String& t) {
  String o; char b[4];
  for (unsigned int i = 0; i < t.length(); i++) {
    unsigned char c = t[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') o += (char)c;
    else { snprintf(b, sizeof(b), "%%%02X", c); o += b; }
  }
  return o;
}

String cariWikipediaDi(const char* bahasa, const String& kata) {
  WiFiClientSecure c; c.setInsecure();
  HTTPClient http;
  String url = String("https://") + bahasa + ".wikipedia.org/w/api.php?action=query&format=json&formatversion=2"
               "&generator=search&gsrlimit=1&prop=extracts&exintro=1&explaintext=1&exchars=1200&redirects=1&gsrsearch=" + kodeUrl(kata);
  if (!http.begin(c, url)) return String();
  http.setUserAgent("Zond/1.0 (robot pribadi ESP32)");   // Wikipedia meminta nama aplikasi
  http.setTimeout(10000);
  int kode = http.GET();
  String resp = kode == 200 ? http.getString() : String();
  http.end();
  if (!resp.length()) { Serial.printf("[cari] Wikipedia %s gagal (%d)\n", bahasa, kode); return String(); }
  JsonDocument saring;
  saring["query"]["pages"][0]["title"] = true;
  saring["query"]["pages"][0]["extract"] = true;
  JsonDocument d;
  if (deserializeJson(d, resp, DeserializationOption::Filter(saring))) return String();
  String judul = d["query"]["pages"][0]["title"] | "", isi = d["query"]["pages"][0]["extract"] | "";
  if (!isi.length()) return String();
  Serial.printf("[cari] %s: %s (%u huruf)\n", bahasa, judul.c_str(), (unsigned)isi.length());
  return judul + " (Wikipedia " + bahasa + "): " + isi;
}

// Wikipedia bahasa Indonesia dulu; kalau tidak ada, bahasa Inggris (AI tetap menjawab dalam bahasa Indonesia).
String cariWikipedia(const String& kata) {
  String h = cariWikipediaDi("id", kata);
  return h.length() ? h : cariWikipediaDi("en", kata);
}

// jalankan JSON {"bicara":..,"aksi":[..]}; kembalikan kalimat untuk dikirim ke Telegram.
// sudahCari: jawaban ini sudah memakai hasil Wikipedia, jadi aksi cari tidak dijalankan lagi.
String jalankanJawaban(const String& jawaban, const String& pertanyaanAsli, bool bolehFoto, bool sudahCari = false) {
  int a = jawaban.indexOf('{'), b = jawaban.lastIndexOf('}');
  JsonDocument j;
  if (a < 0 || b < a || deserializeJson(j, jawaban.substring(a, b + 1))) {
    ucapkan(jawaban); return jawaban;                    // bukan JSON: ucapkan apa adanya
  }
  String bicara = j["bicara"] | "";
  if (bicara.length()) ucapkan(bicara);                  // bicara dulu, baru bergerak
  bool mintaFoto = false;
  String cari;
  for (JsonObject x : j["aksi"].as<JsonArray>()) {
    if (x["m"].is<JsonArray>()) {
      int l = constrain((int)x["m"][0], -200, 200), r = constrain((int)x["m"][1], -200, 200), ms = constrain((int)x["m"][2], 100, 2000);
      gerak(l, r, ms); vTaskDelay(pdMS_TO_TICKS(ms + 150));
    } else if (x["belok"].is<int>()) {
      belok((int)x["belok"], 6000);
    } else if (x["ingatkan"].is<JsonArray>()) {
      String jm = x["ingatkan"][0] | "", ps = x["ingatkan"][1] | "", ul = x["ingatkan"][2] | "sekali";
      int hh = -1, mm = -1;
      if (sscanf(jm.c_str(), "%d:%d", &hh, &mm) == 2) {
        String galat = tambahAlarm(hh, mm, ul == "harian" ? 0x7F : 0, ps, 0);
        if (galat.length()) { bicara = "Gagal memasang pengingat: " + galat; ucapkan(bicara); }
      }
    } else if (x["sekeliling"].is<int>() && bolehFoto && fiturKamera) {
      aksiTertunda = 1;                                  // dijalankan tugas jaringan setelah jawaban ini selesai
    } else if (x["suit"].is<int>() && bolehFoto && fiturKamera) {
      aksiTertunda = 2;
    } else if (x["kepala"].is<JsonArray>()) {
      kirimBadan("P " + String((int)x["kepala"][0]) + " " + String((int)x["kepala"][1])); vTaskDelay(pdMS_TO_TICKS(400));
    } else if (x["foto"].is<int>()) {
      mintaFoto = true;
    } else if (x["cari"].is<const char*>() && !sudahCari) {
      cari = x["cari"].as<String>();
    } else if (x["diam"].is<int>()) {
      kirimAudio(A_STOP);
    } else if (x["mode"].is<const char*>()) {
      String md = x["mode"].as<String>();
      if (md == "jelajah") jelajah(5);
      else gantiMode(md == "ikuti" ? M_IKUTI : md == "jaga" ? M_JAGA : M_BIASA);
    }
  }
  if (cari.length()) {
    setStatus("Mencari: " + cari);
    String hasil = cariWikipedia(cari);
    simpanWiki(cari, hasil);                               // juga tampil di layar (Info -> Wikipedia)
    String q2 = pertanyaanAsli + "\n\n(Hasil pencarian Wikipedia untuk \"" + cari + "\": " +
                (hasil.length() ? hasil : String("tidak ditemukan")) +
                ")\nJawab pertanyaan di atas memakai hasil ini kalau cocok, dengan kata-katamu sendiri. Jangan memakai aksi cari lagi.";
    String j2 = tanyaAI(q2, "", false);
    // ganti jawaban "sebentar, aku cari dulu" di riwayat dengan jawaban akhirnya
    if (riwayatN > 0 && riwayatPeran[riwayatN - 1] == "assistant") riwayatIsi[riwayatN - 1] = j2.substring(0, 1500);
    String akhir = jalankanJawaban(j2, pertanyaanAsli, bolehFoto, true);
    return bicara.length() ? bicara + "\n" + akhir : akhir;
  }
  if (mintaFoto && bolehFoto) {
    setStatus("Melihat...");
    xSemaphoreTake(kunciFoto, portMAX_DELAY);
    bool ok = ambilFoto("vga");
    String b64 = ok ? base64::encode(fotoBuf, fotoLen) : "";
    xSemaphoreGive(kunciFoto);
    if (ok) {
      String j2 = tanyaAI(pertanyaanAsli + " (Ini foto dari kameramu barusan. Jawab berdasarkan foto.)", b64);
      return jalankanJawaban(j2, pertanyaanAsli, false, sudahCari);
    }
    bicara = "Kameraku tidak bisa dihubungi.";
    ucapkan(bicara);
  }
  return bicara;
}

// ======================= FITUR KAMERA AI: LIHAT SEKELILING DAN SUIT =======================
// Keduanya memakai AI dengan foto (jatah AI lebih cepat habis), jadi harus diaktifkan dulu di panel web.
// Dijalankan oleh tugas jaringan; chat = tujuan pesan Telegram.

void aturFiturKamera(bool nyala) {
  fiturKamera = nyala;
  Preferences q; q.begin("robot", false); q.putUChar("kamAI", nyala ? 1 : 0); q.end();
  setStatus(nyala ? "Fitur kamera AI aktif" : "Fitur kamera AI mati");
}

// Tunggu speaker selesai bicara (paling lama batasMs).
void tungguSelesaiBicara(unsigned long batasMs) {
  unsigned long t0 = millis();
  while (!audioJalan && millis() - t0 < 1500) vTaskDelay(pdMS_TO_TICKS(20));   // tunggu mulai
  while (audioJalan && millis() - t0 < batasMs) vTaskDelay(pdMS_TO_TICKS(20));
}

// Ambil satu foto dan kembalikan base64-nya (kosong kalau gagal).
String fotoBase64Sekarang(const char* res) {
  xSemaphoreTake(kunciFoto, portMAX_DELAY);
  String b64 = ambilFoto(res) ? base64::encode(fotoBuf, fotoLen) : String();
  xSemaphoreGive(kunciFoto);
  return b64;
}

void lihatSekeliling(const String& chat) {
  if (!fiturKamera) { bot.sendMessage(chat, "Fitur kamera AI belum diaktifkan. Nyalakan dulu di panel web."); return; }
  if (mode != M_BIASA) gantiMode(M_BIASA);
  ekspresi = PIKIR; setStatus("Melihat sekeliling...");
  bot.sendChatAction(chat, "typing");
  const int PAN[3] = {150, 90, 30};                    // kiri, depan, kanan
  String semua;
  for (int i = 0; i < 3; i++) {
    kirimBadan("P " + String(PAN[i]) + " 90");
    arahMata = i - 1;
    vTaskDelay(pdMS_TO_TICKS(i == 0 ? 1100 : 800));    // servo bergerak 3 derajat tiap 15 ms, lalu diam sebentar supaya foto tidak kabur
    String f = fotoBase64Sekarang("vga");
    if (!f.length()) { bot.sendMessage(chat, "Kamera tidak bisa dihubungi: " + alasanFoto); break; }
    if (semua.length()) semua += ",";
    semua += f;
  }
  kirimBadan("P 90 90"); arahMata = 0;
  if (!semua.length()) { ekspresi = NORMAL; setStatus("Kamera tidak bisa dihubungi"); return; }
  String q = "Ini foto dari kameramu berurutan: kiri, depan, kanan. Ceritakan singkat apa saja yang ada di sekitarmu "
             "(paling banyak 3 kalimat). Jangan memberi aksi.";
  String kata = jalankanJawaban(tanyaAI(q, semua), q, false);
  bot.sendMessage(chat, "Sekelilingku: " + (kata.length() ? kata : String("(tidak ada jawaban)")));
  ekspresi = NORMAL; setStatus(kata);
}

// Suit: Zond memilih acak, memotret tanganmu tepat setelah berkata "kertas!", AI menebak bentuk tanganmu.
void mainSuit(const String& chat) {
  if (!fiturKamera) { bot.sendMessage(chat, "Fitur kamera AI belum diaktifkan. Nyalakan dulu di panel web."); return; }
  if (mode != M_BIASA) gantiMode(M_BIASA);
  const char* NAMA[3] = {"batu", "gunting", "kertas"};
  int aku = esp_random() % 3;                          // dipilih sebelum melihat tanganmu
  kirimBadan("P 90 90");
  ekspresi = DENGAR; setStatus("Suit! Tunjukkan tanganmu ke kamera");
  ucapkan("Suit! Batu, gunting, kertas!");
  tungguSelesaiBicara(8000);
  vTaskDelay(pdMS_TO_TICKS(250));
  String f = fotoBase64Sekarang("vga");
  if (!f.length()) { ekspresi = NORMAL; setStatus("Kamera tidak bisa dihubungi"); bot.sendMessage(chat, "Kamera tidak bisa dihubungi: " + alasanFoto); return; }
  ekspresi = PIKIR; setStatus(String("Aku: ") + NAMA[aku]);
  String q = "Permainan suit (batu-gunting-kertas). Lihat tangan pemain di foto ini. Batu = kepalan, gunting = dua jari, "
             "kertas = telapak terbuka. Jawab JSON {\"bicara\":\"\",\"aksi\":[],\"tangan\":\"batu\" atau \"gunting\" atau \"kertas\" "
             "atau \"tidak_jelas\"}. Isi bicara dengan string kosong.";
  String jaw = tanyaAI(q, f, false);
  int a = jaw.indexOf('{'), b = jaw.lastIndexOf('}');
  JsonDocument j;
  String tangan = (a >= 0 && b > a && !deserializeJson(j, jaw.substring(a, b + 1))) ? String(j["tangan"] | "") : String();
  tangan.toLowerCase();
  int kamu = tangan == "batu" ? 0 : tangan == "gunting" ? 1 : tangan == "kertas" ? 2 : -1;
  String hasil;
  if (kamu < 0) {
    hasil = String("Aku pilih ") + NAMA[aku] + ", tapi tanganmu tidak terlihat jelas. Coba lagi, tunjukkan tanganmu lebih dekat ke kamera.";
    ekspresi = NORMAL;
  } else {
    // batu (0) menang lawan gunting (1), gunting menang lawan kertas (2), kertas menang lawan batu
    int s = (kamu - aku + 3) % 3;                      // 0 seri, 1 aku menang, 2 kamu menang
    hasil = String("Aku pilih ") + NAMA[aku] + ", kamu " + NAMA[kamu] + ". ";
    if (s == 0) hasil += "Seri!";
    else if (s == 1) { skorZond++; hasil += "Aku menang!"; }
    else { skorKamu++; hasil += "Kamu menang!"; ekspresiSebentar(KAGET, 2000); }
    hasil += " Skor kamu " + String(skorKamu) + ", aku " + String(skorZond) + ".";
    if (s != 2) ekspresi = NORMAL;
  }
  ucapkan(hasil);
  setStatus(hasil);
  bot.sendMessage(chat, "Suit: " + hasil);
}

// ======================= TUGAS JARINGAN (Telegram + AI) =======================
enum JenisTugas { T_SUARA };
struct Tugas { JenisTugas jenis; };

// Pasang SD card. Kabel lewat breadboard, jadi kecepatan tinggi kadang gagal: coba beberapa kali, makin pelan.
// 4 MHz pun masih cukup untuk FLAC. Dipanggil lagi kalau SD belum terbaca.
bool pasangSD() {
  if (adaSD) return true;
  static bool spiSiap = false;
  if (!SD_BERBAGI && !spiSiap) {
    spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    gpio_pullup_en((gpio_num_t)SD_MISO);                   // kartu SD butuh penarik ke atas di MISO
    spiSiap = true;
  }
  const uint32_t frek[] = {10000000, 4000000, 1000000};
  kunciSD();
  for (uint32_t f : frek) {
    SD.end();
    if (SD_BERBAGI) { digitalWrite(TFT_CS, HIGH); digitalWrite(TOUCH_CS, HIGH); }   // layar dan sentuhan tidak boleh ikut menjawab
    delay(50);
    bool ok = SD.begin(SD_CS, SD_BERBAGI ? SPI : spiSD, f) && SD.cardType() != CARD_NONE;
    Serial.printf("[sd] coba %u MHz: %s\n", (unsigned)(f / 1000000), ok ? "berhasil" : "gagal");
    if (ok) {
      adaSD = true;
      Serial.printf("SD card terbaca pada %u MHz (%llu MB)\n", (unsigned)(f / 1000000), SD.cardSize() / (1024 * 1024));
      break;
    }
  }
  lepasSD();
  if (!adaSD) Serial.println("SD card tidak terbaca (boleh, hanya untuk musik). Cek kartu terpasang penuh dan kabel SD_");
  return adaSD;
}

// Lagu = file di folder utama SD dengan format yang bisa diputar library Audio. Huruf besar/kecil ekstensi tidak masalah.
bool fileLagu(String n) {
  if (n.startsWith(".")) return false;                     // file sampah macOS (._lagu.mp3) dan file tersembunyi
  n.toLowerCase();
  const char* eks[] = {".mp3", ".flac", ".wav", ".m4a", ".aac", ".ogg", ".opus"};
  for (const char* e : eks) if (n.endsWith(e)) return true;
  return false;
}

// Semua lagu di folder utama SD, satu per baris.
String daftarLagu() {
  if (!pasangSD()) return "SD card tidak terbaca. Cek kartu terpasang penuh, lalu coba lagi.";
  String s;
  kunciSD();
  File root = SD.open("/");
  for (File f = root.openNextFile(); f; f = root.openNextFile())
    if (!f.isDirectory() && fileLagu(f.name())) s += String(f.name()) + "\n";
  root.close();
  lepasSD();
  return s.length() ? s : "Belum ada lagu di folder utama SD card (mp3, flac, wav, m4a, aac, ogg, opus).";
}

// Cari lagu dari sebagian namanya, tanpa peduli huruf besar/kecil: "/putar hujan" menemukan "Hujan_Bulan_Juni.flac".
String cariLagu(String kata) {
  kata.trim(); kata.toLowerCase();
  if (!adaSD || kata.isEmpty()) return "";
  String daftar = daftarLagu(), hasil;
  int i = 0;
  while (i < (int)daftar.length()) {
    int j = daftar.indexOf('\n', i); if (j < 0) j = daftar.length();
    String n = daftar.substring(i, j), k = n; k.toLowerCase();
    if (k == kata) return n;                                 // nama persis selalu menang
    if (hasil.isEmpty() && k.indexOf(kata) >= 0) hasil = n;
    i = j + 1;
  }
  return hasil;
}

const char* BANTUAN =
  "Perintah:\n/status  data sensor\n/cuaca  cuaca hari ini\n/maju /mundur /kiri /kanan [ms]\n/belok 90 (kanan) /belok -90 (kiri) /balik\n/stop\n/kepala pan tilt  (90 90 = lurus)\n"
  "/alarm  daftar alarm\n/alarm 06:30 pesan  sekali\n/alarm harian 06:30 pesan\n/alarm hapus 2\n"
  "/sekeliling /suit  (aktifkan dulu fitur kamera AI di panel web)\n"
  "/foto  kirim foto kamera\n/lihat [pertanyaan]  AI melihat lewat kamera\n/bilang teks  robot mengucapkan teks\n"
  "/panel  alamat panel kontrol web\n/dengar on|off  mendengar terus (boros) atau hanya saat BICARA\n/ai  pilih otak AI (gemini, gemma, groq, haiku)\n/lagu  daftar lagu di SD\n/putar nama lagu (cukup sebagian)\n/radio url\n/diam  hentikan suara\n/volume 0-21\n"
  "/ikuti  ikuti benda di depan\n/jaga  berjaga, kirim foto kalau ada gerakan\n/jelajah [menit]  jalan-jalan sendiri (bawaan 5 menit)\n/biasa  kembali normal\n"
  "/baterai  isi dan perkiraan sisa baterai\n/pesan teks  tampilkan pesan di layar\n/sholat  jadwal sholat hari ini\n/kurs  kurs rupiah\n/wiki topik  cari di Wikipedia\n/suara  suara bicara (/suara gemini atau /suara google)\n\n"
  "Pesan biasa tanpa / = ngobrol dengan AI (bisa menyuruh bergerak).";

void tanganiPesan(const telegramMessage& m) {
  String chat = m.chat_id, t = m.text;
  if (chat != TELEGRAM_CHATID) { bot.sendMessage(chat, "Maaf, aku hanya menurut ke pemilikku."); return; }
  t.trim();
  Serial.println("[telegram] " + t);
  String cmd = t, arg = "";
  int sp = t.indexOf(' ');
  if (sp > 0) { cmd = t.substring(0, sp); arg = t.substring(sp + 1); arg.trim(); }
  cmd.toLowerCase();
  int ms = arg.toInt() > 0 ? constrain(arg.toInt(), 100, 3000) : 700;
  if (mode == M_JELAJAH && (cmd == "/maju" || cmd == "/mundur" || cmd == "/kiri" || cmd == "/kanan")) gantiMode(M_BIASA);

  if (cmd == "/start" || cmd == "/bantuan" || cmd == "/help") bot.sendMessage(chat, String("Halo, aku ") + NAMA_ROBOT + "!\n\n" + BANTUAN);
  else if (cmd == "/status") bot.sendMessage(chat, ringkasSensor());
  else if (cmd == "/baterai") bot.sendMessage(chat, teksBaterai());
  else if (cmd == "/jelajah") {
    int menit = arg.length() ? arg.toInt() : 5;
    if (arg == "stop" || arg == "off") menit = 0;
    jelajah(menit);
    bot.sendMessage(chat, mode == M_JELAJAH ? "Jelajah " + String(constrain(menit, 1, 60)) + " menit: aku maju sampai ada halangan lalu belok. "
                                              "Ketik /biasa atau tekan STOP untuk berhenti." : String("Jelajah berhenti."));
  }
  else if (cmd == "/suara") {
    if (arg == "gemini" || arg == "cowok") aturSuara(true); else if (arg == "google") aturSuara(false);
    bot.sendMessage(chat, teksSuara());
  }
  else if (cmd == "/cuaca") {
    if (!cuaca.ada) ambilCuaca();
    bot.sendMessage(chat, cuaca.ada ? teksCuaca(true) : String("Data cuaca belum bisa diambil. Cek WiFi."));
  }
  else if (cmd == "/belok" || cmd == "/balik") {
    int d = cmd == "/balik" ? 180 : arg.toInt();
    if (d == 0) { bot.sendMessage(chat, "Contoh: /belok 90 (kanan), /belok -90 (kiri), /balik"); return; }
    if (mode != M_BIASA) gantiMode(M_BIASA);
    int h = belok(d, 8000);
    bot.sendMessage(chat, h == 1 ? "Berbelok " + String(putarHasil) + " derajat." :
                          h == 2 ? "Roda selip atau macet, baru " + String(putarHasil) + " derajat." :
                          h == 3 ? String("Badan belum punya giroskop (upload kode badan terbaru). Berbelok memakai waktu.") :
                          h == 4 ? String("Ditolak badan (tombol STOP, miring, atau baterai lemah).") : String("Badan tidak menjawab."));
  }
  else if (cmd == "/alarm") {
    String a = arg; a.trim();
    if (a.startsWith("hapus")) {
      int i = a.substring(5).toInt();
      bot.sendMessage(chat, hapusAlarm(i - 1) ? "Alarm " + String(i) + " dihapus.\n\n" + daftarAlarmTeks() : String("Nomor alarm tidak ada. Ketik /alarm untuk melihat daftarnya."));
    } else if (a.length()) {
      bool harian = a.startsWith("harian");
      if (harian) { a = a.substring(6); a.trim(); }
      int hh = -1, mm = -1;
      int sp2 = a.indexOf(' ');
      String jm = sp2 > 0 ? a.substring(0, sp2) : a, ps = sp2 > 0 ? a.substring(sp2 + 1) : String();
      if (sscanf(jm.c_str(), "%d:%d", &hh, &mm) != 2 && sscanf(jm.c_str(), "%d.%d", &hh, &mm) != 2) {
        bot.sendMessage(chat, "Format: /alarm 06:30 pesan  atau  /alarm harian 06:30 pesan"); return;
      }
      ps.trim();
      String galat = tambahAlarm(hh, mm, harian ? 0x7F : 0, ps, 0);
      bot.sendMessage(chat, galat.length() ? "Gagal: " + galat : "Alarm dipasang.\n\n" + daftarAlarmTeks());
    } else bot.sendMessage(chat, daftarAlarmTeks() + "\nAtur lebih lengkap (hari, radio, lagu) di panel web.");
  }
  else if (cmd == "/sekeliling") lihatSekeliling(chat);
  else if (cmd == "/suit") mainSuit(chat);
  else if (cmd == "/ai") {
    if (arg.length() && !pilihAI(arg)) bot.sendMessage(chat, "Tidak bisa memakai \"" + arg + "\": nama salah atau API key-nya belum diisi.");
    String t = "Otak AI sekarang: " + String(namaAI[pilihanAI]) + (aiTerakhir.length() ? " (terakhir menjawab: " + aiTerakhir + ")" : "") + "\nPilihan:";
    for (int i = 0; i < AI_JUMLAH; i++) t += String("\n/ai ") + namaAI[i] + (aiTersedia(i) ? "" : "  (API key kosong)");
    bot.sendMessage(chat, t);
  }
  else if (cmd == "/dengar") {
    if (arg == "on" || arg == "nyala") aturDengarTerus(true);
    else if (arg == "off" || arg == "mati") aturDengarTerus(false);
    bot.sendMessage(chat, dengarTerus ? "Dengar terus: NYALA (boros jatah suara-ke-teks). /dengar off untuk mematikan."
                                      : "Dengar terus: MATI. Pakai tombol BICARA di layar atau panel web. /dengar on untuk menyalakan.");
  }
  else if (cmd == "/panel")  bot.sendMessage(chat, "Panel kontrol (HP harus satu WiFi dengan robot): http://" + WiFi.localIP().toString() + "/");
  else if (cmd == "/maju")   { gerak(150, 150, ms);   bot.sendMessage(chat, "Maju " + String(ms) + " ms"); }
  else if (cmd == "/mundur") { gerak(-150, -150, ms); bot.sendMessage(chat, "Mundur " + String(ms) + " ms"); }
  else if (cmd == "/kiri")   { gerak(-160, 160, min(ms, 600)); bot.sendMessage(chat, "Putar kiri"); }
  else if (cmd == "/kanan")  { gerak(160, -160, min(ms, 600)); bot.sendMessage(chat, "Putar kanan"); }
  else if (cmd == "/stop")   { gantiMode(M_BIASA); kirimBadan("S"); kirimAudio(A_STOP); bot.sendMessage(chat, "Berhenti."); }
  else if (cmd == "/kepala") {
    int p = 90, q = 90; sscanf(arg.c_str(), "%d %d", &p, &q);
    kirimBadan("P " + String(p) + " " + String(q)); bot.sendMessage(chat, "Kepala ke " + String(p) + ", " + String(q));
  }
  else if (cmd == "/foto") {
    bot.sendChatAction(chat, "upload_photo");
    xSemaphoreTake(kunciFoto, portMAX_DELAY);
    bool ok = ambilFoto("vga");
    bool terkirim = ok && kirimFotoTelegram(chat);
    xSemaphoreGive(kunciFoto);
    if (!ok) bot.sendMessage(chat, "Gagal mengambil foto: " + alasanFoto);
    else if (!terkirim) bot.sendMessage(chat, "Foto berhasil diambil (" + String(fotoLen) + " byte) tapi gagal dikirim ke Telegram. Coba lagi.");
  }
  else if (cmd == "/lihat") {
    String q = arg.length() ? arg : "Apa yang kamu lihat?";
    ekspresi = PIKIR; setStatus("Melihat...");
    bot.sendChatAction(chat, "typing");
    xSemaphoreTake(kunciFoto, portMAX_DELAY);
    bool ok = ambilFoto("vga");
    String b64 = "";
    if (ok) { kirimFotoTelegram(chat); b64 = base64::encode(fotoBuf, fotoLen); }
    xSemaphoreGive(kunciFoto);
    if (!ok) { bot.sendMessage(chat, "Kamera tidak bisa dihubungi: " + alasanFoto); ekspresi = NORMAL; return; }
    Serial.printf("[lihat] foto %u byte, base64 %u, memori bebas %u, PSRAM bebas %u\n",
                  (unsigned)fotoLen, (unsigned)b64.length(), (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
    String jaw = tanyaAI(q, b64);
    String kata = jalankanJawaban(jaw, q, false);
    bot.sendMessage(chat, kata.length() ? kata : "(tidak ada jawaban)");
    ekspresi = NORMAL; setStatus(kata);
  }
  else if (cmd == "/ikuti")  { gantiMode(M_IKUTI); bot.sendMessage(chat, "Mode ikuti: aku menjaga jarak 35-70 cm ke benda di depanku. /biasa untuk berhenti."); }
  else if (cmd == "/jaga")   { gantiMode(M_JAGA);  bot.sendMessage(chat, "Mode jaga aktif dalam 4 detik. Aku kirim foto kalau ada gerakan di depan. /biasa untuk berhenti."); }
  else if (cmd == "/biasa")  { gantiMode(M_BIASA); bot.sendMessage(chat, "Kembali ke mode biasa."); }
  else if (cmd == "/bilang") { ucapkan(arg); bot.sendMessage(chat, "Oke."); }
  else if (cmd == "/sholat") {
    if (arg == "mati" || arg == "suara" || arg == "adzan") {
      pengingatSholat = arg == "mati" ? 0 : arg == "suara" ? 1 : 2;
      Preferences q; q.begin("robot", false); q.putUChar("sholat", pengingatSholat); q.end();
    }
    const char* P[3] = {"mati", "suara", "suara + adzan dari SD (/adzan.mp3)"};
    bot.sendMessage(chat, teksSholat() + "\n\nPengingat: " + P[pengingatSholat % 3] + ". Ubah: /sholat mati, /sholat suara, /sholat adzan");
  }
  else if (cmd == "/kurs") { if (!kurs.ada || millis() - kurs.t > 21600000UL) ambilKurs(); bot.sendMessage(chat, teksKurs()); }
  else if (cmd == "/wiki") {
    if (!arg.length()) bot.sendMessage(chat, "Tulis topiknya, misalnya: /wiki Gunung Rinjani");
    else { bot.sendChatAction(chat, "typing"); String h = cariWikipedia(arg); simpanWiki(arg, h); bot.sendMessage(chat, h.length() ? h : String("Tidak ditemukan di Wikipedia.")); }
  }
  else if (cmd == "/pesan") {
    if (!arg.length()) bot.sendMessage(chat, "Tulis isinya, misalnya: /pesan Makan malam sudah siap");
    else { tambahPesan(m.from_name.length() ? m.from_name : String("Telegram"), arg); bot.sendMessage(chat, "Pesan tampil di layar Zond."); }
  }
  else if (cmd == "/lagu")   bot.sendMessage(chat, daftarLagu());
  else if (cmd == "/putar")  {
    String lagu = cariLagu(arg);
    if (lagu.isEmpty()) bot.sendMessage(chat, "Lagu \"" + arg + "\" tidak ditemukan. Ketik /lagu untuk melihat daftarnya.");
    else { kirimAudio(A_FILE, "/" + lagu); bot.sendMessage(chat, "Memutar " + lagu); }
  }
  else if (cmd == "/radio")  { kirimAudio(A_URL, arg); bot.sendMessage(chat, "Memutar radio"); }
  else if (cmd == "/diam")   { kirimAudio(A_STOP); bot.sendMessage(chat, "Diam."); }
  else if (cmd == "/volume") { kirimAudio(A_VOL, arg); bot.sendMessage(chat, "Volume " + arg); }
  else if (t.startsWith("/")) bot.sendMessage(chat, "Perintah tidak dikenal.\n\n" + String(BANTUAN));
  else {
    ekspresi = PIKIR; setStatus("Berpikir...");
    bot.sendChatAction(chat, "typing");
    String kata = jalankanJawaban(tanyaAI(t), t, true);
    bot.sendMessage(chat, kata.length() ? kata : "(diam)");
    ekspresi = NORMAL; setStatus(kata);
  }
}

void tugasJaringan(void*) {
  unsigned long tPoll = 0;
  for (;;) {
    if (WiFi.status() != WL_CONNECTED) { vTaskDelay(pdMS_TO_TICKS(2000)); continue; }
    if (suaraSiap) {
      bool butuhNama = suaraButuhNama;
      String kata;
      if (!butuhNama || izinSuaraKeTeks()) {
        if (!butuhNama) { ekspresi = PIKIR; setStatus("Mengubah suara ke teks..."); }
        kata = suaraKeTeks();
      } else Serial.println("[dengar] terlalu banyak suara dalam semenit, dilewati");
      suaraSiap = false;                                  // wavBuf boleh dipakai lagi
      if (karanganWhisper(kata)) { Serial.println("[dengar] karangan Whisper dari bising, diabaikan: " + kata); kata = ""; }
      String sisa;
      bool adaNama = kata.length() && cariNama(kata, sisa);
      if (kata.length() == 0) {
        if (!butuhNama) { setStatus(strlen(GROQ_API_KEY) ? "Tidak terdengar jelas" : "GROQ_API_KEY belum diisi"); ekspresi = NORMAL; }
        if (!butuhNama && tujuanDengar) { tujuanDengar = 0; infoBaru = true; }
      } else if (butuhNama && !adaNama) {
        Serial.println("[dengar] tanpa nama, diabaikan: " + kata);
      } else if (adaNama && sisa.length() == 0) {         // hanya memanggil nama: tunggu perintahnya
        if (butuhNama) ucapkan("Ya?");                    // sudah menunggu perintah: jangan jawab "Ya?" lagi,
        else Serial.println("[dengar] hanya nama lagi, diabaikan");   // supaya gema/bising tidak membuat putaran
        jendelaSampai = millis() + 9000;
        ekspresi = DENGAR; setStatus("Ya? Silakan bicara");
      } else if (tujuanDengar == 1 && !butuhNama) {          // layar Wikipedia menunggu topik
        tujuanDengar = 0; jendelaSampai = 0;
        ekspresi = PIKIR; setStatus("Mencari: " + kata);
        simpanWiki(kata, cariWikipedia(kata));
        ekspresi = NORMAL; setStatus("Wikipedia: " + kata);
      } else {
        jendelaSampai = 0;
        ekspresi = PIKIR; setStatus("Kamu: " + kata);
        String jawab = jalankanJawaban(tanyaAI(kata), kata, true);
        bot.sendMessage(TELEGRAM_CHATID, "Kamu bilang: " + kata + "\n" + String(NAMA_ROBOT) + ": " + jawab);
        setStatus(jawab); ekspresi = NORMAL;
      }
    }
    if (alarmJaga) {
      alarmJaga = false;
      xSemaphoreTake(kunciFoto, portMAX_DELAY);
      bool ok = ambilFoto("vga") && kirimFotoTelegram(TELEGRAM_CHATID);
      xSemaphoreGive(kunciFoto);
      bot.sendMessage(TELEGRAM_CHATID, String("Mode jaga: ada gerakan di depan robot!") + (ok ? "" : " (foto gagal)"));
    }
    if (pesanBadan) {
      bot.sendMessage(TELEGRAM_CHATID, pesanBadan == 1 ? "Baterai lemah! Motor dimatikan. Isi ulang baterai." : "Tombol STOP di badan ditekan.");
      pesanBadan = 0;
    }
    PesanKirim pk;                                       // pesan dari tugas lain (pengingat, dll.)
    while (antrianKirim && xQueueReceive(antrianKirim, &pk, 0) == pdTRUE) bot.sendMessage(TELEGRAM_CHATID, pk.isi);
    if (aksiTertunda) {                                  // dari panel web atau perintah suara
      int a = aksiTertunda; aksiTertunda = 0;
      if (a == 1) lihatSekeliling(TELEGRAM_CHATID);
      else if (a == 2) mainSuit(TELEGRAM_CHATID);
    }
    if (mintaInfo) {                                     // data yang diminta layar
      uint8_t m = mintaInfo; mintaInfo = 0;
      if (m & INFO_CUACA) ambilCuaca();
      if (m & INFO_SHOLAT) ambilSholat();
      if (m & INFO_BERITA) ambilBerita();
      if (m & INFO_KURS) ambilKurs();
      infoBaru = true;
    }
    static unsigned long tSholat = 0;                    // jadwal sholat sekali sehari; coba lagi 2 menit kalau gagal
    uint32_t hariIni = tanggalHariIni();
    if (hariIni && (!sholat.ada || sholat.tanggal != hariIni) && (!tSholat || millis() - tSholat > 120000UL)) {
      tSholat = millis(); if (ambilSholat()) infoBaru = true;
    }
    static unsigned long tCuaca = 0;                     // cuaca tiap 30 menit; coba lagi 2 menit kalau gagal
    if (!tCuaca || millis() - tCuaca > (cuaca.ada ? 1800000UL : 120000UL)) { tCuaca = millis(); ambilCuaca(); }
    if (millis() - tPoll > 1500) {
      tPoll = millis();
      int n = bot.getUpdates(bot.last_message_received + 1);
      while (n) {
        for (int i = 0; i < n; i++) tanganiPesan(bot.messages[i]);
        n = bot.getUpdates(bot.last_message_received + 1);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// ---- kalibrasi sentuh: sama dengan tes_layar (5 titik + 1 titik uji, diulang sendiri kalau meleset) ----
bool sentuhMentah(int32_t &x, int32_t &y) { return lcd.getTouchRaw(&x, &y) > 0; }

void tungguLepasLayar() {
  int32_t x, y; unsigned long t = millis();
  while (millis() - t < 200) { if (sentuhMentah(x, y)) t = millis(); delay(10); }
}

// Tunggu satu tekanan, ambil 15 bacaan, kembalikan nilai tengahnya. false = terlalu singkat atau goyang.
bool ambilTekanStabil(bool angkaLayar, int32_t &hx, int32_t &hy) {
  int32_t xs[15], ys[15], x, y; int n = 0, luput = 0;
  while (!(angkaLayar ? lcd.getTouch(&x, &y) : lcd.getTouchRaw(&x, &y))) delay(5);
  delay(60);
  unsigned long t0 = millis();
  while (n < 15 && millis() - t0 < 2000) {
    if (angkaLayar ? lcd.getTouch(&x, &y) : lcd.getTouchRaw(&x, &y)) { xs[n] = x; ys[n] = y; n++; luput = 0; }
    else if (++luput >= 3) break;
    delay(15);
  }
  tungguLepasLayar();
  if (n < 8) return false;
  std::sort(xs, xs + n); std::sort(ys, ys + n);
  int32_t batas = angkaLayar ? 6 : 60;
  if (xs[n * 3 / 4] - xs[n / 4] > batas || ys[n * 3 / 4] - ys[n / 4] > batas) return false;
  hx = xs[n / 2]; hy = ys[n / 2];
  return true;
}

void tandaKalibrasi(int x, int y, uint16_t w) {
  lcd.drawCircle(x, y, 12, w);
  lcd.drawFastHLine(x - 18, y, 37, w);
  lcd.drawFastVLine(x, y - 18, 37, w);
  lcd.fillCircle(x, y, 2, TFT_RED);
}

void keArah0(float &x, float &y) {          // titik arah sekarang -> titik arah 0 (cara LovyanGFX)
  uint8_t r = lcd.getRotation();
  float w = lcd.width(), h = lcd.height();
  if ((1 << r) & 0b10010110) y = (h - 1) - y;
  if (r & 2) x = (w - 1) - x;
  if (r & 1) std::swap(x, y);
}

bool hitungKalibrasi(const float *mx, const float *my, const float *tx, const float *ty, int n, float *a) {
  double rx = 0, ry = 0, ax = 0, ay = 0;
  for (int i = 0; i < n; i++) { rx += mx[i]; ry += my[i]; ax += tx[i]; ay += ty[i]; }
  rx /= n; ry /= n; ax /= n; ay /= n;
  double uu = 0, uv = 0, vv = 0, ux = 0, vx = 0, uy = 0, vy = 0;
  for (int i = 0; i < n; i++) {
    double u = mx[i] - rx, v = my[i] - ry, X = tx[i] - ax, Y = ty[i] - ay;
    uu += u * u; uv += u * v; vv += v * v; ux += u * X; vx += v * X; uy += u * Y; vy += v * Y;
  }
  double det = uu * vv - uv * uv;
  if (uu <= 0 || vv <= 0 || det < 0.01 * uu * vv) return false;
  a[0] = (ux * vv - vx * uv) / det; a[1] = (vx * uu - ux * uv) / det; a[2] = ax - a[0] * rx - a[1] * ry;
  a[3] = (uy * vv - vy * uv) / det; a[4] = (vy * uu - uy * uv) / det; a[5] = ay - a[3] * rx - a[4] * ry;
  return true;
}

void tulisKalibrasi(const char* a, const char* b, uint16_t warna) {
  lcd.fillScreen(TFT_BLACK); lcd.setTextDatum(middle_center); lcd.setFont(&fonts::Font2);
  lcd.setTextColor(warna, TFT_BLACK); lcd.drawString(a, lcd.width() / 2, lcd.height() / 2 - 10);
  lcd.setTextColor(TFT_WHITE, TFT_BLACK); lcd.drawString(b, lcd.width() / 2, lcd.height() / 2 + 12);
}

void kalibrasiSentuh(bool paksa) {
  float aff[6];
  pref.begin("robot", false);
  if (!paksa && pref.getBytesLength("aff") == sizeof(aff)) {     // hasil tes_layar atau kalibrasi sebelumnya
    pref.getBytes("aff", aff, sizeof(aff)); lcd.panel()->setCalibrateAffine(aff); pref.end(); return;
  }
  int w = lcd.width(), h = lcd.height(), m = 25;
  const int px[5] = {m, w - 1 - m, w - 1 - m, m, w / 2};
  const int py[5] = {m, m, h - 1 - m, h - 1 - m, h / 2};
  float mx[5], my[5], tx[5], ty[5];
  int32_t x, y;
  char s[48];
  while (true) {
    for (int i = 0; i < 5; i++) {
      while (true) {
        lcd.fillScreen(TFT_BLACK); lcd.setTextDatum(middle_center); lcd.setFont(&fonts::Font2); lcd.setTextColor(TFT_WHITE, TFT_BLACK);
        snprintf(s, sizeof(s), "KALIBRASI titik %d dari 5", i + 1);
        lcd.drawString(s, w / 2, h / 2 - 50);
        lcd.drawString("Tekan tepat di tengah tanda +, tahan 1 detik", w / 2, h / 2 + 40);
        tandaKalibrasi(px[i], py[i], TFT_WHITE);
        if (ambilTekanStabil(false, x, y)) break;
        tulisKalibrasi("TEKAN LAGI", "Tahan 1 detik, jangan digeser", TFT_YELLOW); delay(1200);
      }
      tandaKalibrasi(px[i], py[i], TFT_GREEN); delay(250);
      mx[i] = x; my[i] = y;
      float a = px[i], b = py[i]; keArah0(a, b); tx[i] = a; ty[i] = b;
    }
    float x0 = mx[0], x1 = mx[0], y0 = my[0], y1 = my[0];
    for (int i = 1; i < 5; i++) { x0 = min(x0, mx[i]); x1 = max(x1, mx[i]); y0 = min(y0, my[i]); y1 = max(y1, my[i]); }
    if (x1 - x0 < 800 || y1 - y0 < 800 || !hitungKalibrasi(mx, my, tx, ty, 5, aff)) {
      tulisKalibrasi("ULANGI KALIBRASI", "Tekanan tidak terbaca benar. Cek kabel T_", TFT_RED); delay(3000); continue;
    }
    float terburuk = 0;
    for (int i = 0; i < 5; i++) {
      float ex = aff[0] * mx[i] + aff[1] * my[i] + aff[2] - tx[i];
      float ey = aff[3] * mx[i] + aff[4] * my[i] + aff[5] - ty[i];
      terburuk = max(terburuk, sqrtf(ex * ex + ey * ey));
    }
    if (terburuk > 12) { tulisKalibrasi("ULANGI KALIBRASI", "Tekan tepat di tengah tanda +", TFT_RED); delay(3000); continue; }
    lcd.panel()->setCalibrateAffine(aff);
    int qx = w * 3 / 4, qy = h / 3;                                  // titik uji
    while (true) {
      lcd.fillScreen(TFT_BLACK); lcd.setTextColor(TFT_WHITE, TFT_BLACK);
      lcd.drawString("UJI: tekan tanda + ini", w / 2, h * 2 / 3);
      tandaKalibrasi(qx, qy, TFT_CYAN);
      if (ambilTekanStabil(true, x, y)) break;
      tulisKalibrasi("TEKAN LAGI", "Tahan 1 detik, jangan digeser", TFT_YELLOW); delay(1200);
    }
    if (sqrtf((float)(x - qx) * (x - qx) + (float)(y - qy) * (y - qy)) > 15) {
      tulisKalibrasi("ULANGI KALIBRASI", "Titik uji meleset", TFT_RED); delay(3000); continue;
    }
    break;
  }
  pref.putBytes("aff", aff, sizeof(aff));
  if (pref.isKey("cal")) pref.remove("cal");
  pref.end();
  Serial.println("Kalibrasi sentuh lulus dan disimpan");
  lcd.fillScreen(TFT_BLACK);
}

// ======================= TAMPILAN (layar sentuh) =======================
// Beranda: wajah Zond, tombol BICARA dan MENU. MENU berisi 6 ubin: Musik, Info, Alat, Main, Pesan, Setelan.
// Tiap halaman digambar dulu di memori (sprite di PSRAM) lalu dikirim sekaligus, jadi tidak berkedip.
// Tombol dibuat besar karena layar sentuh resistif kurang presisi, dan perintah baru dijalankan saat jari DILEPAS
// (kecuali STOP: langsung saat ditekan).
// Tidak ada tombol gerak: orang yang menyentuh layar berdiri dekat robot. Saat robot bergerak sendiri
// (ikuti/jelajah), layar hanya menampilkan tombol berhenti, dan sentuhan di mana saja menghentikan robot.
#define WARNA(r, g, b) (uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))
const uint16_t W_BILAH = WARNA(18, 22, 28), W_TOMBOL = WARNA(42, 50, 62), W_MATI = WARNA(28, 32, 38), W_TEKAN = 0xFFFF,
               W_REDUP = WARNA(140, 150, 160), W_GARIS = WARNA(55, 65, 78), W_MERAH = WARNA(190, 30, 30),
               W_HIJAU = WARNA(20, 140, 60), W_BIRU = WARNA(30, 100, 200);
const uint16_t W_UBIN[6] = {WARNA(130, 60, 190), WARNA(30, 110, 200), WARNA(20, 140, 100), WARNA(220, 120, 20),
                            WARNA(190, 150, 20), WARNA(90, 100, 115)};
const int BILAH_H = 28, ISI_Y = 28, ISI_H = 212, MUKA_H = 144;
LGFX_Sprite bilah(&lcd), kanvas(&lcd), muka(&lcd), bawah(&lcd);   // muka dan bawah: khusus Beranda

enum { H_BERANDA, H_MENU, H_SUB, H_MUSIK, H_DAFTARLAGU, H_RADIO, H_STATUS, H_PESAN, H_SETELAN, H_MODE, H_HITUNG, H_BERGERAK,
       H_CUACA, H_SHOLAT, H_BERITA, H_BACA, H_KALENDER, H_KURS, H_WIKI };
// id tombol: 1-9 umum, 10-99 khusus halaman, 100 ke atas = baris daftar
enum { B_KEMBALI = 1, B_STOP, B_MUKA, B_BICARA, B_MENU, B_ATAS, B_BAWAH };
int halaman = H_BERANDA, halamanSub = 0, halamanHal = 0, tabStatus = 0;
bool gambarUlang = true, bilahUlang = true, abaikanSentuh = false, konfirmasiRestart = false, layarRedup = false;
unsigned long tSentuh = 0, tIsi = 0;
String toastTeks; unsigned long toastSampai = 0;
int hitungMode = 0; unsigned long hitungMulai = 0;              // mode yang menunggu hitung mundur (ikuti/jelajah)
uint8_t kecerahan = 100; bool redupMalam = true;                // hanya berlaku kalau LAYAR_LED diisi

struct TombolLayar { int16_t x, y, w, h, id; };
TombolLayar tombolLayar[48]; int jumlahTombolLayar = 0;
int tombolDitekan = -1;

// ---- pesan di layar (dikirim lewat Telegram /pesan atau panel web) ----
struct PesanLayar { char dari[24]; char isi[200]; uint32_t waktu; uint8_t dibaca; };
const int PESAN_MAKS = 10;
PesanLayar daftarPesan[PESAN_MAKS]; int jumlahPesan = 0, pesanDilihat = 0;
SemaphoreHandle_t kunciPesan;
volatile bool pesanMasuk = false;

void muatPesan() {
  Preferences q; q.begin("robot", true);
  size_t n = q.getBytesLength("psn");
  if (n && n % sizeof(PesanLayar) == 0 && n <= sizeof(daftarPesan)) { q.getBytes("psn", daftarPesan, n); jumlahPesan = n / sizeof(PesanLayar); }
  q.end();
}

void simpanPesan() {
  Preferences q; q.begin("robot", false);
  if (jumlahPesan) q.putBytes("psn", daftarPesan, jumlahPesan * sizeof(PesanLayar)); else q.remove("psn");
  q.end();
}

void tambahPesan(const String& dari, const String& isi) {
  xSemaphoreTake(kunciPesan, portMAX_DELAY);
  for (int i = min(jumlahPesan, PESAN_MAKS - 1); i > 0; i--) daftarPesan[i] = daftarPesan[i - 1];
  if (jumlahPesan < PESAN_MAKS) jumlahPesan++;
  strlcpy(daftarPesan[0].dari, dari.c_str(), sizeof(daftarPesan[0].dari));
  strlcpy(daftarPesan[0].isi, isi.c_str(), sizeof(daftarPesan[0].isi));
  time_t sk; time(&sk); daftarPesan[0].waktu = (uint32_t)sk; daftarPesan[0].dibaca = 0;
  simpanPesan();
  xSemaphoreGive(kunciPesan);
  pesanMasuk = true;
}

int pesanBelumDibaca() {
  int n = 0;
  for (int i = 0; i < jumlahPesan; i++) if (!daftarPesan[i].dibaca) n++;
  return n;
}

// ---- musik dari SD: daftar lagu, putar, berikutnya, acak, ulang ----
std::vector<String> laguSD;
int laguKe = -1, radioKe = -1;
bool acakLagu = false, ulangLagu = false;
volatile bool putarSekali = false;                         // adzan: jangan lanjut ke lagu berikutnya setelah selesai

void muatDaftarLagu() {
  laguSD.clear();
  String d = daftarLagu();
  if (d.indexOf('\n') < 0) return;                         // pesan galat (SD tidak terbaca / belum ada lagu)
  int a = 0;
  while (a < (int)d.length()) {
    int b = d.indexOf('\n', a); if (b < 0) b = d.length();
    String n = d.substring(a, b); n.trim();
    if (n.length()) laguSD.push_back(n);
    a = b + 1;
  }
  std::sort(laguSD.begin(), laguSD.end());
}

void putarLaguKe(int i) {
  if (i < 0 || i >= (int)laguSD.size()) return;
  laguKe = i; radioKe = -1; putarSekali = false;
  kirimAudio(A_FILE, "/" + laguSD[i]);
  setStatus("Lagu " + String(i + 1) + "/" + String(laguSD.size()) + ": " + laguSD[i]);
}

// arah +1 = berikutnya, -1 = sebelumnya. Mengikuti lagu yang sedang diputar walau dimulai dari Telegram.
void laguBerikut(int arah) {
  if (laguSD.empty()) muatDaftarLagu();
  int n = laguSD.size();
  if (!n) { setStatus("Tidak ada lagu di SD card"); return; }
  if (laguFile[0]) for (int i = 0; i < n; i++) if (laguSD[i] == String(laguFile + 1)) { laguKe = i; break; }
  if (acakLagu && n > 1) { int j; do j = esp_random() % n; while (j == laguKe); putarLaguKe(j); }
  else putarLaguKe(laguKe < 0 ? 0 : (laguKe + arah + n) % n);
}

const int JUMLAH_RADIO = sizeof(DAFTAR_RADIO) / sizeof(DAFTAR_RADIO[0]);

void putarRadioKe(int i) {
  if (WiFi.status() != WL_CONNECTED) { setStatus("Radio butuh WiFi"); return; }
  radioKe = (i + JUMLAH_RADIO) % JUMLAH_RADIO; laguKe = -1;
  kirimAudio(A_URL, DAFTAR_RADIO[radioKe]);
  setStatus("Radio " + String(radioKe + 1) + "/" + String(JUMLAH_RADIO));
}

void hentikanSemua() {
  gantiMode(M_BIASA); kirimBadan("S"); kirimAudio(A_STOP);
  radioKe = -1; setStatus("Berhenti");
}

// ---- alat gambar ----
void toast(const String& t) { toastTeks = t; toastSampai = millis() + 2500; gambarUlang = true; }

void daftarkan(int id, int x, int y, int w, int h) {
  if (jumlahTombolLayar < 48) tombolLayar[jumlahTombolLayar++] = {(int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h, (int16_t)id};
}

// Huruf di luar ASCII (misalnya é) tidak ada di font layar: ganti dengan '?'.
String asciiSaja(const String& t) {
  String o; o.reserve(t.length());
  for (unsigned int i = 0; i < t.length(); i++) {
    uint8_t c = t[i];
    if (c < 0x80) { o += (char)c; continue; }
    if (c == 0xE2 && i + 2 < t.length() && (uint8_t)t[i + 1] == 0x80) {        // tanda baca "pintar"
      uint8_t d = t[i + 2];
      if (d == 0x98 || d == 0x99) { o += '\''; i += 2; continue; }
      if (d == 0x9C || d == 0x9D) { o += '"'; i += 2; continue; }
      if (d == 0x93 || d == 0x94) { o += '-'; i += 2; continue; }
      if (d == 0xA6) { o += "..."; i += 2; continue; }
    }
    if (c == 0xC2 && i + 1 < t.length() && (uint8_t)t[i + 1] == 0xA0) { o += ' '; i++; continue; }
    if ((c & 0xC0) == 0xC0) o += '?';                      // awal huruf UTF-8 lain; byte lanjutan dibuang
  }
  return o;
}

String potongLebar(LGFX_Sprite& s, const String& teks, int lebar) {
  String t = asciiSaja(teks);
  if (s.textWidth(t.c_str()) <= lebar) return t;
  while (t.length() > 1 && s.textWidth((t + "..").c_str()) > lebar) t.remove(t.length() - 1);
  return t + "..";
}

// Tulis teks dengan pindah baris di batas lebar. Kembalikan jumlah baris yang ditulis.
int tulisBungkus(LGFX_Sprite& s, const String& teks, int x, int y, int lebar, int tinggiBaris, int maksBaris) {
  String t = asciiSaja(teks), baris;
  int n = 0, i = 0;
  s.setTextDatum(top_left);
  while (i <= (int)t.length() && n < maksBaris) {
    int j = t.indexOf(' ', i); if (j < 0) j = t.length();
    String kata = t.substring(i, j);
    String coba = baris.length() ? baris + " " + kata : kata;
    if (s.textWidth(coba.c_str()) > lebar && baris.length()) {
      s.drawString(baris.c_str(), x, y + n * tinggiBaris); n++;
      baris = kata;
    } else baris = coba;
    i = j + 1;
  }
  if (baris.length() && n < maksBaris) { s.drawString(baris.c_str(), x, y + n * tinggiBaris); n++; }
  return n;
}

// Tombol di sprite s yang tampil mulai baris layar oy. Teks yang terlalu lebar memakai huruf lebih kecil.
void tombol(LGFX_Sprite& s, int oy, int id, int x, int y, int w, int h, const char* teks, uint16_t warna, bool aktif) {
  bool tekan = aktif && id == tombolDitekan;
  s.fillRoundRect(x, y, w, h, 8, tekan ? W_TEKAN : aktif ? warna : W_MATI);
  s.setTextColor(tekan ? TFT_BLACK : aktif ? TFT_WHITE : WARNA(95, 100, 108));
  s.setFont(&fonts::FreeSansBold9pt7b);
  if (s.textWidth(teks) > w - 8) s.setFont(&fonts::Font2);
  s.setTextDatum(middle_center);
  s.drawString(teks, x + w / 2, y + h / 2 + 1);
  if (aktif) daftarkan(id, x, y + oy, w, h);
}

uint16_t warnaIkon(int id) { return id == tombolDitekan ? TFT_BLACK : TFT_WHITE; }

// Ikon sederhana dari garis dan bidang (font layar tidak punya emoji).
enum { I_MUSIK, I_INFO, I_ALAT, I_MAIN, I_PESAN, I_SETELAN, I_PLAY, I_JEDA, I_MAJU, I_MUNDUR, I_MIK };
void ikon(LGFX_Sprite& s, int jenis, int cx, int cy, uint16_t w) {
  switch (jenis) {
    case I_MUSIK:   s.fillCircle(cx - 8, cy + 10, 7, w); s.fillCircle(cx + 12, cy + 6, 7, w);
                    s.fillRect(cx - 3, cy - 14, 4, 24, w); s.fillRect(cx + 17, cy - 18, 4, 24, w);
                    s.fillTriangle(cx - 3, cy - 14, cx + 21, cy - 18, cx + 21, cy - 10, w); break;
    case I_INFO:    s.drawCircle(cx, cy, 18, w); s.drawCircle(cx, cy, 17, w); s.fillCircle(cx, cy - 9, 3, w);
                    s.fillRect(cx - 2, cy - 3, 5, 14, w); break;
    case I_ALAT:    for (int i = 0; i < 4; i++) s.fillRoundRect(cx - 17 + (i % 2) * 19, cy - 17 + (i / 2) * 19, 15, 15, 3, w); break;
    case I_MAIN:    s.fillRoundRect(cx - 22, cy - 11, 44, 24, 10, w);
                    s.fillRect(cx - 15, cy - 1, 11, 3, TFT_BLACK); s.fillRect(cx - 11, cy - 5, 3, 11, TFT_BLACK);
                    s.fillCircle(cx + 9, cy - 3, 3, TFT_BLACK); s.fillCircle(cx + 15, cy + 3, 3, TFT_BLACK); break;
    case I_PESAN:   s.drawRect(cx - 20, cy - 13, 40, 27, w); s.drawRect(cx - 19, cy - 12, 38, 25, w);
                    s.drawLine(cx - 19, cy - 12, cx, cy + 2, w); s.drawLine(cx + 19, cy - 12, cx, cy + 2, w); break;
    case I_SETELAN: for (int i = 0; i < 8; i++) { float a = i * PI / 4; s.fillCircle(cx + cosf(a) * 15, cy + sinf(a) * 15, 4, w); }
                    s.fillCircle(cx, cy, 14, w); s.fillCircle(cx, cy, 6, TFT_BLACK); break;
    case I_PLAY:    s.fillTriangle(cx - 8, cy - 11, cx - 8, cy + 11, cx + 11, cy, w); break;
    case I_JEDA:    s.fillRect(cx - 9, cy - 11, 6, 22, w); s.fillRect(cx + 3, cy - 11, 6, 22, w); break;
    case I_MAJU:    s.fillTriangle(cx - 10, cy - 10, cx - 10, cy + 10, cx + 5, cy, w); s.fillRect(cx + 6, cy - 10, 4, 20, w); break;
    case I_MUNDUR:  s.fillTriangle(cx + 10, cy - 10, cx + 10, cy + 10, cx - 5, cy, w); s.fillRect(cx - 10, cy - 10, 4, 20, w); break;
    case I_MIK:     s.fillRoundRect(cx - 5, cy - 13, 10, 18, 5, w); s.drawArc(cx, cy, 10, 9, 0, 180, w);
                    s.fillRect(cx - 1, cy + 10, 3, 5, w); break;
  }
}

// Tampilan kamera langsung di layar, sentuh untuk menutup (paling lama 30 detik).
void lihatKameraDiLayar() {
  setStatus("Membuka kamera...");
  unsigned long t0 = millis();
  int32_t x, y;
  while (millis() - t0 < 30000) {
    xSemaphoreTake(kunciFoto, portMAX_DELAY);
    bool ok = ambilFoto("qvga");
    if (ok) { xSemaphoreTake(kunciSPI, portMAX_DELAY); lcd.drawJpg(fotoBuf, fotoLen, 0, ISI_Y, 320, ISI_H); xSemaphoreGive(kunciSPI); }
    xSemaphoreGive(kunciFoto);
    if (!ok) { setStatus("Kamera tidak bisa dihubungi"); toast("Kamera tidak bisa dihubungi"); return; }
    bacaBadan();
    xSemaphoreTake(kunciSPI, portMAX_DELAY); bool sentuh = lcd.getTouch(&x, &y); xSemaphoreGive(kunciSPI);
    if (millis() - t0 > 800 && sentuh) break;
  }
  for (int i = 0; i < 100; i++) {                           // tunggu jari diangkat
    xSemaphoreTake(kunciSPI, portMAX_DELAY); bool sentuh = lcd.getTouch(&x, &y); xSemaphoreGive(kunciSPI);
    if (!sentuh) break;
    delay(20);
  }
  tSentuh = millis();
  setStatus("Siap");
}

String judulHalaman() {
  const char* SUB[4] = {"Musik", "Info", "Alat", "Main"};
  switch (halaman) {
    case H_MENU: return "Menu";
    case H_SUB: return SUB[halamanSub];
    case H_MUSIK: return "Lagu SD";
    case H_DAFTARLAGU: return "Daftar lagu";
    case H_RADIO: return "Radio";
    case H_STATUS: return "Status robot";
    case H_PESAN: return "Pesan";
    case H_SETELAN: return "Setelan";
    case H_MODE: return "Mode gerak";
    case H_CUACA: return String("Cuaca ") + CUACA_KOTA;
    case H_SHOLAT: return "Jadwal sholat";
    case H_BERITA: return "Berita";
    case H_BACA: return "Berita";
    case H_KALENDER: return "Kalender";
    case H_KURS: return "Kurs rupiah";
    case H_WIKI: return "Wikipedia";
    default: return "";
  }
}

bool musikJalan() { return audioJalan && !sedangBicara; }

// ---- bilah status atas: kembali / jam, lalu pesan, musik, WiFi, baterai, STOP ----
void gambarBilah() {
  LGFX_Sprite& s = bilah;
  s.fillScreen(W_BILAH);
  bool ts = tombolDitekan == B_STOP;
  s.fillRoundRect(264, 2, 54, 24, 6, ts ? W_TEKAN : W_MERAH);
  s.setFont(&fonts::FreeSansBold9pt7b); s.setTextDatum(middle_center); s.setTextColor(ts ? TFT_BLACK : TFT_WHITE);
  s.drawString("STOP", 291, 15);
  int x = 258;
  s.setFont(&fonts::Font2); s.setTextDatum(middle_right); s.setTextColor(TFT_WHITE);
  if (persenBat >= 0) {
    uint16_t wb = persenBat <= 20 ? TFT_RED : persenBat <= 40 ? TFT_ORANGE : TFT_GREEN;
    s.drawRect(x - 22, 8, 20, 12, TFT_WHITE); s.fillRect(x - 2, 11, 2, 6, TFT_WHITE);
    s.fillRect(x - 20, 10, max(1, 16 * (int)persenBat / 100), 8, wb);
    char b[6]; snprintf(b, sizeof(b), "%d%%", (int)persenBat);
    s.drawString(b, x - 26, 15);
    x -= 26 + s.textWidth(b) + 8;
  }
  bool beranda = halaman == H_BERANDA || halaman == H_BERGERAK || halaman == H_HITUNG;
  if (beranda) {                                            // halaman lain: judul butuh tempat, cukup baterai dan STOP
  if (WiFi.status() == WL_CONNECTED) {
    int r = WiFi.RSSI(), bar = r > -60 ? 4 : r > -70 ? 3 : r > -80 ? 2 : 1;
    for (int i = 0; i < 4; i++) {
      int h = 4 + i * 3;
      if (i < bar) s.fillRect(x - 16 + i * 4, 21 - h, 3, h, TFT_WHITE); else s.drawRect(x - 16 + i * 4, 21 - h, 3, h, W_REDUP);
    }
  } else { s.setTextColor(TFT_RED); s.drawString("noWiFi", x, 15); x -= s.textWidth("noWiFi") - 16; s.setTextColor(TFT_WHITE); }
  x -= 24;
  if (pesanBelumDibaca() > 0) {
    s.drawRect(x - 18, 8, 18, 13, TFT_YELLOW); s.drawLine(x - 18, 8, x - 9, 15, TFT_YELLOW); s.drawLine(x - 1, 8, x - 9, 15, TFT_YELLOW);
    x -= 24;
  }
  if (musikJalan()) {
    s.fillCircle(x - 12, 18, 3, TFT_CYAN); s.fillRect(x - 10, 6, 2, 12, TFT_CYAN); s.fillRect(x - 10, 6, 7, 2, TFT_CYAN);
    x -= 18;
  }
  }
  s.setTextDatum(middle_left); s.setTextColor(TFT_WHITE);
  if (!beranda) {
    bool t = tombolDitekan == B_KEMBALI;
    s.fillRoundRect(2, 2, 54, 24, 6, t ? W_TEKAN : W_TOMBOL);
    s.fillTriangle(12, 14, 24, 6, 24, 22, t ? TFT_BLACK : TFT_WHITE);
    s.fillRect(24, 11, 20, 6, t ? TFT_BLACK : TFT_WHITE);
    s.setFont(&fonts::FreeSansBold9pt7b);
    s.drawString(potongLebar(s, judulHalaman(), x - 66).c_str(), 62, 15);
  } else {
    String j = teksJam();
    s.setFont(&fonts::FreeSansBold9pt7b);
    s.drawString(j.length() ? j.c_str() : "--:--", 6, 15);
    struct tm tt;
    if (jamSiap(tt) && x > 170) {
      char b[24]; snprintf(b, sizeof(b), "%.3s %d %.3s", NAMA_HARI[tt.tm_wday], tt.tm_mday, NAMA_BULAN[tt.tm_mon]);
      s.setFont(&fonts::Font2); s.setTextColor(W_REDUP); s.drawString(b, 62, 15);
    }
  }
  xSemaphoreTake(kunciSPI, portMAX_DELAY); s.pushSprite(0, 0); xSemaphoreGive(kunciSPI);
}

// ---- Beranda: wajah (dianimasikan) dan dua tombol besar ----
void gambarMuka() {
  static unsigned long tKedip = 0; static bool kedip = false;
  if (!kedip && millis() - tKedip > 3500 + (esp_random() % 2000)) { kedip = true; tKedip = millis(); }
  if (kedip && millis() - tKedip > 140) { kedip = false; tKedip = millis(); }
  LGFX_Sprite& s = muka;
  s.fillScreen(TFT_BLACK);
  Ekspresi e = ekspresi;
  struct tm tt;
  if (e == NGANTUK && jamSiap(tt)) {                       // malam dan lama tidak dipakai: jam besar
    char b[8]; snprintf(b, sizeof(b), "%02d:%02d", tt.tm_hour, tt.tm_min);
    s.setFont(&fonts::Font7); s.setTextDatum(middle_center); s.setTextColor(WARNA(90, 110, 130));
    s.drawString(b, 160, 58);
    s.setFont(&fonts::Font2); s.setTextColor(WARNA(70, 80, 90));
    s.drawString(teksTanggal().c_str(), 160, 104);
    int n = (millis() / 600) % 3;
    for (int i = 0; i <= n; i++) s.drawString("z", 262 + i * 10, 26 - i * 8);
  } else {
    uint16_t warna = (e == KAGET) ? TFT_ORANGE : (e == LEMAH || e == NGANTUK) ? 0x7BEF : (e == DENGAR) ? TFT_GREEN : (e == PUSING) ? 0xFD20 : TFT_CYAN;
    int geser = arahMata * 14;
    int tinggi = kedip ? 6 : (e == KAGET ? 62 : e == LEMAH ? 18 : e == NGANTUK ? 8 : 50);
    for (int sisi = -1; sisi <= 1; sisi += 2) {
      int cx = 160 + sisi * 62 + geser, cy = 50;
      if (e == PUSING) {
        int putaran = (millis() / 120) % 8;
        for (int r = 6; r <= 24; r += 6) s.drawCircle(cx + (putaran % 4 - 2) * (r / 6), cy + (putaran / 4) * 2, r, warna);
        continue;
      }
      int y = (e == NGANTUK) ? cy + 12 : cy;
      s.fillRoundRect(cx - 26, y - tinggi / 2, 52, tinggi, e == NGANTUK ? 4 : 14, warna);
      if (e == PIKIR && !kedip) s.fillCircle(cx + 10, cy - 10, 7, TFT_BLACK);
    }
    int my = 102;
    if (sedangBicara) { int h = 4 + (millis() / 90) % 3 * 6; s.fillRoundRect(130, my - h / 2, 60, h, 4, warna); }
    else if (e == KAGET) s.drawCircle(160, my, 9, warna);
    else if (e == PUSING) { for (int i = 0; i < 4; i++) s.drawLine(136 + i * 12, my + (i % 2 ? 4 : -4), 148 + i * 12, my + (i % 2 ? -4 : 4), warna); }
    else if (e == NGANTUK || e == LEMAH) s.drawFastHLine(144, my, 32, warna);
    else s.drawArc(160, my - 22, 30, 27, 50, 130, warna);
  }
  s.setFont(&fonts::Font2); s.setTextDatum(middle_center);
  if (pesanBelumDibaca() > 0) {
    s.setTextColor(TFT_YELLOW);
    s.drawString(potongLebar(s, String("Pesan dari ") + daftarPesan[0].dari + " - ketuk wajah untuk membaca", 310).c_str(), 160, 134);
  } else {
    s.setTextColor(TFT_WHITE);
    s.drawString(potongLebar(s, teksStatus, 310).c_str(), 160, 134);
  }
  xSemaphoreTake(kunciSPI, portMAX_DELAY); s.pushSprite(0, ISI_Y); xSemaphoreGive(kunciSPI);
}

void gambarBawahBeranda() {
  LGFX_Sprite& s = bawah;
  s.fillScreen(TFT_BLACK);
  jumlahTombolLayar = 0;
  int oy = ISI_Y + MUKA_H;
  bool dengar = ekspresi == DENGAR;
  tombol(s, oy, B_BICARA, 6, 6, 150, 56, dengar ? "  MENDENGAR" : "  BICARA", dengar ? WARNA(40, 190, 90) : W_HIJAU, true);
  ikon(s, I_MIK, 30, 34, warnaIkon(B_BICARA));
  tombol(s, oy, B_MENU, 164, 6, 150, 56, "  MENU", W_BIRU, true);
  for (int i = 0; i < 3; i++) s.fillRect(184, 24 + i * 9, 18, 4, warnaIkon(B_MENU));
  xSemaphoreTake(kunciSPI, portMAX_DELAY); s.pushSprite(0, oy); xSemaphoreGive(kunciSPI);
}

// ---- halaman menu dan sub-menu ----
struct ItemMenu { const char* nama; int16_t tujuan; };        // tujuan: halaman, -1 = menyusul, -2 kamera, -3 suit
const ItemMenu MENU_MUSIK[] = {{"Lagu SD", H_MUSIK}, {"Radio", H_RADIO}, {"Podcast", -1}, {"Murottal", -1}};
const ItemMenu MENU_INFO[]  = {{"Status robot", H_STATUS}, {"Cuaca", H_CUACA}, {"Jadwal sholat", H_SHOLAT}, {"Berita", H_BERITA},
                               {"Kalender", H_KALENDER}, {"Kurs", H_KURS}, {"Wikipedia", H_WIKI}};
const ItemMenu MENU_ALAT[]  = {{"Mode gerak", H_MODE}, {"Kamera", -2}, {"Alarm", -1}, {"Timer", -1}, {"Kalkulator", -1},
                               {"Senter", -1}, {"Penerjemah", -1}, {"Catatan", -1}, {"Rekam suara", -1}, {"Galeri foto", -1}};
const ItemMenu MENU_MAIN[]  = {{"Suit", -3}, {"Tic-tac-toe", -1}, {"2048", -1}, {"Ketuk terbang", -1}};
const ItemMenu* MENU_SUB[4] = {MENU_MUSIK, MENU_INFO, MENU_ALAT, MENU_MAIN};
const int JUMLAH_SUB[4] = {sizeof(MENU_MUSIK) / sizeof(ItemMenu), sizeof(MENU_INFO) / sizeof(ItemMenu),
                           sizeof(MENU_ALAT) / sizeof(ItemMenu), sizeof(MENU_MAIN) / sizeof(ItemMenu)};

// Tombol ▲ ▼ di baris bawah untuk daftar yang lebih dari satu halaman.
void navigasiHalaman(int jumlahHal) {
  tombol(kanvas, ISI_Y, B_ATAS, 6, 180, 96, 28, "", W_TOMBOL, halamanHal > 0);
  if (halamanHal > 0) kanvas.fillTriangle(54, 186, 44, 202, 64, 202, warnaIkon(B_ATAS));
  tombol(kanvas, ISI_Y, B_BAWAH, 218, 180, 96, 28, "", W_TOMBOL, halamanHal < jumlahHal - 1);
  if (halamanHal < jumlahHal - 1) kanvas.fillTriangle(256, 186, 276, 186, 266, 202, warnaIkon(B_BAWAH));
  kanvas.setFont(&fonts::Font2); kanvas.setTextDatum(middle_center); kanvas.setTextColor(W_REDUP);
  kanvas.drawString((String(halamanHal + 1) + " / " + String(jumlahHal)).c_str(), 160, 194);
}

void gambarMenu() {
  const char* NAMA[6] = {"Musik", "Info", "Alat", "Main", "Pesan", "Setelan"};
  const int IKON[6] = {I_MUSIK, I_INFO, I_ALAT, I_MAIN, I_PESAN, I_SETELAN};
  for (int i = 0; i < 6; i++) {
    int x = 6 + (i % 3) * 104, y = 6 + (i / 3) * 104, id = 10 + i;
    bool tekan = id == tombolDitekan;
    kanvas.fillRoundRect(x, y, 100, 98, 10, tekan ? W_TEKAN : W_UBIN[i]);
    ikon(kanvas, IKON[i], x + 50, y + 38, tekan ? TFT_BLACK : TFT_WHITE);
    kanvas.setFont(&fonts::FreeSansBold9pt7b); kanvas.setTextDatum(middle_center); kanvas.setTextColor(tekan ? TFT_BLACK : TFT_WHITE);
    kanvas.drawString(NAMA[i], x + 50, y + 80);
    if (i == 4 && pesanBelumDibaca() > 0) {
      kanvas.fillCircle(x + 86, y + 14, 11, TFT_RED);
      kanvas.setFont(&fonts::Font2); kanvas.setTextColor(TFT_WHITE);
      kanvas.drawString(String(pesanBelumDibaca()).c_str(), x + 86, y + 14);
    }
    daftarkan(id, x, y + ISI_Y, 100, 98);
  }
}

void gambarSub() {
  const ItemMenu* m = MENU_SUB[halamanSub];
  int n = JUMLAH_SUB[halamanSub], hal = (n + 5) / 6;
  halamanHal = constrain(halamanHal, 0, hal - 1);
  for (int k = 0; k < 6; k++) {
    int i = halamanHal * 6 + k; if (i >= n) break;
    int x = 6 + (k % 2) * 157, y = 6 + (k / 2) * 58, id = 100 + i;
    bool ada = m[i].tujuan != -1 && !(m[i].tujuan == -3 && !fiturKamera);
    bool tekan = id == tombolDitekan;
    kanvas.fillRoundRect(x, y, 151, 52, 8, tekan ? W_TEKAN : ada ? W_TOMBOL : W_MATI);
    kanvas.fillRect(x, y + 8, 5, 36, W_UBIN[halamanSub]);
    kanvas.setFont(&fonts::FreeSansBold9pt7b); kanvas.setTextDatum(middle_left);
    kanvas.setTextColor(tekan ? TFT_BLACK : ada ? TFT_WHITE : WARNA(110, 116, 124));
    kanvas.drawString(m[i].nama, x + 14, y + (ada ? 26 : 20));
    if (!ada) {
      kanvas.setFont(&fonts::Font2); kanvas.setTextColor(WARNA(110, 116, 124));
      kanvas.drawString(m[i].tujuan == -3 ? "nyalakan fitur kamera" : "menyusul", x + 14, y + 38);
    }
    daftarkan(id, x, y + ISI_Y, 151, 52);
  }
  if (hal > 1) navigasiHalaman(hal);
}

// Daftar baris (lagu, radio, setelan). teks(i) boleh berisi "label\tnilai": nilai ditulis rata kanan.
void gambarDaftar(int jumlah, int perHal, String (*teks)(int), bool (*sorot)(int)) {
  int hal = max(1, (jumlah + perHal - 1) / perHal);
  halamanHal = constrain(halamanHal, 0, hal - 1);
  for (int k = 0; k < perHal; k++) {
    int i = halamanHal * perHal + k; if (i >= jumlah) break;
    int y = 4 + k * 44, id = 100 + i;
    bool tekan = id == tombolDitekan;
    kanvas.fillRoundRect(6, y, 308, 40, 8, tekan ? W_TEKAN : (sorot && sorot(i)) ? W_HIJAU : W_TOMBOL);
    String t = teks(i), nilai;
    int tab = t.indexOf('\t');
    if (tab >= 0) { nilai = t.substring(tab + 1); t = t.substring(0, tab); }
    kanvas.setFont(&fonts::FreeSans9pt7b); kanvas.setTextColor(tekan ? TFT_BLACK : TFT_WHITE);
    int lebarNilai = 0;
    if (nilai.length()) {
      kanvas.setFont(&fonts::Font2); kanvas.setTextDatum(middle_right);
      kanvas.setTextColor(tekan ? TFT_BLACK : WARNA(170, 200, 230));
      String v = potongLebar(kanvas, nilai, 140);
      kanvas.drawString(v.c_str(), 304, y + 21);
      lebarNilai = kanvas.textWidth(v.c_str()) + 12;
      kanvas.setFont(&fonts::FreeSans9pt7b); kanvas.setTextColor(tekan ? TFT_BLACK : TFT_WHITE);
    }
    kanvas.setTextDatum(middle_left);
    kanvas.drawString(potongLebar(kanvas, t, 286 - lebarNilai).c_str(), 16, y + 21);
    daftarkan(id, 6, y + ISI_Y, 308, 40);
  }
  if (hal > 1) navigasiHalaman(hal);
}

String waktuMenit(uint32_t detik) { char b[12]; snprintf(b, sizeof(b), "%u:%02u", (unsigned)(detik / 60), (unsigned)(detik % 60)); return b; }

// ---- Musik: pemutar lagu SD ----
void gambarMusik() {
  bool adaLagu = !laguSD.empty();
  String judul = laguFile[0] ? String(laguFile + 1) : String(adaLagu ? "Pilih lagu atau tekan putar" : "Belum ada lagu di SD card");
  kanvas.setFont(&fonts::Font2); kanvas.setTextDatum(top_left); kanvas.setTextColor(W_REDUP);
  String atas = laguFile[0] ? (musikJeda ? "Dijeda" : musikJalan() ? "Sedang diputar" : "Berhenti") : String("Lagu SD");
  if (adaLagu && laguKe >= 0) atas += "  -  " + String(laguKe + 1) + " dari " + String(laguSD.size());
  kanvas.drawString(atas.c_str(), 10, 6);
  kanvas.setFont(&fonts::FreeSansBold12pt7b); kanvas.setTextColor(TFT_WHITE);
  kanvas.drawString(potongLebar(kanvas, judul, 300).c_str(), 10, 24);
  uint32_t d = laguDurasi, p = laguDetik;
  if (d && p > d) p = d;
  kanvas.fillRoundRect(10, 56, 300, 8, 4, W_GARIS);
  if (d) kanvas.fillRoundRect(10, 56, max(8, (int)(300 * p / d)), 8, 4, TFT_CYAN);
  kanvas.setFont(&fonts::Font2); kanvas.setTextColor(W_REDUP);
  kanvas.setTextDatum(top_left); kanvas.drawString(waktuMenit(p).c_str(), 10, 68);
  kanvas.setTextDatum(top_right); kanvas.drawString(d ? waktuMenit(d).c_str() : "--:--", 310, 68);
  // kendali
  tombol(kanvas, ISI_Y, 20, 6, 92, 58, 54, "", W_TOMBOL, adaLagu);
  ikon(kanvas, I_MUNDUR, 35, 119, adaLagu ? warnaIkon(20) : W_REDUP);
  tombol(kanvas, ISI_Y, 21, 70, 92, 70, 54, "", W_BIRU, adaLagu);
  ikon(kanvas, (musikJalan() && !musikJeda && laguFile[0]) ? I_JEDA : I_PLAY, 105, 119, adaLagu ? warnaIkon(21) : W_REDUP);
  tombol(kanvas, ISI_Y, 22, 146, 92, 58, 54, "", W_TOMBOL, adaLagu);
  ikon(kanvas, I_MAJU, 175, 119, adaLagu ? warnaIkon(22) : W_REDUP);
  tombol(kanvas, ISI_Y, 23, 210, 92, 50, 54, "ACAK", acakLagu ? W_HIJAU : W_TOMBOL, adaLagu);
  tombol(kanvas, ISI_Y, 24, 264, 92, 50, 54, "ULANG", ulangLagu ? W_HIJAU : W_TOMBOL, adaLagu);
  // volume dan daftar
  tombol(kanvas, ISI_Y, 25, 6, 156, 52, 50, "-", W_TOMBOL, true);
  kanvas.fillRoundRect(64, 176, 132, 10, 5, W_GARIS);
  kanvas.fillRoundRect(64, 176, max(10, 132 * volSekarang / 21), 10, 5, TFT_GREEN);
  kanvas.setFont(&fonts::Font2); kanvas.setTextDatum(top_center); kanvas.setTextColor(W_REDUP);
  kanvas.drawString(("Volume " + String(volSekarang)).c_str(), 130, 158);
  tombol(kanvas, ISI_Y, 26, 202, 156, 52, 50, "+", W_TOMBOL, true);
  tombol(kanvas, ISI_Y, 27, 258, 156, 56, 50, "DAFTAR", W_TOMBOL, adaLagu);
}

String teksLagu(int i) { return laguSD[i]; }
bool sorotLagu(int i) { return laguFile[0] && laguSD[i] == String(laguFile + 1); }
String teksRadio(int i) {
  String u = DAFTAR_RADIO[i];
  u.replace("http://", ""); u.replace("https://", "");
  int g = u.indexOf('/'); if (g > 0) u = u.substring(0, g);
  return "Radio " + String(i + 1) + "\t" + u;
}
bool sorotRadio(int i) { return radioKe == i && musikJalan(); }

void gambarRadio() {
  gambarDaftar(JUMLAH_RADIO, 3, teksRadio, sorotRadio);
  tombol(kanvas, ISI_Y, 30, 6, 136, 150, 40, "BERHENTI", W_MERAH, true);
  tombol(kanvas, ISI_Y, 25, 162, 136, 74, 40, "VOL -", W_TOMBOL, true);
  tombol(kanvas, ISI_Y, 26, 240, 136, 74, 40, "VOL +", W_TOMBOL, true);
}

// ---- Status robot: baterai, jarak, kemiringan / jaringan dan QR panel web ----
void gambarStatus() {
  tombol(kanvas, ISI_Y, 40, 6, 4, 151, 32, "ROBOT", tabStatus == 0 ? W_BIRU : W_TOMBOL, true);
  tombol(kanvas, ISI_Y, 41, 163, 4, 151, 32, "JARINGAN", tabStatus == 1 ? W_BIRU : W_TOMBOL, true);
  kanvas.setTextDatum(top_left);
  bool hidup = badan.terakhir && millis() - badan.terakhir < 2000;
  if (tabStatus == 0) {
    kanvas.setFont(&fonts::FreeSansBold9pt7b); kanvas.setTextColor(TFT_WHITE);
    String b = !hidup ? String("Badan belum terhubung") : persenBat >= 0 ? "Baterai " + String((int)persenBat) + "%  -  " + String(badan.baterai, 2) + " V"
                                                                           : String("Baterai ") + String(badan.baterai, 2) + " V";
    kanvas.drawString(b.c_str(), 10, 44);
    kanvas.setFont(&fonts::Font2); kanvas.setTextColor(W_REDUP);
    kanvas.drawString(sisaMenitBat >= 0 ? ("Perkiraan sisa " + teksDurasi(sisaMenitBat)).c_str() : "Perkiraan sisa muncul setelah 10 menit menyala", 10, 64);
    // grafik tegangan (paling lama di kiri)
    kanvas.drawRect(10, 82, 300, 40, W_GARIS);
    int n = jumlahRiwayat;
    if (n >= 2) {
      int sebelum = -1;
      for (int i = 0; i < n; i++) {
        float v = riwayatBat[(ujungRiwayat - n + i + RIWAYAT_BAT) % RIWAYAT_BAT] / 100.0f;
        int y = 120 - (int)(constrain((v - 6.4f) / 2.0f, 0.0f, 1.0f) * 36), x = 11 + i * 298 / (n - 1);
        if (sebelum >= 0) kanvas.drawLine(11 + (i - 1) * 298 / (n - 1), sebelum, x, y, TFT_GREEN);
        sebelum = y;
      }
    } else { kanvas.setTextDatum(middle_center); kanvas.drawString("grafik muncul setelah 2 menit", 160, 102); kanvas.setTextDatum(top_left); }
    // jarak depan dan belakang (bar 0-200 cm)
    const char* LBL[2] = {"Depan", "Belakang"};
    int jarak[2] = {badan.depan, badan.belakang};
    for (int k = 0; k < 2; k++) {
      int y = 132 + k * 22;
      kanvas.setTextColor(TFT_WHITE); kanvas.drawString(LBL[k], 10, y);
      kanvas.fillRoundRect(84, y + 3, 160, 10, 5, W_GARIS);
      if (hidup && jarak[k] > 0) {
        int w = constrain(jarak[k] * 160 / 200, 6, 160);
        kanvas.fillRoundRect(84, y + 3, w, 10, 5, jarak[k] < 25 ? TFT_RED : jarak[k] < 50 ? TFT_ORANGE : TFT_GREEN);
      }
      kanvas.setTextDatum(top_right);
      kanvas.drawString(hidup && jarak[k] > 0 ? (String(jarak[k]) + " cm").c_str() : "-", 310, y);
      kanvas.setTextDatum(top_left);
    }
    kanvas.setTextColor(W_REDUP);
    String l = hidup ? "Miring " + String(badan.pitch) + " / " + String(badan.roll) + " derajat" : String("");
    if (hidup && adaSensorSuhu()) l += "   Suhu " + String(badan.suhu, 0) + " C, lembap " + String(badan.lembap, 0) + "%";
    kanvas.drawString(l.c_str(), 10, 180);
  } else {
    if (WiFi.status() == WL_CONNECTED) {
      String url = "http://" + WiFi.localIP().toString() + "/";
      kanvas.fillRect(8, 42, 128, 128, TFT_WHITE);
      kanvas.qrcode(url.c_str(), 12, 46, 120, 3);
      kanvas.setFont(&fonts::Font2); kanvas.setTextColor(W_REDUP); kanvas.setTextDatum(top_center);
      kanvas.drawString("pindai dari HP", 72, 176);
      kanvas.setTextDatum(top_left); kanvas.setTextColor(TFT_WHITE);
      kanvas.setFont(&fonts::FreeSansBold9pt7b); kanvas.drawString("Panel web", 146, 44);
      kanvas.setFont(&fonts::Font2);
      kanvas.drawString(url.c_str(), 146, 68);
      kanvas.drawString(("atau http://" + namaHost() + ".local/").c_str(), 146, 86);
      kanvas.setTextColor(W_REDUP);
      kanvas.drawString(potongLebar(kanvas, "WiFi: " + WiFi.SSID(), 168).c_str(), 146, 116);
      kanvas.drawString(("Sinyal: " + String(WiFi.RSSI()) + " dBm").c_str(), 146, 134);
      kanvas.drawString(("Menyala " + teksDurasi(millis() / 60000)).c_str(), 146, 152);
    } else {
      kanvas.setFont(&fonts::FreeSansBold9pt7b); kanvas.setTextColor(TFT_WHITE); kanvas.setTextDatum(middle_center);
      kanvas.drawString("WiFi tidak tersambung", 160, 110);
    }
  }
}

// ---- Pesan ----
void gambarPesan() {
  if (!jumlahPesan) {
    kanvas.setFont(&fonts::FreeSansBold9pt7b); kanvas.setTextColor(TFT_WHITE); kanvas.setTextDatum(middle_center);
    kanvas.drawString("Belum ada pesan", 160, 80);
    kanvas.setFont(&fonts::Font2); kanvas.setTextColor(W_REDUP);
    kanvas.drawString("Kirim dari Telegram:  /pesan isi pesannya", 160, 110);
    kanvas.drawString("atau dari panel web", 160, 130);
    return;
  }
  xSemaphoreTake(kunciPesan, portMAX_DELAY);
  pesanDilihat = constrain(pesanDilihat, 0, jumlahPesan - 1);
  PesanLayar p = daftarPesan[pesanDilihat];
  if (!daftarPesan[pesanDilihat].dibaca) { daftarPesan[pesanDilihat].dibaca = 1; simpanPesan(); bilahUlang = true; }
  xSemaphoreGive(kunciPesan);
  kanvas.setTextDatum(top_left); kanvas.setFont(&fonts::FreeSansBold9pt7b); kanvas.setTextColor(TFT_YELLOW);
  kanvas.drawString(potongLebar(kanvas, String("Dari ") + p.dari, 230).c_str(), 10, 6);
  kanvas.setFont(&fonts::Font2); kanvas.setTextColor(W_REDUP); kanvas.setTextDatum(top_right);
  kanvas.drawString((String(pesanDilihat + 1) + " / " + String(jumlahPesan)).c_str(), 310, 8);
  if (p.waktu > 1700000000UL) {                             // jam sudah tersinkron saat pesan datang
    time_t w = p.waktu; struct tm t; localtime_r(&w, &t);
    char b[32]; snprintf(b, sizeof(b), "%d %.3s, %02d:%02d", t.tm_mday, NAMA_BULAN[t.tm_mon], t.tm_hour, t.tm_min);
    kanvas.setTextDatum(top_left); kanvas.drawString(b, 10, 28);
  }
  kanvas.drawRoundRect(6, 46, 308, 104, 8, W_GARIS);
  kanvas.setFont(&fonts::FreeSans9pt7b); kanvas.setTextColor(TFT_WHITE);
  tulisBungkus(kanvas, p.isi, 14, 54, 292, 19, 5);
  tombol(kanvas, ISI_Y, 50, 6, 156, 56, 50, "", W_TOMBOL, pesanDilihat > 0);
  if (pesanDilihat > 0) kanvas.fillTriangle(24, 181, 44, 169, 44, 193, warnaIkon(50));
  tombol(kanvas, ISI_Y, 51, 68, 156, 112, 50, "BACAKAN", W_BIRU, true);
  tombol(kanvas, ISI_Y, 52, 186, 156, 66, 50, "HAPUS", W_MERAH, true);
  tombol(kanvas, ISI_Y, 53, 258, 156, 56, 50, "", W_TOMBOL, pesanDilihat < jumlahPesan - 1);
  if (pesanDilihat < jumlahPesan - 1) kanvas.fillTriangle(296, 181, 276, 169, 276, 193, warnaIkon(53));
}

// ---- Setelan ----
const int JUMLAH_SETELAN = 10;
String teksSetelan(int i) {
  switch (i) {
    case 0: return String("Suara bicara\t") + (pakaiSuaraAI && strlen(SUARA_GEMINI) ? "Cowok (" SUARA_GEMINI ")" : "Google");
    case 1: return String("Otak AI\t") + namaAI[pilihanAI];
    case 2: return String("Fitur kamera AI\t") + (fiturKamera ? "Nyala" : "Mati");
    case 3: return String("Kecerahan\t") + (LAYAR_LED >= 0 ? String(kecerahan) + "%" : String("perlu kabel LED ke IO38"));
    case 4: return String("Redup malam hari\t") + (LAYAR_LED >= 0 ? (redupMalam ? "Nyala" : "Mati") : "perlu kabel LED");
    case 5: return "Putar layar 180 derajat\tketuk";
    case 6: return "Kalibrasi sentuh\tketuk";
    case 7: return "WiFi\t" + (WiFi.status() == WL_CONNECTED ? WiFi.SSID() : String("tidak tersambung"));
    case 8: return "Alamat panel\t" + WiFi.localIP().toString();
    default: return String("Mulai ulang robot\t") + (konfirmasiRestart ? "ketuk sekali lagi" : "ketuk");
  }
}

void gambarSetelan() { gambarDaftar(JUMLAH_SETELAN, 4, teksSetelan, nullptr); }

void aturKecerahan() {
  struct tm t;
  bool malam = redupMalam && jamSiap(t) && (t.tm_hour >= 22 || t.tm_hour < 5) && millis() - tSentuh > 120000;
  layarRedup = malam;
  static int lalu = -1;
  int v = malam ? 15 : kecerahan;
  if (v != lalu) { lalu = v; xSemaphoreTake(kunciSPI, portMAX_DELAY); lcd.setBrightness(v * 255 / 100); xSemaphoreGive(kunciSPI); }
}

// ---- Mode gerak (dengan hitung mundur untuk mode yang membuat robot berjalan) ----
void gambarMode() {
  const char* NAMA[4] = {"BIASA", "JAGA", "IKUTI", "JELAJAH"};
  const int MODE_KE[4] = {M_BIASA, M_JAGA, M_IKUTI, M_JELAJAH};
  for (int i = 0; i < 4; i++)
    tombol(kanvas, ISI_Y, 60 + i, 6 + (i % 2) * 157, 6 + (i / 2) * 76, 151, 70, NAMA[i], mode == MODE_KE[i] ? W_HIJAU : W_TOMBOL, true);
  kanvas.setFont(&fonts::Font2); kanvas.setTextColor(W_REDUP); kanvas.setTextDatum(top_left);
  kanvas.drawString("Jaga: diam, kirim foto kalau ada yang lewat.", 10, 162);
  kanvas.drawString("Ikuti dan Jelajah mulai 5 detik setelah ditekan.", 10, 182);
}

void gambarHitung() {
  int sisa = 5 - (int)((millis() - hitungMulai) / 1000);
  kanvas.fillScreen(WARNA(60, 40, 0));
  kanvas.setFont(&fonts::Font7); kanvas.setTextDatum(middle_center); kanvas.setTextColor(TFT_YELLOW);
  kanvas.drawString(String(max(sisa, 1)).c_str(), 160, 50);
  kanvas.setFont(&fonts::FreeSansBold9pt7b); kanvas.setTextColor(TFT_WHITE);
  kanvas.drawString(hitungMode == M_JELAJAH ? "Jelajah dimulai" : "Ikuti dimulai", 160, 98);
  kanvas.setFont(&fonts::Font2);
  kanvas.drawString("Lepaskan tangan dan beri ruang di sekitar robot", 160, 120);
  tombol(kanvas, ISI_Y, 70, 60, 144, 200, 56, "BATAL", W_MERAH, true);
}

void gambarBergerak() {
  kanvas.fillScreen(WARNA(120, 15, 15));
  kanvas.setFont(&fonts::FreeSansBold12pt7b); kanvas.setTextDatum(middle_center); kanvas.setTextColor(TFT_WHITE);
  kanvas.drawString("SENTUH LAYAR", 160, 70);
  kanvas.drawString("UNTUK BERHENTI", 160, 100);
  kanvas.setFont(&fonts::Font2);
  String m = mode == M_JELAJAH ? "Mode jelajah, sisa " + waktuMenit(max(0L, (long)(jelajahSampai - millis()) / 1000)) : String("Mode ikuti");
  kanvas.drawString(m.c_str(), 160, 140);
  kanvas.drawString(potongLebar(kanvas, teksStatus, 300).c_str(), 160, 164);
}

// ---- halaman Info: cuaca, jadwal sholat, berita, kalender, kurs, Wikipedia ----
std::vector<String> barisTeks;                       // hasil pecahBaris()

// Pecah teks jadi baris-baris selebar `lebar` piksel dengan font yang sedang dipakai sprite s.
void pecahBaris(LGFX_Sprite& s, const String& teks, int lebar) {
  barisTeks.clear();
  String t = asciiSaja(teks), baris;
  int i = 0, n = t.length();
  while (i <= n) {
    int j = t.indexOf(' ', i); if (j < 0) j = n;
    String kata = t.substring(i, j);
    String coba = baris.length() ? baris + " " + kata : kata;
    if (s.textWidth(coba.c_str()) > lebar && baris.length()) { barisTeks.push_back(baris); baris = kata; }
    else baris = coba;
    i = j + 1;
  }
  if (baris.length()) barisTeks.push_back(baris);
}

// Ikon cuaca dari kode WMO: matahari, awan, hujan, petir, kabut. r = ukuran.
void ikonCuaca(LGFX_Sprite& s, int kode, int cx, int cy, int r) {
  uint16_t kuning = TFT_YELLOW, awan = WARNA(200, 210, 220), biru = WARNA(80, 160, 255);
  bool matahari = kode <= 2, berawan = kode >= 1;
  if (matahari) {
    int mx = kode == 0 ? cx : cx - r / 3, my = kode == 0 ? cy : cy - r / 3, rr = kode == 0 ? r * 5 / 10 : r * 4 / 10;
    for (int i = 0; i < 8; i++) { float a = i * PI / 4; s.drawLine(mx + cosf(a) * (rr + 3), my + sinf(a) * (rr + 3), mx + cosf(a) * (rr + r / 4), my + sinf(a) * (rr + r / 4), kuning); }
    s.fillCircle(mx, my, rr, kuning);
  }
  if (berawan) {
    uint16_t w = kode >= 3 ? WARNA(150, 160, 170) : awan;
    s.fillCircle(cx - r / 3, cy + r / 6, r / 3, w); s.fillCircle(cx + r / 6, cy - r / 8, r * 4 / 10, w);
    s.fillCircle(cx + r / 2, cy + r / 6, r / 3, w); s.fillRoundRect(cx - r * 2 / 3, cy + r / 6, r * 3 / 2, r / 3, r / 6, w);
  }
  if ((kode >= 51 && kode <= 67) || (kode >= 80 && kode <= 82))
    for (int i = 0; i < 3; i++) s.drawLine(cx - r / 3 + i * r / 3, cy + r * 6 / 10, cx - r / 2 + i * r / 3, cy + r * 9 / 10, biru);
  if (kode >= 95) s.fillTriangle(cx, cy + r / 2, cx - r / 6, cy + r, cx + r / 5, cy + r * 7 / 10, kuning);
  if (kode == 45 || kode == 48) for (int i = 0; i < 3; i++) s.drawFastHLine(cx - r * 2 / 3, cy + r / 2 + i * 5, r * 4 / 3, awan);
}

void tulisMemuat(const char* apa) {
  kanvas.setFont(&fonts::FreeSansBold9pt7b); kanvas.setTextColor(TFT_WHITE); kanvas.setTextDatum(middle_center);
  kanvas.drawString(apa, 160, 90);
  xSemaphoreTake(kunciInfo, portMAX_DELAY); String g = galatInfo; xSemaphoreGive(kunciInfo);
  if (g.length()) {
    kanvas.setFont(&fonts::Font2); kanvas.setTextColor(TFT_ORANGE);
    kanvas.drawString(potongLebar(kanvas, g, 300).c_str(), 160, 120);
  }
}

const char* HARI_PENDEK[7] = {"Min", "Sen", "Sel", "Rab", "Kam", "Jum", "Sab"};

void gambarCuaca() {
  xSemaphoreTake(kunciInfo, portMAX_DELAY);
  Cuaca c = cuaca;
  xSemaphoreGive(kunciInfo);
  if (!c.ada) { tulisMemuat(WiFi.status() == WL_CONNECTED ? "Memuat cuaca..." : "Cuaca butuh WiFi"); return; }
  ikonCuaca(kanvas, c.kode, 40, 34, 34);
  kanvas.setFont(&fonts::Font7); kanvas.setTextDatum(top_left); kanvas.setTextColor(TFT_WHITE);
  char b[48]; snprintf(b, sizeof(b), "%.0f", c.suhu);
  kanvas.drawString(b, 86, 6);
  int xs = 86 + kanvas.textWidth(b) + 4;
  kanvas.drawCircle(xs + 4, 10, 4, TFT_WHITE);
  kanvas.setFont(&fonts::FreeSansBold9pt7b); kanvas.drawString("C", xs + 10, 6);
  kanvas.setFont(&fonts::FreeSansBold9pt7b); kanvas.setTextColor(TFT_WHITE);
  String nama = namaCuaca(c.kode); nama.setCharAt(0, toupper(nama[0]));
  int xk = max(186, xs + 30);                              // kolom kanan, setelah angka suhu
  kanvas.drawString(potongLebar(kanvas, nama, 314 - xk).c_str(), xk, 6);
  kanvas.setFont(&fonts::Font2); kanvas.setTextColor(W_REDUP);
  snprintf(b, sizeof(b), "%.0f-%.0f C, lembap %d%%", c.terendah, c.tertinggi, c.lembap);
  kanvas.drawString(potongLebar(kanvas, b, 314 - xk).c_str(), xk, 28);
  snprintf(b, sizeof(b), "Peluang hujan %d%%", c.hujan);
  kanvas.drawString(potongLebar(kanvas, b, 314 - xk).c_str(), xk, 44);
  // 12 jam ke depan: batang peluang hujan, suhu, jam
  kanvas.drawFastHLine(6, 126, 308, W_GARIS);
  for (int i = 0; i < c.nJam && i < 12; i++) {
    int x = 8 + i * 25, h = c.jamHujan[i] * 40 / 100;
    if (h > 0) kanvas.fillRect(x + 4, 126 - h, 16, h, c.jamHujan[i] >= 50 ? WARNA(60, 140, 255) : WARNA(40, 80, 140));
    kanvas.setTextDatum(top_center); kanvas.setTextColor(TFT_WHITE);
    kanvas.drawString(String((int)roundf(c.jamSuhu[i])).c_str(), x + 12, 70);
    kanvas.setTextColor(W_REDUP);
    kanvas.drawString(String((c.jamAwal + i) % 24).c_str(), x + 12, 128);
  }
  // 3 hari ke depan
  struct tm t; bool jam = jamSiap(t);
  for (int i = 1; i <= 3 && i < 4; i++) {
    int x = 6 + (i - 1) * 104, y = 148;
    kanvas.fillRoundRect(x, y, 100, 60, 8, W_TOMBOL);
    kanvas.setFont(&fonts::FreeSansBold9pt7b); kanvas.setTextDatum(top_left); kanvas.setTextColor(TFT_WHITE);
    kanvas.drawString(i == 1 ? "Besok" : jam ? HARI_PENDEK[(t.tm_wday + i) % 7] : "-", x + 8, y + 6);
    ikonCuaca(kanvas, c.hariKode[i], x + 78, y + 18, 14);
    kanvas.setFont(&fonts::Font2); kanvas.setTextColor(W_REDUP);
    snprintf(b, sizeof(b), "%.0f-%.0f C", c.hariMin[i], c.hariMaks[i]); kanvas.drawString(b, x + 8, y + 26);
    snprintf(b, sizeof(b), "hujan %d%%", c.hariHujan[i]); kanvas.drawString(b, x + 8, y + 42);
  }
}

void gambarSholat() {
  xSemaphoreTake(kunciInfo, portMAX_DELAY);
  JadwalSholat j = sholat;
  xSemaphoreGive(kunciInfo);
  struct tm t;
  if (!j.ada || !jamSiap(t)) { tulisMemuat(WiFi.status() == WL_CONNECTED ? "Memuat jadwal sholat..." : "Jadwal sholat butuh WiFi"); return; }
  int sekarang = t.tm_hour * 60 + t.tm_min, k = -1;
  const int URUT[5] = {1, 3, 4, 5, 6};
  for (int u : URUT) if (menitDari(j.waktu[u]) > sekarang) { k = u; break; }
  kanvas.setTextDatum(top_left);
  if (k >= 0) {
    kanvas.setFont(&fonts::FreeSansBold12pt7b); kanvas.setTextColor(TFT_WHITE);
    kanvas.drawString((String(NAMA_SHOLAT[k]) + "  " + j.waktu[k]).c_str(), 10, 6);
    kanvas.setFont(&fonts::Font2); kanvas.setTextColor(TFT_GREEN);
    kanvas.drawString((teksDurasi(menitDari(j.waktu[k]) - sekarang) + " lagi").c_str(), 10, 32);
  } else {
    kanvas.setFont(&fonts::FreeSansBold9pt7b); kanvas.setTextColor(TFT_WHITE);
    kanvas.drawString("Sholat hari ini sudah lewat", 10, 10);
  }
  kanvas.setFont(&fonts::Font2); kanvas.setTextColor(W_REDUP); kanvas.setTextDatum(top_right);
  kanvas.drawString(j.hijri, 310, 8);
  kanvas.drawString(CUACA_KOTA, 310, 26);
  for (int i = 0; i < 7; i++) {
    int y = 52 + i * 18;
    bool lewat = menitDari(j.waktu[i]) <= sekarang, ini = i == k;
    if (ini) kanvas.fillRoundRect(6, y, 308, 17, 4, W_HIJAU);
    kanvas.setTextColor(ini ? TFT_WHITE : lewat ? WARNA(100, 108, 118) : TFT_WHITE);
    kanvas.setTextDatum(top_left); kanvas.drawString(NAMA_SHOLAT[i], 16, y + 1);
    kanvas.setTextDatum(top_right); kanvas.drawString(j.waktu[i], 304, y + 1);
  }
  const char* PENGINGAT[3] = {"Pengingat: mati", "Pengingat: suara", "Pengingat: suara + adzan"};
  tombol(kanvas, ISI_Y, 85, 6, 180, 308, 30, PENGINGAT[pengingatSholat % 3], pengingatSholat ? W_BIRU : W_TOMBOL, true);
}

// Judul tebal, keterangan kecil, lalu isi yang dibagi per halaman. Baris bawah: ▲, tombol tengah (dibuat pemanggil), ▼.
void gambarTeksPanjang(const String& judul, const String& ket, const String& isi) {
  kanvas.setFont(&fonts::FreeSansBold9pt7b); kanvas.setTextColor(TFT_WHITE); kanvas.setTextDatum(top_left);
  pecahBaris(kanvas, judul, 300);
  int y = 4;
  for (size_t i = 0; i < barisTeks.size() && i < 2; i++) { kanvas.drawString(barisTeks[i].c_str(), 10, y); y += 19; }
  if (ket.length()) { kanvas.setFont(&fonts::Font2); kanvas.setTextColor(W_REDUP); kanvas.drawString(potongLebar(kanvas, ket, 300).c_str(), 10, y); y += 17; }
  kanvas.drawFastHLine(10, y + 2, 300, W_GARIS);
  y += 6;
  kanvas.setFont(&fonts::FreeSans9pt7b); kanvas.setTextColor(TFT_WHITE);
  pecahBaris(kanvas, isi, 300);
  int per = max(1, (176 - y) / 19), hal = max(1, ((int)barisTeks.size() + per - 1) / per);
  halamanHal = constrain(halamanHal, 0, hal - 1);
  for (int i = 0; i < per; i++) {
    int k = halamanHal * per + i; if (k >= (int)barisTeks.size()) break;
    kanvas.drawString(barisTeks[k].c_str(), 10, y + i * 19);
  }
  tombol(kanvas, ISI_Y, B_ATAS, 6, 180, 60, 30, "", W_TOMBOL, halamanHal > 0);
  if (halamanHal > 0) kanvas.fillTriangle(36, 186, 26, 202, 46, 202, warnaIkon(B_ATAS));
  tombol(kanvas, ISI_Y, B_BAWAH, 254, 180, 60, 30, "", W_TOMBOL, halamanHal < hal - 1);
  if (halamanHal < hal - 1) kanvas.fillTriangle(274, 186, 294, 186, 284, 202, warnaIkon(B_BAWAH));
}

int beritaDipilih = 0;
void gambarBerita() {
  for (int i = 0; i < JUMLAH_SUMBER_BERITA; i++)
    tombol(kanvas, ISI_Y, 86 + i, 6 + i * 104, 2, 100, 28, SUMBER_BERITA[i][0], i == sumberBerita ? W_BIRU : W_TOMBOL, true);
  xSemaphoreTake(kunciInfo, portMAX_DELAY);
  int n = berita.size();
  if (!n) { xSemaphoreGive(kunciInfo); tulisMemuat(WiFi.status() == WL_CONNECTED ? "Memuat berita..." : "Berita butuh WiFi"); return; }
  const int PER = 3;
  int hal = (n + PER - 1) / PER;
  halamanHal = constrain(halamanHal, 0, hal - 1);
  for (int k = 0; k < PER; k++) {
    int i = halamanHal * PER + k; if (i >= n) break;
    int y = 34 + k * 48, id = 100 + i;
    bool tekan = id == tombolDitekan;
    kanvas.fillRoundRect(6, y, 308, 44, 8, tekan ? W_TEKAN : W_TOMBOL);
    kanvas.setFont(&fonts::Font2); kanvas.setTextColor(tekan ? TFT_BLACK : TFT_WHITE); kanvas.setTextDatum(top_left);
    pecahBaris(kanvas, berita[i].judul, 292);
    for (size_t b = 0; b < barisTeks.size() && b < 2; b++) {
      String s = barisTeks[b];
      if (b == 1 && barisTeks.size() > 2) s = potongLebar(kanvas, s + " " + barisTeks[2], 292);
      kanvas.drawString(s.c_str(), 14, y + 5 + b * 18);
    }
    daftarkan(id, 6, y + ISI_Y, 308, 44);
  }
  xSemaphoreGive(kunciInfo);
  if (hal > 1) navigasiHalaman(hal);
}

void gambarBaca() {
  xSemaphoreTake(kunciInfo, portMAX_DELAY);
  Berita b = beritaDipilih < (int)berita.size() ? berita[beritaDipilih] : Berita();
  xSemaphoreGive(kunciInfo);
  gambarTeksPanjang(b.judul, String(SUMBER_BERITA[sumberBerita][0]) + "  -  " + b.waktu, b.isi.length() ? b.isi : String("(tanpa ringkasan)"));
  tombol(kanvas, ISI_Y, 88, 72, 180, 176, 30, "BACAKAN", W_BIRU, true);
}

int kalTahun = 0, kalBulan = 0;                      // bulan yang ditampilkan kalender (1-12)
void gambarKalender() {
  struct tm t; bool ada = jamSiap(t);
  if (!kalTahun) { kalTahun = ada ? t.tm_year + 1900 : 2026; kalBulan = ada ? t.tm_mon + 1 : 1; }
  tombol(kanvas, ISI_Y, 80, 6, 2, 50, 28, "<", W_TOMBOL, true);
  tombol(kanvas, ISI_Y, 81, 264, 2, 50, 28, ">", W_TOMBOL, true);
  kanvas.setFont(&fonts::FreeSansBold9pt7b); kanvas.setTextDatum(middle_center); kanvas.setTextColor(TFT_WHITE);
  kanvas.drawString((String(NAMA_BULAN[kalBulan - 1]) + " " + String(kalTahun)).c_str(), 160, 16);
  struct tm awal = {}; awal.tm_year = kalTahun - 1900; awal.tm_mon = kalBulan - 1; awal.tm_mday = 1; awal.tm_hour = 12;
  mktime(&awal);
  int mulai = awal.tm_wday;
  struct tm akhir = {}; akhir.tm_year = kalTahun - 1900; akhir.tm_mon = kalBulan; akhir.tm_mday = 0; akhir.tm_hour = 12;
  mktime(&akhir);
  int jumlahHari = akhir.tm_mday;
  kanvas.setFont(&fonts::Font2);
  for (int d = 0; d < 7; d++) {
    kanvas.setTextColor(d == 0 ? TFT_RED : W_REDUP);
    kanvas.drawString(HARI_PENDEK[d], 28 + d * 44, 40);
  }
  uint32_t hariIni = ada ? angkaTanggal(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday) : 0;
  String keterangan[4]; int nKet = 0;
  for (int h = 1; h <= jumlahHari; h++) {
    int sel = mulai + h - 1, x = 28 + (sel % 7) * 44, y = 60 + (sel / 7) * 18;
    uint32_t tgl = angkaTanggal(kalTahun, kalBulan, h);
    int libur = jenisLibur(tgl);
    if (tgl == hariIni) kanvas.fillRoundRect(x - 17, y - 8, 34, 17, 6, W_BIRU);
    kanvas.setTextColor(libur == 1 || sel % 7 == 0 ? WARNA(255, 90, 90) : libur == 2 ? TFT_ORANGE : TFT_WHITE);
    kanvas.drawString(String(h).c_str(), x, y);
    if (libur && nKet < 4) {
      for (int i = 0; i < JUMLAH_LIBUR; i++)
        if (DAFTAR_LIBUR[i].tanggal == tgl) { keterangan[nKet++] = String(h) + "  " + DAFTAR_LIBUR[i].nama; break; }
    }
  }
  kanvas.setTextDatum(top_left);
  int y0 = 60 + ((mulai + jumlahHari - 1) / 7) * 18 + 12;
  if (kalTahun > TAHUN_LIBUR_TERAKHIR) {
    kanvas.setTextColor(W_REDUP); kanvas.drawString("Tanggal merah tahun ini belum ada di kode.", 10, y0);
  }
  for (int i = 0; i < nKet && y0 + i * 16 < 196; i++) {
    kanvas.setTextColor(keterangan[i].indexOf("Cuti") >= 0 ? TFT_ORANGE : WARNA(255, 120, 120));
    kanvas.drawString(potongLebar(kanvas, keterangan[i], 300).c_str(), 10, y0 + i * 16);
  }
}

void gambarKurs() {
  xSemaphoreTake(kunciInfo, portMAX_DELAY);
  Kurs k = kurs;
  xSemaphoreGive(kunciInfo);
  if (!k.ada) { tulisMemuat(WiFi.status() == WL_CONNECTED ? "Memuat kurs..." : "Kurs butuh WiFi"); return; }
  for (int i = 0; i < 8; i++) {
    int x = 6 + (i % 2) * 157, y = 4 + (i / 2) * 42;
    kanvas.fillRoundRect(x, y, 151, 38, 8, W_TOMBOL);
    kanvas.setFont(&fonts::FreeSansBold9pt7b); kanvas.setTextColor(TFT_CYAN); kanvas.setTextDatum(middle_left);
    kanvas.drawString((String(i == 5 ? "100 " : "") + KODE_KURS[i]).c_str(), x + 8, y + 20);
    kanvas.setTextColor(TFT_WHITE); kanvas.setTextDatum(middle_right);
    kanvas.drawString(("Rp" + angkaRibuan(k.idr[i] * (i == 5 ? 100 : 1))).c_str(), x + 144, y + 20);
  }
  kanvas.setFont(&fonts::Font2); kanvas.setTextColor(W_REDUP); kanvas.setTextDatum(top_left);
  if (k.diperbarui) {
    time_t w = k.diperbarui; struct tm t; localtime_r(&w, &t);
    char b[48]; snprintf(b, sizeof(b), "Diperbarui %d %.3s %d, %02d:%02d", t.tm_mday, NAMA_BULAN[t.tm_mon], t.tm_year + 1900, t.tm_hour, t.tm_min);
    kanvas.drawString(b, 10, 174);
  }
  kanvas.drawString("Sumber: ExchangeRate-API (open.er-api.com)", 10, 192);
}

void gambarWiki() {
  xSemaphoreTake(kunciInfo, portMAX_DELAY);
  String j = wikiJudul, isi = wikiIsi;
  xSemaphoreGive(kunciInfo);
  if (tujuanDengar == 1 || !j.length()) {
    kanvas.setFont(&fonts::FreeSansBold9pt7b); kanvas.setTextColor(TFT_WHITE); kanvas.setTextDatum(middle_center);
    kanvas.drawString(tujuanDengar == 1 ? "Sebutkan topiknya..." : "Cari apa saja di Wikipedia", 160, 50);
    kanvas.setFont(&fonts::Font2); kanvas.setTextColor(W_REDUP);
    kanvas.drawString("Ketuk tombol di bawah, lalu sebutkan topiknya,", 160, 76);
    kanvas.drawString("misalnya \"Gunung Rinjani\".", 160, 94);
    kanvas.drawString("Dari Telegram: /wiki topik", 160, 112);
    tombol(kanvas, ISI_Y, 89, 40, 136, 240, 56, tujuanDengar == 1 ? "MENDENGAR..." : "CARI DENGAN SUARA", W_HIJAU, tujuanDengar != 1);
    return;
  }
  gambarTeksPanjang(j, "Wikipedia", isi);
  tombol(kanvas, ISI_Y, 88, 72, 180, 86, 30, "BACAKAN", W_BIRU, true);
  tombol(kanvas, ISI_Y, 89, 162, 180, 86, 30, "CARI", W_HIJAU, true);
}

// ---- gambar isi halaman (selain Beranda) ----
void gambarIsi() {
  kanvas.fillScreen(TFT_BLACK);
  jumlahTombolLayar = 0;
  switch (halaman) {
    case H_MENU: gambarMenu(); break;
    case H_SUB: gambarSub(); break;
    case H_MUSIK: gambarMusik(); break;
    case H_DAFTARLAGU:
      if (laguSD.empty()) { kanvas.setFont(&fonts::FreeSansBold9pt7b); kanvas.setTextColor(TFT_WHITE); kanvas.setTextDatum(middle_center);
                            kanvas.drawString(adaSD ? "Belum ada lagu di SD card" : "SD card tidak terbaca", 160, 100); }
      else gambarDaftar(laguSD.size(), 4, teksLagu, sorotLagu);
      break;
    case H_RADIO: gambarRadio(); break;
    case H_STATUS: gambarStatus(); break;
    case H_PESAN: gambarPesan(); break;
    case H_SETELAN: gambarSetelan(); break;
    case H_MODE: gambarMode(); break;
    case H_HITUNG: gambarHitung(); break;
    case H_BERGERAK: gambarBergerak(); break;
    case H_CUACA: gambarCuaca(); break;
    case H_SHOLAT: gambarSholat(); break;
    case H_BERITA: gambarBerita(); break;
    case H_BACA: gambarBaca(); break;
    case H_KALENDER: gambarKalender(); break;
    case H_KURS: gambarKurs(); break;
    case H_WIKI: gambarWiki(); break;
  }
  if (millis() < toastSampai) {
    kanvas.setFont(&fonts::FreeSansBold9pt7b);
    int w = min(300, (int)kanvas.textWidth(asciiSaja(toastTeks).c_str()) + 28);
    kanvas.fillRoundRect(160 - w / 2, 150, w, 40, 10, WARNA(240, 240, 240));
    kanvas.setTextColor(TFT_BLACK); kanvas.setTextDatum(middle_center);
    kanvas.drawString(potongLebar(kanvas, toastTeks, w - 20).c_str(), 160, 171);
  }
  xSemaphoreTake(kunciSPI, portMAX_DELAY); kanvas.pushSprite(0, ISI_Y); xSemaphoreGive(kunciSPI);
}

void buka(int h) {
  if (h == H_MUSIK || h == H_DAFTARLAGU) { if (laguSD.empty()) muatDaftarLagu(); }
  if (h == H_DAFTARLAGU && laguKe >= 0) halamanHal = laguKe / 4; else if (h != halaman) halamanHal = 0;
  if (h == H_PESAN && pesanBelumDibaca() > 0) pesanDilihat = 0;
  if (h != H_SETELAN) konfirmasiRestart = false;
  if (h != H_WIKI) tujuanDengar = 0;
  uint8_t minta = 0;                                       // data Info yang belum ada atau sudah lama
  if (h == H_CUACA && !cuaca.ada) minta |= INFO_CUACA;
  if (h == H_SHOLAT && (!sholat.ada || sholat.tanggal != tanggalHariIni())) minta |= INFO_SHOLAT;
  if (h == H_BERITA && (berita.empty() || millis() - tBerita > 900000UL)) minta |= INFO_BERITA;
  if (h == H_KURS && (!kurs.ada || millis() - kurs.t > 21600000UL)) minta |= INFO_KURS;
  if (h == H_KALENDER && h != halaman) kalTahun = 0;     // kembali ke bulan ini
  if (minta) { aturGalat(""); mintaInfo |= minta; }
  halaman = h;
  tombolDitekan = -1; jumlahTombolLayar = 0;
  gambarUlang = bilahUlang = true;
}

void kembali() {
  switch (halaman) {
    case H_SUB: case H_PESAN: case H_SETELAN: buka(H_MENU); break;
    case H_MUSIK: case H_RADIO: halamanSub = 0; buka(H_SUB); halamanHal = 0; break;
    case H_DAFTARLAGU: buka(H_MUSIK); break;
    case H_STATUS: halamanSub = 1; buka(H_SUB); break;
    case H_MODE: halamanSub = 2; buka(H_SUB); break;
    case H_CUACA: case H_SHOLAT: case H_BERITA: case H_KALENDER: case H_KURS: case H_WIKI: halamanSub = 1; buka(H_SUB); break;
    case H_BACA: { int hl = beritaDipilih / 3; buka(H_BERITA); halamanHal = hl; break; }
    default: buka(H_BERANDA);
  }
}

void jalankanTombol(int id) {
  if (id == B_KEMBALI) { kembali(); return; }
  if (id == B_ATAS) { halamanHal--; gambarUlang = true; return; }
  if (id == B_BAWAH) { halamanHal++; gambarUlang = true; return; }
  switch (halaman) {
    case H_BERANDA:
      if (id == B_BICARA) { kirimAudio(A_STOP); mintaDengar = true; }
      else if (id == B_MENU) buka(H_MENU);
      else if (id == B_MUKA) { if (pesanBelumDibaca() > 0) buka(H_PESAN); else { lihatKameraDiLayar(); gambarUlang = bilahUlang = true; } }
      break;
    case H_MENU:
      if (id >= 10 && id <= 13) { halamanSub = id - 10; buka(H_SUB); }
      else if (id == 14) buka(H_PESAN);
      else if (id == 15) buka(H_SETELAN);
      break;
    case H_SUB: {
      int i = id - 100;
      if (i < 0 || i >= JUMLAH_SUB[halamanSub]) break;
      int t = MENU_SUB[halamanSub][i].tujuan;
      if (t >= 0) buka(t);
      else if (t == -2) { lihatKameraDiLayar(); gambarUlang = bilahUlang = true; }
      else if (t == -3) {
        if (!fiturKamera) toast("Nyalakan dulu Fitur kamera AI di Setelan");
        else { aksiTertunda = 2; toast("Tunjukkan tanganmu ke kamera"); }
      }
      else toast("Menyusul di tahap berikutnya");
      break;
    }
    case H_MUSIK:
      if (id == 20) laguBerikut(-1);
      else if (id == 21) { if (laguFile[0] && (musikJalan() || musikJeda)) kirimAudio(A_JEDA); else laguBerikut(laguKe < 0 ? 1 : 0); }
      else if (id == 22) laguBerikut(1);
      else if (id == 23) acakLagu = !acakLagu;
      else if (id == 24) ulangLagu = !ulangLagu;
      else if (id == 25 || id == 26) { int v = constrain(volSekarang + (id == 26 ? 2 : -2), 0, 21); kirimAudio(A_VOL, String(v)); volSekarang = v; }
      else if (id == 27) buka(H_DAFTARLAGU);
      gambarUlang = true;
      break;
    case H_DAFTARLAGU:
      if (id >= 100) { putarLaguKe(id - 100); buka(H_MUSIK); }
      break;
    case H_RADIO:
      if (id >= 100) putarRadioKe(id - 100);
      else if (id == 30) { kirimAudio(A_STOP); radioKe = -1; }
      else if (id == 25 || id == 26) { int v = constrain(volSekarang + (id == 26 ? 2 : -2), 0, 21); kirimAudio(A_VOL, String(v)); volSekarang = v; toast("Volume " + String(v)); }
      gambarUlang = true;
      break;
    case H_STATUS:
      if (id == 40 || id == 41) { tabStatus = id - 40; gambarUlang = true; }
      break;
    case H_PESAN:
      if (id == 50) pesanDilihat--;
      else if (id == 53) pesanDilihat++;
      else if (id == 51 && jumlahPesan) ucapkan(String("Pesan dari ") + daftarPesan[pesanDilihat].dari + ". " + daftarPesan[pesanDilihat].isi);
      else if (id == 52 && jumlahPesan) {
        xSemaphoreTake(kunciPesan, portMAX_DELAY);
        for (int i = pesanDilihat; i < jumlahPesan - 1; i++) daftarPesan[i] = daftarPesan[i + 1];
        jumlahPesan--; simpanPesan();
        xSemaphoreGive(kunciPesan);
        toast("Pesan dihapus"); bilahUlang = true;
      }
      gambarUlang = true;
      break;
    case H_SETELAN: {
      int i = id - 100;
      if (i != 9) konfirmasiRestart = false;
      if (i == 0) {
        if (!strlen(SUARA_GEMINI)) toast("Isi SUARA_GEMINI di kode dulu");
        else { aturSuara(!pakaiSuaraAI); ucapkan(pakaiSuaraAI ? "Ini suara cowokku." : "Ini suara Google."); }
      } else if (i == 1) {
        for (int k = 1; k <= AI_JUMLAH; k++) { int j = (pilihanAI + k) % AI_JUMLAH; if (aiTersedia(j)) { pilihAI(namaAI[j]); break; } }
      } else if (i == 2) aturFiturKamera(!fiturKamera);
      else if (i == 3 || i == 4) {
        if (LAYAR_LED < 0) toast("Pindah kabel LED layar ke IO38 dulu");
        else {
          if (i == 3) kecerahan = kecerahan <= 25 ? 100 : kecerahan - 25; else redupMalam = !redupMalam;
          Preferences q; q.begin("robot", false); q.putUChar("cerah", kecerahan); q.putUChar("redup", redupMalam); q.end();
        }
      } else if (i == 5) {
        Preferences q; q.begin("robot", false); bool p = !q.getUChar("putar", 0); q.putUChar("putar", p); q.end();
        xSemaphoreTake(kunciSPI, portMAX_DELAY); lcd.setRotation(lcd.getRotation() ^ 2); lcd.fillScreen(TFT_BLACK); xSemaphoreGive(kunciSPI);
        bilahUlang = true;
      } else if (i == 6) {
        xSemaphoreTake(kunciSPI, portMAX_DELAY); kalibrasiSentuh(true); xSemaphoreGive(kunciSPI);
        bilahUlang = true;
      } else if (i == 9) {
        if (konfirmasiRestart) { hentikanSemua(); delay(300); ESP.restart(); }
        konfirmasiRestart = true;
      }
      gambarUlang = true;
      break;
    }
    case H_MODE:
      if (id == 60) gantiMode(M_BIASA);
      else if (id == 61) gantiMode(M_JAGA);
      else if (id == 62 || id == 63) { hitungMode = id == 62 ? M_IKUTI : M_JELAJAH; hitungMulai = millis(); buka(H_HITUNG); }
      gambarUlang = true;
      break;
    case H_HITUNG:
      if (id == 70) buka(H_MODE);
      break;
    case H_SHOLAT:
      if (id == 85) {
        pengingatSholat = (pengingatSholat + 1) % 3;
        Preferences q; q.begin("robot", false); q.putUChar("sholat", pengingatSholat); q.end();
        if (pengingatSholat == 2) toast("Adzan diputar dari SD: /adzan.mp3");
        gambarUlang = true;
      }
      break;
    case H_BERITA:
      if (id >= 86 && id < 86 + JUMLAH_SUMBER_BERITA) {
        if (sumberBerita != id - 86) {
          sumberBerita = id - 86;
          xSemaphoreTake(kunciInfo, portMAX_DELAY); berita.clear(); xSemaphoreGive(kunciInfo);
          halamanHal = 0; aturGalat(""); mintaInfo |= INFO_BERITA;
        }
        gambarUlang = true;
      } else if (id >= 100) { beritaDipilih = id - 100; buka(H_BACA); }
      break;
    case H_BACA:
      if (id == 88) {
        xSemaphoreTake(kunciInfo, portMAX_DELAY);
        String t = beritaDipilih < (int)berita.size() ? berita[beritaDipilih].judul + ". " + berita[beritaDipilih].isi : String();
        xSemaphoreGive(kunciInfo);
        if (t.length()) ucapkan(t);
      }
      break;
    case H_KALENDER:
      if (id == 80) { if (--kalBulan < 1) { kalBulan = 12; kalTahun--; } }
      else if (id == 81) { if (++kalBulan > 12) { kalBulan = 1; kalTahun++; } }
      gambarUlang = true;
      break;
    case H_WIKI:
      if (id == 88) { xSemaphoreTake(kunciInfo, portMAX_DELAY); String t = wikiIsi; xSemaphoreGive(kunciInfo); if (t.length()) ucapkan(t); }
      else if (id == 89) { tujuanDengar = 1; kirimAudio(A_STOP); mintaDengar = true; halamanHal = 0; }
      gambarUlang = true;
      break;
  }
}

// Baca sentuhan. Sebuah tombol dianggap ditekan setelah terbaca sama 4 kali berturut-turut
// (sentuhan palsu dari bising speaker hanya sesaat), dan dijalankan saat jari diangkat.
int tombolDi(int x, int y) {
  if (y < BILAH_H) {
    if (x >= 258) return B_STOP;
    if (x < 62 && halaman != H_BERANDA && halaman != H_BERGERAK && halaman != H_HITUNG) return B_KEMBALI;
    return -1;
  }
  for (int i = 0; i < jumlahTombolLayar; i++) {
    const TombolLayar& t = tombolLayar[i];
    if (x >= t.x - 3 && x < t.x + t.w + 3 && y >= t.y - 3 && y < t.y + t.h + 3) return t.id;
  }
  if (halaman == H_BERANDA && y < ISI_Y + MUKA_H) return B_MUKA;
  return -1;
}

void tanganiSentuh() {
  int32_t x, y;
  xSemaphoreTake(kunciSPI, portMAX_DELAY); bool ada = lcd.getTouch(&x, &y); xSemaphoreGive(kunciSPI);
  int baca = ada ? tombolDi(x, y) : -2;                    // -2 = tidak disentuh, -1 = di luar tombol
  static int calon = -3, ulang = 0;
  static bool menempel = false;
  if (baca == calon) { if (ulang < 100) ulang++; } else { calon = baca; ulang = 1; }
  if (ulang < 4) return;
  if (calon != -2) {
    tSentuh = millis();
    if (!menempel) {
      menempel = true;
      Serial.printf("[sentuh] x %d, y %d, tombol %d\n", (int)x, (int)y, calon);
      tAktif = millis();                                     // layar dipakai: jangan pasang wajah mengantuk
      if (layarRedup) { layarRedup = false; abaikanSentuh = true; aturKecerahan(); return; }   // sentuhan pertama hanya membangunkan
      if (halaman == H_BERGERAK || calon == B_STOP) { hentikanSemua(); if (halaman == H_BERGERAK || halaman == H_HITUNG) buka(H_BERANDA); abaikanSentuh = true; return; }
    }
    if (abaikanSentuh) return;
    int t = calon >= 0 ? calon : -1;
    if (t != tombolDitekan) { tombolDitekan = t; gambarUlang = true; if (t == B_KEMBALI || t == B_STOP || t == -1) bilahUlang = true; }
  } else if (menempel) {
    menempel = false;
    int id = tombolDitekan; tombolDitekan = -1;
    bool abaikan = abaikanSentuh; abaikanSentuh = false;
    gambarUlang = true; if (id == B_KEMBALI) bilahUlang = true;
    if (!abaikan && id >= 0) jalankanTombol(id);
  }
}

// Kompatibel dengan panel web dan alarm: lagu berikutnya, radio berikutnya, dll.
enum { T_BICARA, T_LAGU, T_RADIO, T_VOLKURANG, T_VOLTAMBAH, T_STOP };
void tekanTombol(int t) {
  switch (t) {
    case T_BICARA: kirimAudio(A_STOP); mintaDengar = true; break;
    case T_LAGU: laguBerikut(1); break;
    case T_RADIO: putarRadioKe(radioKe + 1); break;
    case T_VOLKURANG: case T_VOLTAMBAH: {
      int v = constrain(volSekarang + (t == T_VOLTAMBAH ? 2 : -2), 0, 21);
      kirimAudio(A_VOL, String(v)); volSekarang = v; setStatus("Volume " + String(v) + " dari 21");
      break;
    }
    case T_STOP: hentikanSemua(); break;
  }
  gambarUlang = bilahUlang = true;
}

void gambarOTA() {
  kanvas.fillScreen(TFT_BLACK);
  kanvas.setFont(&fonts::FreeSansBold9pt7b); kanvas.setTextColor(TFT_WHITE); kanvas.setTextDatum(middle_center);
  kanvas.drawString("Memperbarui kode lewat WiFi", 160, 60);
  kanvas.drawRect(40, 86, 240, 20, TFT_CYAN);
  kanvas.fillRect(42, 88, 236 * otaPersen / 100, 16, TFT_CYAN);
  kanvas.setFont(&fonts::Font2);
  kanvas.drawString((String(otaPersen) + " %  -  jangan matikan robot").c_str(), 160, 126);
  xSemaphoreTake(kunciSPI, portMAX_DELAY); kanvas.pushSprite(0, ISI_Y); xSemaphoreGive(kunciSPI);
}

void siapkanLayar() {
  bilah.setPsram(true); bilah.setColorDepth(16); bilah.createSprite(320, BILAH_H);
  kanvas.setPsram(true); kanvas.setColorDepth(16); kanvas.createSprite(320, ISI_H);
  muka.setPsram(true); muka.setColorDepth(16); muka.createSprite(320, MUKA_H);
  bawah.setPsram(true); bawah.setColorDepth(16); bawah.createSprite(320, ISI_H - MUKA_H);
  Preferences q; q.begin("robot", true);
  kecerahan = constrain(q.getUChar("cerah", 100), 25, 100); redupMalam = q.getUChar("redup", 1) != 0;
  q.end();
  tSentuh = millis();
}

// Dipanggil terus dari loop(). Menggambar hanya yang perlu.
void perbaruiLayar() {
  static unsigned long tBilah = 0, tMuka = 0;
  static bool otaLalu = false;
  if (otaJalan) { otaLalu = true; static unsigned long tOta = 0; if (millis() - tOta > 300) { tOta = millis(); gambarOTA(); } return; }
  if (otaLalu) { otaLalu = false; gambarUlang = bilahUlang = true; }   // upload WiFi gagal: kembali seperti biasa
  bool bergerak = mode == M_IKUTI || mode == M_JELAJAH;
  if (bergerak && halaman != H_BERGERAK) buka(H_BERGERAK);
  else if (!bergerak && halaman == H_BERGERAK) buka(H_BERANDA);
  if (halaman == H_HITUNG && millis() - hitungMulai >= 5000) {
    if (hitungMode == M_JELAJAH) jelajah(5); else gantiMode(M_IKUTI);
    buka(H_BERGERAK);
  }
  bool tetap = halaman == H_BERANDA || halaman == H_BERGERAK || halaman == H_HITUNG || (halaman == H_MUSIK && musikJalan());
  if (!tetap && millis() - tSentuh > 45000) buka(H_BERANDA);
  if (pesanMasuk) {
    pesanMasuk = false;
    ucapkan(String("Ada pesan dari ") + daftarPesan[0].dari + ".");
    bilahUlang = gambarUlang = true;
  }
  if (infoBaru) { infoBaru = false; if (halaman >= H_CUACA) gambarUlang = true; }
  if (laguHabis) {
    laguHabis = false;
    if (putarSekali) putarSekali = false;
    else if (ulangLagu && laguKe >= 0) putarLaguKe(laguKe); else laguBerikut(1);
    gambarUlang = true;
  }
  if (LAYAR_LED >= 0) aturKecerahan();

  if (bilahUlang || millis() - tBilah > 1000) { bilahUlang = false; tBilah = millis(); gambarBilah(); }
  if (halaman == H_BERANDA) {
    if (millis() - tMuka > 110) { tMuka = millis(); gambarMuka(); }
    static bool dengarLalu = false;
    bool dengar = ekspresi == DENGAR;
    if (gambarUlang || dengar != dengarLalu) { gambarUlang = false; dengarLalu = dengar; gambarBawahBeranda(); }
    return;
  }
  unsigned long jeda = halaman == H_HITUNG ? 200 : (halaman == H_MUSIK || halaman == H_BERGERAK) ? 500 :
                       halaman == H_STATUS ? (tabStatus ? 5000 : 1000) : halaman == H_RADIO ? 2000 :
                       halaman == H_SHOLAT ? 10000 : 0;
  bool toastHabis = toastSampai && millis() >= toastSampai;
  if (toastHabis) toastSampai = 0;
  if (gambarUlang || toastHabis || (jeda && millis() - tIsi > jeda)) { gambarUlang = false; tIsi = millis(); gambarIsi(); }
}

// ======================= PANEL WEB =======================
// Buka http://<IP otak>/ dari HP atau laptop di WiFi yang sama: gerak tahan-jalan, kamera, kepala, musik.
// Halamannya ditulis dengan Svelte (folder _build/panel di repo panduan) lalu ditanam di sini sebagai satu string.
// <PANEL_WEB> dibuat otomatis oleh _build/panel/bangun.ts, jangan diedit di sini
const char PANEL_HTML[] = R"PANEL(<!doctype html><html lang="id"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1"><title>Panel Zond</title><style>html{color:#e6e9ee;background:#10141a;font-family:system-ui,-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,Noto Sans,Ubuntu,Cantarell,Helvetica Neue,sans-serif}body{margin:0}main.svelte-1xjvnya8h1aa3{max-width:480px;margin:0 auto;padding:12px 16px 32px}header.svelte-1xjvnya8h1aa3{display:flex;flex-wrap:wrap;align-items: center;gap:8px}h1.svelte-1xjvnya8h1aa3{margin:0;font-size:1.4rem}h2.svelte-1xjvnya8h1aa3{margin:0 0 8px;font-size:1rem}h2.svelte-1xjvnya8h1aa3 small:where(.svelte-1xjvnya8h1aa3){color:#8a94a3;font-weight:400}.kecil.svelte-1xjvnya8h1aa3{color:#8a94a3;font-size:.85rem}.titik.svelte-1xjvnya8h1aa3{background:#c0392b;border-radius:50%;width:10px;height:10px}.titik.on.svelte-1xjvnya8h1aa3{background:#2ecc71}.peringatan.svelte-1xjvnya8h1aa3{display:flex;flex-wrap:wrap;gap:6px;margin-top:8px}.peringatan.svelte-1xjvnya8h1aa3 span:where(.svelte-1xjvnya8h1aa3){background:#7a1d1d;border-radius:6px;padding:3px 8px;font-size:.85rem}section.svelte-1xjvnya8h1aa3{background:#18202a;border-radius:12px;margin-top:12px;padding:12px}.kamera.svelte-1xjvnya8h1aa3{text-align:center}.kamera.svelte-1xjvnya8h1aa3 img:where(.svelte-1xjvnya8h1aa3){display:block;aspect-ratio:4/3;object-fit:cover;background:#000;border-radius:8px;width:100%}.kamera.svelte-1xjvnya8h1aa3 img.redup:where(.svelte-1xjvnya8h1aa3){opacity:.15}.ket.svelte-1xjvnya8h1aa3{color:#8a94a3;margin:8px 0}.dpad.svelte-1xjvnya8h1aa3{display:grid;grid-template-columns:repeat(3,1fr);touch-action:none;user-select:none;-webkit-user-select:none;gap:8px;max-width:300px;margin:0 auto 12px}.dpad.svelte-1xjvnya8h1aa3 button:where(.svelte-1xjvnya8h1aa3){aspect-ratio:1;touch-action:none;background:#2a3a4d;border-radius:14px;font-size:1.6rem}.dpad.svelte-1xjvnya8h1aa3 button.aktif:where(.svelte-1xjvnya8h1aa3){background:#3d7bd9}.dpad.svelte-1xjvnya8h1aa3 button.stop:where(.svelte-1xjvnya8h1aa3){background:#a0302a}button.svelte-1xjvnya8h1aa3{color:inherit;cursor:pointer;background:#2a3a4d;border:0;border-radius:10px;padding:10px 12px;font-size:.95rem}button.svelte-1xjvnya8h1aa3:active{filter:brightness(1.3)}.kecil-btn.svelte-1xjvnya8h1aa3{margin-top:8px;padding:6px 10px;font-size:.85rem}.baris.svelte-1xjvnya8h1aa3{display:flex;flex-wrap:wrap;gap:8px}.baris.svelte-1xjvnya8h1aa3 button:where(.svelte-1xjvnya8h1aa3){flex:1}label.svelte-1xjvnya8h1aa3{display:block;margin-top:10px;font-size:.9rem}input[type=range].svelte-1xjvnya8h1aa3{width:100%}form.svelte-1xjvnya8h1aa3{display:flex;gap:8px;margin-top:12px}input[type=text].svelte-1xjvnya8h1aa3{color:inherit;background:#10141a;border:1px solid #2a3a4d;border-radius:10px;flex:1;min-width:0;padding:10px;font-size:1rem}.mik.svelte-1xjvnya8h1aa3{background:#1f6f3a;border-radius:14px;width:100%;padding:16px;font-size:1.1rem;font-weight:600}.mik.dengar.svelte-1xjvnya8h1aa3{animation:1s ease-in-out infinite svelte-1xjvnya8h1aa3-denyut;background:#c2410c}@keyframes svelte-1xjvnya8h1aa3-denyut{50%{filter:brightness(1.3)}}.mik.svelte-1xjvnya8h1aa3:disabled{opacity:.5}.saklar.svelte-1xjvnya8h1aa3{display:flex;color:#8a94a3;align-items: flex-start;gap:8px;font-size:.8rem}.saklar.svelte-1xjvnya8h1aa3 input:where(.svelte-1xjvnya8h1aa3){margin-top:2px}.ai.svelte-1xjvnya8h1aa3{display:grid;grid-template-columns:1fr 1fr;gap:8px}.ai.svelte-1xjvnya8h1aa3 button:where(.svelte-1xjvnya8h1aa3){text-align:left;display:flex;flex-direction:column;gap:2px}.ai.svelte-1xjvnya8h1aa3 button:where(.svelte-1xjvnya8h1aa3):first-child{grid-column:1/-1}.ai.svelte-1xjvnya8h1aa3 button:where(.svelte-1xjvnya8h1aa3) span:where(.svelte-1xjvnya8h1aa3){color:#8a94a3;font-size:.75rem}.ai.svelte-1xjvnya8h1aa3 button.pilih:where(.svelte-1xjvnya8h1aa3){outline:2px solid #5fcf72;background:#2f6a3a}.ai.svelte-1xjvnya8h1aa3 button.pilih:where(.svelte-1xjvnya8h1aa3) span:where(.svelte-1xjvnya8h1aa3){color:#cfe9d4}.ai.svelte-1xjvnya8h1aa3 button:where(.svelte-1xjvnya8h1aa3):disabled{opacity:.45;cursor:default}.kecil-ket.svelte-1xjvnya8h1aa3{margin-bottom:0;font-size:.8rem}.waktu.svelte-1xjvnya8h1aa3{color:#8a94a3;margin:6px 0 0;font-size:.85rem}.belok.svelte-1xjvnya8h1aa3{margin-top:10px}.alarm.svelte-1xjvnya8h1aa3{list-style:none;margin:0 0 8px;padding:0}.alarm.svelte-1xjvnya8h1aa3 li:where(.svelte-1xjvnya8h1aa3){display:flex;border-bottom:1px solid #243041;align-items: center;gap:10px;padding:8px 0}.alarm.svelte-1xjvnya8h1aa3 li:where(.svelte-1xjvnya8h1aa3) b:where(.svelte-1xjvnya8h1aa3){font-size:1.1rem}.alarm.svelte-1xjvnya8h1aa3 li:where(.svelte-1xjvnya8h1aa3) span:where(.svelte-1xjvnya8h1aa3){display:flex;flex-direction:column;flex:1}.alarm.svelte-1xjvnya8h1aa3 li:where(.svelte-1xjvnya8h1aa3) small:where(.svelte-1xjvnya8h1aa3){color:#8a94a3;font-size:.75rem}.hapus.svelte-1xjvnya8h1aa3{background:#3a2430;padding:4px 10px}.form-alarm.svelte-1xjvnya8h1aa3{flex-direction:column;gap:8px}.form-alarm.svelte-1xjvnya8h1aa3 .baris:where(.svelte-1xjvnya8h1aa3)>:where(.svelte-1xjvnya8h1aa3){flex:1}input[type=time].svelte-1xjvnya8h1aa3,select.svelte-1xjvnya8h1aa3{color:inherit;background:#10141a;border:1px solid #2a3a4d;border-radius:10px;min-width:0;padding:9px;font-size:.95rem}.hari.svelte-1xjvnya8h1aa3{display:flex;flex-wrap:wrap;gap:6px 12px;font-size:.85rem}.hari.svelte-1xjvnya8h1aa3 label:where(.svelte-1xjvnya8h1aa3){display:flex;align-items: center;gap:4px;margin:0}.jelajah.svelte-1xjvnya8h1aa3{margin-top:12px}.jelajah.svelte-1xjvnya8h1aa3 select:where(.svelte-1xjvnya8h1aa3){flex:none}.aktif-jelajah.svelte-1xjvnya8h1aa3{background:#3d7bd9}.grafik.svelte-1xjvnya8h1aa3{display:block;background:#10141a;border-radius:8px;width:100%;height:auto}.grafik.svelte-1xjvnya8h1aa3 line:where(.svelte-1xjvnya8h1aa3){stroke:#243041;stroke-width:1}.grafik.svelte-1xjvnya8h1aa3 text:where(.svelte-1xjvnya8h1aa3){fill:#8a94a3;font-size:9px}.grafik.svelte-1xjvnya8h1aa3 path:where(.svelte-1xjvnya8h1aa3){fill:none;stroke:#5fcf72;stroke-width:2;stroke-linejoin:round}.sumbu.svelte-1xjvnya8h1aa3{display:flex;color:#8a94a3;justify-content:space-between;margin-top:4px;font-size:.75rem}.suara.svelte-1xjvnya8h1aa3{display:flex;align-items: center;gap:8px}.suara.svelte-1xjvnya8h1aa3 select:where(.svelte-1xjvnya8h1aa3){flex:1}.status.svelte-1xjvnya8h1aa3{text-align:center;color:#8a94a3;margin-top:12px}
</style></head><body><script type="module">var p=!1;var mt=Array.isArray,Ka=Array.prototype.indexOf,Lt=Array.prototype.includes,Nr=Array.from,{keys:ci,defineProperty:he,getOwnPropertyDescriptor:ze,getOwnPropertyDescriptors:ui,prototype:pi}=Object,Wa=Array.prototype,{getPrototypeOf:Rr,isExtensible:di}=Object;var Et=()=>{};function Mn(e){for(var t=0;t<e.length;t++)e[t]()}function Ln(){var e,t,r=new Promise((n,o)=>{e=n,t=o});return{promise:r,resolve:e,reject:t}}function _i(e,t){if(Array.isArray(e))return e;if(t===void 0||!(Symbol.iterator in e))return Array.from(e);let r=[];for(let n of e)if(r.push(n),r.length===t)break;return r}var te=2,kt=4,zt=8,cr=16777216,Se=16,ge=32,He=64,jr=128,Fn=256,Fe=512,G=1024,F=2048,oe=4096,fe=8192,W=16384,Pe=32768,Ft=33554432,Ve=65536,Kt=131072,vi=262144,Tt=524288,Za=1048576,ht=33554432,Cr=2097152,Ke=4194304,it=8388608,We=Symbol("$state"),Pn=Symbol("component"),Bn=Symbol("legacy props"),Xa=Symbol(""),Yn=Symbol("proxy path"),Hn=Symbol("attributes"),sn=Symbol("class"),mi=Symbol("style"),ln=Symbol("text"),Wt=Symbol("form reset"),hi=Symbol("hmr anchor"),Pt=new class extends Error{name="StaleReactionError";message="The reaction that called `getAbortSignal()` was re-run or destroyed"},gi=!!globalThis.document?.contentType&&globalThis.document.contentType.includes("xml");var ur=3,at=8;var Ja=1,Qa=2,xi=4,es=8,ts=16;var bi=1,rs=2;var Or="[",Zt="[!",yi="[?",pr="]",Ze={};var Z=Symbol("uninitialized"),st=Symbol("filename"),$i=Symbol("hmr"),Vn="http://www.w3.org/1999/xhtml";var Bt="font-weight: bold",Yt="font-weight: normal";function ns(e){if(p)console.warn(`%c[svelte] await_reactivity_loss
%cDetected reactivity loss when reading \`${e}\`. This happens when state is read in an async function after an earlier \`await\`
https://svelte.dev/e/await_reactivity_loss`,Bt,Yt);else console.warn("https://svelte.dev/e/await_reactivity_loss")}function os(e,t){if(p)console.warn(`%c[svelte] await_waterfall
%cAn async derived, \`${e}\` (${t}) was not read immediately after it resolved. This often indicates an unnecessary waterfall, which can slow down your app
https://svelte.dev/e/await_waterfall`,Bt,Yt);else console.warn("https://svelte.dev/e/await_waterfall")}function is(){if(p)console.warn(`%c[svelte] derived_inert
%cReading a derived belonging to a now-destroyed effect may result in stale values
https://svelte.dev/e/derived_inert`,Bt,Yt);else console.warn("https://svelte.dev/e/derived_inert")}function as(e,t,r){if(p)console.warn(`%c[svelte] hydration_attribute_changed
%cThe \`${e}\` attribute on \`${t}\` changed its value between server and client renders. The client value, \`${r}\`, will be ignored in favour of the server value
https://svelte.dev/e/hydration_attribute_changed`,Bt,Yt);else console.warn("https://svelte.dev/e/hydration_attribute_changed")}function dr(e){if(p)console.warn(`%c[svelte] hydration_mismatch
%c${e?`Hydration failed because the initial UI does not match what was rendered on the server. The error occurred near ${e}`:"Hydration failed because the initial UI does not match what was rendered on the server"}
https://svelte.dev/e/hydration_mismatch`,Bt,Yt);else console.warn("https://svelte.dev/e/hydration_mismatch")}function ss(){if(p)console.warn(`%c[svelte] lifecycle_double_unmount
%cTried to unmount a component that was not mounted
https://svelte.dev/e/lifecycle_double_unmount`,Bt,Yt);else console.warn("https://svelte.dev/e/lifecycle_double_unmount")}function ls(){if(p)console.warn("%c[svelte] select_multiple_invalid_value\n%cThe `value` property of a `<select multiple>` element should be an array, but it received a non-array value. The selection will be kept as is.\nhttps://svelte.dev/e/select_multiple_invalid_value",Bt,Yt);else console.warn("https://svelte.dev/e/select_multiple_invalid_value")}function Un(e){if(p)console.warn(`%c[svelte] state_proxy_equality_mismatch
%cReactive \`$state(...)\` proxies and the values they proxy have different identities. Because of this, comparisons with \`${e}\` will produce unexpected results
https://svelte.dev/e/state_proxy_equality_mismatch`,Bt,Yt);else console.warn("https://svelte.dev/e/state_proxy_equality_mismatch")}function fs(){if(p)console.warn("%c[svelte] svelte_boundary_reset_noop\n%cA `<svelte:boundary>` `reset` function only resets the boundary the first time it is called\nhttps://svelte.dev/e/svelte_boundary_reset_noop",Bt,Yt);else console.warn("https://svelte.dev/e/svelte_boundary_reset_noop")}var w=!1;function pe(e){w=e}var j;function U(e){if(e===null)throw dr(),Ze;return j=e}function Oe(){return U(xe(j))}function S(e){if(!w)return;if(xe(j)!==null)throw dr(),Ze;j=e}function Xe(e=1){if(w){var t=e,r=j;while(t--)r=xe(r);j=r}}function At(e=!0){var t=0,r=j;while(!0){if(r.nodeType===at){var n=r.data;if(n===pr){if(t===0)return r;t-=1}else if(n===Or||n===Zt||n[0]==="["&&!isNaN(Number(n.slice(1))))t+=1}var o=xe(r);if(e)r.remove();r=o}}function _r(e){if(!e||e.nodeType!==at)throw dr(),Ze;return e.data}function Gn(e){return e===this.v}function wi(e,t){return e!=e?t==t:e!==t||e!==null&&typeof e==="object"||typeof e==="function"}function qn(e){return!wi(e,this.v)}function cs(e){if(p){let t=Error(`invariant_violation
An invariant violation occurred, meaning Svelte's internal assumptions were flawed. This is a bug in Svelte, not your app — please open an issue at https://github.com/sveltejs/svelte, citing the following message: "${e}"
https://svelte.dev/e/invariant_violation`);throw t.name="Svelte error",t}else throw Error("https://svelte.dev/e/invariant_violation")}function zn(e){if(p){let t=Error(`lifecycle_outside_component
\`${e}(...)\` can only be used during component initialisation
https://svelte.dev/e/lifecycle_outside_component`);throw t.name="Svelte error",t}else throw Error("https://svelte.dev/e/lifecycle_outside_component")}function ps(){if(p){let e=Error("async_derived_orphan\nCannot create a `$derived(...)` with an `await` expression outside of an effect tree\nhttps://svelte.dev/e/async_derived_orphan");throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/async_derived_orphan")}function Ei(){if(p){let e=Error("bind_invalid_checkbox_value\nUsing `bind:value` together with a checkbox input is not allowed. Use `bind:checked` instead\nhttps://svelte.dev/e/bind_invalid_checkbox_value");throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/bind_invalid_checkbox_value")}function ds(){if(p){let e=Error(`derived_references_self
A derived value cannot reference itself recursively
https://svelte.dev/e/derived_references_self`);throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/derived_references_self")}function ki(e,t,r){if(p){let n=Error(`each_key_duplicate
${r?`Keyed each block has duplicate key \`${r}\` at indexes ${e} and ${t}`:`Keyed each block has duplicate key at indexes ${e} and ${t}`}
https://svelte.dev/e/each_key_duplicate`);throw n.name="Svelte error",n}else throw Error("https://svelte.dev/e/each_key_duplicate")}function _s(e,t,r){if(p){let n=Error(`each_key_volatile
Keyed each block has key that is not idempotent — the key for item at index ${e} was \`${t}\` but is now \`${r}\`. Keys must be the same each time for a given item
https://svelte.dev/e/each_key_volatile`);throw n.name="Svelte error",n}else throw Error("https://svelte.dev/e/each_key_volatile")}function vs(e){if(p){let t=Error(`effect_in_teardown
\`${e}\` cannot be used inside an effect cleanup function
https://svelte.dev/e/effect_in_teardown`);throw t.name="Svelte error",t}else throw Error("https://svelte.dev/e/effect_in_teardown")}function ms(){if(p){let e=Error("effect_in_unowned_derived\nEffect cannot be created inside a `$derived` value that was not itself created inside an effect\nhttps://svelte.dev/e/effect_in_unowned_derived");throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/effect_in_unowned_derived")}function hs(e){if(p){let t=Error(`effect_orphan
\`${e}\` can only be used inside an effect (e.g. during component initialisation)
https://svelte.dev/e/effect_orphan`);throw t.name="Svelte error",t}else throw Error("https://svelte.dev/e/effect_orphan")}function gs(){if(p){let e=Error(`effect_update_depth_exceeded
Maximum update depth exceeded. This typically indicates that an effect reads and writes the same piece of state
https://svelte.dev/e/effect_update_depth_exceeded`);throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/effect_update_depth_exceeded")}function xs(){if(p){let e=Error(`hydration_failed
Failed to hydrate the application
https://svelte.dev/e/hydration_failed`);throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/hydration_failed")}function bs(e){if(p){let t=Error(`rune_outside_svelte
The \`${e}\` rune is only available inside \`.svelte\` and \`.svelte.js/ts\` files
https://svelte.dev/e/rune_outside_svelte`);throw t.name="Svelte error",t}else throw Error("https://svelte.dev/e/rune_outside_svelte")}function ys(){if(p){let e=Error("state_descriptors_fixed\nProperty descriptors defined on `$state` objects must contain `value` and always be `enumerable`, `configurable` and `writable`.\nhttps://svelte.dev/e/state_descriptors_fixed");throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/state_descriptors_fixed")}function $s(){if(p){let e=Error("state_prototype_fixed\nCannot set prototype of `$state` object\nhttps://svelte.dev/e/state_prototype_fixed");throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/state_prototype_fixed")}function ws(){if(p){let e=Error("state_unsafe_mutation\nUpdating state inside `$derived(...)`, `$inspect(...)` or a template expression is forbidden. If the value should not be reactive, declare it without `$state`\nhttps://svelte.dev/e/state_unsafe_mutation");throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/state_unsafe_mutation")}function Es(){if(p){let e=Error("svelte_boundary_reset_onerror\nA `<svelte:boundary>` `reset` function cannot be called while an error is still being handled\nhttps://svelte.dev/e/svelte_boundary_reset_onerror");throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/svelte_boundary_reset_onerror")}var ce=!1,Xt=!1,gt=!1;var fn=null;function Ie(e,t){return e.label=t,Wn(e.v,t),e}function Wn(e,t){return e?.[Yn]?.(t),e}function xt(e){let t=Error(),r=Lf();if(r.length===0)return null;return r.unshift(`
`),he(t,"stack",{value:r.join(`
`)}),he(t,"name",{value:e}),t}function Lf(){let e=Error.stackTraceLimit;Error.stackTraceLimit=1/0;let t=Error().stack;if(Error.stackTraceLimit=e,!t)return[];let r=t.split(`
`),n=[];for(let o=0;o<r.length;o++){let i=r[o],a=i.replaceAll("\\","/");if(i.trim()==="Error")continue;if(i.includes("validate_each_keys"))return[];if(a.includes("svelte/src/internal")||a.includes("node_modules/.vite"))continue;n.push(i)}return n}function Ts(e,t){if(!p)throw Error("invariant(...) was not guarded by if (DEV)");if(!e)cs(t)}var L=null;function Ht(e){L=e}var St=null;function Ir(e){St=e}var bt=null;function Zn(e){bt=e}function cn(e,t=!1,r){if(L={p:L,i:!1,c:null,e:null,s:e,x:null,r:k,l:Xt&&!t?{s:null,u:null,$:[]}:null},p)L.function=r,bt=r}function un(e){var t=L,r=t.e;if(r!==null){t.e=null;for(var n of r)Ti(n)}if(e!==void 0)t.x=e;if(t.i=!0,L=t.p,p)bt=L?.function??null;return Xn(e)}function Xn(e={}){return he(e,Pn,{value:!0}),e}function Nt(){return!Xt||L!==null&&L.l===null}var vr=[];function As(){var e=vr;vr=[],Mn(e)}function ue(e){if(vr.length===0&&!mr){var t=vr;queueMicrotask(()=>{if(t===vr)As()})}vr.push(e)}function Ss(){while(vr.length>0)As()}var Ff=~(F|oe|G);function q(e,t){e.f=e.f&Ff|t}function Dr(e){if((e.f&Fe)!==0||e.deps===null)q(e,G);else q(e,oe)}function Jn(e,t){if((e.f&F)!==0)t.set(e,F);else if((e.f&oe)!==0&&t.get(e)!==F)t.set(e,oe);q(e,G)}function Ns(e){return`{${e.toString().replace(/^async \(\) => /,"").replace(/^\(/,"").replace(/\$\.get\((.+?)\)/g,(t,r)=>r).replace(/\$\.track_reactivity_loss\((.+?)\)\(\)/g,(t,r)=>r).replace(/\)$/,"")}}`}var Rs=!1;function Qn(){if(!Rs)Rs=!0,document.addEventListener("reset",(e)=>{Promise.resolve().then(()=>{if(!e.defaultPrevented)for(let t of e.target.elements)t[Wt]?.()})},{capture:!0})}function Je(e){var t=N,r=k;de(null),_e(null);try{return e()}finally{de(t),_e(r)}}function dn(e,t,r,n=r){e.addEventListener(t,()=>Je(r));let o=e[Wt];if(o)e[Wt]=()=>{o(),n(!0)};else e[Wt]=()=>n(!0);Qn()}function eo(e,t,r,n){let o=Nt()?Lr:Fr;var i=e.filter((d)=>!d.settled),a=t.map(o);if(p)a.forEach((d,y)=>{d.label=t[y].toString().replace("() => ","").replaceAll("$.eager(() => ","$state.eager(").replace(/\$\.get\((.+?)\)/g,(R,b)=>b)});if(r.length===0&&i.length===0){n(a);return}var f=k,c=js(),h=i.length===1?i[0].promise:i.length>1?Promise.all(i.map((d)=>d.promise)):null;function m(d){if((f.f&W)!==0)return;c();try{n([...a,...d])}catch(y){Ue(y,f)}Mr()}var v=Ai();if(r.length===0){h.then(()=>m([])).finally(v);return}function _(){Promise.all(r.map((d)=>Ni(d))).then(m).catch((d)=>Ue(d,f)).finally(v)}if(h)h.then(()=>{if((f.f&W)!==0){v();return}c(),_(),Mr()});else _()}function js(){var e=k,t=N,r=L,n=T;if(p)var o=St;return function(a=!0){if(_e(e),de(t),Ht(r),a&&(e.f&W)===0)n?.activate(),n?.apply();if(p)Si(null),Ir(o)}}var Pf=!1;function Mr(e=!0){if(Pf=!1,_e(null),de(null),Ht(null),e)T?.deactivate();if(p)Si(null),Ir(null)}function Ai(){var e=k,t=e.b,r=T,n=!!t?.is_rendered();return t?.update_pending_count(1,r),r.increment(n,e),()=>{t?.update_pending_count(-1,r),r.decrement(n,e)}}var Ge=null;function Si(e){Ge=e}var _n=new Set;function Lr(e){var t=te|F;if(k!==null)k.f|=Tt;let r={ctx:L,deps:null,effects:null,equals:Gn,f:t,fn:e,reactions:null,rv:0,v:Z,wv:0,parent:k,ac:null};if(p&&gt)r.created=xt("created at");return r}var Pr=Symbol("obsolete");function Ni(e,t,r){let n=k;if(n===null)ps();var o=void 0,i=De(Z);if(p)i.label=t??Ns(e);var a=!N,f=new Set;if(Is(()=>{var c=k;if(p)Ge={effect:c,effect_deps:new Set,warned:!1};var h=Ln();o=h.promise;try{Promise.resolve(e()).then(h.resolve,(d)=>{if(d!==Pt)h.reject(d)}).finally(Mr)}catch(d){h.reject(d),Mr()}if(p){if(Ge){if(c.deps!==null)for(let d=0;d<Be;d+=1)Ge.effect_deps.add(c.deps[d]);if(ye!==null)for(let d=0;d<ye.length;d+=1)Ge.effect_deps.add(ye[d])}Ge=null}var m=T;if(a){if((c.f&Pe)!==0)var v=Ai();if(n.b?.is_rendered())m.async_deriveds.get(c)?.reject(Pr);else for(let d of f.values())d.reject(Pr);f.add(h),m.async_deriveds.set(c,h)}let _=(d,y=void 0)=>{if(p)Ge=null;if(v?.(),f.delete(h),y===Pr)return;if(m.activate(),y)i.f|=it,yt(i,y);else{if((i.f&it)!==0)i.f^=it;if(p&&r!==void 0&&!i.equals(d))_n.add(i),setTimeout(()=>{if(_n.has(i)&&(c.f&W)===0)os(i.label,r),_n.delete(i)});yt(i,d)}m.deactivate()};h.promise.then(_,(d)=>_(null,d||"unknown"))}),be(()=>{for(let c of f)c.reject(Pr)}),p)i.f|=Ke;return new Promise((c)=>{function h(m){function v(){if(m===o)c(i);else h(o)}m.then(v,v)}h(o)})}function no(e){let t=Lr(e);if(!ce)oo(t);return t}function Fr(e){let t=Lr(e);return t.equals=qn,t}function Cs(e){var t=e.effects;if(t!==null){e.effects=null;for(var r=0;r<t.length;r+=1)ae(t[r])}}var Ri=[];function vn(e){var t,r=k,n=e.parent;if(!lt&&n!==null&&e.v!==Z&&(n.f&(W|fe))!==0)return is(),e.v;if(_e(n),p){let o=hr;to(new Set);try{if(Lt.call(Ri,e))ds();Ri.push(e),Cs(e),t=ro(e)}finally{_e(r),to(o),Ri.pop()}}else try{Cs(e),t=ro(e)}finally{_e(r)}return t}function ji(e){var t=vn(e);if(!e.equals(t)){if(e.wv=Br(),!T?.is_fork||e.deps===null){if(T!==null)T.capture(e,t,!0),Qe?.capture(e,t,!0);else e.v=t;if(e.deps===null){q(e,G);return}}}if(lt)return;if($e!==null){if(Jt()||T?.is_fork)$e.set(e,t)}else Dr(e)}function Os(e){if(e.effects===null)return;for(let t of e.effects)if(t.teardown||t.ac){if(t.teardown?.(),t.ac!==null)Je(()=>{t.ac.abort(Pt),t.ac=null});if(t.fn!==null)t.teardown=Et;gr(t,0),mn(t)}}function Ci(e){if(e.effects===null)return;for(let t of e.effects)if(t.teardown&&t.fn!==null)Rt(t)}var io=null,Yr=null,T=null,Qe=null,$e=null,Di=null,mr=!1,Oi=!1,Hr=null,hn=null,Ds=0,Ii=new Set,Hf=1;class je{id=Hf++;started=!1;linked=!0;prev=null;next=null;async_deriveds=new Map;current=new Map;previous=new Map;#e=new Set;#t=new Set;#r=0;#s=new Map;#o=null;#n=[];#a=[];#i=new Map;#l=new Map;#d=new Set;is_fork=!1;#f=!1;constructor(){if(Yr===null)io=Yr=this;else Yr.next=this,this.prev=Yr;Yr=this}#v(){if(this.is_fork)return!0;for(let e of this.#s.keys())if(!this.#g(e))return!0;return!1}#g(e){for(var t=e;t!==null;t=t.parent)if(this.#l.has(t))return!0;return!1}skip_effect(e){if(!this.#l.has(e))this.#l.set(e,{d:[],m:[]});this.#d.delete(e)}unskip_effect(e,t=(r)=>this.schedule(r)){var r=this.#l.get(e);if(r){this.#l.delete(e);for(var n of r.d)q(n,F),t(n);for(n of r.m)q(n,oe),t(n)}this.#d.add(e)}#c(){var e=[];for(let o of this.#n){if((o.f&W)!==0||(o.f&(F|oe))===0)continue;var t=o,r=!1;while(t.parent!==null){t=t.parent;var n=t.f;if((n&(He|ge))!==0){if((n&G)===0){r=!0;break}t.f^=G}}if(!r)e.push(t)}return this.#n=[],e}#m(){if(this.started=!0,p)for(let a of this.current.keys())Ii.add(a);for(let[a,f]of this.#i)q(a,f),this.schedule(a);this.apply();var e=Hr=[],t=[],r=hn=[];while(this.#n.length>0){if(Ds++>1000)this.#p(),Vf();for(let a of this.#c())try{this.#x(a,e,t)}catch(f){if(Ps(a),!this.#v())this.discard();throw f}}if(T=null,r.length>0){var n=je.ensure();for(let a of r)n.schedule(a)}if(Hr=null,hn=null,this.#v()){this.#_(t),this.#_(e);for(let[a,f]of this.#l)Fs(a,f);if(r.length>0)T.#m();return}let o=this.#y();if(o){this.#_(t),this.#_(e),o.#u(this);return}this.#i.clear();for(let a of this.#e)a(this);this.#e.clear(),Qe=this,Ms(t),Ms(e),Qe=null,this.#o?.resolve();var i=T;if(this.#r===0&&(this.#n.length===0||i!==null)){if(this.#p(),ce)this.#b(),T=i}if(i!==null)ct.clear(),i.#m()}#x(e,t,r){e.f^=G;var n=e.first;while(n!==null){var o=n.f,i=(o&(ge|He))!==0,a=i&&(o&G)!==0,f=a||(o&fe)!==0||this.#l.has(n);if(!f&&n.fn!==null){if(i)n.f^=G;else if((o&kt)!==0)t.push(n);else if(ce&&(o&(zt|cr))!==0)r.push(n);else if(er(n)){if((o&Se)!==0)this.add_dirty_reaction(n,oe);Rt(n)}var c=n.first;if(c!==null){n=c;continue}}while(n!==null){var h=n.next;if(h!==null){n=h;break}n=n.parent}}}#y(){for(var e=this.prev;e!==null;e=e.prev)if(!e.is_fork&&this.#h(e))return e;return null}#h(e){for(let[t,[,r]]of this.current){if(r)continue;if(e.current.has(t))return!0}return!1}#u(e){for(let[r,n]of e.current){if(!this.previous.has(r)&&e.previous.has(r))this.previous.set(r,e.previous.get(r));this.current.set(r,n)}for(let[r,n]of e.async_deriveds){let o=this.async_deriveds.get(r);if(o)n.promise.then(o.resolve).catch(o.reject)}e.async_deriveds.clear(),this.transfer_reactions(e.#i);let t=(r)=>{var n=r.reactions;if(n===null)return;if((r.f&te)!==0&&(r.f&(F|oe))===0)return;for(let a of n){var o=a.f;if((o&te)!==0)t(a);else{var i=a;if(o&(Ke|Se)&&!this.async_deriveds.has(i))this.#i.delete(i),q(i,F),this.schedule(i)}}};for(let r of this.current.keys())t(r);this.oncommit(()=>e.discard()),e.#p(),T=this,this.#m()}#_(e){for(var t=0;t<e.length;t+=1)Jn(e[t],this.#i)}capture(e,t,r=!1){if(e.v!==Z&&!this.previous.has(e))this.previous.set(e,e.v);if((e.f&it)===0)this.current.set(e,[t,r]),$e?.set(e,t);if(!this.is_fork)e.v=t}activate(){T=this}deactivate(){T=null,$e=null}flush(){try{if(p)Ii.clear();Oi=!0,T=this,this.#m()}finally{if(Ds=0,Di=null,Hr=null,hn=null,Oi=!1,T=null,$e=null,ct.clear(),p)for(let e of Ii)e.updated=null}}discard(){for(let e of this.#t)e(this);this.#t.clear();for(let e of this.async_deriveds.values())e.reject(Pr);this.#p(),this.#o?.resolve()}register_created_effect(e){this.#a.push(e)}#b(){for(let m=io;m!==null;m=m.next){var e=m.id<this.id,t=[];for(let[v,[_,d]]of this.current){if(m.current.has(v)){var r=m.current.get(v)[0];if(e&&_!==r)m.current.set(v,[_,d]);else continue}t.push(v)}if(e)for(let[v,_]of this.async_deriveds){let d=m.async_deriveds.get(v);if(d)_.promise.then(d.resolve).catch(d.reject)}var n=[...m.current.keys()].filter((v)=>!m.current.get(v)[1]);if(!m.started||n.length===0)continue;var o=n.filter((v)=>!this.current.has(v));if(o.length===0){if(e)m.discard()}else if(t.length>0){if(p&&!m.#f)Ts(m.#n.length===0,"Batch has scheduled effects");if(e)for(let v of this.#d)m.unskip_effect(v,(_)=>{if((_.f&(Se|Ke))!==0)m.schedule(_);else m.#_([_])});m.activate();var i=new Set,a=new Map;for(var f of t)Ls(f,o,i,a);a=new Map;var c=[...m.current].filter(([v,_])=>{let d=this.current.get(v);if(!d)return!0;return d[0]!==_[0]||d[1]!==_[1]}).map(([v])=>v);if(c.length>0){for(let v of this.#a)if((v.f&(W|fe|Kt))===0&&Mi(v,c,a))if((v.f&(Ke|Se))!==0)q(v,F),m.schedule(v);else m.add_dirty_reaction(v,F)}if(m.#n.length>0&&!m.#f){m.apply();for(var h of m.#c())m.#x(h,[],[])}m.deactivate()}}}increment(e,t){if(this.#r+=1,e){let r=this.#s.get(t)??0;this.#s.set(t,r+1)}}decrement(e,t){if(this.#r-=1,e){let r=this.#s.get(t)??0;if(r===1)this.#s.delete(t);else this.#s.set(t,r-1)}if(this.#f)return;this.#f=!0,ue(()=>{if(this.#f=!1,this.linked)this.flush()})}transfer_reactions(e){for(let[t,r]of e)this.add_dirty_reaction(t,r);e.clear()}add_dirty_reaction(e,t){if(t===F||this.#i.get(e)!==F)this.#i.set(e,t)}oncommit(e){this.#e.add(e)}ondiscard(e){this.#t.add(e)}settled(){return(this.#o??=Ln()).promise}static ensure(){if(T===null){let e=T=new je;if(!Oi&&!mr)ue(()=>{if(!e.started)e.flush()})}return T}apply(){if(!ce||!this.is_fork&&this.prev===null&&this.next===null){$e=null;return}$e=new Map;for(let[e,[t]]of this.current)$e.set(e,t);for(let e=io;e!==null;e=e.next){if(e===this||e.is_fork)continue;if(e.id>=this.id||!e.#h(this)){for(let[t,r]of e.previous)if(!$e.has(t))$e.set(t,r)}}}schedule(e){if(Di=e,e.b?.is_pending&&(e.f&(kt|zt|cr))!==0&&(e.f&Pe)===0){e.b.defer_effect(e);return}this.#n.push(e)}#p(){if(!this.linked)return;var e=this.prev,t=this.next;if(e===null)io=t;else e.next=t;if(t===null)Yr=e;else t.prev=e;this.linked=!1}}function Qt(e){var t=mr,r=Qe;Qe=null,mr=!0;try{var n;if(e)Qt(),n=e();while(!0){if(Ss(),T===null)return n;T.flush()}}finally{mr=t,Qe=r}}function Vf(){if(p){var e=new Map;for(let r of T.current.keys())for(let[n,o]of r.updated??[]){var t=e.get(n);if(!t)t={error:o.error,count:0},e.set(n,t);t.count+=o.count}for(let r of e.values())if(r.error)console.error(r.error)}try{gs()}catch(r){if(p)he(r,"stack",{value:""});Ue(r,Di)}}var ft=null;function Ms(e){var t=e.length;if(t===0)return;var r=0;while(r<t){var n=e[r++];if((n.f&(W|fe))===0&&er(n)){if(ft=new Set,Rt(n),n.deps===null&&n.first===null&&n.nodes===null&&n.teardown===null&&n.ac===null)Li(n);if(ft?.size>0){ct.clear();for(let o of ft){if((o.f&(W|fe))!==0)continue;let i=[o],a=o.parent;while(a!==null){if(ft.has(a))ft.delete(a),i.push(a);a=a.parent}for(let f=i.length-1;f>=0;f--){let c=i[f];if((c.f&(W|fe))!==0)continue;Rt(c)}}ft.clear()}}}ft=null}function Ls(e,t,r,n){if(r.has(e))return;if(r.add(e),e.reactions!==null)for(let o of e.reactions){let i=o.f;if((i&te)!==0)Ls(o,t,r,n);else if((i&(Ke|Se))!==0&&(i&F)===0&&Mi(o,t,n))q(o,F),gn(o)}}function Mi(e,t,r){let n=r.get(e);if(n!==void 0)return n;if(e.deps!==null)for(let o of e.deps){if(Lt.call(t,o))return!0;if((o.f&te)!==0&&Mi(o,t,r))return r.set(o,!0),!0}return r.set(e,!1),!1}function gn(e){T.schedule(e)}function Fs(e,t){if((e.f&ge)!==0&&(e.f&G)!==0)return;if((e.f&F)!==0)t.d.push(e);else if((e.f&oe)!==0)t.m.push(e);q(e,G);var r=e.first;while(r!==null)Fs(r,t),r=r.next}function Ps(e){q(e,G);var t=e.first;while(t!==null)Ps(t),t=t.next}var hr=new Set,ct=new Map;function to(e){hr=e}var Pi=!1;function Ys(){Pi=!0}function De(e,t){var r={f:0,v:e,reactions:null,equals:Gn,rv:0,wv:0};if(p&&gt)r.created=t??xt("created at"),r.updated=null,r.set_during_effect=!1,r.trace=null;return r}function P(e,t){let r=De(e,t);return oo(r),r}function br(e,t=!1,r=!0){let n=De(e);if(!t)n.equals=qn;if(Xt&&r&&L!==null&&L.l!==null)(L.l.s??=[]).push(n);return n}function A(e,t,r=!1){if(N!==null&&(!Ye||(N.f&Kt)!==0)&&Nt()&&(N.f&(te|Se|Ke|Kt))!==0&&($t===null||!$t.has(e)))ws();let n=r?ut(t):t;if(p)Wn(n,e.label);return yt(e,n,hn)}var xr=null,Fi=0;function yt(e,t,r=null){if(!e.equals(t)){if(lt)ct.set(e,t);else if(!ct.has(e))ct.set(e,e.v);var n=je.ensure();if(n.capture(e,t),p){if(gt||k!==null){e.updated??=new Map;let o=(e.updated.get("")?.count??0)+1;if(e.updated.set("",{error:null,count:o}),gt||o>5){let i=xt("updated at");if(i!==null){let a=e.updated.get(i.stack);if(!a)a={error:i,count:0},e.updated.set(i.stack,a);a.count++}}}if(k!==null)e.set_during_effect=!0}if((e.f&te)!==0){let o=e;if((e.f&F)!==0)vn(o);if($e===null)Dr(o)}if(e.wv=Br(),xr=null,Fi=0,Hs(e,F,r),xr=null,Nt()&&k!==null&&(k.f&G)!==0&&(k.f&(ge|He))===0)if(et===null)Vs([e]);else et.push(e);if(!n.is_fork&&hr.size>0&&!Pi)ao()}return t}function ao(){Pi=!1;for(let e of hr){if((e.f&G)!==0)q(e,oe);let t;try{t=er(e)}catch{t=!0}if(t)Rt(e)}hr.clear()}function yr(e){A(e,e.v+1)}function Hs(e,t,r){var n=e.reactions;if(n===null)return;var o=Nt(),i=n.length;if(Fi+=i,Fi>1e5&&xr===null)xr=new Set;if(xr!==null){if(xr.has(e))return;xr.add(e)}for(var a=0;a<i;a++){var f=n[a],c=f.f;if(!o&&f===k)continue;var h=(c&F)===0;if(h)q(f,t);if((c&Kt)!==0)hr.add(f);else if((c&te)!==0){var m=f;$e?.delete(m),Hs(m,oe,r)}else if(h){var v=f;if((c&Se)!==0&&ft!==null)ft.add(v);if(r!==null)r.push(v);else gn(v)}}}var Gf=/^[a-zA-Z_$][a-zA-Z_$0-9]*$/;function ut(e){if(typeof e!=="object"||e===null||We in e||Pn in e)return e;let t=Rr(e);if(t!==pi&&t!==Wa)return e;var r=new Map,n=mt(e),o=P(0),i=p&&gt?xt("created at"):null,a=tr,f=(v)=>{if(tr===a)return v();var _=N,d=tr;de(null),Bi(a);var y=v();return de(_),Bi(d),y};if(n){if(r.set("length",P(e.length,i)),p)e=zf(e)}var c="";let h=!1;function m(v){if(h)return;h=!0,c=v,Ie(o,`${c} version`);for(let[_,d]of r)Ie(d,$r(c,_));h=!1}return new Proxy(e,{defineProperty(v,_,d){if(!("value"in d)||d.configurable===!1||d.enumerable===!1||d.writable===!1)ys();var y=r.get(_);if(y===void 0)f(()=>{var R=P(d.value,i);if(r.set(_,R),p&&typeof _==="string")Ie(R,$r(c,_));return R});else A(y,d.value,!0);return!0},deleteProperty(v,_){var d=r.get(_);if(d===void 0){if(_ in v){let y=f(()=>P(Z,i));if(r.set(_,y),yr(o),p)Ie(y,$r(c,_))}}else A(d,Z),yr(o);return!0},get(v,_,d){if(_===We)return e;if(p&&_===Yn)return m;var y=r.get(_),R=_ in v;if(y===void 0&&(!R||ze(v,_)?.writable))y=f(()=>{var C=ut(R?v[_]:Z),Y=P(C,i);if(p)Ie(Y,$r(c,_));return Y}),r.set(_,y);if(y!==void 0){var b=l(y);return b===Z?void 0:b}return Reflect.get(v,_,d)},getOwnPropertyDescriptor(v,_){this.has?.(v,_);var d=Reflect.getOwnPropertyDescriptor(v,_),y=r.get(_);if(y!==void 0){var R=l(y);if(R===Z)return;if(d&&"value"in d)d.value=R;else return{enumerable:!0,configurable:!0,value:R,writable:!0}}return d},has(v,_){if(_===We)return!0;var d=r.get(_),y=d!==void 0&&d.v!==Z||Reflect.has(v,_);if(d!==void 0||(k!==null||N!==null)&&(!y||ze(v,_)?.writable)){if(d===void 0)d=f(()=>{var b=y?ut(v[_]):Z,C=P(b,i);if(p)Ie(C,$r(c,_));return C}),r.set(_,d);var R=l(d);if(R===Z)return!1}return y},set(v,_,d,y){var R=r.get(_),b=_ in v;if(n&&_==="length")for(var C=d;C<R.v;C+=1){var Y=r.get(C+"");if(Y!==void 0)A(Y,Z);else if(C in v){if(Y=f(()=>P(Z,i)),r.set(C+"",Y),p)Ie(Y,$r(c,C))}}if(R===void 0){if(!b||ze(v,_)?.writable){if(R=f(()=>P(void 0,i)),p)Ie(R,$r(c,_));A(R,ut(d)),r.set(_,R)}}else{b=R.v!==Z;var ee=f(()=>ut(d));A(R,ee)}var le=Reflect.getOwnPropertyDescriptor(v,_);if(le?.set)le.set.call(y,d);if(!b){if(n&&typeof _==="string"){var dt=r.get("length"),Ee=Number(_);if(Number.isInteger(Ee)&&Ee>=dt.v)A(dt,Ee+1)}yr(o)}return!0},ownKeys(v){l(o);var _=Reflect.ownKeys(v).filter((R)=>{var b=r.get(R);return b===void 0||b.v!==Z});for(var[d,y]of r)if(y.v!==Z&&!(d in v))_.push(d);return _},setPrototypeOf(){$s()}})}function $r(e,t){if(typeof t==="symbol")return`${e}[Symbol(${t.description??""})]`;if(Gf.test(t))return`${e}.${t}`;return/^\d+$/.test(t)?`${e}[${t}]`:`${e}['${t}']`}function Vr(e){try{if(e!==null&&typeof e==="object"&&We in e)return e[We]}catch{}return e}function so(e,t){return Object.is(Vr(e),Vr(t))}var qf=new Set(["copyWithin","fill","pop","push","reverse","shift","sort","splice","unshift"]);function zf(e){return new Proxy(e,{get(t,r,n){var o=Reflect.get(t,r,n);if(!qf.has(r))return o;return function(...i){Ys();var a=o.apply(this,i);return ao(),a}}})}function Us(){let{prototype:e,__svelte_cleanup:t}=Array;if(t)t();let{indexOf:r,lastIndexOf:n,includes:o}=e;e.indexOf=function(i,a){let f=r.call(this,i,a);if(f===-1){for(let c=a??0;c<this.length;c+=1)if(Vr(this[c])===i){Un("array.indexOf(...)");break}}return f},e.lastIndexOf=function(i,a){let f=n.call(this,i,a??this.length-1);if(f===-1){for(let c=0;c<=(a??this.length-1);c+=1)if(Vr(this[c])===i){Un("array.lastIndexOf(...)");break}}return f},e.includes=function(i,a){let f=o.call(this,i,a);if(!f){for(let c=0;c<this.length;c+=1)if(Vr(this[c])===i){Un("array.includes(...)");break}}return f},Array.__svelte_cleanup=()=>{e.indexOf=r,e.lastIndexOf=n,e.includes=o}}var Ur,Gs,xn,qs,zs;function lo(){if(Ur!==void 0)return;Ur=window,Gs=document,xn=/Firefox/.test(navigator.userAgent);var e=Element.prototype,t=Node.prototype,r=Text.prototype;if(qs=ze(t,"firstChild").get,zs=ze(t,"nextSibling").get,di(e))e[sn]=void 0,e[Hn]=null,e[mi]=void 0,e.__e=void 0;if(di(r))r[ln]=void 0;if(p)e.__svelte_meta=null,Us()}function se(e=""){return document.createTextNode(e)}function ie(e){return qs.call(e)}function xe(e){return zs.call(e)}function O(e,t){if(!w)return ie(e);var r=ie(j);if(r===null)r=j.appendChild(se());else if(t&&r.nodeType!==ur){var n=se();return r?.before(n),U(n),n}if(t)bn(r);return U(r),r}function Vt(e,t=!1){if(!w){var r=ie(e);if(r instanceof Comment&&r.data==="")return xe(r);return r}if(t){if(j?.nodeType!==ur){var n=se();return j?.before(n),U(n),n}bn(j)}return j}function X(e,t=!1){if(!w)return ie(e);var r=O(e,t);return S(e),r}function x(e,t=1,r=!1){let n=w?j:e;var o;while(t--)o=n,n=xe(n);if(!w)return n;if(r){if(n?.nodeType!==ur){var i=se();if(n===null)o?.after(i);else n.before(i);return U(i),i}bn(n)}return U(n),n}function pn(e){e.textContent=""}function fo(){if(!ce)return!1;if(ft!==null)return!1;var e=k.f;return(e&Pe)!==0}function jt(e,t,r){if(t==null||t===Vn)return r?document.createElement(e,{is:r}):document.createElement(e);return r?document.createElementNS(t,e,{is:r}):document.createElementNS(t,e)}function bn(e){if(e.nodeValue.length<65536)return;let t=e.nextSibling;while(t!==null&&t.nodeType===ur)t.remove(),e.nodeValue+=t.nodeValue,t=e.nextSibling}var Yi=new WeakMap;function Ks(e){var t=k;if(t===null)return N.f|=it,e;if(p&&e instanceof Error&&!Yi.has(e))Yi.set(e,Kf(e,t));if((t.f&Pe)===0&&(t.f&kt)===0){if(p&&!t.parent&&e instanceof Error)Ws(e);throw e}Ue(e,t)}function Ue(e,t){if(e===Ze)throw e;if(t!==null&&(t.f&W)!==0)return;while(t!==null){if((t.f&jr)!==0&&(t.f&(W|Ft))===0){if((t.f&Pe)===0)throw e;try{t.b.error(e);return}catch(r){e=r}}t=t.parent}if(p&&e instanceof Error)Ws(e);throw e}function Kf(e,t){let r=ze(e,"message");if(r&&!r.configurable)return;var n=xn?"  ":"\t",o=`
${n}in ${t.fn?.name||"<unknown>"}`,i=t.ctx;while(i!==null)o+=`
${n}in ${i.function?.[st].split("/").pop()}`,i=i.p;return{message:e.message+`
${o}
`,stack:e.stack?.split(`
`).filter((a)=>!a.includes("svelte/src/internal")).join(`
`)}}function Ws(e){let t=Yi.get(e);if(t)he(e,"message",{value:t.message}),he(e,"stack",{value:t.stack})}function Zs(e){if(k===null){if(N===null)hs(e);ms()}if(lt)vs(e)}function Wf(e,t){var r=t.last;if(r===null)t.last=t.first=e;else r.next=e,e.prev=r,t.last=e}function Ct(e,t){var r=k;if(p)while(r!==null&&(r.f&Kt)!==0)r=r.parent;if(r!==null&&(r.f&fe)!==0)e|=fe;var n={ctx:L,deps:null,nodes:null,f:e|F|Fe,first:null,fn:t,last:null,next:null,parent:r,b:r&&r.b,prev:null,teardown:null,wv:0,ac:null};if(p)n.component_function=bt;T?.register_created_effect(n);var o=n;if((e&kt)!==0)if(Hr!==null)Hr.push(n);else je.ensure().schedule(n);else if(t!==null){try{Rt(n)}catch(a){throw ae(n),a}if(o.deps===null&&o.teardown===null&&o.nodes===null&&o.first===o.last&&(o.f&Tt)===0){if(o=o.first,(e&Se)!==0&&(e&Ve)!==0&&o!==null)o.f|=Ve}}if(o!==null){if(o.parent=r,r!==null)Wf(o,r);if(N!==null&&(N.f&te)!==0&&(e&He)===0){var i=N;(i.effects??=[]).push(o)}}return n}function Jt(){return N!==null&&!Ye}function be(e){let t=Ct(zt,null);return q(t,G),t.teardown=e,t}function Gr(e){if(Zs("$effect"),p)he(e,"name",{value:"$effect"});var t=k.f,r=!N&&(t&ge)!==0&&L!==null&&!L.i;if(r){var n=L;(n.e??=[]).push(e)}else return Ti(e)}function Ti(e){return Ct(kt|Za,e)}function Vi(e){je.ensure();let t=Ct(He|Tt,e);return()=>{ae(t)}}function Xs(e){je.ensure();let t=Ct(He|Tt,e);return(r={})=>new Promise((n)=>{if(r.outro)Ot(t,()=>{ae(t),n(void 0)});else ae(t),n(void 0)})}function tt(e){return Ct(kt,e)}function Is(e){return Ct(Ke|Tt,e)}function we(e,t=0){return Ct(zt|t,e)}function V(e,t=[],r=[],n=[]){eo(n,t,r,(o)=>{Ct(zt,()=>{e(...o.map(l))})})}function rt(e,t=0){var r=Ct(Se|t,e);if(p)r.dev_stack=St;return r}function ve(e){return Ct(ge|Tt,e)}function Ui(e){var t=e.teardown;if(t!==null){let r=lt,n=N;Hi(!0),de(null);try{t.call(null)}catch(o){Ue(o,e.parent)}finally{Hi(r),de(n)}}}function mn(e,t=!1){var r=e.first;e.first=e.last=null;while(r!==null){let o=r.ac;if(o!==null)Je(()=>{o.abort(Pt)});var n=r.next;if((r.f&He)!==0)r.parent=null;else ae(r,t);r=n}}function Js(e){var t=e.first;while(t!==null){var r=t.next;if((t.f&ge)===0)ae(t);t=r}}function ae(e,t=!0){var r=!1;if((t||(e.f&vi)!==0)&&e.nodes!==null&&e.nodes.end!==null)Qs(e.nodes.start,e.nodes.end),r=!0;e.f|=Ft,mn(e,t&&!r),gr(e,0);var n=e.nodes&&e.nodes.t;if(n!==null)for(let i of n)i.stop();Ui(e),e.f^=Ft,e.f|=W;var o=e.parent;if(o!==null&&o.first!==null)Li(e);if(p)e.component_function=null;e.next=e.prev=e.teardown=e.ctx=e.deps=e.fn=e.nodes=e.ac=e.b=null}function Qs(e,t){while(e!==null){var r=e===t?null:xe(e);e.remove(),e=r}}function Li(e){var{parent:t,prev:r,next:n}=e;if(r!==null)r.next=n;if(n!==null)n.prev=r;if(t!==null){if(t.first===e)t.first=n;if(t.last===e)t.last=r}}function Ot(e,t,r=!0){var n=[];e.f|=Fn,el(e,n,!0);var o=()=>{if(r)ae(e);if(t)t()},i=n.length;if(i>0){var a=()=>--i||o();for(var f of n)f.out(a)}else o()}function el(e,t,r){if((e.f&fe)!==0)return;e.f^=fe;var n=e.nodes&&e.nodes.t;if(n!==null){for(let f of n)if(f.is_global||r)t.push(f)}var o=e.first;while(o!==null){var i=o.next;if((o.f&He)===0){var a=(o.f&Ve)!==0||(o.f&ge)!==0&&(e.f&Se)!==0;el(o,t,a?r:!1)}o=i}}function qr(e){e.f&=~Fn,tl(e,!0)}function tl(e,t){if((e.f&Fn)!==0)return;if((e.f&fe)===0)return;if(e.f^=fe,(e.f&G)===0)q(e,F),je.ensure().schedule(e);var r=e.first;while(r!==null){var n=r.next,o=(r.f&Ve)!==0||(r.f&ge)!==0;tl(r,o?t:!1),r=n}var i=e.nodes&&e.nodes.t;if(i!==null){for(let a of i)if(a.is_global||t)a.in()}}function zr(e,t){if(!e.nodes)return;var r=e.nodes.start,n=e.nodes.end;while(r!==null){var o=r===n?null:xe(r);t.append(r),r=o}}var rl=null;var lt=!1;function Hi(e){lt=e}var N=null,Ye=!1;function de(e){N=e}var k=null;function _e(e){k=e}var $t=null;function oo(e){if(N!==null&&(!ce&&(N.f&Cr)!==0||(N.f&te)!==0))($t??=new Set).add(e)}var ye=null,Be=0,et=null;function Vs(e){et=e}var ol=1,wr=0,tr=wr;function Bi(e){tr=e}function Br(){return++ol}function er(e){var t=e.f;if((t&F)!==0)return!0;if((t&oe)!==0){var r=e.deps,n=r.length;for(var o=0;o<n;o++){var i=r[o];if(er(i))ji(i);if(i.wv>e.wv)return!0}if((t&Fe)!==0&&$e===null)q(e,G)}return!1}function il(e,t,r=!0){var n=e.reactions;if(n===null)return;if(!ce&&$t!==null&&$t.has(e))return;for(var o=0;o<n.length;o++){var i=n[o];if((i.f&te)!==0)il(i,t,!1);else if(t===i){if(r)q(i,F);else if((i.f&G)!==0)q(i,oe);gn(i)}}}function ro(e){var t=ye,r=Be,n=et,o=N,i=$t,a=L,f=Ye,c=tr,h=e.f;if(ye=null,Be=0,et=null,N=(h&(ge|He))===0?e:null,$t=null,Ht(e.ctx),Ye=!1,tr=++wr,e.ac!==null)Je(()=>{e.ac.abort(Pt)}),e.ac=null;try{e.f|=Cr;var m=e.fn,v=m();e.f|=Pe;var _=nl(e);if(Nt()&&et!==null&&!Ye&&_!==null&&(e.f&(te|oe|F))===0)for(var d=0;d<et.length;d++)il(et[d],e);if(o!==null&&o!==e){if(wr++,o.deps!==null)for(let y=0;y<r;y+=1)o.deps[y].rv=wr;if(t!==null)for(let y of t)y.rv=wr;if(et!==null)if(n===null)n=et;else n.push(...et)}if((e.f&it)!==0)e.f^=it;return v}catch(y){return nl(e),Ks(y)}finally{e.f^=Cr,ye=t,Be=r,et=n,N=o,$t=i,Ht(a),Ye=f,tr=c}}function nl(e){var t=e.deps,r=T?.is_fork;if(ye!==null){var n;if(!r)gr(e,Be);if(t!==null&&Be>0){t.length=Be+ye.length;for(n=0;n<ye.length;n++)t[Be+n]=ye[n]}else e.deps=t=ye;if(Jt()&&(e.f&Fe)!==0)for(n=Be;n<t.length;n++)(t[n].reactions??=[]).push(e)}else if(!r&&t!==null&&Be<t.length)gr(e,Be),t.length=Be;return t}function Zf(e,t){let r=t.reactions;if(r!==null){var n=Ka.call(r,e);if(n!==-1){var o=r.length-1;if(o===0)r=t.reactions=null;else r[n]=r[o],r.pop()}}if(r===null&&(t.f&te)!==0&&(ye===null||!Lt.call(ye,t))){var i=t;if((i.f&Fe)!==0)i.f^=Fe;if(i.v!==Z)Dr(i);if(i.ac!==null)Je(()=>{i.ac.abort(Pt),i.ac=null,q(i,F)});Os(i),gr(i,0)}}function gr(e,t){var r=e.deps;if(r===null)return;for(var n=t;n<r.length;n++)Zf(e,r[n])}function Rt(e){var t=e.f;if((t&W)!==0)return;q(e,G);var r=k;if(k=e,p){var n=bt;Zn(e.component_function);var o=St;Ir(e.dev_stack??St)}try{if((t&(Se|cr))!==0)Js(e);else mn(e);Ui(e);var i=ro(e);if(e.teardown=typeof i==="function"?i:null,e.wv=ol,p&&gt&&(e.f&F)!==0&&e.deps!==null){for(var a of e.deps)if(a.set_during_effect)a.wv=Br(),a.set_during_effect=!1}}finally{if(k=r,p)Zn(n),Ir(o)}}async function yn(){if(ce)return new Promise((e)=>{requestAnimationFrame(()=>e()),setTimeout(()=>e())});await Promise.resolve(),Qt()}function l(e){var t=e.f,r=(t&te)!==0;if(rl?.add(e),N!==null&&!Ye){var n=k!==null&&(k.f&W)!==0;if(!n&&($t===null||!$t.has(e))){var o=N.deps;if((N.f&Cr)!==0){if(e.rv<wr)if(e.rv=wr,ye===null&&o!==null&&o[Be]===e)Be++;else if(ye===null)ye=[e];else ye.push(e)}else{if(N.deps??=[],!Lt.call(N.deps,e))N.deps.push(e);var i=e.reactions;if(i===null)e.reactions=[N];else if(!Lt.call(i,N))i.push(N)}}}if(p){if(!Ye&&Ge&&T===null&&Qe===null&&!Ge.warned&&(Ge.effect.f&Cr)===0&&!Ge.effect_deps.has(e)){Ge.warned=!0,ns(e.label);var a=xt("traced at");if(a)console.warn(a)}if(_n.delete(e),gt&&!Ye&&fn!==null&&N!==null&&fn.reaction===N){if(e.trace)e.trace();else if(a=xt("traced at"),a){var f=fn.entries.get(e);if(f===void 0)f={traces:[]},fn.entries.set(e,f);var c=f.traces[f.traces.length-1];if(a.stack!==c?.stack)f.traces.push(a)}}}if(lt&&ct.has(e))return ct.get(e);if(r){var h=e;if(lt){var m=h.v;if((h.f&G)===0&&h.reactions!==null||sl(h))m=vn(h);return ct.set(h,m),m}var v=(h.f&Fe)===0&&!Ye&&N!==null&&(N.f&Fe)!==0,_=(h.f&Pe)===0;if(er(h)){if(v)h.f|=Fe;ji(h)}if(v&&!_)Ci(h),al(h)}if($e?.has(e))return $e.get(e);if((e.f&it)!==0)throw e.v;return e.v}function al(e){if(e.f|=Fe,e.deps===null)return;for(let r of e.deps){var t=r.reactions;if(t===null)r.reactions=[e];else if(!Lt.call(t,e))t.push(e);if((r.f&te)!==0&&(r.f&Fe)===0)Ci(r),al(r)}}function sl(e){if(e.v===Z)return!0;if(e.deps===null)return!1;for(let t of e.deps){if(ct.has(t))return!0;if((t.f&te)!==0&&sl(t))return!0}return!1}function J(e){var t=Ye;try{return Ye=!0,e()}finally{Ye=t}}var Jf=["allowfullscreen","async","autofocus","autoplay","checked","controls","default","disabled","formnovalidate","indeterminate","inert","ismap","loop","multiple","muted","nomodule","novalidate","open","playsinline","readonly","required","reversed","seamless","selected","webkitdirectory","defer","disablepictureinpicture","disableremoteplayback"];var B_=[...Jf,"formNoValidate","isMap","noModule","playsInline","readOnly","value","volume","defaultValue","defaultChecked","srcObject","noValidate","allowFullscreen","disablePictureInPicture","disableRemotePlayback"];var Qf=["touchstart","touchmove"];function ll(e){return Qf.includes(e)}var ec=["$state","$state.raw","$derived","$derived.by"],Y_=[...ec,"$state.eager","$state.snapshot","$props","$props.id","$bindable","$effect","$effect.pre","$effect.tracking","$effect.root","$effect.pending","$inspect","$inspect().with","$inspect.trace","$host"];var Kr=Symbol("events"),zi=new Set,co=new Set;function Ki(e){if(!w)return;e.removeAttribute("onload"),e.removeAttribute("onerror");let t=e.__e;if(t!==void 0)e.__e=void 0,queueMicrotask(()=>{if(e.isConnected)e.dispatchEvent(t)})}function ul(e,t,r,n={}){function o(i){if(!n.capture)po.call(t,i);if(!i.cancelBubble)return Je(()=>r?.call(this,i))}if(e.startsWith("pointer")||e.startsWith("touch")||e==="wheel")o.__removed=!1,ue(()=>{if(!o.__removed)t.addEventListener(e,o,n)});else t.addEventListener(e,o,n);return o}function pt(e,t,r,n,o){var i={capture:n,passive:o},a=ul(e,t,r,i);if(t===document.body||t===window||t===document||t instanceof HTMLMediaElement)be(()=>{a.__removed=!0,t.removeEventListener(e,a,i)})}function z(e,t,r){(t[Kr]??={})[e]=r}function uo(e){for(var t=0;t<e.length;t++)zi.add(e[t]);for(var r of co)r(e)}var Gi=null,qi=!1;function po(e){var t=this,r=t.ownerDocument,n=e.type,o=e.composedPath?.()||[],i=o[0]||e.target;if(Gi=e,!qi)qi=!0,setTimeout(()=>{qi=!1,Gi=null});var a=0,f=Gi===e&&e[Kr];if(f){var c=o.indexOf(f);if(c!==-1&&(t===document||t===window)){e[Kr]=t;return}var h=o.indexOf(t);if(h===-1)return;if(c<=h)a=c}if(i=o[a]||e.target,i===t)return;he(e,"currentTarget",{configurable:!0,get(){return i||r}});var m=N,v=k;de(null),_e(null);try{var _,d=[];while(i!==null){if(i===t)break;try{var y=i[Kr]?.[n];if(y!=null&&(!i.disabled||e.target===i))y.call(i,e)}catch(R){if(_)d.push(R);else _=R}if(e.cancelBubble)break;a++,i=a<o.length?o[a]:null}if(_){for(let R of d)queueMicrotask(()=>{throw R});throw _}}finally{e[Kr]=t,delete e.currentTarget,de(m),_e(v)}}var rc=globalThis?.window?.trustedTypes&&globalThis.window.trustedTypes.createPolicy("svelte-trusted-html",{createHTML:(e)=>e});function pl(e){return rc?.createHTML(e)??e}function _o(e){var t=jt("template");return t.innerHTML=pl(e.replaceAll("<!>","<!---->")),t.content}function Me(e,t){var r=k;if(r.nodes===null)r.nodes={start:e,end:t,a:null,t:null}}function re(e,t){var r=(t&bi)!==0,n=(t&rs)!==0,o,i=!e.startsWith("<!>");return()=>{if(w)return Me(j,null),j;if(o===void 0){if(o=_o(i?e:"<!>"+e),!r)o=ie(o)}var a=n||xn?document.importNode(o,!0):o.cloneNode(!0);if(r){var f=ie(a),c=a.lastChild;Me(f,c)}else Me(a,a);return a}}function ic(e,t,r="svg"){var n=!e.startsWith("<!>"),o=(t&bi)!==0,i=`<${r}>${n?e:"<!>"+e}</${r}>`,a;return()=>{if(w)return Me(j,null),j;if(!a){var f=_o(i),c=ie(f);if(o){a=document.createDocumentFragment();while(ie(c))a.appendChild(ie(c))}else a=ie(c)}var h=a.cloneNode(!0);if(o){var m=ie(h),v=h.lastChild;Me(m,v)}else Me(h,h);return h}}function Zi(e,t){return ic(e,t,"svg")}function Ut(e=""){if(!w){var t=se(e+"");return Me(t,t),t}var r=j;if(r.nodeType!==ur)r.before(r=se()),U(r);else bn(r);return Me(r,r),r}function Xi(){if(w)return Me(j,null),j;var e=document.createDocumentFragment(),t=document.createComment(""),r=se();return e.append(t,r),Me(t,r),e}function D(e,t){if(w){var r=k;if((r.f&Pe)===0||r.nodes.end===null)r.nodes.end=j;Oe();return}if(e===null)return;e.before(t)}function dl(e){let t=0,r=De(0),n;if(p)Ie(r,"createSubscriber version");return()=>{if(Jt())l(r),we(()=>{if(t===0)n=J(()=>e(()=>yr(r)));return t+=1,()=>{ue(()=>{if(t-=1,t===0)n?.(),n=void 0,yr(r)})}})}}var ac=Ve|Tt;function Ji(e,t,r,n){new _l(e,t,r,n)}class _l{parent;is_pending=!1;transform_error;#e;#t=w?j:null;#r;#s;#o;#n=null;#a=null;#i=null;#l=null;#d=0;#f=0;#v=!1;#g=new Map;#c=null;#m=dl(()=>{if(this.#c=De(this.#d),p)Ie(this.#c,"$effect.pending()");return()=>{this.#c=null}});constructor(e,t,r,n){if(this.#e=e,this.#r=t,this.#s=(o)=>{var i=k;i.b=this,i.f|=jr,r(o)},this.parent=k.b,this.transform_error=n??this.parent?.transform_error??((o)=>o),this.#o=rt(()=>{if(w){let o=_r(this.#t);Oe();let i=o===Zt;if(o.startsWith(yi)){let f=JSON.parse(o.slice(yi.length));this.#y(f)}else if(i)this.#_();else this.#x()}else this.#b()},ac),w)this.#e=j}#x(){try{this.#n=ve(()=>this.#s(this.#e))}catch(e){this.error(e)}}#y(e){let t=this.#r.failed,{reset:r,invoke_onerror:n}=this.#h(e);if(ue(n),!t)return;this.#i=ve(()=>{t(this.#e,()=>e,()=>r)})}#h(e){var t=!1,r=!1;let n=()=>{if(this.#u())return;if(t){fs();return}if(t=!0,r)Es();if(this.#i!==null)Ot(this.#i,()=>{this.#i=null});this.#$(()=>{this.#b()})};return{reset:n,invoke_onerror:()=>{if(this.#u())return;try{r=!0,this.#r.onerror?.(e,n),r=!1}catch(i){Ue(i,this.#o&&this.#o.parent)}}}}#u(){return(this.#o.f&(W|Ft))!==0}#_(){let e=this.#r.pending;if(!e)return;this.is_pending=!0,this.#a=ve(()=>e(this.#e)),ue(()=>{if(this.#u())return;var t=this.#l=document.createDocumentFragment(),r=se(),n=!1;if(t.append(r),this.#n=this.#$(()=>{try{return ve(()=>this.#s(r))}catch(o){try{this.error(o),n=!0}catch(i){Ue(i,this.#o.parent)}return null}}),this.#n===null){if(this.#l=null,n)this.#p(T);return}if(this.#f===0)this.#e.before(t),this.#l=null,Ot(this.#a,()=>{this.#a=null}),this.#p(T)})}#b(){try{if(this.is_pending=this.has_pending_snippet(),this.#f=0,this.#d=0,this.#n=ve(()=>{this.#s(this.#e)}),this.#f>0){var e=this.#l=document.createDocumentFragment();zr(this.#n,e);let t=this.#r.pending;this.#a=ve(()=>t(this.#e))}else this.#p(T)}catch(t){this.error(t)}}#p(e){this.is_pending=!1,e.transfer_reactions(this.#g)}defer_effect(e){Jn(e,this.#g)}is_rendered(){return!this.is_pending&&(!this.parent||this.parent.is_rendered())}has_pending_snippet(){return!!this.#r.pending}#$(e){var t=k,r=N,n=L;_e(this.#o),de(this.#o),Ht(this.#o.ctx);try{return je.ensure(),e()}finally{_e(t),de(r),Ht(n)}}#w(e,t){if(!this.has_pending_snippet()){if(this.parent)this.parent.#w(e,t);return}if(this.#f+=e,this.#f===0){if(this.#p(t),this.#a)Ot(this.#a,()=>{this.#a=null});if(this.#l)this.#e.before(this.#l),this.#l=null}}update_pending_count(e,t){if(this.#w(e,t),this.#d+=e,!this.#c||this.#v)return;this.#v=!0,ue(()=>{if(this.#v=!1,this.#c)yt(this.#c,this.#d)})}get_effect_pending(){return this.#m(),l(this.#c)}error(e){if(e===Ze)throw e;if(!this.#r.onerror&&!this.#r.failed)throw e;if(T?.is_fork){if(this.#n)T.skip_effect(this.#n);if(this.#a)T.skip_effect(this.#a);if(this.#i)T.skip_effect(this.#i);T.oncommit(()=>{if(!this.#u())this.#E(e)})}else this.#E(e)}#E(e){if(this.#n)ae(this.#n),this.#n=null;if(this.#a)ae(this.#a),this.#a=null;if(this.#i)ae(this.#i),this.#i=null;if(w)U(this.#t),Xe(),U(At());let t=this.#r.failed,r=(n)=>{if(this.#u())return;let{reset:o,invoke_onerror:i}=this.#h(n);if(i(),t&&!this.#u())this.#i=this.#$(()=>{try{return ve(()=>{var a=k;a.b=this,a.f|=jr,t(this.#e,()=>n,()=>o)})}catch(a){return Ue(a,this.#o.parent),null}})};ue(()=>{if(this.#u())return;var n;try{n=this.transform_error(e)}catch(o){Ue(o,this.#o&&this.#o.parent);return}if(n!==null&&typeof n==="object"&&typeof n.then==="function")n.then(r,(o)=>Ue(o,this.#o&&this.#o.parent));else r(n)})}}var Qi=!0;function M(e,t){var r=t==null?"":typeof t==="object"?`${t}`:t;if(r!==(e[ln]??=e.nodeValue))e[ln]=r,e.nodeValue=`${r}`}function Wr(e,t){return vl(e,t)}function ta(e,t){lo(),t.intro=t.intro??!1;let r=t.target,n=w,o=j;try{var i=ie(r);while(i&&(i.nodeType!==at||i.data!==Or))i=xe(i);if(!i)throw Ze;pe(!0),U(i);let a=vl(e,{...t,anchor:i});return pe(!1),a}catch(a){if(a instanceof Error&&a.message.split(`
`).some((f)=>f.startsWith("https://svelte.dev/e/")))throw a;if(a!==Ze)console.warn("Failed to hydrate: ",a);if(t.recover===!1)xs();return lo(),pn(r),pe(!1),Wr(e,t)}finally{pe(n),U(o)}}var vo=new Map;function vl(e,{target:t,anchor:r,props:n={},events:o,context:i,intro:a=!0,transformError:f}){lo();var c=void 0,h=Xs(()=>{var m=r??t.appendChild(se());Ji(m,{pending:()=>{}},(d)=>{cn({});var y=L;if(i)y.c=i;if(o)n.$$events=o;if(w)Me(d,null);if(Qi=a,c=e(d,n)||Xn(),Qi=!0,w){if(k.nodes.end=j,j===null||j.nodeType!==at||j.data!==pr)throw dr(),Ze}un()},f);var v=new Set,_=(d)=>{for(var y=0;y<d.length;y++){var R=d[y];if(v.has(R))continue;v.add(R);var b=ll(R);for(let ee of[t,document]){var C=vo.get(ee);if(C===void 0)C=new Map,vo.set(ee,C);var Y=C.get(R);if(Y===void 0)ee.addEventListener(R,po,{passive:b}),C.set(R,1);else C.set(R,Y+1)}}};return _(Nr(zi)),co.add(_),()=>{for(var d of v)for(let b of[t,document]){var y=vo.get(b),R=y.get(d);if(--R==0){if(b.removeEventListener(d,po),y.delete(d),y.size===0)vo.delete(b)}else y.set(d,R)}if(co.delete(_),m!==r)m.parentNode?.removeChild(m)}});return ea.set(c,h),c}var ea=new WeakMap;function ra(e,t){let r=ea.get(e);if(r)return ea.delete(e),r(t);if(p)ss();return Promise.resolve()}class rr{anchor;#e=new Map;#t=new Map;#r=new Map;#s=new Set;#o=!0;constructor(e,t=!0){this.anchor=e,this.#o=t}#n=(e)=>{if(!this.#e.has(e))return;var t=this.#e.get(e),r=this.#t.get(t);if(r)qr(r),this.#s.delete(t);else{var n=this.#r.get(t);if(n){if(qr(n.effect),this.#t.set(t,n.effect),this.#r.delete(t),p)n.fragment.lastChild[hi]=this.anchor;n.fragment.lastChild.remove(),this.anchor.before(n.fragment),r=n.effect}}for(let[o,i]of this.#e){if(this.#e.delete(o),o===e)break;let a=this.#r.get(i);if(a)ae(a.effect),this.#r.delete(i)}for(let[o,i]of this.#t){if(o===t||this.#s.has(o))continue;let a=()=>{if(Array.from(this.#e.values()).includes(o)){var c=document.createDocumentFragment();zr(i,c),c.append(se()),this.#r.set(o,{effect:i,fragment:c})}else ae(i);this.#s.delete(o),this.#t.delete(o)};if(this.#o||!r)this.#s.add(o),Ot(i,a,!1);else a()}};#a=(e)=>{this.#e.delete(e);let t=Array.from(this.#e.values());for(let[r,n]of this.#r)if(!t.includes(r))ae(n.effect),this.#r.delete(r)};ensure(e,t){var r=T,n=fo();if(t&&!this.#t.has(e)&&!this.#r.has(e))if(n){var o=document.createDocumentFragment(),i=se();o.append(i),this.#r.set(e,{effect:ve(()=>t(i)),fragment:o})}else this.#t.set(e,ve(()=>t(this.anchor)));if(this.#e.set(r,e),n){for(let[a,f]of this.#t)if(a===e)r.unskip_effect(f);else r.skip_effect(f);for(let[a,f]of this.#r)if(a===e)r.unskip_effect(f.effect);else r.skip_effect(f.effect);r.oncommit(this.#n),r.ondiscard(this.#a)}else{if(w)this.anchor=j;this.#n(r)}}}function ne(e,t,r=!1){var n;if(w)n=j,Oe();var o=new rr(e),i=r?Ve:0;function a(f,c){if(w){var h=_r(n);if(f!==parseInt(h.substring(1))){var m=At();U(m),o.anchor=m,pe(!1),o.ensure(f,c),pe(!0);return}}o.ensure(f,c)}rt(()=>{var f=!1;if(t((c,h=0)=>{f=!0,a(h,c)}),!f)a(-1,null)},i)}var vh=Symbol("NaN");function oa(e,t){return t}function lc(e,t,r){var n=[],o=t.length,i,a=t.length;for(var f=0;f<o;f++){let v=t[f];Ot(v,()=>{if(i){if(i.pending.delete(v),i.done.add(v),i.pending.size===0){var _=e.outrogroups;if(na(e,Nr(i.done)),_.delete(i),_.size===0)e.outrogroups=null}}else a-=1},!1)}if(a===0){var c=n.length===0&&r!==null&&e.pending.size===0;if(c){var h=r,m=h.parentNode;pn(m),m.append(h),e.items.clear()}na(e,t,!c)}else i={pending:new Set(t),done:new Set},(e.outrogroups??=new Set).add(i)}function na(e,t,r=!0){var n;if(e.pending.size>0){n=new Set;for(let a of e.pending.values())for(let f of a)n.add(e.items.get(f).e)}for(var o=0;o<t.length;o++){var i=t[o];if(n?.has(i)){i.f|=ht;let a=document.createDocumentFragment();zr(i,a)}else ae(t[o],r)}}var ml;function or(e,t,r,n,o,i=null){var a=e,f=new Map,c=(t&xi)!==0;if(c){var h=e;a=w?U(ie(h)):h.appendChild(se())}if(w)Oe();var m=null,v=Fr(()=>{var Y=r();return mt(Y)?Y:Y==null?[]:Nr(Y)});if(p)Ie(v,"{#each ...}");var _=new Map,d=!0;function y(Y){if((C.effect.f&W)!==0)return;C.pending.delete(Y);var ee=l(v);if(C.fallback=m,fc(C,ee,a,t,n),m!==null)if(ee.length===0)if((m.f&ht)===0)qr(m);else m.f^=ht,wn(m,null,a);else Ot(m,()=>{m=null})}function R(Y){C.pending.delete(Y)}var b=rt(()=>{var Y=l(v),ee=Y.length;let le=!1;if(w){var dt=_r(a)===Zt;if(dt!==(ee===0))a=At(),U(a),pe(!1),le=!0}var Ee=new Set,ke=T,wt=fo();for(var Le=0;Le<ee;Le+=1){if(w&&j.nodeType===at&&j.data===pr)a=j,le=!0,pe(!1);var qe=Y[Le],Gt=n(qe,Le);if(p){var ar=n(qe,Le);if(Gt!==ar)_s(String(Le),String(Gt),String(ar))}var _t=d?null:f.get(Gt);if(_t){if(_t.v)yt(_t.v,qe);if(_t.i)yt(_t.i,Le);if(wt)ke.unskip_effect(_t.e)}else{if(_t=cc(f,d?a:ml??=se(),qe,Gt,Le,o,t,r),!d)_t.e.f|=ht;f.set(Gt,_t)}Ee.add(Gt)}if(ee===0&&i&&!m)if(d)m=ve(()=>i(a));else m=ve(()=>i(ml??=se())),m.f|=ht;if(ee>Ee.size)if(p)uc(Y,n);else ki("","","");if(w&&ee>0)U(At());if(!d)if(_.set(ke,Ee),wt){for(let[kn,Jr]of f)if(!Ee.has(kn))ke.skip_effect(Jr.e);ke.oncommit(y),ke.ondiscard(R)}else y(ke);if(le)pe(!0);l(v)}),C={effect:b,flags:t,items:f,pending:_,outrogroups:null,fallback:m};if(d=!1,w)a=j}function $n(e){while(e!==null&&(e.f&ge)===0)e=e.next;return e}function fc(e,t,r,n,o){var i=(n&es)!==0,a=t.length,f=e.items,c=$n(e.effect.first),h,m=null,v,_=[],d=[],y,R,b,C;if(i){for(C=0;C<a;C+=1)if(y=t[C],R=o(y,C),b=f.get(R).e,(b.f&ht)===0)b.nodes?.a?.measure(),(v??=new Set).add(b)}for(C=0;C<a;C+=1){if(y=t[C],R=o(y,C),b=f.get(R).e,e.outrogroups!==null)for(let qe of e.outrogroups)qe.pending.delete(b),qe.done.delete(b);if((b.f&fe)!==0){if(qr(b),i)b.nodes?.a?.unfix(),(v??=new Set).delete(b)}if((b.f&ht)!==0)if(b.f^=ht,b===c)wn(b,null,r);else{var Y=m?m.next:c;if(b===e.effect.last)e.effect.last=b.prev;if(b.prev)b.prev.next=b.next;if(b.next)b.next.prev=b.prev;nr(e,m,b),nr(e,b,Y),wn(b,Y,r),m=b,_=[],d=[],c=$n(m.next);continue}if(b!==c){if(h!==void 0&&h.has(b)){if(_.length<d.length){var ee=d[0],le;m=ee.prev;var dt=_[0],Ee=_[_.length-1];for(le=0;le<_.length;le+=1)wn(_[le],ee,r);for(le=0;le<d.length;le+=1)h.delete(d[le]);nr(e,dt.prev,Ee.next),nr(e,m,dt),nr(e,Ee,ee),c=ee,m=Ee,C-=1,_=[],d=[]}else h.delete(b),wn(b,c,r),nr(e,b.prev,b.next),nr(e,b,m===null?e.effect.first:m.next),nr(e,m,b),m=b;continue}_=[],d=[];while(c!==null&&c!==b)(h??=new Set).add(c),d.push(c),c=$n(c.next);if(c===null)continue}if((b.f&ht)===0)_.push(b);m=b,c=$n(b.next)}if(e.outrogroups!==null){for(let qe of e.outrogroups)if(qe.pending.size===0)na(e,Nr(qe.done)),e.outrogroups?.delete(qe);if(e.outrogroups.size===0)e.outrogroups=null}if(c!==null||h!==void 0){var ke=[];if(h!==void 0){for(b of h)if((b.f&fe)===0)ke.push(b)}while(c!==null){if((c.f&fe)===0&&c!==e.fallback)ke.push(c);c=$n(c.next)}var wt=ke.length;if(wt>0){var Le=(n&xi)!==0&&a===0?r:null;if(i){for(C=0;C<wt;C+=1)ke[C].nodes?.a?.measure();for(C=0;C<wt;C+=1)ke[C].nodes?.a?.fix()}lc(e,ke,Le)}}if(i)ue(()=>{if(v===void 0)return;for(b of v)b.nodes?.a?.apply()})}function cc(e,t,r,n,o,i,a,f){var c=(a&Ja)!==0?(a&ts)===0?br(r,!1,!1):De(r):null,h=(a&Qa)!==0?De(o):null;if(p&&c)c.trace=()=>{f()[h?.v??o]};return{v:c,i:h,e:ve(()=>(i(t,c??r,h??o,f),()=>{e.delete(n)}))}}function wn(e,t,r){if(!e.nodes)return;var n=e.nodes.start,o=e.nodes.end,i=t&&(t.f&ht)===0?t.nodes.start:r;while(n!==null){var a=xe(n);if(i.before(n),n===o)return;n=a}}function nr(e,t,r){if(t===null)e.effect.first=r;else t.next=r;if(r===null)e.effect.last=t;else r.prev=t}function uc(e,t){let r=new Map,n=e.length;for(let o=0;o<n;o++){let i=t(e[o],o);if(r.has(i)){let a=String(r.get(i)),f=String(o),c=String(i);if(c.startsWith("[object "))c=null;ki(a,f,c)}r.set(i,o)}}var gl=[...` 	
\r\f \v\uFEFF`];function xl(e,t,r){var n=e==null?"":""+e;if(t)n=n?n+" "+t:t;if(r){for(var o of Object.keys(r))if(r[o])n=n?n+" "+o:o;else if(n.length){var i=o.length,a=0;while((a=n.indexOf(o,a))>=0){var f=a+i;if((a===0||gl.includes(n[a-1]))&&(f===n.length||gl.includes(n[f])))n=(a===0?"":n.substring(0,a))+n.substring(f+1);else a=f}}}return n===""?null:n}function ir(e,t,r,n,o,i){var a=e[sn];if(w||a!==r||a===void 0){var f=xl(r,n,i);if(!w||f!==e.getAttribute("class"))if(f==null)e.removeAttribute("class");else if(t)e.className=f;else e.setAttribute("class",f);e[sn]=r}else if(i&&o!==i)for(var c in i){var h=!!i[c];if(o==null||h!==!!o[c])e.classList.toggle(c,h)}return i}function ia(e,t){if(t){if(!e.hasAttribute("selected"))e.setAttribute("selected","")}else e.removeAttribute("selected")}function hc(e,t){var{__defaultValue:r,multiple:n}=e,o=n?r??[]:null;if(n&&!mt(o))return;var i=e.selectedIndex,a=t&&n?new Set(e.selectedOptions):null;for(var f of e.options){var c=Zr(f);ia(f,n?o.includes(c):so(c,r))}if(!t)return;if(a!==null)for(f of e.options){var h=a.has(f);if(f.selected!==h)f.selected=h}else if(e.selectedIndex!==i)e.selectedIndex=i}function Xr(e,t,r=!1){if(e.multiple){if(t==null)return;if(!mt(t))return ls();for(var n of e.options)n.selected=t.includes(Zr(n));return}for(n of e.options){var o=Zr(n);if(so(o,t)){n.selected=!0;return}}if(!r||t!==void 0)e.selectedIndex=-1}function Er(e){var t=new MutationObserver((r)=>{if(r.every(gc))return;if("__defaultValue"in e)hc(e,!1);if("__value"in e)Xr(e,e.__value)});t.observe(e,{childList:!0,subtree:!0,attributes:!0,attributeFilter:["value"]}),be(()=>{t.disconnect()})}function En(e,t,r=t){var n=new WeakSet,o=!0;dn(e,"change",(i)=>{var a=i?"[selected]":":checked",f;if(e.multiple)f=[].map.call(e.querySelectorAll(a),Zr);else{var c=e.querySelector(a)??e.querySelector("option:not([disabled])");f=c&&Zr(c)}if(r(f),e.__value=f,T!==null)n.add(T)}),tt(()=>{var i=t();if(e===document.activeElement){var a=ce?Qe:T;if(n.has(a))return}if(Xr(e,i,o),o&&i===void 0){var f=e.querySelector(":checked");if(f!==null)i=Zr(f),r(i)}e.__value=i,o=!1})}function Zr(e){if("__value"in e)return e.__value;else return e.value}function gc(e){if(e.target.closest("selectedcontent")!==null)return!0;if(e.type==="childList"){var t=[...e.addedNodes,...e.removedNodes];return t.length>0&&t.every((r)=>r.nodeName==="SELECTEDCONTENT")}return!1}var wc=Symbol("class"),Ec=Symbol("style"),kc=Symbol("is custom element"),Tc=Symbol("is html"),Ac=gi?"link":"LINK";function ot(e){if(!w)return;var t=!1,r=()=>{if(t)return;if(t=!0,e.hasAttribute("value")){var n=e.value;nt(e,"value",null),e.value=n}if(e.hasAttribute("checked")){var o=e.checked;nt(e,"checked",null),e.checked=o}};e[Wt]=r,ue(r),Qn()}function mo(e,t){var r=wl(e);if(r.checked===(r.checked=t??void 0))return;e.checked=t}function nt(e,t,r,n){var o=wl(e);if(w){if(o[t]=e.getAttribute(t),t==="src"||t==="srcset"||t==="href"&&e.nodeName===Ac){if(!n)Nc(e,t,r??"");return}}var i=o[t];if(i===(o[t]=r))return;if(t==="loading")e[Xa]=r;if(r==null)e.removeAttribute(t);else if(typeof r!=="string"&&Sc(e).has(t))e[t]=r;else if(!(w&&(typeof r==="number"||typeof r==="boolean")&&i===String(r)))e.setAttribute(t,r)}function wl(e){return e[Hn]??={[kc]:e.nodeName.includes("-"),[Tc]:e.namespaceURI===Vn}}var bl=new Map;function Sc(e){var t=e.getAttribute("is")||e.nodeName,r=bl.get(t);if(r)return r;bl.set(t,r=new Set);var n,o=e,i=Element.prototype;while(i!==o){n=ui(o);for(var a in n)if(n[a].set&&a!=="innerHTML"&&a!=="textContent"&&a!=="innerText")r.add(a);o=Rr(o)}return r}function Nc(e,t,r){if(!p)return;if(t==="srcset"&&Rc(e,r))return;if(aa(e.getAttribute(t)??"",r))return;as(t,e.outerHTML.replace(e.innerHTML,e.innerHTML&&"..."),String(r))}function aa(e,t){if(e===t)return!0;return new URL(e,document.baseURI).href===new URL(t,document.baseURI).href}function yl(e){return e.split(",").map((t)=>t.trim().split(" ").filter(Boolean))}function Rc(e,t){var r=yl(e.srcset),n=yl(t);return n.length===r.length&&n.every(([o,i],a)=>i===r[a][1]&&(aa(r[a][0],o)||aa(o,r[a][0])))}function It(e,t,r=t){var n=new WeakSet;if(dn(e,"input",async(o)=>{if(p&&e.type==="checkbox")Ei();var i=o?e.defaultValue:e.value;if(i=sa(e)?la(i):i,r(i),T!==null)n.add(T);if(await yn(),i!==(i=t())){var{selectionStart:a,selectionEnd:f}=e,c=e.value.length;if(e.value=i??"",f!==null){var h=e.value.length;if(a===f&&f===c&&h>c)e.selectionStart=h,e.selectionEnd=h;else e.selectionStart=a,e.selectionEnd=Math.min(f,h)}}}),w&&e.defaultValue!==e.value||J(t)==null&&e.value){if(r(sa(e)?la(e.value):e.value),T!==null)n.add(T)}we(()=>{if(p&&e.type==="checkbox")Ei();var o=t();if(e===document.activeElement){var i=ce?Qe:T;if(n.has(i))return}if(sa(e)&&o===la(e.value))return;if(e.type==="date"&&!o&&!e.value)return;if(o!==e.value)e.value=o??""})}function fa(e,t,r=t){if(dn(e,"change",(n)=>{var o=n?e.defaultChecked:e.checked;r(o)}),w&&e.defaultChecked!==e.checked||J(t)==null)r(e.checked);we(()=>{var n=t();e.checked=Boolean(n)})}function sa(e){var t=e.type;return t==="number"||t==="range"}function la(e){return e===""?null:+e}var Oy=Symbol("unmounted");function El(e){return new kl(e)}class kl{#e;#t;constructor(e){var t=new Map,r=(o,i)=>{var a=br(i,!1,!1);return t.set(o,a),a};let n=new Proxy({...e.props||{},$$events:{}},{get(o,i){return l(t.get(i)??r(i,Reflect.get(o,i)))},has(o,i){if(i===Bn)return!0;return l(t.get(i)??r(i,Reflect.get(o,i))),Reflect.has(o,i)},set(o,i,a){return A(t.get(i)??r(i,a),a),Reflect.set(o,i,a)}});if(this.#t=(e.hydrate?ta:Wr)(e.component,{target:e.target,anchor:e.anchor,props:n,context:e.context,intro:e.intro??!1,recover:e.recover,transformError:e.transformError}),!ce&&(!e?.props?.$$host||e.sync===!1))Qt();this.#e=n.$$events;for(let o of Object.keys(this.#t)){if(o==="$set"||o==="$destroy"||o==="$on")continue;he(this,o,{get(){return this.#t[o]},set(i){this.#t[o]=i},enumerable:!0})}this.#t.$set=(o)=>{Object.assign(n,o)},this.#t.$destroy=()=>{ra(this.#t)}}$set(e){this.#t.$set(e)}$on(e,t){this.#e[e]=this.#e[e]||[];let r=(...n)=>t.call(this,...n);return this.#e[e].push(r),()=>{this.#e[e]=this.#e[e].filter((n)=>n!==r)}}$destroy(){this.#t.$destroy()}}var Bc;if(typeof HTMLElement==="function")Bc=class extends HTMLElement{$$ctor;$$s;$$c;$$cn=!1;$$d={};$$r=!1;$$p_d={};$$l={};$$l_u=new Map;$$me;$$shadowRoot=null;constructor(e,t,r){super();if(this.$$ctor=e,this.$$s=t,r)this.$$shadowRoot=this.attachShadow(r)}addEventListener(e,t,r){if(this.$$l[e]=this.$$l[e]||[],this.$$l[e].push(t),this.$$c){let n=this.$$c.$on(e,t);this.$$l_u.set(t,n)}super.addEventListener(e,t,r)}removeEventListener(e,t,r){if(super.removeEventListener(e,t,r),this.$$c){let n=this.$$l_u.get(t);if(n)n(),this.$$l_u.delete(t)}}async connectedCallback(){if(this.$$cn=!0,!this.$$c){let e=function(n){return(o)=>{let i=jt("slot");if(n!=="default")i.name=n;D(o,i)}};if(await Promise.resolve(),!this.$$cn||this.$$c)return;let t={},r=Yc(this);for(let n of this.$$s)if(n in r)if(n==="default"&&!this.$$d.children)this.$$d.children=e(n),t.default=!0;else t[n]=e(n);for(let n of this.attributes){let o=this.$$g_p(n.name);if(!(o in this.$$d))this.$$d[o]=ua(o,n.value,this.$$p_d,"toProp")}for(let n in this.$$p_d)if(!(n in this.$$d)&&this[n]!==void 0)this.$$d[n]=this[n],delete this[n];this.$$c=El({component:this.$$ctor,target:this.$$shadowRoot||this,props:{...this.$$d,$$slots:t,$$host:this}}),this.$$me=Vi(()=>{we(()=>{this.$$r=!0;for(let n of ci(this.$$c)){if(!this.$$p_d[n]?.reflect)continue;this.$$d[n]=this.$$c[n];let o=ua(n,this.$$d[n],this.$$p_d,"toAttribute");if(o==null)this.removeAttribute(this.$$p_d[n].attribute||n);else this.setAttribute(this.$$p_d[n].attribute||n,o)}this.$$r=!1})});for(let n in this.$$l)for(let o of this.$$l[n]){let i=this.$$c.$on(n,o);this.$$l_u.set(o,i)}this.$$l={}}}attributeChangedCallback(e,t,r){if(this.$$r)return;e=this.$$g_p(e),this.$$d[e]=ua(e,r,this.$$p_d,"toProp"),this.$$c?.$set({[e]:this.$$d[e]})}disconnectedCallback(){this.$$cn=!1,Promise.resolve().then(()=>{if(!this.$$cn&&this.$$c)this.$$c.$destroy(),this.$$me(),this.$$c=void 0})}$$g_p(e){return ci(this.$$p_d).find((t)=>this.$$p_d[t].attribute===e||!this.$$p_d[t].attribute&&t.toLowerCase()===e)||e}};function ua(e,t,r,n){let o=r[e]?.type;if(t=o==="Boolean"&&typeof t!=="boolean"?t!=null:t,!n||!r[e])return t;else if(n==="toAttribute")switch(o){case"Object":case"Array":return t==null?null:JSON.stringify(t);case"Boolean":return t?"":null;case"Number":return t==null?null:t;default:return t}else switch(o){case"Object":case"Array":return t&&JSON.parse(t);case"Boolean":return t;case"Number":return t!=null?+t:t;default:return t}}function Yc(e){let t={};return e.childNodes.forEach((r)=>{t[r.slot||"default"]=!0}),t}if(p){let e=function(t){if(!(t in globalThis)){let r;Object.defineProperty(globalThis,t,{configurable:!0,get:()=>{if(r!==void 0)return r;bs(t)},set:(n)=>{r=n}})}};e("$state"),e("$effect"),e("$derived"),e("$inspect"),e("$props"),e("$bindable")}function Tl(e){if(L===null)zn("onMount");if(Xt&&L.l!==null)Gc(L).m.push(e);else Gr(()=>{let t=J(e);if(typeof t==="function")return t})}function Gc(e){var t=e.l;return t.u??={a:[],b:[],m:[]}}var Al="5";if(typeof window<"u")((window.__svelte??={}).v??=new Set).add(Al);var qc=re('<br class="svelte-1xjvnya8h1aa3"/> ',1),zc=re('<p class="waktu svelte-1xjvnya8h1aa3"> <!></p>'),Kc=re('<span class="svelte-1xjvnya8h1aa3"> </span>'),Wc=re('<div class="peringatan svelte-1xjvnya8h1aa3"></div>'),kr=re('<p class="ket svelte-1xjvnya8h1aa3"> </p>'),Zc=re('<img alt=""/> <!>',1),Xc=re('<button class="stop svelte-1xjvnya8h1aa3"> </button>'),Jc=re("<button> </button>"),Qc=re('<button class="aktif-jelajah svelte-1xjvnya8h1aa3"> </button>'),eu=re('<select aria-label="Lama jelajah" class="svelte-1xjvnya8h1aa3"><option class="svelte-1xjvnya8h1aa3">5 menit</option><option class="svelte-1xjvnya8h1aa3">10 menit</option><option class="svelte-1xjvnya8h1aa3">15 menit</option><option class="svelte-1xjvnya8h1aa3">30 menit</option></select> <button class="svelte-1xjvnya8h1aa3">Jelajah sendiri</button>',1),Sl=re('<small class="svelte-1xjvnya8h1aa3"> </small>'),tu=Zi('<line x1="0" class="svelte-1xjvnya8h1aa3"></line><text x="2" class="svelte-1xjvnya8h1aa3"> </text>',1),ru=re('<svg class="grafik svelte-1xjvnya8h1aa3" role="img"><!><path class="svelte-1xjvnya8h1aa3"></path></svg> <div class="sumbu svelte-1xjvnya8h1aa3"><span class="svelte-1xjvnya8h1aa3"> </span><span class="svelte-1xjvnya8h1aa3">sekarang</span></div>',1),nu=re('<p class="ket svelte-1xjvnya8h1aa3">Grafik muncul setelah robot menyala 2 menit (satu titik per menit, paling lama 6 jam).</p>'),ou=re(" <!>",1),iu=re('<li class="svelte-1xjvnya8h1aa3"><b class="svelte-1xjvnya8h1aa3"> </b> <span class="svelte-1xjvnya8h1aa3"> <small class="svelte-1xjvnya8h1aa3"> </small></span> <button class="hapus svelte-1xjvnya8h1aa3">✕</button></li>'),au=re('<ul class="alarm svelte-1xjvnya8h1aa3"></ul>'),su=re('<label class="svelte-1xjvnya8h1aa3"><input type="checkbox" class="svelte-1xjvnya8h1aa3"/> </label>'),lu=re('<div class="hari svelte-1xjvnya8h1aa3"></div>'),fu=re('<button><b class="svelte-1xjvnya8h1aa3"> </b> <span class="svelte-1xjvnya8h1aa3"> </span></button>'),cu=re('<p class="status svelte-1xjvnya8h1aa3"> </p>'),uu=re('<main class="svelte-1xjvnya8h1aa3"><header class="svelte-1xjvnya8h1aa3"><h1 class="svelte-1xjvnya8h1aa3"> </h1> <span></span> <span class="kecil svelte-1xjvnya8h1aa3"><!></span></header> <!> <!> <section class="bicara svelte-1xjvnya8h1aa3"><button> </button> <p class="ket kecil-ket svelte-1xjvnya8h1aa3">Ketuk, lalu bicara ke mikrofon robot. Robot berhenti merekam saat kamu diam (paling lama 6 detik).</p> <label class="saklar svelte-1xjvnya8h1aa3"><input type="checkbox" class="svelte-1xjvnya8h1aa3"/> </label></section> <section class="kamera svelte-1xjvnya8h1aa3"><!> <button class="kecil-btn svelte-1xjvnya8h1aa3"> </button></section> <section class="svelte-1xjvnya8h1aa3"><h2 class="svelte-1xjvnya8h1aa3">Gerak <small class="svelte-1xjvnya8h1aa3">tahan untuk jalan, lepas untuk berhenti</small></h2> <div class="dpad svelte-1xjvnya8h1aa3"></div> <label class="svelte-1xjvnya8h1aa3"> <input type="range" min="100" max="255" class="svelte-1xjvnya8h1aa3"/></label> <div class="baris belok svelte-1xjvnya8h1aa3"><button class="svelte-1xjvnya8h1aa3">⟲ 90° kiri</button> <button class="svelte-1xjvnya8h1aa3">↻ balik</button> <button class="svelte-1xjvnya8h1aa3">90° kanan ⟳</button></div> <p class="ket kecil-ket svelte-1xjvnya8h1aa3">Belok tepat memakai giroskop di badan; sudutnya tidak berubah walau baterai melemah.</p> <div class="baris jelajah svelte-1xjvnya8h1aa3"><!></div> <p class="ket kecil-ket svelte-1xjvnya8h1aa3">Maju sampai ada halangan, lalu belok ke arah acak. Berhenti sendiri kalau waktunya habis, baterai tinggal 10%, atau tombol STOP ditekan. Tombol arah juga menghentikannya.</p></section> <section class="svelte-1xjvnya8h1aa3"><h2 class="svelte-1xjvnya8h1aa3">Baterai <!></h2> <!> <p class="ket kecil-ket svelte-1xjvnya8h1aa3"><!> Peringatan dikirim ke Telegram saat tinggal 20% dan 10%.</p></section> <section class="svelte-1xjvnya8h1aa3"><h2 class="svelte-1xjvnya8h1aa3">Kepala</h2> <label class="svelte-1xjvnya8h1aa3"> <input type="range" min="-60" max="60" class="svelte-1xjvnya8h1aa3"/></label> <label class="svelte-1xjvnya8h1aa3"> <input type="range" min="40" max="150" class="svelte-1xjvnya8h1aa3"/></label> <button class="kecil-btn svelte-1xjvnya8h1aa3">Lurus ke depan</button></section> <section class="svelte-1xjvnya8h1aa3"><h2 class="svelte-1xjvnya8h1aa3">Musik dan suara</h2> <div class="baris svelte-1xjvnya8h1aa3"><button class="svelte-1xjvnya8h1aa3">Lagu berikutnya</button> <button class="svelte-1xjvnya8h1aa3">Radio berikutnya</button> <button class="svelte-1xjvnya8h1aa3">Diam</button></div> <label class="svelte-1xjvnya8h1aa3"> <input type="range" min="0" max="21" class="svelte-1xjvnya8h1aa3"/></label> <label class="suara svelte-1xjvnya8h1aa3">Suara bicara <select class="svelte-1xjvnya8h1aa3"><option class="svelte-1xjvnya8h1aa3"> </option><option class="svelte-1xjvnya8h1aa3">Google</option></select></label> <p class="ket kecil-ket svelte-1xjvnya8h1aa3"><!></p> <form class="svelte-1xjvnya8h1aa3"><input type="text" placeholder="Suruh Zond bilang..." maxlength="140" class="svelte-1xjvnya8h1aa3"/> <button type="submit" class="svelte-1xjvnya8h1aa3">Bilang</button></form> <form class="svelte-1xjvnya8h1aa3"><input type="text" placeholder="Pesan untuk layar Zond..." maxlength="190" class="svelte-1xjvnya8h1aa3"/> <button type="submit" class="svelte-1xjvnya8h1aa3">Kirim</button></form></section> <section class="svelte-1xjvnya8h1aa3"><h2 class="svelte-1xjvnya8h1aa3">Alarm dan pengingat</h2> <!> <form class="form-alarm svelte-1xjvnya8h1aa3"><div class="baris svelte-1xjvnya8h1aa3"><input type="time" required="" class="svelte-1xjvnya8h1aa3"/> <select class="svelte-1xjvnya8h1aa3"><option class="svelte-1xjvnya8h1aa3">Sekali</option><option class="svelte-1xjvnya8h1aa3">Setiap hari</option><option class="svelte-1xjvnya8h1aa3">Senin-Jumat</option><option class="svelte-1xjvnya8h1aa3">Sabtu-Minggu</option><option class="svelte-1xjvnya8h1aa3">Pilih hari</option></select></div> <!> <input type="text" placeholder="Pesan, misalnya: bangun, minum obat" maxlength="79" class="svelte-1xjvnya8h1aa3"/> <div class="baris svelte-1xjvnya8h1aa3"><select class="svelte-1xjvnya8h1aa3"><option class="svelte-1xjvnya8h1aa3">Suara saja</option><option class="svelte-1xjvnya8h1aa3">Suara + radio</option><option class="svelte-1xjvnya8h1aa3">Suara + lagu SD</option></select> <button type="submit" class="svelte-1xjvnya8h1aa3">Pasang</button></div> <!></form> <p class="ket kecil-ket svelte-1xjvnya8h1aa3">Alarm sebelum jam 11 siang sekalian menyebut cuaca hari ini. Pengingat juga dikirim ke Telegram.</p></section> <section class="svelte-1xjvnya8h1aa3"><h2 class="svelte-1xjvnya8h1aa3">Fitur kamera AI</h2> <label class="saklar svelte-1xjvnya8h1aa3"><input type="checkbox" class="svelte-1xjvnya8h1aa3"/> Aktifkan "lihat sekeliling" dan "suit". Tiap kali dipakai, foto dikirim ke AI dan memakai jatah AI lebih cepat.</label> <div class="baris svelte-1xjvnya8h1aa3"><button class="svelte-1xjvnya8h1aa3">Lihat sekeliling</button> <button class="svelte-1xjvnya8h1aa3">Main suit</button></div> <!> <!></section> <section class="svelte-1xjvnya8h1aa3"><h2 class="svelte-1xjvnya8h1aa3">Otak AI <!></h2> <div class="ai svelte-1xjvnya8h1aa3"></div> <!> <p class="ket kecil-ket svelte-1xjvnya8h1aa3">Kalau pilihanmu gagal (jatah habis atau saldo kosong), robot otomatis memakai yang gratis.</p></section> <!></main>');function pu(e,t){cn(t,!0);let r=P(5);async function n(s){await Q(`/api/jelajah?menit=${s}`),await lr()}function o(s){let u=Math.max(0,s);return`${Math.floor(u/60)}:${String(u%60).padStart(2,"0")}`}let i=P(ut([]));async function a(){let s=await Q("/api/baterai");if(s&&s.ok)A(i,(await s.json()).v,!0)}let f=6.4,c=8.4,h=320,m=120;function v(s){let u=(Math.min(c,Math.max(f,s))-f)/(c-f);return m-4-u*(m-8)}let _=no(()=>{let s=l(i).length;if(s<2)return"";return l(i).map((u,g)=>`${g?"L":"M"}${(g/(s-1)*h).toFixed(1)},${v(u/100).toFixed(1)}`).join("")});function d(s){if(s<60)return`${s} menit`;return`${Math.floor(s/60)} jam${s%60?` ${s%60} menit`:""}`}async function y(s){await Q(`/api/suara?pakai=${s.currentTarget.value}`),await lr()}function R(s){Q(`/api/belok?d=${s}`)}let b=["Min","Sen","Sel","Rab","Kam","Jum","Sab"],C=["suara","radio","lagu SD"],Y=P(ut([])),ee=P("06:00"),le=P("0"),dt=ut([!1,!0,!0,!0,!0,!0,!1]),Ee=P(""),ke=P(0),wt=P("");async function Le(){let s=await Q("/api/alarm");if(s&&s.ok)A(Y,(await s.json()).alarm,!0)}async function qe(s){s.preventDefault();let u=l(le)==="pilih"?dt.reduce((H,B,K)=>B?H|1<<K:H,0):Number(l(le)),g=new URLSearchParams({jam:l(ee),hari:String(u),pesan:l(Ee).trim(),bunyi:String(l(ke))}),I=await Q(`/api/alarm/tambah?${g}`);if(A(wt,!I?"Robot tidak menjawab":I.ok?"":await I.text(),!0),I?.ok)A(Ee,"");await Le()}async function Gt(s){await Q(`/api/alarm/hapus?i=${s}`),await Le()}let ar=P("");async function _t(s){let u=s.currentTarget.checked;await Q(`/api/fiturkamera?on=${u?1:0}`),await lr()}async function kn(s,u){let g=await Q(s);A(ar,!g?"Robot tidak menjawab":g.ok?u:await g.text(),!0),setTimeout(()=>A(ar,""),6000)}let Jr=P(!1);async function Rl(){A(Jr,!0),await Q("/api/bicara"),setTimeout(()=>A(Jr,!1),1500)}async function jl(s){let u=s.currentTarget.checked;await Q(`/api/dengar?on=${u?1:0}`),await lr()}let Cl=[{id:"otomatis",nama:"Otomatis",ket:"gratis: Gemini, lalu Gemma, lalu Groq"},{id:"gemini",nama:"Gemini",ket:"gratis, jatah harian kecil"},{id:"gemma",nama:"Gemma",ket:"gratis, jatah besar"},{id:"groq",nama:"Groq",ket:"gratis, cepat"},{id:"haiku",nama:"Claude Haiku",ket:"berbayar per pemakaian"}],go=P(""),Qr=P("");async function Ol(s){A(go,s,!0),A(Qr,"");let u=await Q(`/api/ai?m=${s}`);if(!u)A(Qr,"Robot tidak menjawab");else if(!u.ok)A(Qr,await u.text(),!0);await lr(),A(go,"")}let E=P(null),Dt=P(!1),en=P(160),Tr=P(null),Ar=P(0),tn=P(90),rn=P(12),xo=!1,Tn=P(""),sr=P(!0),An=P(!1),pa=P("");async function Q(s,u=1500){let g=new AbortController,I=setTimeout(()=>g.abort(),u);try{return await fetch(s,{signal:g.signal,cache:"no-store"})}catch{return null}finally{clearTimeout(I)}}async function lr(){let s=await Q("/api/status");if(!s||!s.ok){A(Dt,!1);return}if(A(Dt,!0),A(E,await s.json(),!0),!xo)A(rn,l(E).vol,!0)}let Il={maju:[1,1],mundur:[-1,-1],kiri:[-1,1],kanan:[1,-1],majukiri:[0.35,1],majukanan:[1,0.35],mundurkiri:[-0.35,-1],mundurkanan:[-1,-0.35]},Sr=null,bo=!1;function da(){if(!l(Tr)||bo)return;let[s,u]=Il[l(Tr)];bo=!0,Q(`/api/gerak?l=${Math.round(s*l(en))}&r=${Math.round(u*l(en))}`,600).finally(()=>bo=!1)}function _a(s){if(A(Tr,s,!0),da(),Sr)clearInterval(Sr);Sr=setInterval(da,150)}function Mt(){if(Sr)clearInterval(Sr);if(Sr=null,l(Tr))Q("/api/stop");A(Tr,null)}function Dl(s,u){s.currentTarget.setPointerCapture(s.pointerId),_a(u)}let va={ArrowUp:"maju",w:"maju",ArrowDown:"mundur",s:"mundur",ArrowLeft:"kiri",a:"kiri",ArrowRight:"kanan",d:"kanan"};function Ml(s){if(s.target.tagName==="INPUT")return;if(s.key===" "){s.preventDefault(),Mt(),Q("/api/stop");return}let u=va[s.key];if(u&&!s.repeat)s.preventDefault(),_a(u)}function Ll(s){if(va[s.key])Mt()}let ma=0,yo=null;function $o(){let s=Date.now();if(yo)clearTimeout(yo);let u=()=>{ma=Date.now(),Q(`/api/kepala?p=${90-l(Ar)}&t=${l(tn)}`)};if(s-ma>150)u();else yo=setTimeout(u,150)}function Fl(){A(Ar,0),A(tn,90),$o()}async function Pl(){xo=!0,await Q(`/api/volume?v=${l(rn)}`),setTimeout(()=>xo=!1,1500)}let Sn=P("");async function Bl(s){s.preventDefault();let u=l(Sn).trim();if(!u)return;A(Sn,""),await Q(`/api/pesan?teks=${encodeURIComponent(u)}`,3000)}async function Yl(s){s.preventDefault();let u=l(Tn).trim();if(!u)return;A(Tn,""),await Q(`/api/bilang?t=${encodeURIComponent(u)}`,3000)}function Nn(s){setTimeout(()=>{if(!l(sr)||!l(E)?.kamera){if(l(sr))Nn(1000);return}A(pa,`http://${l(E).kamera}/capture?res=vga&t=${Date.now()}`)},s)}function Hl(s){let u=[];if(s&1)u.push("Tombol STOP terkunci");if(s&2)u.push("Baterai lemah");if(s&4)u.push("Halangan depan");if(s&8)u.push("Halangan belakang");if(s&16)u.push("Robot miring");return u}Tl(()=>{lr(),Le(),a();let s=setInterval(lr,1000),u=setInterval(Le,30000),g=setInterval(a,60000),I=()=>Mt(),H=()=>document.hidden&&Mt();return window.addEventListener("blur",I),document.addEventListener("visibilitychange",H),()=>{clearInterval(s),clearInterval(u),clearInterval(g),window.removeEventListener("blur",I),document.removeEventListener("visibilitychange",H),Mt()}}),Gr(()=>{if(l(sr))Nn(0)});var wo=uu();pt("keydown",Ur,Ml),pt("keyup",Ur,Ll);var Eo=O(wo),ha=O(Eo),Vl=X(ha,!0),ga=x(ha,2);let xa;var ba=x(ga,2),Ul=O(ba);{var Gl=(s)=>{var u=Ut("otak tidak terhubung");D(s,u)},ql=(s)=>{var u=Ut("badan tidak menjawab");D(s,u)},zl=(s)=>{var u=Ut();V((g)=>M(u,`${l(E).bateraiPersen>=0?`${l(E).bateraiPersen}% · `:""}${g??""} V · depan ${l(E).depan??""} cm · belakang ${l(E).belakang??""} cm`),[()=>l(E).baterai.toFixed(1)]),D(s,u)};ne(Ul,(s)=>{if(!l(Dt))s(Gl);else if(!l(E)?.hidup)s(ql,1);else s(zl,-1)})}S(ba),S(Eo);var ya=x(Eo,2);{var Kl=(s)=>{var u=zc(),g=O(u),I=x(g);{var H=(B)=>{var K=qc(),Te=x(Vt(K),1,!0);V(()=>M(Te,l(E).cuaca)),D(B,K)};ne(I,(B)=>{if(l(E).cuaca)B(H)})}S(u),V(()=>M(g,`${l(E).jam??""} · ${l(E).tanggal??""}`)),D(s,u)};ne(ya,(s)=>{if(l(E)?.jam)s(Kl)})}var $a=x(ya,2);{var Wl=(s)=>{var u=Wc();or(u,20,()=>Hl(l(E).flag),(g)=>g,(g,I)=>{var H=Kc(),B=X(H,!0);V(()=>M(B,I)),D(g,H)}),S(u),D(s,u)};ne($a,(s)=>{if(l(E)&&l(E).flag)s(Wl)})}var ko=x($a,2),nn=O(ko);let wa;var Zl=X(nn,!0),Ea=x(nn,4),on=O(Ea);ot(on);var Xl=x(on);S(Ea),S(ko);var To=x(ko,2),ka=O(To);{var Jl=(s)=>{var u=Zc(),g=Vt(u);let I;var H=x(g,2);{var B=(K)=>{var Te=kr(),Ae=X(Te);V(()=>M(Ae,`Kamera tidak terhubung (${l(E).kamera??""})`)),D(K,Te)};ne(H,(K)=>{if(l(An))K(B)})}V(()=>{nt(g,"src",l(pa)),I=ir(g,1,"svelte-1xjvnya8h1aa3",null,I,{redup:l(An)})}),pt("load",g,()=>{A(An,!1),Nn(100)}),pt("error",g,()=>{A(An,!0),Nn(1500)}),Ki(g),D(s,u)},Ql=(s)=>{var u=kr(),g=X(u,!0);V(()=>M(g,l(E)?.kamera?"Kamera dimatikan":"Alamat kamera belum diketahui")),D(s,u)};ne(ka,(s)=>{if(l(sr)&&l(E)?.kamera)s(Jl);else s(Ql,-1)})}var Ta=x(ka,2),ef=X(Ta,!0);S(To);var Ao=x(To,2),Rn=x(O(Ao),2);or(Rn,20,()=>[["majukiri","↖"],["maju","▲"],["majukanan","↗"],["kiri","◀"],["stop","■"],["kanan","▶"],["mundurkiri","↙"],["mundur","▼"],["mundurkanan","↘"]],([s,u])=>s,(s,u)=>{var g=no(()=>_i(u,2));let I=()=>l(g)[0],H=()=>l(g)[1];var B=Xi(),K=Vt(B);{var Te=(me)=>{var Ce=Xc(),vt=X(Ce,!0);V(()=>M(vt,H())),z("pointerdown",Ce,()=>{Mt(),Q("/api/stop")}),D(me,Ce)},Ae=(me)=>{var Ce=Jc();let vt;var fr=X(Ce,!0);V(()=>{vt=ir(Ce,1,"svelte-1xjvnya8h1aa3",null,vt,{aktif:l(Tr)===I()}),M(fr,H())}),z("pointerdown",Ce,(fi)=>Dl(fi,I())),z("pointerup",Ce,Mt),pt("pointercancel",Ce,Mt),pt("lostpointercapture",Ce,Mt),D(me,Ce)};ne(K,(me)=>{if(I()==="stop")me(Te);else me(Ae,-1)})}D(s,B)}),S(Rn);var So=x(Rn,2),Aa=O(So),Sa=x(Aa);ot(Sa),S(So);var No=x(So,2),Na=O(No),Ra=x(Na,2),tf=x(Ra,2);S(No);var ja=x(No,4),rf=O(ja);{var nf=(s)=>{var u=Qc(),g=X(u);V((I)=>M(g,`Berhenti menjelajah (sisa ${I??""})`),[()=>o(l(E).jelajahSisa)]),z("click",u,()=>n(0)),D(s,u)},of=(s)=>{var u=eu(),g=Vt(u),I=O(g);I.value=I.__value=5;var H=x(I);H.value=H.__value=10;var B=x(H);B.value=B.__value=15;var K=x(B);K.value=K.__value=30,S(g),Er(g);var Te=x(g,2);V(()=>Te.disabled=!l(Dt)||!l(E)?.hidup),En(g,()=>l(r),(Ae)=>A(r,Ae)),z("click",Te,()=>n(l(r))),D(s,u)};ne(rf,(s)=>{if(l(E)?.mode==="jelajah")s(nf);else s(of,-1)})}S(ja),Xe(2),S(Ao);var Ro=x(Ao,2),jo=O(Ro),af=x(O(jo));{var sf=(s)=>{var u=Sl(),g=X(u);V((I)=>M(g,`${l(E).bateraiPersen??""}% · ${I??""} V`),[()=>l(E).baterai.toFixed(2)]),D(s,u)};ne(af,(s)=>{if(l(E)&&l(E).bateraiPersen>=0)s(sf)})}S(jo);var Ca=x(jo,2);{var lf=(s)=>{var u=ru(),g=Vt(u);nt(g,"viewBox","0 0 320 120");var I=O(g);or(I,16,()=>[8.4,7.8,7.2,6.6],(Ae)=>Ae,(Ae,me)=>{var Ce=tu(),vt=Vt(Ce);nt(vt,"x2",h);var fr=x(vt),fi=X(fr);V((Of,If,Df,Mf)=>{nt(vt,"y1",Of),nt(vt,"y2",If),nt(fr,"y",Df),M(fi,`${Mf??""} V`)},[()=>v(me),()=>v(me),()=>me===c?v(me)+11:v(me)-3,()=>me.toFixed(1)]),D(Ae,Ce)});var H=x(I);S(g);var B=x(g,2),K=O(B),Te=X(K);Xe(),S(B),V((Ae,me)=>{nt(g,"aria-label",`Tegangan baterai ${Ae??""} terakhir`),nt(H,"d",l(_)),M(Te,`${me??""} lalu`)},[()=>d(l(i).length),()=>d(l(i).length)]),D(s,u)},ff=(s)=>{var u=nu();D(s,u)};ne(Ca,(s)=>{if(l(i).length>=2)s(lf);else s(ff,-1)})}var Oa=x(Ca,2),cf=O(Oa);{var uf=(s)=>{var u=Ut();V((g)=>M(u,`Perkiraan sisa ${g??""}, kalau pemakaiannya seperti setengah jam terakhir.`),[()=>d(l(E).sisaMenit)]),D(s,u)},pf=(s)=>{var u=Ut("Perkiraan sisa waktu muncul setelah sekitar 10 menit menyala.");D(s,u)};ne(cf,(s)=>{if(l(E)&&l(E).sisaMenit>=0)s(uf);else s(pf,-1)})}Xe(),S(Oa),S(Ro);var Co=x(Ro,2),Oo=x(O(Co),2),Ia=O(Oo),Io=x(Ia);ot(Io),S(Oo);var Do=x(Oo,2),Da=O(Do),Mo=x(Da);ot(Mo),S(Do);var df=x(Do,2);S(Co);var Lo=x(Co,2),Fo=x(O(Lo),2),Ma=O(Fo),La=x(Ma,2),_f=x(La,2);S(Fo);var Po=x(Fo,2),Fa=O(Po),Bo=x(Fa);ot(Bo),S(Po);var Yo=x(Po,2),qt=x(O(Yo)),an=O(qt),vf=X(an);an.value=an.__value="gemini";var Pa=x(an);Pa.value=Pa.__value="google",S(qt);var jn;Er(qt),S(Yo);var Ho=x(Yo,2),mf=O(Ho);{var hf=(s)=>{var u=Ut("Suara cowok belum aktif: isi SUARA_GEMINI dan GEMINI_API_KEY di kode.");D(s,u)},gf=(s)=>{var u=ou(),g=Vt(u),I=x(g);{var H=(B)=>{var K=Ut();V(()=>M(K,`Galat terakhir: kode ${l(E).suaraGalat??""}, sementara memakai suara Google.`)),D(B,K)};ne(I,(B)=>{if(l(E).suaraGalat)B(H)})}V(()=>M(g,`Suara Gemini hari ini ${l(E).suaraHariIni??""} kalimat (jatah gratis harian).`)),D(s,u)};ne(mf,(s)=>{if(!l(E)?.suaraSiap)s(hf);else s(gf,-1)})}S(Ho);var Cn=x(Ho,2),Ba=O(Cn);ot(Ba),Xe(2),S(Cn);var Vo=x(Cn,2),Ya=O(Vo);ot(Ya),Xe(2),S(Vo),S(Lo);var Uo=x(Lo,2),Ha=x(O(Uo),2);{var xf=(s)=>{var u=au();or(u,21,()=>l(Y),oa,(g,I,H)=>{var B=iu(),K=O(B),Te=X(K,!0),Ae=x(K,2),me=O(Ae,!0),Ce=x(me),vt=X(Ce);S(Ae);var fr=x(Ae,2);S(B),V(()=>{M(Te,l(I).jam),M(me,l(I).pesan),M(vt,`${l(I).kapan??""} · ${C[l(I).bunyi]??"suara"??""}`),nt(fr,"aria-label",`Hapus alarm ${l(I).jam??""}`)}),z("click",fr,()=>Gt(H)),D(g,B)}),S(u),D(s,u)},bf=(s)=>{var u=kr(),g=X(u);V(()=>M(g,`Belum ada alarm. Bisa juga lewat suara: "${l(E)?.nama??"Zond"??""}, ingatkan aku jam 5 sore minum obat".`)),D(s,u)};ne(Ha,(s)=>{if(l(Y).length)s(xf);else s(bf,-1)})}var Go=x(Ha,2),qo=O(Go),zo=O(qo);ot(zo);var On=x(zo,2),Ko=O(On);Ko.value=Ko.__value="0";var Wo=x(Ko);Wo.value=Wo.__value="127";var Zo=x(Wo);Zo.value=Zo.__value="62";var Xo=x(Zo);Xo.value=Xo.__value="65";var Va=x(Xo);Va.value=Va.__value="pilih",S(On),Er(On),S(qo);var Ua=x(qo,2);{var yf=(s)=>{var u=lu();or(u,22,()=>b,(g)=>g,(g,I,H)=>{var B=su(),K=O(B);ot(K);var Te=x(K,1,!0);S(B),V(()=>M(Te,I)),fa(K,()=>dt[l(H)],(Ae)=>dt[l(H)]=Ae),D(g,B)}),S(u),D(s,u)};ne(Ua,(s)=>{if(l(le)==="pilih")s(yf)})}var Jo=x(Ua,2);ot(Jo);var Qo=x(Jo,2),In=O(Qo),ei=O(In);ei.value=ei.__value=0;var ti=x(ei);ti.value=ti.__value=1;var Ga=x(ti);Ga.value=Ga.__value=2,S(In),Er(In),Xe(2),S(Qo);var $f=x(Qo,2);{var wf=(s)=>{var u=kr(),g=X(u,!0);V(()=>M(g,l(wt))),D(s,u)};ne($f,(s)=>{if(l(wt))s(wf)})}S(Go),Xe(2),S(Uo);var ri=x(Uo,2),ni=x(O(ri),2),Dn=O(ni);ot(Dn),Xe(),S(ni);var oi=x(ni,2),ii=O(oi),qa=x(ii,2);S(oi);var za=x(oi,2);{var Ef=(s)=>{var u=kr(),g=X(u);V(()=>M(g,`Skor suit: kamu ${l(E).skorKamu??""}, ${l(E).nama??""} ${l(E).skorZond??""}`)),D(s,u)};ne(za,(s)=>{if(l(E)?.fiturKamera&&(l(E).skorKamu||l(E).skorZond))s(Ef)})}var kf=x(za,2);{var Tf=(s)=>{var u=kr(),g=X(u,!0);V(()=>M(g,l(ar))),D(s,u)};ne(kf,(s)=>{if(l(ar))s(Tf)})}S(ri);var ai=x(ri,2),si=O(ai),Af=x(O(si));{var Sf=(s)=>{var u=Sl(),g=X(u);V(()=>M(g,`terakhir menjawab: ${l(E).aiTerakhir??""}`)),D(s,u)};ne(Af,(s)=>{if(l(E)?.aiTerakhir)s(Sf)})}S(si);var li=x(si,2);or(li,21,()=>Cl,(s)=>s.id,(s,u)=>{var g=fu();let I;var H=O(g),B=X(H,!0),K=x(H,2),Te=X(K,!0);S(g),V(()=>{g.disabled=!l(E)?.aiTersedia?.[l(u).id]||l(go)!=="",I=ir(g,1,"svelte-1xjvnya8h1aa3",null,I,{pilih:l(E)?.ai===l(u).id}),M(B,l(u).nama),M(Te,l(E)?.aiTersedia?.[l(u).id]===!1?"API key belum diisi":l(u).ket)}),z("click",g,()=>Ol(l(u).id)),D(s,g)}),S(li);var Nf=x(li,2);{var Rf=(s)=>{var u=kr(),g=X(u,!0);V(()=>M(g,l(Qr))),D(s,u)};ne(Nf,(s)=>{if(l(Qr))s(Rf)})}Xe(2),S(ai);var jf=x(ai,2);{var Cf=(s)=>{var u=cu(),g=X(u,!0);V(()=>M(g,l(E).status)),D(s,u)};ne(jf,(s)=>{if(l(E)?.status)s(Cf)})}S(wo),V(()=>{if(M(Vl,l(E)?.nama??"Robot"),xa=ir(ga,1,"titik svelte-1xjvnya8h1aa3",null,xa,{on:l(Dt)&&l(E)?.hidup}),wa=ir(nn,1,"mik svelte-1xjvnya8h1aa3",null,wa,{dengar:l(E)?.mendengar||l(Jr)}),nn.disabled=!l(Dt),M(Zl,l(E)?.mendengar?"Mendengarkan... bicara sekarang":"Bicara ke "+(l(E)?.nama??"robot")),mo(on,l(E)?.dengarTerus??!1),on.disabled=!l(Dt),M(Xl,` Dengar terus (cukup sebut "${l(E)?.nama??"nama robot"??""}"). Boros: setiap suara di ruangan memakai jatah suara-ke-teks.`),M(ef,l(sr)?"Matikan kamera":"Nyalakan kamera"),M(Aa,`Kecepatan ${l(en)??""}`),M(Ia,`Toleh ${l(Ar)<0?"kiri":l(Ar)>0?"kanan":"lurus"}`),M(Da,`Angguk ${l(tn)??""}°`),M(Fa,`Volume ${l(rn)??""}`),qt.disabled=!l(Dt),an.disabled=!l(E)?.suaraSiap,M(vf,`Cowok (${(l(E)?.suaraNama||"Gemini")??""})`),jn!==(jn=l(E)?.suara??"gemini"))qt.value=(qt.__value=jn)??"",Xr(qt,jn);mo(Dn,l(E)?.fiturKamera??!1),Dn.disabled=!l(Dt),ii.disabled=!l(E)?.fiturKamera,qa.disabled=!l(E)?.fiturKamera}),z("click",nn,Rl),z("change",on,jl),z("click",Ta,()=>A(sr,!l(sr))),z("contextmenu",Rn,(s)=>s.preventDefault()),It(Sa,()=>l(en),(s)=>A(en,s)),z("click",Na,()=>R(-90)),z("click",Ra,()=>R(180)),z("click",tf,()=>R(90)),z("input",Io,$o),It(Io,()=>l(Ar),(s)=>A(Ar,s)),z("input",Mo,$o),It(Mo,()=>l(tn),(s)=>A(tn,s)),z("click",df,Fl),z("click",Ma,()=>Q("/api/lagu",4000)),z("click",La,()=>Q("/api/radio",4000)),z("click",_f,()=>Q("/api/diam")),z("change",Bo,Pl),It(Bo,()=>l(rn),(s)=>A(rn,s)),z("change",qt,y),pt("submit",Cn,Yl),It(Ba,()=>l(Tn),(s)=>A(Tn,s)),pt("submit",Vo,Bl),It(Ya,()=>l(Sn),(s)=>A(Sn,s)),pt("submit",Go,qe),It(zo,()=>l(ee),(s)=>A(ee,s)),En(On,()=>l(le),(s)=>A(le,s)),It(Jo,()=>l(Ee),(s)=>A(Ee,s)),En(In,()=>l(ke),(s)=>A(ke,s)),z("change",Dn,_t),z("click",ii,()=>kn("/api/sekeliling","Zond menoleh dan memotret. Ceritanya dikirim ke Telegram.")),z("click",qa,()=>kn("/api/suit","Siapkan tanganmu di depan kamera!")),D(e,wo),un()}var Nl=pu;uo(["click","change","contextmenu","pointerdown","pointerup","input"]);Wr(Nl,{target:document.body});
</script></body></html>)PANEL";
// </PANEL_WEB>

WebServer web(80);

int argAngka(const char* nama, int bawaan, int lo, int hi) {
  return web.hasArg(nama) ? constrain(web.arg(nama).toInt(), lo, hi) : bawaan;
}
void jawabOk() { web.send(200, "text/plain", "ok"); }

void siapkanWeb() {
  web.on("/", HTTP_GET, [] { web.send(200, "text/html", PANEL_HTML); });
  web.on("/api/status", HTTP_GET, [] {
    JsonDocument d;
    d["nama"] = NAMA_ROBOT;
    d["hidup"] = badan.terakhir && millis() - badan.terakhir < 2000;
    d["baterai"] = badan.baterai; d["depan"] = badan.depan; d["belakang"] = badan.belakang; d["flag"] = badan.flag;
    d["vol"] = volSekarang; d["status"] = teksStatus; d["kamera"] = alamatKamera;
    d["mode"] = mode == M_IKUTI ? "ikuti" : mode == M_JAGA ? "jaga" : mode == M_JELAJAH ? "jelajah" : "biasa";
    d["jelajahSisa"] = mode == M_JELAJAH ? (long)(jelajahSampai - millis()) / 1000 : 0;
    d["bateraiPersen"] = (int)persenBat; d["sisaMenit"] = (int)sisaMenitBat;
    d["suara"] = pakaiSuaraAI ? "gemini" : "google";
    d["suaraSiap"] = strlen(SUARA_GEMINI) > 0 && strlen(GEMINI_API_KEY) > 0 && adaFFat;
    d["suaraNama"] = SUARA_GEMINI; d["suaraHariIni"] = (uint32_t)suaraHariIni; d["suaraGalat"] = (int)suaraGalat;
    d["ai"] = namaAI[pilihanAI]; d["aiTerakhir"] = aiTerakhir;
    d["dengarTerus"] = (bool)dengarTerus; d["mendengar"] = ekspresi == DENGAR;
    d["fiturKamera"] = (bool)fiturKamera; d["skorKamu"] = skorKamu; d["skorZond"] = skorZond;
    d["cuaca"] = teksCuaca(true); d["jam"] = teksJam(); d["tanggal"] = teksTanggal();
    JsonObject ada = d["aiTersedia"].to<JsonObject>();
    for (int i = 0; i < AI_JUMLAH; i++) ada[namaAI[i]] = aiTersedia(i);
    String out; serializeJson(d, out);
    web.send(200, "application/json", out);
  });
  // Gerak 350 ms. Panel mengirim ulang tiap 150 ms selama tombol ditahan; kalau kiriman berhenti
  // (jari diangkat, WiFi putus), badan berhenti sendiri. Badan tetap menolak maju kalau ada halangan.
  web.on("/api/gerak", HTTP_GET, [] {
    if (mode != M_BIASA) gantiMode(M_BIASA);
    gerak(argAngka("l", 0, -255, 255), argAngka("r", 0, -255, 255), 350);
    jawabOk();
  });
  web.on("/api/stop", HTTP_GET, [] { if (mode != M_BIASA) gantiMode(M_BIASA); kirimBadan("S"); arahMata = 0; jawabOk(); });
  web.on("/api/pesan", HTTP_GET, [] {                 // pesan ke layar: ?teks=...&dari=...
    String t = web.arg("teks"); t.trim();
    if (!t.length()) { web.send(400, "text/plain", "teks kosong"); return; }
    tambahPesan(web.arg("dari").length() ? web.arg("dari") : String("Panel web"), t.substring(0, 199));
    jawabOk();
  });
  web.on("/api/jelajah", HTTP_GET, [] { jelajah(argAngka("menit", 5, 0, 60)); jawabOk(); });   // menit=0: berhenti
  web.on("/api/suara", HTTP_GET, [] { aturSuara(web.arg("pakai") != "google"); jawabOk(); });   // pakai=gemini|google
  web.on("/api/baterai", HTTP_GET, [] {               // riwayat tegangan per menit, paling lama dulu
    static uint16_t salin[RIWAYAT_BAT];
    int n, ujung;
    portENTER_CRITICAL(&kunciBat);
    n = jumlahRiwayat; ujung = ujungRiwayat; memcpy(salin, riwayatBat, sizeof(salin));
    portEXIT_CRITICAL(&kunciBat);
    JsonDocument d;
    d["persen"] = (int)persenBat; d["sisaMenit"] = (int)sisaMenitBat; d["volt"] = batHalus;
    JsonArray a = d["v"].to<JsonArray>();
    for (int i = 0; i < n; i++) a.add(salin[(ujung - n + i + RIWAYAT_BAT) % RIWAYAT_BAT]);
    String out; serializeJson(d, out);
    web.send(200, "application/json", out);
  });
  web.on("/api/kepala", HTTP_GET, [] {
    kirimBadan("P " + String(argAngka("p", 90, 0, 180)) + " " + String(argAngka("t", 90, 0, 180)));
    jawabOk();
  });
  web.on("/api/ai", HTTP_GET, [] {
    if (pilihAI(web.arg("m"))) { setStatus(String("Otak AI: ") + namaAI[pilihanAI]); jawabOk(); }
    else web.send(400, "text/plain", "model tidak dikenal atau API key-nya belum diisi");
  });
  web.on("/api/bicara", HTTP_GET, [] { kirimAudio(A_STOP); mintaDengar = true; jawabOk(); });   // dengarkan satu kalimat
  web.on("/api/dengar", HTTP_GET, [] { aturDengarTerus(web.arg("on") == "1"); jawabOk(); });
  web.on("/api/belok", HTTP_GET, [] {                 // tidak menunggu: hasilnya terlihat dari gerak robot
    if (mode != M_BIASA) gantiMode(M_BIASA);
    belok(argAngka("d", 90, -720, 720), 0);
    jawabOk();
  });
  web.on("/api/fiturkamera", HTTP_GET, [] { aturFiturKamera(web.arg("on") == "1"); jawabOk(); });
  web.on("/api/sekeliling", HTTP_GET, [] {
    if (!fiturKamera) { web.send(403, "text/plain", "aktifkan fitur kamera AI dulu"); return; }
    aksiTertunda = 1; jawabOk();
  });
  web.on("/api/suit", HTTP_GET, [] {
    if (!fiturKamera) { web.send(403, "text/plain", "aktifkan fitur kamera AI dulu"); return; }
    aksiTertunda = 2; jawabOk();
  });
  web.on("/api/alarm", HTTP_GET, [] {                 // daftar alarm
    JsonDocument d;
    JsonArray a = d["alarm"].to<JsonArray>();
    xSemaphoreTake(kunciAlarm, portMAX_DELAY);
    for (int i = 0; i < jumlahAlarm; i++) {
      JsonObject o = a.add<JsonObject>();
      char jm[6]; snprintf(jm, sizeof(jm), "%02d:%02d", daftarAlarm[i].jam, daftarAlarm[i].menit);
      o["jam"] = jm; o["hari"] = daftarAlarm[i].hari; o["bunyi"] = daftarAlarm[i].bunyi;
      o["pesan"] = (const char*)daftarAlarm[i].pesan;
      o["kapan"] = teksHariAlarm(daftarAlarm[i].hari, daftarAlarm[i].tanggal);
    }
    xSemaphoreGive(kunciAlarm);
    String out; serializeJson(d, out);
    web.send(200, "application/json", out);
  });
  web.on("/api/alarm/tambah", HTTP_GET, [] {           // ?jam=06:30&hari=127&pesan=...&bunyi=0
    int hh = -1, mm = -1;
    sscanf(web.arg("jam").c_str(), "%d:%d", &hh, &mm);
    String galat = tambahAlarm(hh, mm, argAngka("hari", 0, 0, 127), web.arg("pesan").substring(0, 79), argAngka("bunyi", 0, 0, 2));
    if (galat.length()) web.send(400, "text/plain", galat); else jawabOk();
  });
  web.on("/api/alarm/hapus", HTTP_GET, [] {
    if (hapusAlarm(argAngka("i", -1, -1, 99))) jawabOk(); else web.send(400, "text/plain", "nomor alarm tidak ada");
  });
  web.on("/api/lagu", HTTP_GET, [] { tekanTombol(T_LAGU); jawabOk(); });
  web.on("/api/radio", HTTP_GET, [] { tekanTombol(T_RADIO); jawabOk(); });
  web.on("/api/diam", HTTP_GET, [] { kirimAudio(A_STOP); setStatus("Diam"); jawabOk(); });
  web.on("/api/volume", HTTP_GET, [] {
    int v = argAngka("v", volSekarang, 0, 21);
    volSekarang = v; kirimAudio(A_VOL, String(v)); setStatus("Volume " + String(v) + " dari 21");
    jawabOk();
  });
  web.on("/api/bilang", HTTP_GET, [] {
    String t = web.arg("t"); t.trim();
    if (t.length()) { ucapkan(t.substring(0, 300)); setStatus(t); }
    jawabOk();
  });
  web.onNotFound([] { web.send(404, "text/plain", "tidak ada"); });
  web.begin();
}

// Upload kode lewat WiFi. Di Arduino IDE robot muncul di Tools > Port sebagai port jaringan.
String namaHost() {                         // "Zond" -> "zond" (huruf kecil, hanya huruf dan angka)
  String h;
  for (const char* c = NAMA_ROBOT; *c; c++) if (isalnum((unsigned char)*c)) h += (char)tolower((unsigned char)*c);
  return h.length() ? h : String("robot");
}

void siapkanOTA() {
  ArduinoOTA.setHostname(namaHost().c_str());
  ArduinoOTA.setMdnsEnabled(false);                    // mDNS sudah dinyalakan sendiri (MDNS.begin)
  if (strlen(OTA_SANDI)) ArduinoOTA.setPassword(OTA_SANDI);
  ArduinoOTA.onStart([] {
    otaJalan = true; otaPersen = 0;
    kirimBadan("S"); kirimAudio(A_STOP);               // berhenti total selama kode baru ditulis
    setStatus("Memperbarui kode lewat WiFi...");
  });
  ArduinoOTA.onProgress([](unsigned int sudah, unsigned int total) { otaPersen = total ? sudah * 100 / total : 0; });
  ArduinoOTA.onEnd([] { otaPersen = 100; setStatus("Selesai, menyala ulang..."); });
  ArduinoOTA.onError([](ota_error_t e) { otaJalan = false; setStatus("Upload WiFi gagal (kode " + String((int)e) + ")"); });
  ArduinoOTA.begin();
  MDNS.enableArduino(3232, strlen(OTA_SANDI) > 0);    // supaya muncul di Tools > Port
}

void tugasWeb(void*) {
  for (;;) { web.handleClient(); ArduinoOTA.handle(); vTaskDelay(pdMS_TO_TICKS(2)); }
}

// ======================= SETUP & LOOP =======================
// Baca pilihan layar tersimpan: jenis chip, balik layar kiri-kanan / atas-bawah, dan warna negatif (invert).
// Bawaan untuk modul merahmu (ILI9342C): layar dibalik atas-bawah dan invert aktif. Ubah lewat tombol Serial di tes_media.
// ======================= DIAGNOSIS LAYAR (dicetak di Serial Monitor saat menyala) =======================
// Uji tiap kabel layar sebelum layar dinyalakan: pin dibaca dengan penarik ke atas lalu ke bawah.
//   "1/0" = mengikuti penarik: kabel tidak terhubung singkat (normal)
//   "0/0" = terhubung ke GND, "1/1" = terhubung ke 3,3 V (CS dengan resistor penarik di modul juga 1/1, itu normal)
void diagPinLayar() {
  struct { const char* nama; int pin; } p[] = {{"SCK  IO12", TFT_SCK}, {"MOSI IO11", TFT_MOSI}, {"MISO IO13", TFT_MISO},
    {"CS   IO10", TFT_CS}, {"DC   IO9 ", TFT_DC}, {"RST  IO8 ", TFT_RST}, {"T_CS IO14", TOUCH_CS}, {"SDCS IO21", SD_CS},
    {"SD_SCK  ", SD_SCK}, {"SD_MOSI ", SD_MOSI}, {"SD_MISO ", SD_MISO}};
  Serial.printf("[layar] alasan menyala: %d (1 = saklar/daya baru, 3 = RST/upload, 9 = brownout)\n", (int)esp_reset_reason());
  for (auto& x : p) {
    if (x.pin < 0) continue;                               // SD ikut jalur layar
    pinMode(x.pin, INPUT_PULLUP);   delay(2); int a = digitalRead(x.pin);
    pinMode(x.pin, INPUT_PULLDOWN); delay(2); int b = digitalRead(x.pin);
    pinMode(x.pin, INPUT);
    Serial.printf("[layar] pin %s  %d/%d  %s\n", x.nama, a, b,
                  a && !b ? "ok" : (!a && !b) ? "TERHUBUNG KE GND?" : "ditarik ke 3,3 V");
  }
  Preferences q; q.begin("robot", true);
  Serial.printf("[layar] setelan tersimpan: ic %d, fh %d, fv %d, inv %d, geo %d, rgb %d\n", q.getUChar("ic", 1), q.getUChar("fh", 0),
                q.getUChar("fv", 1), q.getUChar("inv", 1), q.getUChar("geo", 1), q.getUChar("rgb", 1));
  q.end();
}

// Chip sentuh XPT2046 ada di modul layar, memakai kabel SCK/MOSI/MISO dan daya VCC yang sama.
// Kalau chip ini menjawab dengan angka suhu yang wajar, berarti daya modul dan kabel SPI sampai.
void diagSentuh() {
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  int v[3];
  for (int i = 0; i < 3; i++) {
    digitalWrite(TOUCH_CS, LOW);
    SPI.transfer(0x84);                                   // ukur suhu chip (TEMP0), 12 bit
    v[i] = SPI.transfer16(0) >> 3;
    digitalWrite(TOUCH_CS, HIGH);
    delay(2);
  }
  SPI.endTransaction();
  bool hidup = v[0] > 100 && v[0] < 4000 && abs(v[0] - v[2]) < 200;
  Serial.printf("[layar] chip sentuh menjawab: %d %d %d -> %s\n", v[0], v[1], v[2],
                hidup ? "HIDUP: daya modul dan kabel SCK/MOSI/MISO sampai"
                : (v[0] == 0 ? "DIAM (semua 0): VCC/GND modul, SCK, MOSI, atau MISO putus"
                : v[0] >= 4000 ? "MISO selalu 1: kabel MISO atau GND modul bermasalah" : "angka acak: sambungan SPI goyang"));
}

void muatLayar() {
  Preferences p; p.begin("robot", true);
  bool ic = p.getUChar("ic", 1) != 0;
  lcd.pakaiLayar(ic, p.getUChar("fh", 0) != 0, p.getUChar("fv", ic ? 1 : 0) != 0);
  lcd.balikInvert(p.getUChar("inv", ic ? 1 : 0) != 0);
  p.end();
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== ROBOT OTAK ===");
  Serial.printf("PSRAM: %u byte (harus sekitar 8 juta)\n", ESP.getPsramSize());
  Serial1.begin(115200, SERIAL_8N1, BADAN_RX, BADAN_TX);
  kunciSPI = xSemaphoreCreateMutex(); kunciUART = xSemaphoreCreateMutex(); kunciFoto = xSemaphoreCreateMutex();
  antrianAudio = xQueueCreate(12, sizeof(PerintahAudio));
  antrianTugas = xQueueCreate(2, sizeof(Tugas));
  antrianKirim = xQueueCreate(6, sizeof(PesanKirim));
  kunciAlarm = xSemaphoreCreateMutex();
  kunciPesan = xSemaphoreCreateMutex();
  kunciInfo = xSemaphoreCreateMutex();
  muatAlarm();
  muatPesan();

  pref.begin("robot", false);
  uint8_t arah = ARAH_LAYAR, geo = pref.getUChar("geo", 1), rgb = pref.getUChar("rgb", 1);   // geo dan rgb: pilihan di tes_layar
  if (pref.getUChar("rot", 255) != arah) pref.putUChar("rot", arah);   // tes_media, tes_sd, tes_musik ikut arah ini
  pilihanAI = pref.getUChar("ai", AI_OTOMATIS);                         // otak AI pilihan terakhir (panel web / /ai)
  dengarTerus = pref.getUChar("dengar", 0) != 0;                        // bawaan mati: hemat jatah suara-ke-teks
  fiturKamera = pref.getUChar("kamAI", 0) != 0;                         // lihat sekeliling dan suit: bawaan mati
  pakaiSuaraAI = pref.getUChar("suara", 1) != 0;
  pengingatSholat = pref.getUChar("sholat", 1);
  suaraHariIni = pref.getUInt("tsN", 0);
  if (pilihanAI >= AI_JUMLAH || !aiTersedia(pilihanAI)) pilihanAI = AI_OTOMATIS;
  pref.end();
  diagPinLayar();
  lcd.aturPanel(geo != 0, rgb != 0);
  muatLayar();
  // Saat saklar baru dinyalakan, tegangan layar naik lebih lambat dari S3. Kalau perintah awal dikirim terlalu cepat,
  // layar tidak menerimanya dan hanya tampil putih. Tunggu daya stabil, lalu kirim pengaturan awal dua kali.
  if (esp_reset_reason() == ESP_RST_POWERON || esp_reset_reason() == ESP_RST_BROWNOUT) delay(600);
  unsigned long tLayar = millis();
  bool okLayar = lcd.init(); delay(150); okLayar = lcd.init() && okLayar;
  Serial.printf("[layar] init %s dalam %lu ms (jeda awal %s), kecepatan tulis %u MHz\n", okLayar ? "selesai" : "GAGAL",
                millis() - tLayar, (esp_reset_reason() == ESP_RST_POWERON || esp_reset_reason() == ESP_RST_BROWNOUT) ? "600 ms" : "tidak ada", (unsigned)LAYAR_MHZ);
  lcd.setRotation(arah > 7 ? 6 : arah);
  if (lcd.width() < lcd.height()) lcd.setRotation(geo ? 6 : 1);   // robot memakai layar mendatar 320 x 240
  { Preferences q; q.begin("robot", true); if (q.getUChar("putar", 0)) lcd.setRotation(lcd.getRotation() ^ 2); q.end(); }   // Setelan: putar 180
  lcd.setBrightness(255); lcd.fillScreen(TFT_BLACK);
  Serial.printf("[layar] arah %d, ukuran %d x %d\n", lcd.getRotation(), (int)lcd.width(), (int)lcd.height());
  int32_t tx, ty; bool paksa = lcd.getTouchRaw(&tx, &ty);    // sentuh layar saat menyala = kalibrasi ulang
  if (paksa) Serial.printf("[layar] sentuhan terbaca saat menyala (x %d, y %d): masuk kalibrasi ulang\n", (int)tx, (int)ty);
  kalibrasiSentuh(paksa);
  siapkanLayar();

  SPI.begin(TFT_SCK, TFT_MISO, TFT_MOSI);
  diagSentuh();
  pasangSD();
  // Penyimpanan di flash untuk file suara Gemini. Pertama kali diformat dulu (beberapa detik, sekali saja).
  adaFFat = FFat.begin(false) || (Serial.println("[suara] menyiapkan penyimpanan flash (sekali, beberapa detik)..."), FFat.begin(true));
  Serial.printf("[suara] penyimpanan flash %s\n", adaFFat ? "siap" : "TIDAK ADA (Partition Scheme belum 3MB APP/9.9MB FATFS): suara Google");

  Audio::audio_info_callback = infoAudio;
  audio.setPinout(AMP_BCLK, AMP_LRC, AMP_DIN);
  audio.setVolume(VOLUME_AWAL);
  mic.setPins(MIC_SCK, MIC_WS, -1, MIC_SD);
  // Mode STEREO, bukan MONO: di ESP32-S3 mode mono memberi data rusak (sebagian besar nol). Jalur kiri diambil di tugasDengar().
  if (!mic.begin(I2S_MODE_STD, MIC_RATE, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO))
    Serial.println("Mikrofon gagal mulai");

  setStatus("Menyambung WiFi...");
  perbaruiLayar();
  WiFi.mode(WIFI_STA); WiFi.begin(WIFI_NAMA, WIFI_PASSWORD);
  for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) delay(500);
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("IP otak: "); Serial.println(WiFi.localIP());
    configTzTime(ZONA_WAKTU, "pool.ntp.org", "time.google.com");   // jam dari internet, berjalan sendiri di latar
    MDNS.begin(namaHost().c_str());                    // panel juga bisa dibuka di http://zond.local
    MDNS.addService("http", "tcp", 80);
    siapkanWeb();
    siapkanOTA();
    String panel = "http://" + WiFi.localIP().toString() + "/  (atau http://" + namaHost() + ".local/)";
    Serial.println("Panel web: " + panel);
    alamatKamera = KAMERA_ALAMAT;                      // terima juga "http://1.2.3.4/"
    alamatKamera.replace("http://", ""); alamatKamera.replace("https://", ""); alamatKamera.trim();
    while (alamatKamera.endsWith("/")) alamatKamera.remove(alamatKamera.length() - 1);
    tgClient.setCACert(TELEGRAM_CERTIFICATE_ROOT);
    // Buang pesan Telegram lama yang belum dikonfirmasi. Tanpa ini, perintah yang membuat
    // otak restart (misalnya kehabisan memori) akan dijalankan lagi setiap kali menyala: restart berulang.
    if (bot.getUpdates(-1) > 0) Serial.println("[telegram] pesan lama dilewati: " + bot.messages[0].text);
    bot.sendMessage(TELEGRAM_CHATID, String(NAMA_ROBOT) + " menyala. Ketik /bantuan\nPanel kontrol: " + panel + alasanRestart());
    setStatus("Siap");
    ucapkan(String("Halo, aku ") + NAMA_ROBOT + ". Aku siap.");
  } else setStatus("WiFi gagal. Cek nama dan password");

  xTaskCreatePinnedToCore(tugasAudio, "audio", 16384, nullptr, 3, nullptr, 1);
  xTaskCreatePinnedToCore(tugasJaringan, "jaringan", 16384, nullptr, 1, nullptr, 0);
  xTaskCreatePinnedToCore(tugasDengar, "dengar", 6144, nullptr, 2, nullptr, 0);
  if (WiFi.status() == WL_CONNECTED) xTaskCreatePinnedToCore(tugasWeb, "web", 8192, nullptr, 1, nullptr, 0);
}

void loop() {
  bacaBadan();
  tanganiSentuh();
  jalankanMode();
  catatBaterai();
  ekspresiOtomatis();
  cekAlarm();
  cekSholat();
  // radio/lagu dari alarm diputar setelah pengingat selesai diucapkan
  if (bunyiTertunda && millis() - tBunyiTertunda > 2000 && !audioJalan) {
    int b = bunyiTertunda; bunyiTertunda = 0;
    if (b == 1 && WiFi.status() == WL_CONNECTED) kirimAudio(A_URL, DAFTAR_RADIO[0]);
    else if (b == 2) tekanTombol(T_LAGU);
    else if (b == 3 || b == 4) {                           // adzan dari SD; Subuh memakai /adzan_subuh.mp3 kalau ada
      kunciSD(); bool subuh = b == 4 && adaSD && SD.exists("/adzan_subuh.mp3"); bool ada = adaSD && SD.exists("/adzan.mp3"); lepasSD();
      if (subuh || ada) { putarSekali = true; kirimAudio(A_FILE, subuh ? "/adzan_subuh.mp3" : "/adzan.mp3"); }
    }
  }
  perbaruiLayar();
  delay(5);
}
