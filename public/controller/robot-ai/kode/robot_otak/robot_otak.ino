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
#include <time.h>
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
const uint8_t ARAH_LAYAR    = 6;                      // 6 = slot SD di atas, 4 = slot SD di bawah. Tampilan terbalik? ganti 6 <-> 4
#define LAYAR_MHZ        27                       // kecepatan kirim ke layar. Layar putih/bergaris dengan kabel panjang? coba 10
#define ZONA_WAKTU       "WITA-8"                 // jam di layar: WIB-7 (Jawa, Sumatra), WITA-8 (Bali, NTB, Sulawesi), WIT-9 (Papua)
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

#ifndef LAYAR_MHZ                                     // kalau baris LAYAR_MHZ tidak ada di pengaturan atas
#define LAYAR_MHZ 27
#endif
#ifndef ZONA_WAKTU
#define ZONA_WAKTU "WITA-8"
#endif

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

// Sensor suhu/lembap tidak wajib: badan mengirim -1 kalau tidak ada.
bool adaSensorSuhu() { return badan.suhu > -40 && fabsf(badan.suhu + 1) > 0.05f; }

String ringkasSensor() {
  char b[200];
  bool hidup = badan.terakhir && millis() - badan.terakhir < 2000;
  if (!hidup) return "badan tidak terhubung";
  char cuaca[40] = "";
  if (adaSensorSuhu()) snprintf(cuaca, sizeof(cuaca), ", suhu %.1f C, lembap %.0f%%", badan.suhu, badan.lembap);
  snprintf(b, sizeof(b), "jarak depan %d cm, belakang %d cm, baterai %.2f V%s, miring %d/%d derajat%s",
           badan.depan, badan.belakang, badan.baterai, cuaca, badan.pitch, badan.roll,
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
    "Data sensor terbaru: " + ringkasSensor() + "." +
    (teksJam().length() ? String(" Sekarang ") + teksTanggal() + ", jam " + teksJam() + "." : String());
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
  "/panel  alamat panel kontrol web\n/dengar on|off  mendengar terus (boros) atau hanya saat BICARA\n/ai  pilih otak AI (gemini, gemma, groq, haiku)\n/lagu  daftar lagu di SD\n/putar nama lagu (cukup sebagian)\n/radio url\n/diam  hentikan suara\n/volume 0-21\n"
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
  if (hidup && adaSensorSuhu()) snprintf(atas, sizeof(atas), "%.1fV  depan %dcm  %.0fC", badan.baterai, badan.depan, badan.suhu);
  else if (hidup) snprintf(atas, sizeof(atas), "%.1fV  depan %dcm", badan.baterai, badan.depan);
  else snprintf(atas, sizeof(atas), "badan belum terhubung");
  wajah.drawString(atas, 4, 2);
  wajah.setTextDatum(top_right);
  String jam = teksJam();                                   // jam di kanan atas, tanggal di baris kedua
  wajah.drawString(jam.length() ? jam.c_str() : (WiFi.status() == WL_CONNECTED ? "WiFi" : "no WiFi"), 316, 2);
  if (jam.length()) { wajah.setTextDatum(top_center); wajah.drawString(teksTanggal().c_str(), 160, 20); }

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
const char PANEL_HTML[] = R"PANEL(<!doctype html><html lang="id"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1"><title>Panel Zond</title><style>html{color:#e6e9ee;background:#10141a;font-family:system-ui,-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,Noto Sans,Ubuntu,Cantarell,Helvetica Neue,sans-serif}body{margin:0}main.svelte-194c7efawxkxi{max-width:480px;margin:0 auto;padding:12px 16px 32px}header.svelte-194c7efawxkxi{display:flex;flex-wrap:wrap;align-items: center;gap:8px}h1.svelte-194c7efawxkxi{margin:0;font-size:1.4rem}h2.svelte-194c7efawxkxi{margin:0 0 8px;font-size:1rem}h2.svelte-194c7efawxkxi small:where(.svelte-194c7efawxkxi){color:#8a94a3;font-weight:400}.kecil.svelte-194c7efawxkxi{color:#8a94a3;font-size:.85rem}.titik.svelte-194c7efawxkxi{background:#c0392b;border-radius:50%;width:10px;height:10px}.titik.on.svelte-194c7efawxkxi{background:#2ecc71}.peringatan.svelte-194c7efawxkxi{display:flex;flex-wrap:wrap;gap:6px;margin-top:8px}.peringatan.svelte-194c7efawxkxi span:where(.svelte-194c7efawxkxi){background:#7a1d1d;border-radius:6px;padding:3px 8px;font-size:.85rem}section.svelte-194c7efawxkxi{background:#18202a;border-radius:12px;margin-top:12px;padding:12px}.kamera.svelte-194c7efawxkxi{text-align:center}.kamera.svelte-194c7efawxkxi img:where(.svelte-194c7efawxkxi){display:block;aspect-ratio:4/3;object-fit:cover;background:#000;border-radius:8px;width:100%}.kamera.svelte-194c7efawxkxi img.redup:where(.svelte-194c7efawxkxi){opacity:.15}.ket.svelte-194c7efawxkxi{color:#8a94a3;margin:8px 0}.dpad.svelte-194c7efawxkxi{display:grid;grid-template-columns:repeat(3,1fr);touch-action:none;user-select:none;-webkit-user-select:none;gap:8px;max-width:300px;margin:0 auto 12px}.dpad.svelte-194c7efawxkxi button:where(.svelte-194c7efawxkxi){aspect-ratio:1;touch-action:none;background:#2a3a4d;border-radius:14px;font-size:1.6rem}.dpad.svelte-194c7efawxkxi button.aktif:where(.svelte-194c7efawxkxi){background:#3d7bd9}.dpad.svelte-194c7efawxkxi button.stop:where(.svelte-194c7efawxkxi){background:#a0302a}button.svelte-194c7efawxkxi{color:inherit;cursor:pointer;background:#2a3a4d;border:0;border-radius:10px;padding:10px 12px;font-size:.95rem}button.svelte-194c7efawxkxi:active{filter:brightness(1.3)}.kecil-btn.svelte-194c7efawxkxi{margin-top:8px;padding:6px 10px;font-size:.85rem}.baris.svelte-194c7efawxkxi{display:flex;flex-wrap:wrap;gap:8px}.baris.svelte-194c7efawxkxi button:where(.svelte-194c7efawxkxi){flex:1}label.svelte-194c7efawxkxi{display:block;margin-top:10px;font-size:.9rem}input[type=range].svelte-194c7efawxkxi{width:100%}form.svelte-194c7efawxkxi{display:flex;gap:8px;margin-top:12px}input[type=text].svelte-194c7efawxkxi{color:inherit;background:#10141a;border:1px solid #2a3a4d;border-radius:10px;flex:1;min-width:0;padding:10px;font-size:1rem}.mik.svelte-194c7efawxkxi{background:#1f6f3a;border-radius:14px;width:100%;padding:16px;font-size:1.1rem;font-weight:600}.mik.dengar.svelte-194c7efawxkxi{animation:1s ease-in-out infinite svelte-194c7efawxkxi-denyut;background:#c2410c}@keyframes svelte-194c7efawxkxi-denyut{50%{filter:brightness(1.3)}}.mik.svelte-194c7efawxkxi:disabled{opacity:.5}.saklar.svelte-194c7efawxkxi{display:flex;color:#8a94a3;align-items: flex-start;gap:8px;font-size:.8rem}.saklar.svelte-194c7efawxkxi input:where(.svelte-194c7efawxkxi){margin-top:2px}.ai.svelte-194c7efawxkxi{display:grid;grid-template-columns:1fr 1fr;gap:8px}.ai.svelte-194c7efawxkxi button:where(.svelte-194c7efawxkxi){text-align:left;display:flex;flex-direction:column;gap:2px}.ai.svelte-194c7efawxkxi button:where(.svelte-194c7efawxkxi):first-child{grid-column:1/-1}.ai.svelte-194c7efawxkxi button:where(.svelte-194c7efawxkxi) span:where(.svelte-194c7efawxkxi){color:#8a94a3;font-size:.75rem}.ai.svelte-194c7efawxkxi button.pilih:where(.svelte-194c7efawxkxi){outline:2px solid #5fcf72;background:#2f6a3a}.ai.svelte-194c7efawxkxi button.pilih:where(.svelte-194c7efawxkxi) span:where(.svelte-194c7efawxkxi){color:#cfe9d4}.ai.svelte-194c7efawxkxi button:where(.svelte-194c7efawxkxi):disabled{opacity:.45;cursor:default}.kecil-ket.svelte-194c7efawxkxi{margin-bottom:0;font-size:.8rem}.status.svelte-194c7efawxkxi{text-align:center;color:#8a94a3;margin-top:12px}
</style></head><body><script type="module">var c=!1;var bt=Array.isArray,ki=Array.prototype.indexOf,yt=Array.prototype.includes,lr=Array.from,{keys:no,defineProperty:ie,getOwnPropertyDescriptor:Fe,getOwnPropertyDescriptors:oo,prototype:io}=Object,Ti=Array.prototype,{getPrototypeOf:cr,isExtensible:so}=Object;var ct=()=>{};function tn(e){for(var t=0;t<e.length;t++)e[t]()}function rn(){var e,t,r=new Promise((n,o)=>{e=n,t=o});return{promise:r,resolve:e,reject:t}}function ao(e,t){if(Array.isArray(e))return e;if(t===void 0||!(Symbol.iterator in e))return Array.from(e);let r=[];for(let n of e)if(r.push(n),r.length===t)break;return r}var U=2,Et=4,Nt=8,Gt=16777216,pe=16,se=32,Ce=64,ur=128,nn=256,Ee=512,M=1024,O=2048,q=4096,Z=8192,F=16384,$e=32768,$t=33554432,Oe=65536,Rt=131072,fo=262144,ut=524288,Ai=1048576,rt=33554432,pr=2097152,Pe=4194304,Ge=8388608,Be=Symbol("$state"),on=Symbol("component"),sn=Symbol("legacy props"),Si=Symbol(""),an=Symbol("proxy path"),fn=Symbol("attributes"),Or=Symbol("class"),lo=Symbol("style"),Dr=Symbol("text"),Ct=Symbol("form reset"),co=Symbol("hmr anchor"),kt=new class extends Error{name="StaleReactionError";message="The reaction that called `getAbortSignal()` was re-run or destroyed"},uo=!!globalThis.document?.contentType&&globalThis.document.contentType.includes("xml");var zt=3,ze=8;var Ni=1,Ri=2,po=4,Ci=8,Oi=16;var Di=1,Ii=2;var dr="[",Ot="[!",_o="[?",Kt="]",Ye={};var P=Symbol("uninitialized"),Ke=Symbol("filename"),mo=Symbol("hmr"),ln="http://www.w3.org/1999/xhtml";var Dt="font-weight: bold",It="font-weight: normal";function Mi(e){if(c)console.warn(`%c[svelte] await_reactivity_loss
%cDetected reactivity loss when reading \`${e}\`. This happens when state is read in an async function after an earlier \`await\`
https://svelte.dev/e/await_reactivity_loss`,Dt,It);else console.warn("https://svelte.dev/e/await_reactivity_loss")}function Li(e,t){if(c)console.warn(`%c[svelte] await_waterfall
%cAn async derived, \`${e}\` (${t}) was not read immediately after it resolved. This often indicates an unnecessary waterfall, which can slow down your app
https://svelte.dev/e/await_waterfall`,Dt,It);else console.warn("https://svelte.dev/e/await_waterfall")}function Fi(){if(c)console.warn(`%c[svelte] derived_inert
%cReading a derived belonging to a now-destroyed effect may result in stale values
https://svelte.dev/e/derived_inert`,Dt,It);else console.warn("https://svelte.dev/e/derived_inert")}function Pi(e,t,r){if(c)console.warn(`%c[svelte] hydration_attribute_changed
%cThe \`${e}\` attribute on \`${t}\` changed its value between server and client renders. The client value, \`${r}\`, will be ignored in favour of the server value
https://svelte.dev/e/hydration_attribute_changed`,Dt,It);else console.warn("https://svelte.dev/e/hydration_attribute_changed")}function Wt(e){if(c)console.warn(`%c[svelte] hydration_mismatch
%c${e?`Hydration failed because the initial UI does not match what was rendered on the server. The error occurred near ${e}`:"Hydration failed because the initial UI does not match what was rendered on the server"}
https://svelte.dev/e/hydration_mismatch`,Dt,It);else console.warn("https://svelte.dev/e/hydration_mismatch")}function Bi(){if(c)console.warn(`%c[svelte] lifecycle_double_unmount
%cTried to unmount a component that was not mounted
https://svelte.dev/e/lifecycle_double_unmount`,Dt,It);else console.warn("https://svelte.dev/e/lifecycle_double_unmount")}function cn(e){if(c)console.warn(`%c[svelte] state_proxy_equality_mismatch
%cReactive \`$state(...)\` proxies and the values they proxy have different identities. Because of this, comparisons with \`${e}\` will produce unexpected results
https://svelte.dev/e/state_proxy_equality_mismatch`,Dt,It);else console.warn("https://svelte.dev/e/state_proxy_equality_mismatch")}function Yi(){if(c)console.warn("%c[svelte] svelte_boundary_reset_noop\n%cA `<svelte:boundary>` `reset` function only resets the boundary the first time it is called\nhttps://svelte.dev/e/svelte_boundary_reset_noop",Dt,It);else console.warn("https://svelte.dev/e/svelte_boundary_reset_noop")}var w=!1;function Q(e){w=e}var A;function I(e){if(e===null)throw Wt(),Ye;return A=e}function xe(){return I(ae(A))}function B(e){if(!w)return;if(ae(A)!==null)throw Wt(),Ye;A=e}function _r(e=1){if(w){var t=e,r=A;while(t--)r=ae(r);A=r}}function pt(e=!0){var t=0,r=A;while(!0){if(r.nodeType===ze){var n=r.data;if(n===Kt){if(t===0)return r;t-=1}else if(n===dr||n===Ot||n[0]==="["&&!isNaN(Number(n.slice(1))))t+=1}var o=ae(r);if(e)r.remove();r=o}}function Xt(e){if(!e||e.nodeType!==ze)throw Wt(),Ye;return e.data}function un(e){return e===this.v}function vo(e,t){return e!=e?t==t:e!==t||e!==null&&typeof e==="object"||typeof e==="function"}function pn(e){return!vo(e,this.v)}function Hi(e){if(c){let t=Error(`invariant_violation
An invariant violation occurred, meaning Svelte's internal assumptions were flawed. This is a bug in Svelte, not your app — please open an issue at https://github.com/sveltejs/svelte, citing the following message: "${e}"
https://svelte.dev/e/invariant_violation`);throw t.name="Svelte error",t}else throw Error("https://svelte.dev/e/invariant_violation")}function dn(e){if(c){let t=Error(`lifecycle_outside_component
\`${e}(...)\` can only be used during component initialisation
https://svelte.dev/e/lifecycle_outside_component`);throw t.name="Svelte error",t}else throw Error("https://svelte.dev/e/lifecycle_outside_component")}function Ui(){if(c){let e=Error("async_derived_orphan\nCannot create a `$derived(...)` with an `await` expression outside of an effect tree\nhttps://svelte.dev/e/async_derived_orphan");throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/async_derived_orphan")}function ho(){if(c){let e=Error("bind_invalid_checkbox_value\nUsing `bind:value` together with a checkbox input is not allowed. Use `bind:checked` instead\nhttps://svelte.dev/e/bind_invalid_checkbox_value");throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/bind_invalid_checkbox_value")}function ji(){if(c){let e=Error(`derived_references_self
A derived value cannot reference itself recursively
https://svelte.dev/e/derived_references_self`);throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/derived_references_self")}function go(e,t,r){if(c){let n=Error(`each_key_duplicate
${r?`Keyed each block has duplicate key \`${r}\` at indexes ${e} and ${t}`:`Keyed each block has duplicate key at indexes ${e} and ${t}`}
https://svelte.dev/e/each_key_duplicate`);throw n.name="Svelte error",n}else throw Error("https://svelte.dev/e/each_key_duplicate")}function qi(e,t,r){if(c){let n=Error(`each_key_volatile
Keyed each block has key that is not idempotent — the key for item at index ${e} was \`${t}\` but is now \`${r}\`. Keys must be the same each time for a given item
https://svelte.dev/e/each_key_volatile`);throw n.name="Svelte error",n}else throw Error("https://svelte.dev/e/each_key_volatile")}function Gi(e){if(c){let t=Error(`effect_in_teardown
\`${e}\` cannot be used inside an effect cleanup function
https://svelte.dev/e/effect_in_teardown`);throw t.name="Svelte error",t}else throw Error("https://svelte.dev/e/effect_in_teardown")}function zi(){if(c){let e=Error("effect_in_unowned_derived\nEffect cannot be created inside a `$derived` value that was not itself created inside an effect\nhttps://svelte.dev/e/effect_in_unowned_derived");throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/effect_in_unowned_derived")}function Ki(e){if(c){let t=Error(`effect_orphan
\`${e}\` can only be used inside an effect (e.g. during component initialisation)
https://svelte.dev/e/effect_orphan`);throw t.name="Svelte error",t}else throw Error("https://svelte.dev/e/effect_orphan")}function Wi(){if(c){let e=Error(`effect_update_depth_exceeded
Maximum update depth exceeded. This typically indicates that an effect reads and writes the same piece of state
https://svelte.dev/e/effect_update_depth_exceeded`);throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/effect_update_depth_exceeded")}function Xi(){if(c){let e=Error(`hydration_failed
Failed to hydrate the application
https://svelte.dev/e/hydration_failed`);throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/hydration_failed")}function Zi(e){if(c){let t=Error(`rune_outside_svelte
The \`${e}\` rune is only available inside \`.svelte\` and \`.svelte.js/ts\` files
https://svelte.dev/e/rune_outside_svelte`);throw t.name="Svelte error",t}else throw Error("https://svelte.dev/e/rune_outside_svelte")}function Ji(){if(c){let e=Error("state_descriptors_fixed\nProperty descriptors defined on `$state` objects must contain `value` and always be `enumerable`, `configurable` and `writable`.\nhttps://svelte.dev/e/state_descriptors_fixed");throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/state_descriptors_fixed")}function Qi(){if(c){let e=Error("state_prototype_fixed\nCannot set prototype of `$state` object\nhttps://svelte.dev/e/state_prototype_fixed");throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/state_prototype_fixed")}function es(){if(c){let e=Error("state_unsafe_mutation\nUpdating state inside `$derived(...)`, `$inspect(...)` or a template expression is forbidden. If the value should not be reactive, declare it without `$state`\nhttps://svelte.dev/e/state_unsafe_mutation");throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/state_unsafe_mutation")}function ts(){if(c){let e=Error("svelte_boundary_reset_onerror\nA `<svelte:boundary>` `reset` function cannot be called while an error is still being handled\nhttps://svelte.dev/e/svelte_boundary_reset_onerror");throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/svelte_boundary_reset_onerror")}var ee=!1,Mt=!1,nt=!1;var Ir=null;function we(e,t){return e.label=t,mn(e.v,t),e}function mn(e,t){return e?.[an]?.(t),e}function ot(e){let t=Error(),r=Oa();if(r.length===0)return null;return r.unshift(`
`),ie(t,"stack",{value:r.join(`
`)}),ie(t,"name",{value:e}),t}function Oa(){let e=Error.stackTraceLimit;Error.stackTraceLimit=1/0;let t=Error().stack;if(Error.stackTraceLimit=e,!t)return[];let r=t.split(`
`),n=[];for(let o=0;o<r.length;o++){let i=r[o],s=i.replaceAll("\\","/");if(i.trim()==="Error")continue;if(i.includes("validate_each_keys"))return[];if(s.includes("svelte/src/internal")||s.includes("node_modules/.vite"))continue;n.push(i)}return n}function ns(e,t){if(!c)throw Error("invariant(...) was not guarded by if (DEV)");if(!e)Hi(t)}var N=null;function Tt(e){N=e}var dt=null;function mr(e){dt=e}var it=null;function vn(e){it=e}function Mr(e,t=!1,r){if(N={p:N,i:!1,c:null,e:null,s:e,x:null,r:b,l:Mt&&!t?{s:null,u:null,$:[]}:null},c)N.function=r,it=r}function Lr(e){var t=N,r=t.e;if(r!==null){t.e=null;for(var n of r)xo(n)}if(e!==void 0)t.x=e;if(t.i=!0,N=t.p,c)it=N?.function??null;return hn(e)}function hn(e={}){return ie(e,on,{value:!0}),e}function _t(){return!Mt||N!==null&&N.l===null}var Zt=[];function os(){var e=Zt;Zt=[],tn(e)}function J(e){if(Zt.length===0&&!Jt){var t=Zt;queueMicrotask(()=>{if(t===Zt)os()})}Zt.push(e)}function is(){while(Zt.length>0)os()}var Da=~(O|q|M);function L(e,t){e.f=e.f&Da|t}function vr(e){if((e.f&Ee)!==0||e.deps===null)L(e,M);else L(e,q)}function gn(e,t){if((e.f&O)!==0)t.set(e,O);else if((e.f&q)!==0&&t.get(e)!==O)t.set(e,q);L(e,M)}function ss(e){return`{${e.toString().replace(/^async \(\) => /,"").replace(/^\(/,"").replace(/\$\.get\((.+?)\)/g,(t,r)=>r).replace(/\$\.track_reactivity_loss\((.+?)\)\(\)/g,(t,r)=>r).replace(/\)$/,"")}}`}var as=!1;function xn(){if(!as)as=!0,document.addEventListener("reset",(e)=>{Promise.resolve().then(()=>{if(!e.defaultPrevented)for(let t of e.target.elements)t[Ct]?.()})},{capture:!0})}function He(e){var t=E,r=b;te(null),re(null);try{return e()}finally{te(t),re(r)}}function wo(e,t,r,n=r){e.addEventListener(t,()=>He(r));let o=e[Ct];if(o)e[Ct]=()=>{o(),n(!0)};else e[Ct]=()=>n(!0);xn()}function wn(e,t,r,n){let o=_t()?gr:xr;var i=e.filter((f)=>!f.settled),s=t.map(o);if(c)s.forEach((f,g)=>{f.label=t[g].toString().replace("() => ","").replaceAll("$.eager(() => ","$state.eager(").replace(/\$\.get\((.+?)\)/g,(y,h)=>h)});if(r.length===0&&i.length===0){n(s);return}var a=b,l=fs(),u=i.length===1?i[0].promise:i.length>1?Promise.all(i.map((f)=>f.promise)):null;function _(f){if((a.f&F)!==0)return;l();try{n([...s,...f])}catch(g){De(g,a)}hr()}var m=bo();if(r.length===0){u.then(()=>_([])).finally(m);return}function p(){Promise.all(r.map((f)=>Eo(f))).then(_).catch((f)=>De(f,a)).finally(m)}if(u)u.then(()=>{if((a.f&F)!==0){m();return}l(),p(),hr()});else p()}function fs(){var e=b,t=E,r=N,n=k;if(c)var o=dt;return function(s=!0){if(re(e),te(t),Tt(r),s&&(e.f&F)===0)n?.activate(),n?.apply();if(c)yo(null),mr(o)}}var Ia=!1;function hr(e=!0){if(Ia=!1,re(null),te(null),Tt(null),e)k?.deactivate();if(c)yo(null),mr(null)}function bo(){var e=b,t=e.b,r=k,n=!!t?.is_rendered();return t?.update_pending_count(1,r),r.increment(n,e),()=>{t?.update_pending_count(-1,r),r.decrement(n,e)}}var Ie=null;function yo(e){Ie=e}var Pr=new Set;function gr(e){var t=U|O;if(b!==null)b.f|=ut;let r={ctx:N,deps:null,effects:null,equals:un,f:t,fn:e,reactions:null,rv:0,v:P,wv:0,parent:b,ac:null};if(c&&nt)r.created=ot("created at");return r}var wr=Symbol("obsolete");function Eo(e,t,r){let n=b;if(n===null)Ui();var o=void 0,i=be(P);if(c)i.label=t??ss(e);var s=!E,a=new Set;if(us(()=>{var l=b;if(c)Ie={effect:l,effect_deps:new Set,warned:!1};var u=rn();o=u.promise;try{Promise.resolve(e()).then(u.resolve,(f)=>{if(f!==kt)u.reject(f)}).finally(hr)}catch(f){u.reject(f),hr()}if(c){if(Ie){if(l.deps!==null)for(let f=0;f<ke;f+=1)Ie.effect_deps.add(l.deps[f]);if(fe!==null)for(let f=0;f<fe.length;f+=1)Ie.effect_deps.add(fe[f])}Ie=null}var _=k;if(s){if((l.f&$e)!==0)var m=bo();if(n.b?.is_rendered())_.async_deriveds.get(l)?.reject(wr);else for(let f of a.values())f.reject(wr);a.add(u),_.async_deriveds.set(l,u)}let p=(f,g=void 0)=>{if(c)Ie=null;if(m?.(),a.delete(u),g===wr)return;if(_.activate(),g)i.f|=Ge,st(i,g);else{if((i.f&Ge)!==0)i.f^=Ge;if(c&&r!==void 0&&!i.equals(f))Pr.add(i),setTimeout(()=>{if(Pr.has(i)&&(l.f&F)===0)Li(i.label,r),Pr.delete(i)});st(i,f)}_.deactivate()};u.promise.then(p,(f)=>p(null,f||"unknown"))}),ve(()=>{for(let l of a)l.reject(wr)}),c)i.f|=Pe;return new Promise((l)=>{function u(_){function m(){if(_===o)l(i);else u(o)}_.then(m,m)}u(o)})}function ko(e){let t=gr(e);if(!ee)En(t);return t}function xr(e){let t=gr(e);return t.equals=pn,t}function ls(e){var t=e.effects;if(t!==null){e.effects=null;for(var r=0;r<t.length;r+=1)G(t[r])}}var $o=[];function Br(e){var t,r=b,n=e.parent;if(!Xe&&n!==null&&e.v!==P&&(n.f&(F|Z))!==0)return Fi(),e.v;if(re(n),c){let o=Qt;bn(new Set);try{if(yt.call($o,e))ji();$o.push(e),ls(e),t=yn(e)}finally{re(r),bn(o),$o.pop()}}else try{ls(e),t=yn(e)}finally{re(r)}return t}function To(e){var t=Br(e);if(!e.equals(t)){if(e.wv=br(),!k?.is_fork||e.deps===null){if(k!==null)k.capture(e,t,!0),We?.capture(e,t,!0);else e.v=t;if(e.deps===null){L(e,M);return}}}if(Xe)return;if(le!==null){if(Lt()||k?.is_fork)le.set(e,t)}else vr(e)}function cs(e){if(e.effects===null)return;for(let t of e.effects)if(t.teardown||t.ac){if(t.teardown?.(),t.ac!==null)He(()=>{t.ac.abort(kt),t.ac=null});if(t.fn!==null)t.teardown=ct;er(t,0),Yr(t)}}function Ao(e){if(e.effects===null)return;for(let t of e.effects)if(t.teardown&&t.fn!==null)mt(t)}var $n=null,yr=null,k=null,We=null,le=null,Ro=null,Jt=!1,So=!1,Er=null,Hr=null,ps=0,No=new Set,Fa=1;class he{id=Fa++;started=!1;linked=!0;prev=null;next=null;async_deriveds=new Map;current=new Map;previous=new Map;#e=new Set;#t=new Set;#r=0;#a=new Map;#o=null;#n=[];#s=[];#i=new Map;#f=new Map;#d=new Set;is_fork=!1;#l=!1;constructor(){if(yr===null)$n=yr=this;else yr.next=this,this.prev=yr;yr=this}#m(){if(this.is_fork)return!0;for(let e of this.#a.keys())if(!this.#g(e))return!0;return!1}#g(e){for(var t=e;t!==null;t=t.parent)if(this.#f.has(t))return!0;return!1}skip_effect(e){if(!this.#f.has(e))this.#f.set(e,{d:[],m:[]});this.#d.delete(e)}unskip_effect(e,t=(r)=>this.schedule(r)){var r=this.#f.get(e);if(r){this.#f.delete(e);for(var n of r.d)L(n,O),t(n);for(n of r.m)L(n,q),t(n)}this.#d.add(e)}#c(){var e=[];for(let o of this.#n){if((o.f&F)!==0||(o.f&(O|q))===0)continue;var t=o,r=!1;while(t.parent!==null){t=t.parent;var n=t.f;if((n&(Ce|se))!==0){if((n&M)===0){r=!0;break}t.f^=M}}if(!r)e.push(t)}return this.#n=[],e}#v(){if(this.started=!0,c)for(let s of this.current.keys())No.add(s);for(let[s,a]of this.#i)L(s,a),this.schedule(s);this.apply();var e=Er=[],t=[],r=Hr=[];while(this.#n.length>0){if(ps++>1000)this.#p(),Pa();for(let s of this.#c())try{this.#x(s,e,t)}catch(a){if(vs(s),!this.#m())this.discard();throw a}}if(k=null,r.length>0){var n=he.ensure();for(let s of r)n.schedule(s)}if(Er=null,Hr=null,this.#m()){this.#_(t),this.#_(e);for(let[s,a]of this.#f)ms(s,a);if(r.length>0)k.#v();return}let o=this.#b();if(o){this.#_(t),this.#_(e),o.#u(this);return}this.#i.clear();for(let s of this.#e)s(this);this.#e.clear(),We=this,ds(t),ds(e),We=null,this.#o?.resolve();var i=k;if(this.#r===0&&(this.#n.length===0||i!==null)){if(this.#p(),ee)this.#w(),k=i}if(i!==null)Je.clear(),i.#v()}#x(e,t,r){e.f^=M;var n=e.first;while(n!==null){var o=n.f,i=(o&(se|Ce))!==0,s=i&&(o&M)!==0,a=s||(o&Z)!==0||this.#f.has(n);if(!a&&n.fn!==null){if(i)n.f^=M;else if((o&Et)!==0)t.push(n);else if(ee&&(o&(Nt|Gt))!==0)r.push(n);else if(Pt(n)){if((o&pe)!==0)this.add_dirty_reaction(n,q);mt(n)}var l=n.first;if(l!==null){n=l;continue}}while(n!==null){var u=n.next;if(u!==null){n=u;break}n=n.parent}}}#b(){for(var e=this.prev;e!==null;e=e.prev)if(!e.is_fork&&this.#h(e))return e;return null}#h(e){for(let[t,[,r]]of this.current){if(r)continue;if(e.current.has(t))return!0}return!1}#u(e){for(let[r,n]of e.current){if(!this.previous.has(r)&&e.previous.has(r))this.previous.set(r,e.previous.get(r));this.current.set(r,n)}for(let[r,n]of e.async_deriveds){let o=this.async_deriveds.get(r);if(o)n.promise.then(o.resolve).catch(o.reject)}e.async_deriveds.clear(),this.transfer_reactions(e.#i);let t=(r)=>{var n=r.reactions;if(n===null)return;if((r.f&U)!==0&&(r.f&(O|q))===0)return;for(let s of n){var o=s.f;if((o&U)!==0)t(s);else{var i=s;if(o&(Pe|pe)&&!this.async_deriveds.has(i))this.#i.delete(i),L(i,O),this.schedule(i)}}};for(let r of this.current.keys())t(r);this.oncommit(()=>e.discard()),e.#p(),k=this,this.#v()}#_(e){for(var t=0;t<e.length;t+=1)gn(e[t],this.#i)}capture(e,t,r=!1){if(e.v!==P&&!this.previous.has(e))this.previous.set(e,e.v);if((e.f&Ge)===0)this.current.set(e,[t,r]),le?.set(e,t);if(!this.is_fork)e.v=t}activate(){k=this}deactivate(){k=null,le=null}flush(){try{if(c)No.clear();So=!0,k=this,this.#v()}finally{if(ps=0,Ro=null,Er=null,Hr=null,So=!1,k=null,le=null,Je.clear(),c)for(let e of No)e.updated=null}}discard(){for(let e of this.#t)e(this);this.#t.clear();for(let e of this.async_deriveds.values())e.reject(wr);this.#p(),this.#o?.resolve()}register_created_effect(e){this.#s.push(e)}#w(){for(let _=$n;_!==null;_=_.next){var e=_.id<this.id,t=[];for(let[m,[p,f]]of this.current){if(_.current.has(m)){var r=_.current.get(m)[0];if(e&&p!==r)_.current.set(m,[p,f]);else continue}t.push(m)}if(e)for(let[m,p]of this.async_deriveds){let f=_.async_deriveds.get(m);if(f)p.promise.then(f.resolve).catch(f.reject)}var n=[..._.current.keys()].filter((m)=>!_.current.get(m)[1]);if(!_.started||n.length===0)continue;var o=n.filter((m)=>!this.current.has(m));if(o.length===0){if(e)_.discard()}else if(t.length>0){if(c&&!_.#l)ns(_.#n.length===0,"Batch has scheduled effects");if(e)for(let m of this.#d)_.unskip_effect(m,(p)=>{if((p.f&(pe|Pe))!==0)_.schedule(p);else _.#_([p])});_.activate();var i=new Set,s=new Map;for(var a of t)_s(a,o,i,s);s=new Map;var l=[..._.current].filter(([m,p])=>{let f=this.current.get(m);if(!f)return!0;return f[0]!==p[0]||f[1]!==p[1]}).map(([m])=>m);if(l.length>0){for(let m of this.#s)if((m.f&(F|Z|Rt))===0&&Co(m,l,s))if((m.f&(Pe|pe))!==0)L(m,O),_.schedule(m);else _.add_dirty_reaction(m,O)}if(_.#n.length>0&&!_.#l){_.apply();for(var u of _.#c())_.#x(u,[],[])}_.deactivate()}}}increment(e,t){if(this.#r+=1,e){let r=this.#a.get(t)??0;this.#a.set(t,r+1)}}decrement(e,t){if(this.#r-=1,e){let r=this.#a.get(t)??0;if(r===1)this.#a.delete(t);else this.#a.set(t,r-1)}if(this.#l)return;this.#l=!0,J(()=>{if(this.#l=!1,this.linked)this.flush()})}transfer_reactions(e){for(let[t,r]of e)this.add_dirty_reaction(t,r);e.clear()}add_dirty_reaction(e,t){if(t===O||this.#i.get(e)!==O)this.#i.set(e,t)}oncommit(e){this.#e.add(e)}ondiscard(e){this.#t.add(e)}settled(){return(this.#o??=rn()).promise}static ensure(){if(k===null){let e=k=new he;if(!So&&!Jt)J(()=>{if(!e.started)e.flush()})}return k}apply(){if(!ee||!this.is_fork&&this.prev===null&&this.next===null){le=null;return}le=new Map;for(let[e,[t]]of this.current)le.set(e,t);for(let e=$n;e!==null;e=e.next){if(e===this||e.is_fork)continue;if(e.id>=this.id||!e.#h(this)){for(let[t,r]of e.previous)if(!le.has(t))le.set(t,r)}}}schedule(e){if(Ro=e,e.b?.is_pending&&(e.f&(Et|Nt|Gt))!==0&&(e.f&$e)===0){e.b.defer_effect(e);return}this.#n.push(e)}#p(){if(!this.linked)return;var e=this.prev,t=this.next;if(e===null)$n=t;else e.next=t;if(t===null)yr=e;else t.prev=e;this.linked=!1}}function Ft(e){var t=Jt,r=We;We=null,Jt=!0;try{var n;if(e)Ft(),n=e();while(!0){if(is(),k===null)return n;k.flush()}}finally{Jt=t,We=r}}function Pa(){if(c){var e=new Map;for(let r of k.current.keys())for(let[n,o]of r.updated??[]){var t=e.get(n);if(!t)t={error:o.error,count:0},e.set(n,t);t.count+=o.count}for(let r of e.values())if(r.error)console.error(r.error)}try{Wi()}catch(r){if(c)ie(r,"stack",{value:""});De(r,Ro)}}var Ze=null;function ds(e){var t=e.length;if(t===0)return;var r=0;while(r<t){var n=e[r++];if((n.f&(F|Z))===0&&Pt(n)){if(Ze=new Set,mt(n),n.deps===null&&n.first===null&&n.nodes===null&&n.teardown===null&&n.ac===null)Oo(n);if(Ze?.size>0){Je.clear();for(let o of Ze){if((o.f&(F|Z))!==0)continue;let i=[o],s=o.parent;while(s!==null){if(Ze.has(s))Ze.delete(s),i.push(s);s=s.parent}for(let a=i.length-1;a>=0;a--){let l=i[a];if((l.f&(F|Z))!==0)continue;mt(l)}}Ze.clear()}}}Ze=null}function _s(e,t,r,n){if(r.has(e))return;if(r.add(e),e.reactions!==null)for(let o of e.reactions){let i=o.f;if((i&U)!==0)_s(o,t,r,n);else if((i&(Pe|pe))!==0&&(i&O)===0&&Co(o,t,n))L(o,O),Vr(o)}}function Co(e,t,r){let n=r.get(e);if(n!==void 0)return n;if(e.deps!==null)for(let o of e.deps){if(yt.call(t,o))return!0;if((o.f&U)!==0&&Co(o,t,r))return r.set(o,!0),!0}return r.set(e,!1),!1}function Vr(e){k.schedule(e)}function ms(e,t){if((e.f&se)!==0&&(e.f&M)!==0)return;if((e.f&O)!==0)t.d.push(e);else if((e.f&q)!==0)t.m.push(e);L(e,M);var r=e.first;while(r!==null)ms(r,t),r=r.next}function vs(e){L(e,M);var t=e.first;while(t!==null)vs(t),t=t.next}var Qt=new Set,Je=new Map;function bn(e){Qt=e}var Io=!1;function gs(){Io=!0}function be(e,t){var r={f:0,v:e,reactions:null,equals:un,rv:0,wv:0};if(c&&nt)r.created=t??ot("created at"),r.updated=null,r.set_during_effect=!1,r.trace=null;return r}function Y(e,t){let r=be(e,t);return En(r),r}function rr(e,t=!1,r=!0){let n=be(e);if(!t)n.equals=pn;if(Mt&&r&&N!==null&&N.l!==null)(N.l.s??=[]).push(n);return n}function S(e,t,r=!1){if(E!==null&&(!Te||(E.f&Rt)!==0)&&_t()&&(E.f&(U|pe|Pe|Rt))!==0&&(at===null||!at.has(e)))es();let n=r?Bt(t):t;if(c)mn(n,e.label);return st(e,n,Hr)}var tr=null,Do=0;function st(e,t,r=null){if(!e.equals(t)){if(Xe)Je.set(e,t);else if(!Je.has(e))Je.set(e,e.v);var n=he.ensure();if(n.capture(e,t),c){if(nt||b!==null){e.updated??=new Map;let o=(e.updated.get("")?.count??0)+1;if(e.updated.set("",{error:null,count:o}),nt||o>5){let i=ot("updated at");if(i!==null){let s=e.updated.get(i.stack);if(!s)s={error:i,count:0},e.updated.set(i.stack,s);s.count++}}}if(b!==null)e.set_during_effect=!0}if((e.f&U)!==0){let o=e;if((e.f&O)!==0)Br(o);if(le===null)vr(o)}if(e.wv=br(),tr=null,Do=0,xs(e,O,r),tr=null,_t()&&b!==null&&(b.f&M)!==0&&(b.f&(se|Ce))===0)if(Ve===null)ws([e]);else Ve.push(e);if(!n.is_fork&&Qt.size>0&&!Io)kn()}return t}function kn(){Io=!1;for(let e of Qt){if((e.f&M)!==0)L(e,q);let t;try{t=Pt(e)}catch{t=!0}if(t)mt(e)}Qt.clear()}function nr(e){S(e,e.v+1)}function xs(e,t,r){var n=e.reactions;if(n===null)return;var o=_t(),i=n.length;if(Do+=i,Do>1e5&&tr===null)tr=new Set;if(tr!==null){if(tr.has(e))return;tr.add(e)}for(var s=0;s<i;s++){var a=n[s],l=a.f;if(!o&&a===b)continue;var u=(l&O)===0;if(u)L(a,t);if((l&Rt)!==0)Qt.add(a);else if((l&U)!==0){var _=a;le?.delete(_),xs(_,q,r)}else if(u){var m=a;if((l&pe)!==0&&Ze!==null)Ze.add(m);if(r!==null)r.push(m);else Vr(m)}}}var Ya=/^[a-zA-Z_$][a-zA-Z_$0-9]*$/;function Bt(e){if(typeof e!=="object"||e===null||Be in e||on in e)return e;let t=cr(e);if(t!==io&&t!==Ti)return e;var r=new Map,n=bt(e),o=Y(0),i=c&&nt?ot("created at"):null,s=Yt,a=(m)=>{if(Yt===s)return m();var p=E,f=Yt;te(null),Mo(s);var g=m();return te(p),Mo(f),g};if(n){if(r.set("length",Y(e.length,i)),c)e=Va(e)}var l="";let u=!1;function _(m){if(u)return;u=!0,l=m,we(o,`${l} version`);for(let[p,f]of r)we(f,or(l,p));u=!1}return new Proxy(e,{defineProperty(m,p,f){if(!("value"in f)||f.configurable===!1||f.enumerable===!1||f.writable===!1)Ji();var g=r.get(p);if(g===void 0)a(()=>{var y=Y(f.value,i);if(r.set(p,y),c&&typeof p==="string")we(y,or(l,p));return y});else S(g,f.value,!0);return!0},deleteProperty(m,p){var f=r.get(p);if(f===void 0){if(p in m){let g=a(()=>Y(P,i));if(r.set(p,g),nr(o),c)we(g,or(l,p))}}else S(f,P),nr(o);return!0},get(m,p,f){if(p===Be)return e;if(c&&p===an)return _;var g=r.get(p),y=p in m;if(g===void 0&&(!y||Fe(m,p)?.writable))g=a(()=>{var T=Bt(y?m[p]:P),C=Y(T,i);if(c)we(C,or(l,p));return C}),r.set(p,g);if(g!==void 0){var h=d(g);return h===P?void 0:h}return Reflect.get(m,p,f)},getOwnPropertyDescriptor(m,p){this.has?.(m,p);var f=Reflect.getOwnPropertyDescriptor(m,p),g=r.get(p);if(g!==void 0){var y=d(g);if(y===P)return;if(f&&"value"in f)f.value=y;else return{enumerable:!0,configurable:!0,value:y,writable:!0}}return f},has(m,p){if(p===Be)return!0;var f=r.get(p),g=f!==void 0&&f.v!==P||Reflect.has(m,p);if(f!==void 0||(b!==null||E!==null)&&(!g||Fe(m,p)?.writable)){if(f===void 0)f=a(()=>{var h=g?Bt(m[p]):P,T=Y(h,i);if(c)we(T,or(l,p));return T}),r.set(p,f);var y=d(f);if(y===P)return!1}return g},set(m,p,f,g){var y=r.get(p),h=p in m;if(n&&p==="length")for(var T=f;T<y.v;T+=1){var C=r.get(T+"");if(C!==void 0)S(C,P);else if(T in m){if(C=a(()=>Y(P,i)),r.set(T+"",C),c)we(C,or(l,T))}}if(y===void 0){if(!h||Fe(m,p)?.writable){if(y=a(()=>Y(void 0,i)),c)we(y,or(l,p));S(y,Bt(f)),r.set(p,y)}}else{h=y.v!==P;var H=a(()=>Bt(f));S(y,H)}var oe=Reflect.getOwnPropertyDescriptor(m,p);if(oe?.set)oe.set.call(g,f);if(!h){if(n&&typeof p==="string"){var j=r.get("length"),ue=Number(p);if(Number.isInteger(ue)&&ue>=j.v)S(j,ue+1)}nr(o)}return!0},ownKeys(m){d(o);var p=Reflect.ownKeys(m).filter((y)=>{var h=r.get(y);return h===void 0||h.v!==P});for(var[f,g]of r)if(g.v!==P&&!(f in m))p.push(f);return p},setPrototypeOf(){Qi()}})}function or(e,t){if(typeof t==="symbol")return`${e}[Symbol(${t.description??""})]`;if(Ya.test(t))return`${e}.${t}`;return/^\d+$/.test(t)?`${e}[${t}]`:`${e}['${t}']`}function Tn(e){try{if(e!==null&&typeof e==="object"&&Be in e)return e[Be]}catch{}return e}var Ha=new Set(["copyWithin","fill","pop","push","reverse","shift","sort","splice","unshift"]);function Va(e){return new Proxy(e,{get(t,r,n){var o=Reflect.get(t,r,n);if(!Ha.has(r))return o;return function(...i){gs();var s=o.apply(this,i);return kn(),s}}})}function bs(){let{prototype:e,__svelte_cleanup:t}=Array;if(t)t();let{indexOf:r,lastIndexOf:n,includes:o}=e;e.indexOf=function(i,s){let a=r.call(this,i,s);if(a===-1){for(let l=s??0;l<this.length;l+=1)if(Tn(this[l])===i){cn("array.indexOf(...)");break}}return a},e.lastIndexOf=function(i,s){let a=n.call(this,i,s??this.length-1);if(a===-1){for(let l=0;l<=(s??this.length-1);l+=1)if(Tn(this[l])===i){cn("array.lastIndexOf(...)");break}}return a},e.includes=function(i,s){let a=o.call(this,i,s);if(!a){for(let l=0;l<this.length;l+=1)if(Tn(this[l])===i){cn("array.includes(...)");break}}return a},Array.__svelte_cleanup=()=>{e.indexOf=r,e.lastIndexOf=n,e.includes=o}}var $r,ys,Ur,Es,$s;function An(){if($r!==void 0)return;$r=window,ys=document,Ur=/Firefox/.test(navigator.userAgent);var e=Element.prototype,t=Node.prototype,r=Text.prototype;if(Es=Fe(t,"firstChild").get,$s=Fe(t,"nextSibling").get,so(e))e[Or]=void 0,e[fn]=null,e[lo]=void 0,e.__e=void 0;if(so(r))r[Dr]=void 0;if(c)e.__svelte_meta=null,bs()}function z(e=""){return document.createTextNode(e)}function me(e){return Es.call(e)}function ae(e){return $s.call(e)}function K(e,t){if(!w)return me(e);var r=me(A);if(r===null)r=A.appendChild(z());else if(t&&r.nodeType!==zt){var n=z();return r?.before(n),I(n),n}if(t)jr(r);return I(r),r}function Sn(e,t=!1){if(!w){var r=me(e);if(r instanceof Comment&&r.data==="")return ae(r);return r}if(t){if(A?.nodeType!==zt){var n=z();return A?.before(n),I(n),n}jr(A)}return A}function Ae(e,t=!1){if(!w)return me(e);var r=K(e,t);return B(e),r}function D(e,t=1,r=!1){let n=w?A:e;var o;while(t--)o=n,n=ae(n);if(!w)return n;if(r){if(n?.nodeType!==zt){var i=z();if(n===null)o?.after(i);else n.before(i);return I(i),i}jr(n)}return I(n),n}function Fr(e){e.textContent=""}function Nn(){if(!ee)return!1;if(Ze!==null)return!1;var e=b.f;return(e&$e)!==0}function vt(e,t,r){if(t==null||t===ln)return r?document.createElement(e,{is:r}):document.createElement(e);return r?document.createElementNS(t,e,{is:r}):document.createElementNS(t,e)}function jr(e){if(e.nodeValue.length<65536)return;let t=e.nextSibling;while(t!==null&&t.nodeType===zt)t.remove(),e.nodeValue+=t.nodeValue,t=e.nextSibling}var Lo=new WeakMap;function ks(e){var t=b;if(t===null)return E.f|=Ge,e;if(c&&e instanceof Error&&!Lo.has(e))Lo.set(e,Ua(e,t));if((t.f&$e)===0&&(t.f&Et)===0){if(c&&!t.parent&&e instanceof Error)Ts(e);throw e}De(e,t)}function De(e,t){if(e===Ye)throw e;if(t!==null&&(t.f&F)!==0)return;while(t!==null){if((t.f&ur)!==0&&(t.f&(F|$t))===0){if((t.f&$e)===0)throw e;try{t.b.error(e);return}catch(r){e=r}}t=t.parent}if(c&&e instanceof Error)Ts(e);throw e}function Ua(e,t){let r=Fe(e,"message");if(r&&!r.configurable)return;var n=Ur?"  ":"\t",o=`
${n}in ${t.fn?.name||"<unknown>"}`,i=t.ctx;while(i!==null)o+=`
${n}in ${i.function?.[Ke].split("/").pop()}`,i=i.p;return{message:e.message+`
${o}
`,stack:e.stack?.split(`
`).filter((s)=>!s.includes("svelte/src/internal")).join(`
`)}}function Ts(e){let t=Lo.get(e);if(t)ie(e,"message",{value:t.message}),ie(e,"stack",{value:t.stack})}function As(e){if(b===null){if(E===null)Ki(e);zi()}if(Xe)Gi(e)}function ja(e,t){var r=t.last;if(r===null)t.last=t.first=e;else r.next=e,e.prev=r,t.last=e}function At(e,t){var r=b;if(c)while(r!==null&&(r.f&Rt)!==0)r=r.parent;if(r!==null&&(r.f&Z)!==0)e|=Z;var n={ctx:N,deps:null,nodes:null,f:e|O|Ee,first:null,fn:t,last:null,next:null,parent:r,b:r&&r.b,prev:null,teardown:null,wv:0,ac:null};if(c)n.component_function=it;k?.register_created_effect(n);var o=n;if((e&Et)!==0)if(Er!==null)Er.push(n);else he.ensure().schedule(n);else if(t!==null){try{mt(n)}catch(s){throw G(n),s}if(o.deps===null&&o.teardown===null&&o.nodes===null&&o.first===o.last&&(o.f&ut)===0){if(o=o.first,(e&pe)!==0&&(e&Oe)!==0&&o!==null)o.f|=Oe}}if(o!==null){if(o.parent=r,r!==null)ja(o,r);if(E!==null&&(E.f&U)!==0&&(e&Ce)===0){var i=E;(i.effects??=[]).push(o)}}return n}function Lt(){return E!==null&&!Te}function ve(e){let t=At(Nt,null);return L(t,M),t.teardown=e,t}function kr(e){if(As("$effect"),c)ie(e,"name",{value:"$effect"});var t=b.f,r=!E&&(t&se)!==0&&N!==null&&!N.i;if(r){var n=N;(n.e??=[]).push(e)}else return xo(e)}function xo(e){return At(Et|Ai,e)}function Po(e){he.ensure();let t=At(Ce|ut,e);return()=>{G(t)}}function Ss(e){he.ensure();let t=At(Ce|ut,e);return(r={})=>new Promise((n)=>{if(r.outro)ht(t,()=>{G(t),n(void 0)});else G(t),n(void 0)})}function us(e){return At(Pe|ut,e)}function ge(e,t=0){return At(Nt|t,e)}function Se(e,t=[],r=[],n=[]){wn(n,t,r,(o)=>{At(Nt,()=>{e(...o.map(d))})})}function Ue(e,t=0){var r=At(pe|t,e);if(c)r.dev_stack=dt;return r}function ne(e){return At(se|ut,e)}function Bo(e){var t=e.teardown;if(t!==null){let r=Xe,n=E;Fo(!0),te(null);try{t.call(null)}catch(o){De(o,e.parent)}finally{Fo(r),te(n)}}}function Yr(e,t=!1){var r=e.first;e.first=e.last=null;while(r!==null){let o=r.ac;if(o!==null)He(()=>{o.abort(kt)});var n=r.next;if((r.f&Ce)!==0)r.parent=null;else G(r,t);r=n}}function Ns(e){var t=e.first;while(t!==null){var r=t.next;if((t.f&se)===0)G(t);t=r}}function G(e,t=!0){var r=!1;if((t||(e.f&fo)!==0)&&e.nodes!==null&&e.nodes.end!==null)Rs(e.nodes.start,e.nodes.end),r=!0;e.f|=$t,Yr(e,t&&!r),er(e,0);var n=e.nodes&&e.nodes.t;if(n!==null)for(let i of n)i.stop();Bo(e),e.f^=$t,e.f|=F;var o=e.parent;if(o!==null&&o.first!==null)Oo(e);if(c)e.component_function=null;e.next=e.prev=e.teardown=e.ctx=e.deps=e.fn=e.nodes=e.ac=e.b=null}function Rs(e,t){while(e!==null){var r=e===t?null:ae(e);e.remove(),e=r}}function Oo(e){var{parent:t,prev:r,next:n}=e;if(r!==null)r.next=n;if(n!==null)n.prev=r;if(t!==null){if(t.first===e)t.first=n;if(t.last===e)t.last=r}}function ht(e,t,r=!0){var n=[];e.f|=nn,Cs(e,n,!0);var o=()=>{if(r)G(e);if(t)t()},i=n.length;if(i>0){var s=()=>--i||o();for(var a of n)a.out(s)}else o()}function Cs(e,t,r){if((e.f&Z)!==0)return;e.f^=Z;var n=e.nodes&&e.nodes.t;if(n!==null){for(let a of n)if(a.is_global||r)t.push(a)}var o=e.first;while(o!==null){var i=o.next;if((o.f&Ce)===0){var s=(o.f&Oe)!==0||(o.f&se)!==0&&(e.f&pe)!==0;Cs(o,t,s?r:!1)}o=i}}function Tr(e){e.f&=~nn,Os(e,!0)}function Os(e,t){if((e.f&nn)!==0)return;if((e.f&Z)===0)return;if(e.f^=Z,(e.f&M)===0)L(e,O),he.ensure().schedule(e);var r=e.first;while(r!==null){var n=r.next,o=(r.f&Oe)!==0||(r.f&se)!==0;Os(r,o?t:!1),r=n}var i=e.nodes&&e.nodes.t;if(i!==null){for(let s of i)if(s.is_global||t)s.in()}}function Ar(e,t){if(!e.nodes)return;var r=e.nodes.start,n=e.nodes.end;while(r!==null){var o=r===n?null:ae(r);t.append(r),r=o}}var Ds=null;var Xe=!1;function Fo(e){Xe=e}var E=null,Te=!1;function te(e){E=e}var b=null;function re(e){b=e}var at=null;function En(e){if(E!==null&&(!ee&&(E.f&pr)!==0||(E.f&U)!==0))(at??=new Set).add(e)}var fe=null,ke=0,Ve=null;function ws(e){Ve=e}var Ms=1,ir=0,Yt=ir;function Mo(e){Yt=e}function br(){return++Ms}function Pt(e){var t=e.f;if((t&O)!==0)return!0;if((t&q)!==0){var r=e.deps,n=r.length;for(var o=0;o<n;o++){var i=r[o];if(Pt(i))To(i);if(i.wv>e.wv)return!0}if((t&Ee)!==0&&le===null)L(e,M)}return!1}function Ls(e,t,r=!0){var n=e.reactions;if(n===null)return;if(!ee&&at!==null&&at.has(e))return;for(var o=0;o<n.length;o++){var i=n[o];if((i.f&U)!==0)Ls(i,t,!1);else if(t===i){if(r)L(i,O);else if((i.f&M)!==0)L(i,q);Vr(i)}}}function yn(e){var t=fe,r=ke,n=Ve,o=E,i=at,s=N,a=Te,l=Yt,u=e.f;if(fe=null,ke=0,Ve=null,E=(u&(se|Ce))===0?e:null,at=null,Tt(e.ctx),Te=!1,Yt=++ir,e.ac!==null)He(()=>{e.ac.abort(kt)}),e.ac=null;try{e.f|=pr;var _=e.fn,m=_();e.f|=$e;var p=Is(e);if(_t()&&Ve!==null&&!Te&&p!==null&&(e.f&(U|q|O))===0)for(var f=0;f<Ve.length;f++)Ls(Ve[f],e);if(o!==null&&o!==e){if(ir++,o.deps!==null)for(let g=0;g<r;g+=1)o.deps[g].rv=ir;if(t!==null)for(let g of t)g.rv=ir;if(Ve!==null)if(n===null)n=Ve;else n.push(...Ve)}if((e.f&Ge)!==0)e.f^=Ge;return m}catch(g){return Is(e),ks(g)}finally{e.f^=pr,fe=t,ke=r,Ve=n,E=o,at=i,Tt(s),Te=a,Yt=l}}function Is(e){var t=e.deps,r=k?.is_fork;if(fe!==null){var n;if(!r)er(e,ke);if(t!==null&&ke>0){t.length=ke+fe.length;for(n=0;n<fe.length;n++)t[ke+n]=fe[n]}else e.deps=t=fe;if(Lt()&&(e.f&Ee)!==0)for(n=ke;n<t.length;n++)(t[n].reactions??=[]).push(e)}else if(!r&&t!==null&&ke<t.length)er(e,ke),t.length=ke;return t}function qa(e,t){let r=t.reactions;if(r!==null){var n=ki.call(r,e);if(n!==-1){var o=r.length-1;if(o===0)r=t.reactions=null;else r[n]=r[o],r.pop()}}if(r===null&&(t.f&U)!==0&&(fe===null||!yt.call(fe,t))){var i=t;if((i.f&Ee)!==0)i.f^=Ee;if(i.v!==P)vr(i);if(i.ac!==null)He(()=>{i.ac.abort(kt),i.ac=null,L(i,O)});cs(i),er(i,0)}}function er(e,t){var r=e.deps;if(r===null)return;for(var n=t;n<r.length;n++)qa(e,r[n])}function mt(e){var t=e.f;if((t&F)!==0)return;L(e,M);var r=b;if(b=e,c){var n=it;vn(e.component_function);var o=dt;mr(e.dev_stack??dt)}try{if((t&(pe|Gt))!==0)Ns(e);else Yr(e);Bo(e);var i=yn(e);if(e.teardown=typeof i==="function"?i:null,e.wv=Ms,c&&nt&&(e.f&O)!==0&&e.deps!==null){for(var s of e.deps)if(s.set_during_effect)s.wv=br(),s.set_during_effect=!1}}finally{if(b=r,c)vn(n),mr(o)}}async function qr(){if(ee)return new Promise((e)=>{requestAnimationFrame(()=>e()),setTimeout(()=>e())});await Promise.resolve(),Ft()}function d(e){var t=e.f,r=(t&U)!==0;if(Ds?.add(e),E!==null&&!Te){var n=b!==null&&(b.f&F)!==0;if(!n&&(at===null||!at.has(e))){var o=E.deps;if((E.f&pr)!==0){if(e.rv<ir)if(e.rv=ir,fe===null&&o!==null&&o[ke]===e)ke++;else if(fe===null)fe=[e];else fe.push(e)}else{if(E.deps??=[],!yt.call(E.deps,e))E.deps.push(e);var i=e.reactions;if(i===null)e.reactions=[E];else if(!yt.call(i,E))i.push(E)}}}if(c){if(!Te&&Ie&&k===null&&We===null&&!Ie.warned&&(Ie.effect.f&pr)===0&&!Ie.effect_deps.has(e)){Ie.warned=!0,Mi(e.label);var s=ot("traced at");if(s)console.warn(s)}if(Pr.delete(e),nt&&!Te&&Ir!==null&&E!==null&&Ir.reaction===E){if(e.trace)e.trace();else if(s=ot("traced at"),s){var a=Ir.entries.get(e);if(a===void 0)a={traces:[]},Ir.entries.set(e,a);var l=a.traces[a.traces.length-1];if(s.stack!==l?.stack)a.traces.push(s)}}}if(Xe&&Je.has(e))return Je.get(e);if(r){var u=e;if(Xe){var _=u.v;if((u.f&M)===0&&u.reactions!==null||Ps(u))_=Br(u);return Je.set(u,_),_}var m=(u.f&Ee)===0&&!Te&&E!==null&&(E.f&Ee)!==0,p=(u.f&$e)===0;if(Pt(u)){if(m)u.f|=Ee;To(u)}if(m&&!p)Ao(u),Fs(u)}if(le?.has(e))return le.get(e);if((e.f&Ge)!==0)throw e.v;return e.v}function Fs(e){if(e.f|=Ee,e.deps===null)return;for(let r of e.deps){var t=r.reactions;if(t===null)r.reactions=[e];else if(!yt.call(t,e))t.push(e);if((r.f&U)!==0&&(r.f&Ee)===0)Ao(r),Fs(r)}}function Ps(e){if(e.v===P)return!0;if(e.deps===null)return!1;for(let t of e.deps){if(Je.has(t))return!0;if((t.f&U)!==0&&Ps(t))return!0}return!1}function V(e){var t=Te;try{return Te=!0,e()}finally{Te=t}}var za=["allowfullscreen","async","autofocus","autoplay","checked","controls","default","disabled","formnovalidate","indeterminate","inert","ismap","loop","multiple","muted","nomodule","novalidate","open","playsinline","readonly","required","reversed","seamless","selected","webkitdirectory","defer","disablepictureinpicture","disableremoteplayback"];var Tp=[...za,"formNoValidate","isMap","noModule","playsInline","readOnly","value","volume","defaultValue","defaultChecked","srcObject","noValidate","allowFullscreen","disablePictureInPicture","disableRemotePlayback"];var Ka=["touchstart","touchmove"];function Bs(e){return Ka.includes(e)}var Wa=["$state","$state.raw","$derived","$derived.by"],Ap=[...Wa,"$state.eager","$state.snapshot","$props","$props.id","$bindable","$effect","$effect.pre","$effect.tracking","$effect.root","$effect.pending","$inspect","$inspect().with","$inspect.trace","$host"];var Sr=Symbol("events"),Vo=new Set,Rn=new Set;function Uo(e){if(!w)return;e.removeAttribute("onload"),e.removeAttribute("onerror");let t=e.__e;if(t!==void 0)e.__e=void 0,queueMicrotask(()=>{if(e.isConnected)e.dispatchEvent(t)})}function Vs(e,t,r,n={}){function o(i){if(!n.capture)On.call(t,i);if(!i.cancelBubble)return He(()=>r?.call(this,i))}if(e.startsWith("pointer")||e.startsWith("touch")||e==="wheel")o.__removed=!1,J(()=>{if(!o.__removed)t.addEventListener(e,o,n)});else t.addEventListener(e,o,n);return o}function gt(e,t,r,n,o){var i={capture:n,passive:o},s=Vs(e,t,r,i);if(t===document.body||t===window||t===document||t instanceof HTMLMediaElement)ve(()=>{s.__removed=!0,t.removeEventListener(e,s,i)})}function ce(e,t,r){(t[Sr]??={})[e]=r}function Cn(e){for(var t=0;t<e.length;t++)Vo.add(e[t]);for(var r of Rn)r(e)}var Yo=null,Ho=!1;function On(e){var t=this,r=t.ownerDocument,n=e.type,o=e.composedPath?.()||[],i=o[0]||e.target;if(Yo=e,!Ho)Ho=!0,setTimeout(()=>{Ho=!1,Yo=null});var s=0,a=Yo===e&&e[Sr];if(a){var l=o.indexOf(a);if(l!==-1&&(t===document||t===window)){e[Sr]=t;return}var u=o.indexOf(t);if(u===-1)return;if(l<=u)s=l}if(i=o[s]||e.target,i===t)return;ie(e,"currentTarget",{configurable:!0,get(){return i||r}});var _=E,m=b;te(null),re(null);try{var p,f=[];while(i!==null){if(i===t)break;try{var g=i[Sr]?.[n];if(g!=null&&(!i.disabled||e.target===i))g.call(i,e)}catch(y){if(p)f.push(y);else p=y}if(e.cancelBubble)break;s++,i=s<o.length?o[s]:null}if(p){for(let y of f)queueMicrotask(()=>{throw y});throw p}}finally{e[Sr]=t,delete e.currentTarget,te(_),re(m)}}var Za=globalThis?.window?.trustedTypes&&globalThis.window.trustedTypes.createPolicy("svelte-trusted-html",{createHTML:(e)=>e});function Us(e){return Za?.createHTML(e)??e}function jo(e){var t=vt("template");return t.innerHTML=Us(e.replaceAll("<!>","<!---->")),t.content}function je(e,t){var r=b;if(r.nodes===null)r.nodes={start:e,end:t,a:null,t:null}}function Qe(e,t){var r=(t&Di)!==0,n=(t&Ii)!==0,o,i=!e.startsWith("<!>");return()=>{if(w)return je(A,null),A;if(o===void 0){if(o=jo(i?e:"<!>"+e),!r)o=me(o)}var s=n||Ur?document.importNode(o,!0):o.cloneNode(!0);if(r){var a=me(s),l=s.lastChild;je(a,l)}else je(s,s);return s}}function Gr(e=""){if(!w){var t=z(e+"");return je(t,t),t}var r=A;if(r.nodeType!==zt)r.before(r=z()),I(r);else jr(r);return je(r,r),r}function Go(){if(w)return je(A,null),A;var e=document.createDocumentFragment(),t=document.createComment(""),r=z();return e.append(t,r),je(t,r),e}function W(e,t){if(w){var r=b;if((r.f&$e)===0||r.nodes.end===null)r.nodes.end=A;xe();return}if(e===null)return;e.before(t)}function js(e){let t=0,r=be(0),n;if(c)we(r,"createSubscriber version");return()=>{if(Lt())d(r),ge(()=>{if(t===0)n=V(()=>e(()=>nr(r)));return t+=1,()=>{J(()=>{if(t-=1,t===0)n?.(),n=void 0,nr(r)})}})}}var ef=Oe|ut;function zo(e,t,r,n){new qs(e,t,r,n)}class qs{parent;is_pending=!1;transform_error;#e;#t=w?A:null;#r;#a;#o;#n=null;#s=null;#i=null;#f=null;#d=0;#l=0;#m=!1;#g=new Map;#c=null;#v=js(()=>{if(this.#c=be(this.#d),c)we(this.#c,"$effect.pending()");return()=>{this.#c=null}});constructor(e,t,r,n){if(this.#e=e,this.#r=t,this.#a=(o)=>{var i=b;i.b=this,i.f|=ur,r(o)},this.parent=b.b,this.transform_error=n??this.parent?.transform_error??((o)=>o),this.#o=Ue(()=>{if(w){let o=Xt(this.#t);xe();let i=o===Ot;if(o.startsWith(_o)){let a=JSON.parse(o.slice(_o.length));this.#b(a)}else if(i)this.#_();else this.#x()}else this.#w()},ef),w)this.#e=A}#x(){try{this.#n=ne(()=>this.#a(this.#e))}catch(e){this.error(e)}}#b(e){let t=this.#r.failed,{reset:r,invoke_onerror:n}=this.#h(e);if(J(n),!t)return;this.#i=ne(()=>{t(this.#e,()=>e,()=>r)})}#h(e){var t=!1,r=!1;let n=()=>{if(this.#u())return;if(t){Yi();return}if(t=!0,r)ts();if(this.#i!==null)ht(this.#i,()=>{this.#i=null});this.#y(()=>{this.#w()})};return{reset:n,invoke_onerror:()=>{if(this.#u())return;try{r=!0,this.#r.onerror?.(e,n),r=!1}catch(i){De(i,this.#o&&this.#o.parent)}}}}#u(){return(this.#o.f&(F|$t))!==0}#_(){let e=this.#r.pending;if(!e)return;this.is_pending=!0,this.#s=ne(()=>e(this.#e)),J(()=>{if(this.#u())return;var t=this.#f=document.createDocumentFragment(),r=z(),n=!1;if(t.append(r),this.#n=this.#y(()=>{try{return ne(()=>this.#a(r))}catch(o){try{this.error(o),n=!0}catch(i){De(i,this.#o.parent)}return null}}),this.#n===null){if(this.#f=null,n)this.#p(k);return}if(this.#l===0)this.#e.before(t),this.#f=null,ht(this.#s,()=>{this.#s=null}),this.#p(k)})}#w(){try{if(this.is_pending=this.has_pending_snippet(),this.#l=0,this.#d=0,this.#n=ne(()=>{this.#a(this.#e)}),this.#l>0){var e=this.#f=document.createDocumentFragment();Ar(this.#n,e);let t=this.#r.pending;this.#s=ne(()=>t(this.#e))}else this.#p(k)}catch(t){this.error(t)}}#p(e){this.is_pending=!1,e.transfer_reactions(this.#g)}defer_effect(e){gn(e,this.#g)}is_rendered(){return!this.is_pending&&(!this.parent||this.parent.is_rendered())}has_pending_snippet(){return!!this.#r.pending}#y(e){var t=b,r=E,n=N;re(this.#o),te(this.#o),Tt(this.#o.ctx);try{return he.ensure(),e()}finally{re(t),te(r),Tt(n)}}#E(e,t){if(!this.has_pending_snippet()){if(this.parent)this.parent.#E(e,t);return}if(this.#l+=e,this.#l===0){if(this.#p(t),this.#s)ht(this.#s,()=>{this.#s=null});if(this.#f)this.#e.before(this.#f),this.#f=null}}update_pending_count(e,t){if(this.#E(e,t),this.#d+=e,!this.#c||this.#m)return;this.#m=!0,J(()=>{if(this.#m=!1,this.#c)st(this.#c,this.#d)})}get_effect_pending(){return this.#v(),d(this.#c)}error(e){if(e===Ye)throw e;if(!this.#r.onerror&&!this.#r.failed)throw e;if(k?.is_fork){if(this.#n)k.skip_effect(this.#n);if(this.#s)k.skip_effect(this.#s);if(this.#i)k.skip_effect(this.#i);k.oncommit(()=>{if(!this.#u())this.#$(e)})}else this.#$(e)}#$(e){if(this.#n)G(this.#n),this.#n=null;if(this.#s)G(this.#s),this.#s=null;if(this.#i)G(this.#i),this.#i=null;if(w)I(this.#t),_r(),I(pt());let t=this.#r.failed,r=(n)=>{if(this.#u())return;let{reset:o,invoke_onerror:i}=this.#h(n);if(i(),t&&!this.#u())this.#i=this.#y(()=>{try{return ne(()=>{var s=b;s.b=this,s.f|=ur,t(this.#e,()=>n,()=>o)})}catch(s){return De(s,this.#o.parent),null}})};J(()=>{if(this.#u())return;var n;try{n=this.transform_error(e)}catch(o){De(o,this.#o&&this.#o.parent);return}if(n!==null&&typeof n==="object"&&typeof n.then==="function")n.then(r,(o)=>De(o,this.#o&&this.#o.parent));else r(n)})}}var Ko=!0;function X(e,t){var r=t==null?"":typeof t==="object"?`${t}`:t;if(r!==(e[Dr]??=e.nodeValue))e[Dr]=r,e.nodeValue=`${r}`}function Nr(e,t){return Gs(e,t)}function Xo(e,t){An(),t.intro=t.intro??!1;let r=t.target,n=w,o=A;try{var i=me(r);while(i&&(i.nodeType!==ze||i.data!==dr))i=ae(i);if(!i)throw Ye;Q(!0),I(i);let s=Gs(e,{...t,anchor:i});return Q(!1),s}catch(s){if(s instanceof Error&&s.message.split(`
`).some((a)=>a.startsWith("https://svelte.dev/e/")))throw s;if(s!==Ye)console.warn("Failed to hydrate: ",s);if(t.recover===!1)Xi();return An(),Fr(r),Q(!1),Nr(e,t)}finally{Q(n),I(o)}}var Dn=new Map;function Gs(e,{target:t,anchor:r,props:n={},events:o,context:i,intro:s=!0,transformError:a}){An();var l=void 0,u=Ss(()=>{var _=r??t.appendChild(z());zo(_,{pending:()=>{}},(f)=>{Mr({});var g=N;if(i)g.c=i;if(o)n.$$events=o;if(w)je(f,null);if(Ko=s,l=e(f,n)||hn(),Ko=!0,w){if(b.nodes.end=A,A===null||A.nodeType!==ze||A.data!==Kt)throw Wt(),Ye}Lr()},a);var m=new Set,p=(f)=>{for(var g=0;g<f.length;g++){var y=f[g];if(m.has(y))continue;m.add(y);var h=Bs(y);for(let H of[t,document]){var T=Dn.get(H);if(T===void 0)T=new Map,Dn.set(H,T);var C=T.get(y);if(C===void 0)H.addEventListener(y,On,{passive:h}),T.set(y,1);else T.set(y,C+1)}}};return p(lr(Vo)),Rn.add(p),()=>{for(var f of m)for(let h of[t,document]){var g=Dn.get(h),y=g.get(f);if(--y==0){if(h.removeEventListener(f,On),g.delete(f),g.size===0)Dn.delete(h)}else g.set(f,y)}if(Rn.delete(p),_!==r)_.parentNode?.removeChild(_)}});return Wo.set(l,u),l}var Wo=new WeakMap;function Zo(e,t){let r=Wo.get(e);if(r)return Wo.delete(e),r(t);if(c)Bi();return Promise.resolve()}class Ht{anchor;#e=new Map;#t=new Map;#r=new Map;#a=new Set;#o=!0;constructor(e,t=!0){this.anchor=e,this.#o=t}#n=(e)=>{if(!this.#e.has(e))return;var t=this.#e.get(e),r=this.#t.get(t);if(r)Tr(r),this.#a.delete(t);else{var n=this.#r.get(t);if(n){if(Tr(n.effect),this.#t.set(t,n.effect),this.#r.delete(t),c)n.fragment.lastChild[co]=this.anchor;n.fragment.lastChild.remove(),this.anchor.before(n.fragment),r=n.effect}}for(let[o,i]of this.#e){if(this.#e.delete(o),o===e)break;let s=this.#r.get(i);if(s)G(s.effect),this.#r.delete(i)}for(let[o,i]of this.#t){if(o===t||this.#a.has(o))continue;let s=()=>{if(Array.from(this.#e.values()).includes(o)){var l=document.createDocumentFragment();Ar(i,l),l.append(z()),this.#r.set(o,{effect:i,fragment:l})}else G(i);this.#a.delete(o),this.#t.delete(o)};if(this.#o||!r)this.#a.add(o),ht(i,s,!1);else s()}};#s=(e)=>{this.#e.delete(e);let t=Array.from(this.#e.values());for(let[r,n]of this.#r)if(!t.includes(r))G(n.effect),this.#r.delete(r)};ensure(e,t){var r=k,n=Nn();if(t&&!this.#t.has(e)&&!this.#r.has(e))if(n){var o=document.createDocumentFragment(),i=z();o.append(i),this.#r.set(e,{effect:ne(()=>t(i)),fragment:o})}else this.#t.set(e,ne(()=>t(this.anchor)));if(this.#e.set(r,e),n){for(let[s,a]of this.#t)if(s===e)r.unskip_effect(a);else r.skip_effect(a);for(let[s,a]of this.#r)if(s===e)r.unskip_effect(a.effect);else r.skip_effect(a.effect);r.oncommit(this.#n),r.ondiscard(this.#s)}else{if(w)this.anchor=A;this.#n(r)}}}function xt(e,t,r=!1){var n;if(w)n=A,xe();var o=new Ht(e),i=r?Oe:0;function s(a,l){if(w){var u=Xt(n);if(a!==parseInt(u.substring(1))){var _=pt();I(_),o.anchor=_,Q(!1),o.ensure(a,l),Q(!0);return}}o.ensure(a,l)}Ue(()=>{var a=!1;if(t((l,u=0)=>{a=!0,s(u,l)}),!a)s(-1,null)},i)}var tm=Symbol("NaN");function rf(e,t,r){var n=[],o=t.length,i,s=t.length;for(var a=0;a<o;a++){let m=t[a];ht(m,()=>{if(i){if(i.pending.delete(m),i.done.add(m),i.pending.size===0){var p=e.outrogroups;if(Jo(e,lr(i.done)),p.delete(i),p.size===0)e.outrogroups=null}}else s-=1},!1)}if(s===0){var l=n.length===0&&r!==null&&e.pending.size===0;if(l){var u=r,_=u.parentNode;Fr(_),_.append(u),e.items.clear()}Jo(e,t,!l)}else i={pending:new Set(t),done:new Set},(e.outrogroups??=new Set).add(i)}function Jo(e,t,r=!0){var n;if(e.pending.size>0){n=new Set;for(let s of e.pending.values())for(let a of s)n.add(e.items.get(a).e)}for(var o=0;o<t.length;o++){var i=t[o];if(n?.has(i)){i.f|=rt;let s=document.createDocumentFragment();Ar(i,s)}else G(t[o],r)}}var zs;function Wr(e,t,r,n,o,i=null){var s=e,a=new Map,l=(t&po)!==0;if(l){var u=e;s=w?I(me(u)):u.appendChild(z())}if(w)xe();var _=null,m=xr(()=>{var C=r();return bt(C)?C:C==null?[]:lr(C)});if(c)we(m,"{#each ...}");var p=new Map,f=!0;function g(C){if((T.effect.f&F)!==0)return;T.pending.delete(C);var H=d(m);if(T.fallback=_,nf(T,H,s,t,n),_!==null)if(H.length===0)if((_.f&rt)===0)Tr(_);else _.f^=rt,Kr(_,null,s);else ht(_,()=>{_=null})}function y(C){T.pending.delete(C)}var h=Ue(()=>{var C=d(m),H=C.length;let oe=!1;if(w){var j=Xt(s)===Ot;if(j!==(H===0))s=pt(),I(s),Q(!1),oe=!0}var ue=new Set,ye=k,Me=Nn();for(var Ne=0;Ne<H;Ne+=1){if(w&&A.nodeType===ze&&A.data===Kt)s=A,oe=!0,Q(!1);var Re=C[Ne],wt=n(Re,Ne);if(c){var qe=n(Re,Ne);if(wt!==qe)qi(String(Ne),String(wt),String(qe))}var et=f?null:a.get(wt);if(et){if(et.v)st(et.v,Re);if(et.i)st(et.i,Ne);if(Me)ye.unskip_effect(et.e)}else{if(et=of(a,f?s:zs??=z(),Re,wt,Ne,o,t,r),!f)et.e.f|=rt;a.set(wt,et)}ue.add(wt)}if(H===0&&i&&!_)if(f)_=ne(()=>i(s));else _=ne(()=>i(zs??=z())),_.f|=rt;if(H>ue.size)if(c)sf(C,n);else go("","","");if(w&&H>0)I(pt());if(!f)if(p.set(ye,ue),Me){for(let[Zr,Mn]of a)if(!ue.has(Zr))ye.skip_effect(Mn.e);ye.oncommit(g),ye.ondiscard(y)}else g(ye);if(oe)Q(!0);d(m)}),T={effect:h,flags:t,items:a,pending:p,outrogroups:null,fallback:_};if(f=!1,w)s=A}function zr(e){while(e!==null&&(e.f&se)===0)e=e.next;return e}function nf(e,t,r,n,o){var i=(n&Ci)!==0,s=t.length,a=e.items,l=zr(e.effect.first),u,_=null,m,p=[],f=[],g,y,h,T;if(i){for(T=0;T<s;T+=1)if(g=t[T],y=o(g,T),h=a.get(y).e,(h.f&rt)===0)h.nodes?.a?.measure(),(m??=new Set).add(h)}for(T=0;T<s;T+=1){if(g=t[T],y=o(g,T),h=a.get(y).e,e.outrogroups!==null)for(let Re of e.outrogroups)Re.pending.delete(h),Re.done.delete(h);if((h.f&Z)!==0){if(Tr(h),i)h.nodes?.a?.unfix(),(m??=new Set).delete(h)}if((h.f&rt)!==0)if(h.f^=rt,h===l)Kr(h,null,r);else{var C=_?_.next:l;if(h===e.effect.last)e.effect.last=h.prev;if(h.prev)h.prev.next=h.next;if(h.next)h.next.prev=h.prev;Vt(e,_,h),Vt(e,h,C),Kr(h,C,r),_=h,p=[],f=[],l=zr(_.next);continue}if(h!==l){if(u!==void 0&&u.has(h)){if(p.length<f.length){var H=f[0],oe;_=H.prev;var j=p[0],ue=p[p.length-1];for(oe=0;oe<p.length;oe+=1)Kr(p[oe],H,r);for(oe=0;oe<f.length;oe+=1)u.delete(f[oe]);Vt(e,j.prev,ue.next),Vt(e,_,j),Vt(e,ue,H),l=H,_=ue,T-=1,p=[],f=[]}else u.delete(h),Kr(h,l,r),Vt(e,h.prev,h.next),Vt(e,h,_===null?e.effect.first:_.next),Vt(e,_,h),_=h;continue}p=[],f=[];while(l!==null&&l!==h)(u??=new Set).add(l),f.push(l),l=zr(l.next);if(l===null)continue}if((h.f&rt)===0)p.push(h);_=h,l=zr(h.next)}if(e.outrogroups!==null){for(let Re of e.outrogroups)if(Re.pending.size===0)Jo(e,lr(Re.done)),e.outrogroups?.delete(Re);if(e.outrogroups.size===0)e.outrogroups=null}if(l!==null||u!==void 0){var ye=[];if(u!==void 0){for(h of u)if((h.f&Z)===0)ye.push(h)}while(l!==null){if((l.f&Z)===0&&l!==e.fallback)ye.push(l);l=zr(l.next)}var Me=ye.length;if(Me>0){var Ne=(n&po)!==0&&s===0?r:null;if(i){for(T=0;T<Me;T+=1)ye[T].nodes?.a?.measure();for(T=0;T<Me;T+=1)ye[T].nodes?.a?.fix()}rf(e,ye,Ne)}}if(i)J(()=>{if(m===void 0)return;for(h of m)h.nodes?.a?.apply()})}function of(e,t,r,n,o,i,s,a){var l=(s&Ni)!==0?(s&Oi)===0?rr(r,!1,!1):be(r):null,u=(s&Ri)!==0?be(o):null;if(c&&l)l.trace=()=>{a()[u?.v??o]};return{v:l,i:u,e:ne(()=>(i(t,l??r,u??o,a),()=>{e.delete(n)}))}}function Kr(e,t,r){if(!e.nodes)return;var n=e.nodes.start,o=e.nodes.end,i=t&&(t.f&rt)===0?t.nodes.start:r;while(n!==null){var s=ae(n);if(i.before(n),n===o)return;n=s}}function Vt(e,t,r){if(t===null)e.effect.first=r;else t.next=r;if(r===null)e.effect.last=t;else r.prev=t}function sf(e,t){let r=new Map,n=e.length;for(let o=0;o<n;o++){let i=t(e[o],o);if(r.has(i)){let s=String(r.get(i)),a=String(o),l=String(i);if(l.startsWith("[object "))l=null;go(s,a,l)}r.set(i,o)}}var Ws=[...` 	
\r\f \v\uFEFF`];function Xs(e,t,r){var n=e==null?"":""+e;if(t)n=n?n+" "+t:t;if(r){for(var o of Object.keys(r))if(r[o])n=n?n+" "+o:o;else if(n.length){var i=o.length,s=0;while((s=n.indexOf(o,s))>=0){var a=s+i;if((s===0||Ws.includes(n[s-1]))&&(a===n.length||Ws.includes(n[a])))n=(s===0?"":n.substring(0,s))+n.substring(a+1);else s=a}}}return n===""?null:n}function Ut(e,t,r,n,o,i){var s=e[Or];if(w||s!==r||s===void 0){var a=Xs(r,n,i);if(!w||a!==e.getAttribute("class"))if(a==null)e.removeAttribute("class");else if(t)e.className=a;else e.setAttribute("class",a);e[Or]=r}else if(i&&o!==i)for(var l in i){var u=!!i[l];if(o==null||u!==!!o[l])e.classList.toggle(l,u)}return i}var wf=Symbol("class"),bf=Symbol("style"),yf=Symbol("is custom element"),Ef=Symbol("is html"),$f=uo?"link":"LINK";function jt(e){if(!w)return;var t=!1,r=()=>{if(t)return;if(t=!0,e.hasAttribute("value")){var n=e.value;Xr(e,"value",null),e.value=n}if(e.hasAttribute("checked")){var o=e.checked;Xr(e,"checked",null),e.checked=o}};e[Ct]=r,J(r),xn()}function ei(e,t){var r=ea(e);if(r.checked===(r.checked=t??void 0))return;e.checked=t}function Xr(e,t,r,n){var o=ea(e);if(w){if(o[t]=e.getAttribute(t),t==="src"||t==="srcset"||t==="href"&&e.nodeName===$f){if(!n)Tf(e,t,r??"");return}}var i=o[t];if(i===(o[t]=r))return;if(t==="loading")e[Si]=r;if(r==null)e.removeAttribute(t);else if(typeof r!=="string"&&kf(e).has(t))e[t]=r;else if(!(w&&(typeof r==="number"||typeof r==="boolean")&&i===String(r)))e.setAttribute(t,r)}function ea(e){return e[fn]??={[yf]:e.nodeName.includes("-"),[Ef]:e.namespaceURI===ln}}var Zs=new Map;function kf(e){var t=e.getAttribute("is")||e.nodeName,r=Zs.get(t);if(r)return r;Zs.set(t,r=new Set);var n,o=e,i=Element.prototype;while(i!==o){n=oo(o);for(var s in n)if(n[s].set&&s!=="innerHTML"&&s!=="textContent"&&s!=="innerText")r.add(s);o=cr(o)}return r}function Tf(e,t,r){if(!c)return;if(t==="srcset"&&Af(e,r))return;if(Qo(e.getAttribute(t)??"",r))return;Pi(t,e.outerHTML.replace(e.innerHTML,e.innerHTML&&"..."),String(r))}function Qo(e,t){if(e===t)return!0;return new URL(e,document.baseURI).href===new URL(t,document.baseURI).href}function Js(e){return e.split(",").map((t)=>t.trim().split(" ").filter(Boolean))}function Af(e,t){var r=Js(e.srcset),n=Js(t);return n.length===r.length&&n.every(([o,i],s)=>i===r[s][1]&&(Qo(r[s][0],o)||Qo(o,r[s][0])))}function sr(e,t,r=t){var n=new WeakSet;if(wo(e,"input",async(o)=>{if(c&&e.type==="checkbox")ho();var i=o?e.defaultValue:e.value;if(i=ti(e)?ri(i):i,r(i),k!==null)n.add(k);if(await qr(),i!==(i=t())){var{selectionStart:s,selectionEnd:a}=e,l=e.value.length;if(e.value=i??"",a!==null){var u=e.value.length;if(s===a&&a===l&&u>l)e.selectionStart=u,e.selectionEnd=u;else e.selectionStart=s,e.selectionEnd=Math.min(a,u)}}}),w&&e.defaultValue!==e.value||V(t)==null&&e.value){if(r(ti(e)?ri(e.value):e.value),k!==null)n.add(k)}ge(()=>{if(c&&e.type==="checkbox")ho();var o=t();if(e===document.activeElement){var i=ee?We:k;if(n.has(i))return}if(ti(e)&&o===ri(e.value))return;if(e.type==="date"&&!o&&!e.value)return;if(o!==e.value)e.value=o??""})}function ti(e){var t=e.type;return t==="number"||t==="range"}function ri(e){return e===""?null:+e}var wx=Symbol("unmounted");function ta(e){return new ra(e)}class ra{#e;#t;constructor(e){var t=new Map,r=(o,i)=>{var s=rr(i,!1,!1);return t.set(o,s),s};let n=new Proxy({...e.props||{},$$events:{}},{get(o,i){return d(t.get(i)??r(i,Reflect.get(o,i)))},has(o,i){if(i===sn)return!0;return d(t.get(i)??r(i,Reflect.get(o,i))),Reflect.has(o,i)},set(o,i,s){return S(t.get(i)??r(i,s),s),Reflect.set(o,i,s)}});if(this.#t=(e.hydrate?Xo:Nr)(e.component,{target:e.target,anchor:e.anchor,props:n,context:e.context,intro:e.intro??!1,recover:e.recover,transformError:e.transformError}),!ee&&(!e?.props?.$$host||e.sync===!1))Ft();this.#e=n.$$events;for(let o of Object.keys(this.#t)){if(o==="$set"||o==="$destroy"||o==="$on")continue;ie(this,o,{get(){return this.#t[o]},set(i){this.#t[o]=i},enumerable:!0})}this.#t.$set=(o)=>{Object.assign(n,o)},this.#t.$destroy=()=>{Zo(this.#t)}}$set(e){this.#t.$set(e)}$on(e,t){this.#e[e]=this.#e[e]||[];let r=(...n)=>t.call(this,...n);return this.#e[e].push(r),()=>{this.#e[e]=this.#e[e].filter((n)=>n!==r)}}$destroy(){this.#t.$destroy()}}var Ff;if(typeof HTMLElement==="function")Ff=class extends HTMLElement{$$ctor;$$s;$$c;$$cn=!1;$$d={};$$r=!1;$$p_d={};$$l={};$$l_u=new Map;$$me;$$shadowRoot=null;constructor(e,t,r){super();if(this.$$ctor=e,this.$$s=t,r)this.$$shadowRoot=this.attachShadow(r)}addEventListener(e,t,r){if(this.$$l[e]=this.$$l[e]||[],this.$$l[e].push(t),this.$$c){let n=this.$$c.$on(e,t);this.$$l_u.set(t,n)}super.addEventListener(e,t,r)}removeEventListener(e,t,r){if(super.removeEventListener(e,t,r),this.$$c){let n=this.$$l_u.get(t);if(n)n(),this.$$l_u.delete(t)}}async connectedCallback(){if(this.$$cn=!0,!this.$$c){let e=function(n){return(o)=>{let i=vt("slot");if(n!=="default")i.name=n;W(o,i)}};if(await Promise.resolve(),!this.$$cn||this.$$c)return;let t={},r=Pf(this);for(let n of this.$$s)if(n in r)if(n==="default"&&!this.$$d.children)this.$$d.children=e(n),t.default=!0;else t[n]=e(n);for(let n of this.attributes){let o=this.$$g_p(n.name);if(!(o in this.$$d))this.$$d[o]=oi(o,n.value,this.$$p_d,"toProp")}for(let n in this.$$p_d)if(!(n in this.$$d)&&this[n]!==void 0)this.$$d[n]=this[n],delete this[n];this.$$c=ta({component:this.$$ctor,target:this.$$shadowRoot||this,props:{...this.$$d,$$slots:t,$$host:this}}),this.$$me=Po(()=>{ge(()=>{this.$$r=!0;for(let n of no(this.$$c)){if(!this.$$p_d[n]?.reflect)continue;this.$$d[n]=this.$$c[n];let o=oi(n,this.$$d[n],this.$$p_d,"toAttribute");if(o==null)this.removeAttribute(this.$$p_d[n].attribute||n);else this.setAttribute(this.$$p_d[n].attribute||n,o)}this.$$r=!1})});for(let n in this.$$l)for(let o of this.$$l[n]){let i=this.$$c.$on(n,o);this.$$l_u.set(o,i)}this.$$l={}}}attributeChangedCallback(e,t,r){if(this.$$r)return;e=this.$$g_p(e),this.$$d[e]=oi(e,r,this.$$p_d,"toProp"),this.$$c?.$set({[e]:this.$$d[e]})}disconnectedCallback(){this.$$cn=!1,Promise.resolve().then(()=>{if(!this.$$cn&&this.$$c)this.$$c.$destroy(),this.$$me(),this.$$c=void 0})}$$g_p(e){return no(this.$$p_d).find((t)=>this.$$p_d[t].attribute===e||!this.$$p_d[t].attribute&&t.toLowerCase()===e)||e}};function oi(e,t,r,n){let o=r[e]?.type;if(t=o==="Boolean"&&typeof t!=="boolean"?t!=null:t,!n||!r[e])return t;else if(n==="toAttribute")switch(o){case"Object":case"Array":return t==null?null:JSON.stringify(t);case"Boolean":return t?"":null;case"Number":return t==null?null:t;default:return t}else switch(o){case"Object":case"Array":return t&&JSON.parse(t);case"Boolean":return t;case"Number":return t!=null?+t:t;default:return t}}function Pf(e){let t={};return e.childNodes.forEach((r)=>{t[r.slot||"default"]=!0}),t}if(c){let e=function(t){if(!(t in globalThis)){let r;Object.defineProperty(globalThis,t,{configurable:!0,get:()=>{if(r!==void 0)return r;Zi(t)},set:(n)=>{r=n}})}};e("$state"),e("$effect"),e("$derived"),e("$inspect"),e("$props"),e("$bindable")}function na(e){if(N===null)dn("onMount");if(Mt&&N.l!==null)Vf(N).m.push(e);else kr(()=>{let t=V(e);if(typeof t==="function")return t})}function Vf(e){var t=e.l;return t.u??={a:[],b:[],m:[]}}var oa="5";if(typeof window<"u")((window.__svelte??={}).v??=new Set).add(oa);var Uf=Qe('<span class="svelte-194c7efawxkxi"> </span>'),jf=Qe('<div class="peringatan svelte-194c7efawxkxi"></div>'),ii=Qe('<p class="ket svelte-194c7efawxkxi"> </p>'),qf=Qe('<img alt=""/> <!>',1),Gf=Qe('<button class="stop svelte-194c7efawxkxi"> </button>'),zf=Qe("<button> </button>"),Kf=Qe('<small class="svelte-194c7efawxkxi"> </small>'),Wf=Qe('<button><b class="svelte-194c7efawxkxi"> </b> <span class="svelte-194c7efawxkxi"> </span></button>'),Xf=Qe('<p class="status svelte-194c7efawxkxi"> </p>'),Zf=Qe('<main class="svelte-194c7efawxkxi"><header class="svelte-194c7efawxkxi"><h1 class="svelte-194c7efawxkxi"> </h1> <span></span> <span class="kecil svelte-194c7efawxkxi"><!></span></header> <!> <section class="bicara svelte-194c7efawxkxi"><button> </button> <p class="ket kecil-ket svelte-194c7efawxkxi">Ketuk, lalu bicara ke mikrofon robot. Robot berhenti merekam saat kamu diam (paling lama 6 detik).</p> <label class="saklar svelte-194c7efawxkxi"><input type="checkbox" class="svelte-194c7efawxkxi"/> </label></section> <section class="kamera svelte-194c7efawxkxi"><!> <button class="kecil-btn svelte-194c7efawxkxi"> </button></section> <section class="svelte-194c7efawxkxi"><h2 class="svelte-194c7efawxkxi">Gerak <small class="svelte-194c7efawxkxi">tahan untuk jalan, lepas untuk berhenti</small></h2> <div class="dpad svelte-194c7efawxkxi"></div> <label class="svelte-194c7efawxkxi"> <input type="range" min="100" max="255" class="svelte-194c7efawxkxi"/></label></section> <section class="svelte-194c7efawxkxi"><h2 class="svelte-194c7efawxkxi">Kepala</h2> <label class="svelte-194c7efawxkxi"> <input type="range" min="-60" max="60" class="svelte-194c7efawxkxi"/></label> <label class="svelte-194c7efawxkxi"> <input type="range" min="40" max="150" class="svelte-194c7efawxkxi"/></label> <button class="kecil-btn svelte-194c7efawxkxi">Lurus ke depan</button></section> <section class="svelte-194c7efawxkxi"><h2 class="svelte-194c7efawxkxi">Musik dan suara</h2> <div class="baris svelte-194c7efawxkxi"><button class="svelte-194c7efawxkxi">Lagu berikutnya</button> <button class="svelte-194c7efawxkxi">Radio berikutnya</button> <button class="svelte-194c7efawxkxi">Diam</button></div> <label class="svelte-194c7efawxkxi"> <input type="range" min="0" max="21" class="svelte-194c7efawxkxi"/></label> <form class="svelte-194c7efawxkxi"><input type="text" placeholder="Suruh Zond bilang..." maxlength="140" class="svelte-194c7efawxkxi"/> <button type="submit" class="svelte-194c7efawxkxi">Bilang</button></form></section> <section class="svelte-194c7efawxkxi"><h2 class="svelte-194c7efawxkxi">Otak AI <!></h2> <div class="ai svelte-194c7efawxkxi"></div> <!> <p class="ket kecil-ket svelte-194c7efawxkxi">Kalau pilihanmu gagal (jatah habis atau saldo kosong), robot otomatis memakai yang gratis.</p></section> <!></main>');function Jf(e,t){Mr(t,!0);let r=Y(!1);async function n(){S(r,!0),await j("/api/bicara"),setTimeout(()=>S(r,!1),1500)}async function o(v){let x=v.currentTarget.checked;await j(`/api/dengar?on=${x?1:0}`),await ue()}let i=[{id:"otomatis",nama:"Otomatis",ket:"gratis: Gemini, lalu Gemma, lalu Groq"},{id:"gemini",nama:"Gemini",ket:"gratis, jatah harian kecil"},{id:"gemma",nama:"Gemma",ket:"gratis, jatah besar"},{id:"groq",nama:"Groq",ket:"gratis, cepat"},{id:"haiku",nama:"Claude Haiku",ket:"berbayar per pemakaian"}],s=Y(""),a=Y("");async function l(v){S(s,v,!0),S(a,"");let x=await j(`/api/ai?m=${v}`);if(!x)S(a,"Robot tidak menjawab");else if(!x.ok)S(a,await x.text(),!0);await ue(),S(s,"")}let u=Y(null),_=Y(!1),m=Y(160),p=Y(null),f=Y(0),g=Y(90),y=Y(12),h=!1,T=Y(""),C=Y(!0),H=Y(!1),oe=Y("");async function j(v,x=1500){let R=new AbortController,Le=setTimeout(()=>R.abort(),x);try{return await fetch(v,{signal:R.signal,cache:"no-store"})}catch{return null}finally{clearTimeout(Le)}}async function ue(){let v=await j("/api/status");if(!v||!v.ok){S(_,!1);return}if(S(_,!0),S(u,await v.json(),!0),!h)S(y,d(u).vol,!0)}let ye={maju:[1,1],mundur:[-1,-1],kiri:[-1,1],kanan:[1,-1],majukiri:[0.35,1],majukanan:[1,0.35],mundurkiri:[-0.35,-1],mundurkanan:[-1,-0.35]},Me=null,Ne=!1;function Re(){if(!d(p)||Ne)return;let[v,x]=ye[d(p)];Ne=!0,j(`/api/gerak?l=${Math.round(v*d(m))}&r=${Math.round(x*d(m))}`,600).finally(()=>Ne=!1)}function wt(v){if(S(p,v,!0),Re(),Me)clearInterval(Me);Me=setInterval(Re,150)}function qe(){if(Me)clearInterval(Me);if(Me=null,d(p))j("/api/stop");S(p,null)}function et(v,x){v.currentTarget.setPointerCapture(v.pointerId),wt(x)}let Zr={ArrowUp:"maju",w:"maju",ArrowDown:"mundur",s:"mundur",ArrowLeft:"kiri",a:"kiri",ArrowRight:"kanan",d:"kanan"};function Mn(v){if(v.target.tagName==="INPUT")return;if(v.key===" "){v.preventDefault(),qe(),j("/api/stop");return}let x=Zr[v.key];if(x&&!v.repeat)v.preventDefault(),wt(x)}function sa(v){if(Zr[v.key])qe()}let si=0,Ln=null;function Fn(){let v=Date.now();if(Ln)clearTimeout(Ln);let x=()=>{si=Date.now(),j(`/api/kepala?p=${90-d(f)}&t=${d(g)}`)};if(v-si>150)x();else Ln=setTimeout(x,150)}function aa(){S(f,0),S(g,90),Fn()}async function fa(){h=!0,await j(`/api/volume?v=${d(y)}`),setTimeout(()=>h=!1,1500)}async function la(v){v.preventDefault();let x=d(T).trim();if(!x)return;S(T,""),await j(`/api/bilang?t=${encodeURIComponent(x)}`,3000)}function Jr(v){setTimeout(()=>{if(!d(C)||!d(u)?.kamera){if(d(C))Jr(1000);return}S(oe,`http://${d(u).kamera}/capture?res=vga&t=${Date.now()}`)},v)}function ca(v){let x=[];if(v&1)x.push("Tombol STOP terkunci");if(v&2)x.push("Baterai lemah");if(v&4)x.push("Halangan depan");if(v&8)x.push("Halangan belakang");if(v&16)x.push("Robot miring");return x}na(()=>{ue();let v=setInterval(ue,1000),x=()=>qe(),R=()=>document.hidden&&qe();return window.addEventListener("blur",x),document.addEventListener("visibilitychange",R),()=>{clearInterval(v),window.removeEventListener("blur",x),document.removeEventListener("visibilitychange",R),qe()}}),kr(()=>{if(d(C))Jr(0)});var Pn=Zf();gt("keydown",$r,Mn),gt("keyup",$r,sa);var Bn=K(Pn),ai=K(Bn),ua=Ae(ai,!0),fi=D(ai,2);let li;var ci=D(fi,2),pa=K(ci);{var da=(v)=>{var x=Gr("otak tidak terhubung");W(v,x)},_a=(v)=>{var x=Gr("badan tidak menjawab");W(v,x)},ma=(v)=>{var x=Gr();Se((R)=>X(x,`${R??""} V · depan ${d(u).depan??""} cm · belakang ${d(u).belakang??""} cm`),[()=>d(u).baterai.toFixed(1)]),W(v,x)};xt(pa,(v)=>{if(!d(_))v(da);else if(!d(u)?.hidup)v(_a,1);else v(ma,-1)})}B(ci),B(Bn);var ui=D(Bn,2);{var va=(v)=>{var x=jf();Wr(x,20,()=>ca(d(u).flag),(R)=>R,(R,Le)=>{var lt=Uf(),St=Ae(lt,!0);Se(()=>X(St,Le)),W(R,lt)}),B(x),W(v,x)};xt(ui,(v)=>{if(d(u)&&d(u).flag)v(va)})}var Yn=D(ui,2),Rr=K(Yn);let pi;var ha=Ae(Rr,!0),di=D(Rr,4),Cr=K(di);jt(Cr);var ga=D(Cr);B(di),B(Yn);var Hn=D(Yn,2),_i=K(Hn);{var xa=(v)=>{var x=qf(),R=Sn(x);let Le;var lt=D(R,2);{var St=(qt)=>{var ar=ii(),ro=Ae(ar);Se(()=>X(ro,`Kamera tidak terhubung (${d(u).kamera??""})`)),W(qt,ar)};xt(lt,(qt)=>{if(d(H))qt(St)})}Se(()=>{Xr(R,"src",d(oe)),Le=Ut(R,1,"svelte-194c7efawxkxi",null,Le,{redup:d(H)})}),gt("load",R,()=>{S(H,!1),Jr(100)}),gt("error",R,()=>{S(H,!0),Jr(1500)}),Uo(R),W(v,x)},wa=(v)=>{var x=ii(),R=Ae(x,!0);Se(()=>X(R,d(u)?.kamera?"Kamera dimatikan":"Alamat kamera belum diketahui")),W(v,x)};xt(_i,(v)=>{if(d(C)&&d(u)?.kamera)v(xa);else v(wa,-1)})}var mi=D(_i,2),ba=Ae(mi,!0);B(Hn);var Vn=D(Hn,2),Qr=D(K(Vn),2);Wr(Qr,20,()=>[["majukiri","↖"],["maju","▲"],["majukanan","↗"],["kiri","◀"],["stop","■"],["kanan","▶"],["mundurkiri","↙"],["mundur","▼"],["mundurkanan","↘"]],([v,x])=>v,(v,x)=>{var R=ko(()=>ao(x,2));let Le=()=>d(R)[0],lt=()=>d(R)[1];var St=Go(),qt=Sn(St);{var ar=(fr)=>{var tt=Gf(),en=Ae(tt,!0);Se(()=>X(en,lt())),ce("pointerdown",tt,()=>{qe(),j("/api/stop")}),W(fr,tt)},ro=(fr)=>{var tt=zf();let en;var Ra=Ae(tt,!0);Se(()=>{en=Ut(tt,1,"svelte-194c7efawxkxi",null,en,{aktif:d(p)===Le()}),X(Ra,lt())}),ce("pointerdown",tt,(Ca)=>et(Ca,Le())),ce("pointerup",tt,qe),gt("pointercancel",tt,qe),gt("lostpointercapture",tt,qe),W(fr,tt)};xt(qt,(fr)=>{if(Le()==="stop")fr(ar);else fr(ro,-1)})}W(v,St)}),B(Qr);var vi=D(Qr,2),hi=K(vi),gi=D(hi);jt(gi),B(vi),B(Vn);var Un=D(Vn,2),jn=D(K(Un),2),xi=K(jn),qn=D(xi);jt(qn),B(jn);var Gn=D(jn,2),wi=K(Gn),zn=D(wi);jt(zn),B(Gn);var ya=D(Gn,2);B(Un);var Kn=D(Un,2),Wn=D(K(Kn),2),bi=K(Wn),yi=D(bi,2),Ea=D(yi,2);B(Wn);var Xn=D(Wn,2),Ei=K(Xn),Zn=D(Ei);jt(Zn),B(Xn);var Jn=D(Xn,2),$i=K(Jn);jt($i),_r(2),B(Jn),B(Kn);var Qn=D(Kn,2),eo=K(Qn),$a=D(K(eo));{var ka=(v)=>{var x=Kf(),R=Ae(x);Se(()=>X(R,`terakhir menjawab: ${d(u).aiTerakhir??""}`)),W(v,x)};xt($a,(v)=>{if(d(u)?.aiTerakhir)v(ka)})}B(eo);var to=D(eo,2);Wr(to,21,()=>i,(v)=>v.id,(v,x)=>{var R=Wf();let Le;var lt=K(R),St=Ae(lt,!0),qt=D(lt,2),ar=Ae(qt,!0);B(R),Se(()=>{R.disabled=!d(u)?.aiTersedia?.[d(x).id]||d(s)!=="",Le=Ut(R,1,"svelte-194c7efawxkxi",null,Le,{pilih:d(u)?.ai===d(x).id}),X(St,d(x).nama),X(ar,d(u)?.aiTersedia?.[d(x).id]===!1?"API key belum diisi":d(x).ket)}),ce("click",R,()=>l(d(x).id)),W(v,R)}),B(to);var Ta=D(to,2);{var Aa=(v)=>{var x=ii(),R=Ae(x,!0);Se(()=>X(R,d(a))),W(v,x)};xt(Ta,(v)=>{if(d(a))v(Aa)})}_r(2),B(Qn);var Sa=D(Qn,2);{var Na=(v)=>{var x=Xf(),R=Ae(x,!0);Se(()=>X(R,d(u).status)),W(v,x)};xt(Sa,(v)=>{if(d(u)?.status)v(Na)})}B(Pn),Se(()=>{X(ua,d(u)?.nama??"Robot"),li=Ut(fi,1,"titik svelte-194c7efawxkxi",null,li,{on:d(_)&&d(u)?.hidup}),pi=Ut(Rr,1,"mik svelte-194c7efawxkxi",null,pi,{dengar:d(u)?.mendengar||d(r)}),Rr.disabled=!d(_),X(ha,d(u)?.mendengar?"Mendengarkan... bicara sekarang":"Bicara ke "+(d(u)?.nama??"robot")),ei(Cr,d(u)?.dengarTerus??!1),Cr.disabled=!d(_),X(ga,` Dengar terus (cukup sebut "${d(u)?.nama??"nama robot"??""}"). Boros: setiap suara di ruangan memakai jatah suara-ke-teks.`),X(ba,d(C)?"Matikan kamera":"Nyalakan kamera"),X(hi,`Kecepatan ${d(m)??""}`),X(xi,`Toleh ${d(f)<0?"kiri":d(f)>0?"kanan":"lurus"}`),X(wi,`Angguk ${d(g)??""}°`),X(Ei,`Volume ${d(y)??""}`)}),ce("click",Rr,n),ce("change",Cr,o),ce("click",mi,()=>S(C,!d(C))),ce("contextmenu",Qr,(v)=>v.preventDefault()),sr(gi,()=>d(m),(v)=>S(m,v)),ce("input",qn,Fn),sr(qn,()=>d(f),(v)=>S(f,v)),ce("input",zn,Fn),sr(zn,()=>d(g),(v)=>S(g,v)),ce("click",ya,aa),ce("click",bi,()=>j("/api/lagu",4000)),ce("click",yi,()=>j("/api/radio",4000)),ce("click",Ea,()=>j("/api/diam")),ce("change",Zn,fa),sr(Zn,()=>d(y),(v)=>S(y,v)),gt("submit",Jn,la),sr($i,()=>d(T),(v)=>S(T,v)),W(e,Pn),Lr()}var ia=Jf;Cn(["click","change","contextmenu","pointerdown","pointerup","input"]);Nr(ia,{target:document.body});
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
    d["dengarTerus"] = (bool)dengarTerus; d["mendengar"] = ekspresi == DENGAR;
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
  web.on("/api/bicara", HTTP_GET, [] { kirimAudio(A_STOP); mintaDengar = true; jawabOk(); });   // dengarkan satu kalimat
  web.on("/api/dengar", HTTP_GET, [] { aturDengarTerus(web.arg("on") == "1"); jawabOk(); });
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
  dengarTerus = pref.getUChar("dengar", 0) != 0;                        // bawaan mati: hemat jatah suara-ke-teks
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
    configTzTime(ZONA_WAKTU, "pool.ntp.org", "time.google.com");   // jam dari internet, berjalan sendiri di latar
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
