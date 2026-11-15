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
#include <WebServer.h>
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
const char* KAMERA_ALAMAT   = "";                     // IP saja, contoh "192.168.1.50". Kosong = cari robotcam.local
const char* NAMA_ROBOT      = "Zond";                 // nama robot; perintah suara harus menyebut nama ini
const bool  DENGAR_NAMA     = true;                   // true = mendengar terus, menanggapi kalau namanya disebut
const uint8_t ARAH_LAYAR    = 6;                      // 6 = slot SD di atas, 4 = slot SD di bawah. Tampilan terbalik? ganti 6 <-> 4
                                                      // false = hanya mendengar setelah tombol BICARA
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
int volSekarang = VOLUME_AWAL;   // volume yang sedang dipakai, 0..21
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

// Pesan dari library Audio (format lagu, kesalahan baca, akhir lagu) dicetak ke Serial Monitor.
void infoAudio(Audio::msg_t m) {
  if (m.e == Audio::evt_vu || m.e == Audio::evt_spectrum) return;   // data meter, terlalu sering
  Serial.printf("[audio] %s: %s\n", m.s ? m.s : "", m.msg ? m.msg : "");
}

void tugasAudio(void*) {
  std::vector<String> antreTTS;
  bool pakaiSD = false;          // kunci bus SPI hanya saat memutar dari SD (layar memakai bus yang sama)
  String laguSekarang;           // untuk menyambung ulang kalau pembacaan SD macet
  uint32_t posLalu = 0; unsigned long tPosBerubah = 0;
  for (;;) {
    PerintahAudio p;
    while (xQueueReceive(antrianAudio, &p, 0) == pdTRUE) {
      xSemaphoreTake(kunciSPI, portMAX_DELAY);
      switch (p.jenis) {
        case A_FILE: antreTTS.clear(); pakaiSD = true; laguSekarang = p.isi; audio.connecttoFS(SD, p.isi); break;
        case A_URL:  antreTTS.clear(); pakaiSD = false; audio.connecttohost(p.isi); break;
        case A_TTS:  if (audio.isRunning() && !sedangBicara) audio.stopSong();   // bicara memotong musik
                     antreTTS.push_back(String(p.isi)); break;
        case A_STOP: antreTTS.clear(); audio.stopSong(); break;
        case A_VOL:  volSekarang = constrain(atoi(p.isi), 0, 21); audio.setVolume(volSekarang); break;
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
        if (adaSD) { xSemaphoreTake(kunciSPI, portMAX_DELAY); audio.connecttoFS(SD, laguSekarang.c_str(), detik); xSemaphoreGive(kunciSPI); }
        else { pakaiSD = false; setStatus("SD card terlepas"); }
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

// Pasang SD card. Kabel SD lewat breadboard dan berbagi jalur dengan layar, jadi kecepatan tinggi kadang gagal:
// coba beberapa kali, makin pelan. 10 MHz pun masih jauh cukup untuk FLAC. Dipanggil lagi kalau SD belum terbaca.
bool pasangSD() {
  if (adaSD) return true;
  const uint32_t frek[] = {10000000, 4000000, 4000000};   // 10 MHz cukup untuk FLAC; lebih cepat sering error di kabel jumper
  xSemaphoreTake(kunciSPI, portMAX_DELAY);
  for (uint32_t f : frek) {
    SD.end();
    digitalWrite(TFT_CS, HIGH); digitalWrite(TOUCH_CS, HIGH);   // layar dan sentuh tidak boleh ikut menjawab
    delay(50);
    if (SD.begin(SD_CS, SPI, f) && SD.cardType() != CARD_NONE) {
      adaSD = true;
      Serial.printf("SD card terbaca pada %u MHz (%llu MB)\n", (unsigned)(f / 1000000), SD.cardSize() / (1024 * 1024));
      break;
    }
  }
  xSemaphoreGive(kunciSPI);
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
  xSemaphoreTake(kunciSPI, portMAX_DELAY);
  File root = SD.open("/");
  for (File f = root.openNextFile(); f; f = root.openNextFile())
    if (!f.isDirectory() && fileLagu(f.name())) s += String(f.name()) + "\n";
  root.close();
  xSemaphoreGive(kunciSPI);
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
  "Perintah:\n/status  data sensor\n/maju /mundur /kiri /kanan [ms]\n/stop\n/kepala pan tilt  (90 90 = lurus)\n"
  "/foto  kirim foto kamera\n/lihat [pertanyaan]  AI melihat lewat kamera\n/bilang teks  robot mengucapkan teks\n"
  "/panel  alamat panel kontrol web\n/lagu  daftar lagu di SD\n/putar nama lagu (cukup sebagian)\n/radio url\n/diam  hentikan suara\n/volume 0-21\n"
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
      } else if (butuhNama && !adaNama) {
        Serial.println("[dengar] tanpa nama, diabaikan: " + kata);
      } else if (adaNama && sisa.length() == 0) {         // hanya memanggil nama: tunggu perintahnya
        if (butuhNama) ucapkan("Ya?");                    // sudah menunggu perintah: jangan jawab "Ya?" lagi,
        else Serial.println("[dengar] hanya nama lagi, diabaikan");   // supaya gema/bising tidak membuat putaran
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
// Tombol layar hanya untuk hiburan dan suara. Gerak lewat suara atau Telegram, karena orang yang menyentuh layar
// berdiri dekat robot, dan robot yang bergerak saat disentuh bisa menabrak kaki atau terjatuh dari meja.
enum { T_BICARA, T_LAGU, T_RADIO, T_VOLKURANG, T_VOLTAMBAH, T_STOP };
const char* labelTombol[TOMBOL_N] = {"BICARA", "LAGU", "RADIO", "VOL -", "VOL +", "STOP"};
const uint16_t warnaTombol[TOMBOL_N] = {0x0320, 0x2945, 0x2945, 0x4208, 0x4208, 0xA000};

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
  static int tombolLalu = -1;
  int32_t x, y; bool ada;
  xSemaphoreTake(kunciSPI, portMAX_DELAY);
  ada = lcd.getTouch(&x, &y);
  xSemaphoreGive(kunciSPI);
  // zona: 0..5 = tombol bawah, 10 = area wajah, -1 = tidak disentuh
  int baca = !ada ? -1 : (y >= TOMBOL_Y) ? constrain(x / TOMBOL_W, 0, TOMBOL_N - 1) : (y < TOMBOL_Y - 20) ? 10 : -1;
  // Sentuhan palsu (bising listrik dari speaker yang keras) hanya terbaca sesaat. Zona baru dianggap
  // sungguhan setelah terbaca sama 4 kali berturut-turut (sekitar 40 milidetik).
  static int calon = -1, ulang = 0;
  if (baca == calon) ulang++; else { calon = baca; ulang = 1; }
  int tombol = (ulang >= 4) ? calon : tombolLalu;
  if (tombol != tombolLalu && tombol >= 0) Serial.printf("[sentuh] tombol %d (x %d, y %d)\n", tombol, (int)x, (int)y);
  if (tombol != tombolLalu) {
    gambarTombol(tombol <= 5 ? tombol : -1);
    if (tombol >= 0 && tombol <= 5) tekanTombol(tombol);
    tombolLalu = tombol;
    if (tombol == 10) { lihatKameraDiLayar(); tombolLalu = -1; }
  }
}

void tekanTombol(int t) {
  static int lagu = -1, radio = -1;
  switch (t) {
    case T_BICARA: kirimAudio(A_STOP); mintaDengar = true; break;   // dengarkan tanpa perlu menyebut nama
    case T_LAGU: {                                                   // lagu berikutnya di SD card
      String daftar = daftarLagu();
      if (!adaSD || daftar.indexOf('.') < 0 || daftar.startsWith("Belum")) { setStatus("Tidak ada lagu di SD card"); break; }
      int n = 0; for (char c : daftar) if (c == '\n') n++;
      lagu = (lagu + 1) % n;
      int a = 0; for (int i = 0; i < lagu; i++) a = daftar.indexOf('\n', a) + 1;
      String nama = daftar.substring(a, daftar.indexOf('\n', a));
      kirimAudio(A_FILE, "/" + nama); setStatus("Lagu " + String(lagu + 1) + "/" + String(n) + ": " + nama);
      break;
    }
    case T_RADIO: {                                                  // stasiun berikutnya
      const int n = sizeof(DAFTAR_RADIO) / sizeof(DAFTAR_RADIO[0]);
      if (WiFi.status() != WL_CONNECTED) { setStatus("Radio butuh WiFi"); break; }
      radio = (radio + 1) % n;
      kirimAudio(A_URL, DAFTAR_RADIO[radio]); setStatus("Radio " + String(radio + 1) + "/" + String(n));
      break;
    }
    case T_VOLKURANG: case T_VOLTAMBAH: {
      int v = constrain(volSekarang + (t == T_VOLTAMBAH ? 2 : -2), 0, 21);
      kirimAudio(A_VOL, String(v)); volSekarang = v; setStatus("Volume " + String(v) + " dari 21");
      break;
    }
    case T_STOP: gantiMode(M_BIASA); kirimBadan("S"); kirimAudio(A_STOP); setStatus("Berhenti"); break;
  }
}

// ======================= PANEL WEB =======================
// Buka http://<IP otak>/ dari HP atau laptop di WiFi yang sama: gerak tahan-jalan, kamera, kepala, musik.
// Halamannya ditulis dengan Svelte (folder _build/panel di repo panduan) lalu ditanam di sini sebagai satu string.
// <PANEL_WEB> dibuat otomatis oleh _build/panel/bangun.ts, jangan diedit di sini
const char PANEL_HTML[] = R"PANEL(<!doctype html><html lang="id"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1"><title>Panel Zond</title><style>html{color:#e6e9ee;background:#10141a;font-family:system-ui,-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,Noto Sans,Ubuntu,Cantarell,Helvetica Neue,sans-serif}body{margin:0}main.svelte-1hsz8dnnzz8ho{max-width:480px;margin:0 auto;padding:12px 16px 32px}header.svelte-1hsz8dnnzz8ho{display:flex;flex-wrap:wrap;align-items: center;gap:8px}h1.svelte-1hsz8dnnzz8ho{margin:0;font-size:1.4rem}h2.svelte-1hsz8dnnzz8ho{margin:0 0 8px;font-size:1rem}h2.svelte-1hsz8dnnzz8ho small:where(.svelte-1hsz8dnnzz8ho){color:#8a94a3;font-weight:400}.kecil.svelte-1hsz8dnnzz8ho{color:#8a94a3;font-size:.85rem}.titik.svelte-1hsz8dnnzz8ho{background:#c0392b;border-radius:50%;width:10px;height:10px}.titik.on.svelte-1hsz8dnnzz8ho{background:#2ecc71}.peringatan.svelte-1hsz8dnnzz8ho{display:flex;flex-wrap:wrap;gap:6px;margin-top:8px}.peringatan.svelte-1hsz8dnnzz8ho span:where(.svelte-1hsz8dnnzz8ho){background:#7a1d1d;border-radius:6px;padding:3px 8px;font-size:.85rem}section.svelte-1hsz8dnnzz8ho{background:#18202a;border-radius:12px;margin-top:12px;padding:12px}.kamera.svelte-1hsz8dnnzz8ho{text-align:center}.kamera.svelte-1hsz8dnnzz8ho img:where(.svelte-1hsz8dnnzz8ho){display:block;aspect-ratio:4/3;object-fit:cover;background:#000;border-radius:8px;width:100%}.kamera.svelte-1hsz8dnnzz8ho img.redup:where(.svelte-1hsz8dnnzz8ho){opacity:.15}.ket.svelte-1hsz8dnnzz8ho{color:#8a94a3;margin:8px 0}.dpad.svelte-1hsz8dnnzz8ho{display:grid;grid-template-columns:repeat(3,1fr);touch-action:none;user-select:none;-webkit-user-select:none;gap:8px;max-width:300px;margin:0 auto 12px}.dpad.svelte-1hsz8dnnzz8ho button:where(.svelte-1hsz8dnnzz8ho){aspect-ratio:1;touch-action:none;background:#2a3a4d;border-radius:14px;font-size:1.6rem}.dpad.svelte-1hsz8dnnzz8ho button.aktif:where(.svelte-1hsz8dnnzz8ho){background:#3d7bd9}.dpad.svelte-1hsz8dnnzz8ho button.stop:where(.svelte-1hsz8dnnzz8ho){background:#a0302a}button.svelte-1hsz8dnnzz8ho{color:inherit;cursor:pointer;background:#2a3a4d;border:0;border-radius:10px;padding:10px 12px;font-size:.95rem}button.svelte-1hsz8dnnzz8ho:active{filter:brightness(1.3)}.kecil-btn.svelte-1hsz8dnnzz8ho{margin-top:8px;padding:6px 10px;font-size:.85rem}.baris.svelte-1hsz8dnnzz8ho{display:flex;flex-wrap:wrap;gap:8px}.baris.svelte-1hsz8dnnzz8ho button:where(.svelte-1hsz8dnnzz8ho){flex:1}label.svelte-1hsz8dnnzz8ho{display:block;margin-top:10px;font-size:.9rem}input[type=range].svelte-1hsz8dnnzz8ho{width:100%}form.svelte-1hsz8dnnzz8ho{display:flex;gap:8px;margin-top:12px}input[type=text].svelte-1hsz8dnnzz8ho{color:inherit;background:#10141a;border:1px solid #2a3a4d;border-radius:10px;flex:1;min-width:0;padding:10px;font-size:1rem}.status.svelte-1hsz8dnnzz8ho{text-align:center;color:#8a94a3;margin-top:12px}
</style></head><body><script type="module">var l=!0;var vt=Array.isArray,di=Array.prototype.indexOf,gt=Array.prototype.includes,sr=Array.from,{keys:Wn,defineProperty:te,getOwnPropertyDescriptor:Oe,getOwnPropertyDescriptors:Xn,prototype:Zn}=Object,pi=Array.prototype,{getPrototypeOf:ar,isExtensible:Jn}=Object;var st=()=>{};function Zr(e){for(var t=0;t<e.length;t++)e[t]()}function Jr(){var e,t,r=new Promise((n,o)=>{e=n,t=o});return{promise:r,resolve:e,reject:t}}function Qn(e,t){if(Array.isArray(e))return e;if(t===void 0||!(Symbol.iterator in e))return Array.from(e);let r=[];for(let n of e)if(r.push(n),r.length===t)break;return r}var Y=2,xt=4,Tt=8,Bt=16777216,fe=16,re=32,Se=64,fr=128,Qr=256,be=512,D=1024,N=2048,H=4096,j=8192,I=16384,we=32768,bt=33554432,Ne=65536,kt=131072,eo=262144,at=524288,_i=1048576,Ze=33554432,lr=2097152,De=4194304,Be=8388608,ze=Symbol("$state"),en=Symbol("component"),tn=Symbol("legacy props"),hi=Symbol(""),rn=Symbol("proxy path"),nn=Symbol("attributes"),Sr=Symbol("class"),to=Symbol("style"),Nr=Symbol("text"),At=Symbol("form reset"),ro=Symbol("hmr anchor"),wt=new class extends Error{name="StaleReactionError";message="The reaction that called `getAbortSignal()` was re-run or destroyed"},no=!!globalThis.document?.contentType&&globalThis.document.contentType.includes("xml");var Ht=3,He=8;var mi=1,vi=2,oo=4,gi=8,xi=16;var bi=1,wi=2;var cr="[",St="[!",io="[?",Vt="]",Ie={};var M=Symbol("uninitialized"),Ve=Symbol("filename"),so=Symbol("hmr"),on="http://www.w3.org/1999/xhtml";var Nt="font-weight: bold",Rt="font-weight: normal";function yi(e){if(l)console.warn(`%c[svelte] await_reactivity_loss
%cDetected reactivity loss when reading \`${e}\`. This happens when state is read in an async function after an earlier \`await\`
https://svelte.dev/e/await_reactivity_loss`,Nt,Rt);else console.warn("https://svelte.dev/e/await_reactivity_loss")}function Ei(e,t){if(l)console.warn(`%c[svelte] await_waterfall
%cAn async derived, \`${e}\` (${t}) was not read immediately after it resolved. This often indicates an unnecessary waterfall, which can slow down your app
https://svelte.dev/e/await_waterfall`,Nt,Rt);else console.warn("https://svelte.dev/e/await_waterfall")}function $i(){if(l)console.warn(`%c[svelte] derived_inert
%cReading a derived belonging to a now-destroyed effect may result in stale values
https://svelte.dev/e/derived_inert`,Nt,Rt);else console.warn("https://svelte.dev/e/derived_inert")}function Ti(e,t,r){if(l)console.warn(`%c[svelte] hydration_attribute_changed
%cThe \`${e}\` attribute on \`${t}\` changed its value between server and client renders. The client value, \`${r}\`, will be ignored in favour of the server value
https://svelte.dev/e/hydration_attribute_changed`,Nt,Rt);else console.warn("https://svelte.dev/e/hydration_attribute_changed")}function Ut(e){if(l)console.warn(`%c[svelte] hydration_mismatch
%c${e?`Hydration failed because the initial UI does not match what was rendered on the server. The error occurred near ${e}`:"Hydration failed because the initial UI does not match what was rendered on the server"}
https://svelte.dev/e/hydration_mismatch`,Nt,Rt);else console.warn("https://svelte.dev/e/hydration_mismatch")}function ki(){if(l)console.warn(`%c[svelte] lifecycle_double_unmount
%cTried to unmount a component that was not mounted
https://svelte.dev/e/lifecycle_double_unmount`,Nt,Rt);else console.warn("https://svelte.dev/e/lifecycle_double_unmount")}function sn(e){if(l)console.warn(`%c[svelte] state_proxy_equality_mismatch
%cReactive \`$state(...)\` proxies and the values they proxy have different identities. Because of this, comparisons with \`${e}\` will produce unexpected results
https://svelte.dev/e/state_proxy_equality_mismatch`,Nt,Rt);else console.warn("https://svelte.dev/e/state_proxy_equality_mismatch")}function Ai(){if(l)console.warn("%c[svelte] svelte_boundary_reset_noop\n%cA `<svelte:boundary>` `reset` function only resets the boundary the first time it is called\nhttps://svelte.dev/e/svelte_boundary_reset_noop",Nt,Rt);else console.warn("https://svelte.dev/e/svelte_boundary_reset_noop")}var x=!1;function W(e){x=e}var k;function O(e){if(e===null)throw Ut(),Ie;return k=e}function me(){return O(ne(k))}function G(e){if(!x)return;if(ne(k)!==null)throw Ut(),Ie;k=e}function Rr(e=1){if(x){var t=e,r=k;while(t--)r=ne(r);k=r}}function ft(e=!0){var t=0,r=k;while(!0){if(r.nodeType===He){var n=r.data;if(n===Vt){if(t===0)return r;t-=1}else if(n===cr||n===St||n[0]==="["&&!isNaN(Number(n.slice(1))))t+=1}var o=ne(r);if(e)r.remove();r=o}}function qt(e){if(!e||e.nodeType!==He)throw Ut(),Ie;return e.data}function an(e){return e===this.v}function ao(e,t){return e!=e?t==t:e!==t||e!==null&&typeof e==="object"||typeof e==="function"}function fn(e){return!ao(e,this.v)}function Si(e){if(l){let t=Error(`invariant_violation
An invariant violation occurred, meaning Svelte's internal assumptions were flawed. This is a bug in Svelte, not your app — please open an issue at https://github.com/sveltejs/svelte, citing the following message: "${e}"
https://svelte.dev/e/invariant_violation`);throw t.name="Svelte error",t}else throw Error("https://svelte.dev/e/invariant_violation")}function ln(e){if(l){let t=Error(`lifecycle_outside_component
\`${e}(...)\` can only be used during component initialisation
https://svelte.dev/e/lifecycle_outside_component`);throw t.name="Svelte error",t}else throw Error("https://svelte.dev/e/lifecycle_outside_component")}function Ri(){if(l){let e=Error("async_derived_orphan\nCannot create a `$derived(...)` with an `await` expression outside of an effect tree\nhttps://svelte.dev/e/async_derived_orphan");throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/async_derived_orphan")}function fo(){if(l){let e=Error("bind_invalid_checkbox_value\nUsing `bind:value` together with a checkbox input is not allowed. Use `bind:checked` instead\nhttps://svelte.dev/e/bind_invalid_checkbox_value");throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/bind_invalid_checkbox_value")}function Ci(){if(l){let e=Error(`derived_references_self
A derived value cannot reference itself recursively
https://svelte.dev/e/derived_references_self`);throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/derived_references_self")}function lo(e,t,r){if(l){let n=Error(`each_key_duplicate
${r?`Keyed each block has duplicate key \`${r}\` at indexes ${e} and ${t}`:`Keyed each block has duplicate key at indexes ${e} and ${t}`}
https://svelte.dev/e/each_key_duplicate`);throw n.name="Svelte error",n}else throw Error("https://svelte.dev/e/each_key_duplicate")}function Oi(e,t,r){if(l){let n=Error(`each_key_volatile
Keyed each block has key that is not idempotent — the key for item at index ${e} was \`${t}\` but is now \`${r}\`. Keys must be the same each time for a given item
https://svelte.dev/e/each_key_volatile`);throw n.name="Svelte error",n}else throw Error("https://svelte.dev/e/each_key_volatile")}function Di(e){if(l){let t=Error(`effect_in_teardown
\`${e}\` cannot be used inside an effect cleanup function
https://svelte.dev/e/effect_in_teardown`);throw t.name="Svelte error",t}else throw Error("https://svelte.dev/e/effect_in_teardown")}function zi(){if(l){let e=Error("effect_in_unowned_derived\nEffect cannot be created inside a `$derived` value that was not itself created inside an effect\nhttps://svelte.dev/e/effect_in_unowned_derived");throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/effect_in_unowned_derived")}function Ii(e){if(l){let t=Error(`effect_orphan
\`${e}\` can only be used inside an effect (e.g. during component initialisation)
https://svelte.dev/e/effect_orphan`);throw t.name="Svelte error",t}else throw Error("https://svelte.dev/e/effect_orphan")}function Mi(){if(l){let e=Error(`effect_update_depth_exceeded
Maximum update depth exceeded. This typically indicates that an effect reads and writes the same piece of state
https://svelte.dev/e/effect_update_depth_exceeded`);throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/effect_update_depth_exceeded")}function Li(){if(l){let e=Error(`hydration_failed
Failed to hydrate the application
https://svelte.dev/e/hydration_failed`);throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/hydration_failed")}function Fi(e){if(l){let t=Error(`rune_outside_svelte
The \`${e}\` rune is only available inside \`.svelte\` and \`.svelte.js/ts\` files
https://svelte.dev/e/rune_outside_svelte`);throw t.name="Svelte error",t}else throw Error("https://svelte.dev/e/rune_outside_svelte")}function Pi(){if(l){let e=Error("state_descriptors_fixed\nProperty descriptors defined on `$state` objects must contain `value` and always be `enumerable`, `configurable` and `writable`.\nhttps://svelte.dev/e/state_descriptors_fixed");throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/state_descriptors_fixed")}function Yi(){if(l){let e=Error("state_prototype_fixed\nCannot set prototype of `$state` object\nhttps://svelte.dev/e/state_prototype_fixed");throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/state_prototype_fixed")}function Bi(){if(l){let e=Error("state_unsafe_mutation\nUpdating state inside `$derived(...)`, `$inspect(...)` or a template expression is forbidden. If the value should not be reactive, declare it without `$state`\nhttps://svelte.dev/e/state_unsafe_mutation");throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/state_unsafe_mutation")}function Hi(){if(l){let e=Error("svelte_boundary_reset_onerror\nA `<svelte:boundary>` `reset` function cannot be called while an error is still being handled\nhttps://svelte.dev/e/svelte_boundary_reset_onerror");throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/svelte_boundary_reset_onerror")}var X=!1,Ct=!1,Je=!1;var Cr=null;function ve(e,t){return e.label=t,un(e.v,t),e}function un(e,t){return e?.[rn]?.(t),e}function Qe(e){let t=Error(),r=la();if(r.length===0)return null;return r.unshift(`
`),te(t,"stack",{value:r.join(`
`)}),te(t,"name",{value:e}),t}function la(){let e=Error.stackTraceLimit;Error.stackTraceLimit=1/0;let t=Error().stack;if(Error.stackTraceLimit=e,!t)return[];let r=t.split(`
`),n=[];for(let o=0;o<r.length;o++){let i=r[o],s=i.replaceAll("\\","/");if(i.trim()==="Error")continue;if(i.includes("validate_each_keys"))return[];if(s.includes("svelte/src/internal")||s.includes("node_modules/.vite"))continue;n.push(i)}return n}function Ui(e,t){if(!l)throw Error("invariant(...) was not guarded by if (DEV)");if(!e)Si(t)}var S=null;function yt(e){S=e}var lt=null;function ur(e){lt=e}var et=null;function dn(e){et=e}function Or(e,t=!1,r){if(S={p:S,i:!1,c:null,e:null,s:e,x:null,r:b,l:Ct&&!t?{s:null,u:null,$:[]}:null},l)S.function=r,et=r}function Dr(e){var t=S,r=t.e;if(r!==null){t.e=null;for(var n of r)co(n)}if(e!==void 0)t.x=e;if(t.i=!0,S=t.p,l)et=S?.function??null;return pn(e)}function pn(e={}){return te(e,en,{value:!0}),e}function ct(){return!Ct||S!==null&&S.l===null}var jt=[];function qi(){var e=jt;jt=[],Zr(e)}function K(e){if(jt.length===0&&!Gt){var t=jt;queueMicrotask(()=>{if(t===jt)qi()})}jt.push(e)}function ji(){while(jt.length>0)qi()}var ca=~(N|H|D);function z(e,t){e.f=e.f&ca|t}function dr(e){if((e.f&be)!==0||e.deps===null)z(e,D);else z(e,H)}function _n(e,t){if((e.f&N)!==0)t.set(e,N);else if((e.f&H)!==0&&t.get(e)!==N)t.set(e,H);z(e,D)}function Gi(e){return`{${e.toString().replace(/^async \(\) => /,"").replace(/^\(/,"").replace(/\$\.get\((.+?)\)/g,(t,r)=>r).replace(/\$\.track_reactivity_loss\((.+?)\)\(\)/g,(t,r)=>r).replace(/\)$/,"")}}`}var Ki=!1;function hn(){if(!Ki)Ki=!0,document.addEventListener("reset",(e)=>{Promise.resolve().then(()=>{if(!e.defaultPrevented)for(let t of e.target.elements)t[At]?.()})},{capture:!0})}function Me(e){var t=w,r=b;Z(null),J(null);try{return e()}finally{Z(t),J(r)}}function uo(e,t,r,n=r){e.addEventListener(t,()=>Me(r));let o=e[At];if(o)e[At]=()=>{o(),n(!0)};else e[At]=()=>n(!0);hn()}function mn(e,t,r,n){let o=ct()?_r:hr;var i=e.filter((c)=>!c.settled),s=t.map(o);if(l)s.forEach((c,v)=>{c.label=t[v].toString().replace("() => ","").replaceAll("$.eager(() => ","$state.eager(").replace(/\$\.get\((.+?)\)/g,(T,g)=>g)});if(r.length===0&&i.length===0){n(s);return}var a=b,f=Wi(),_=i.length===1?i[0].promise:i.length>1?Promise.all(i.map((c)=>c.promise)):null;function p(c){if((a.f&I)!==0)return;f();try{n([...s,...c])}catch(v){Re(v,a)}pr()}var d=po();if(r.length===0){_.then(()=>p([])).finally(d);return}function u(){Promise.all(r.map((c)=>ho(c))).then(p).catch((c)=>Re(c,a)).finally(d)}if(_)_.then(()=>{if((a.f&I)!==0){d();return}f(),u(),pr()});else u()}function Wi(){var e=b,t=w,r=S,n=y;if(l)var o=lt;return function(s=!0){if(J(e),Z(t),yt(r),s&&(e.f&I)===0)n?.activate(),n?.apply();if(l)_o(null),ur(o)}}var ua=!1;function pr(e=!0){if(ua=!1,J(null),Z(null),yt(null),e)y?.deactivate();if(l)_o(null),ur(null)}function po(){var e=b,t=e.b,r=y,n=!!t?.is_rendered();return t?.update_pending_count(1,r),r.increment(n,e),()=>{t?.update_pending_count(-1,r),r.decrement(n,e)}}var Ce=null;function _o(e){Ce=e}var Ir=new Set;function _r(e){var t=Y|N;if(b!==null)b.f|=at;let r={ctx:S,deps:null,effects:null,equals:an,f:t,fn:e,reactions:null,rv:0,v:M,wv:0,parent:b,ac:null};if(l&&Je)r.created=Qe("created at");return r}var mr=Symbol("obsolete");function ho(e,t,r){let n=b;if(n===null)Ri();var o=void 0,i=ge(M);if(l)i.label=t??Gi(e);var s=!w,a=new Set;if(Ji(()=>{var f=b;if(l)Ce={effect:f,effect_deps:new Set,warned:!1};var _=Jr();o=_.promise;try{Promise.resolve(e()).then(_.resolve,(c)=>{if(c!==wt)_.reject(c)}).finally(pr)}catch(c){_.reject(c),pr()}if(l){if(Ce){if(f.deps!==null)for(let c=0;c<ye;c+=1)Ce.effect_deps.add(f.deps[c]);if(oe!==null)for(let c=0;c<oe.length;c+=1)Ce.effect_deps.add(oe[c])}Ce=null}var p=y;if(s){if((f.f&we)!==0)var d=po();if(n.b?.is_rendered())p.async_deriveds.get(f)?.reject(mr);else for(let c of a.values())c.reject(mr);a.add(_),p.async_deriveds.set(f,_)}let u=(c,v=void 0)=>{if(l)Ce=null;if(d?.(),a.delete(_),v===mr)return;if(p.activate(),v)i.f|=Be,tt(i,v);else{if((i.f&Be)!==0)i.f^=Be;if(l&&r!==void 0&&!i.equals(c))Ir.add(i),setTimeout(()=>{if(Ir.has(i)&&(f.f&I)===0)Ei(i.label,r),Ir.delete(i)});tt(i,c)}p.deactivate()};_.promise.then(u,(c)=>u(null,c||"unknown"))}),de(()=>{for(let f of a)f.reject(mr)}),l)i.f|=De;return new Promise((f)=>{function _(p){function d(){if(p===o)f(i);else _(o)}p.then(d,d)}_(o)})}function vo(e){let t=_r(e);if(!X)xn(t);return t}function hr(e){let t=_r(e);return t.equals=fn,t}function Xi(e){var t=e.effects;if(t!==null){e.effects=null;for(var r=0;r<t.length;r+=1)V(t[r])}}var mo=[];function Mr(e){var t,r=b,n=e.parent;if(!qe&&n!==null&&e.v!==M&&(n.f&(I|j))!==0)return $i(),e.v;if(J(n),l){let o=Kt;vn(new Set);try{if(gt.call(mo,e))Ci();mo.push(e),Xi(e),t=gn(e)}finally{J(r),vn(o),mo.pop()}}else try{Xi(e),t=gn(e)}finally{J(r)}return t}function go(e){var t=Mr(e);if(!e.equals(t)){if(e.wv=vr(),!y?.is_fork||e.deps===null){if(y!==null)y.capture(e,t,!0),Ue?.capture(e,t,!0);else e.v=t;if(e.deps===null){z(e,D);return}}}if(qe)return;if(ie!==null){if(Ot()||y?.is_fork)ie.set(e,t)}else dr(e)}function Zi(e){if(e.effects===null)return;for(let t of e.effects)if(t.teardown||t.ac){if(t.teardown?.(),t.ac!==null)Me(()=>{t.ac.abort(wt),t.ac=null});if(t.fn!==null)t.teardown=st;Wt(t,0),Lr(t)}}function xo(e){if(e.effects===null)return;for(let t of e.effects)if(t.teardown&&t.fn!==null)ut(t)}var bn=null,gr=null,y=null,Ue=null,ie=null,yo=null,Gt=!1,bo=!1,xr=null,Fr=null,Qi=0,wo=new Set,_a=1;class pe{id=_a++;started=!1;linked=!0;prev=null;next=null;async_deriveds=new Map;current=new Map;previous=new Map;#e=new Set;#t=new Set;#r=0;#a=new Map;#o=null;#n=[];#s=[];#i=new Map;#f=new Map;#p=new Set;is_fork=!1;#l=!1;constructor(){if(gr===null)bn=gr=this;else gr.next=this,this.prev=gr;gr=this}#h(){if(this.is_fork)return!0;for(let e of this.#a.keys())if(!this.#g(e))return!0;return!1}#g(e){for(var t=e;t!==null;t=t.parent)if(this.#f.has(t))return!0;return!1}skip_effect(e){if(!this.#f.has(e))this.#f.set(e,{d:[],m:[]});this.#p.delete(e)}unskip_effect(e,t=(r)=>this.schedule(r)){var r=this.#f.get(e);if(r){this.#f.delete(e);for(var n of r.d)z(n,N),t(n);for(n of r.m)z(n,H),t(n)}this.#p.add(e)}#c(){var e=[];for(let o of this.#n){if((o.f&I)!==0||(o.f&(N|H))===0)continue;var t=o,r=!1;while(t.parent!==null){t=t.parent;var n=t.f;if((n&(Se|re))!==0){if((n&D)===0){r=!0;break}t.f^=D}}if(!r)e.push(t)}return this.#n=[],e}#m(){if(this.started=!0,l)for(let s of this.current.keys())wo.add(s);for(let[s,a]of this.#i)z(s,a),this.schedule(s);this.apply();var e=xr=[],t=[],r=Fr=[];while(this.#n.length>0){if(Qi++>1000)this.#d(),ha();for(let s of this.#c())try{this.#x(s,e,t)}catch(a){if(ns(s),!this.#h())this.discard();throw a}}if(y=null,r.length>0){var n=pe.ensure();for(let s of r)n.schedule(s)}if(xr=null,Fr=null,this.#h()){this.#_(t),this.#_(e);for(let[s,a]of this.#f)rs(s,a);if(r.length>0)y.#m();return}let o=this.#w();if(o){this.#_(t),this.#_(e),o.#u(this);return}this.#i.clear();for(let s of this.#e)s(this);this.#e.clear(),Ue=this,es(t),es(e),Ue=null,this.#o?.resolve();var i=y;if(this.#r===0&&(this.#n.length===0||i!==null)){if(this.#d(),X)this.#b(),y=i}if(i!==null)Ge.clear(),i.#m()}#x(e,t,r){e.f^=D;var n=e.first;while(n!==null){var o=n.f,i=(o&(re|Se))!==0,s=i&&(o&D)!==0,a=s||(o&j)!==0||this.#f.has(n);if(!a&&n.fn!==null){if(i)n.f^=D;else if((o&xt)!==0)t.push(n);else if(X&&(o&(Tt|Bt))!==0)r.push(n);else if(zt(n)){if((o&fe)!==0)this.add_dirty_reaction(n,H);ut(n)}var f=n.first;if(f!==null){n=f;continue}}while(n!==null){var _=n.next;if(_!==null){n=_;break}n=n.parent}}}#w(){for(var e=this.prev;e!==null;e=e.prev)if(!e.is_fork&&this.#v(e))return e;return null}#v(e){for(let[t,[,r]]of this.current){if(r)continue;if(e.current.has(t))return!0}return!1}#u(e){for(let[r,n]of e.current){if(!this.previous.has(r)&&e.previous.has(r))this.previous.set(r,e.previous.get(r));this.current.set(r,n)}for(let[r,n]of e.async_deriveds){let o=this.async_deriveds.get(r);if(o)n.promise.then(o.resolve).catch(o.reject)}e.async_deriveds.clear(),this.transfer_reactions(e.#i);let t=(r)=>{var n=r.reactions;if(n===null)return;if((r.f&Y)!==0&&(r.f&(N|H))===0)return;for(let s of n){var o=s.f;if((o&Y)!==0)t(s);else{var i=s;if(o&(De|fe)&&!this.async_deriveds.has(i))this.#i.delete(i),z(i,N),this.schedule(i)}}};for(let r of this.current.keys())t(r);this.oncommit(()=>e.discard()),e.#d(),y=this,this.#m()}#_(e){for(var t=0;t<e.length;t+=1)_n(e[t],this.#i)}capture(e,t,r=!1){if(e.v!==M&&!this.previous.has(e))this.previous.set(e,e.v);if((e.f&Be)===0)this.current.set(e,[t,r]),ie?.set(e,t);if(!this.is_fork)e.v=t}activate(){y=this}deactivate(){y=null,ie=null}flush(){try{if(l)wo.clear();bo=!0,y=this,this.#m()}finally{if(Qi=0,yo=null,xr=null,Fr=null,bo=!1,y=null,ie=null,Ge.clear(),l)for(let e of wo)e.updated=null}}discard(){for(let e of this.#t)e(this);this.#t.clear();for(let e of this.async_deriveds.values())e.reject(mr);this.#d(),this.#o?.resolve()}register_created_effect(e){this.#s.push(e)}#b(){for(let p=bn;p!==null;p=p.next){var e=p.id<this.id,t=[];for(let[d,[u,c]]of this.current){if(p.current.has(d)){var r=p.current.get(d)[0];if(e&&u!==r)p.current.set(d,[u,c]);else continue}t.push(d)}if(e)for(let[d,u]of this.async_deriveds){let c=p.async_deriveds.get(d);if(c)u.promise.then(c.resolve).catch(c.reject)}var n=[...p.current.keys()].filter((d)=>!p.current.get(d)[1]);if(!p.started||n.length===0)continue;var o=n.filter((d)=>!this.current.has(d));if(o.length===0){if(e)p.discard()}else if(t.length>0){if(l&&!p.#l)Ui(p.#n.length===0,"Batch has scheduled effects");if(e)for(let d of this.#p)p.unskip_effect(d,(u)=>{if((u.f&(fe|De))!==0)p.schedule(u);else p.#_([u])});p.activate();var i=new Set,s=new Map;for(var a of t)ts(a,o,i,s);s=new Map;var f=[...p.current].filter(([d,u])=>{let c=this.current.get(d);if(!c)return!0;return c[0]!==u[0]||c[1]!==u[1]}).map(([d])=>d);if(f.length>0){for(let d of this.#s)if((d.f&(I|j|kt))===0&&Eo(d,f,s))if((d.f&(De|fe))!==0)z(d,N),p.schedule(d);else p.add_dirty_reaction(d,N)}if(p.#n.length>0&&!p.#l){p.apply();for(var _ of p.#c())p.#x(_,[],[])}p.deactivate()}}}increment(e,t){if(this.#r+=1,e){let r=this.#a.get(t)??0;this.#a.set(t,r+1)}}decrement(e,t){if(this.#r-=1,e){let r=this.#a.get(t)??0;if(r===1)this.#a.delete(t);else this.#a.set(t,r-1)}if(this.#l)return;this.#l=!0,K(()=>{if(this.#l=!1,this.linked)this.flush()})}transfer_reactions(e){for(let[t,r]of e)this.add_dirty_reaction(t,r);e.clear()}add_dirty_reaction(e,t){if(t===N||this.#i.get(e)!==N)this.#i.set(e,t)}oncommit(e){this.#e.add(e)}ondiscard(e){this.#t.add(e)}settled(){return(this.#o??=Jr()).promise}static ensure(){if(y===null){let e=y=new pe;if(!bo&&!Gt)K(()=>{if(!e.started)e.flush()})}return y}apply(){if(!X||!this.is_fork&&this.prev===null&&this.next===null){ie=null;return}ie=new Map;for(let[e,[t]]of this.current)ie.set(e,t);for(let e=bn;e!==null;e=e.next){if(e===this||e.is_fork)continue;if(e.id>=this.id||!e.#v(this)){for(let[t,r]of e.previous)if(!ie.has(t))ie.set(t,r)}}}schedule(e){if(yo=e,e.b?.is_pending&&(e.f&(xt|Tt|Bt))!==0&&(e.f&we)===0){e.b.defer_effect(e);return}this.#n.push(e)}#d(){if(!this.linked)return;var e=this.prev,t=this.next;if(e===null)bn=t;else e.next=t;if(t===null)gr=e;else t.prev=e;this.linked=!1}}function Dt(e){var t=Gt,r=Ue;Ue=null,Gt=!0;try{var n;if(e)Dt(),n=e();while(!0){if(ji(),y===null)return n;y.flush()}}finally{Gt=t,Ue=r}}function ha(){if(l){var e=new Map;for(let r of y.current.keys())for(let[n,o]of r.updated??[]){var t=e.get(n);if(!t)t={error:o.error,count:0},e.set(n,t);t.count+=o.count}for(let r of e.values())if(r.error)console.error(r.error)}try{Mi()}catch(r){if(l)te(r,"stack",{value:""});Re(r,yo)}}var je=null;function es(e){var t=e.length;if(t===0)return;var r=0;while(r<t){var n=e[r++];if((n.f&(I|j))===0&&zt(n)){if(je=new Set,ut(n),n.deps===null&&n.first===null&&n.nodes===null&&n.teardown===null&&n.ac===null)$o(n);if(je?.size>0){Ge.clear();for(let o of je){if((o.f&(I|j))!==0)continue;let i=[o],s=o.parent;while(s!==null){if(je.has(s))je.delete(s),i.push(s);s=s.parent}for(let a=i.length-1;a>=0;a--){let f=i[a];if((f.f&(I|j))!==0)continue;ut(f)}}je.clear()}}}je=null}function ts(e,t,r,n){if(r.has(e))return;if(r.add(e),e.reactions!==null)for(let o of e.reactions){let i=o.f;if((i&Y)!==0)ts(o,t,r,n);else if((i&(De|fe))!==0&&(i&N)===0&&Eo(o,t,n))z(o,N),Pr(o)}}function Eo(e,t,r){let n=r.get(e);if(n!==void 0)return n;if(e.deps!==null)for(let o of e.deps){if(gt.call(t,o))return!0;if((o.f&Y)!==0&&Eo(o,t,r))return r.set(o,!0),!0}return r.set(e,!1),!1}function Pr(e){y.schedule(e)}function rs(e,t){if((e.f&re)!==0&&(e.f&D)!==0)return;if((e.f&N)!==0)t.d.push(e);else if((e.f&H)!==0)t.m.push(e);z(e,D);var r=e.first;while(r!==null)rs(r,t),r=r.next}function ns(e){z(e,D);var t=e.first;while(t!==null)ns(t),t=t.next}var Kt=new Set,Ge=new Map;function vn(e){Kt=e}var ko=!1;function is(){ko=!0}function ge(e,t){var r={f:0,v:e,reactions:null,equals:an,rv:0,wv:0};if(l&&Je)r.created=t??Qe("created at"),r.updated=null,r.set_during_effect=!1,r.trace=null;return r}function U(e,t){let r=ge(e,t);return xn(r),r}function Zt(e,t=!1,r=!0){let n=ge(e);if(!t)n.equals=fn;if(Ct&&r&&S!==null&&S.l!==null)(S.l.s??=[]).push(n);return n}function R(e,t,r=!1){if(w!==null&&(!Ee||(w.f&kt)!==0)&&ct()&&(w.f&(Y|fe|De|kt))!==0&&(rt===null||!rt.has(e)))Bi();let n=r?It(t):t;if(l)un(n,e.label);return tt(e,n,Fr)}var Xt=null,To=0;function tt(e,t,r=null){if(!e.equals(t)){if(qe)Ge.set(e,t);else if(!Ge.has(e))Ge.set(e,e.v);var n=pe.ensure();if(n.capture(e,t),l){if(Je||b!==null){e.updated??=new Map;let o=(e.updated.get("")?.count??0)+1;if(e.updated.set("",{error:null,count:o}),Je||o>5){let i=Qe("updated at");if(i!==null){let s=e.updated.get(i.stack);if(!s)s={error:i,count:0},e.updated.set(i.stack,s);s.count++}}}if(b!==null)e.set_during_effect=!0}if((e.f&Y)!==0){let o=e;if((e.f&N)!==0)Mr(o);if(ie===null)dr(o)}if(e.wv=vr(),Xt=null,To=0,ss(e,N,r),Xt=null,ct()&&b!==null&&(b.f&D)!==0&&(b.f&(re|Se))===0)if(Le===null)as([e]);else Le.push(e);if(!n.is_fork&&Kt.size>0&&!ko)wn()}return t}function wn(){ko=!1;for(let e of Kt){if((e.f&D)!==0)z(e,H);let t;try{t=zt(e)}catch{t=!0}if(t)ut(e)}Kt.clear()}function Jt(e){R(e,e.v+1)}function ss(e,t,r){var n=e.reactions;if(n===null)return;var o=ct(),i=n.length;if(To+=i,To>1e5&&Xt===null)Xt=new Set;if(Xt!==null){if(Xt.has(e))return;Xt.add(e)}for(var s=0;s<i;s++){var a=n[s],f=a.f;if(!o&&a===b)continue;var _=(f&N)===0;if(_)z(a,t);if((f&kt)!==0)Kt.add(a);else if((f&Y)!==0){var p=a;ie?.delete(p),ss(p,H,r)}else if(_){var d=a;if((f&fe)!==0&&je!==null)je.add(d);if(r!==null)r.push(d);else Pr(d)}}}var va=/^[a-zA-Z_$][a-zA-Z_$0-9]*$/;function It(e){if(typeof e!=="object"||e===null||ze in e||en in e)return e;let t=ar(e);if(t!==Zn&&t!==pi)return e;var r=new Map,n=vt(e),o=U(0),i=l&&Je?Qe("created at"):null,s=Mt,a=(d)=>{if(Mt===s)return d();var u=w,c=Mt;Z(null),Ao(s);var v=d();return Z(u),Ao(c),v};if(n){if(r.set("length",U(e.length,i)),l)e=xa(e)}var f="";let _=!1;function p(d){if(_)return;_=!0,f=d,ve(o,`${f} version`);for(let[u,c]of r)ve(c,Qt(f,u));_=!1}return new Proxy(e,{defineProperty(d,u,c){if(!("value"in c)||c.configurable===!1||c.enumerable===!1||c.writable===!1)Pi();var v=r.get(u);if(v===void 0)a(()=>{var T=U(c.value,i);if(r.set(u,T),l&&typeof u==="string")ve(T,Qt(f,u));return T});else R(v,c.value,!0);return!0},deleteProperty(d,u){var c=r.get(u);if(c===void 0){if(u in d){let v=a(()=>U(M,i));if(r.set(u,v),Jt(o),l)ve(v,Qt(f,u))}}else R(c,M),Jt(o);return!0},get(d,u,c){if(u===ze)return e;if(l&&u===rn)return p;var v=r.get(u),T=u in d;if(v===void 0&&(!T||Oe(d,u)?.writable))v=a(()=>{var E=It(T?d[u]:M),C=U(E,i);if(l)ve(C,Qt(f,u));return C}),r.set(u,v);if(v!==void 0){var g=h(v);return g===M?void 0:g}return Reflect.get(d,u,c)},getOwnPropertyDescriptor(d,u){this.has?.(d,u);var c=Reflect.getOwnPropertyDescriptor(d,u),v=r.get(u);if(v!==void 0){var T=h(v);if(T===M)return;if(c&&"value"in c)c.value=T;else return{enumerable:!0,configurable:!0,value:T,writable:!0}}return c},has(d,u){if(u===ze)return!0;var c=r.get(u),v=c!==void 0&&c.v!==M||Reflect.has(d,u);if(c!==void 0||(b!==null||w!==null)&&(!v||Oe(d,u)?.writable)){if(c===void 0)c=a(()=>{var g=v?It(d[u]):M,E=U(g,i);if(l)ve(E,Qt(f,u));return E}),r.set(u,c);var T=h(c);if(T===M)return!1}return v},set(d,u,c,v){var T=r.get(u),g=u in d;if(n&&u==="length")for(var E=c;E<T.v;E+=1){var C=r.get(E+"");if(C!==void 0)R(C,M);else if(E in d){if(C=a(()=>U(M,i)),r.set(E+"",C),l)ve(C,Qt(f,E))}}if(T===void 0){if(!g||Oe(d,u)?.writable){if(T=a(()=>U(void 0,i)),l)ve(T,Qt(f,u));R(T,It(c)),r.set(u,T)}}else{g=T.v!==M;var B=a(()=>It(c));R(T,B)}var ee=Reflect.getOwnPropertyDescriptor(d,u);if(ee?.set)ee.set.call(v,c);if(!g){if(n&&typeof u==="string"){var ae=r.get("length"),ke=Number(u);if(Number.isInteger(ke)&&ke>=ae.v)R(ae,ke+1)}Jt(o)}return!0},ownKeys(d){h(o);var u=Reflect.ownKeys(d).filter((T)=>{var g=r.get(T);return g===void 0||g.v!==M});for(var[c,v]of r)if(v.v!==M&&!(c in d))u.push(c);return u},setPrototypeOf(){Yi()}})}function Qt(e,t){if(typeof t==="symbol")return`${e}[Symbol(${t.description??""})]`;if(va.test(t))return`${e}.${t}`;return/^\d+$/.test(t)?`${e}[${t}]`:`${e}['${t}']`}function yn(e){try{if(e!==null&&typeof e==="object"&&ze in e)return e[ze]}catch{}return e}var ga=new Set(["copyWithin","fill","pop","push","reverse","shift","sort","splice","unshift"]);function xa(e){return new Proxy(e,{get(t,r,n){var o=Reflect.get(t,r,n);if(!ga.has(r))return o;return function(...i){is();var s=o.apply(this,i);return wn(),s}}})}function fs(){let{prototype:e,__svelte_cleanup:t}=Array;if(t)t();let{indexOf:r,lastIndexOf:n,includes:o}=e;e.indexOf=function(i,s){let a=r.call(this,i,s);if(a===-1){for(let f=s??0;f<this.length;f+=1)if(yn(this[f])===i){sn("array.indexOf(...)");break}}return a},e.lastIndexOf=function(i,s){let a=n.call(this,i,s??this.length-1);if(a===-1){for(let f=0;f<=(s??this.length-1);f+=1)if(yn(this[f])===i){sn("array.lastIndexOf(...)");break}}return a},e.includes=function(i,s){let a=o.call(this,i,s);if(!a){for(let f=0;f<this.length;f+=1)if(yn(this[f])===i){sn("array.includes(...)");break}}return a},Array.__svelte_cleanup=()=>{e.indexOf=r,e.lastIndexOf=n,e.includes=o}}var br,ls,Yr,cs,us;function En(){if(br!==void 0)return;br=window,ls=document,Yr=/Firefox/.test(navigator.userAgent);var e=Element.prototype,t=Node.prototype,r=Text.prototype;if(cs=Oe(t,"firstChild").get,us=Oe(t,"nextSibling").get,Jn(e))e[Sr]=void 0,e[nn]=null,e[to]=void 0,e.__e=void 0;if(Jn(r))r[Nr]=void 0;if(l)e.__svelte_meta=null,fs()}function q(e=""){return document.createTextNode(e)}function ue(e){return cs.call(e)}function ne(e){return us.call(e)}function xe(e,t){if(!x)return ue(e);var r=ue(k);if(r===null)r=k.appendChild(q());else if(t&&r.nodeType!==Ht){var n=q();return r?.before(n),O(n),n}if(t)Br(r);return O(r),r}function $n(e,t=!1){if(!x){var r=ue(e);if(r instanceof Comment&&r.data==="")return ne(r);return r}if(t){if(k?.nodeType!==Ht){var n=q();return k?.before(n),O(n),n}Br(k)}return k}function dt(e,t=!1){if(!x)return ue(e);var r=xe(e,t);return G(e),r}function L(e,t=1,r=!1){let n=x?k:e;var o;while(t--)o=n,n=ne(n);if(!x)return n;if(r){if(n?.nodeType!==Ht){var i=q();if(n===null)o?.after(i);else n.before(i);return O(i),i}Br(n)}return O(n),n}function zr(e){e.textContent=""}function Tn(){if(!X)return!1;if(je!==null)return!1;var e=b.f;return(e&we)!==0}function pt(e,t,r){if(t==null||t===on)return r?document.createElement(e,{is:r}):document.createElement(e);return r?document.createElementNS(t,e,{is:r}):document.createElementNS(t,e)}function Br(e){if(e.nodeValue.length<65536)return;let t=e.nextSibling;while(t!==null&&t.nodeType===Ht)t.remove(),e.nodeValue+=t.nodeValue,t=e.nextSibling}var So=new WeakMap;function ds(e){var t=b;if(t===null)return w.f|=Be,e;if(l&&e instanceof Error&&!So.has(e))So.set(e,ba(e,t));if((t.f&we)===0&&(t.f&xt)===0){if(l&&!t.parent&&e instanceof Error)ps(e);throw e}Re(e,t)}function Re(e,t){if(e===Ie)throw e;if(t!==null&&(t.f&I)!==0)return;while(t!==null){if((t.f&fr)!==0&&(t.f&(I|bt))===0){if((t.f&we)===0)throw e;try{t.b.error(e);return}catch(r){e=r}}t=t.parent}if(l&&e instanceof Error)ps(e);throw e}function ba(e,t){let r=Oe(e,"message");if(r&&!r.configurable)return;var n=Yr?"  ":"\t",o=`
${n}in ${t.fn?.name||"<unknown>"}`,i=t.ctx;while(i!==null)o+=`
${n}in ${i.function?.[Ve].split("/").pop()}`,i=i.p;return{message:e.message+`
${o}
`,stack:e.stack?.split(`
`).filter((s)=>!s.includes("svelte/src/internal")).join(`
`)}}function ps(e){let t=So.get(e);if(t)te(e,"message",{value:t.message}),te(e,"stack",{value:t.stack})}function _s(e){if(b===null){if(w===null)Ii(e);zi()}if(qe)Di(e)}function wa(e,t){var r=t.last;if(r===null)t.last=t.first=e;else r.next=e,e.prev=r,t.last=e}function Et(e,t){var r=b;if(l)while(r!==null&&(r.f&kt)!==0)r=r.parent;if(r!==null&&(r.f&j)!==0)e|=j;var n={ctx:S,deps:null,nodes:null,f:e|N|be,first:null,fn:t,last:null,next:null,parent:r,b:r&&r.b,prev:null,teardown:null,wv:0,ac:null};if(l)n.component_function=et;y?.register_created_effect(n);var o=n;if((e&xt)!==0)if(xr!==null)xr.push(n);else pe.ensure().schedule(n);else if(t!==null){try{ut(n)}catch(s){throw V(n),s}if(o.deps===null&&o.teardown===null&&o.nodes===null&&o.first===o.last&&(o.f&at)===0){if(o=o.first,(e&fe)!==0&&(e&Ne)!==0&&o!==null)o.f|=Ne}}if(o!==null){if(o.parent=r,r!==null)wa(o,r);if(w!==null&&(w.f&Y)!==0&&(e&Se)===0){var i=w;(i.effects??=[]).push(o)}}return n}function Ot(){return w!==null&&!Ee}function de(e){let t=Et(Tt,null);return z(t,D),t.teardown=e,t}function wr(e){if(_s("$effect"),l)te(e,"name",{value:"$effect"});var t=b.f,r=!w&&(t&re)!==0&&S!==null&&!S.i;if(r){var n=S;(n.e??=[]).push(e)}else return co(e)}function co(e){return Et(xt|_i,e)}function Ro(e){pe.ensure();let t=Et(Se|at,e);return()=>{V(t)}}function hs(e){pe.ensure();let t=Et(Se|at,e);return(r={})=>new Promise((n)=>{if(r.outro)_t(t,()=>{V(t),n(void 0)});else V(t),n(void 0)})}function Ji(e){return Et(De|at,e)}function _e(e,t=0){return Et(Tt|t,e)}function Ke(e,t=[],r=[],n=[]){mn(n,t,r,(o)=>{Et(Tt,()=>{e(...o.map(h))})})}function Fe(e,t=0){var r=Et(fe|t,e);if(l)r.dev_stack=lt;return r}function Q(e){return Et(re|at,e)}function Co(e){var t=e.teardown;if(t!==null){let r=qe,n=w;No(!0),Z(null);try{t.call(null)}catch(o){Re(o,e.parent)}finally{No(r),Z(n)}}}function Lr(e,t=!1){var r=e.first;e.first=e.last=null;while(r!==null){let o=r.ac;if(o!==null)Me(()=>{o.abort(wt)});var n=r.next;if((r.f&Se)!==0)r.parent=null;else V(r,t);r=n}}function ms(e){var t=e.first;while(t!==null){var r=t.next;if((t.f&re)===0)V(t);t=r}}function V(e,t=!0){var r=!1;if((t||(e.f&eo)!==0)&&e.nodes!==null&&e.nodes.end!==null)vs(e.nodes.start,e.nodes.end),r=!0;e.f|=bt,Lr(e,t&&!r),Wt(e,0);var n=e.nodes&&e.nodes.t;if(n!==null)for(let i of n)i.stop();Co(e),e.f^=bt,e.f|=I;var o=e.parent;if(o!==null&&o.first!==null)$o(e);if(l)e.component_function=null;e.next=e.prev=e.teardown=e.ctx=e.deps=e.fn=e.nodes=e.ac=e.b=null}function vs(e,t){while(e!==null){var r=e===t?null:ne(e);e.remove(),e=r}}function $o(e){var{parent:t,prev:r,next:n}=e;if(r!==null)r.next=n;if(n!==null)n.prev=r;if(t!==null){if(t.first===e)t.first=n;if(t.last===e)t.last=r}}function _t(e,t,r=!0){var n=[];e.f|=Qr,gs(e,n,!0);var o=()=>{if(r)V(e);if(t)t()},i=n.length;if(i>0){var s=()=>--i||o();for(var a of n)a.out(s)}else o()}function gs(e,t,r){if((e.f&j)!==0)return;e.f^=j;var n=e.nodes&&e.nodes.t;if(n!==null){for(let a of n)if(a.is_global||r)t.push(a)}var o=e.first;while(o!==null){var i=o.next;if((o.f&Se)===0){var s=(o.f&Ne)!==0||(o.f&re)!==0&&(e.f&fe)!==0;gs(o,t,s?r:!1)}o=i}}function yr(e){e.f&=~Qr,xs(e,!0)}function xs(e,t){if((e.f&Qr)!==0)return;if((e.f&j)===0)return;if(e.f^=j,(e.f&D)===0)z(e,N),pe.ensure().schedule(e);var r=e.first;while(r!==null){var n=r.next,o=(r.f&Ne)!==0||(r.f&re)!==0;xs(r,o?t:!1),r=n}var i=e.nodes&&e.nodes.t;if(i!==null){for(let s of i)if(s.is_global||t)s.in()}}function Er(e,t){if(!e.nodes)return;var r=e.nodes.start,n=e.nodes.end;while(r!==null){var o=r===n?null:ne(r);t.append(r),r=o}}var bs=null;var qe=!1;function No(e){qe=e}var w=null,Ee=!1;function Z(e){w=e}var b=null;function J(e){b=e}var rt=null;function xn(e){if(w!==null&&(!X&&(w.f&lr)!==0||(w.f&Y)!==0))(rt??=new Set).add(e)}var oe=null,ye=0,Le=null;function as(e){Le=e}var ys=1,er=0,Mt=er;function Ao(e){Mt=e}function vr(){return++ys}function zt(e){var t=e.f;if((t&N)!==0)return!0;if((t&H)!==0){var r=e.deps,n=r.length;for(var o=0;o<n;o++){var i=r[o];if(zt(i))go(i);if(i.wv>e.wv)return!0}if((t&be)!==0&&ie===null)z(e,D)}return!1}function Es(e,t,r=!0){var n=e.reactions;if(n===null)return;if(!X&&rt!==null&&rt.has(e))return;for(var o=0;o<n.length;o++){var i=n[o];if((i.f&Y)!==0)Es(i,t,!1);else if(t===i){if(r)z(i,N);else if((i.f&D)!==0)z(i,H);Pr(i)}}}function gn(e){var t=oe,r=ye,n=Le,o=w,i=rt,s=S,a=Ee,f=Mt,_=e.f;if(oe=null,ye=0,Le=null,w=(_&(re|Se))===0?e:null,rt=null,yt(e.ctx),Ee=!1,Mt=++er,e.ac!==null)Me(()=>{e.ac.abort(wt)}),e.ac=null;try{e.f|=lr;var p=e.fn,d=p();e.f|=we;var u=ws(e);if(ct()&&Le!==null&&!Ee&&u!==null&&(e.f&(Y|H|N))===0)for(var c=0;c<Le.length;c++)Es(Le[c],e);if(o!==null&&o!==e){if(er++,o.deps!==null)for(let v=0;v<r;v+=1)o.deps[v].rv=er;if(t!==null)for(let v of t)v.rv=er;if(Le!==null)if(n===null)n=Le;else n.push(...Le)}if((e.f&Be)!==0)e.f^=Be;return d}catch(v){return ws(e),ds(v)}finally{e.f^=lr,oe=t,ye=r,Le=n,w=o,rt=i,yt(s),Ee=a,Mt=f}}function ws(e){var t=e.deps,r=y?.is_fork;if(oe!==null){var n;if(!r)Wt(e,ye);if(t!==null&&ye>0){t.length=ye+oe.length;for(n=0;n<oe.length;n++)t[ye+n]=oe[n]}else e.deps=t=oe;if(Ot()&&(e.f&be)!==0)for(n=ye;n<t.length;n++)(t[n].reactions??=[]).push(e)}else if(!r&&t!==null&&ye<t.length)Wt(e,ye),t.length=ye;return t}function ya(e,t){let r=t.reactions;if(r!==null){var n=di.call(r,e);if(n!==-1){var o=r.length-1;if(o===0)r=t.reactions=null;else r[n]=r[o],r.pop()}}if(r===null&&(t.f&Y)!==0&&(oe===null||!gt.call(oe,t))){var i=t;if((i.f&be)!==0)i.f^=be;if(i.v!==M)dr(i);if(i.ac!==null)Me(()=>{i.ac.abort(wt),i.ac=null,z(i,N)});Zi(i),Wt(i,0)}}function Wt(e,t){var r=e.deps;if(r===null)return;for(var n=t;n<r.length;n++)ya(e,r[n])}function ut(e){var t=e.f;if((t&I)!==0)return;z(e,D);var r=b;if(b=e,l){var n=et;dn(e.component_function);var o=lt;ur(e.dev_stack??lt)}try{if((t&(fe|Bt))!==0)ms(e);else Lr(e);Co(e);var i=gn(e);if(e.teardown=typeof i==="function"?i:null,e.wv=ys,l&&Je&&(e.f&N)!==0&&e.deps!==null){for(var s of e.deps)if(s.set_during_effect)s.wv=vr(),s.set_during_effect=!1}}finally{if(b=r,l)dn(n),ur(o)}}async function Hr(){if(X)return new Promise((e)=>{requestAnimationFrame(()=>e()),setTimeout(()=>e())});await Promise.resolve(),Dt()}function h(e){var t=e.f,r=(t&Y)!==0;if(bs?.add(e),w!==null&&!Ee){var n=b!==null&&(b.f&I)!==0;if(!n&&(rt===null||!rt.has(e))){var o=w.deps;if((w.f&lr)!==0){if(e.rv<er)if(e.rv=er,oe===null&&o!==null&&o[ye]===e)ye++;else if(oe===null)oe=[e];else oe.push(e)}else{if(w.deps??=[],!gt.call(w.deps,e))w.deps.push(e);var i=e.reactions;if(i===null)e.reactions=[w];else if(!gt.call(i,w))i.push(w)}}}if(l){if(!Ee&&Ce&&y===null&&Ue===null&&!Ce.warned&&(Ce.effect.f&lr)===0&&!Ce.effect_deps.has(e)){Ce.warned=!0,yi(e.label);var s=Qe("traced at");if(s)console.warn(s)}if(Ir.delete(e),Je&&!Ee&&Cr!==null&&w!==null&&Cr.reaction===w){if(e.trace)e.trace();else if(s=Qe("traced at"),s){var a=Cr.entries.get(e);if(a===void 0)a={traces:[]},Cr.entries.set(e,a);var f=a.traces[a.traces.length-1];if(s.stack!==f?.stack)a.traces.push(s)}}}if(qe&&Ge.has(e))return Ge.get(e);if(r){var _=e;if(qe){var p=_.v;if((_.f&D)===0&&_.reactions!==null||Ts(_))p=Mr(_);return Ge.set(_,p),p}var d=(_.f&be)===0&&!Ee&&w!==null&&(w.f&be)!==0,u=(_.f&we)===0;if(zt(_)){if(d)_.f|=be;go(_)}if(d&&!u)xo(_),$s(_)}if(ie?.has(e))return ie.get(e);if((e.f&Be)!==0)throw e.v;return e.v}function $s(e){if(e.f|=be,e.deps===null)return;for(let r of e.deps){var t=r.reactions;if(t===null)r.reactions=[e];else if(!gt.call(t,e))t.push(e);if((r.f&Y)!==0&&(r.f&be)===0)xo(r),$s(r)}}function Ts(e){if(e.v===M)return!0;if(e.deps===null)return!1;for(let t of e.deps){if(Ge.has(t))return!0;if((t.f&Y)!==0&&Ts(t))return!0}return!1}function P(e){var t=Ee;try{return Ee=!0,e()}finally{Ee=t}}var $a=["allowfullscreen","async","autofocus","autoplay","checked","controls","default","disabled","formnovalidate","indeterminate","inert","ismap","loop","multiple","muted","nomodule","novalidate","open","playsinline","readonly","required","reversed","seamless","selected","webkitdirectory","defer","disablepictureinpicture","disableremoteplayback"];var fd=[...$a,"formNoValidate","isMap","noModule","playsInline","readOnly","value","volume","defaultValue","defaultChecked","srcObject","noValidate","allowFullscreen","disablePictureInPicture","disableRemotePlayback"];var Ta=["touchstart","touchmove"];function ks(e){return Ta.includes(e)}var ka=["$state","$state.raw","$derived","$derived.by"],ld=[...ka,"$state.eager","$state.snapshot","$props","$props.id","$bindable","$effect","$effect.pre","$effect.tracking","$effect.root","$effect.pending","$inspect","$inspect().with","$inspect.trace","$host"];var $r=Symbol("events"),zo=new Set,kn=new Set;function Io(e){if(!x)return;e.removeAttribute("onload"),e.removeAttribute("onerror");let t=e.__e;if(t!==void 0)e.__e=void 0,queueMicrotask(()=>{if(e.isConnected)e.dispatchEvent(t)})}function Ns(e,t,r,n={}){function o(i){if(!n.capture)Sn.call(t,i);if(!i.cancelBubble)return Me(()=>r?.call(this,i))}if(e.startsWith("pointer")||e.startsWith("touch")||e==="wheel")o.__removed=!1,K(()=>{if(!o.__removed)t.addEventListener(e,o,n)});else t.addEventListener(e,o,n);return o}function ht(e,t,r,n,o){var i={capture:n,passive:o},s=Ns(e,t,r,i);if(t===document.body||t===window||t===document||t instanceof HTMLMediaElement)de(()=>{s.__removed=!0,t.removeEventListener(e,s,i)})}function $e(e,t,r){(t[$r]??={})[e]=r}function An(e){for(var t=0;t<e.length;t++)zo.add(e[t]);for(var r of kn)r(e)}var Oo=null,Do=!1;function Sn(e){var t=this,r=t.ownerDocument,n=e.type,o=e.composedPath?.()||[],i=o[0]||e.target;if(Oo=e,!Do)Do=!0,setTimeout(()=>{Do=!1,Oo=null});var s=0,a=Oo===e&&e[$r];if(a){var f=o.indexOf(a);if(f!==-1&&(t===document||t===window)){e[$r]=t;return}var _=o.indexOf(t);if(_===-1)return;if(f<=_)s=f}if(i=o[s]||e.target,i===t)return;te(e,"currentTarget",{configurable:!0,get(){return i||r}});var p=w,d=b;Z(null),J(null);try{var u,c=[];while(i!==null){if(i===t)break;try{var v=i[$r]?.[n];if(v!=null&&(!i.disabled||e.target===i))v.call(i,e)}catch(T){if(u)c.push(T);else u=T}if(e.cancelBubble)break;s++,i=s<o.length?o[s]:null}if(u){for(let T of c)queueMicrotask(()=>{throw T});throw u}}finally{e[$r]=t,delete e.currentTarget,Z(p),J(d)}}var Sa=globalThis?.window?.trustedTypes&&globalThis.window.trustedTypes.createPolicy("svelte-trusted-html",{createHTML:(e)=>e});function Rs(e){return Sa?.createHTML(e)??e}function Mo(e){var t=pt("template");return t.innerHTML=Rs(e.replaceAll("<!>","<!---->")),t.content}function Pe(e,t){var r=b;if(r.nodes===null)r.nodes={start:e,end:t,a:null,t:null}}function mt(e,t){var r=(t&bi)!==0,n=(t&wi)!==0,o,i=!e.startsWith("<!>");return()=>{if(x)return Pe(k,null),k;if(o===void 0){if(o=Mo(i?e:"<!>"+e),!r)o=ue(o)}var s=n||Yr?document.importNode(o,!0):o.cloneNode(!0);if(r){var a=ue(s),f=s.lastChild;Pe(a,f)}else Pe(s,s);return s}}function Vr(e=""){if(!x){var t=q(e+"");return Pe(t,t),t}var r=k;if(r.nodeType!==Ht)r.before(r=q()),O(r);else Br(r);return Pe(r,r),r}function Fo(){if(x)return Pe(k,null),k;var e=document.createDocumentFragment(),t=document.createComment(""),r=q();return e.append(t,r),Pe(t,r),e}function se(e,t){if(x){var r=b;if((r.f&we)===0||r.nodes.end===null)r.nodes.end=k;me();return}if(e===null)return;e.before(t)}function Cs(e){let t=0,r=ge(0),n;if(l)ve(r,"createSubscriber version");return()=>{if(Ot())h(r),_e(()=>{if(t===0)n=P(()=>e(()=>Jt(r)));return t+=1,()=>{K(()=>{if(t-=1,t===0)n?.(),n=void 0,Jt(r)})}})}}var Ca=Ne|at;function Po(e,t,r,n){new Os(e,t,r,n)}class Os{parent;is_pending=!1;transform_error;#e;#t=x?k:null;#r;#a;#o;#n=null;#s=null;#i=null;#f=null;#p=0;#l=0;#h=!1;#g=new Map;#c=null;#m=Cs(()=>{if(this.#c=ge(this.#p),l)ve(this.#c,"$effect.pending()");return()=>{this.#c=null}});constructor(e,t,r,n){if(this.#e=e,this.#r=t,this.#a=(o)=>{var i=b;i.b=this,i.f|=fr,r(o)},this.parent=b.b,this.transform_error=n??this.parent?.transform_error??((o)=>o),this.#o=Fe(()=>{if(x){let o=qt(this.#t);me();let i=o===St;if(o.startsWith(io)){let a=JSON.parse(o.slice(io.length));this.#w(a)}else if(i)this.#_();else this.#x()}else this.#b()},Ca),x)this.#e=k}#x(){try{this.#n=Q(()=>this.#a(this.#e))}catch(e){this.error(e)}}#w(e){let t=this.#r.failed,{reset:r,invoke_onerror:n}=this.#v(e);if(K(n),!t)return;this.#i=Q(()=>{t(this.#e,()=>e,()=>r)})}#v(e){var t=!1,r=!1;let n=()=>{if(this.#u())return;if(t){Ai();return}if(t=!0,r)Hi();if(this.#i!==null)_t(this.#i,()=>{this.#i=null});this.#y(()=>{this.#b()})};return{reset:n,invoke_onerror:()=>{if(this.#u())return;try{r=!0,this.#r.onerror?.(e,n),r=!1}catch(i){Re(i,this.#o&&this.#o.parent)}}}}#u(){return(this.#o.f&(I|bt))!==0}#_(){let e=this.#r.pending;if(!e)return;this.is_pending=!0,this.#s=Q(()=>e(this.#e)),K(()=>{if(this.#u())return;var t=this.#f=document.createDocumentFragment(),r=q(),n=!1;if(t.append(r),this.#n=this.#y(()=>{try{return Q(()=>this.#a(r))}catch(o){try{this.error(o),n=!0}catch(i){Re(i,this.#o.parent)}return null}}),this.#n===null){if(this.#f=null,n)this.#d(y);return}if(this.#l===0)this.#e.before(t),this.#f=null,_t(this.#s,()=>{this.#s=null}),this.#d(y)})}#b(){try{if(this.is_pending=this.has_pending_snippet(),this.#l=0,this.#p=0,this.#n=Q(()=>{this.#a(this.#e)}),this.#l>0){var e=this.#f=document.createDocumentFragment();Er(this.#n,e);let t=this.#r.pending;this.#s=Q(()=>t(this.#e))}else this.#d(y)}catch(t){this.error(t)}}#d(e){this.is_pending=!1,e.transfer_reactions(this.#g)}defer_effect(e){_n(e,this.#g)}is_rendered(){return!this.is_pending&&(!this.parent||this.parent.is_rendered())}has_pending_snippet(){return!!this.#r.pending}#y(e){var t=b,r=w,n=S;J(this.#o),Z(this.#o),yt(this.#o.ctx);try{return pe.ensure(),e()}finally{J(t),Z(r),yt(n)}}#E(e,t){if(!this.has_pending_snippet()){if(this.parent)this.parent.#E(e,t);return}if(this.#l+=e,this.#l===0){if(this.#d(t),this.#s)_t(this.#s,()=>{this.#s=null});if(this.#f)this.#e.before(this.#f),this.#f=null}}update_pending_count(e,t){if(this.#E(e,t),this.#p+=e,!this.#c||this.#h)return;this.#h=!0,K(()=>{if(this.#h=!1,this.#c)tt(this.#c,this.#p)})}get_effect_pending(){return this.#m(),h(this.#c)}error(e){if(e===Ie)throw e;if(!this.#r.onerror&&!this.#r.failed)throw e;if(y?.is_fork){if(this.#n)y.skip_effect(this.#n);if(this.#s)y.skip_effect(this.#s);if(this.#i)y.skip_effect(this.#i);y.oncommit(()=>{if(!this.#u())this.#$(e)})}else this.#$(e)}#$(e){if(this.#n)V(this.#n),this.#n=null;if(this.#s)V(this.#s),this.#s=null;if(this.#i)V(this.#i),this.#i=null;if(x)O(this.#t),Rr(),O(ft());let t=this.#r.failed,r=(n)=>{if(this.#u())return;let{reset:o,invoke_onerror:i}=this.#v(n);if(i(),t&&!this.#u())this.#i=this.#y(()=>{try{return Q(()=>{var s=b;s.b=this,s.f|=fr,t(this.#e,()=>n,()=>o)})}catch(s){return Re(s,this.#o.parent),null}})};K(()=>{if(this.#u())return;var n;try{n=this.transform_error(e)}catch(o){Re(o,this.#o&&this.#o.parent);return}if(n!==null&&typeof n==="object"&&typeof n.then==="function")n.then(r,(o)=>Re(o,this.#o&&this.#o.parent));else r(n)})}}var Yo=!0;function Te(e,t){var r=t==null?"":typeof t==="object"?`${t}`:t;if(r!==(e[Nr]??=e.nodeValue))e[Nr]=r,e.nodeValue=`${r}`}function Tr(e,t){return Ds(e,t)}function Ho(e,t){En(),t.intro=t.intro??!1;let r=t.target,n=x,o=k;try{var i=ue(r);while(i&&(i.nodeType!==He||i.data!==cr))i=ne(i);if(!i)throw Ie;W(!0),O(i);let s=Ds(e,{...t,anchor:i});return W(!1),s}catch(s){if(s instanceof Error&&s.message.split(`
`).some((a)=>a.startsWith("https://svelte.dev/e/")))throw s;if(s!==Ie)console.warn("Failed to hydrate: ",s);if(t.recover===!1)Li();return En(),zr(r),W(!1),Tr(e,t)}finally{W(n),O(o)}}var Nn=new Map;function Ds(e,{target:t,anchor:r,props:n={},events:o,context:i,intro:s=!0,transformError:a}){En();var f=void 0,_=hs(()=>{var p=r??t.appendChild(q());Po(p,{pending:()=>{}},(c)=>{Or({});var v=S;if(i)v.c=i;if(o)n.$$events=o;if(x)Pe(c,null);if(Yo=s,f=e(c,n)||pn(),Yo=!0,x){if(b.nodes.end=k,k===null||k.nodeType!==He||k.data!==Vt)throw Ut(),Ie}Dr()},a);var d=new Set,u=(c)=>{for(var v=0;v<c.length;v++){var T=c[v];if(d.has(T))continue;d.add(T);var g=ks(T);for(let B of[t,document]){var E=Nn.get(B);if(E===void 0)E=new Map,Nn.set(B,E);var C=E.get(T);if(C===void 0)B.addEventListener(T,Sn,{passive:g}),E.set(T,1);else E.set(T,C+1)}}};return u(sr(zo)),kn.add(u),()=>{for(var c of d)for(let g of[t,document]){var v=Nn.get(g),T=v.get(c);if(--T==0){if(g.removeEventListener(c,Sn),v.delete(c),v.size===0)Nn.delete(g)}else v.set(c,T)}if(kn.delete(u),p!==r)p.parentNode?.removeChild(p)}});return Bo.set(f,_),f}var Bo=new WeakMap;function Vo(e,t){let r=Bo.get(e);if(r)return Bo.delete(e),r(t);if(l)ki();return Promise.resolve()}class Lt{anchor;#e=new Map;#t=new Map;#r=new Map;#a=new Set;#o=!0;constructor(e,t=!0){this.anchor=e,this.#o=t}#n=(e)=>{if(!this.#e.has(e))return;var t=this.#e.get(e),r=this.#t.get(t);if(r)yr(r),this.#a.delete(t);else{var n=this.#r.get(t);if(n){if(yr(n.effect),this.#t.set(t,n.effect),this.#r.delete(t),l)n.fragment.lastChild[ro]=this.anchor;n.fragment.lastChild.remove(),this.anchor.before(n.fragment),r=n.effect}}for(let[o,i]of this.#e){if(this.#e.delete(o),o===e)break;let s=this.#r.get(i);if(s)V(s.effect),this.#r.delete(i)}for(let[o,i]of this.#t){if(o===t||this.#a.has(o))continue;let s=()=>{if(Array.from(this.#e.values()).includes(o)){var f=document.createDocumentFragment();Er(i,f),f.append(q()),this.#r.set(o,{effect:i,fragment:f})}else V(i);this.#a.delete(o),this.#t.delete(o)};if(this.#o||!r)this.#a.add(o),_t(i,s,!1);else s()}};#s=(e)=>{this.#e.delete(e);let t=Array.from(this.#e.values());for(let[r,n]of this.#r)if(!t.includes(r))V(n.effect),this.#r.delete(r)};ensure(e,t){var r=y,n=Tn();if(t&&!this.#t.has(e)&&!this.#r.has(e))if(n){var o=document.createDocumentFragment(),i=q();o.append(i),this.#r.set(e,{effect:Q(()=>t(i)),fragment:o})}else this.#t.set(e,Q(()=>t(this.anchor)));if(this.#e.set(r,e),n){for(let[s,a]of this.#t)if(s===e)r.unskip_effect(a);else r.skip_effect(a);for(let[s,a]of this.#r)if(s===e)r.unskip_effect(a.effect);else r.skip_effect(a.effect);r.oncommit(this.#n),r.ondiscard(this.#s)}else{if(x)this.anchor=k;this.#n(r)}}}function Ft(e,t,r=!1){var n;if(x)n=k,me();var o=new Lt(e),i=r?Ne:0;function s(a,f){if(x){var _=qt(n);if(a!==parseInt(_.substring(1))){var p=ft();O(p),o.anchor=p,W(!1),o.ensure(a,f),W(!0);return}}o.ensure(a,f)}Fe(()=>{var a=!1;if(t((f,_=0)=>{a=!0,s(_,f)}),!a)s(-1,null)},i)}var L_=Symbol("NaN");function Da(e,t,r){var n=[],o=t.length,i,s=t.length;for(var a=0;a<o;a++){let d=t[a];_t(d,()=>{if(i){if(i.pending.delete(d),i.done.add(d),i.pending.size===0){var u=e.outrogroups;if(Uo(e,sr(i.done)),u.delete(i),u.size===0)e.outrogroups=null}}else s-=1},!1)}if(s===0){var f=n.length===0&&r!==null&&e.pending.size===0;if(f){var _=r,p=_.parentNode;zr(p),p.append(_),e.items.clear()}Uo(e,t,!f)}else i={pending:new Set(t),done:new Set},(e.outrogroups??=new Set).add(i)}function Uo(e,t,r=!0){var n;if(e.pending.size>0){n=new Set;for(let s of e.pending.values())for(let a of s)n.add(e.items.get(a).e)}for(var o=0;o<t.length;o++){var i=t[o];if(n?.has(i)){i.f|=Ze;let s=document.createDocumentFragment();Er(i,s)}else V(t[o],r)}}var zs;function Rn(e,t,r,n,o,i=null){var s=e,a=new Map,f=(t&oo)!==0;if(f){var _=e;s=x?O(ue(_)):_.appendChild(q())}if(x)me();var p=null,d=hr(()=>{var C=r();return vt(C)?C:C==null?[]:sr(C)});if(l)ve(d,"{#each ...}");var u=new Map,c=!0;function v(C){if((E.effect.f&I)!==0)return;E.pending.delete(C);var B=h(d);if(E.fallback=p,za(E,B,s,t,n),p!==null)if(B.length===0)if((p.f&Ze)===0)yr(p);else p.f^=Ze,qr(p,null,s);else _t(p,()=>{p=null})}function T(C){E.pending.delete(C)}var g=Fe(()=>{var C=h(d),B=C.length;let ee=!1;if(x){var ae=qt(s)===St;if(ae!==(B===0))s=ft(),O(s),W(!1),ee=!0}var ke=new Set,he=y,$t=Tn();for(var Ye=0;Ye<B;Ye+=1){if(x&&k.nodeType===He&&k.data===Vt)s=k,ee=!0,W(!1);var Ae=C[Ye],ot=n(Ae,Ye);if(l){var nr=n(Ae,Ye);if(ot!==nr)Oi(String(Ye),String(ot),String(nr))}var We=c?null:a.get(ot);if(We){if(We.v)tt(We.v,Ae);if(We.i)tt(We.i,Ye);if($t)he.unskip_effect(We.e)}else{if(We=Ia(a,c?s:zs??=q(),Ae,ot,Ye,o,t,r),!c)We.e.f|=Ze;a.set(ot,We)}ke.add(ot)}if(B===0&&i&&!p)if(c)p=Q(()=>i(s));else p=Q(()=>i(zs??=q())),p.f|=Ze;if(B>ke.size)if(l)Ma(C,n);else lo("","","");if(x&&B>0)O(ft());if(!c)if(u.set(he,ke),$t){for(let[On,Dn]of a)if(!ke.has(On))he.skip_effect(Dn.e);he.oncommit(v),he.ondiscard(T)}else v(he);if(ee)W(!0);h(d)}),E={effect:g,flags:t,items:a,pending:u,outrogroups:null,fallback:p};if(c=!1,x)s=k}function Ur(e){while(e!==null&&(e.f&re)===0)e=e.next;return e}function za(e,t,r,n,o){var i=(n&gi)!==0,s=t.length,a=e.items,f=Ur(e.effect.first),_,p=null,d,u=[],c=[],v,T,g,E;if(i){for(E=0;E<s;E+=1)if(v=t[E],T=o(v,E),g=a.get(T).e,(g.f&Ze)===0)g.nodes?.a?.measure(),(d??=new Set).add(g)}for(E=0;E<s;E+=1){if(v=t[E],T=o(v,E),g=a.get(T).e,e.outrogroups!==null)for(let Ae of e.outrogroups)Ae.pending.delete(g),Ae.done.delete(g);if((g.f&j)!==0){if(yr(g),i)g.nodes?.a?.unfix(),(d??=new Set).delete(g)}if((g.f&Ze)!==0)if(g.f^=Ze,g===f)qr(g,null,r);else{var C=p?p.next:f;if(g===e.effect.last)e.effect.last=g.prev;if(g.prev)g.prev.next=g.next;if(g.next)g.next.prev=g.prev;Pt(e,p,g),Pt(e,g,C),qr(g,C,r),p=g,u=[],c=[],f=Ur(p.next);continue}if(g!==f){if(_!==void 0&&_.has(g)){if(u.length<c.length){var B=c[0],ee;p=B.prev;var ae=u[0],ke=u[u.length-1];for(ee=0;ee<u.length;ee+=1)qr(u[ee],B,r);for(ee=0;ee<c.length;ee+=1)_.delete(c[ee]);Pt(e,ae.prev,ke.next),Pt(e,p,ae),Pt(e,ke,B),f=B,p=ke,E-=1,u=[],c=[]}else _.delete(g),qr(g,f,r),Pt(e,g.prev,g.next),Pt(e,g,p===null?e.effect.first:p.next),Pt(e,p,g),p=g;continue}u=[],c=[];while(f!==null&&f!==g)(_??=new Set).add(f),c.push(f),f=Ur(f.next);if(f===null)continue}if((g.f&Ze)===0)u.push(g);p=g,f=Ur(g.next)}if(e.outrogroups!==null){for(let Ae of e.outrogroups)if(Ae.pending.size===0)Uo(e,sr(Ae.done)),e.outrogroups?.delete(Ae);if(e.outrogroups.size===0)e.outrogroups=null}if(f!==null||_!==void 0){var he=[];if(_!==void 0){for(g of _)if((g.f&j)===0)he.push(g)}while(f!==null){if((f.f&j)===0&&f!==e.fallback)he.push(f);f=Ur(f.next)}var $t=he.length;if($t>0){var Ye=(n&oo)!==0&&s===0?r:null;if(i){for(E=0;E<$t;E+=1)he[E].nodes?.a?.measure();for(E=0;E<$t;E+=1)he[E].nodes?.a?.fix()}Da(e,he,Ye)}}if(i)K(()=>{if(d===void 0)return;for(g of d)g.nodes?.a?.apply()})}function Ia(e,t,r,n,o,i,s,a){var f=(s&mi)!==0?(s&xi)===0?Zt(r,!1,!1):ge(r):null,_=(s&vi)!==0?ge(o):null;if(l&&f)f.trace=()=>{a()[_?.v??o]};return{v:f,i:_,e:Q(()=>(i(t,f??r,_??o,a),()=>{e.delete(n)}))}}function qr(e,t,r){if(!e.nodes)return;var n=e.nodes.start,o=e.nodes.end,i=t&&(t.f&Ze)===0?t.nodes.start:r;while(n!==null){var s=ne(n);if(i.before(n),n===o)return;n=s}}function Pt(e,t,r){if(t===null)e.effect.first=r;else t.next=r;if(r===null)e.effect.last=t;else r.prev=t}function Ma(e,t){let r=new Map,n=e.length;for(let o=0;o<n;o++){let i=t(e[o],o);if(r.has(i)){let s=String(r.get(i)),a=String(o),f=String(i);if(f.startsWith("[object "))f=null;lo(s,a,f)}r.set(i,o)}}var Ms=[...` 	
\r\f \v\uFEFF`];function Ls(e,t,r){var n=e==null?"":""+e;if(t)n=n?n+" "+t:t;if(r){for(var o of Object.keys(r))if(r[o])n=n?n+" "+o:o;else if(n.length){var i=o.length,s=0;while((s=n.indexOf(o,s))>=0){var a=s+i;if((s===0||Ms.includes(n[s-1]))&&(a===n.length||Ms.includes(n[a])))n=(s===0?"":n.substring(0,s))+n.substring(a+1);else s=a}}}return n===""?null:n}function kr(e,t,r,n,o,i){var s=e[Sr];if(x||s!==r||s===void 0){var a=Ls(r,n,i);if(!x||a!==e.getAttribute("class"))if(a==null)e.removeAttribute("class");else if(t)e.className=a;else e.setAttribute("class",a);e[Sr]=r}else if(i&&o!==i)for(var f in i){var _=!!i[f];if(o==null||_!==!!o[f])e.classList.toggle(f,_)}return i}var Xa=Symbol("class"),Za=Symbol("style"),Ja=Symbol("is custom element"),Qa=Symbol("is html"),ef=no?"link":"LINK";function tr(e){if(!x)return;var t=!1,r=()=>{if(t)return;if(t=!0,e.hasAttribute("value")){var n=e.value;jr(e,"value",null),e.value=n}if(e.hasAttribute("checked")){var o=e.checked;jr(e,"checked",null),e.checked=o}};e[At]=r,K(r),hn()}function jr(e,t,r,n){var o=tf(e);if(x){if(o[t]=e.getAttribute(t),t==="src"||t==="srcset"||t==="href"&&e.nodeName===ef){if(!n)nf(e,t,r??"");return}}var i=o[t];if(i===(o[t]=r))return;if(t==="loading")e[hi]=r;if(r==null)e.removeAttribute(t);else if(typeof r!=="string"&&rf(e).has(t))e[t]=r;else if(!(x&&(typeof r==="number"||typeof r==="boolean")&&i===String(r)))e.setAttribute(t,r)}function tf(e){return e[nn]??={[Ja]:e.nodeName.includes("-"),[Qa]:e.namespaceURI===on}}var Fs=new Map;function rf(e){var t=e.getAttribute("is")||e.nodeName,r=Fs.get(t);if(r)return r;Fs.set(t,r=new Set);var n,o=e,i=Element.prototype;while(i!==o){n=Xn(o);for(var s in n)if(n[s].set&&s!=="innerHTML"&&s!=="textContent"&&s!=="innerText")r.add(s);o=ar(o)}return r}function nf(e,t,r){if(!l)return;if(t==="srcset"&&of(e,r))return;if(qo(e.getAttribute(t)??"",r))return;Ti(t,e.outerHTML.replace(e.innerHTML,e.innerHTML&&"..."),String(r))}function qo(e,t){if(e===t)return!0;return new URL(e,document.baseURI).href===new URL(t,document.baseURI).href}function Ps(e){return e.split(",").map((t)=>t.trim().split(" ").filter(Boolean))}function of(e,t){var r=Ps(e.srcset),n=Ps(t);return n.length===r.length&&n.every(([o,i],s)=>i===r[s][1]&&(qo(r[s][0],o)||qo(o,r[s][0])))}function rr(e,t,r=t){var n=new WeakSet;if(uo(e,"input",async(o)=>{if(l&&e.type==="checkbox")fo();var i=o?e.defaultValue:e.value;if(i=jo(e)?Go(i):i,r(i),y!==null)n.add(y);if(await Hr(),i!==(i=t())){var{selectionStart:s,selectionEnd:a}=e,f=e.value.length;if(e.value=i??"",a!==null){var _=e.value.length;if(s===a&&a===f&&_>f)e.selectionStart=_,e.selectionEnd=_;else e.selectionStart=s,e.selectionEnd=Math.min(a,_)}}}),x&&e.defaultValue!==e.value||P(t)==null&&e.value){if(r(jo(e)?Go(e.value):e.value),y!==null)n.add(y)}_e(()=>{if(l&&e.type==="checkbox")fo();var o=t();if(e===document.activeElement){var i=X?Ue:y;if(n.has(i))return}if(jo(e)&&o===Go(e.value))return;if(e.type==="date"&&!o&&!e.value)return;if(o!==e.value)e.value=o??""})}function jo(e){var t=e.type;return t==="number"||t==="range"}function Go(e){return e===""?null:+e}var tx=Symbol("unmounted");function Bs(e){return new Hs(e)}class Hs{#e;#t;constructor(e){var t=new Map,r=(o,i)=>{var s=Zt(i,!1,!1);return t.set(o,s),s};let n=new Proxy({...e.props||{},$$events:{}},{get(o,i){return h(t.get(i)??r(i,Reflect.get(o,i)))},has(o,i){if(i===tn)return!0;return h(t.get(i)??r(i,Reflect.get(o,i))),Reflect.has(o,i)},set(o,i,s){return R(t.get(i)??r(i,s),s),Reflect.set(o,i,s)}});if(this.#t=(e.hydrate?Ho:Tr)(e.component,{target:e.target,anchor:e.anchor,props:n,context:e.context,intro:e.intro??!1,recover:e.recover,transformError:e.transformError}),!X&&(!e?.props?.$$host||e.sync===!1))Dt();this.#e=n.$$events;for(let o of Object.keys(this.#t)){if(o==="$set"||o==="$destroy"||o==="$on")continue;te(this,o,{get(){return this.#t[o]},set(i){this.#t[o]=i},enumerable:!0})}this.#t.$set=(o)=>{Object.assign(n,o)},this.#t.$destroy=()=>{Vo(this.#t)}}$set(e){this.#t.$set(e)}$on(e,t){this.#e[e]=this.#e[e]||[];let r=(...n)=>t.call(this,...n);return this.#e[e].push(r),()=>{this.#e[e]=this.#e[e].filter((n)=>n!==r)}}$destroy(){this.#t.$destroy()}}var hf;if(typeof HTMLElement==="function")hf=class extends HTMLElement{$$ctor;$$s;$$c;$$cn=!1;$$d={};$$r=!1;$$p_d={};$$l={};$$l_u=new Map;$$me;$$shadowRoot=null;constructor(e,t,r){super();if(this.$$ctor=e,this.$$s=t,r)this.$$shadowRoot=this.attachShadow(r)}addEventListener(e,t,r){if(this.$$l[e]=this.$$l[e]||[],this.$$l[e].push(t),this.$$c){let n=this.$$c.$on(e,t);this.$$l_u.set(t,n)}super.addEventListener(e,t,r)}removeEventListener(e,t,r){if(super.removeEventListener(e,t,r),this.$$c){let n=this.$$l_u.get(t);if(n)n(),this.$$l_u.delete(t)}}async connectedCallback(){if(this.$$cn=!0,!this.$$c){let e=function(n){return(o)=>{let i=pt("slot");if(n!=="default")i.name=n;se(o,i)}};if(await Promise.resolve(),!this.$$cn||this.$$c)return;let t={},r=mf(this);for(let n of this.$$s)if(n in r)if(n==="default"&&!this.$$d.children)this.$$d.children=e(n),t.default=!0;else t[n]=e(n);for(let n of this.attributes){let o=this.$$g_p(n.name);if(!(o in this.$$d))this.$$d[o]=Wo(o,n.value,this.$$p_d,"toProp")}for(let n in this.$$p_d)if(!(n in this.$$d)&&this[n]!==void 0)this.$$d[n]=this[n],delete this[n];this.$$c=Bs({component:this.$$ctor,target:this.$$shadowRoot||this,props:{...this.$$d,$$slots:t,$$host:this}}),this.$$me=Ro(()=>{_e(()=>{this.$$r=!0;for(let n of Wn(this.$$c)){if(!this.$$p_d[n]?.reflect)continue;this.$$d[n]=this.$$c[n];let o=Wo(n,this.$$d[n],this.$$p_d,"toAttribute");if(o==null)this.removeAttribute(this.$$p_d[n].attribute||n);else this.setAttribute(this.$$p_d[n].attribute||n,o)}this.$$r=!1})});for(let n in this.$$l)for(let o of this.$$l[n]){let i=this.$$c.$on(n,o);this.$$l_u.set(o,i)}this.$$l={}}}attributeChangedCallback(e,t,r){if(this.$$r)return;e=this.$$g_p(e),this.$$d[e]=Wo(e,r,this.$$p_d,"toProp"),this.$$c?.$set({[e]:this.$$d[e]})}disconnectedCallback(){this.$$cn=!1,Promise.resolve().then(()=>{if(!this.$$cn&&this.$$c)this.$$c.$destroy(),this.$$me(),this.$$c=void 0})}$$g_p(e){return Wn(this.$$p_d).find((t)=>this.$$p_d[t].attribute===e||!this.$$p_d[t].attribute&&t.toLowerCase()===e)||e}};function Wo(e,t,r,n){let o=r[e]?.type;if(t=o==="Boolean"&&typeof t!=="boolean"?t!=null:t,!n||!r[e])return t;else if(n==="toAttribute")switch(o){case"Object":case"Array":return t==null?null:JSON.stringify(t);case"Boolean":return t?"":null;case"Number":return t==null?null:t;default:return t}else switch(o){case"Object":case"Array":return t&&JSON.parse(t);case"Boolean":return t;case"Number":return t!=null?+t:t;default:return t}}function mf(e){let t={};return e.childNodes.forEach((r)=>{t[r.slot||"default"]=!0}),t}if(l){let e=function(t){if(!(t in globalThis)){let r;Object.defineProperty(globalThis,t,{configurable:!0,get:()=>{if(r!==void 0)return r;Fi(t)},set:(n)=>{r=n}})}};e("$state"),e("$effect"),e("$derived"),e("$inspect"),e("$props"),e("$bindable")}function Vs(e){if(S===null)ln("onMount");if(Ct&&S.l!==null)bf(S).m.push(e);else wr(()=>{let t=P(e);if(typeof t==="function")return t})}function bf(e){var t=e.l;return t.u??={a:[],b:[],m:[]}}var Us="5";if(typeof window<"u")((window.__svelte??={}).v??=new Set).add(Us);var wf=mt('<span class="svelte-1hsz8dnnzz8ho"> </span>'),yf=mt('<div class="peringatan svelte-1hsz8dnnzz8ho"></div>'),qs=mt('<p class="ket svelte-1hsz8dnnzz8ho"> </p>'),Ef=mt('<img alt=""/> <!>',1),$f=mt('<button class="stop svelte-1hsz8dnnzz8ho"> </button>'),Tf=mt("<button> </button>"),kf=mt('<p class="status svelte-1hsz8dnnzz8ho"> </p>'),Af=mt('<main class="svelte-1hsz8dnnzz8ho"><header class="svelte-1hsz8dnnzz8ho"><h1 class="svelte-1hsz8dnnzz8ho"> </h1> <span></span> <span class="kecil svelte-1hsz8dnnzz8ho"><!></span></header> <!> <section class="kamera svelte-1hsz8dnnzz8ho"><!> <button class="kecil-btn svelte-1hsz8dnnzz8ho"> </button></section> <section class="svelte-1hsz8dnnzz8ho"><h2 class="svelte-1hsz8dnnzz8ho">Gerak <small class="svelte-1hsz8dnnzz8ho">tahan untuk jalan, lepas untuk berhenti</small></h2> <div class="dpad svelte-1hsz8dnnzz8ho"></div> <label class="svelte-1hsz8dnnzz8ho"> <input type="range" min="100" max="255" class="svelte-1hsz8dnnzz8ho"/></label></section> <section class="svelte-1hsz8dnnzz8ho"><h2 class="svelte-1hsz8dnnzz8ho">Kepala</h2> <label class="svelte-1hsz8dnnzz8ho"> <input type="range" min="-60" max="60" class="svelte-1hsz8dnnzz8ho"/></label> <label class="svelte-1hsz8dnnzz8ho"> <input type="range" min="40" max="150" class="svelte-1hsz8dnnzz8ho"/></label> <button class="kecil-btn svelte-1hsz8dnnzz8ho">Lurus ke depan</button></section> <section class="svelte-1hsz8dnnzz8ho"><h2 class="svelte-1hsz8dnnzz8ho">Musik dan suara</h2> <div class="baris svelte-1hsz8dnnzz8ho"><button class="svelte-1hsz8dnnzz8ho">Lagu berikutnya</button> <button class="svelte-1hsz8dnnzz8ho">Radio berikutnya</button> <button class="svelte-1hsz8dnnzz8ho">Diam</button></div> <label class="svelte-1hsz8dnnzz8ho"> <input type="range" min="0" max="21" class="svelte-1hsz8dnnzz8ho"/></label> <form class="svelte-1hsz8dnnzz8ho"><input type="text" placeholder="Suruh Zond bilang..." maxlength="140" class="svelte-1hsz8dnnzz8ho"/> <button type="submit" class="svelte-1hsz8dnnzz8ho">Bilang</button></form></section> <!></main>');function Sf(e,t){Or(t,!0);let r=U(null),n=U(!1),o=U(160),i=U(null),s=U(0),a=U(90),f=U(12),_=!1,p=U(""),d=U(!0),u=U(!1),c=U("");async function v(m,A=1500){let F=new AbortController,it=setTimeout(()=>F.abort(),A);try{return await fetch(m,{signal:F.signal,cache:"no-store"})}catch{return null}finally{clearTimeout(it)}}async function T(){let m=await v("/api/status");if(!m||!m.ok){R(n,!1);return}if(R(n,!0),R(r,await m.json(),!0),!_)R(f,h(r).vol,!0)}let g={maju:[1,1],mundur:[-1,-1],kiri:[-1,1],kanan:[1,-1],majukiri:[0.35,1],majukanan:[1,0.35],mundurkiri:[-0.35,-1],mundurkanan:[-1,-0.35]},E=null,C=!1;function B(){if(!h(i)||C)return;let[m,A]=g[h(i)];C=!0,v(`/api/gerak?l=${Math.round(m*h(o))}&r=${Math.round(A*h(o))}`,600).finally(()=>C=!1)}function ee(m){if(R(i,m,!0),B(),E)clearInterval(E);E=setInterval(B,150)}function ae(){if(E)clearInterval(E);if(E=null,h(i))v("/api/stop");R(i,null)}function ke(m,A){m.currentTarget.setPointerCapture(m.pointerId),ee(A)}let he={ArrowUp:"maju",w:"maju",ArrowDown:"mundur",s:"mundur",ArrowLeft:"kiri",a:"kiri",ArrowRight:"kanan",d:"kanan"};function $t(m){if(m.target.tagName==="INPUT")return;if(m.key===" "){m.preventDefault(),ae(),v("/api/stop");return}let A=he[m.key];if(A&&!m.repeat)m.preventDefault(),ee(A)}function Ye(m){if(he[m.key])ae()}let Ae=0,ot=null;function nr(){let m=Date.now();if(ot)clearTimeout(ot);let A=()=>{Ae=Date.now(),v(`/api/kepala?p=${90-h(s)}&t=${h(a)}`)};if(m-Ae>150)A();else ot=setTimeout(A,150)}function We(){R(s,0),R(a,90),nr()}async function On(){_=!0,await v(`/api/volume?v=${h(f)}`),setTimeout(()=>_=!1,1500)}async function Dn(m){m.preventDefault();let A=h(p).trim();if(!A)return;R(p,""),await v(`/api/bilang?t=${encodeURIComponent(A)}`,3000)}function Gr(m){setTimeout(()=>{if(!h(d)||!h(r)?.kamera){if(h(d))Gr(1000);return}R(c,`http://${h(r).kamera}/capture?res=vga&t=${Date.now()}`)},m)}function Gs(m){let A=[];if(m&1)A.push("Tombol STOP terkunci");if(m&2)A.push("Baterai lemah");if(m&4)A.push("Halangan depan");if(m&8)A.push("Halangan belakang");if(m&16)A.push("Robot miring");return A}Vs(()=>{T();let m=setInterval(T,1000),A=()=>ae(),F=()=>document.hidden&&ae();return window.addEventListener("blur",A),document.addEventListener("visibilitychange",F),()=>{clearInterval(m),window.removeEventListener("blur",A),document.removeEventListener("visibilitychange",F),ae()}}),wr(()=>{if(h(d))Gr(0)});var zn=Af();ht("keydown",br,$t),ht("keyup",br,Ye);var In=xe(zn),Xo=xe(In),Ks=dt(Xo,!0),Zo=L(Xo,2);let Jo;var Qo=L(Zo,2),Ws=xe(Qo);{var Xs=(m)=>{var A=Vr("otak tidak terhubung");se(m,A)},Zs=(m)=>{var A=Vr("badan tidak menjawab");se(m,A)},Js=(m)=>{var A=Vr();Ke((F)=>Te(A,`${F??""} V · depan ${h(r).depan??""} cm · belakang ${h(r).belakang??""} cm`),[()=>h(r).baterai.toFixed(1)]),se(m,A)};Ft(Ws,(m)=>{if(!h(n))m(Xs);else if(!h(r)?.hidup)m(Zs,1);else m(Js,-1)})}G(Qo),G(In);var ei=L(In,2);{var Qs=(m)=>{var A=yf();Rn(A,20,()=>Gs(h(r).flag),(F)=>F,(F,it)=>{var Yt=wf(),or=dt(Yt,!0);Ke(()=>Te(or,it)),se(F,Yt)}),G(A),se(m,A)};Ft(ei,(m)=>{if(h(r)&&h(r).flag)m(Qs)})}var Mn=L(ei,2),ti=xe(Mn);{var ea=(m)=>{var A=Ef(),F=$n(A);let it;var Yt=L(F,2);{var or=(Ar)=>{var Wr=qs(),Kn=dt(Wr);Ke(()=>Te(Kn,`Kamera tidak terhubung (${h(r).kamera??""})`)),se(Ar,Wr)};Ft(Yt,(Ar)=>{if(h(u))Ar(or)})}Ke(()=>{jr(F,"src",h(c)),it=kr(F,1,"svelte-1hsz8dnnzz8ho",null,it,{redup:h(u)})}),ht("load",F,()=>{R(u,!1),Gr(100)}),ht("error",F,()=>{R(u,!0),Gr(1500)}),Io(F),se(m,A)},ta=(m)=>{var A=qs(),F=dt(A,!0);Ke(()=>Te(F,h(r)?.kamera?"Kamera dimatikan":"Alamat kamera belum diketahui")),se(m,A)};Ft(ti,(m)=>{if(h(d)&&h(r)?.kamera)m(ea);else m(ta,-1)})}var ri=L(ti,2),ra=dt(ri,!0);G(Mn);var Ln=L(Mn,2),Kr=L(xe(Ln),2);Rn(Kr,20,()=>[["majukiri","↖"],["maju","▲"],["majukanan","↗"],["kiri","◀"],["stop","■"],["kanan","▶"],["mundurkiri","↙"],["mundur","▼"],["mundurkanan","↘"]],([m,A])=>m,(m,A)=>{var F=vo(()=>Qn(A,2));let it=()=>h(F)[0],Yt=()=>h(F)[1];var or=Fo(),Ar=$n(or);{var Wr=(ir)=>{var Xe=$f(),Xr=dt(Xe,!0);Ke(()=>Te(Xr,Yt())),$e("pointerdown",Xe,()=>{ae(),v("/api/stop")}),se(ir,Xe)},Kn=(ir)=>{var Xe=Tf();let Xr;var aa=dt(Xe,!0);Ke(()=>{Xr=kr(Xe,1,"svelte-1hsz8dnnzz8ho",null,Xr,{aktif:h(i)===it()}),Te(aa,Yt())}),$e("pointerdown",Xe,(fa)=>ke(fa,it())),$e("pointerup",Xe,ae),ht("pointercancel",Xe,ae),ht("lostpointercapture",Xe,ae),se(ir,Xe)};Ft(Ar,(ir)=>{if(it()==="stop")ir(Wr);else ir(Kn,-1)})}se(m,or)}),G(Kr);var ni=L(Kr,2),oi=xe(ni),ii=L(oi);tr(ii),G(ni),G(Ln);var Fn=L(Ln,2),Pn=L(xe(Fn),2),si=xe(Pn),Yn=L(si);tr(Yn),G(Pn);var Bn=L(Pn,2),ai=xe(Bn),Hn=L(ai);tr(Hn),G(Bn);var na=L(Bn,2);G(Fn);var Vn=L(Fn,2),Un=L(xe(Vn),2),fi=xe(Un),li=L(fi,2),oa=L(li,2);G(Un);var qn=L(Un,2),ci=xe(qn),jn=L(ci);tr(jn),G(qn);var Gn=L(qn,2),ui=xe(Gn);tr(ui),Rr(2),G(Gn),G(Vn);var ia=L(Vn,2);{var sa=(m)=>{var A=kf(),F=dt(A,!0);Ke(()=>Te(F,h(r).status)),se(m,A)};Ft(ia,(m)=>{if(h(r)?.status)m(sa)})}G(zn),Ke(()=>{Te(Ks,h(r)?.nama??"Robot"),Jo=kr(Zo,1,"titik svelte-1hsz8dnnzz8ho",null,Jo,{on:h(n)&&h(r)?.hidup}),Te(ra,h(d)?"Matikan kamera":"Nyalakan kamera"),Te(oi,`Kecepatan ${h(o)??""}`),Te(si,`Toleh ${h(s)<0?"kiri":h(s)>0?"kanan":"lurus"}`),Te(ai,`Angguk ${h(a)??""}°`),Te(ci,`Volume ${h(f)??""}`)}),$e("click",ri,()=>R(d,!h(d))),$e("contextmenu",Kr,(m)=>m.preventDefault()),rr(ii,()=>h(o),(m)=>R(o,m)),$e("input",Yn,nr),rr(Yn,()=>h(s),(m)=>R(s,m)),$e("input",Hn,nr),rr(Hn,()=>h(a),(m)=>R(a,m)),$e("click",na,We),$e("click",fi,()=>v("/api/lagu",4000)),$e("click",li,()=>v("/api/radio",4000)),$e("click",oa,()=>v("/api/diam")),$e("change",jn,On),rr(jn,()=>h(f),(m)=>R(f,m)),ht("submit",Gn,Dn),rr(ui,()=>h(p),(m)=>R(p,m)),se(e,zn),Dr()}var js=Sf;An(["click","contextmenu","pointerdown","pointerup","input","change"]);Tr(js,{target:document.body});
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
    d["mode"] = mode == M_IKUTI ? "ikuti" : mode == M_JAGA ? "jaga" : "biasa";
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
  web.on("/api/stop", HTTP_GET, [] { kirimBadan("S"); arahMata = 0; jawabOk(); });
  web.on("/api/kepala", HTTP_GET, [] {
    kirimBadan("P " + String(argAngka("p", 90, 0, 180)) + " " + String(argAngka("t", 90, 0, 180)));
    jawabOk();
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

void tugasWeb(void*) {
  for (;;) { web.handleClient(); vTaskDelay(pdMS_TO_TICKS(2)); }
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
  uint8_t arah = ARAH_LAYAR, geo = pref.getUChar("geo", 1), rgb = pref.getUChar("rgb", 1);   // geo dan rgb: pilihan di tes_layar
  if (pref.getUChar("rot", 255) != arah) pref.putUChar("rot", arah);   // tes_media, tes_sd, tes_musik ikut arah ini
  pref.end();
  lcd.aturPanel(geo != 0, rgb != 0);
  muatLayar();
  // Saat saklar baru dinyalakan, tegangan layar naik lebih lambat dari S3. Kalau perintah awal dikirim terlalu cepat,
  // layar tidak menerimanya dan hanya tampil putih. Tunggu daya stabil, lalu kirim pengaturan awal dua kali.
  if (esp_reset_reason() == ESP_RST_POWERON || esp_reset_reason() == ESP_RST_BROWNOUT) delay(600);
  lcd.init(); delay(150); lcd.init();
  lcd.setRotation(arah > 7 ? 6 : arah);
  if (lcd.width() < lcd.height()) lcd.setRotation(geo ? 6 : 1);   // robot memakai layar mendatar 320 x 240
  lcd.setBrightness(255); lcd.fillScreen(TFT_BLACK);
  int32_t tx, ty; bool paksa = lcd.getTouchRaw(&tx, &ty);    // sentuh layar saat menyala = kalibrasi ulang
  kalibrasiSentuh(paksa);
  wajah.setPsram(true); wajah.setColorDepth(16); wajah.createSprite(320, 186);
  gambarTombol();

  SPI.begin(TFT_SCK, TFT_MISO, TFT_MOSI);
  pasangSD();

  Audio::audio_info_callback = infoAudio;
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
    MDNS.addService("http", "tcp", 80);
    siapkanWeb();
    String panel = "http://" + WiFi.localIP().toString() + "/";
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

  xTaskCreatePinnedToCore(tugasAudio, "audio", 12288, nullptr, 3, nullptr, 1);
  xTaskCreatePinnedToCore(tugasJaringan, "jaringan", 16384, nullptr, 1, nullptr, 0);
  xTaskCreatePinnedToCore(tugasDengar, "dengar", 6144, nullptr, 2, nullptr, 0);
  if (WiFi.status() == WL_CONNECTED) xTaskCreatePinnedToCore(tugasWeb, "web", 8192, nullptr, 1, nullptr, 0);
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
