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
    Partition Scheme : Huge APP (3MB No OTA/1MB SPIFFS)
    USB CDC On Boot  : Enabled kalau log upload menulis "USB mode: USB-Serial/JTAG" (port USB bawaan chip),
                       Disabled kalau menulis "USB mode: UART"
  LIBRARY (Tools > Manage Libraries):
    LovyanGFX (lovyan03), UniversalTelegramBot (Brian Lough), ArduinoJson (Benoit Blanchon)
    ESP32-audioI2S (schreibfaul1) -> tidak ada di Library Manager, pasang dari ZIP GitHub (lihat panduan)

  Isi bagian KONFIGURASI di bawah. Jangan bagikan file yang sudah berisi token/API key.
*/

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <SPI.h>
#include <SD.h>
#include <Preferences.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <base64.h>
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
const char* KAMERA_ALAMAT   = "";                     // contoh "192.168.1.50". Kosong = cari robotcam.local
const char* NAMA_ROBOT      = "Zond";                 // nama robot; perintah suara harus menyebut nama ini
const bool  DENGAR_NAMA     = true;                   // true = mendengar terus, menanggapi kalau namanya disebut
                                                      // false = hanya mendengar setelah tombol BICARA
const char* NAMA_PEMILIK    = "Rizky";
const int   VOLUME_AWAL     = 12;                     // 0..21

// ======================= PIN (sesuai panduan) =======================
#define TFT_SCK   12
#define TFT_MOSI  11
#define TFT_MISO  13
#define TFT_CS    10
#define TFT_DC     9
#define TFT_RST    8
#define TOUCH_CS  14
#define SD_CS     21
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

LGFX_Sprite wajah(&lcd);
Audio audio;                 // memakai I2S_NUM_0 untuk speaker
I2SClass mic;                // otomatis memakai I2S_NUM_1
Preferences pref;
WiFiClientSecure tgClient;
UniversalTelegramBot bot(TELEGRAM_TOKEN, tgClient);

SemaphoreHandle_t kunciSPI, kunciUART, kunciFoto;   // kunciFoto: satu pemakai foto dalam satu waktu
QueueHandle_t antrianAudio, antrianTugas;

// ======================= STATUS BERSAMA =======================
struct DataBadan { int depan = -1, belakang = -1; float baterai = -1, suhu = -1, lembap = -1; int pitch = 0, roll = 0, flag = 0; unsigned long terakhir = 0; };
volatile DataBadan badan;
enum Ekspresi { NORMAL, DENGAR, PIKIR, BICARA, KAGET, LEMAH };
volatile Ekspresi ekspresi = NORMAL;
char teksStatus[96] = "Menyala...";
volatile int arahMata = 0;          // -1 kiri, 0 tengah, 1 kanan
volatile bool sedangBicara = false;
volatile bool audioJalan = false;          // speaker sedang memutar sesuatu (bicara, musik, radio)
volatile bool adaSD = false;
String alamatKamera;
volatile int pesanBadan = 0;        // 1 = baterai lemah, 2 = stop dikunci (diteruskan ke Telegram)
enum Mode { M_BIASA, M_IKUTI, M_JAGA };
volatile Mode mode = M_BIASA;
volatile bool alarmJaga = false;    // diisi mode jaga, dikirim ke Telegram oleh tugas jaringan

// ======================= AUDIO (task sendiri) =======================
enum JenisAudio { A_FILE, A_URL, A_TTS, A_STOP, A_VOL };
struct PerintahAudio { JenisAudio jenis; char isi[200]; };

// Semua jenis data (enum/struct) harus dibuat sebelum fungsi pertama: Arduino IDE menaruh deklarasi
// fungsi otomatis tepat sebelum fungsi pertama. Fungsi dengan nilai bawaan dideklarasikan sendiri di sini.
void kirimAudio(JenisAudio j, const String& isi = "");

void setStatus(const String& s) { strlcpy(teksStatus, s.c_str(), sizeof(teksStatus)); Serial.println("[status] " + s); }

void kirimAudio(JenisAudio j, const String& isi) {
  PerintahAudio p; p.jenis = j; strlcpy(p.isi, isi.c_str(), sizeof(p.isi));
  xQueueSend(antrianAudio, &p, pdMS_TO_TICKS(100));
}

