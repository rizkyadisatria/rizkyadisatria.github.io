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
const char* ANTHROPIC_API_KEY = "";                   // sk-ant-... dari console.anthropic.com (BERBAYAR). Kosong = Haiku mati
const char* HAIKU_MODEL     = "claude-haiku-5-5";     // Claude Haiku: cepat, bisa melihat foto, bayar per pemakaian
const char* KAMERA_ALAMAT   = "";                     // IP saja, contoh "192.168.1.50". Kosong = cari robotcam.local
const char* NAMA_ROBOT      = "Zond";                 // nama robot; perintah suara harus menyebut nama ini
const bool  DENGAR_NAMA     = true;                   // true = mendengar terus, menanggapi kalau namanya disebut
const uint8_t ARAH_LAYAR    = 6;                      // 6 = slot SD di atas, 4 = slot SD di bawah. Tampilan terbalik? ganti 6 <-> 4
const uint8_t LAYAR_MHZ     = 27;                     // kecepatan kirim ke layar. Layar putih/bergaris dengan kabel panjang? coba 10
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
  if (fotoBase64.length()) {
    JsonObject img = isi.add<JsonObject>(); img["type"] = "image";
    img["source"]["type"] = "base64"; img["source"]["media_type"] = "image/jpeg"; img["source"]["data"] = fotoBase64;
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
String tanyaAI(const String& pertanyaan, const String& fotoBase64 = "") {
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
    bool ok = SD.begin(SD_CS, SPI, f) && SD.cardType() != CARD_NONE;
    Serial.printf("[sd] coba %u MHz: %s\n", (unsigned)(f / 1000000), ok ? "berhasil" : "gagal");
    if (ok) {
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
  "/panel  alamat panel kontrol web\n/ai  pilih otak AI (gemini, gemma, groq, haiku)\n/lagu  daftar lagu di SD\n/putar nama lagu (cukup sebagian)\n/radio url\n/diam  hentikan suara\n/volume 0-21\n"
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
  else if (cmd == "/ai") {
    if (arg.length() && !pilihAI(arg)) bot.sendMessage(chat, "Tidak bisa memakai \"" + arg + "\": nama salah atau API key-nya belum diisi.");
    String t = "Otak AI sekarang: " + String(namaAI[pilihanAI]) + (aiTerakhir.length() ? " (terakhir menjawab: " + aiTerakhir + ")" : "") + "\nPilihan:";
    for (int i = 0; i < AI_JUMLAH; i++) t += String("\n/ai ") + namaAI[i] + (aiTersedia(i) ? "" : "  (API key kosong)");
    bot.sendMessage(chat, t);
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
const char PANEL_HTML[] = R"PANEL(<!doctype html><html lang="id"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1"><title>Panel Zond</title><style>html{color:#e6e9ee;background:#10141a;font-family:system-ui,-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,Noto Sans,Ubuntu,Cantarell,Helvetica Neue,sans-serif}body{margin:0}main.svelte-2gk0ybm4y39io{max-width:480px;margin:0 auto;padding:12px 16px 32px}header.svelte-2gk0ybm4y39io{display:flex;flex-wrap:wrap;align-items: center;gap:8px}h1.svelte-2gk0ybm4y39io{margin:0;font-size:1.4rem}h2.svelte-2gk0ybm4y39io{margin:0 0 8px;font-size:1rem}h2.svelte-2gk0ybm4y39io small:where(.svelte-2gk0ybm4y39io){color:#8a94a3;font-weight:400}.kecil.svelte-2gk0ybm4y39io{color:#8a94a3;font-size:.85rem}.titik.svelte-2gk0ybm4y39io{background:#c0392b;border-radius:50%;width:10px;height:10px}.titik.on.svelte-2gk0ybm4y39io{background:#2ecc71}.peringatan.svelte-2gk0ybm4y39io{display:flex;flex-wrap:wrap;gap:6px;margin-top:8px}.peringatan.svelte-2gk0ybm4y39io span:where(.svelte-2gk0ybm4y39io){background:#7a1d1d;border-radius:6px;padding:3px 8px;font-size:.85rem}section.svelte-2gk0ybm4y39io{background:#18202a;border-radius:12px;margin-top:12px;padding:12px}.kamera.svelte-2gk0ybm4y39io{text-align:center}.kamera.svelte-2gk0ybm4y39io img:where(.svelte-2gk0ybm4y39io){display:block;aspect-ratio:4/3;object-fit:cover;background:#000;border-radius:8px;width:100%}.kamera.svelte-2gk0ybm4y39io img.redup:where(.svelte-2gk0ybm4y39io){opacity:.15}.ket.svelte-2gk0ybm4y39io{color:#8a94a3;margin:8px 0}.dpad.svelte-2gk0ybm4y39io{display:grid;grid-template-columns:repeat(3,1fr);touch-action:none;user-select:none;-webkit-user-select:none;gap:8px;max-width:300px;margin:0 auto 12px}.dpad.svelte-2gk0ybm4y39io button:where(.svelte-2gk0ybm4y39io){aspect-ratio:1;touch-action:none;background:#2a3a4d;border-radius:14px;font-size:1.6rem}.dpad.svelte-2gk0ybm4y39io button.aktif:where(.svelte-2gk0ybm4y39io){background:#3d7bd9}.dpad.svelte-2gk0ybm4y39io button.stop:where(.svelte-2gk0ybm4y39io){background:#a0302a}button.svelte-2gk0ybm4y39io{color:inherit;cursor:pointer;background:#2a3a4d;border:0;border-radius:10px;padding:10px 12px;font-size:.95rem}button.svelte-2gk0ybm4y39io:active{filter:brightness(1.3)}.kecil-btn.svelte-2gk0ybm4y39io{margin-top:8px;padding:6px 10px;font-size:.85rem}.baris.svelte-2gk0ybm4y39io{display:flex;flex-wrap:wrap;gap:8px}.baris.svelte-2gk0ybm4y39io button:where(.svelte-2gk0ybm4y39io){flex:1}label.svelte-2gk0ybm4y39io{display:block;margin-top:10px;font-size:.9rem}input[type=range].svelte-2gk0ybm4y39io{width:100%}form.svelte-2gk0ybm4y39io{display:flex;gap:8px;margin-top:12px}input[type=text].svelte-2gk0ybm4y39io{color:inherit;background:#10141a;border:1px solid #2a3a4d;border-radius:10px;flex:1;min-width:0;padding:10px;font-size:1rem}.ai.svelte-2gk0ybm4y39io{display:grid;grid-template-columns:1fr 1fr;gap:8px}.ai.svelte-2gk0ybm4y39io button:where(.svelte-2gk0ybm4y39io){text-align:left;display:flex;flex-direction:column;gap:2px}.ai.svelte-2gk0ybm4y39io button:where(.svelte-2gk0ybm4y39io):first-child{grid-column:1/-1}.ai.svelte-2gk0ybm4y39io button:where(.svelte-2gk0ybm4y39io) span:where(.svelte-2gk0ybm4y39io){color:#8a94a3;font-size:.75rem}.ai.svelte-2gk0ybm4y39io button.pilih:where(.svelte-2gk0ybm4y39io){outline:2px solid #5fcf72;background:#2f6a3a}.ai.svelte-2gk0ybm4y39io button.pilih:where(.svelte-2gk0ybm4y39io) span:where(.svelte-2gk0ybm4y39io){color:#cfe9d4}.ai.svelte-2gk0ybm4y39io button:where(.svelte-2gk0ybm4y39io):disabled{opacity:.45;cursor:default}.kecil-ket.svelte-2gk0ybm4y39io{margin-bottom:0;font-size:.8rem}.status.svelte-2gk0ybm4y39io{text-align:center;color:#8a94a3;margin-top:12px}
</style></head><body><script type="module">var f=!0;var bt=Array.isArray,hi=Array.prototype.indexOf,xt=Array.prototype.includes,lr=Array.from,{keys:Qn,defineProperty:oe,getOwnPropertyDescriptor:Le,getOwnPropertyDescriptors:eo,prototype:to}=Object,gi=Array.prototype,{getPrototypeOf:fr,isExtensible:ro}=Object;var lt=()=>{};function en(e){for(var t=0;t<e.length;t++)e[t]()}function tn(){var e,t,r=new Promise((n,o)=>{e=n,t=o});return{promise:r,resolve:e,reject:t}}function no(e,t){if(Array.isArray(e))return e;if(t===void 0||!(Symbol.iterator in e))return Array.from(e);let r=[];for(let n of e)if(r.push(n),r.length===t)break;return r}var U=2,wt=4,St=8,Ut=16777216,ce=16,ie=32,Ne=64,cr=128,rn=256,Ee=512,M=1024,O=2048,j=4096,W=8192,F=16384,$e=32768,Et=33554432,Re=65536,Nt=131072,oo=262144,ft=524288,yi=1048576,et=33554432,ur=2097152,Fe=4194304,je=8388608,Pe=Symbol("$state"),nn=Symbol("component"),on=Symbol("legacy props"),bi=Symbol(""),sn=Symbol("proxy path"),an=Symbol("attributes"),Rr=Symbol("class"),io=Symbol("style"),Cr=Symbol("text"),Rt=Symbol("form reset"),so=Symbol("hmr anchor"),$t=new class extends Error{name="StaleReactionError";message="The reaction that called `getAbortSignal()` was re-run or destroyed"},ao=!!globalThis.document?.contentType&&globalThis.document.contentType.includes("xml");var jt=3,qe=8;var xi=1,wi=2,lo=4,Ei=8,$i=16;var ki=1,Ti=2;var pr="[",Ct="[!",fo="[?",qt="]",Ye={};var P=Symbol("uninitialized"),Ge=Symbol("filename"),co=Symbol("hmr"),ln="http://www.w3.org/1999/xhtml";var Ot="font-weight: bold",Dt="font-weight: normal";function Ai(e){if(f)console.warn(`%c[svelte] await_reactivity_loss
%cDetected reactivity loss when reading \`${e}\`. This happens when state is read in an async function after an earlier \`await\`
https://svelte.dev/e/await_reactivity_loss`,Ot,Dt);else console.warn("https://svelte.dev/e/await_reactivity_loss")}function Si(e,t){if(f)console.warn(`%c[svelte] await_waterfall
%cAn async derived, \`${e}\` (${t}) was not read immediately after it resolved. This often indicates an unnecessary waterfall, which can slow down your app
https://svelte.dev/e/await_waterfall`,Ot,Dt);else console.warn("https://svelte.dev/e/await_waterfall")}function Ni(){if(f)console.warn(`%c[svelte] derived_inert
%cReading a derived belonging to a now-destroyed effect may result in stale values
https://svelte.dev/e/derived_inert`,Ot,Dt);else console.warn("https://svelte.dev/e/derived_inert")}function Ri(e,t,r){if(f)console.warn(`%c[svelte] hydration_attribute_changed
%cThe \`${e}\` attribute on \`${t}\` changed its value between server and client renders. The client value, \`${r}\`, will be ignored in favour of the server value
https://svelte.dev/e/hydration_attribute_changed`,Ot,Dt);else console.warn("https://svelte.dev/e/hydration_attribute_changed")}function Gt(e){if(f)console.warn(`%c[svelte] hydration_mismatch
%c${e?`Hydration failed because the initial UI does not match what was rendered on the server. The error occurred near ${e}`:"Hydration failed because the initial UI does not match what was rendered on the server"}
https://svelte.dev/e/hydration_mismatch`,Ot,Dt);else console.warn("https://svelte.dev/e/hydration_mismatch")}function Ci(){if(f)console.warn(`%c[svelte] lifecycle_double_unmount
%cTried to unmount a component that was not mounted
https://svelte.dev/e/lifecycle_double_unmount`,Ot,Dt);else console.warn("https://svelte.dev/e/lifecycle_double_unmount")}function fn(e){if(f)console.warn(`%c[svelte] state_proxy_equality_mismatch
%cReactive \`$state(...)\` proxies and the values they proxy have different identities. Because of this, comparisons with \`${e}\` will produce unexpected results
https://svelte.dev/e/state_proxy_equality_mismatch`,Ot,Dt);else console.warn("https://svelte.dev/e/state_proxy_equality_mismatch")}function Oi(){if(f)console.warn("%c[svelte] svelte_boundary_reset_noop\n%cA `<svelte:boundary>` `reset` function only resets the boundary the first time it is called\nhttps://svelte.dev/e/svelte_boundary_reset_noop",Ot,Dt);else console.warn("https://svelte.dev/e/svelte_boundary_reset_noop")}var b=!1;function Z(e){b=e}var T;function I(e){if(e===null)throw Gt(),Ye;return T=e}function ye(){return I(se(T))}function Y(e){if(!b)return;if(se(T)!==null)throw Gt(),Ye;T=e}function dr(e=1){if(b){var t=e,r=T;while(t--)r=se(r);T=r}}function ct(e=!0){var t=0,r=T;while(!0){if(r.nodeType===qe){var n=r.data;if(n===qt){if(t===0)return r;t-=1}else if(n===pr||n===Ct||n[0]==="["&&!isNaN(Number(n.slice(1))))t+=1}var o=se(r);if(e)r.remove();r=o}}function zt(e){if(!e||e.nodeType!==qe)throw Gt(),Ye;return e.data}function cn(e){return e===this.v}function uo(e,t){return e!=e?t==t:e!==t||e!==null&&typeof e==="object"||typeof e==="function"}function un(e){return!uo(e,this.v)}function Di(e){if(f){let t=Error(`invariant_violation
An invariant violation occurred, meaning Svelte's internal assumptions were flawed. This is a bug in Svelte, not your app — please open an issue at https://github.com/sveltejs/svelte, citing the following message: "${e}"
https://svelte.dev/e/invariant_violation`);throw t.name="Svelte error",t}else throw Error("https://svelte.dev/e/invariant_violation")}function pn(e){if(f){let t=Error(`lifecycle_outside_component
\`${e}(...)\` can only be used during component initialisation
https://svelte.dev/e/lifecycle_outside_component`);throw t.name="Svelte error",t}else throw Error("https://svelte.dev/e/lifecycle_outside_component")}function Mi(){if(f){let e=Error("async_derived_orphan\nCannot create a `$derived(...)` with an `await` expression outside of an effect tree\nhttps://svelte.dev/e/async_derived_orphan");throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/async_derived_orphan")}function po(){if(f){let e=Error("bind_invalid_checkbox_value\nUsing `bind:value` together with a checkbox input is not allowed. Use `bind:checked` instead\nhttps://svelte.dev/e/bind_invalid_checkbox_value");throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/bind_invalid_checkbox_value")}function Li(){if(f){let e=Error(`derived_references_self
A derived value cannot reference itself recursively
https://svelte.dev/e/derived_references_self`);throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/derived_references_self")}function _o(e,t,r){if(f){let n=Error(`each_key_duplicate
${r?`Keyed each block has duplicate key \`${r}\` at indexes ${e} and ${t}`:`Keyed each block has duplicate key at indexes ${e} and ${t}`}
https://svelte.dev/e/each_key_duplicate`);throw n.name="Svelte error",n}else throw Error("https://svelte.dev/e/each_key_duplicate")}function Fi(e,t,r){if(f){let n=Error(`each_key_volatile
Keyed each block has key that is not idempotent — the key for item at index ${e} was \`${t}\` but is now \`${r}\`. Keys must be the same each time for a given item
https://svelte.dev/e/each_key_volatile`);throw n.name="Svelte error",n}else throw Error("https://svelte.dev/e/each_key_volatile")}function Pi(e){if(f){let t=Error(`effect_in_teardown
\`${e}\` cannot be used inside an effect cleanup function
https://svelte.dev/e/effect_in_teardown`);throw t.name="Svelte error",t}else throw Error("https://svelte.dev/e/effect_in_teardown")}function Yi(){if(f){let e=Error("effect_in_unowned_derived\nEffect cannot be created inside a `$derived` value that was not itself created inside an effect\nhttps://svelte.dev/e/effect_in_unowned_derived");throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/effect_in_unowned_derived")}function Bi(e){if(f){let t=Error(`effect_orphan
\`${e}\` can only be used inside an effect (e.g. during component initialisation)
https://svelte.dev/e/effect_orphan`);throw t.name="Svelte error",t}else throw Error("https://svelte.dev/e/effect_orphan")}function Hi(){if(f){let e=Error(`effect_update_depth_exceeded
Maximum update depth exceeded. This typically indicates that an effect reads and writes the same piece of state
https://svelte.dev/e/effect_update_depth_exceeded`);throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/effect_update_depth_exceeded")}function Vi(){if(f){let e=Error(`hydration_failed
Failed to hydrate the application
https://svelte.dev/e/hydration_failed`);throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/hydration_failed")}function Ui(e){if(f){let t=Error(`rune_outside_svelte
The \`${e}\` rune is only available inside \`.svelte\` and \`.svelte.js/ts\` files
https://svelte.dev/e/rune_outside_svelte`);throw t.name="Svelte error",t}else throw Error("https://svelte.dev/e/rune_outside_svelte")}function ji(){if(f){let e=Error("state_descriptors_fixed\nProperty descriptors defined on `$state` objects must contain `value` and always be `enumerable`, `configurable` and `writable`.\nhttps://svelte.dev/e/state_descriptors_fixed");throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/state_descriptors_fixed")}function qi(){if(f){let e=Error("state_prototype_fixed\nCannot set prototype of `$state` object\nhttps://svelte.dev/e/state_prototype_fixed");throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/state_prototype_fixed")}function Gi(){if(f){let e=Error("state_unsafe_mutation\nUpdating state inside `$derived(...)`, `$inspect(...)` or a template expression is forbidden. If the value should not be reactive, declare it without `$state`\nhttps://svelte.dev/e/state_unsafe_mutation");throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/state_unsafe_mutation")}function zi(){if(f){let e=Error("svelte_boundary_reset_onerror\nA `<svelte:boundary>` `reset` function cannot be called while an error is still being handled\nhttps://svelte.dev/e/svelte_boundary_reset_onerror");throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/svelte_boundary_reset_onerror")}var J=!1,It=!1,tt=!1;var Or=null;function be(e,t){return e.label=t,_n(e.v,t),e}function _n(e,t){return e?.[sn]?.(t),e}function rt(e){let t=Error(),r=ba();if(r.length===0)return null;return r.unshift(`
`),oe(t,"stack",{value:r.join(`
`)}),oe(t,"name",{value:e}),t}function ba(){let e=Error.stackTraceLimit;Error.stackTraceLimit=1/0;let t=Error().stack;if(Error.stackTraceLimit=e,!t)return[];let r=t.split(`
`),n=[];for(let o=0;o<r.length;o++){let i=r[o],s=i.replaceAll("\\","/");if(i.trim()==="Error")continue;if(i.includes("validate_each_keys"))return[];if(s.includes("svelte/src/internal")||s.includes("node_modules/.vite"))continue;n.push(i)}return n}function Wi(e,t){if(!f)throw Error("invariant(...) was not guarded by if (DEV)");if(!e)Di(t)}var R=null;function kt(e){R=e}var ut=null;function _r(e){ut=e}var nt=null;function mn(e){nt=e}function Dr(e,t=!1,r){if(R={p:R,i:!1,c:null,e:null,s:e,x:null,r:x,l:It&&!t?{s:null,u:null,$:[]}:null},f)R.function=r,nt=r}function Ir(e){var t=R,r=t.e;if(r!==null){t.e=null;for(var n of r)mo(n)}if(e!==void 0)t.x=e;if(t.i=!0,R=t.p,f)nt=R?.function??null;return vn(e)}function vn(e={}){return oe(e,nn,{value:!0}),e}function pt(){return!It||R!==null&&R.l===null}var Kt=[];function Xi(){var e=Kt;Kt=[],en(e)}function X(e){if(Kt.length===0&&!Wt){var t=Kt;queueMicrotask(()=>{if(t===Kt)Xi()})}Kt.push(e)}function Zi(){while(Kt.length>0)Xi()}var xa=~(O|j|M);function L(e,t){e.f=e.f&xa|t}function mr(e){if((e.f&Ee)!==0||e.deps===null)L(e,M);else L(e,j)}function hn(e,t){if((e.f&O)!==0)t.set(e,O);else if((e.f&j)!==0&&t.get(e)!==O)t.set(e,j);L(e,M)}function Ji(e){return`{${e.toString().replace(/^async \(\) => /,"").replace(/^\(/,"").replace(/\$\.get\((.+?)\)/g,(t,r)=>r).replace(/\$\.track_reactivity_loss\((.+?)\)\(\)/g,(t,r)=>r).replace(/\)$/,"")}}`}var Qi=!1;function gn(){if(!Qi)Qi=!0,document.addEventListener("reset",(e)=>{Promise.resolve().then(()=>{if(!e.defaultPrevented)for(let t of e.target.elements)t[Rt]?.()})},{capture:!0})}function Be(e){var t=E,r=x;Q(null),ee(null);try{return e()}finally{Q(t),ee(r)}}function vo(e,t,r,n=r){e.addEventListener(t,()=>Be(r));let o=e[Rt];if(o)e[Rt]=()=>{o(),n(!0)};else e[Rt]=()=>n(!0);gn()}function yn(e,t,r,n){let o=pt()?hr:gr;var i=e.filter((c)=>!c.settled),s=t.map(o);if(f)s.forEach((c,g)=>{c.label=t[g].toString().replace("() => ","").replaceAll("$.eager(() => ","$state.eager(").replace(/\$\.get\((.+?)\)/g,(w,h)=>h)});if(r.length===0&&i.length===0){n(s);return}var a=x,l=es(),v=i.length===1?i[0].promise:i.length>1?Promise.all(i.map((c)=>c.promise)):null;function p(c){if((a.f&F)!==0)return;l();try{n([...s,...c])}catch(g){Ce(g,a)}vr()}var d=ho();if(r.length===0){v.then(()=>p([])).finally(d);return}function u(){Promise.all(r.map((c)=>yo(c))).then(p).catch((c)=>Ce(c,a)).finally(d)}if(v)v.then(()=>{if((a.f&F)!==0){d();return}l(),u(),vr()});else u()}function es(){var e=x,t=E,r=R,n=k;if(f)var o=ut;return function(s=!0){if(ee(e),Q(t),kt(r),s&&(e.f&F)===0)n?.activate(),n?.apply();if(f)go(null),_r(o)}}var wa=!1;function vr(e=!0){if(wa=!1,ee(null),Q(null),kt(null),e)k?.deactivate();if(f)go(null),_r(null)}function ho(){var e=x,t=e.b,r=k,n=!!t?.is_rendered();return t?.update_pending_count(1,r),r.increment(n,e),()=>{t?.update_pending_count(-1,r),r.decrement(n,e)}}var Oe=null;function go(e){Oe=e}var Lr=new Set;function hr(e){var t=U|O;if(x!==null)x.f|=ft;let r={ctx:R,deps:null,effects:null,equals:cn,f:t,fn:e,reactions:null,rv:0,v:P,wv:0,parent:x,ac:null};if(f&&tt)r.created=rt("created at");return r}var yr=Symbol("obsolete");function yo(e,t,r){let n=x;if(n===null)Mi();var o=void 0,i=xe(P);if(f)i.label=t??Ji(e);var s=!E,a=new Set;if(ns(()=>{var l=x;if(f)Oe={effect:l,effect_deps:new Set,warned:!1};var v=tn();o=v.promise;try{Promise.resolve(e()).then(v.resolve,(c)=>{if(c!==$t)v.reject(c)}).finally(vr)}catch(c){v.reject(c),vr()}if(f){if(Oe){if(l.deps!==null)for(let c=0;c<ke;c+=1)Oe.effect_deps.add(l.deps[c]);if(ae!==null)for(let c=0;c<ae.length;c+=1)Oe.effect_deps.add(ae[c])}Oe=null}var p=k;if(s){if((l.f&$e)!==0)var d=ho();if(n.b?.is_rendered())p.async_deriveds.get(l)?.reject(yr);else for(let c of a.values())c.reject(yr);a.add(v),p.async_deriveds.set(l,v)}let u=(c,g=void 0)=>{if(f)Oe=null;if(d?.(),a.delete(v),g===yr)return;if(p.activate(),g)i.f|=je,ot(i,g);else{if((i.f&je)!==0)i.f^=je;if(f&&r!==void 0&&!i.equals(c))Lr.add(i),setTimeout(()=>{if(Lr.has(i)&&(l.f&F)===0)Si(i.label,r),Lr.delete(i)});ot(i,c)}p.deactivate()};v.promise.then(u,(c)=>u(null,c||"unknown"))}),_e(()=>{for(let l of a)l.reject(yr)}),f)i.f|=Fe;return new Promise((l)=>{function v(p){function d(){if(p===o)l(i);else v(o)}p.then(d,d)}v(o)})}function xo(e){let t=hr(e);if(!J)wn(t);return t}function gr(e){let t=hr(e);return t.equals=un,t}function ts(e){var t=e.effects;if(t!==null){e.effects=null;for(var r=0;r<t.length;r+=1)q(t[r])}}var bo=[];function Fr(e){var t,r=x,n=e.parent;if(!Ke&&n!==null&&e.v!==P&&(n.f&(F|W))!==0)return Ni(),e.v;if(ee(n),f){let o=Xt;bn(new Set);try{if(xt.call(bo,e))Li();bo.push(e),ts(e),t=xn(e)}finally{ee(r),bn(o),bo.pop()}}else try{ts(e),t=xn(e)}finally{ee(r)}return t}function wo(e){var t=Fr(e);if(!e.equals(t)){if(e.wv=br(),!k?.is_fork||e.deps===null){if(k!==null)k.capture(e,t,!0),ze?.capture(e,t,!0);else e.v=t;if(e.deps===null){L(e,M);return}}}if(Ke)return;if(le!==null){if(Mt()||k?.is_fork)le.set(e,t)}else mr(e)}function rs(e){if(e.effects===null)return;for(let t of e.effects)if(t.teardown||t.ac){if(t.teardown?.(),t.ac!==null)Be(()=>{t.ac.abort($t),t.ac=null});if(t.fn!==null)t.teardown=lt;Zt(t,0),Pr(t)}}function Eo(e){if(e.effects===null)return;for(let t of e.effects)if(t.teardown&&t.fn!==null)dt(t)}var En=null,xr=null,k=null,ze=null,le=null,To=null,Wt=!1,$o=!1,wr=null,Yr=null,os=0,ko=new Set,ka=1;class me{id=ka++;started=!1;linked=!0;prev=null;next=null;async_deriveds=new Map;current=new Map;previous=new Map;#e=new Set;#t=new Set;#r=0;#a=new Map;#o=null;#n=[];#s=[];#i=new Map;#l=new Map;#d=new Set;is_fork=!1;#f=!1;constructor(){if(xr===null)En=xr=this;else xr.next=this,this.prev=xr;xr=this}#m(){if(this.is_fork)return!0;for(let e of this.#a.keys())if(!this.#g(e))return!0;return!1}#g(e){for(var t=e;t!==null;t=t.parent)if(this.#l.has(t))return!0;return!1}skip_effect(e){if(!this.#l.has(e))this.#l.set(e,{d:[],m:[]});this.#d.delete(e)}unskip_effect(e,t=(r)=>this.schedule(r)){var r=this.#l.get(e);if(r){this.#l.delete(e);for(var n of r.d)L(n,O),t(n);for(n of r.m)L(n,j),t(n)}this.#d.add(e)}#c(){var e=[];for(let o of this.#n){if((o.f&F)!==0||(o.f&(O|j))===0)continue;var t=o,r=!1;while(t.parent!==null){t=t.parent;var n=t.f;if((n&(Ne|ie))!==0){if((n&M)===0){r=!0;break}t.f^=M}}if(!r)e.push(t)}return this.#n=[],e}#v(){if(this.started=!0,f)for(let s of this.current.keys())ko.add(s);for(let[s,a]of this.#i)L(s,a),this.schedule(s);this.apply();var e=wr=[],t=[],r=Yr=[];while(this.#n.length>0){if(os++>1000)this.#p(),Ta();for(let s of this.#c())try{this.#y(s,e,t)}catch(a){if(ls(s),!this.#m())this.discard();throw a}}if(k=null,r.length>0){var n=me.ensure();for(let s of r)n.schedule(s)}if(wr=null,Yr=null,this.#m()){this.#_(t),this.#_(e);for(let[s,a]of this.#l)as(s,a);if(r.length>0)k.#v();return}let o=this.#x();if(o){this.#_(t),this.#_(e),o.#u(this);return}this.#i.clear();for(let s of this.#e)s(this);this.#e.clear(),ze=this,is(t),is(e),ze=null,this.#o?.resolve();var i=k;if(this.#r===0&&(this.#n.length===0||i!==null)){if(this.#p(),J)this.#b(),k=i}if(i!==null)Xe.clear(),i.#v()}#y(e,t,r){e.f^=M;var n=e.first;while(n!==null){var o=n.f,i=(o&(ie|Ne))!==0,s=i&&(o&M)!==0,a=s||(o&W)!==0||this.#l.has(n);if(!a&&n.fn!==null){if(i)n.f^=M;else if((o&wt)!==0)t.push(n);else if(J&&(o&(St|Ut))!==0)r.push(n);else if(Ft(n)){if((o&ce)!==0)this.add_dirty_reaction(n,j);dt(n)}var l=n.first;if(l!==null){n=l;continue}}while(n!==null){var v=n.next;if(v!==null){n=v;break}n=n.parent}}}#x(){for(var e=this.prev;e!==null;e=e.prev)if(!e.is_fork&&this.#h(e))return e;return null}#h(e){for(let[t,[,r]]of this.current){if(r)continue;if(e.current.has(t))return!0}return!1}#u(e){for(let[r,n]of e.current){if(!this.previous.has(r)&&e.previous.has(r))this.previous.set(r,e.previous.get(r));this.current.set(r,n)}for(let[r,n]of e.async_deriveds){let o=this.async_deriveds.get(r);if(o)n.promise.then(o.resolve).catch(o.reject)}e.async_deriveds.clear(),this.transfer_reactions(e.#i);let t=(r)=>{var n=r.reactions;if(n===null)return;if((r.f&U)!==0&&(r.f&(O|j))===0)return;for(let s of n){var o=s.f;if((o&U)!==0)t(s);else{var i=s;if(o&(Fe|ce)&&!this.async_deriveds.has(i))this.#i.delete(i),L(i,O),this.schedule(i)}}};for(let r of this.current.keys())t(r);this.oncommit(()=>e.discard()),e.#p(),k=this,this.#v()}#_(e){for(var t=0;t<e.length;t+=1)hn(e[t],this.#i)}capture(e,t,r=!1){if(e.v!==P&&!this.previous.has(e))this.previous.set(e,e.v);if((e.f&je)===0)this.current.set(e,[t,r]),le?.set(e,t);if(!this.is_fork)e.v=t}activate(){k=this}deactivate(){k=null,le=null}flush(){try{if(f)ko.clear();$o=!0,k=this,this.#v()}finally{if(os=0,To=null,wr=null,Yr=null,$o=!1,k=null,le=null,Xe.clear(),f)for(let e of ko)e.updated=null}}discard(){for(let e of this.#t)e(this);this.#t.clear();for(let e of this.async_deriveds.values())e.reject(yr);this.#p(),this.#o?.resolve()}register_created_effect(e){this.#s.push(e)}#b(){for(let p=En;p!==null;p=p.next){var e=p.id<this.id,t=[];for(let[d,[u,c]]of this.current){if(p.current.has(d)){var r=p.current.get(d)[0];if(e&&u!==r)p.current.set(d,[u,c]);else continue}t.push(d)}if(e)for(let[d,u]of this.async_deriveds){let c=p.async_deriveds.get(d);if(c)u.promise.then(c.resolve).catch(c.reject)}var n=[...p.current.keys()].filter((d)=>!p.current.get(d)[1]);if(!p.started||n.length===0)continue;var o=n.filter((d)=>!this.current.has(d));if(o.length===0){if(e)p.discard()}else if(t.length>0){if(f&&!p.#f)Wi(p.#n.length===0,"Batch has scheduled effects");if(e)for(let d of this.#d)p.unskip_effect(d,(u)=>{if((u.f&(ce|Fe))!==0)p.schedule(u);else p.#_([u])});p.activate();var i=new Set,s=new Map;for(var a of t)ss(a,o,i,s);s=new Map;var l=[...p.current].filter(([d,u])=>{let c=this.current.get(d);if(!c)return!0;return c[0]!==u[0]||c[1]!==u[1]}).map(([d])=>d);if(l.length>0){for(let d of this.#s)if((d.f&(F|W|Nt))===0&&Ao(d,l,s))if((d.f&(Fe|ce))!==0)L(d,O),p.schedule(d);else p.add_dirty_reaction(d,O)}if(p.#n.length>0&&!p.#f){p.apply();for(var v of p.#c())p.#y(v,[],[])}p.deactivate()}}}increment(e,t){if(this.#r+=1,e){let r=this.#a.get(t)??0;this.#a.set(t,r+1)}}decrement(e,t){if(this.#r-=1,e){let r=this.#a.get(t)??0;if(r===1)this.#a.delete(t);else this.#a.set(t,r-1)}if(this.#f)return;this.#f=!0,X(()=>{if(this.#f=!1,this.linked)this.flush()})}transfer_reactions(e){for(let[t,r]of e)this.add_dirty_reaction(t,r);e.clear()}add_dirty_reaction(e,t){if(t===O||this.#i.get(e)!==O)this.#i.set(e,t)}oncommit(e){this.#e.add(e)}ondiscard(e){this.#t.add(e)}settled(){return(this.#o??=tn()).promise}static ensure(){if(k===null){let e=k=new me;if(!$o&&!Wt)X(()=>{if(!e.started)e.flush()})}return k}apply(){if(!J||!this.is_fork&&this.prev===null&&this.next===null){le=null;return}le=new Map;for(let[e,[t]]of this.current)le.set(e,t);for(let e=En;e!==null;e=e.next){if(e===this||e.is_fork)continue;if(e.id>=this.id||!e.#h(this)){for(let[t,r]of e.previous)if(!le.has(t))le.set(t,r)}}}schedule(e){if(To=e,e.b?.is_pending&&(e.f&(wt|St|Ut))!==0&&(e.f&$e)===0){e.b.defer_effect(e);return}this.#n.push(e)}#p(){if(!this.linked)return;var e=this.prev,t=this.next;if(e===null)En=t;else e.next=t;if(t===null)xr=e;else t.prev=e;this.linked=!1}}function Lt(e){var t=Wt,r=ze;ze=null,Wt=!0;try{var n;if(e)Lt(),n=e();while(!0){if(Zi(),k===null)return n;k.flush()}}finally{Wt=t,ze=r}}function Ta(){if(f){var e=new Map;for(let r of k.current.keys())for(let[n,o]of r.updated??[]){var t=e.get(n);if(!t)t={error:o.error,count:0},e.set(n,t);t.count+=o.count}for(let r of e.values())if(r.error)console.error(r.error)}try{Hi()}catch(r){if(f)oe(r,"stack",{value:""});Ce(r,To)}}var We=null;function is(e){var t=e.length;if(t===0)return;var r=0;while(r<t){var n=e[r++];if((n.f&(F|W))===0&&Ft(n)){if(We=new Set,dt(n),n.deps===null&&n.first===null&&n.nodes===null&&n.teardown===null&&n.ac===null)So(n);if(We?.size>0){Xe.clear();for(let o of We){if((o.f&(F|W))!==0)continue;let i=[o],s=o.parent;while(s!==null){if(We.has(s))We.delete(s),i.push(s);s=s.parent}for(let a=i.length-1;a>=0;a--){let l=i[a];if((l.f&(F|W))!==0)continue;dt(l)}}We.clear()}}}We=null}function ss(e,t,r,n){if(r.has(e))return;if(r.add(e),e.reactions!==null)for(let o of e.reactions){let i=o.f;if((i&U)!==0)ss(o,t,r,n);else if((i&(Fe|ce))!==0&&(i&O)===0&&Ao(o,t,n))L(o,O),Br(o)}}function Ao(e,t,r){let n=r.get(e);if(n!==void 0)return n;if(e.deps!==null)for(let o of e.deps){if(xt.call(t,o))return!0;if((o.f&U)!==0&&Ao(o,t,r))return r.set(o,!0),!0}return r.set(e,!1),!1}function Br(e){k.schedule(e)}function as(e,t){if((e.f&ie)!==0&&(e.f&M)!==0)return;if((e.f&O)!==0)t.d.push(e);else if((e.f&j)!==0)t.m.push(e);L(e,M);var r=e.first;while(r!==null)as(r,t),r=r.next}function ls(e){L(e,M);var t=e.first;while(t!==null)ls(t),t=t.next}var Xt=new Set,Xe=new Map;function bn(e){Xt=e}var Ro=!1;function cs(){Ro=!0}function xe(e,t){var r={f:0,v:e,reactions:null,equals:cn,rv:0,wv:0};if(f&&tt)r.created=t??rt("created at"),r.updated=null,r.set_during_effect=!1,r.trace=null;return r}function H(e,t){let r=xe(e,t);return wn(r),r}function Qt(e,t=!1,r=!0){let n=xe(e);if(!t)n.equals=un;if(It&&r&&R!==null&&R.l!==null)(R.l.s??=[]).push(n);return n}function N(e,t,r=!1){if(E!==null&&(!Te||(E.f&Nt)!==0)&&pt()&&(E.f&(U|ce|Fe|Nt))!==0&&(it===null||!it.has(e)))Gi();let n=r?Pt(t):t;if(f)_n(n,e.label);return ot(e,n,Yr)}var Jt=null,No=0;function ot(e,t,r=null){if(!e.equals(t)){if(Ke)Xe.set(e,t);else if(!Xe.has(e))Xe.set(e,e.v);var n=me.ensure();if(n.capture(e,t),f){if(tt||x!==null){e.updated??=new Map;let o=(e.updated.get("")?.count??0)+1;if(e.updated.set("",{error:null,count:o}),tt||o>5){let i=rt("updated at");if(i!==null){let s=e.updated.get(i.stack);if(!s)s={error:i,count:0},e.updated.set(i.stack,s);s.count++}}}if(x!==null)e.set_during_effect=!0}if((e.f&U)!==0){let o=e;if((e.f&O)!==0)Fr(o);if(le===null)mr(o)}if(e.wv=br(),Jt=null,No=0,us(e,O,r),Jt=null,pt()&&x!==null&&(x.f&M)!==0&&(x.f&(ie|Ne))===0)if(He===null)ps([e]);else He.push(e);if(!n.is_fork&&Xt.size>0&&!Ro)$n()}return t}function $n(){Ro=!1;for(let e of Xt){if((e.f&M)!==0)L(e,j);let t;try{t=Ft(e)}catch{t=!0}if(t)dt(e)}Xt.clear()}function er(e){N(e,e.v+1)}function us(e,t,r){var n=e.reactions;if(n===null)return;var o=pt(),i=n.length;if(No+=i,No>1e5&&Jt===null)Jt=new Set;if(Jt!==null){if(Jt.has(e))return;Jt.add(e)}for(var s=0;s<i;s++){var a=n[s],l=a.f;if(!o&&a===x)continue;var v=(l&O)===0;if(v)L(a,t);if((l&Nt)!==0)Xt.add(a);else if((l&U)!==0){var p=a;le?.delete(p),us(p,j,r)}else if(v){var d=a;if((l&ce)!==0&&We!==null)We.add(d);if(r!==null)r.push(d);else Br(d)}}}var Sa=/^[a-zA-Z_$][a-zA-Z_$0-9]*$/;function Pt(e){if(typeof e!=="object"||e===null||Pe in e||nn in e)return e;let t=fr(e);if(t!==to&&t!==gi)return e;var r=new Map,n=bt(e),o=H(0),i=f&&tt?rt("created at"):null,s=Yt,a=(d)=>{if(Yt===s)return d();var u=E,c=Yt;Q(null),Co(s);var g=d();return Q(u),Co(c),g};if(n){if(r.set("length",H(e.length,i)),f)e=Ra(e)}var l="";let v=!1;function p(d){if(v)return;v=!0,l=d,be(o,`${l} version`);for(let[u,c]of r)be(c,tr(l,u));v=!1}return new Proxy(e,{defineProperty(d,u,c){if(!("value"in c)||c.configurable===!1||c.enumerable===!1||c.writable===!1)ji();var g=r.get(u);if(g===void 0)a(()=>{var w=H(c.value,i);if(r.set(u,w),f&&typeof u==="string")be(w,tr(l,u));return w});else N(g,c.value,!0);return!0},deleteProperty(d,u){var c=r.get(u);if(c===void 0){if(u in d){let g=a(()=>H(P,i));if(r.set(u,g),er(o),f)be(g,tr(l,u))}}else N(c,P),er(o);return!0},get(d,u,c){if(u===Pe)return e;if(f&&u===sn)return p;var g=r.get(u),w=u in d;if(g===void 0&&(!w||Le(d,u)?.writable))g=a(()=>{var A=Pt(w?d[u]:P),S=H(A,i);if(f)be(S,tr(l,u));return S}),r.set(u,g);if(g!==void 0){var h=_(g);return h===P?void 0:h}return Reflect.get(d,u,c)},getOwnPropertyDescriptor(d,u){this.has?.(d,u);var c=Reflect.getOwnPropertyDescriptor(d,u),g=r.get(u);if(g!==void 0){var w=_(g);if(w===P)return;if(c&&"value"in c)c.value=w;else return{enumerable:!0,configurable:!0,value:w,writable:!0}}return c},has(d,u){if(u===Pe)return!0;var c=r.get(u),g=c!==void 0&&c.v!==P||Reflect.has(d,u);if(c!==void 0||(x!==null||E!==null)&&(!g||Le(d,u)?.writable)){if(c===void 0)c=a(()=>{var h=g?Pt(d[u]):P,A=H(h,i);if(f)be(A,tr(l,u));return A}),r.set(u,c);var w=_(c);if(w===P)return!1}return g},set(d,u,c,g){var w=r.get(u),h=u in d;if(n&&u==="length")for(var A=c;A<w.v;A+=1){var S=r.get(A+"");if(S!==void 0)N(S,P);else if(A in d){if(S=a(()=>H(P,i)),r.set(A+"",S),f)be(S,tr(l,A))}}if(w===void 0){if(!h||Le(d,u)?.writable){if(w=a(()=>H(void 0,i)),f)be(w,tr(l,u));N(w,Pt(c)),r.set(u,w)}}else{h=w.v!==P;var V=a(()=>Pt(c));N(w,V)}var fe=Reflect.getOwnPropertyDescriptor(d,u);if(fe?.set)fe.set.call(g,c);if(!h){if(n&&typeof u==="string"){var Se=r.get("length"),he=Number(u);if(Number.isInteger(he)&&he>=Se.v)N(Se,he+1)}er(o)}return!0},ownKeys(d){_(o);var u=Reflect.ownKeys(d).filter((w)=>{var h=r.get(w);return h===void 0||h.v!==P});for(var[c,g]of r)if(g.v!==P&&!(c in d))u.push(c);return u},setPrototypeOf(){qi()}})}function tr(e,t){if(typeof t==="symbol")return`${e}[Symbol(${t.description??""})]`;if(Sa.test(t))return`${e}.${t}`;return/^\d+$/.test(t)?`${e}[${t}]`:`${e}['${t}']`}function kn(e){try{if(e!==null&&typeof e==="object"&&Pe in e)return e[Pe]}catch{}return e}var Na=new Set(["copyWithin","fill","pop","push","reverse","shift","sort","splice","unshift"]);function Ra(e){return new Proxy(e,{get(t,r,n){var o=Reflect.get(t,r,n);if(!Na.has(r))return o;return function(...i){cs();var s=o.apply(this,i);return $n(),s}}})}function ds(){let{prototype:e,__svelte_cleanup:t}=Array;if(t)t();let{indexOf:r,lastIndexOf:n,includes:o}=e;e.indexOf=function(i,s){let a=r.call(this,i,s);if(a===-1){for(let l=s??0;l<this.length;l+=1)if(kn(this[l])===i){fn("array.indexOf(...)");break}}return a},e.lastIndexOf=function(i,s){let a=n.call(this,i,s??this.length-1);if(a===-1){for(let l=0;l<=(s??this.length-1);l+=1)if(kn(this[l])===i){fn("array.lastIndexOf(...)");break}}return a},e.includes=function(i,s){let a=o.call(this,i,s);if(!a){for(let l=0;l<this.length;l+=1)if(kn(this[l])===i){fn("array.includes(...)");break}}return a},Array.__svelte_cleanup=()=>{e.indexOf=r,e.lastIndexOf=n,e.includes=o}}var Er,_s,Hr,ms,vs;function Tn(){if(Er!==void 0)return;Er=window,_s=document,Hr=/Firefox/.test(navigator.userAgent);var e=Element.prototype,t=Node.prototype,r=Text.prototype;if(ms=Le(t,"firstChild").get,vs=Le(t,"nextSibling").get,ro(e))e[Rr]=void 0,e[an]=null,e[io]=void 0,e.__e=void 0;if(ro(r))r[Cr]=void 0;if(f)e.__svelte_meta=null,ds()}function G(e=""){return document.createTextNode(e)}function de(e){return ms.call(e)}function se(e){return vs.call(e)}function te(e,t){if(!b)return de(e);var r=de(T);if(r===null)r=T.appendChild(G());else if(t&&r.nodeType!==jt){var n=G();return r?.before(n),I(n),n}if(t)Vr(r);return I(r),r}function An(e,t=!1){if(!b){var r=de(e);if(r instanceof Comment&&r.data==="")return se(r);return r}if(t){if(T?.nodeType!==jt){var n=G();return T?.before(n),I(n),n}Vr(T)}return T}function De(e,t=!1){if(!b)return de(e);var r=te(e,t);return Y(e),r}function D(e,t=1,r=!1){let n=b?T:e;var o;while(t--)o=n,n=se(n);if(!b)return n;if(r){if(n?.nodeType!==jt){var i=G();if(n===null)o?.after(i);else n.before(i);return I(i),i}Vr(n)}return I(n),n}function Mr(e){e.textContent=""}function Sn(){if(!J)return!1;if(We!==null)return!1;var e=x.f;return(e&$e)!==0}function _t(e,t,r){if(t==null||t===ln)return r?document.createElement(e,{is:r}):document.createElement(e);return r?document.createElementNS(t,e,{is:r}):document.createElementNS(t,e)}function Vr(e){if(e.nodeValue.length<65536)return;let t=e.nextSibling;while(t!==null&&t.nodeType===jt)t.remove(),e.nodeValue+=t.nodeValue,t=e.nextSibling}var Oo=new WeakMap;function hs(e){var t=x;if(t===null)return E.f|=je,e;if(f&&e instanceof Error&&!Oo.has(e))Oo.set(e,Ca(e,t));if((t.f&$e)===0&&(t.f&wt)===0){if(f&&!t.parent&&e instanceof Error)gs(e);throw e}Ce(e,t)}function Ce(e,t){if(e===Ye)throw e;if(t!==null&&(t.f&F)!==0)return;while(t!==null){if((t.f&cr)!==0&&(t.f&(F|Et))===0){if((t.f&$e)===0)throw e;try{t.b.error(e);return}catch(r){e=r}}t=t.parent}if(f&&e instanceof Error)gs(e);throw e}function Ca(e,t){let r=Le(e,"message");if(r&&!r.configurable)return;var n=Hr?"  ":"\t",o=`
${n}in ${t.fn?.name||"<unknown>"}`,i=t.ctx;while(i!==null)o+=`
${n}in ${i.function?.[Ge].split("/").pop()}`,i=i.p;return{message:e.message+`
${o}
`,stack:e.stack?.split(`
`).filter((s)=>!s.includes("svelte/src/internal")).join(`
`)}}function gs(e){let t=Oo.get(e);if(t)oe(e,"message",{value:t.message}),oe(e,"stack",{value:t.stack})}function ys(e){if(x===null){if(E===null)Bi(e);Yi()}if(Ke)Pi(e)}function Oa(e,t){var r=t.last;if(r===null)t.last=t.first=e;else r.next=e,e.prev=r,t.last=e}function Tt(e,t){var r=x;if(f)while(r!==null&&(r.f&Nt)!==0)r=r.parent;if(r!==null&&(r.f&W)!==0)e|=W;var n={ctx:R,deps:null,nodes:null,f:e|O|Ee,first:null,fn:t,last:null,next:null,parent:r,b:r&&r.b,prev:null,teardown:null,wv:0,ac:null};if(f)n.component_function=nt;k?.register_created_effect(n);var o=n;if((e&wt)!==0)if(wr!==null)wr.push(n);else me.ensure().schedule(n);else if(t!==null){try{dt(n)}catch(s){throw q(n),s}if(o.deps===null&&o.teardown===null&&o.nodes===null&&o.first===o.last&&(o.f&ft)===0){if(o=o.first,(e&ce)!==0&&(e&Re)!==0&&o!==null)o.f|=Re}}if(o!==null){if(o.parent=r,r!==null)Oa(o,r);if(E!==null&&(E.f&U)!==0&&(e&Ne)===0){var i=E;(i.effects??=[]).push(o)}}return n}function Mt(){return E!==null&&!Te}function _e(e){let t=Tt(St,null);return L(t,M),t.teardown=e,t}function $r(e){if(ys("$effect"),f)oe(e,"name",{value:"$effect"});var t=x.f,r=!E&&(t&ie)!==0&&R!==null&&!R.i;if(r){var n=R;(n.e??=[]).push(e)}else return mo(e)}function mo(e){return Tt(wt|yi,e)}function Io(e){me.ensure();let t=Tt(Ne|ft,e);return()=>{q(t)}}function bs(e){me.ensure();let t=Tt(Ne|ft,e);return(r={})=>new Promise((n)=>{if(r.outro)mt(t,()=>{q(t),n(void 0)});else q(t),n(void 0)})}function ns(e){return Tt(Fe|ft,e)}function ve(e,t=0){return Tt(St|t,e)}function Ae(e,t=[],r=[],n=[]){yn(n,t,r,(o)=>{Tt(St,()=>{e(...o.map(_))})})}function Ve(e,t=0){var r=Tt(ce|t,e);if(f)r.dev_stack=ut;return r}function re(e){return Tt(ie|ft,e)}function Mo(e){var t=e.teardown;if(t!==null){let r=Ke,n=E;Do(!0),Q(null);try{t.call(null)}catch(o){Ce(o,e.parent)}finally{Do(r),Q(n)}}}function Pr(e,t=!1){var r=e.first;e.first=e.last=null;while(r!==null){let o=r.ac;if(o!==null)Be(()=>{o.abort($t)});var n=r.next;if((r.f&Ne)!==0)r.parent=null;else q(r,t);r=n}}function xs(e){var t=e.first;while(t!==null){var r=t.next;if((t.f&ie)===0)q(t);t=r}}function q(e,t=!0){var r=!1;if((t||(e.f&oo)!==0)&&e.nodes!==null&&e.nodes.end!==null)ws(e.nodes.start,e.nodes.end),r=!0;e.f|=Et,Pr(e,t&&!r),Zt(e,0);var n=e.nodes&&e.nodes.t;if(n!==null)for(let i of n)i.stop();Mo(e),e.f^=Et,e.f|=F;var o=e.parent;if(o!==null&&o.first!==null)So(e);if(f)e.component_function=null;e.next=e.prev=e.teardown=e.ctx=e.deps=e.fn=e.nodes=e.ac=e.b=null}function ws(e,t){while(e!==null){var r=e===t?null:se(e);e.remove(),e=r}}function So(e){var{parent:t,prev:r,next:n}=e;if(r!==null)r.next=n;if(n!==null)n.prev=r;if(t!==null){if(t.first===e)t.first=n;if(t.last===e)t.last=r}}function mt(e,t,r=!0){var n=[];e.f|=rn,Es(e,n,!0);var o=()=>{if(r)q(e);if(t)t()},i=n.length;if(i>0){var s=()=>--i||o();for(var a of n)a.out(s)}else o()}function Es(e,t,r){if((e.f&W)!==0)return;e.f^=W;var n=e.nodes&&e.nodes.t;if(n!==null){for(let a of n)if(a.is_global||r)t.push(a)}var o=e.first;while(o!==null){var i=o.next;if((o.f&Ne)===0){var s=(o.f&Re)!==0||(o.f&ie)!==0&&(e.f&ce)!==0;Es(o,t,s?r:!1)}o=i}}function kr(e){e.f&=~rn,$s(e,!0)}function $s(e,t){if((e.f&rn)!==0)return;if((e.f&W)===0)return;if(e.f^=W,(e.f&M)===0)L(e,O),me.ensure().schedule(e);var r=e.first;while(r!==null){var n=r.next,o=(r.f&Re)!==0||(r.f&ie)!==0;$s(r,o?t:!1),r=n}var i=e.nodes&&e.nodes.t;if(i!==null){for(let s of i)if(s.is_global||t)s.in()}}function Tr(e,t){if(!e.nodes)return;var r=e.nodes.start,n=e.nodes.end;while(r!==null){var o=r===n?null:se(r);t.append(r),r=o}}var ks=null;var Ke=!1;function Do(e){Ke=e}var E=null,Te=!1;function Q(e){E=e}var x=null;function ee(e){x=e}var it=null;function wn(e){if(E!==null&&(!J&&(E.f&ur)!==0||(E.f&U)!==0))(it??=new Set).add(e)}var ae=null,ke=0,He=null;function ps(e){He=e}var As=1,rr=0,Yt=rr;function Co(e){Yt=e}function br(){return++As}function Ft(e){var t=e.f;if((t&O)!==0)return!0;if((t&j)!==0){var r=e.deps,n=r.length;for(var o=0;o<n;o++){var i=r[o];if(Ft(i))wo(i);if(i.wv>e.wv)return!0}if((t&Ee)!==0&&le===null)L(e,M)}return!1}function Ss(e,t,r=!0){var n=e.reactions;if(n===null)return;if(!J&&it!==null&&it.has(e))return;for(var o=0;o<n.length;o++){var i=n[o];if((i.f&U)!==0)Ss(i,t,!1);else if(t===i){if(r)L(i,O);else if((i.f&M)!==0)L(i,j);Br(i)}}}function xn(e){var t=ae,r=ke,n=He,o=E,i=it,s=R,a=Te,l=Yt,v=e.f;if(ae=null,ke=0,He=null,E=(v&(ie|Ne))===0?e:null,it=null,kt(e.ctx),Te=!1,Yt=++rr,e.ac!==null)Be(()=>{e.ac.abort($t)}),e.ac=null;try{e.f|=ur;var p=e.fn,d=p();e.f|=$e;var u=Ts(e);if(pt()&&He!==null&&!Te&&u!==null&&(e.f&(U|j|O))===0)for(var c=0;c<He.length;c++)Ss(He[c],e);if(o!==null&&o!==e){if(rr++,o.deps!==null)for(let g=0;g<r;g+=1)o.deps[g].rv=rr;if(t!==null)for(let g of t)g.rv=rr;if(He!==null)if(n===null)n=He;else n.push(...He)}if((e.f&je)!==0)e.f^=je;return d}catch(g){return Ts(e),hs(g)}finally{e.f^=ur,ae=t,ke=r,He=n,E=o,it=i,kt(s),Te=a,Yt=l}}function Ts(e){var t=e.deps,r=k?.is_fork;if(ae!==null){var n;if(!r)Zt(e,ke);if(t!==null&&ke>0){t.length=ke+ae.length;for(n=0;n<ae.length;n++)t[ke+n]=ae[n]}else e.deps=t=ae;if(Mt()&&(e.f&Ee)!==0)for(n=ke;n<t.length;n++)(t[n].reactions??=[]).push(e)}else if(!r&&t!==null&&ke<t.length)Zt(e,ke),t.length=ke;return t}function Da(e,t){let r=t.reactions;if(r!==null){var n=hi.call(r,e);if(n!==-1){var o=r.length-1;if(o===0)r=t.reactions=null;else r[n]=r[o],r.pop()}}if(r===null&&(t.f&U)!==0&&(ae===null||!xt.call(ae,t))){var i=t;if((i.f&Ee)!==0)i.f^=Ee;if(i.v!==P)mr(i);if(i.ac!==null)Be(()=>{i.ac.abort($t),i.ac=null,L(i,O)});rs(i),Zt(i,0)}}function Zt(e,t){var r=e.deps;if(r===null)return;for(var n=t;n<r.length;n++)Da(e,r[n])}function dt(e){var t=e.f;if((t&F)!==0)return;L(e,M);var r=x;if(x=e,f){var n=nt;mn(e.component_function);var o=ut;_r(e.dev_stack??ut)}try{if((t&(ce|Ut))!==0)xs(e);else Pr(e);Mo(e);var i=xn(e);if(e.teardown=typeof i==="function"?i:null,e.wv=As,f&&tt&&(e.f&O)!==0&&e.deps!==null){for(var s of e.deps)if(s.set_during_effect)s.wv=br(),s.set_during_effect=!1}}finally{if(x=r,f)mn(n),_r(o)}}async function Ur(){if(J)return new Promise((e)=>{requestAnimationFrame(()=>e()),setTimeout(()=>e())});await Promise.resolve(),Lt()}function _(e){var t=e.f,r=(t&U)!==0;if(ks?.add(e),E!==null&&!Te){var n=x!==null&&(x.f&F)!==0;if(!n&&(it===null||!it.has(e))){var o=E.deps;if((E.f&ur)!==0){if(e.rv<rr)if(e.rv=rr,ae===null&&o!==null&&o[ke]===e)ke++;else if(ae===null)ae=[e];else ae.push(e)}else{if(E.deps??=[],!xt.call(E.deps,e))E.deps.push(e);var i=e.reactions;if(i===null)e.reactions=[E];else if(!xt.call(i,E))i.push(E)}}}if(f){if(!Te&&Oe&&k===null&&ze===null&&!Oe.warned&&(Oe.effect.f&ur)===0&&!Oe.effect_deps.has(e)){Oe.warned=!0,Ai(e.label);var s=rt("traced at");if(s)console.warn(s)}if(Lr.delete(e),tt&&!Te&&Or!==null&&E!==null&&Or.reaction===E){if(e.trace)e.trace();else if(s=rt("traced at"),s){var a=Or.entries.get(e);if(a===void 0)a={traces:[]},Or.entries.set(e,a);var l=a.traces[a.traces.length-1];if(s.stack!==l?.stack)a.traces.push(s)}}}if(Ke&&Xe.has(e))return Xe.get(e);if(r){var v=e;if(Ke){var p=v.v;if((v.f&M)===0&&v.reactions!==null||Rs(v))p=Fr(v);return Xe.set(v,p),p}var d=(v.f&Ee)===0&&!Te&&E!==null&&(E.f&Ee)!==0,u=(v.f&$e)===0;if(Ft(v)){if(d)v.f|=Ee;wo(v)}if(d&&!u)Eo(v),Ns(v)}if(le?.has(e))return le.get(e);if((e.f&je)!==0)throw e.v;return e.v}function Ns(e){if(e.f|=Ee,e.deps===null)return;for(let r of e.deps){var t=r.reactions;if(t===null)r.reactions=[e];else if(!xt.call(t,e))t.push(e);if((r.f&U)!==0&&(r.f&Ee)===0)Eo(r),Ns(r)}}function Rs(e){if(e.v===P)return!0;if(e.deps===null)return!1;for(let t of e.deps){if(Xe.has(t))return!0;if((t.f&U)!==0&&Rs(t))return!0}return!1}function B(e){var t=Te;try{return Te=!0,e()}finally{Te=t}}var Ma=["allowfullscreen","async","autofocus","autoplay","checked","controls","default","disabled","formnovalidate","indeterminate","inert","ismap","loop","multiple","muted","nomodule","novalidate","open","playsinline","readonly","required","reversed","seamless","selected","webkitdirectory","defer","disablepictureinpicture","disableremoteplayback"];var xp=[...Ma,"formNoValidate","isMap","noModule","playsInline","readOnly","value","volume","defaultValue","defaultChecked","srcObject","noValidate","allowFullscreen","disablePictureInPicture","disableRemotePlayback"];var La=["touchstart","touchmove"];function Cs(e){return La.includes(e)}var Fa=["$state","$state.raw","$derived","$derived.by"],wp=[...Fa,"$state.eager","$state.snapshot","$props","$props.id","$bindable","$effect","$effect.pre","$effect.tracking","$effect.root","$effect.pending","$inspect","$inspect().with","$inspect.trace","$host"];var Ar=Symbol("events"),Po=new Set,Nn=new Set;function Yo(e){if(!b)return;e.removeAttribute("onload"),e.removeAttribute("onerror");let t=e.__e;if(t!==void 0)e.__e=void 0,queueMicrotask(()=>{if(e.isConnected)e.dispatchEvent(t)})}function Is(e,t,r,n={}){function o(i){if(!n.capture)Cn.call(t,i);if(!i.cancelBubble)return Be(()=>r?.call(this,i))}if(e.startsWith("pointer")||e.startsWith("touch")||e==="wheel")o.__removed=!1,X(()=>{if(!o.__removed)t.addEventListener(e,o,n)});else t.addEventListener(e,o,n);return o}function vt(e,t,r,n,o){var i={capture:n,passive:o},s=Is(e,t,r,i);if(t===document.body||t===window||t===document||t instanceof HTMLMediaElement)_e(()=>{s.__removed=!0,t.removeEventListener(e,s,i)})}function we(e,t,r){(t[Ar]??={})[e]=r}function Rn(e){for(var t=0;t<e.length;t++)Po.add(e[t]);for(var r of Nn)r(e)}var Lo=null,Fo=!1;function Cn(e){var t=this,r=t.ownerDocument,n=e.type,o=e.composedPath?.()||[],i=o[0]||e.target;if(Lo=e,!Fo)Fo=!0,setTimeout(()=>{Fo=!1,Lo=null});var s=0,a=Lo===e&&e[Ar];if(a){var l=o.indexOf(a);if(l!==-1&&(t===document||t===window)){e[Ar]=t;return}var v=o.indexOf(t);if(v===-1)return;if(l<=v)s=l}if(i=o[s]||e.target,i===t)return;oe(e,"currentTarget",{configurable:!0,get(){return i||r}});var p=E,d=x;Q(null),ee(null);try{var u,c=[];while(i!==null){if(i===t)break;try{var g=i[Ar]?.[n];if(g!=null&&(!i.disabled||e.target===i))g.call(i,e)}catch(w){if(u)c.push(w);else u=w}if(e.cancelBubble)break;s++,i=s<o.length?o[s]:null}if(u){for(let w of c)queueMicrotask(()=>{throw w});throw u}}finally{e[Ar]=t,delete e.currentTarget,Q(p),ee(d)}}var Ya=globalThis?.window?.trustedTypes&&globalThis.window.trustedTypes.createPolicy("svelte-trusted-html",{createHTML:(e)=>e});function Ms(e){return Ya?.createHTML(e)??e}function Bo(e){var t=_t("template");return t.innerHTML=Ms(e.replaceAll("<!>","<!---->")),t.content}function Ue(e,t){var r=x;if(r.nodes===null)r.nodes={start:e,end:t,a:null,t:null}}function Ze(e,t){var r=(t&ki)!==0,n=(t&Ti)!==0,o,i=!e.startsWith("<!>");return()=>{if(b)return Ue(T,null),T;if(o===void 0){if(o=Bo(i?e:"<!>"+e),!r)o=de(o)}var s=n||Hr?document.importNode(o,!0):o.cloneNode(!0);if(r){var a=de(s),l=s.lastChild;Ue(a,l)}else Ue(s,s);return s}}function jr(e=""){if(!b){var t=G(e+"");return Ue(t,t),t}var r=T;if(r.nodeType!==jt)r.before(r=G()),I(r);else Vr(r);return Ue(r,r),r}function Vo(){if(b)return Ue(T,null),T;var e=document.createDocumentFragment(),t=document.createComment(""),r=G();return e.append(t,r),Ue(t,r),e}function z(e,t){if(b){var r=x;if((r.f&$e)===0||r.nodes.end===null)r.nodes.end=T;ye();return}if(e===null)return;e.before(t)}function Ls(e){let t=0,r=xe(0),n;if(f)be(r,"createSubscriber version");return()=>{if(Mt())_(r),ve(()=>{if(t===0)n=B(()=>e(()=>er(r)));return t+=1,()=>{X(()=>{if(t-=1,t===0)n?.(),n=void 0,er(r)})}})}}var Va=Re|ft;function Uo(e,t,r,n){new Fs(e,t,r,n)}class Fs{parent;is_pending=!1;transform_error;#e;#t=b?T:null;#r;#a;#o;#n=null;#s=null;#i=null;#l=null;#d=0;#f=0;#m=!1;#g=new Map;#c=null;#v=Ls(()=>{if(this.#c=xe(this.#d),f)be(this.#c,"$effect.pending()");return()=>{this.#c=null}});constructor(e,t,r,n){if(this.#e=e,this.#r=t,this.#a=(o)=>{var i=x;i.b=this,i.f|=cr,r(o)},this.parent=x.b,this.transform_error=n??this.parent?.transform_error??((o)=>o),this.#o=Ve(()=>{if(b){let o=zt(this.#t);ye();let i=o===Ct;if(o.startsWith(fo)){let a=JSON.parse(o.slice(fo.length));this.#x(a)}else if(i)this.#_();else this.#y()}else this.#b()},Va),b)this.#e=T}#y(){try{this.#n=re(()=>this.#a(this.#e))}catch(e){this.error(e)}}#x(e){let t=this.#r.failed,{reset:r,invoke_onerror:n}=this.#h(e);if(X(n),!t)return;this.#i=re(()=>{t(this.#e,()=>e,()=>r)})}#h(e){var t=!1,r=!1;let n=()=>{if(this.#u())return;if(t){Oi();return}if(t=!0,r)zi();if(this.#i!==null)mt(this.#i,()=>{this.#i=null});this.#w(()=>{this.#b()})};return{reset:n,invoke_onerror:()=>{if(this.#u())return;try{r=!0,this.#r.onerror?.(e,n),r=!1}catch(i){Ce(i,this.#o&&this.#o.parent)}}}}#u(){return(this.#o.f&(F|Et))!==0}#_(){let e=this.#r.pending;if(!e)return;this.is_pending=!0,this.#s=re(()=>e(this.#e)),X(()=>{if(this.#u())return;var t=this.#l=document.createDocumentFragment(),r=G(),n=!1;if(t.append(r),this.#n=this.#w(()=>{try{return re(()=>this.#a(r))}catch(o){try{this.error(o),n=!0}catch(i){Ce(i,this.#o.parent)}return null}}),this.#n===null){if(this.#l=null,n)this.#p(k);return}if(this.#f===0)this.#e.before(t),this.#l=null,mt(this.#s,()=>{this.#s=null}),this.#p(k)})}#b(){try{if(this.is_pending=this.has_pending_snippet(),this.#f=0,this.#d=0,this.#n=re(()=>{this.#a(this.#e)}),this.#f>0){var e=this.#l=document.createDocumentFragment();Tr(this.#n,e);let t=this.#r.pending;this.#s=re(()=>t(this.#e))}else this.#p(k)}catch(t){this.error(t)}}#p(e){this.is_pending=!1,e.transfer_reactions(this.#g)}defer_effect(e){hn(e,this.#g)}is_rendered(){return!this.is_pending&&(!this.parent||this.parent.is_rendered())}has_pending_snippet(){return!!this.#r.pending}#w(e){var t=x,r=E,n=R;ee(this.#o),Q(this.#o),kt(this.#o.ctx);try{return me.ensure(),e()}finally{ee(t),Q(r),kt(n)}}#E(e,t){if(!this.has_pending_snippet()){if(this.parent)this.parent.#E(e,t);return}if(this.#f+=e,this.#f===0){if(this.#p(t),this.#s)mt(this.#s,()=>{this.#s=null});if(this.#l)this.#e.before(this.#l),this.#l=null}}update_pending_count(e,t){if(this.#E(e,t),this.#d+=e,!this.#c||this.#m)return;this.#m=!0,X(()=>{if(this.#m=!1,this.#c)ot(this.#c,this.#d)})}get_effect_pending(){return this.#v(),_(this.#c)}error(e){if(e===Ye)throw e;if(!this.#r.onerror&&!this.#r.failed)throw e;if(k?.is_fork){if(this.#n)k.skip_effect(this.#n);if(this.#s)k.skip_effect(this.#s);if(this.#i)k.skip_effect(this.#i);k.oncommit(()=>{if(!this.#u())this.#$(e)})}else this.#$(e)}#$(e){if(this.#n)q(this.#n),this.#n=null;if(this.#s)q(this.#s),this.#s=null;if(this.#i)q(this.#i),this.#i=null;if(b)I(this.#t),dr(),I(ct());let t=this.#r.failed,r=(n)=>{if(this.#u())return;let{reset:o,invoke_onerror:i}=this.#h(n);if(i(),t&&!this.#u())this.#i=this.#w(()=>{try{return re(()=>{var s=x;s.b=this,s.f|=cr,t(this.#e,()=>n,()=>o)})}catch(s){return Ce(s,this.#o.parent),null}})};X(()=>{if(this.#u())return;var n;try{n=this.transform_error(e)}catch(o){Ce(o,this.#o&&this.#o.parent);return}if(n!==null&&typeof n==="object"&&typeof n.then==="function")n.then(r,(o)=>Ce(o,this.#o&&this.#o.parent));else r(n)})}}var jo=!0;function ne(e,t){var r=t==null?"":typeof t==="object"?`${t}`:t;if(r!==(e[Cr]??=e.nodeValue))e[Cr]=r,e.nodeValue=`${r}`}function Sr(e,t){return Ps(e,t)}function Go(e,t){Tn(),t.intro=t.intro??!1;let r=t.target,n=b,o=T;try{var i=de(r);while(i&&(i.nodeType!==qe||i.data!==pr))i=se(i);if(!i)throw Ye;Z(!0),I(i);let s=Ps(e,{...t,anchor:i});return Z(!1),s}catch(s){if(s instanceof Error&&s.message.split(`
`).some((a)=>a.startsWith("https://svelte.dev/e/")))throw s;if(s!==Ye)console.warn("Failed to hydrate: ",s);if(t.recover===!1)Vi();return Tn(),Mr(r),Z(!1),Sr(e,t)}finally{Z(n),I(o)}}var On=new Map;function Ps(e,{target:t,anchor:r,props:n={},events:o,context:i,intro:s=!0,transformError:a}){Tn();var l=void 0,v=bs(()=>{var p=r??t.appendChild(G());Uo(p,{pending:()=>{}},(c)=>{Dr({});var g=R;if(i)g.c=i;if(o)n.$$events=o;if(b)Ue(c,null);if(jo=s,l=e(c,n)||vn(),jo=!0,b){if(x.nodes.end=T,T===null||T.nodeType!==qe||T.data!==qt)throw Gt(),Ye}Ir()},a);var d=new Set,u=(c)=>{for(var g=0;g<c.length;g++){var w=c[g];if(d.has(w))continue;d.add(w);var h=Cs(w);for(let V of[t,document]){var A=On.get(V);if(A===void 0)A=new Map,On.set(V,A);var S=A.get(w);if(S===void 0)V.addEventListener(w,Cn,{passive:h}),A.set(w,1);else A.set(w,S+1)}}};return u(lr(Po)),Nn.add(u),()=>{for(var c of d)for(let h of[t,document]){var g=On.get(h),w=g.get(c);if(--w==0){if(h.removeEventListener(c,Cn),g.delete(c),g.size===0)On.delete(h)}else g.set(c,w)}if(Nn.delete(u),p!==r)p.parentNode?.removeChild(p)}});return qo.set(l,v),l}var qo=new WeakMap;function zo(e,t){let r=qo.get(e);if(r)return qo.delete(e),r(t);if(f)Ci();return Promise.resolve()}class Bt{anchor;#e=new Map;#t=new Map;#r=new Map;#a=new Set;#o=!0;constructor(e,t=!0){this.anchor=e,this.#o=t}#n=(e)=>{if(!this.#e.has(e))return;var t=this.#e.get(e),r=this.#t.get(t);if(r)kr(r),this.#a.delete(t);else{var n=this.#r.get(t);if(n){if(kr(n.effect),this.#t.set(t,n.effect),this.#r.delete(t),f)n.fragment.lastChild[so]=this.anchor;n.fragment.lastChild.remove(),this.anchor.before(n.fragment),r=n.effect}}for(let[o,i]of this.#e){if(this.#e.delete(o),o===e)break;let s=this.#r.get(i);if(s)q(s.effect),this.#r.delete(i)}for(let[o,i]of this.#t){if(o===t||this.#a.has(o))continue;let s=()=>{if(Array.from(this.#e.values()).includes(o)){var l=document.createDocumentFragment();Tr(i,l),l.append(G()),this.#r.set(o,{effect:i,fragment:l})}else q(i);this.#a.delete(o),this.#t.delete(o)};if(this.#o||!r)this.#a.add(o),mt(i,s,!1);else s()}};#s=(e)=>{this.#e.delete(e);let t=Array.from(this.#e.values());for(let[r,n]of this.#r)if(!t.includes(r))q(n.effect),this.#r.delete(r)};ensure(e,t){var r=k,n=Sn();if(t&&!this.#t.has(e)&&!this.#r.has(e))if(n){var o=document.createDocumentFragment(),i=G();o.append(i),this.#r.set(e,{effect:re(()=>t(i)),fragment:o})}else this.#t.set(e,re(()=>t(this.anchor)));if(this.#e.set(r,e),n){for(let[s,a]of this.#t)if(s===e)r.unskip_effect(a);else r.skip_effect(a);for(let[s,a]of this.#r)if(s===e)r.unskip_effect(a.effect);else r.skip_effect(a.effect);r.oncommit(this.#n),r.ondiscard(this.#s)}else{if(b)this.anchor=T;this.#n(r)}}}function ht(e,t,r=!1){var n;if(b)n=T,ye();var o=new Bt(e),i=r?Re:0;function s(a,l){if(b){var v=zt(n);if(a!==parseInt(v.substring(1))){var p=ct();I(p),o.anchor=p,Z(!1),o.ensure(a,l),Z(!0);return}}o.ensure(a,l)}Ve(()=>{var a=!1;if(t((l,v=0)=>{a=!0,s(v,l)}),!a)s(-1,null)},i)}var X_=Symbol("NaN");function ja(e,t,r){var n=[],o=t.length,i,s=t.length;for(var a=0;a<o;a++){let d=t[a];mt(d,()=>{if(i){if(i.pending.delete(d),i.done.add(d),i.pending.size===0){var u=e.outrogroups;if(Ko(e,lr(i.done)),u.delete(i),u.size===0)e.outrogroups=null}}else s-=1},!1)}if(s===0){var l=n.length===0&&r!==null&&e.pending.size===0;if(l){var v=r,p=v.parentNode;Mr(p),p.append(v),e.items.clear()}Ko(e,t,!l)}else i={pending:new Set(t),done:new Set},(e.outrogroups??=new Set).add(i)}function Ko(e,t,r=!0){var n;if(e.pending.size>0){n=new Set;for(let s of e.pending.values())for(let a of s)n.add(e.items.get(a).e)}for(var o=0;o<t.length;o++){var i=t[o];if(n?.has(i)){i.f|=et;let s=document.createDocumentFragment();Tr(i,s)}else q(t[o],r)}}var Ys;function zr(e,t,r,n,o,i=null){var s=e,a=new Map,l=(t&lo)!==0;if(l){var v=e;s=b?I(de(v)):v.appendChild(G())}if(b)ye();var p=null,d=gr(()=>{var S=r();return bt(S)?S:S==null?[]:lr(S)});if(f)be(d,"{#each ...}");var u=new Map,c=!0;function g(S){if((A.effect.f&F)!==0)return;A.pending.delete(S);var V=_(d);if(A.fallback=p,qa(A,V,s,t,n),p!==null)if(V.length===0)if((p.f&et)===0)kr(p);else p.f^=et,Gr(p,null,s);else mt(p,()=>{p=null})}function w(S){A.pending.delete(S)}var h=Ve(()=>{var S=_(d),V=S.length;let fe=!1;if(b){var Se=zt(s)===Ct;if(Se!==(V===0))s=ct(),I(s),Z(!1),fe=!0}var he=new Set,ge=k,gt=Sn();for(var K=0;K<V;K+=1){if(b&&T.nodeType===qe&&T.data===qt)s=T,fe=!0,Z(!1);var Ie=S[K],yt=n(Ie,K);if(f){var Wr=n(Ie,K);if(yt!==Wr)Fi(String(K),String(yt),String(Wr))}var Je=c?null:a.get(yt);if(Je){if(Je.v)ot(Je.v,Ie);if(Je.i)ot(Je.i,K);if(gt)ge.unskip_effect(Je.e)}else{if(Je=Ga(a,c?s:Ys??=G(),Ie,yt,K,o,t,r),!c)Je.e.f|=et;a.set(yt,Je)}he.add(yt)}if(V===0&&i&&!p)if(c)p=re(()=>i(s));else p=re(()=>i(Ys??=G())),p.f|=et;if(V>he.size)if(f)za(S,n);else _o("","","");if(b&&V>0)I(ct());if(!c)if(u.set(ge,he),gt){for(let[Xr,Nr]of a)if(!he.has(Xr))ge.skip_effect(Nr.e);ge.oncommit(g),ge.ondiscard(w)}else g(ge);if(fe)Z(!0);_(d)}),A={effect:h,flags:t,items:a,pending:u,outrogroups:null,fallback:p};if(c=!1,b)s=T}function qr(e){while(e!==null&&(e.f&ie)===0)e=e.next;return e}function qa(e,t,r,n,o){var i=(n&Ei)!==0,s=t.length,a=e.items,l=qr(e.effect.first),v,p=null,d,u=[],c=[],g,w,h,A;if(i){for(A=0;A<s;A+=1)if(g=t[A],w=o(g,A),h=a.get(w).e,(h.f&et)===0)h.nodes?.a?.measure(),(d??=new Set).add(h)}for(A=0;A<s;A+=1){if(g=t[A],w=o(g,A),h=a.get(w).e,e.outrogroups!==null)for(let Ie of e.outrogroups)Ie.pending.delete(h),Ie.done.delete(h);if((h.f&W)!==0){if(kr(h),i)h.nodes?.a?.unfix(),(d??=new Set).delete(h)}if((h.f&et)!==0)if(h.f^=et,h===l)Gr(h,null,r);else{var S=p?p.next:l;if(h===e.effect.last)e.effect.last=h.prev;if(h.prev)h.prev.next=h.next;if(h.next)h.next.prev=h.prev;Ht(e,p,h),Ht(e,h,S),Gr(h,S,r),p=h,u=[],c=[],l=qr(p.next);continue}if(h!==l){if(v!==void 0&&v.has(h)){if(u.length<c.length){var V=c[0],fe;p=V.prev;var Se=u[0],he=u[u.length-1];for(fe=0;fe<u.length;fe+=1)Gr(u[fe],V,r);for(fe=0;fe<c.length;fe+=1)v.delete(c[fe]);Ht(e,Se.prev,he.next),Ht(e,p,Se),Ht(e,he,V),l=V,p=he,A-=1,u=[],c=[]}else v.delete(h),Gr(h,l,r),Ht(e,h.prev,h.next),Ht(e,h,p===null?e.effect.first:p.next),Ht(e,p,h),p=h;continue}u=[],c=[];while(l!==null&&l!==h)(v??=new Set).add(l),c.push(l),l=qr(l.next);if(l===null)continue}if((h.f&et)===0)u.push(h);p=h,l=qr(h.next)}if(e.outrogroups!==null){for(let Ie of e.outrogroups)if(Ie.pending.size===0)Ko(e,lr(Ie.done)),e.outrogroups?.delete(Ie);if(e.outrogroups.size===0)e.outrogroups=null}if(l!==null||v!==void 0){var ge=[];if(v!==void 0){for(h of v)if((h.f&W)===0)ge.push(h)}while(l!==null){if((l.f&W)===0&&l!==e.fallback)ge.push(l);l=qr(l.next)}var gt=ge.length;if(gt>0){var K=(n&lo)!==0&&s===0?r:null;if(i){for(A=0;A<gt;A+=1)ge[A].nodes?.a?.measure();for(A=0;A<gt;A+=1)ge[A].nodes?.a?.fix()}ja(e,ge,K)}}if(i)X(()=>{if(d===void 0)return;for(h of d)h.nodes?.a?.apply()})}function Ga(e,t,r,n,o,i,s,a){var l=(s&xi)!==0?(s&$i)===0?Qt(r,!1,!1):xe(r):null,v=(s&wi)!==0?xe(o):null;if(f&&l)l.trace=()=>{a()[v?.v??o]};return{v:l,i:v,e:re(()=>(i(t,l??r,v??o,a),()=>{e.delete(n)}))}}function Gr(e,t,r){if(!e.nodes)return;var n=e.nodes.start,o=e.nodes.end,i=t&&(t.f&et)===0?t.nodes.start:r;while(n!==null){var s=se(n);if(i.before(n),n===o)return;n=s}}function Ht(e,t,r){if(t===null)e.effect.first=r;else t.next=r;if(r===null)e.effect.last=t;else r.prev=t}function za(e,t){let r=new Map,n=e.length;for(let o=0;o<n;o++){let i=t(e[o],o);if(r.has(i)){let s=String(r.get(i)),a=String(o),l=String(i);if(l.startsWith("[object "))l=null;_o(s,a,l)}r.set(i,o)}}var Hs=[...` 	
\r\f \v\uFEFF`];function Vs(e,t,r){var n=e==null?"":""+e;if(t)n=n?n+" "+t:t;if(r){for(var o of Object.keys(r))if(r[o])n=n?n+" "+o:o;else if(n.length){var i=o.length,s=0;while((s=n.indexOf(o,s))>=0){var a=s+i;if((s===0||Hs.includes(n[s-1]))&&(a===n.length||Hs.includes(n[a])))n=(s===0?"":n.substring(0,s))+n.substring(a+1);else s=a}}}return n===""?null:n}function nr(e,t,r,n,o,i){var s=e[Rr];if(b||s!==r||s===void 0){var a=Vs(r,n,i);if(!b||a!==e.getAttribute("class"))if(a==null)e.removeAttribute("class");else if(t)e.className=a;else e.setAttribute("class",a);e[Rr]=r}else if(i&&o!==i)for(var l in i){var v=!!i[l];if(o==null||v!==!!o[l])e.classList.toggle(l,v)}return i}var al=Symbol("class"),ll=Symbol("style"),fl=Symbol("is custom element"),cl=Symbol("is html"),ul=ao?"link":"LINK";function or(e){if(!b)return;var t=!1,r=()=>{if(t)return;if(t=!0,e.hasAttribute("value")){var n=e.value;Kr(e,"value",null),e.value=n}if(e.hasAttribute("checked")){var o=e.checked;Kr(e,"checked",null),e.checked=o}};e[Rt]=r,X(r),gn()}function Kr(e,t,r,n){var o=pl(e);if(b){if(o[t]=e.getAttribute(t),t==="src"||t==="srcset"||t==="href"&&e.nodeName===ul){if(!n)_l(e,t,r??"");return}}var i=o[t];if(i===(o[t]=r))return;if(t==="loading")e[bi]=r;if(r==null)e.removeAttribute(t);else if(typeof r!=="string"&&dl(e).has(t))e[t]=r;else if(!(b&&(typeof r==="number"||typeof r==="boolean")&&i===String(r)))e.setAttribute(t,r)}function pl(e){return e[an]??={[fl]:e.nodeName.includes("-"),[cl]:e.namespaceURI===ln}}var Us=new Map;function dl(e){var t=e.getAttribute("is")||e.nodeName,r=Us.get(t);if(r)return r;Us.set(t,r=new Set);var n,o=e,i=Element.prototype;while(i!==o){n=eo(o);for(var s in n)if(n[s].set&&s!=="innerHTML"&&s!=="textContent"&&s!=="innerText")r.add(s);o=fr(o)}return r}function _l(e,t,r){if(!f)return;if(t==="srcset"&&ml(e,r))return;if(Wo(e.getAttribute(t)??"",r))return;Ri(t,e.outerHTML.replace(e.innerHTML,e.innerHTML&&"..."),String(r))}function Wo(e,t){if(e===t)return!0;return new URL(e,document.baseURI).href===new URL(t,document.baseURI).href}function js(e){return e.split(",").map((t)=>t.trim().split(" ").filter(Boolean))}function ml(e,t){var r=js(e.srcset),n=js(t);return n.length===r.length&&n.every(([o,i],s)=>i===r[s][1]&&(Wo(r[s][0],o)||Wo(o,r[s][0])))}function ir(e,t,r=t){var n=new WeakSet;if(vo(e,"input",async(o)=>{if(f&&e.type==="checkbox")po();var i=o?e.defaultValue:e.value;if(i=Xo(e)?Zo(i):i,r(i),k!==null)n.add(k);if(await Ur(),i!==(i=t())){var{selectionStart:s,selectionEnd:a}=e,l=e.value.length;if(e.value=i??"",a!==null){var v=e.value.length;if(s===a&&a===l&&v>l)e.selectionStart=v,e.selectionEnd=v;else e.selectionStart=s,e.selectionEnd=Math.min(a,v)}}}),b&&e.defaultValue!==e.value||B(t)==null&&e.value){if(r(Xo(e)?Zo(e.value):e.value),k!==null)n.add(k)}ve(()=>{if(f&&e.type==="checkbox")po();var o=t();if(e===document.activeElement){var i=J?ze:k;if(n.has(i))return}if(Xo(e)&&o===Zo(e.value))return;if(e.type==="date"&&!o&&!e.value)return;if(o!==e.value)e.value=o??""})}function Xo(e){var t=e.type;return t==="number"||t==="range"}function Zo(e){return e===""?null:+e}var _y=Symbol("unmounted");function Gs(e){return new zs(e)}class zs{#e;#t;constructor(e){var t=new Map,r=(o,i)=>{var s=Qt(i,!1,!1);return t.set(o,s),s};let n=new Proxy({...e.props||{},$$events:{}},{get(o,i){return _(t.get(i)??r(i,Reflect.get(o,i)))},has(o,i){if(i===on)return!0;return _(t.get(i)??r(i,Reflect.get(o,i))),Reflect.has(o,i)},set(o,i,s){return N(t.get(i)??r(i,s),s),Reflect.set(o,i,s)}});if(this.#t=(e.hydrate?Go:Sr)(e.component,{target:e.target,anchor:e.anchor,props:n,context:e.context,intro:e.intro??!1,recover:e.recover,transformError:e.transformError}),!J&&(!e?.props?.$$host||e.sync===!1))Lt();this.#e=n.$$events;for(let o of Object.keys(this.#t)){if(o==="$set"||o==="$destroy"||o==="$on")continue;oe(this,o,{get(){return this.#t[o]},set(i){this.#t[o]=i},enumerable:!0})}this.#t.$set=(o)=>{Object.assign(n,o)},this.#t.$destroy=()=>{zo(this.#t)}}$set(e){this.#t.$set(e)}$on(e,t){this.#e[e]=this.#e[e]||[];let r=(...n)=>t.call(this,...n);return this.#e[e].push(r),()=>{this.#e[e]=this.#e[e].filter((n)=>n!==r)}}$destroy(){this.#t.$destroy()}}var kl;if(typeof HTMLElement==="function")kl=class extends HTMLElement{$$ctor;$$s;$$c;$$cn=!1;$$d={};$$r=!1;$$p_d={};$$l={};$$l_u=new Map;$$me;$$shadowRoot=null;constructor(e,t,r){super();if(this.$$ctor=e,this.$$s=t,r)this.$$shadowRoot=this.attachShadow(r)}addEventListener(e,t,r){if(this.$$l[e]=this.$$l[e]||[],this.$$l[e].push(t),this.$$c){let n=this.$$c.$on(e,t);this.$$l_u.set(t,n)}super.addEventListener(e,t,r)}removeEventListener(e,t,r){if(super.removeEventListener(e,t,r),this.$$c){let n=this.$$l_u.get(t);if(n)n(),this.$$l_u.delete(t)}}async connectedCallback(){if(this.$$cn=!0,!this.$$c){let e=function(n){return(o)=>{let i=_t("slot");if(n!=="default")i.name=n;z(o,i)}};if(await Promise.resolve(),!this.$$cn||this.$$c)return;let t={},r=Tl(this);for(let n of this.$$s)if(n in r)if(n==="default"&&!this.$$d.children)this.$$d.children=e(n),t.default=!0;else t[n]=e(n);for(let n of this.attributes){let o=this.$$g_p(n.name);if(!(o in this.$$d))this.$$d[o]=Qo(o,n.value,this.$$p_d,"toProp")}for(let n in this.$$p_d)if(!(n in this.$$d)&&this[n]!==void 0)this.$$d[n]=this[n],delete this[n];this.$$c=Gs({component:this.$$ctor,target:this.$$shadowRoot||this,props:{...this.$$d,$$slots:t,$$host:this}}),this.$$me=Io(()=>{ve(()=>{this.$$r=!0;for(let n of Qn(this.$$c)){if(!this.$$p_d[n]?.reflect)continue;this.$$d[n]=this.$$c[n];let o=Qo(n,this.$$d[n],this.$$p_d,"toAttribute");if(o==null)this.removeAttribute(this.$$p_d[n].attribute||n);else this.setAttribute(this.$$p_d[n].attribute||n,o)}this.$$r=!1})});for(let n in this.$$l)for(let o of this.$$l[n]){let i=this.$$c.$on(n,o);this.$$l_u.set(o,i)}this.$$l={}}}attributeChangedCallback(e,t,r){if(this.$$r)return;e=this.$$g_p(e),this.$$d[e]=Qo(e,r,this.$$p_d,"toProp"),this.$$c?.$set({[e]:this.$$d[e]})}disconnectedCallback(){this.$$cn=!1,Promise.resolve().then(()=>{if(!this.$$cn&&this.$$c)this.$$c.$destroy(),this.$$me(),this.$$c=void 0})}$$g_p(e){return Qn(this.$$p_d).find((t)=>this.$$p_d[t].attribute===e||!this.$$p_d[t].attribute&&t.toLowerCase()===e)||e}};function Qo(e,t,r,n){let o=r[e]?.type;if(t=o==="Boolean"&&typeof t!=="boolean"?t!=null:t,!n||!r[e])return t;else if(n==="toAttribute")switch(o){case"Object":case"Array":return t==null?null:JSON.stringify(t);case"Boolean":return t?"":null;case"Number":return t==null?null:t;default:return t}else switch(o){case"Object":case"Array":return t&&JSON.parse(t);case"Boolean":return t;case"Number":return t!=null?+t:t;default:return t}}function Tl(e){let t={};return e.childNodes.forEach((r)=>{t[r.slot||"default"]=!0}),t}if(f){let e=function(t){if(!(t in globalThis)){let r;Object.defineProperty(globalThis,t,{configurable:!0,get:()=>{if(r!==void 0)return r;Ui(t)},set:(n)=>{r=n}})}};e("$state"),e("$effect"),e("$derived"),e("$inspect"),e("$props"),e("$bindable")}function Ks(e){if(R===null)pn("onMount");if(It&&R.l!==null)Rl(R).m.push(e);else $r(()=>{let t=B(e);if(typeof t==="function")return t})}function Rl(e){var t=e.l;return t.u??={a:[],b:[],m:[]}}var Ws="5";if(typeof window<"u")((window.__svelte??={}).v??=new Set).add(Ws);var Cl=Ze('<span class="svelte-2gk0ybm4y39io"> </span>'),Ol=Ze('<div class="peringatan svelte-2gk0ybm4y39io"></div>'),ei=Ze('<p class="ket svelte-2gk0ybm4y39io"> </p>'),Dl=Ze('<img alt=""/> <!>',1),Il=Ze('<button class="stop svelte-2gk0ybm4y39io"> </button>'),Ml=Ze("<button> </button>"),Ll=Ze('<small class="svelte-2gk0ybm4y39io"> </small>'),Fl=Ze('<button><b> </b> <span class="svelte-2gk0ybm4y39io"> </span></button>'),Pl=Ze('<p class="status svelte-2gk0ybm4y39io"> </p>'),Yl=Ze('<main class="svelte-2gk0ybm4y39io"><header class="svelte-2gk0ybm4y39io"><h1 class="svelte-2gk0ybm4y39io"> </h1> <span></span> <span class="kecil svelte-2gk0ybm4y39io"><!></span></header> <!> <section class="kamera svelte-2gk0ybm4y39io"><!> <button class="kecil-btn svelte-2gk0ybm4y39io"> </button></section> <section class="svelte-2gk0ybm4y39io"><h2 class="svelte-2gk0ybm4y39io">Gerak <small class="svelte-2gk0ybm4y39io">tahan untuk jalan, lepas untuk berhenti</small></h2> <div class="dpad svelte-2gk0ybm4y39io"></div> <label class="svelte-2gk0ybm4y39io"> <input type="range" min="100" max="255" class="svelte-2gk0ybm4y39io"/></label></section> <section class="svelte-2gk0ybm4y39io"><h2 class="svelte-2gk0ybm4y39io">Kepala</h2> <label class="svelte-2gk0ybm4y39io"> <input type="range" min="-60" max="60" class="svelte-2gk0ybm4y39io"/></label> <label class="svelte-2gk0ybm4y39io"> <input type="range" min="40" max="150" class="svelte-2gk0ybm4y39io"/></label> <button class="kecil-btn svelte-2gk0ybm4y39io">Lurus ke depan</button></section> <section class="svelte-2gk0ybm4y39io"><h2 class="svelte-2gk0ybm4y39io">Musik dan suara</h2> <div class="baris svelte-2gk0ybm4y39io"><button class="svelte-2gk0ybm4y39io">Lagu berikutnya</button> <button class="svelte-2gk0ybm4y39io">Radio berikutnya</button> <button class="svelte-2gk0ybm4y39io">Diam</button></div> <label class="svelte-2gk0ybm4y39io"> <input type="range" min="0" max="21" class="svelte-2gk0ybm4y39io"/></label> <form class="svelte-2gk0ybm4y39io"><input type="text" placeholder="Suruh Zond bilang..." maxlength="140" class="svelte-2gk0ybm4y39io"/> <button type="submit" class="svelte-2gk0ybm4y39io">Bilang</button></form></section> <section class="svelte-2gk0ybm4y39io"><h2 class="svelte-2gk0ybm4y39io">Otak AI <!></h2> <div class="ai svelte-2gk0ybm4y39io"></div> <!> <p class="ket kecil-ket svelte-2gk0ybm4y39io">Kalau pilihanmu gagal (jatah habis atau saldo kosong), robot otomatis memakai yang gratis.</p></section> <!></main>');function Bl(e,t){Dr(t,!0);let r=[{id:"otomatis",nama:"Otomatis",ket:"gratis: Gemini, lalu Gemma, lalu Groq"},{id:"gemini",nama:"Gemini",ket:"gratis, jatah harian kecil"},{id:"gemma",nama:"Gemma",ket:"gratis, jatah besar"},{id:"groq",nama:"Groq",ket:"gratis, cepat"},{id:"haiku",nama:"Claude Haiku",ket:"berbayar per pemakaian"}],n=H(""),o=H("");async function i(m){N(n,m,!0),N(o,"");let y=await S(`/api/ai?m=${m}`);if(!y)N(o,"Robot tidak menjawab");else if(!y.ok)N(o,await y.text(),!0);await V(),N(n,"")}let s=H(null),a=H(!1),l=H(160),v=H(null),p=H(0),d=H(90),u=H(12),c=!1,g=H(""),w=H(!0),h=H(!1),A=H("");async function S(m,y=1500){let C=new AbortController,Me=setTimeout(()=>C.abort(),y);try{return await fetch(m,{signal:C.signal,cache:"no-store"})}catch{return null}finally{clearTimeout(Me)}}async function V(){let m=await S("/api/status");if(!m||!m.ok){N(a,!1);return}if(N(a,!0),N(s,await m.json(),!0),!c)N(u,_(s).vol,!0)}let fe={maju:[1,1],mundur:[-1,-1],kiri:[-1,1],kanan:[1,-1],majukiri:[0.35,1],majukanan:[1,0.35],mundurkiri:[-0.35,-1],mundurkanan:[-1,-0.35]},Se=null,he=!1;function ge(){if(!_(v)||he)return;let[m,y]=fe[_(v)];he=!0,S(`/api/gerak?l=${Math.round(m*_(l))}&r=${Math.round(y*_(l))}`,600).finally(()=>he=!1)}function gt(m){if(N(v,m,!0),ge(),Se)clearInterval(Se);Se=setInterval(ge,150)}function K(){if(Se)clearInterval(Se);if(Se=null,_(v))S("/api/stop");N(v,null)}function Ie(m,y){m.currentTarget.setPointerCapture(m.pointerId),gt(y)}let yt={ArrowUp:"maju",w:"maju",ArrowDown:"mundur",s:"mundur",ArrowLeft:"kiri",a:"kiri",ArrowRight:"kanan",d:"kanan"};function Wr(m){if(m.target.tagName==="INPUT")return;if(m.key===" "){m.preventDefault(),K(),S("/api/stop");return}let y=yt[m.key];if(y&&!m.repeat)m.preventDefault(),gt(y)}function Je(m){if(yt[m.key])K()}let Xr=0,Nr=null;function In(){let m=Date.now();if(Nr)clearTimeout(Nr);let y=()=>{Xr=Date.now(),S(`/api/kepala?p=${90-_(p)}&t=${_(d)}`)};if(m-Xr>150)y();else Nr=setTimeout(y,150)}function Zs(){N(p,0),N(d,90),In()}async function Js(){c=!0,await S(`/api/volume?v=${_(u)}`),setTimeout(()=>c=!1,1500)}async function Qs(m){m.preventDefault();let y=_(g).trim();if(!y)return;N(g,""),await S(`/api/bilang?t=${encodeURIComponent(y)}`,3000)}function Zr(m){setTimeout(()=>{if(!_(w)||!_(s)?.kamera){if(_(w))Zr(1000);return}N(A,`http://${_(s).kamera}/capture?res=vga&t=${Date.now()}`)},m)}function ea(m){let y=[];if(m&1)y.push("Tombol STOP terkunci");if(m&2)y.push("Baterai lemah");if(m&4)y.push("Halangan depan");if(m&8)y.push("Halangan belakang");if(m&16)y.push("Robot miring");return y}Ks(()=>{V();let m=setInterval(V,1000),y=()=>K(),C=()=>document.hidden&&K();return window.addEventListener("blur",y),document.addEventListener("visibilitychange",C),()=>{clearInterval(m),window.removeEventListener("blur",y),document.removeEventListener("visibilitychange",C),K()}}),$r(()=>{if(_(w))Zr(0)});var Mn=Yl();vt("keydown",Er,Wr),vt("keyup",Er,Je);var Ln=te(Mn),ti=te(Ln),ta=De(ti,!0),ri=D(ti,2);let ni;var oi=D(ri,2),ra=te(oi);{var na=(m)=>{var y=jr("otak tidak terhubung");z(m,y)},oa=(m)=>{var y=jr("badan tidak menjawab");z(m,y)},ia=(m)=>{var y=jr();Ae((C)=>ne(y,`${C??""} V · depan ${_(s).depan??""} cm · belakang ${_(s).belakang??""} cm`),[()=>_(s).baterai.toFixed(1)]),z(m,y)};ht(ra,(m)=>{if(!_(a))m(na);else if(!_(s)?.hidup)m(oa,1);else m(ia,-1)})}Y(oi),Y(Ln);var ii=D(Ln,2);{var sa=(m)=>{var y=Ol();zr(y,20,()=>ea(_(s).flag),(C)=>C,(C,Me)=>{var at=Cl(),At=De(at,!0);Ae(()=>ne(At,Me)),z(C,at)}),Y(y),z(m,y)};ht(ii,(m)=>{if(_(s)&&_(s).flag)m(sa)})}var Fn=D(ii,2),si=te(Fn);{var aa=(m)=>{var y=Dl(),C=An(y);let Me;var at=D(C,2);{var At=(Vt)=>{var sr=ei(),Jn=De(sr);Ae(()=>ne(Jn,`Kamera tidak terhubung (${_(s).kamera??""})`)),z(Vt,sr)};ht(at,(Vt)=>{if(_(h))Vt(At)})}Ae(()=>{Kr(C,"src",_(A)),Me=nr(C,1,"svelte-2gk0ybm4y39io",null,Me,{redup:_(h)})}),vt("load",C,()=>{N(h,!1),Zr(100)}),vt("error",C,()=>{N(h,!0),Zr(1500)}),Yo(C),z(m,y)},la=(m)=>{var y=ei(),C=De(y,!0);Ae(()=>ne(C,_(s)?.kamera?"Kamera dimatikan":"Alamat kamera belum diketahui")),z(m,y)};ht(si,(m)=>{if(_(w)&&_(s)?.kamera)m(aa);else m(la,-1)})}var ai=D(si,2),fa=De(ai,!0);Y(Fn);var Pn=D(Fn,2),Jr=D(te(Pn),2);zr(Jr,20,()=>[["majukiri","↖"],["maju","▲"],["majukanan","↗"],["kiri","◀"],["stop","■"],["kanan","▶"],["mundurkiri","↙"],["mundur","▼"],["mundurkanan","↘"]],([m,y])=>m,(m,y)=>{var C=xo(()=>no(y,2));let Me=()=>_(C)[0],at=()=>_(C)[1];var At=Vo(),Vt=An(At);{var sr=(ar)=>{var Qe=Il(),Qr=De(Qe,!0);Ae(()=>ne(Qr,at())),we("pointerdown",Qe,()=>{K(),S("/api/stop")}),z(ar,Qe)},Jn=(ar)=>{var Qe=Ml();let Qr;var ga=De(Qe,!0);Ae(()=>{Qr=nr(Qe,1,"svelte-2gk0ybm4y39io",null,Qr,{aktif:_(v)===Me()}),ne(ga,at())}),we("pointerdown",Qe,(ya)=>Ie(ya,Me())),we("pointerup",Qe,K),vt("pointercancel",Qe,K),vt("lostpointercapture",Qe,K),z(ar,Qe)};ht(Vt,(ar)=>{if(Me()==="stop")ar(sr);else ar(Jn,-1)})}z(m,At)}),Y(Jr);var li=D(Jr,2),fi=te(li),ci=D(fi);or(ci),Y(li),Y(Pn);var Yn=D(Pn,2),Bn=D(te(Yn),2),ui=te(Bn),Hn=D(ui);or(Hn),Y(Bn);var Vn=D(Bn,2),pi=te(Vn),Un=D(pi);or(Un),Y(Vn);var ca=D(Vn,2);Y(Yn);var jn=D(Yn,2),qn=D(te(jn),2),di=te(qn),_i=D(di,2),ua=D(_i,2);Y(qn);var Gn=D(qn,2),mi=te(Gn),zn=D(mi);or(zn),Y(Gn);var Kn=D(Gn,2),vi=te(Kn);or(vi),dr(2),Y(Kn),Y(jn);var Wn=D(jn,2),Xn=te(Wn),pa=D(te(Xn));{var da=(m)=>{var y=Ll(),C=De(y);Ae(()=>ne(C,`terakhir menjawab: ${_(s).aiTerakhir??""}`)),z(m,y)};ht(pa,(m)=>{if(_(s)?.aiTerakhir)m(da)})}Y(Xn);var Zn=D(Xn,2);zr(Zn,21,()=>r,(m)=>m.id,(m,y)=>{var C=Fl();let Me;var at=te(C),At=De(at,!0),Vt=D(at,2),sr=De(Vt,!0);Y(C),Ae(()=>{C.disabled=!_(s)?.aiTersedia?.[_(y).id]||_(n)!=="",Me=nr(C,1,"svelte-2gk0ybm4y39io",null,Me,{pilih:_(s)?.ai===_(y).id}),ne(At,_(y).nama),ne(sr,_(s)?.aiTersedia?.[_(y).id]===!1?"API key belum diisi":_(y).ket)}),we("click",C,()=>i(_(y).id)),z(m,C)}),Y(Zn);var _a=D(Zn,2);{var ma=(m)=>{var y=ei(),C=De(y,!0);Ae(()=>ne(C,_(o))),z(m,y)};ht(_a,(m)=>{if(_(o))m(ma)})}dr(2),Y(Wn);var va=D(Wn,2);{var ha=(m)=>{var y=Pl(),C=De(y,!0);Ae(()=>ne(C,_(s).status)),z(m,y)};ht(va,(m)=>{if(_(s)?.status)m(ha)})}Y(Mn),Ae(()=>{ne(ta,_(s)?.nama??"Robot"),ni=nr(ri,1,"titik svelte-2gk0ybm4y39io",null,ni,{on:_(a)&&_(s)?.hidup}),ne(fa,_(w)?"Matikan kamera":"Nyalakan kamera"),ne(fi,`Kecepatan ${_(l)??""}`),ne(ui,`Toleh ${_(p)<0?"kiri":_(p)>0?"kanan":"lurus"}`),ne(pi,`Angguk ${_(d)??""}°`),ne(mi,`Volume ${_(u)??""}`)}),we("click",ai,()=>N(w,!_(w))),we("contextmenu",Jr,(m)=>m.preventDefault()),ir(ci,()=>_(l),(m)=>N(l,m)),we("input",Hn,In),ir(Hn,()=>_(p),(m)=>N(p,m)),we("input",Un,In),ir(Un,()=>_(d),(m)=>N(d,m)),we("click",ca,Zs),we("click",di,()=>S("/api/lagu",4000)),we("click",_i,()=>S("/api/radio",4000)),we("click",ua,()=>S("/api/diam")),we("change",zn,Js),ir(zn,()=>_(u),(m)=>N(u,m)),vt("submit",Kn,Qs),ir(vi,()=>_(g),(m)=>N(g,m)),z(e,Mn),Ir()}var Xs=Bl;Rn(["click","contextmenu","pointerdown","pointerup","input","change"]);Sr(Xs,{target:document.body});
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
    d["ai"] = namaAI[pilihanAI]; d["aiTerakhir"] = aiTerakhir;
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
  web.on("/api/stop", HTTP_GET, [] { kirimBadan("S"); arahMata = 0; jawabOk(); });
  web.on("/api/kepala", HTTP_GET, [] {
    kirimBadan("P " + String(argAngka("p", 90, 0, 180)) + " " + String(argAngka("t", 90, 0, 180)));
    jawabOk();
  });
  web.on("/api/ai", HTTP_GET, [] {
    if (pilihAI(web.arg("m"))) { setStatus(String("Otak AI: ") + namaAI[pilihanAI]); jawabOk(); }
    else web.send(400, "text/plain", "model tidak dikenal atau API key-nya belum diisi");
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
// ======================= DIAGNOSIS LAYAR (dicetak di Serial Monitor saat menyala) =======================
// Uji tiap kabel layar sebelum layar dinyalakan: pin dibaca dengan penarik ke atas lalu ke bawah.
//   "1/0" = mengikuti penarik: kabel tidak terhubung singkat (normal)
//   "0/0" = terhubung ke GND, "1/1" = terhubung ke 3,3 V (CS dengan resistor penarik di modul juga 1/1, itu normal)
void diagPinLayar() {
  struct { const char* nama; int pin; } p[] = {{"SCK  IO12", TFT_SCK}, {"MOSI IO11", TFT_MOSI}, {"MISO IO13", TFT_MISO},
    {"CS   IO10", TFT_CS}, {"DC   IO9 ", TFT_DC}, {"RST  IO8 ", TFT_RST}, {"T_CS IO14", TOUCH_CS}, {"SDCS IO21", SD_CS}};
  Serial.printf("[layar] alasan menyala: %d (1 = saklar/daya baru, 3 = RST/upload, 9 = brownout)\n", (int)esp_reset_reason());
  for (auto& x : p) {
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

  pref.begin("robot", false);
  uint8_t arah = ARAH_LAYAR, geo = pref.getUChar("geo", 1), rgb = pref.getUChar("rgb", 1);   // geo dan rgb: pilihan di tes_layar
  if (pref.getUChar("rot", 255) != arah) pref.putUChar("rot", arah);   // tes_media, tes_sd, tes_musik ikut arah ini
  pilihanAI = pref.getUChar("ai", AI_OTOMATIS);                         // otak AI pilihan terakhir (panel web / /ai)
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
  lcd.setBrightness(255); lcd.fillScreen(TFT_BLACK);
  Serial.printf("[layar] arah %d, ukuran %d x %d\n", lcd.getRotation(), (int)lcd.width(), (int)lcd.height());
  int32_t tx, ty; bool paksa = lcd.getTouchRaw(&tx, &ty);    // sentuh layar saat menyala = kalibrasi ulang
  if (paksa) Serial.printf("[layar] sentuhan terbaca saat menyala (x %d, y %d): masuk kalibrasi ulang\n", (int)tx, (int)ty);
  kalibrasiSentuh(paksa);
  wajah.setPsram(true); wajah.setColorDepth(16); wajah.createSprite(320, 186);
  gambarTombol();

  SPI.begin(TFT_SCK, TFT_MISO, TFT_MOSI);
  diagSentuh();
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