// pecah kalimat panjang jadi potongan <= 150 huruf supaya bisa diucapkan Google TTS
void ucapkan(const String& teks) {
  String t = teks; t.trim();
  while (t.length()) {
    int potong = t.length();
    if (potong > 150) {
      potong = 150;
      for (int i = 150; i > 60; i--) if (t[i] == '.' || t[i] == ',' || t[i] == '?' || t[i] == '!') { potong = i + 1; break; }
      if (potong == 150) for (int i = 150; i > 60; i--) if (t[i] == ' ') { potong = i; break; }
    }
    kirimAudio(A_TTS, t.substring(0, potong));
    t = t.substring(potong); t.trim();
  }
}

void tugasAudio(void*) {
  std::vector<String> antreTTS;
  bool pakaiSD = false;          // kunci bus SPI hanya saat memutar dari SD (layar memakai bus yang sama)
  for (;;) {
    PerintahAudio p;
    while (xQueueReceive(antrianAudio, &p, 0) == pdTRUE) {
      xSemaphoreTake(kunciSPI, portMAX_DELAY);
      switch (p.jenis) {
        case A_FILE: antreTTS.clear(); pakaiSD = true; audio.connecttoFS(SD, p.isi); break;
        case A_URL:  antreTTS.clear(); pakaiSD = false; audio.connecttohost(p.isi); break;
        case A_TTS:  if (audio.isRunning() && !sedangBicara) audio.stopSong();   // bicara memotong musik
                     antreTTS.push_back(String(p.isi)); break;
        case A_STOP: antreTTS.clear(); audio.stopSong(); break;
        case A_VOL:  audio.setVolume(constrain(atoi(p.isi), 0, 21)); break;
      }
      xSemaphoreGive(kunciSPI);
    }
    if (!audio.isRunning() && !antreTTS.empty()) {
      String s = antreTTS.front(); antreTTS.erase(antreTTS.begin());
      pakaiSD = false;
      audio.connecttospeech(s.c_str(), "id");
      sedangBicara = true;
    }
    if (!audio.isRunning() && antreTTS.empty()) sedangBicara = false;
    audioJalan = audio.isRunning() || !antreTTS.empty();
    if (pakaiSD) xSemaphoreTake(kunciSPI, portMAX_DELAY);
    audio.loop();
    if (pakaiSD) xSemaphoreGive(kunciSPI);
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
}

void bacaBadan() {
  static String buf;
  while (Serial1.available()) {
    char c = Serial1.read();
    if (c == '\n') {
      buf.trim();
      if (buf.startsWith("D ")) {
        int d, b, p, r, f; float v, s, h;
        if (sscanf(buf.c_str(), "D %d %d %f %f %f %d %d %d", &d, &b, &v, &s, &h, &p, &r, &f) == 8) {
          badan.depan = d; badan.belakang = b; badan.baterai = v; badan.suhu = s; badan.lembap = h;
          badan.pitch = p; badan.roll = r; badan.flag = f; badan.terakhir = millis();
        }
      } else if (buf.startsWith("E ")) {
        Serial.println("[badan] " + buf);
        if (buf.indexOf("DITOLAK") > 0) { ekspresi = KAGET; setStatus("Ada halangan!"); }
        if (buf.indexOf("BATERAI_LEMAH") > 0) { ekspresi = LEMAH; pesanBadan = 1; }
        if (buf.indexOf("STOP_DIKUNCI") > 0) pesanBadan = 2;
      }
      buf = "";
    } else if (c != '\r' && buf.length() < 100) buf += c;
  }
}

String ringkasSensor() {
  char b[160];
  bool hidup = badan.terakhir && millis() - badan.terakhir < 2000;
  if (!hidup) return "badan tidak terhubung";
  snprintf(b, sizeof(b), "jarak depan %d cm, belakang %d cm, baterai %.2f V, suhu %.1f C, lembap %.0f%%, miring %d/%d derajat%s",
           badan.depan, badan.belakang, badan.baterai, badan.suhu, badan.lembap, badan.pitch, badan.roll,
           (badan.flag & 1) ? ", TOMBOL STOP DIKUNCI" : "");
  return String(b);
}

// ======================= MODE IKUTI & JAGA =======================
// IKUTI: jaga jarak 35-70 cm ke benda di depan (sensor ultrasonik tidak bisa membedakan orang dan benda).
// JAGA : robot diam; kalau jarak depan berubah jauh (ada yang lewat/mendekat), kirim foto ke Telegram.
int dasarJaga = -1; unsigned long mulaiJaga = 0;

void gantiMode(Mode m) {
  mode = m;
  kirimBadan("S");
  if (m == M_JAGA) { dasarJaga = -1; mulaiJaga = millis(); setStatus("Mode jaga"); }
  else if (m == M_IKUTI) setStatus("Mode ikuti");
  else setStatus("Mode biasa");
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
  } else if (mode == M_JAGA) {
    if (millis() - mulaiJaga < 4000) return;            // beri waktu pemilik menjauh
    if (dasarJaga < 0) { dasarJaga = (d < 0) ? 400 : d; return; }
    int now = (d < 0) ? 400 : d;
    beda = (abs(now - dasarJaga) > 25) ? beda + 1 : 0;
    if (beda >= 2 && millis() - tAlarm > 30000) {       // jeda 30 detik antar-alarm
      tAlarm = millis(); beda = 0;
      ekspresi = KAGET; ucapkan("Siapa itu?");
      alarmJaga = true;
    }
  }
}

// ======================= KAMERA =======================
uint8_t* fotoBuf = nullptr; size_t fotoLen = 0, fotoPos = 0;

bool ambilFoto(const char* res) {
  if (alamatKamera.isEmpty()) {
    IPAddress ip = MDNS.queryHost("robotcam", 2000);
    if (ip == IPAddress()) return false;
    alamatKamera = ip.toString();
  }
  HTTPClient http;
  http.begin("http://" + alamatKamera + "/capture?res=" + res);
  http.setTimeout(8000);
  int code = http.GET();
  if (code != 200) { http.end(); return false; }
  int len = http.getSize();
  if (len <= 0 || len > 400000) { http.end(); return false; }
  if (fotoBuf) free(fotoBuf);
  fotoBuf = (uint8_t*)ps_malloc(len);
  if (!fotoBuf) { http.end(); return false; }
  WiFiClient* s = http.getStreamPtr();
  size_t got = 0; unsigned long t0 = millis();
  while (got < (size_t)len && millis() - t0 < 8000) {
    size_t n = s->available();
    if (n) got += s->readBytes(fotoBuf + got, min(n, (size_t)len - got)); else delay(2);
  }
  http.end();
  fotoLen = got; fotoPos = 0;
  return got == (size_t)len;
}

// Library Telegram memanggil fotoMasihAda() dulu di setiap putaran, lalu buffer dan panjangnya.
// Potongan (chunk) dihitung di fotoMasihAda() supaya urutan pemanggilan dua fungsi lainnya tidak berpengaruh.
size_t potAwal = 0, potLen = 0;
bool fotoMasihAda() { fotoPos = potAwal + potLen; potAwal = fotoPos; potLen = min((size_t)1024, fotoLen - fotoPos); return potLen > 0; }
byte fotoByteBerikut() { return fotoBuf[fotoPos++]; }
byte* fotoBufferBerikut() { return fotoBuf + potAwal; }
int fotoPanjangBerikut() { return potLen; }

bool kirimFotoTelegram(const String& chat) {
  if (!fotoBuf || !fotoLen) return false;
  fotoPos = 0; potAwal = 0; potLen = 0;
  String r = bot.sendPhotoByBinary(chat, "image/jpeg", fotoLen, fotoMasihAda, fotoByteBerikut, fotoBufferBerikut, fotoPanjangBerikut);
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
    if (millis() - tSibuk < 600) continue;               // tunggu gema speaker hilang
    if (mintaDengar) {
      mintaDengar = false; jendelaSampai = millis() + 6000;
      ekspresi = DENGAR; setStatus("Mendengarkan...");
    }
    bool bolehDengar = DENGAR_NAMA || millis() < jendelaSampai;
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
                "--" + batas + "\r\nContent-Disposition: form-data; name=\"prompt\"\r\n\r\n" + NAMA_ROBOT + "\r\n" +
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
String riwayatPeran[6], riwayatIsi[6]; int riwayatN = 0;

void ingat(const String& peran, const String& isi) {
  if (riwayatN == 6) { for (int i = 0; i < 4; i++) { riwayatPeran[i] = riwayatPeran[i + 2]; riwayatIsi[i] = riwayatIsi[i + 2]; } riwayatN = 4; }
  riwayatPeran[riwayatN] = peran; riwayatIsi[riwayatN] = isi; riwayatN++;
}

String promptSistem() {
  return String("Kamu adalah ") + NAMA_ROBOT + ", robot beroda empat milik " + NAMA_PEMILIK + " di Lombok. "
    "Namamu " + String(NAMA_ROBOT) + "; pemilikmu memanggil namamu sebelum memberi perintah, jadi abaikan panggilan itu dan tanggapi permintaannya. "
    "Bicara Bahasa Indonesia santai, ramah, dan SINGKAT (paling banyak 2 kalimat) karena jawabanmu diucapkan lewat speaker kecil. "
    "Jujur: kalau tidak tahu, bilang tidak tahu. Kamu punya badan yang bisa bergerak. "
    "Jawab HANYA dengan satu objek JSON tanpa teks lain, formatnya: "
    "{\"bicara\":\"kalimatmu\",\"aksi\":[...]} . Aksi yang tersedia: "
    "{\"m\":[kiri,kanan,ms]} menggerakkan roda, kiri/kanan -200..200, ms 100..2000 "
    "(maju [150,150,800], mundur [-150,-150,800], putar kanan di tempat [160,-160,450], putar kiri [-160,160,450]); "
    "{\"kepala\":[pan,tilt]} arah kepala dalam derajat, 90,90 = lurus ke depan, pan 30=kanan 150=kiri; "
    "{\"foto\":1} ambil foto dari kameramu, HANYA kalau diminta melihat sesuatu; "
    "{\"diam\":1} hentikan musik; {\"mode\":\"ikuti\"} ikuti benda di depanmu (misalnya kaki pemilik), "
    "{\"mode\":\"jaga\"} diam dan berjaga, {\"mode\":\"biasa\"} kembali normal. Kosongkan aksi kalau tidak perlu. "
    "Badanmu otomatis menolak maju kalau ada halangan dekat, jadi tidak perlu khawatir menabrak. "
    "Data sensor terbaru: " + ringkasSensor() + ".";
}

// Buang bagian "berpikir" <think>...</think> yang kadang ditulis model cadangan, lalu rapikan.
String bersihkan(String t) {
  int a = t.indexOf("<think>"), b = t.indexOf("</think>");
  if (a >= 0 && b > a) t.remove(a, b + 8 - a);
  t.trim();
  return t;
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
  if (fotoBase64.length()) {
    JsonObject img = bagian.add<JsonObject>();
    img["inline_data"]["mime_type"] = "image/jpeg"; img["inline_data"]["data"] = fotoBase64;
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
    JsonObject img = isi.add<JsonObject>(); img["type"] = "image_url";
    img["image_url"]["url"] = "data:image/jpeg;base64," + fotoBase64;
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

// Tanya AI: Gemini dulu; kalau gagal (misalnya jatah gratis hari ini habis), Gemma dengan kunci yang sama,
// lalu Groq kalau kuncinya ada.
// fotoBase64 kosong = hanya teks.
String tanyaAI(const String& pertanyaan, const String& fotoBase64 = "") {
  bool adaGemini = strlen(GEMINI_API_KEY) > 0, adaGroq = strlen(GROQ_API_KEY) > 0;
  if (!adaGemini && !adaGroq) return "{\"bicara\":\"API key Gemini belum diisi di kode.\",\"aksi\":[]}";
  int kode = 0; String teks;
  if (adaGemini) teks = tanyaGemini(GEMINI_MODEL, false, pertanyaan, fotoBase64, kode);
  if ((kode != 200 || teks.length() == 0) && adaGemini && strlen(GEMMA_MODEL)) {
    Serial.printf("[ai] %s gagal (kode %d), pakai cadangan %s\n", GEMINI_MODEL, kode, GEMMA_MODEL);
    teks = tanyaGemini(GEMMA_MODEL, true, pertanyaan, fotoBase64, kode);
  }
  if ((kode != 200 || teks.length() == 0) && adaGroq) {
    if (adaGemini) Serial.printf("[ai] AI Studio gagal (kode %d), pakai cadangan Groq\n", kode);
    teks = tanyaGroq(pertanyaan, fotoBase64, kode);
  }
  if (kode != 200 || teks.length() == 0) {
    if (kode == 429) return "{\"bicara\":\"Jatah AI gratis sedang habis. Coba lagi nanti.\",\"aksi\":[]}";
    return "{\"bicara\":\"Maaf, aku gagal menghubungi otak AI (kode " + String(kode) + ").\",\"aksi\":[]}";
  }
  ingat("user", pertanyaan);                 // foto tidak disimpan di riwayat, terlalu besar
  ingat("assistant", teks);
  return teks;
}

// jalankan JSON {"bicara":..,"aksi":[..]}; kembalikan kalimat untuk dikirim ke Telegram
String jalankanJawaban(const String& jawaban, const String& pertanyaanAsli, bool bolehFoto) {
  int a = jawaban.indexOf('{'), b = jawaban.lastIndexOf('}');
  JsonDocument j;
  if (a < 0 || b < a || deserializeJson(j, jawaban.substring(a, b + 1))) {
    ucapkan(jawaban); return jawaban;                    // bukan JSON: ucapkan apa adanya
  }
  String bicara = j["bicara"] | "";
  if (bicara.length()) ucapkan(bicara);                  // bicara dulu, baru bergerak
  bool mintaFoto = false;
  for (JsonObject x : j["aksi"].as<JsonArray>()) {
    if (x["m"].is<JsonArray>()) {
      int l = constrain((int)x["m"][0], -200, 200), r = constrain((int)x["m"][1], -200, 200), ms = constrain((int)x["m"][2], 100, 2000);
      gerak(l, r, ms); vTaskDelay(pdMS_TO_TICKS(ms + 150));
    } else if (x["kepala"].is<JsonArray>()) {
      kirimBadan("P " + String((int)x["kepala"][0]) + " " + String((int)x["kepala"][1])); vTaskDelay(pdMS_TO_TICKS(400));
    } else if (x["foto"].is<int>()) {
      mintaFoto = true;
    } else if (x["diam"].is<int>()) {
      kirimAudio(A_STOP);
    } else if (x["mode"].is<const char*>()) {
      String md = x["mode"].as<String>();
      gantiMode(md == "ikuti" ? M_IKUTI : md == "jaga" ? M_JAGA : M_BIASA);
    }
  }
  if (mintaFoto && bolehFoto) {
    setStatus("Melihat...");
    xSemaphoreTake(kunciFoto, portMAX_DELAY);
    bool ok = ambilFoto("vga");
    String b64 = ok ? base64::encode(fotoBuf, fotoLen) : "";
    xSemaphoreGive(kunciFoto);
    if (ok) {
      String j2 = tanyaAI(pertanyaanAsli + " (Ini foto dari kameramu barusan. Jawab berdasarkan foto.)", b64);
      return jalankanJawaban(j2, pertanyaanAsli, false);
    }
    bicara = "Kameraku tidak bisa dihubungi.";
    ucapkan(bicara);
  }
  return bicara;
}

// ======================= TUGAS JARINGAN (Telegram + AI) =======================
enum JenisTugas { T_SUARA };
struct Tugas { JenisTugas jenis; };

String daftarLagu() {
  if (!adaSD) return "SD card tidak terbaca.";
  String s;
  xSemaphoreTake(kunciSPI, portMAX_DELAY);
  File root = SD.open("/");
  for (File f = root.openNextFile(); f; f = root.openNextFile()) {
    String n = f.name();
    if (n.endsWith(".mp3") || n.endsWith(".MP3") || n.endsWith(".wav") || n.endsWith(".m4a")) s += n + "\n";
  }
  xSemaphoreGive(kunciSPI);
  return s.length() ? s : "Belum ada lagu (.mp3) di folder utama SD card.";
}

const char* BANTUAN =
  "Perintah:\n/status  data sensor\n/maju /mundur /kiri /kanan [ms]\n/stop\n/kepala pan tilt  (90 90 = lurus)\n"
  "/foto  kirim foto kamera\n/lihat [pertanyaan]  AI melihat lewat kamera\n/bilang teks  robot mengucapkan teks\n"
  "/lagu  daftar lagu di SD\n/putar nama.mp3\n/radio url\n/diam  hentikan suara\n/volume 0-21\n"
  "/ikuti  ikuti benda di depan\n/jaga  berjaga, kirim foto kalau ada gerakan\n/biasa  kembali normal\n\n"
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

  if (cmd == "/start" || cmd == "/bantuan" || cmd == "/help") bot.sendMessage(chat, String("Halo, aku ") + NAMA_ROBOT + "!\n\n" + BANTUAN);
  else if (cmd == "/status") bot.sendMessage(chat, ringkasSensor());
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
    bool ok = ambilFoto("vga") && kirimFotoTelegram(chat);
    xSemaphoreGive(kunciFoto);
    if (!ok) bot.sendMessage(chat, "Gagal mengambil foto. Kamera menyala dan satu WiFi?");
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
    if (!ok) { bot.sendMessage(chat, "Kamera tidak bisa dihubungi."); ekspresi = NORMAL; return; }
    String jaw = tanyaAI(q, b64);
    String kata = jalankanJawaban(jaw, q, false);
    bot.sendMessage(chat, kata.length() ? kata : "(tidak ada jawaban)");
    ekspresi = NORMAL; setStatus(kata);
  }
  else if (cmd == "/ikuti")  { gantiMode(M_IKUTI); bot.sendMessage(chat, "Mode ikuti: aku menjaga jarak 35-70 cm ke benda di depanku. /biasa untuk berhenti."); }
  else if (cmd == "/jaga")   { gantiMode(M_JAGA);  bot.sendMessage(chat, "Mode jaga aktif dalam 4 detik. Aku kirim foto kalau ada gerakan di depan. /biasa untuk berhenti."); }
  else if (cmd == "/biasa")  { gantiMode(M_BIASA); bot.sendMessage(chat, "Kembali ke mode biasa."); }
  else if (cmd == "/bilang") { ucapkan(arg); bot.sendMessage(chat, "Oke."); }
  else if (cmd == "/lagu")   bot.sendMessage(chat, daftarLagu());
  else if (cmd == "/putar")  { kirimAudio(A_FILE, "/" + arg); bot.sendMessage(chat, "Memutar " + arg); }
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
      String sisa;
      bool adaNama = kata.length() && cariNama(kata, sisa);
      if (kata.length() == 0) {
        if (!butuhNama) { setStatus(strlen(GROQ_API_KEY) ? "Tidak terdengar jelas" : "GROQ_API_KEY belum diisi"); ekspresi = NORMAL; }
      } else if (butuhNama && !adaNama) {
        Serial.println("[dengar] tanpa nama, diabaikan: " + kata);
      } else if (adaNama && sisa.length() == 0) {         // hanya memanggil nama: tunggu perintahnya
        ucapkan("Ya?");
        jendelaSampai = millis() + 9000;
        ekspresi = DENGAR; setStatus("Ya? Silakan bicara");
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

// ======================= TAMPILAN =======================
const int TOMBOL_Y = 192, TOMBOL_H = 48, TOMBOL_N = 6, TOMBOL_W = 320 / TOMBOL_N;
const char* labelTombol[TOMBOL_N] = {"KIRI", "MAJU", "MUNDUR", "KANAN", "STOP", "BICARA"};
const uint16_t warnaTombol[TOMBOL_N] = {0x2104, 0x2104, 0x2104, 0x2104, 0xA000, 0x0320};

void gambarTombol(int tekan = -1) {
  xSemaphoreTake(kunciSPI, portMAX_DELAY);
  for (int i = 0; i < TOMBOL_N; i++) {
    int x = i * TOMBOL_W;
    lcd.fillRoundRect(x + 2, TOMBOL_Y + 2, TOMBOL_W - 4, TOMBOL_H - 4, 6, i == tekan ? TFT_WHITE : warnaTombol[i]);
    lcd.setTextColor(i == tekan ? TFT_BLACK : TFT_WHITE);
    lcd.setTextDatum(middle_center); lcd.setFont(&fonts::Font2);
    lcd.drawString(labelTombol[i], x + TOMBOL_W / 2, TOMBOL_Y + TOMBOL_H / 2);
  }
  xSemaphoreGive(kunciSPI);
}

void gambarWajah() {
  static unsigned long tKedip = 0; static bool kedip = false;
  if (!kedip && millis() - tKedip > 3500 + (esp_random() % 2000)) { kedip = true; tKedip = millis(); }
  if (kedip && millis() - tKedip > 140) { kedip = false; tKedip = millis(); }

  wajah.fillScreen(TFT_BLACK);
  // baris status atas
  wajah.setFont(&fonts::Font2); wajah.setTextDatum(top_left); wajah.setTextColor(0x9CF3);
  char atas[80];
  bool hidup = badan.terakhir && millis() - badan.terakhir < 2000;
  if (hidup) snprintf(atas, sizeof(atas), "%.1fV  depan %dcm  %.0fC", badan.baterai, badan.depan, badan.suhu);
  else snprintf(atas, sizeof(atas), "badan belum terhubung");
  wajah.drawString(atas, 4, 2);
  wajah.setTextDatum(top_right);
  wajah.drawString(WiFi.status() == WL_CONNECTED ? "WiFi" : "no WiFi", 316, 2);

  uint16_t warna = (ekspresi == KAGET) ? TFT_ORANGE : (ekspresi == LEMAH) ? 0x7BEF : (ekspresi == DENGAR) ? TFT_GREEN : TFT_CYAN;
  int geser = arahMata * 14;
  int tinggi = kedip ? 6 : (ekspresi == KAGET ? 62 : ekspresi == LEMAH ? 18 : 50);
  for (int s = -1; s <= 1; s += 2) {
    int cx = 160 + s * 62 + geser, cy = 80;
    wajah.fillRoundRect(cx - 26, cy - tinggi / 2, 52, tinggi, 14, warna);
    if (ekspresi == PIKIR && !kedip) wajah.fillCircle(cx + 10, cy - 10, 7, TFT_BLACK);   // melirik ke atas
  }
  // mulut
  int my = 132;
  if (sedangBicara) { int h = 4 + (millis() / 90) % 3 * 6; wajah.fillRoundRect(130, my - h / 2, 60, h, 4, warna); }
  else if (ekspresi == KAGET) wajah.drawCircle(160, my, 9, warna);
  else wajah.drawArc(160, my - 22, 30, 27, 50, 130, warna);
  // teks status bawah
  wajah.setTextDatum(middle_center); wajah.setTextColor(TFT_WHITE);
  String s = teksStatus; if (s.length() > 44) s = s.substring(0, 42) + "..";
  wajah.drawString(s, 160, 168);

  xSemaphoreTake(kunciSPI, portMAX_DELAY);
  wajah.pushSprite(0, 0);
  xSemaphoreGive(kunciSPI);
}

void lihatKameraDiLayar() {                       // ketuk wajah = lihat kamera 10 detik
  setStatus("Membuka kamera...");
  unsigned long t0 = millis();
  int32_t x, y;
  while (millis() - t0 < 10000) {
    xSemaphoreTake(kunciFoto, portMAX_DELAY);
    bool ok = ambilFoto("qvga");
    if (ok) { xSemaphoreTake(kunciSPI, portMAX_DELAY); lcd.drawJpg(fotoBuf, fotoLen, 0, 0, 320, 192); xSemaphoreGive(kunciSPI); }
    xSemaphoreGive(kunciFoto);
    if (!ok) { setStatus("Kamera tidak bisa dihubungi"); return; }
    bacaBadan();
    xSemaphoreTake(kunciSPI, portMAX_DELAY); bool sentuh = lcd.getTouch(&x, &y); xSemaphoreGive(kunciSPI);
    if (millis() - t0 > 800 && sentuh) break;
  }
  for (int i = 0; i < 100; i++) {                 // tunggu jari diangkat
    xSemaphoreTake(kunciSPI, portMAX_DELAY); bool sentuh = lcd.getTouch(&x, &y); xSemaphoreGive(kunciSPI);
    if (!sentuh) break; delay(20);
  }
  setStatus("Siap");
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

void tanganiSentuh() {
  static int tombolLalu = -1; static unsigned long tKirim = 0;
  int32_t x, y; bool ada;
  xSemaphoreTake(kunciSPI, portMAX_DELAY);
  ada = lcd.getTouch(&x, &y);
  xSemaphoreGive(kunciSPI);
  // zona: 0..5 = tombol bawah, 10 = area wajah, -1 = tidak disentuh
  int tombol = !ada ? -1 : (y >= TOMBOL_Y) ? constrain(x / TOMBOL_W, 0, TOMBOL_N - 1) : (y < TOMBOL_Y - 20) ? 10 : -1;
  if (tombol != tombolLalu) {
    gambarTombol(tombol <= 5 ? tombol : -1);
    if (tombolLalu >= 0 && tombolLalu <= 3) kirimBadan("S");               // jari diangkat = berhenti
    if (tombol == 4) { gantiMode(M_BIASA); kirimBadan("S"); kirimAudio(A_STOP); setStatus("Berhenti"); }
    if (tombol == 5) { kirimAudio(A_STOP); mintaDengar = true; }   // BICARA: dengarkan tanpa perlu menyebut nama
    tombolLalu = tombol; tKirim = 0;
    if (tombol == 10) { lihatKameraDiLayar(); tombolLalu = -1; }
  }
  if (tombol >= 0 && tombol <= 3 && millis() - tKirim > 200) {             // tahan = terus bergerak
    tKirim = millis();
    const int L[4] = {-150, 150, -150, 150}, R[4] = {150, 150, -150, -150};
    gerak(L[tombol], R[tombol], 400);
  }
}

// ======================= SETUP & LOOP =======================
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
  delay(300);
  Serial.println("\n=== ROBOT OTAK ===");
  Serial.printf("PSRAM: %u byte (harus sekitar 8 juta)\n", ESP.getPsramSize());
  Serial1.begin(115200, SERIAL_8N1, BADAN_RX, BADAN_TX);
  kunciSPI = xSemaphoreCreateMutex(); kunciUART = xSemaphoreCreateMutex(); kunciFoto = xSemaphoreCreateMutex();
  antrianAudio = xQueueCreate(12, sizeof(PerintahAudio));
  antrianTugas = xQueueCreate(2, sizeof(Tugas));

  pref.begin("robot", false);
  uint8_t arah = pref.getUChar("rot", 6), geo = pref.getUChar("geo", 1), rgb = pref.getUChar("rgb", 1);   // pilihan di tes_layar
  pref.end();
  lcd.aturPanel(geo != 0, rgb != 0);
  muatLayar();
  lcd.init(); lcd.setRotation(arah > 7 ? 6 : arah);
  if (lcd.width() < lcd.height()) lcd.setRotation(geo ? 6 : 1);   // robot memakai layar mendatar 320 x 240
  lcd.setBrightness(255); lcd.fillScreen(TFT_BLACK);
  int32_t tx, ty; bool paksa = lcd.getTouchRaw(&tx, &ty);    // sentuh layar saat menyala = kalibrasi ulang
  kalibrasiSentuh(paksa);
  wajah.setPsram(true); wajah.setColorDepth(16); wajah.createSprite(320, 186);
  gambarTombol();

  SPI.begin(TFT_SCK, TFT_MISO, TFT_MOSI);
  adaSD = SD.begin(SD_CS, SPI, 20000000);
  Serial.println(adaSD ? "SD card terbaca" : "SD card tidak terbaca (boleh, hanya untuk musik)");

  audio.setPinout(AMP_BCLK, AMP_LRC, AMP_DIN);
  audio.setVolume(VOLUME_AWAL);
  mic.setPins(MIC_SCK, MIC_WS, -1, MIC_SD);
  // Mode STEREO, bukan MONO: di ESP32-S3 mode mono memberi data rusak (sebagian besar nol). Jalur kiri diambil di tugasDengar().
  if (!mic.begin(I2S_MODE_STD, MIC_RATE, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO))
    Serial.println("Mikrofon gagal mulai");

  setStatus("Menyambung WiFi...");
  gambarWajah();
  WiFi.mode(WIFI_STA); WiFi.begin(WIFI_NAMA, WIFI_PASSWORD);
  for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) delay(500);
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("IP otak: "); Serial.println(WiFi.localIP());
    MDNS.begin("robototak");
    alamatKamera = KAMERA_ALAMAT;
    tgClient.setCACert(TELEGRAM_CERTIFICATE_ROOT);
    bot.sendMessage(TELEGRAM_CHATID, String(NAMA_ROBOT) + " menyala. Ketik /bantuan");
    setStatus("Siap");
    ucapkan(String("Halo, aku ") + NAMA_ROBOT + ". Aku siap.");
  } else setStatus("WiFi gagal. Cek nama dan password");

  xTaskCreatePinnedToCore(tugasAudio, "audio", 12288, nullptr, 3, nullptr, 1);
  xTaskCreatePinnedToCore(tugasJaringan, "jaringan", 16384, nullptr, 1, nullptr, 0);
  xTaskCreatePinnedToCore(tugasDengar, "dengar", 6144, nullptr, 2, nullptr, 0);
}

void loop() {
  bacaBadan();
  tanganiSentuh();
  jalankanMode();
  static unsigned long tWajah = 0, tKaget = 0;
  if (ekspresi == KAGET && !tKaget) tKaget = millis();
  if (tKaget && millis() - tKaget > 1500) { if (ekspresi == KAGET) ekspresi = NORMAL; tKaget = 0; }
  if (millis() - tWajah > 100) { tWajah = millis(); gambarWajah(); }
  delay(5);
}
