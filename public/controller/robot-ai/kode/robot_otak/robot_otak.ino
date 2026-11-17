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
#define OTA_SANDI        ""                       // kata sandi upload lewat WiFi. Kosong = tanpa sandi (cukup untuk WiFi pribadi)
#define CUACA_KOTA       "Mataram"                // lokasi cuaca di layar. Angka lintang/bujur dari Google Maps (klik kanan peta)
#define CUACA_LAT        "-8.583"
#define CUACA_LON        "116.116"
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
#ifndef OTA_SANDI
#define OTA_SANDI ""
#endif
#ifndef CUACA_KOTA
#define CUACA_KOTA "Mataram"
#define CUACA_LAT "-8.583"
#define CUACA_LON "116.116"
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
enum Mode { M_BIASA, M_IKUTI, M_JAGA };
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

// ======================= AUDIO (task sendiri) =======================
enum JenisAudio { A_FILE, A_URL, A_TTS, A_STOP, A_VOL };
int volSekarang = VOLUME_AWAL;   // volume yang sedang dipakai, 0..21
struct PerintahAudio { JenisAudio jenis; char isi[200]; };

// Semua jenis data (enum/struct) harus dibuat sebelum fungsi pertama: Arduino IDE menaruh deklarasi
// fungsi otomatis tepat sebelum fungsi pertama. Fungsi dengan nilai bawaan dideklarasikan sendiri di sini.
void kirimAudio(JenisAudio j, const String& isi = "");

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

// ======================= CUACA (Open-Meteo, gratis tanpa API key) =======================
struct Cuaca { bool ada = false; float suhu = 0, tertinggi = 0, terendah = 0; int lembap = 0, kode = 0, hujan = 0; };
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
                "&daily=temperature_2m_max,temperature_2m_min,precipitation_probability_max&timezone=auto&forecast_days=1");
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
  c2.ada = c2.kode >= 0;
  cuaca = c2;
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
    "{\"m\":[kiri,kanan,ms]} maju atau mundur, kiri/kanan -200..200, ms 100..2000 (maju [150,150,800], mundur [-150,-150,800]); "
    "{\"belok\":derajat} berputar di tempat dengan tepat, positif = kanan, negatif = kiri (90 = seperempat putaran, 180 = balik arah); "
    "{\"kepala\":[pan,tilt]} arah kepala dalam derajat, 90,90 = lurus ke depan, pan 30=kanan 150=kiri; "
    "{\"foto\":1} ambil foto dari kameramu, HANYA kalau diminta melihat sesuatu; "
    "{\"diam\":1} hentikan musik; {\"mode\":\"ikuti\"} ikuti benda di depanmu (misalnya kaki pemilik), "
    "{\"mode\":\"jaga\"} diam dan berjaga, {\"mode\":\"biasa\"} kembali normal; "
    "{\"ingatkan\":[\"HH:MM\",\"pesan\",\"sekali\" atau \"harian\"]} pasang alarm/pengingat (jam 24 jam; untuk \"10 menit lagi\" hitung sendiri jamnya dari waktu sekarang). "
    + String(fiturKamera ? "{\"sekeliling\":1} menoleh kiri, depan, kanan lalu menceritakan sekitarmu; {\"suit\":1} main suit (batu-gunting-kertas) lewat kamera. " : "") +
    "Kosongkan aksi kalau tidak perlu. "
    "Badanmu otomatis menolak maju kalau ada halangan dekat, jadi tidak perlu khawatir menabrak. "
    "Data sensor terbaru: " + ringkasSensor() + ". " + teksCuaca(true) +
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
  "Perintah:\n/status  data sensor\n/cuaca  cuaca hari ini\n/maju /mundur /kiri /kanan [ms]\n/belok 90 (kanan) /belok -90 (kiri) /balik\n/stop\n/kepala pan tilt  (90 90 = lurus)\n"
  "/alarm  daftar alarm\n/alarm 06:30 pesan  sekali\n/alarm harian 06:30 pesan\n/alarm hapus 2\n"
  "/sekeliling /suit  (aktifkan dulu fitur kamera AI di panel web)\n"
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
    PesanKirim pk;                                       // pesan dari tugas lain (pengingat, dll.)
    while (antrianKirim && xQueueReceive(antrianKirim, &pk, 0) == pdTRUE) bot.sendMessage(TELEGRAM_CHATID, pk.isi);
    if (aksiTertunda) {                                  // dari panel web atau perintah suara
      int a = aksiTertunda; aksiTertunda = 0;
      if (a == 1) lihatSekeliling(TELEGRAM_CHATID);
      else if (a == 2) mainSuit(TELEGRAM_CHATID);
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
  if (otaJalan) {                                          // upload lewat WiFi: tampilkan kemajuannya saja
    wajah.setFont(&fonts::Font2); wajah.setTextColor(TFT_WHITE); wajah.setTextDatum(middle_center);
    wajah.drawString("Memperbarui kode lewat WiFi", 160, 70);
    wajah.drawRect(40, 92, 240, 18, TFT_CYAN);
    wajah.fillRect(42, 94, 236 * otaPersen / 100, 14, TFT_CYAN);
    wajah.drawString(String(otaPersen) + " %  -  jangan matikan robot", 160, 128);
    xSemaphoreTake(kunciSPI, portMAX_DELAY); wajah.pushSprite(0, 0); xSemaphoreGive(kunciSPI);
    return;
  }
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
  if (jam.length()) {                                       // baris kedua: tanggal, lalu cuaca kalau ada
    wajah.setTextDatum(top_center);
    String c = teksCuaca(false), tgl = teksTanggal();
    struct tm tt;
    if (c.length() && jamSiap(tt)) {                        // ada cuaca: nama bulan disingkat supaya muat
      char b[40]; snprintf(b, sizeof(b), "%s, %d %.3s %d  ", NAMA_HARI[tt.tm_wday], tt.tm_mday, NAMA_BULAN[tt.tm_mon], tt.tm_year + 1900);
      tgl = String(b) + c;
    }
    wajah.drawString(tgl.c_str(), 160, 20);
  }

  Ekspresi e = ekspresi;
  uint16_t warna = (e == KAGET) ? TFT_ORANGE : (e == LEMAH || e == NGANTUK) ? 0x7BEF : (e == DENGAR) ? TFT_GREEN : (e == PUSING) ? 0xFD20 : TFT_CYAN;
  int geser = arahMata * 14;
  int tinggi = kedip ? 6 : (e == KAGET ? 62 : e == LEMAH ? 18 : e == NGANTUK ? 8 : 50);
  for (int s = -1; s <= 1; s += 2) {
    int cx = 160 + s * 62 + geser, cy = 80;
    if (e == PUSING) {                                      // mata berputar: lingkaran bertumpuk yang bergeser
      int putaran = (millis() / 120) % 8;
      for (int r = 6; r <= 24; r += 6) wajah.drawCircle(cx + (putaran % 4 - 2) * (r / 6), cy + (putaran / 4) * 2, r, warna);
      continue;
    }
    int y = (e == NGANTUK) ? cy + 12 : cy;                  // mengantuk: kelopak turun
    wajah.fillRoundRect(cx - 26, y - tinggi / 2, 52, tinggi, e == NGANTUK ? 4 : 14, warna);
    if (e == PIKIR && !kedip) wajah.fillCircle(cx + 10, cy - 10, 7, TFT_BLACK);   // melirik ke atas
  }
  if (e == NGANTUK) {                                       // "z z z" naik pelan di kanan atas
    wajah.setTextDatum(middle_center); wajah.setTextColor(0x7BEF);
    int n = (millis() / 600) % 3;
    for (int i = 0; i <= n; i++) wajah.drawString("z", 236 + i * 12, 58 - i * 10);
  }
  // mulut
  int my = 132;
  if (sedangBicara) { int h = 4 + (millis() / 90) % 3 * 6; wajah.fillRoundRect(130, my - h / 2, 60, h, 4, warna); }
  else if (e == KAGET) wajah.drawCircle(160, my, 9, warna);
  else if (e == PUSING) { for (int i = 0; i < 4; i++) wajah.drawLine(136 + i * 12, my + (i % 2 ? 4 : -4), 148 + i * 12, my + (i % 2 ? -4 : 4), warna); }
  else if (e == NGANTUK || e == LEMAH) wajah.drawFastHLine(144, my, 32, warna);
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
const char PANEL_HTML[] = R"PANEL(<!doctype html><html lang="id"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1"><title>Panel Zond</title><style>html{color:#e6e9ee;background:#10141a;font-family:system-ui,-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,Noto Sans,Ubuntu,Cantarell,Helvetica Neue,sans-serif}body{margin:0}main.svelte-2hlthq23qytbf{max-width:480px;margin:0 auto;padding:12px 16px 32px}header.svelte-2hlthq23qytbf{display:flex;flex-wrap:wrap;align-items: center;gap:8px}h1.svelte-2hlthq23qytbf{margin:0;font-size:1.4rem}h2.svelte-2hlthq23qytbf{margin:0 0 8px;font-size:1rem}h2.svelte-2hlthq23qytbf small:where(.svelte-2hlthq23qytbf){color:#8a94a3;font-weight:400}.kecil.svelte-2hlthq23qytbf{color:#8a94a3;font-size:.85rem}.titik.svelte-2hlthq23qytbf{background:#c0392b;border-radius:50%;width:10px;height:10px}.titik.on.svelte-2hlthq23qytbf{background:#2ecc71}.peringatan.svelte-2hlthq23qytbf{display:flex;flex-wrap:wrap;gap:6px;margin-top:8px}.peringatan.svelte-2hlthq23qytbf span:where(.svelte-2hlthq23qytbf){background:#7a1d1d;border-radius:6px;padding:3px 8px;font-size:.85rem}section.svelte-2hlthq23qytbf{background:#18202a;border-radius:12px;margin-top:12px;padding:12px}.kamera.svelte-2hlthq23qytbf{text-align:center}.kamera.svelte-2hlthq23qytbf img:where(.svelte-2hlthq23qytbf){display:block;aspect-ratio:4/3;object-fit:cover;background:#000;border-radius:8px;width:100%}.kamera.svelte-2hlthq23qytbf img.redup:where(.svelte-2hlthq23qytbf){opacity:.15}.ket.svelte-2hlthq23qytbf{color:#8a94a3;margin:8px 0}.dpad.svelte-2hlthq23qytbf{display:grid;grid-template-columns:repeat(3,1fr);touch-action:none;user-select:none;-webkit-user-select:none;gap:8px;max-width:300px;margin:0 auto 12px}.dpad.svelte-2hlthq23qytbf button:where(.svelte-2hlthq23qytbf){aspect-ratio:1;touch-action:none;background:#2a3a4d;border-radius:14px;font-size:1.6rem}.dpad.svelte-2hlthq23qytbf button.aktif:where(.svelte-2hlthq23qytbf){background:#3d7bd9}.dpad.svelte-2hlthq23qytbf button.stop:where(.svelte-2hlthq23qytbf){background:#a0302a}button.svelte-2hlthq23qytbf{color:inherit;cursor:pointer;background:#2a3a4d;border:0;border-radius:10px;padding:10px 12px;font-size:.95rem}button.svelte-2hlthq23qytbf:active{filter:brightness(1.3)}.kecil-btn.svelte-2hlthq23qytbf{margin-top:8px;padding:6px 10px;font-size:.85rem}.baris.svelte-2hlthq23qytbf{display:flex;flex-wrap:wrap;gap:8px}.baris.svelte-2hlthq23qytbf button:where(.svelte-2hlthq23qytbf){flex:1}label.svelte-2hlthq23qytbf{display:block;margin-top:10px;font-size:.9rem}input[type=range].svelte-2hlthq23qytbf{width:100%}form.svelte-2hlthq23qytbf{display:flex;gap:8px;margin-top:12px}input[type=text].svelte-2hlthq23qytbf{color:inherit;background:#10141a;border:1px solid #2a3a4d;border-radius:10px;flex:1;min-width:0;padding:10px;font-size:1rem}.mik.svelte-2hlthq23qytbf{background:#1f6f3a;border-radius:14px;width:100%;padding:16px;font-size:1.1rem;font-weight:600}.mik.dengar.svelte-2hlthq23qytbf{animation:1s ease-in-out infinite svelte-2hlthq23qytbf-denyut;background:#c2410c}@keyframes svelte-2hlthq23qytbf-denyut{50%{filter:brightness(1.3)}}.mik.svelte-2hlthq23qytbf:disabled{opacity:.5}.saklar.svelte-2hlthq23qytbf{display:flex;color:#8a94a3;align-items: flex-start;gap:8px;font-size:.8rem}.saklar.svelte-2hlthq23qytbf input:where(.svelte-2hlthq23qytbf){margin-top:2px}.ai.svelte-2hlthq23qytbf{display:grid;grid-template-columns:1fr 1fr;gap:8px}.ai.svelte-2hlthq23qytbf button:where(.svelte-2hlthq23qytbf){text-align:left;display:flex;flex-direction:column;gap:2px}.ai.svelte-2hlthq23qytbf button:where(.svelte-2hlthq23qytbf):first-child{grid-column:1/-1}.ai.svelte-2hlthq23qytbf button:where(.svelte-2hlthq23qytbf) span:where(.svelte-2hlthq23qytbf){color:#8a94a3;font-size:.75rem}.ai.svelte-2hlthq23qytbf button.pilih:where(.svelte-2hlthq23qytbf){outline:2px solid #5fcf72;background:#2f6a3a}.ai.svelte-2hlthq23qytbf button.pilih:where(.svelte-2hlthq23qytbf) span:where(.svelte-2hlthq23qytbf){color:#cfe9d4}.ai.svelte-2hlthq23qytbf button:where(.svelte-2hlthq23qytbf):disabled{opacity:.45;cursor:default}.kecil-ket.svelte-2hlthq23qytbf{margin-bottom:0;font-size:.8rem}.waktu.svelte-2hlthq23qytbf{color:#8a94a3;margin:6px 0 0;font-size:.85rem}.belok.svelte-2hlthq23qytbf{margin-top:10px}.alarm.svelte-2hlthq23qytbf{list-style:none;margin:0 0 8px;padding:0}.alarm.svelte-2hlthq23qytbf li:where(.svelte-2hlthq23qytbf){display:flex;border-bottom:1px solid #243041;align-items: center;gap:10px;padding:8px 0}.alarm.svelte-2hlthq23qytbf li:where(.svelte-2hlthq23qytbf) b:where(.svelte-2hlthq23qytbf){font-size:1.1rem}.alarm.svelte-2hlthq23qytbf li:where(.svelte-2hlthq23qytbf) span:where(.svelte-2hlthq23qytbf){display:flex;flex-direction:column;flex:1}.alarm.svelte-2hlthq23qytbf li:where(.svelte-2hlthq23qytbf) small:where(.svelte-2hlthq23qytbf){color:#8a94a3;font-size:.75rem}.hapus.svelte-2hlthq23qytbf{background:#3a2430;padding:4px 10px}.form-alarm.svelte-2hlthq23qytbf{flex-direction:column;gap:8px}.form-alarm.svelte-2hlthq23qytbf .baris:where(.svelte-2hlthq23qytbf)>:where(.svelte-2hlthq23qytbf){flex:1}input[type=time].svelte-2hlthq23qytbf,select.svelte-2hlthq23qytbf{color:inherit;background:#10141a;border:1px solid #2a3a4d;border-radius:10px;min-width:0;padding:9px;font-size:.95rem}.hari.svelte-2hlthq23qytbf{display:flex;flex-wrap:wrap;gap:6px 12px;font-size:.85rem}.hari.svelte-2hlthq23qytbf label:where(.svelte-2hlthq23qytbf){display:flex;align-items: center;gap:4px;margin:0}.status.svelte-2hlthq23qytbf{text-align:center;color:#8a94a3;margin-top:12px}
</style></head><body><script type="module">var u=!1;var lt=Array.isArray,ws=Array.prototype.indexOf,Ot=Array.prototype.includes,yr=Array.from,{keys:Ho,defineProperty:_e,getOwnPropertyDescriptor:Ve,getOwnPropertyDescriptors:Vo,prototype:jo}=Object,$s=Array.prototype,{getPrototypeOf:xr,isExtensible:Uo}=Object;var vt=()=>{};function xn(e){for(var t=0;t<e.length;t++)e[t]()}function wn(){var e,t,r=new Promise((n,o)=>{e=n,t=o});return{promise:r,resolve:e,reject:t}}function Go(e,t){if(Array.isArray(e))return e;if(t===void 0||!(Symbol.iterator in e))return Array.from(e);let r=[];for(let n of e)if(r.push(n),r.length===t)break;return r}var K=2,gt=4,Yt=8,rr=16777216,$e=16,he=32,Fe=64,wr=128,$n=256,Ie=512,H=1024,I=2048,Z=4096,oe=8192,j=16384,De=32768,It=33554432,Pe=65536,Ht=131072,zo=262144,bt=524288,Es=1048576,ft=33554432,$r=2097152,je=4194304,tt=8388608,Ue=Symbol("$state"),En=Symbol("component"),kn=Symbol("legacy props"),ks=Symbol(""),Tn=Symbol("proxy path"),An=Symbol("attributes"),Gr=Symbol("class"),Ko=Symbol("style"),zr=Symbol("text"),Vt=Symbol("form reset"),Wo=Symbol("hmr anchor"),Dt=new class extends Error{name="StaleReactionError";message="The reaction that called `getAbortSignal()` was re-run or destroyed"},Xo=!!globalThis.document?.contentType&&globalThis.document.contentType.includes("xml");var nr=3,rt=8;var Ts=1,As=2,Zo=4,Ss=8,qs=16;var Ns=1,Rs=2;var Er="[",jt="[!",Jo="[?",or="]",Ge={};var U=Symbol("uninitialized"),nt=Symbol("filename"),Qo=Symbol("hmr"),Sn="http://www.w3.org/1999/xhtml";var Mt="font-weight: bold",Lt="font-weight: normal";function Cs(e){if(u)console.warn(`%c[svelte] await_reactivity_loss
%cDetected reactivity loss when reading \`${e}\`. This happens when state is read in an async function after an earlier \`await\`
https://svelte.dev/e/await_reactivity_loss`,Mt,Lt);else console.warn("https://svelte.dev/e/await_reactivity_loss")}function Os(e,t){if(u)console.warn(`%c[svelte] await_waterfall
%cAn async derived, \`${e}\` (${t}) was not read immediately after it resolved. This often indicates an unnecessary waterfall, which can slow down your app
https://svelte.dev/e/await_waterfall`,Mt,Lt);else console.warn("https://svelte.dev/e/await_waterfall")}function Is(){if(u)console.warn(`%c[svelte] derived_inert
%cReading a derived belonging to a now-destroyed effect may result in stale values
https://svelte.dev/e/derived_inert`,Mt,Lt);else console.warn("https://svelte.dev/e/derived_inert")}function Ds(e,t,r){if(u)console.warn(`%c[svelte] hydration_attribute_changed
%cThe \`${e}\` attribute on \`${t}\` changed its value between server and client renders. The client value, \`${r}\`, will be ignored in favour of the server value
https://svelte.dev/e/hydration_attribute_changed`,Mt,Lt);else console.warn("https://svelte.dev/e/hydration_attribute_changed")}function ir(e){if(u)console.warn(`%c[svelte] hydration_mismatch
%c${e?`Hydration failed because the initial UI does not match what was rendered on the server. The error occurred near ${e}`:"Hydration failed because the initial UI does not match what was rendered on the server"}
https://svelte.dev/e/hydration_mismatch`,Mt,Lt);else console.warn("https://svelte.dev/e/hydration_mismatch")}function Ms(){if(u)console.warn(`%c[svelte] lifecycle_double_unmount
%cTried to unmount a component that was not mounted
https://svelte.dev/e/lifecycle_double_unmount`,Mt,Lt);else console.warn("https://svelte.dev/e/lifecycle_double_unmount")}function Ls(){if(u)console.warn("%c[svelte] select_multiple_invalid_value\n%cThe `value` property of a `<select multiple>` element should be an array, but it received a non-array value. The selection will be kept as is.\nhttps://svelte.dev/e/select_multiple_invalid_value",Mt,Lt);else console.warn("https://svelte.dev/e/select_multiple_invalid_value")}function qn(e){if(u)console.warn(`%c[svelte] state_proxy_equality_mismatch
%cReactive \`$state(...)\` proxies and the values they proxy have different identities. Because of this, comparisons with \`${e}\` will produce unexpected results
https://svelte.dev/e/state_proxy_equality_mismatch`,Mt,Lt);else console.warn("https://svelte.dev/e/state_proxy_equality_mismatch")}function Fs(){if(u)console.warn("%c[svelte] svelte_boundary_reset_noop\n%cA `<svelte:boundary>` `reset` function only resets the boundary the first time it is called\nhttps://svelte.dev/e/svelte_boundary_reset_noop",Mt,Lt);else console.warn("https://svelte.dev/e/svelte_boundary_reset_noop")}var x=!1;function ce(e){x=e}var N;function Y(e){if(e===null)throw ir(),Ge;return N=e}function Re(){return Y(me(N))}function C(e){if(!x)return;if(me(N)!==null)throw ir(),Ge;N=e}function yt(e=1){if(x){var t=e,r=N;while(t--)r=me(r);N=r}}function xt(e=!0){var t=0,r=N;while(!0){if(r.nodeType===rt){var n=r.data;if(n===or){if(t===0)return r;t-=1}else if(n===Er||n===jt||n[0]==="["&&!isNaN(Number(n.slice(1))))t+=1}var o=me(r);if(e)r.remove();r=o}}function sr(e){if(!e||e.nodeType!==rt)throw ir(),Ge;return e.data}function Nn(e){return e===this.v}function ei(e,t){return e!=e?t==t:e!==t||e!==null&&typeof e==="object"||typeof e==="function"}function Rn(e){return!ei(e,this.v)}function Ps(e){if(u){let t=Error(`invariant_violation
An invariant violation occurred, meaning Svelte's internal assumptions were flawed. This is a bug in Svelte, not your app — please open an issue at https://github.com/sveltejs/svelte, citing the following message: "${e}"
https://svelte.dev/e/invariant_violation`);throw t.name="Svelte error",t}else throw Error("https://svelte.dev/e/invariant_violation")}function Cn(e){if(u){let t=Error(`lifecycle_outside_component
\`${e}(...)\` can only be used during component initialisation
https://svelte.dev/e/lifecycle_outside_component`);throw t.name="Svelte error",t}else throw Error("https://svelte.dev/e/lifecycle_outside_component")}function Ys(){if(u){let e=Error("async_derived_orphan\nCannot create a `$derived(...)` with an `await` expression outside of an effect tree\nhttps://svelte.dev/e/async_derived_orphan");throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/async_derived_orphan")}function ti(){if(u){let e=Error("bind_invalid_checkbox_value\nUsing `bind:value` together with a checkbox input is not allowed. Use `bind:checked` instead\nhttps://svelte.dev/e/bind_invalid_checkbox_value");throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/bind_invalid_checkbox_value")}function Hs(){if(u){let e=Error(`derived_references_self
A derived value cannot reference itself recursively
https://svelte.dev/e/derived_references_self`);throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/derived_references_self")}function ri(e,t,r){if(u){let n=Error(`each_key_duplicate
${r?`Keyed each block has duplicate key \`${r}\` at indexes ${e} and ${t}`:`Keyed each block has duplicate key at indexes ${e} and ${t}`}
https://svelte.dev/e/each_key_duplicate`);throw n.name="Svelte error",n}else throw Error("https://svelte.dev/e/each_key_duplicate")}function Vs(e,t,r){if(u){let n=Error(`each_key_volatile
Keyed each block has key that is not idempotent — the key for item at index ${e} was \`${t}\` but is now \`${r}\`. Keys must be the same each time for a given item
https://svelte.dev/e/each_key_volatile`);throw n.name="Svelte error",n}else throw Error("https://svelte.dev/e/each_key_volatile")}function js(e){if(u){let t=Error(`effect_in_teardown
\`${e}\` cannot be used inside an effect cleanup function
https://svelte.dev/e/effect_in_teardown`);throw t.name="Svelte error",t}else throw Error("https://svelte.dev/e/effect_in_teardown")}function Us(){if(u){let e=Error("effect_in_unowned_derived\nEffect cannot be created inside a `$derived` value that was not itself created inside an effect\nhttps://svelte.dev/e/effect_in_unowned_derived");throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/effect_in_unowned_derived")}function Gs(e){if(u){let t=Error(`effect_orphan
\`${e}\` can only be used inside an effect (e.g. during component initialisation)
https://svelte.dev/e/effect_orphan`);throw t.name="Svelte error",t}else throw Error("https://svelte.dev/e/effect_orphan")}function zs(){if(u){let e=Error(`effect_update_depth_exceeded
Maximum update depth exceeded. This typically indicates that an effect reads and writes the same piece of state
https://svelte.dev/e/effect_update_depth_exceeded`);throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/effect_update_depth_exceeded")}function Ks(){if(u){let e=Error(`hydration_failed
Failed to hydrate the application
https://svelte.dev/e/hydration_failed`);throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/hydration_failed")}function Ws(e){if(u){let t=Error(`rune_outside_svelte
The \`${e}\` rune is only available inside \`.svelte\` and \`.svelte.js/ts\` files
https://svelte.dev/e/rune_outside_svelte`);throw t.name="Svelte error",t}else throw Error("https://svelte.dev/e/rune_outside_svelte")}function Xs(){if(u){let e=Error("state_descriptors_fixed\nProperty descriptors defined on `$state` objects must contain `value` and always be `enumerable`, `configurable` and `writable`.\nhttps://svelte.dev/e/state_descriptors_fixed");throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/state_descriptors_fixed")}function Zs(){if(u){let e=Error("state_prototype_fixed\nCannot set prototype of `$state` object\nhttps://svelte.dev/e/state_prototype_fixed");throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/state_prototype_fixed")}function Js(){if(u){let e=Error("state_unsafe_mutation\nUpdating state inside `$derived(...)`, `$inspect(...)` or a template expression is forbidden. If the value should not be reactive, declare it without `$state`\nhttps://svelte.dev/e/state_unsafe_mutation");throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/state_unsafe_mutation")}function Qs(){if(u){let e=Error("svelte_boundary_reset_onerror\nA `<svelte:boundary>` `reset` function cannot be called while an error is still being handled\nhttps://svelte.dev/e/svelte_boundary_reset_onerror");throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/svelte_boundary_reset_onerror")}var ie=!1,Ut=!1,ct=!1;var Kr=null;function Ce(e,t){return e.label=t,In(e.v,t),e}function In(e,t){return e?.[Tn]?.(t),e}function ut(e){let t=Error(),r=Hl();if(r.length===0)return null;return r.unshift(`
`),_e(t,"stack",{value:r.join(`
`)}),_e(t,"name",{value:e}),t}function Hl(){let e=Error.stackTraceLimit;Error.stackTraceLimit=1/0;let t=Error().stack;if(Error.stackTraceLimit=e,!t)return[];let r=t.split(`
`),n=[];for(let o=0;o<r.length;o++){let i=r[o],s=i.replaceAll("\\","/");if(i.trim()==="Error")continue;if(i.includes("validate_each_keys"))return[];if(s.includes("svelte/src/internal")||s.includes("node_modules/.vite"))continue;n.push(i)}return n}function ta(e,t){if(!u)throw Error("invariant(...) was not guarded by if (DEV)");if(!e)Ps(t)}var O=null;function Ft(e){O=e}var wt=null;function kr(e){wt=e}var pt=null;function Dn(e){pt=e}function Wr(e,t=!1,r){if(O={p:O,i:!1,c:null,e:null,s:e,x:null,r:w,l:Ut&&!t?{s:null,u:null,$:[]}:null},u)O.function=r,pt=r}function Xr(e){var t=O,r=t.e;if(r!==null){t.e=null;for(var n of r)ni(n)}if(e!==void 0)t.x=e;if(t.i=!0,O=t.p,u)pt=O?.function??null;return Mn(e)}function Mn(e={}){return _e(e,En,{value:!0}),e}function $t(){return!Ut||O!==null&&O.l===null}var ar=[];function ra(){var e=ar;ar=[],xn(e)}function se(e){if(ar.length===0&&!lr){var t=ar;queueMicrotask(()=>{if(t===ar)ra()})}ar.push(e)}function na(){while(ar.length>0)ra()}var Vl=~(I|Z|H);function V(e,t){e.f=e.f&Vl|t}function Tr(e){if((e.f&Ie)!==0||e.deps===null)V(e,H);else V(e,Z)}function Ln(e,t){if((e.f&I)!==0)t.set(e,I);else if((e.f&Z)!==0&&t.get(e)!==I)t.set(e,Z);V(e,H)}function oa(e){return`{${e.toString().replace(/^async \(\) => /,"").replace(/^\(/,"").replace(/\$\.get\((.+?)\)/g,(t,r)=>r).replace(/\$\.track_reactivity_loss\((.+?)\)\(\)/g,(t,r)=>r).replace(/\)$/,"")}}`}var ia=!1;function Fn(){if(!ia)ia=!0,document.addEventListener("reset",(e)=>{Promise.resolve().then(()=>{if(!e.defaultPrevented)for(let t of e.target.elements)t[Vt]?.()})},{capture:!0})}function ze(e){var t=S,r=w;ue(null),pe(null);try{return e()}finally{ue(t),pe(r)}}function Jr(e,t,r,n=r){e.addEventListener(t,()=>ze(r));let o=e[Vt];if(o)e[Vt]=()=>{o(),n(!0)};else e[Vt]=()=>n(!0);Fn()}function Pn(e,t,r,n){let o=$t()?Sr:qr;var i=e.filter((p)=>!p.settled),s=t.map(o);if(u)s.forEach((p,b)=>{p.label=t[b].toString().replace("() => ","").replaceAll("$.eager(() => ","$state.eager(").replace(/\$\.get\((.+?)\)/g,(A,g)=>g)});if(r.length===0&&i.length===0){n(s);return}var a=w,c=sa(),v=i.length===1?i[0].promise:i.length>1?Promise.all(i.map((p)=>p.promise)):null;function h(p){if((a.f&j)!==0)return;c();try{n([...s,...p])}catch(b){Be(b,a)}Ar()}var m=oi();if(r.length===0){v.then(()=>h([])).finally(m);return}function d(){Promise.all(r.map((p)=>si(p))).then(h).catch((p)=>Be(p,a)).finally(m)}if(v)v.then(()=>{if((a.f&j)!==0){m();return}c(),d(),Ar()});else d()}function sa(){var e=w,t=S,r=O,n=k;if(u)var o=wt;return function(s=!0){if(pe(e),ue(t),Ft(r),s&&(e.f&j)===0)n?.activate(),n?.apply();if(u)ii(null),kr(o)}}var jl=!1;function Ar(e=!0){if(jl=!1,pe(null),ue(null),Ft(null),e)k?.deactivate();if(u)ii(null),kr(null)}function oi(){var e=w,t=e.b,r=k,n=!!t?.is_rendered();return t?.update_pending_count(1,r),r.increment(n,e),()=>{t?.update_pending_count(-1,r),r.decrement(n,e)}}var Ye=null;function ii(e){Ye=e}var Qr=new Set;function Sr(e){var t=K|I;if(w!==null)w.f|=bt;let r={ctx:O,deps:null,effects:null,equals:Nn,f:t,fn:e,reactions:null,rv:0,v:U,wv:0,parent:w,ac:null};if(u&&ct)r.created=ut("created at");return r}var Nr=Symbol("obsolete");function si(e,t,r){let n=w;if(n===null)Ys();var o=void 0,i=Oe(U);if(u)i.label=t??oa(e);var s=!S,a=new Set;if(fa(()=>{var c=w;if(u)Ye={effect:c,effect_deps:new Set,warned:!1};var v=wn();o=v.promise;try{Promise.resolve(e()).then(v.resolve,(p)=>{if(p!==Dt)v.reject(p)}).finally(Ar)}catch(p){v.reject(p),Ar()}if(u){if(Ye){if(c.deps!==null)for(let p=0;p<Me;p+=1)Ye.effect_deps.add(c.deps[p]);if(ge!==null)for(let p=0;p<ge.length;p+=1)Ye.effect_deps.add(ge[p])}Ye=null}var h=k;if(s){if((c.f&De)!==0)var m=oi();if(n.b?.is_rendered())h.async_deriveds.get(c)?.reject(Nr);else for(let p of a.values())p.reject(Nr);a.add(v),h.async_deriveds.set(c,v)}let d=(p,b=void 0)=>{if(u)Ye=null;if(m?.(),a.delete(v),b===Nr)return;if(h.activate(),b)i.f|=tt,dt(i,b);else{if((i.f&tt)!==0)i.f^=tt;if(u&&r!==void 0&&!i.equals(p))Qr.add(i),setTimeout(()=>{if(Qr.has(i)&&(c.f&j)===0)Os(i.label,r),Qr.delete(i)});dt(i,p)}h.deactivate()};v.promise.then(d,(p)=>d(null,p||"unknown"))}),ve(()=>{for(let c of a)c.reject(Nr)}),u)i.f|=je;return new Promise((c)=>{function v(h){function m(){if(h===o)c(i);else v(o)}h.then(m,m)}v(o)})}function li(e){let t=Sr(e);if(!ie)Hn(t);return t}function qr(e){let t=Sr(e);return t.equals=Rn,t}function aa(e){var t=e.effects;if(t!==null){e.effects=null;for(var r=0;r<t.length;r+=1)J(t[r])}}var ai=[];function en(e){var t,r=w,n=e.parent;if(!ot&&n!==null&&e.v!==U&&(n.f&(j|oe))!==0)return Is(),e.v;if(pe(n),u){let o=fr;Bn(new Set);try{if(Ot.call(ai,e))Hs();ai.push(e),aa(e),t=Yn(e)}finally{pe(r),Bn(o),ai.pop()}}else try{aa(e),t=Yn(e)}finally{pe(r)}return t}function fi(e){var t=en(e);if(!e.equals(t)){if(e.wv=Rr(),!k?.is_fork||e.deps===null){if(k!==null)k.capture(e,t,!0),Ke?.capture(e,t,!0);else e.v=t;if(e.deps===null){V(e,H);return}}}if(ot)return;if(be!==null){if(Gt()||k?.is_fork)be.set(e,t)}else Tr(e)}function la(e){if(e.effects===null)return;for(let t of e.effects)if(t.teardown||t.ac){if(t.teardown?.(),t.ac!==null)ze(()=>{t.ac.abort(Dt),t.ac=null});if(t.fn!==null)t.teardown=vt;cr(t,0),tn(t)}}function ci(e){if(e.effects===null)return;for(let t of e.effects)if(t.teardown&&t.fn!==null)Et(t)}var Vn=null,Cr=null,k=null,Ke=null,be=null,di=null,lr=!1,ui=!1,Or=null,rn=null,ca=0,pi=new Set,zl=1;class Ae{id=zl++;started=!1;linked=!0;prev=null;next=null;async_deriveds=new Map;current=new Map;previous=new Map;#e=new Set;#t=new Set;#r=0;#a=new Map;#o=null;#n=[];#s=[];#i=new Map;#l=new Map;#d=new Set;is_fork=!1;#f=!1;constructor(){if(Cr===null)Vn=Cr=this;else Cr.next=this,this.prev=Cr;Cr=this}#h(){if(this.is_fork)return!0;for(let e of this.#a.keys())if(!this.#g(e))return!0;return!1}#g(e){for(var t=e;t!==null;t=t.parent)if(this.#l.has(t))return!0;return!1}skip_effect(e){if(!this.#l.has(e))this.#l.set(e,{d:[],m:[]});this.#d.delete(e)}unskip_effect(e,t=(r)=>this.schedule(r)){var r=this.#l.get(e);if(r){this.#l.delete(e);for(var n of r.d)V(n,I),t(n);for(n of r.m)V(n,Z),t(n)}this.#d.add(e)}#c(){var e=[];for(let o of this.#n){if((o.f&j)!==0||(o.f&(I|Z))===0)continue;var t=o,r=!1;while(t.parent!==null){t=t.parent;var n=t.f;if((n&(Fe|he))!==0){if((n&H)===0){r=!0;break}t.f^=H}}if(!r)e.push(t)}return this.#n=[],e}#m(){if(this.started=!0,u)for(let s of this.current.keys())pi.add(s);for(let[s,a]of this.#i)V(s,a),this.schedule(s);this.apply();var e=Or=[],t=[],r=rn=[];while(this.#n.length>0){if(ca++>1000)this.#p(),Kl();for(let s of this.#c())try{this.#b(s,e,t)}catch(a){if(_a(s),!this.#h())this.discard();throw a}}if(k=null,r.length>0){var n=Ae.ensure();for(let s of r)n.schedule(s)}if(Or=null,rn=null,this.#h()){this.#_(t),this.#_(e);for(let[s,a]of this.#l)da(s,a);if(r.length>0)k.#m();return}let o=this.#x();if(o){this.#_(t),this.#_(e),o.#u(this);return}this.#i.clear();for(let s of this.#e)s(this);this.#e.clear(),Ke=this,ua(t),ua(e),Ke=null,this.#o?.resolve();var i=k;if(this.#r===0&&(this.#n.length===0||i!==null)){if(this.#p(),ie)this.#y(),k=i}if(i!==null)st.clear(),i.#m()}#b(e,t,r){e.f^=H;var n=e.first;while(n!==null){var o=n.f,i=(o&(he|Fe))!==0,s=i&&(o&H)!==0,a=s||(o&oe)!==0||this.#l.has(n);if(!a&&n.fn!==null){if(i)n.f^=H;else if((o&gt)!==0)t.push(n);else if(ie&&(o&(Yt|rr))!==0)r.push(n);else if(Kt(n)){if((o&$e)!==0)this.add_dirty_reaction(n,Z);Et(n)}var c=n.first;if(c!==null){n=c;continue}}while(n!==null){var v=n.next;if(v!==null){n=v;break}n=n.parent}}}#x(){for(var e=this.prev;e!==null;e=e.prev)if(!e.is_fork&&this.#v(e))return e;return null}#v(e){for(let[t,[,r]]of this.current){if(r)continue;if(e.current.has(t))return!0}return!1}#u(e){for(let[r,n]of e.current){if(!this.previous.has(r)&&e.previous.has(r))this.previous.set(r,e.previous.get(r));this.current.set(r,n)}for(let[r,n]of e.async_deriveds){let o=this.async_deriveds.get(r);if(o)n.promise.then(o.resolve).catch(o.reject)}e.async_deriveds.clear(),this.transfer_reactions(e.#i);let t=(r)=>{var n=r.reactions;if(n===null)return;if((r.f&K)!==0&&(r.f&(I|Z))===0)return;for(let s of n){var o=s.f;if((o&K)!==0)t(s);else{var i=s;if(o&(je|$e)&&!this.async_deriveds.has(i))this.#i.delete(i),V(i,I),this.schedule(i)}}};for(let r of this.current.keys())t(r);this.oncommit(()=>e.discard()),e.#p(),k=this,this.#m()}#_(e){for(var t=0;t<e.length;t+=1)Ln(e[t],this.#i)}capture(e,t,r=!1){if(e.v!==U&&!this.previous.has(e))this.previous.set(e,e.v);if((e.f&tt)===0)this.current.set(e,[t,r]),be?.set(e,t);if(!this.is_fork)e.v=t}activate(){k=this}deactivate(){k=null,be=null}flush(){try{if(u)pi.clear();ui=!0,k=this,this.#m()}finally{if(ca=0,di=null,Or=null,rn=null,ui=!1,k=null,be=null,st.clear(),u)for(let e of pi)e.updated=null}}discard(){for(let e of this.#t)e(this);this.#t.clear();for(let e of this.async_deriveds.values())e.reject(Nr);this.#p(),this.#o?.resolve()}register_created_effect(e){this.#s.push(e)}#y(){for(let h=Vn;h!==null;h=h.next){var e=h.id<this.id,t=[];for(let[m,[d,p]]of this.current){if(h.current.has(m)){var r=h.current.get(m)[0];if(e&&d!==r)h.current.set(m,[d,p]);else continue}t.push(m)}if(e)for(let[m,d]of this.async_deriveds){let p=h.async_deriveds.get(m);if(p)d.promise.then(p.resolve).catch(p.reject)}var n=[...h.current.keys()].filter((m)=>!h.current.get(m)[1]);if(!h.started||n.length===0)continue;var o=n.filter((m)=>!this.current.has(m));if(o.length===0){if(e)h.discard()}else if(t.length>0){if(u&&!h.#f)ta(h.#n.length===0,"Batch has scheduled effects");if(e)for(let m of this.#d)h.unskip_effect(m,(d)=>{if((d.f&($e|je))!==0)h.schedule(d);else h.#_([d])});h.activate();var i=new Set,s=new Map;for(var a of t)pa(a,o,i,s);s=new Map;var c=[...h.current].filter(([m,d])=>{let p=this.current.get(m);if(!p)return!0;return p[0]!==d[0]||p[1]!==d[1]}).map(([m])=>m);if(c.length>0){for(let m of this.#s)if((m.f&(j|oe|Ht))===0&&_i(m,c,s))if((m.f&(je|$e))!==0)V(m,I),h.schedule(m);else h.add_dirty_reaction(m,I)}if(h.#n.length>0&&!h.#f){h.apply();for(var v of h.#c())h.#b(v,[],[])}h.deactivate()}}}increment(e,t){if(this.#r+=1,e){let r=this.#a.get(t)??0;this.#a.set(t,r+1)}}decrement(e,t){if(this.#r-=1,e){let r=this.#a.get(t)??0;if(r===1)this.#a.delete(t);else this.#a.set(t,r-1)}if(this.#f)return;this.#f=!0,se(()=>{if(this.#f=!1,this.linked)this.flush()})}transfer_reactions(e){for(let[t,r]of e)this.add_dirty_reaction(t,r);e.clear()}add_dirty_reaction(e,t){if(t===I||this.#i.get(e)!==I)this.#i.set(e,t)}oncommit(e){this.#e.add(e)}ondiscard(e){this.#t.add(e)}settled(){return(this.#o??=wn()).promise}static ensure(){if(k===null){let e=k=new Ae;if(!ui&&!lr)se(()=>{if(!e.started)e.flush()})}return k}apply(){if(!ie||!this.is_fork&&this.prev===null&&this.next===null){be=null;return}be=new Map;for(let[e,[t]]of this.current)be.set(e,t);for(let e=Vn;e!==null;e=e.next){if(e===this||e.is_fork)continue;if(e.id>=this.id||!e.#v(this)){for(let[t,r]of e.previous)if(!be.has(t))be.set(t,r)}}}schedule(e){if(di=e,e.b?.is_pending&&(e.f&(gt|Yt|rr))!==0&&(e.f&De)===0){e.b.defer_effect(e);return}this.#n.push(e)}#p(){if(!this.linked)return;var e=this.prev,t=this.next;if(e===null)Vn=t;else e.next=t;if(t===null)Cr=e;else t.prev=e;this.linked=!1}}function zt(e){var t=lr,r=Ke;Ke=null,lr=!0;try{var n;if(e)zt(),n=e();while(!0){if(na(),k===null)return n;k.flush()}}finally{lr=t,Ke=r}}function Kl(){if(u){var e=new Map;for(let r of k.current.keys())for(let[n,o]of r.updated??[]){var t=e.get(n);if(!t)t={error:o.error,count:0},e.set(n,t);t.count+=o.count}for(let r of e.values())if(r.error)console.error(r.error)}try{zs()}catch(r){if(u)_e(r,"stack",{value:""});Be(r,di)}}var it=null;function ua(e){var t=e.length;if(t===0)return;var r=0;while(r<t){var n=e[r++];if((n.f&(j|oe))===0&&Kt(n)){if(it=new Set,Et(n),n.deps===null&&n.first===null&&n.nodes===null&&n.teardown===null&&n.ac===null)hi(n);if(it?.size>0){st.clear();for(let o of it){if((o.f&(j|oe))!==0)continue;let i=[o],s=o.parent;while(s!==null){if(it.has(s))it.delete(s),i.push(s);s=s.parent}for(let a=i.length-1;a>=0;a--){let c=i[a];if((c.f&(j|oe))!==0)continue;Et(c)}}it.clear()}}}it=null}function pa(e,t,r,n){if(r.has(e))return;if(r.add(e),e.reactions!==null)for(let o of e.reactions){let i=o.f;if((i&K)!==0)pa(o,t,r,n);else if((i&(je|$e))!==0&&(i&I)===0&&_i(o,t,n))V(o,I),nn(o)}}function _i(e,t,r){let n=r.get(e);if(n!==void 0)return n;if(e.deps!==null)for(let o of e.deps){if(Ot.call(t,o))return!0;if((o.f&K)!==0&&_i(o,t,r))return r.set(o,!0),!0}return r.set(e,!1),!1}function nn(e){k.schedule(e)}function da(e,t){if((e.f&he)!==0&&(e.f&H)!==0)return;if((e.f&I)!==0)t.d.push(e);else if((e.f&Z)!==0)t.m.push(e);V(e,H);var r=e.first;while(r!==null)da(r,t),r=r.next}function _a(e){V(e,H);var t=e.first;while(t!==null)_a(t),t=t.next}var fr=new Set,st=new Map;function Bn(e){fr=e}var vi=!1;function ma(){vi=!0}function Oe(e,t){var r={f:0,v:e,reactions:null,equals:Nn,rv:0,wv:0};if(u&&ct)r.created=t??ut("created at"),r.updated=null,r.set_during_effect=!1,r.trace=null;return r}function M(e,t){let r=Oe(e,t);return Hn(r),r}function pr(e,t=!1,r=!0){let n=Oe(e);if(!t)n.equals=Rn;if(Ut&&r&&O!==null&&O.l!==null)(O.l.s??=[]).push(n);return n}function q(e,t,r=!1){if(S!==null&&(!Le||(S.f&Ht)!==0)&&$t()&&(S.f&(K|$e|je|Ht))!==0&&(_t===null||!_t.has(e)))Js();let n=r?ht(t):t;if(u)In(n,e.label);return dt(e,n,rn)}var ur=null,mi=0;function dt(e,t,r=null){if(!e.equals(t)){if(ot)st.set(e,t);else if(!st.has(e))st.set(e,e.v);var n=Ae.ensure();if(n.capture(e,t),u){if(ct||w!==null){e.updated??=new Map;let o=(e.updated.get("")?.count??0)+1;if(e.updated.set("",{error:null,count:o}),ct||o>5){let i=ut("updated at");if(i!==null){let s=e.updated.get(i.stack);if(!s)s={error:i,count:0},e.updated.set(i.stack,s);s.count++}}}if(w!==null)e.set_during_effect=!0}if((e.f&K)!==0){let o=e;if((e.f&I)!==0)en(o);if(be===null)Tr(o)}if(e.wv=Rr(),ur=null,mi=0,va(e,I,r),ur=null,$t()&&w!==null&&(w.f&H)!==0&&(w.f&(he|Fe))===0)if(We===null)ga([e]);else We.push(e);if(!n.is_fork&&fr.size>0&&!vi)jn()}return t}function jn(){vi=!1;for(let e of fr){if((e.f&H)!==0)V(e,Z);let t;try{t=Kt(e)}catch{t=!0}if(t)Et(e)}fr.clear()}function dr(e){q(e,e.v+1)}function va(e,t,r){var n=e.reactions;if(n===null)return;var o=$t(),i=n.length;if(mi+=i,mi>1e5&&ur===null)ur=new Set;if(ur!==null){if(ur.has(e))return;ur.add(e)}for(var s=0;s<i;s++){var a=n[s],c=a.f;if(!o&&a===w)continue;var v=(c&I)===0;if(v)V(a,t);if((c&Ht)!==0)fr.add(a);else if((c&K)!==0){var h=a;be?.delete(h),va(h,Z,r)}else if(v){var m=a;if((c&$e)!==0&&it!==null)it.add(m);if(r!==null)r.push(m);else nn(m)}}}var Xl=/^[a-zA-Z_$][a-zA-Z_$0-9]*$/;function ht(e){if(typeof e!=="object"||e===null||Ue in e||En in e)return e;let t=xr(e);if(t!==jo&&t!==$s)return e;var r=new Map,n=lt(e),o=M(0),i=u&&ct?ut("created at"):null,s=Wt,a=(m)=>{if(Wt===s)return m();var d=S,p=Wt;ue(null),gi(s);var b=m();return ue(d),gi(p),b};if(n){if(r.set("length",M(e.length,i)),u)e=Jl(e)}var c="";let v=!1;function h(m){if(v)return;v=!0,c=m,Ce(o,`${c} version`);for(let[d,p]of r)Ce(p,_r(c,d));v=!1}return new Proxy(e,{defineProperty(m,d,p){if(!("value"in p)||p.configurable===!1||p.enumerable===!1||p.writable===!1)Xs();var b=r.get(d);if(b===void 0)a(()=>{var A=M(p.value,i);if(r.set(d,A),u&&typeof d==="string")Ce(A,_r(c,d));return A});else q(b,p.value,!0);return!0},deleteProperty(m,d){var p=r.get(d);if(p===void 0){if(d in m){let b=a(()=>M(U,i));if(r.set(d,b),dr(o),u)Ce(b,_r(c,d))}}else q(p,U),dr(o);return!0},get(m,d,p){if(d===Ue)return e;if(u&&d===Tn)return h;var b=r.get(d),A=d in m;if(b===void 0&&(!A||Ve(m,d)?.writable))b=a(()=>{var R=ht(A?m[d]:U),L=M(R,i);if(u)Ce(L,_r(c,d));return L}),r.set(d,b);if(b!==void 0){var g=f(b);return g===U?void 0:g}return Reflect.get(m,d,p)},getOwnPropertyDescriptor(m,d){this.has?.(m,d);var p=Reflect.getOwnPropertyDescriptor(m,d),b=r.get(d);if(b!==void 0){var A=f(b);if(A===U)return;if(p&&"value"in p)p.value=A;else return{enumerable:!0,configurable:!0,value:A,writable:!0}}return p},has(m,d){if(d===Ue)return!0;var p=r.get(d),b=p!==void 0&&p.v!==U||Reflect.has(m,d);if(p!==void 0||(w!==null||S!==null)&&(!b||Ve(m,d)?.writable)){if(p===void 0)p=a(()=>{var g=b?ht(m[d]):U,R=M(g,i);if(u)Ce(R,_r(c,d));return R}),r.set(d,p);var A=f(p);if(A===U)return!1}return b},set(m,d,p,b){var A=r.get(d),g=d in m;if(n&&d==="length")for(var R=p;R<A.v;R+=1){var L=r.get(R+"");if(L!==void 0)q(L,U);else if(R in m){if(L=a(()=>M(U,i)),r.set(R+"",L),u)Ce(L,_r(c,R))}}if(A===void 0){if(!g||Ve(m,d)?.writable){if(A=a(()=>M(void 0,i)),u)Ce(A,_r(c,d));q(A,ht(p)),r.set(d,A)}}else{g=A.v!==U;var ee=a(()=>ht(p));q(A,ee)}var we=Reflect.getOwnPropertyDescriptor(m,d);if(we?.set)we.set.call(b,p);if(!g){if(n&&typeof d==="string"){var St=r.get("length"),qe=Number(d);if(Number.isInteger(qe)&&qe>=St.v)q(St,qe+1)}dr(o)}return!0},ownKeys(m){f(o);var d=Reflect.ownKeys(m).filter((A)=>{var g=r.get(A);return g===void 0||g.v!==U});for(var[p,b]of r)if(b.v!==U&&!(p in m))d.push(p);return d},setPrototypeOf(){Zs()}})}function _r(e,t){if(typeof t==="symbol")return`${e}[Symbol(${t.description??""})]`;if(Xl.test(t))return`${e}.${t}`;return/^\d+$/.test(t)?`${e}[${t}]`:`${e}['${t}']`}function Ir(e){try{if(e!==null&&typeof e==="object"&&Ue in e)return e[Ue]}catch{}return e}function Un(e,t){return Object.is(Ir(e),Ir(t))}var Zl=new Set(["copyWithin","fill","pop","push","reverse","shift","sort","splice","unshift"]);function Jl(e){return new Proxy(e,{get(t,r,n){var o=Reflect.get(t,r,n);if(!Zl.has(r))return o;return function(...i){ma();var s=o.apply(this,i);return jn(),s}}})}function ba(){let{prototype:e,__svelte_cleanup:t}=Array;if(t)t();let{indexOf:r,lastIndexOf:n,includes:o}=e;e.indexOf=function(i,s){let a=r.call(this,i,s);if(a===-1){for(let c=s??0;c<this.length;c+=1)if(Ir(this[c])===i){qn("array.indexOf(...)");break}}return a},e.lastIndexOf=function(i,s){let a=n.call(this,i,s??this.length-1);if(a===-1){for(let c=0;c<=(s??this.length-1);c+=1)if(Ir(this[c])===i){qn("array.lastIndexOf(...)");break}}return a},e.includes=function(i,s){let a=o.call(this,i,s);if(!a){for(let c=0;c<this.length;c+=1)if(Ir(this[c])===i){qn("array.includes(...)");break}}return a},Array.__svelte_cleanup=()=>{e.indexOf=r,e.lastIndexOf=n,e.includes=o}}var Dr,ya,on,xa,wa;function Gn(){if(Dr!==void 0)return;Dr=window,ya=document,on=/Firefox/.test(navigator.userAgent);var e=Element.prototype,t=Node.prototype,r=Text.prototype;if(xa=Ve(t,"firstChild").get,wa=Ve(t,"nextSibling").get,Uo(e))e[Gr]=void 0,e[An]=null,e[Ko]=void 0,e.__e=void 0;if(Uo(r))r[zr]=void 0;if(u)e.__svelte_meta=null,ba()}function Q(e=""){return document.createTextNode(e)}function Te(e){return xa.call(e)}function me(e){return wa.call(e)}function D(e,t){if(!x)return Te(e);var r=Te(N);if(r===null)r=N.appendChild(Q());else if(t&&r.nodeType!==nr){var n=Q();return r?.before(n),Y(n),n}if(t)an(r);return Y(r),r}function sn(e,t=!1){if(!x){var r=Te(e);if(r instanceof Comment&&r.data==="")return me(r);return r}if(t){if(N?.nodeType!==nr){var n=Q();return N?.before(n),Y(n),n}an(N)}return N}function te(e,t=!1){if(!x)return Te(e);var r=D(e,t);return C(e),r}function y(e,t=1,r=!1){let n=x?N:e;var o;while(t--)o=n,n=me(n);if(!x)return n;if(r){if(n?.nodeType!==nr){var i=Q();if(n===null)o?.after(i);else n.before(i);return Y(i),i}an(n)}return Y(n),n}function Zr(e){e.textContent=""}function zn(){if(!ie)return!1;if(it!==null)return!1;var e=w.f;return(e&De)!==0}function kt(e,t,r){if(t==null||t===Sn)return r?document.createElement(e,{is:r}):document.createElement(e);return r?document.createElementNS(t,e,{is:r}):document.createElementNS(t,e)}function an(e){if(e.nodeValue.length<65536)return;let t=e.nextSibling;while(t!==null&&t.nodeType===nr)t.remove(),e.nodeValue+=t.nodeValue,t=e.nextSibling}var bi=new WeakMap;function $a(e){var t=w;if(t===null)return S.f|=tt,e;if(u&&e instanceof Error&&!bi.has(e))bi.set(e,Ql(e,t));if((t.f&De)===0&&(t.f&gt)===0){if(u&&!t.parent&&e instanceof Error)Ea(e);throw e}Be(e,t)}function Be(e,t){if(e===Ge)throw e;if(t!==null&&(t.f&j)!==0)return;while(t!==null){if((t.f&wr)!==0&&(t.f&(j|It))===0){if((t.f&De)===0)throw e;try{t.b.error(e);return}catch(r){e=r}}t=t.parent}if(u&&e instanceof Error)Ea(e);throw e}function Ql(e,t){let r=Ve(e,"message");if(r&&!r.configurable)return;var n=on?"  ":"\t",o=`
${n}in ${t.fn?.name||"<unknown>"}`,i=t.ctx;while(i!==null)o+=`
${n}in ${i.function?.[nt].split("/").pop()}`,i=i.p;return{message:e.message+`
${o}
`,stack:e.stack?.split(`
`).filter((s)=>!s.includes("svelte/src/internal")).join(`
`)}}function Ea(e){let t=bi.get(e);if(t)_e(e,"message",{value:t.message}),_e(e,"stack",{value:t.stack})}function ka(e){if(w===null){if(S===null)Gs(e);Us()}if(ot)js(e)}function ef(e,t){var r=t.last;if(r===null)t.last=t.first=e;else r.next=e,e.prev=r,t.last=e}function Tt(e,t){var r=w;if(u)while(r!==null&&(r.f&Ht)!==0)r=r.parent;if(r!==null&&(r.f&oe)!==0)e|=oe;var n={ctx:O,deps:null,nodes:null,f:e|I|Ie,first:null,fn:t,last:null,next:null,parent:r,b:r&&r.b,prev:null,teardown:null,wv:0,ac:null};if(u)n.component_function=pt;k?.register_created_effect(n);var o=n;if((e&gt)!==0)if(Or!==null)Or.push(n);else Ae.ensure().schedule(n);else if(t!==null){try{Et(n)}catch(s){throw J(n),s}if(o.deps===null&&o.teardown===null&&o.nodes===null&&o.first===o.last&&(o.f&bt)===0){if(o=o.first,(e&$e)!==0&&(e&Pe)!==0&&o!==null)o.f|=Pe}}if(o!==null){if(o.parent=r,r!==null)ef(o,r);if(S!==null&&(S.f&K)!==0&&(e&Fe)===0){var i=S;(i.effects??=[]).push(o)}}return n}function Gt(){return S!==null&&!Le}function ve(e){let t=Tt(Yt,null);return V(t,H),t.teardown=e,t}function Mr(e){if(ka("$effect"),u)_e(e,"name",{value:"$effect"});var t=w.f,r=!S&&(t&he)!==0&&O!==null&&!O.i;if(r){var n=O;(n.e??=[]).push(e)}else return ni(e)}function ni(e){return Tt(gt|Es,e)}function xi(e){Ae.ensure();let t=Tt(Fe|bt,e);return()=>{J(t)}}function Ta(e){Ae.ensure();let t=Tt(Fe|bt,e);return(r={})=>new Promise((n)=>{if(r.outro)At(t,()=>{J(t),n(void 0)});else J(t),n(void 0)})}function Xe(e){return Tt(gt,e)}function fa(e){return Tt(je|bt,e)}function ye(e,t=0){return Tt(Yt|t,e)}function W(e,t=[],r=[],n=[]){Pn(n,t,r,(o)=>{Tt(Yt,()=>{e(...o.map(f))})})}function Ze(e,t=0){var r=Tt($e|t,e);if(u)r.dev_stack=wt;return r}function de(e){return Tt(he|bt,e)}function wi(e){var t=e.teardown;if(t!==null){let r=ot,n=S;yi(!0),ue(null);try{t.call(null)}catch(o){Be(o,e.parent)}finally{yi(r),ue(n)}}}function tn(e,t=!1){var r=e.first;e.first=e.last=null;while(r!==null){let o=r.ac;if(o!==null)ze(()=>{o.abort(Dt)});var n=r.next;if((r.f&Fe)!==0)r.parent=null;else J(r,t);r=n}}function Aa(e){var t=e.first;while(t!==null){var r=t.next;if((t.f&he)===0)J(t);t=r}}function J(e,t=!0){var r=!1;if((t||(e.f&zo)!==0)&&e.nodes!==null&&e.nodes.end!==null)Sa(e.nodes.start,e.nodes.end),r=!0;e.f|=It,tn(e,t&&!r),cr(e,0);var n=e.nodes&&e.nodes.t;if(n!==null)for(let i of n)i.stop();wi(e),e.f^=It,e.f|=j;var o=e.parent;if(o!==null&&o.first!==null)hi(e);if(u)e.component_function=null;e.next=e.prev=e.teardown=e.ctx=e.deps=e.fn=e.nodes=e.ac=e.b=null}function Sa(e,t){while(e!==null){var r=e===t?null:me(e);e.remove(),e=r}}function hi(e){var{parent:t,prev:r,next:n}=e;if(r!==null)r.next=n;if(n!==null)n.prev=r;if(t!==null){if(t.first===e)t.first=n;if(t.last===e)t.last=r}}function At(e,t,r=!0){var n=[];e.f|=$n,qa(e,n,!0);var o=()=>{if(r)J(e);if(t)t()},i=n.length;if(i>0){var s=()=>--i||o();for(var a of n)a.out(s)}else o()}function qa(e,t,r){if((e.f&oe)!==0)return;e.f^=oe;var n=e.nodes&&e.nodes.t;if(n!==null){for(let a of n)if(a.is_global||r)t.push(a)}var o=e.first;while(o!==null){var i=o.next;if((o.f&Fe)===0){var s=(o.f&Pe)!==0||(o.f&he)!==0&&(e.f&$e)!==0;qa(o,t,s?r:!1)}o=i}}function Lr(e){e.f&=~$n,Na(e,!0)}function Na(e,t){if((e.f&$n)!==0)return;if((e.f&oe)===0)return;if(e.f^=oe,(e.f&H)===0)V(e,I),Ae.ensure().schedule(e);var r=e.first;while(r!==null){var n=r.next,o=(r.f&Pe)!==0||(r.f&he)!==0;Na(r,o?t:!1),r=n}var i=e.nodes&&e.nodes.t;if(i!==null){for(let s of i)if(s.is_global||t)s.in()}}function Fr(e,t){if(!e.nodes)return;var r=e.nodes.start,n=e.nodes.end;while(r!==null){var o=r===n?null:me(r);t.append(r),r=o}}var Ra=null;var ot=!1;function yi(e){ot=e}var S=null,Le=!1;function ue(e){S=e}var w=null;function pe(e){w=e}var _t=null;function Hn(e){if(S!==null&&(!ie&&(S.f&$r)!==0||(S.f&K)!==0))(_t??=new Set).add(e)}var ge=null,Me=0,We=null;function ga(e){We=e}var Oa=1,hr=0,Wt=hr;function gi(e){Wt=e}function Rr(){return++Oa}function Kt(e){var t=e.f;if((t&I)!==0)return!0;if((t&Z)!==0){var r=e.deps,n=r.length;for(var o=0;o<n;o++){var i=r[o];if(Kt(i))fi(i);if(i.wv>e.wv)return!0}if((t&Ie)!==0&&be===null)V(e,H)}return!1}function Ia(e,t,r=!0){var n=e.reactions;if(n===null)return;if(!ie&&_t!==null&&_t.has(e))return;for(var o=0;o<n.length;o++){var i=n[o];if((i.f&K)!==0)Ia(i,t,!1);else if(t===i){if(r)V(i,I);else if((i.f&H)!==0)V(i,Z);nn(i)}}}function Yn(e){var t=ge,r=Me,n=We,o=S,i=_t,s=O,a=Le,c=Wt,v=e.f;if(ge=null,Me=0,We=null,S=(v&(he|Fe))===0?e:null,_t=null,Ft(e.ctx),Le=!1,Wt=++hr,e.ac!==null)ze(()=>{e.ac.abort(Dt)}),e.ac=null;try{e.f|=$r;var h=e.fn,m=h();e.f|=De;var d=Ca(e);if($t()&&We!==null&&!Le&&d!==null&&(e.f&(K|Z|I))===0)for(var p=0;p<We.length;p++)Ia(We[p],e);if(o!==null&&o!==e){if(hr++,o.deps!==null)for(let b=0;b<r;b+=1)o.deps[b].rv=hr;if(t!==null)for(let b of t)b.rv=hr;if(We!==null)if(n===null)n=We;else n.push(...We)}if((e.f&tt)!==0)e.f^=tt;return m}catch(b){return Ca(e),$a(b)}finally{e.f^=$r,ge=t,Me=r,We=n,S=o,_t=i,Ft(s),Le=a,Wt=c}}function Ca(e){var t=e.deps,r=k?.is_fork;if(ge!==null){var n;if(!r)cr(e,Me);if(t!==null&&Me>0){t.length=Me+ge.length;for(n=0;n<ge.length;n++)t[Me+n]=ge[n]}else e.deps=t=ge;if(Gt()&&(e.f&Ie)!==0)for(n=Me;n<t.length;n++)(t[n].reactions??=[]).push(e)}else if(!r&&t!==null&&Me<t.length)cr(e,Me),t.length=Me;return t}function tf(e,t){let r=t.reactions;if(r!==null){var n=ws.call(r,e);if(n!==-1){var o=r.length-1;if(o===0)r=t.reactions=null;else r[n]=r[o],r.pop()}}if(r===null&&(t.f&K)!==0&&(ge===null||!Ot.call(ge,t))){var i=t;if((i.f&Ie)!==0)i.f^=Ie;if(i.v!==U)Tr(i);if(i.ac!==null)ze(()=>{i.ac.abort(Dt),i.ac=null,V(i,I)});la(i),cr(i,0)}}function cr(e,t){var r=e.deps;if(r===null)return;for(var n=t;n<r.length;n++)tf(e,r[n])}function Et(e){var t=e.f;if((t&j)!==0)return;V(e,H);var r=w;if(w=e,u){var n=pt;Dn(e.component_function);var o=wt;kr(e.dev_stack??wt)}try{if((t&($e|rr))!==0)Aa(e);else tn(e);wi(e);var i=Yn(e);if(e.teardown=typeof i==="function"?i:null,e.wv=Oa,u&&ct&&(e.f&I)!==0&&e.deps!==null){for(var s of e.deps)if(s.set_during_effect)s.wv=Rr(),s.set_during_effect=!1}}finally{if(w=r,u)Dn(n),kr(o)}}async function ln(){if(ie)return new Promise((e)=>{requestAnimationFrame(()=>e()),setTimeout(()=>e())});await Promise.resolve(),zt()}function f(e){var t=e.f,r=(t&K)!==0;if(Ra?.add(e),S!==null&&!Le){var n=w!==null&&(w.f&j)!==0;if(!n&&(_t===null||!_t.has(e))){var o=S.deps;if((S.f&$r)!==0){if(e.rv<hr)if(e.rv=hr,ge===null&&o!==null&&o[Me]===e)Me++;else if(ge===null)ge=[e];else ge.push(e)}else{if(S.deps??=[],!Ot.call(S.deps,e))S.deps.push(e);var i=e.reactions;if(i===null)e.reactions=[S];else if(!Ot.call(i,S))i.push(S)}}}if(u){if(!Le&&Ye&&k===null&&Ke===null&&!Ye.warned&&(Ye.effect.f&$r)===0&&!Ye.effect_deps.has(e)){Ye.warned=!0,Cs(e.label);var s=ut("traced at");if(s)console.warn(s)}if(Qr.delete(e),ct&&!Le&&Kr!==null&&S!==null&&Kr.reaction===S){if(e.trace)e.trace();else if(s=ut("traced at"),s){var a=Kr.entries.get(e);if(a===void 0)a={traces:[]},Kr.entries.set(e,a);var c=a.traces[a.traces.length-1];if(s.stack!==c?.stack)a.traces.push(s)}}}if(ot&&st.has(e))return st.get(e);if(r){var v=e;if(ot){var h=v.v;if((v.f&H)===0&&v.reactions!==null||Ma(v))h=en(v);return st.set(v,h),h}var m=(v.f&Ie)===0&&!Le&&S!==null&&(S.f&Ie)!==0,d=(v.f&De)===0;if(Kt(v)){if(m)v.f|=Ie;fi(v)}if(m&&!d)ci(v),Da(v)}if(be?.has(e))return be.get(e);if((e.f&tt)!==0)throw e.v;return e.v}function Da(e){if(e.f|=Ie,e.deps===null)return;for(let r of e.deps){var t=r.reactions;if(t===null)r.reactions=[e];else if(!Ot.call(t,e))t.push(e);if((r.f&K)!==0&&(r.f&Ie)===0)ci(r),Da(r)}}function Ma(e){if(e.v===U)return!0;if(e.deps===null)return!1;for(let t of e.deps){if(st.has(t))return!0;if((t.f&K)!==0&&Ma(t))return!0}return!1}function G(e){var t=Le;try{return Le=!0,e()}finally{Le=t}}var nf=["allowfullscreen","async","autofocus","autoplay","checked","controls","default","disabled","formnovalidate","indeterminate","inert","ismap","loop","multiple","muted","nomodule","novalidate","open","playsinline","readonly","required","reversed","seamless","selected","webkitdirectory","defer","disablepictureinpicture","disableremoteplayback"];var Bd=[...nf,"formNoValidate","isMap","noModule","playsInline","readOnly","value","volume","defaultValue","defaultChecked","srcObject","noValidate","allowFullscreen","disablePictureInPicture","disableRemotePlayback"];var of=["touchstart","touchmove"];function La(e){return of.includes(e)}var sf=["$state","$state.raw","$derived","$derived.by"],Yd=[...sf,"$state.eager","$state.snapshot","$props","$props.id","$bindable","$effect","$effect.pre","$effect.tracking","$effect.root","$effect.pending","$inspect","$inspect().with","$inspect.trace","$host"];var Pr=Symbol("events"),ki=new Set,Kn=new Set;function Ti(e){if(!x)return;e.removeAttribute("onload"),e.removeAttribute("onerror");let t=e.__e;if(t!==void 0)e.__e=void 0,queueMicrotask(()=>{if(e.isConnected)e.dispatchEvent(t)})}function Ba(e,t,r,n={}){function o(i){if(!n.capture)Xn.call(t,i);if(!i.cancelBubble)return ze(()=>r?.call(this,i))}if(e.startsWith("pointer")||e.startsWith("touch")||e==="wheel")o.__removed=!1,se(()=>{if(!o.__removed)t.addEventListener(e,o,n)});else t.addEventListener(e,o,n);return o}function mt(e,t,r,n,o){var i={capture:n,passive:o},s=Ba(e,t,r,i);if(t===document.body||t===window||t===document||t instanceof HTMLMediaElement)ve(()=>{s.__removed=!0,t.removeEventListener(e,s,i)})}function z(e,t,r){(t[Pr]??={})[e]=r}function Wn(e){for(var t=0;t<e.length;t++)ki.add(e[t]);for(var r of Kn)r(e)}var $i=null,Ei=!1;function Xn(e){var t=this,r=t.ownerDocument,n=e.type,o=e.composedPath?.()||[],i=o[0]||e.target;if($i=e,!Ei)Ei=!0,setTimeout(()=>{Ei=!1,$i=null});var s=0,a=$i===e&&e[Pr];if(a){var c=o.indexOf(a);if(c!==-1&&(t===document||t===window)){e[Pr]=t;return}var v=o.indexOf(t);if(v===-1)return;if(c<=v)s=c}if(i=o[s]||e.target,i===t)return;_e(e,"currentTarget",{configurable:!0,get(){return i||r}});var h=S,m=w;ue(null),pe(null);try{var d,p=[];while(i!==null){if(i===t)break;try{var b=i[Pr]?.[n];if(b!=null&&(!i.disabled||e.target===i))b.call(i,e)}catch(A){if(d)p.push(A);else d=A}if(e.cancelBubble)break;s++,i=s<o.length?o[s]:null}if(d){for(let A of p)queueMicrotask(()=>{throw A});throw d}}finally{e[Pr]=t,delete e.currentTarget,ue(h),pe(m)}}var lf=globalThis?.window?.trustedTypes&&globalThis.window.trustedTypes.createPolicy("svelte-trusted-html",{createHTML:(e)=>e});function Ya(e){return lf?.createHTML(e)??e}function Ai(e){var t=kt("template");return t.innerHTML=Ya(e.replaceAll("<!>","<!---->")),t.content}function Je(e,t){var r=w;if(r.nodes===null)r.nodes={start:e,end:t,a:null,t:null}}function xe(e,t){var r=(t&Ns)!==0,n=(t&Rs)!==0,o,i=!e.startsWith("<!>");return()=>{if(x)return Je(N,null),N;if(o===void 0){if(o=Ai(i?e:"<!>"+e),!r)o=Te(o)}var s=n||on?document.importNode(o,!0):o.cloneNode(!0);if(r){var a=Te(s),c=s.lastChild;Je(a,c)}else Je(s,s);return s}}function fn(e=""){if(!x){var t=Q(e+"");return Je(t,t),t}var r=N;if(r.nodeType!==nr)r.before(r=Q()),Y(r);else an(r);return Je(r,r),r}function qi(){if(x)return Je(N,null),N;var e=document.createDocumentFragment(),t=document.createComment(""),r=Q();return e.append(t,r),Je(t,r),e}function P(e,t){if(x){var r=w;if((r.f&De)===0||r.nodes.end===null)r.nodes.end=N;Re();return}if(e===null)return;e.before(t)}function Ha(e){let t=0,r=Oe(0),n;if(u)Ce(r,"createSubscriber version");return()=>{if(Gt())f(r),ye(()=>{if(t===0)n=G(()=>e(()=>dr(r)));return t+=1,()=>{se(()=>{if(t-=1,t===0)n?.(),n=void 0,dr(r)})}})}}var uf=Pe|bt;function Ni(e,t,r,n){new Va(e,t,r,n)}class Va{parent;is_pending=!1;transform_error;#e;#t=x?N:null;#r;#a;#o;#n=null;#s=null;#i=null;#l=null;#d=0;#f=0;#h=!1;#g=new Map;#c=null;#m=Ha(()=>{if(this.#c=Oe(this.#d),u)Ce(this.#c,"$effect.pending()");return()=>{this.#c=null}});constructor(e,t,r,n){if(this.#e=e,this.#r=t,this.#a=(o)=>{var i=w;i.b=this,i.f|=wr,r(o)},this.parent=w.b,this.transform_error=n??this.parent?.transform_error??((o)=>o),this.#o=Ze(()=>{if(x){let o=sr(this.#t);Re();let i=o===jt;if(o.startsWith(Jo)){let a=JSON.parse(o.slice(Jo.length));this.#x(a)}else if(i)this.#_();else this.#b()}else this.#y()},uf),x)this.#e=N}#b(){try{this.#n=de(()=>this.#a(this.#e))}catch(e){this.error(e)}}#x(e){let t=this.#r.failed,{reset:r,invoke_onerror:n}=this.#v(e);if(se(n),!t)return;this.#i=de(()=>{t(this.#e,()=>e,()=>r)})}#v(e){var t=!1,r=!1;let n=()=>{if(this.#u())return;if(t){Fs();return}if(t=!0,r)Qs();if(this.#i!==null)At(this.#i,()=>{this.#i=null});this.#w(()=>{this.#y()})};return{reset:n,invoke_onerror:()=>{if(this.#u())return;try{r=!0,this.#r.onerror?.(e,n),r=!1}catch(i){Be(i,this.#o&&this.#o.parent)}}}}#u(){return(this.#o.f&(j|It))!==0}#_(){let e=this.#r.pending;if(!e)return;this.is_pending=!0,this.#s=de(()=>e(this.#e)),se(()=>{if(this.#u())return;var t=this.#l=document.createDocumentFragment(),r=Q(),n=!1;if(t.append(r),this.#n=this.#w(()=>{try{return de(()=>this.#a(r))}catch(o){try{this.error(o),n=!0}catch(i){Be(i,this.#o.parent)}return null}}),this.#n===null){if(this.#l=null,n)this.#p(k);return}if(this.#f===0)this.#e.before(t),this.#l=null,At(this.#s,()=>{this.#s=null}),this.#p(k)})}#y(){try{if(this.is_pending=this.has_pending_snippet(),this.#f=0,this.#d=0,this.#n=de(()=>{this.#a(this.#e)}),this.#f>0){var e=this.#l=document.createDocumentFragment();Fr(this.#n,e);let t=this.#r.pending;this.#s=de(()=>t(this.#e))}else this.#p(k)}catch(t){this.error(t)}}#p(e){this.is_pending=!1,e.transfer_reactions(this.#g)}defer_effect(e){Ln(e,this.#g)}is_rendered(){return!this.is_pending&&(!this.parent||this.parent.is_rendered())}has_pending_snippet(){return!!this.#r.pending}#w(e){var t=w,r=S,n=O;pe(this.#o),ue(this.#o),Ft(this.#o.ctx);try{return Ae.ensure(),e()}finally{pe(t),ue(r),Ft(n)}}#$(e,t){if(!this.has_pending_snippet()){if(this.parent)this.parent.#$(e,t);return}if(this.#f+=e,this.#f===0){if(this.#p(t),this.#s)At(this.#s,()=>{this.#s=null});if(this.#l)this.#e.before(this.#l),this.#l=null}}update_pending_count(e,t){if(this.#$(e,t),this.#d+=e,!this.#c||this.#h)return;this.#h=!0,se(()=>{if(this.#h=!1,this.#c)dt(this.#c,this.#d)})}get_effect_pending(){return this.#m(),f(this.#c)}error(e){if(e===Ge)throw e;if(!this.#r.onerror&&!this.#r.failed)throw e;if(k?.is_fork){if(this.#n)k.skip_effect(this.#n);if(this.#s)k.skip_effect(this.#s);if(this.#i)k.skip_effect(this.#i);k.oncommit(()=>{if(!this.#u())this.#E(e)})}else this.#E(e)}#E(e){if(this.#n)J(this.#n),this.#n=null;if(this.#s)J(this.#s),this.#s=null;if(this.#i)J(this.#i),this.#i=null;if(x)Y(this.#t),yt(),Y(xt());let t=this.#r.failed,r=(n)=>{if(this.#u())return;let{reset:o,invoke_onerror:i}=this.#v(n);if(i(),t&&!this.#u())this.#i=this.#w(()=>{try{return de(()=>{var s=w;s.b=this,s.f|=wr,t(this.#e,()=>n,()=>o)})}catch(s){return Be(s,this.#o.parent),null}})};se(()=>{if(this.#u())return;var n;try{n=this.transform_error(e)}catch(o){Be(o,this.#o&&this.#o.parent);return}if(n!==null&&typeof n==="object"&&typeof n.then==="function")n.then(r,(o)=>Be(o,this.#o&&this.#o.parent));else r(n)})}}var Ri=!0;function B(e,t){var r=t==null?"":typeof t==="object"?`${t}`:t;if(r!==(e[zr]??=e.nodeValue))e[zr]=r,e.nodeValue=`${r}`}function Br(e,t){return ja(e,t)}function Oi(e,t){Gn(),t.intro=t.intro??!1;let r=t.target,n=x,o=N;try{var i=Te(r);while(i&&(i.nodeType!==rt||i.data!==Er))i=me(i);if(!i)throw Ge;ce(!0),Y(i);let s=ja(e,{...t,anchor:i});return ce(!1),s}catch(s){if(s instanceof Error&&s.message.split(`
`).some((a)=>a.startsWith("https://svelte.dev/e/")))throw s;if(s!==Ge)console.warn("Failed to hydrate: ",s);if(t.recover===!1)Ks();return Gn(),Zr(r),ce(!1),Br(e,t)}finally{ce(n),Y(o)}}var Zn=new Map;function ja(e,{target:t,anchor:r,props:n={},events:o,context:i,intro:s=!0,transformError:a}){Gn();var c=void 0,v=Ta(()=>{var h=r??t.appendChild(Q());Ni(h,{pending:()=>{}},(p)=>{Wr({});var b=O;if(i)b.c=i;if(o)n.$$events=o;if(x)Je(p,null);if(Ri=s,c=e(p,n)||Mn(),Ri=!0,x){if(w.nodes.end=N,N===null||N.nodeType!==rt||N.data!==or)throw ir(),Ge}Xr()},a);var m=new Set,d=(p)=>{for(var b=0;b<p.length;b++){var A=p[b];if(m.has(A))continue;m.add(A);var g=La(A);for(let ee of[t,document]){var R=Zn.get(ee);if(R===void 0)R=new Map,Zn.set(ee,R);var L=R.get(A);if(L===void 0)ee.addEventListener(A,Xn,{passive:g}),R.set(A,1);else R.set(A,L+1)}}};return d(yr(ki)),Kn.add(d),()=>{for(var p of m)for(let g of[t,document]){var b=Zn.get(g),A=b.get(p);if(--A==0){if(g.removeEventListener(p,Xn),b.delete(p),b.size===0)Zn.delete(g)}else b.set(p,A)}if(Kn.delete(d),h!==r)h.parentNode?.removeChild(h)}});return Ci.set(c,v),c}var Ci=new WeakMap;function Ii(e,t){let r=Ci.get(e);if(r)return Ci.delete(e),r(t);if(u)Ms();return Promise.resolve()}class Xt{anchor;#e=new Map;#t=new Map;#r=new Map;#a=new Set;#o=!0;constructor(e,t=!0){this.anchor=e,this.#o=t}#n=(e)=>{if(!this.#e.has(e))return;var t=this.#e.get(e),r=this.#t.get(t);if(r)Lr(r),this.#a.delete(t);else{var n=this.#r.get(t);if(n){if(Lr(n.effect),this.#t.set(t,n.effect),this.#r.delete(t),u)n.fragment.lastChild[Wo]=this.anchor;n.fragment.lastChild.remove(),this.anchor.before(n.fragment),r=n.effect}}for(let[o,i]of this.#e){if(this.#e.delete(o),o===e)break;let s=this.#r.get(i);if(s)J(s.effect),this.#r.delete(i)}for(let[o,i]of this.#t){if(o===t||this.#a.has(o))continue;let s=()=>{if(Array.from(this.#e.values()).includes(o)){var c=document.createDocumentFragment();Fr(i,c),c.append(Q()),this.#r.set(o,{effect:i,fragment:c})}else J(i);this.#a.delete(o),this.#t.delete(o)};if(this.#o||!r)this.#a.add(o),At(i,s,!1);else s()}};#s=(e)=>{this.#e.delete(e);let t=Array.from(this.#e.values());for(let[r,n]of this.#r)if(!t.includes(r))J(n.effect),this.#r.delete(r)};ensure(e,t){var r=k,n=zn();if(t&&!this.#t.has(e)&&!this.#r.has(e))if(n){var o=document.createDocumentFragment(),i=Q();o.append(i),this.#r.set(e,{effect:de(()=>t(i)),fragment:o})}else this.#t.set(e,de(()=>t(this.anchor)));if(this.#e.set(r,e),n){for(let[s,a]of this.#t)if(s===e)r.unskip_effect(a);else r.skip_effect(a);for(let[s,a]of this.#r)if(s===e)r.unskip_effect(a.effect);else r.skip_effect(a.effect);r.oncommit(this.#n),r.ondiscard(this.#s)}else{if(x)this.anchor=N;this.#n(r)}}}function Se(e,t,r=!1){var n;if(x)n=N,Re();var o=new Xt(e),i=r?Pe:0;function s(a,c){if(x){var v=sr(n);if(a!==parseInt(v.substring(1))){var h=xt();Y(h),o.anchor=h,ce(!1),o.ensure(a,c),ce(!0);return}}o.ensure(a,c)}Ze(()=>{var a=!1;if(t((c,v=0)=>{a=!0,s(v,c)}),!a)s(-1,null)},i)}var hm=Symbol("NaN");function Mi(e,t){return t}function df(e,t,r){var n=[],o=t.length,i,s=t.length;for(var a=0;a<o;a++){let m=t[a];At(m,()=>{if(i){if(i.pending.delete(m),i.done.add(m),i.pending.size===0){var d=e.outrogroups;if(Di(e,yr(i.done)),d.delete(i),d.size===0)e.outrogroups=null}}else s-=1},!1)}if(s===0){var c=n.length===0&&r!==null&&e.pending.size===0;if(c){var v=r,h=v.parentNode;Zr(h),h.append(v),e.items.clear()}Di(e,t,!c)}else i={pending:new Set(t),done:new Set},(e.outrogroups??=new Set).add(i)}function Di(e,t,r=!0){var n;if(e.pending.size>0){n=new Set;for(let s of e.pending.values())for(let a of s)n.add(e.items.get(a).e)}for(var o=0;o<t.length;o++){var i=t[o];if(n?.has(i)){i.f|=ft;let s=document.createDocumentFragment();Fr(i,s)}else J(t[o],r)}}var Ua;function mr(e,t,r,n,o,i=null){var s=e,a=new Map,c=(t&Zo)!==0;if(c){var v=e;s=x?Y(Te(v)):v.appendChild(Q())}if(x)Re();var h=null,m=qr(()=>{var L=r();return lt(L)?L:L==null?[]:yr(L)});if(u)Ce(m,"{#each ...}");var d=new Map,p=!0;function b(L){if((R.effect.f&j)!==0)return;R.pending.delete(L);var ee=f(m);if(R.fallback=h,_f(R,ee,s,t,n),h!==null)if(ee.length===0)if((h.f&ft)===0)Lr(h);else h.f^=ft,un(h,null,s);else At(h,()=>{h=null})}function A(L){R.pending.delete(L)}var g=Ze(()=>{var L=f(m),ee=L.length;let we=!1;if(x){var St=sr(s)===jt;if(St!==(ee===0))s=xt(),Y(s),ce(!1),we=!0}var qe=new Set,ae=k,Bt=zn();for(var T=0;T<ee;T+=1){if(x&&N.nodeType===rt&&N.data===or)s=N,we=!0,ce(!1);var le=L[T],Qe=n(le,T);if(u){var qt=n(le,T);if(Qe!==qt)Vs(String(T),String(Qe),String(qt))}var Ne=p?null:a.get(Qe);if(Ne){if(Ne.v)dt(Ne.v,le);if(Ne.i)dt(Ne.i,T);if(Bt)ae.unskip_effect(Ne.e)}else{if(Ne=hf(a,p?s:Ua??=Q(),le,Qe,T,o,t,r),!p)Ne.e.f|=ft;a.set(Qe,Ne)}qe.add(Qe)}if(ee===0&&i&&!h)if(p)h=de(()=>i(s));else h=de(()=>i(Ua??=Q())),h.f|=ft;if(ee>qe.size)if(u)mf(L,n);else ri("","","");if(x&&ee>0)Y(xt());if(!p)if(d.set(ae,qe),Bt){for(let[Qt,er]of a)if(!qe.has(Qt))ae.skip_effect(er.e);ae.oncommit(b),ae.ondiscard(A)}else b(ae);if(we)ce(!0);f(m)}),R={effect:g,flags:t,items:a,pending:d,outrogroups:null,fallback:h};if(p=!1,x)s=N}function cn(e){while(e!==null&&(e.f&he)===0)e=e.next;return e}function _f(e,t,r,n,o){var i=(n&Ss)!==0,s=t.length,a=e.items,c=cn(e.effect.first),v,h=null,m,d=[],p=[],b,A,g,R;if(i){for(R=0;R<s;R+=1)if(b=t[R],A=o(b,R),g=a.get(A).e,(g.f&ft)===0)g.nodes?.a?.measure(),(m??=new Set).add(g)}for(R=0;R<s;R+=1){if(b=t[R],A=o(b,R),g=a.get(A).e,e.outrogroups!==null)for(let le of e.outrogroups)le.pending.delete(g),le.done.delete(g);if((g.f&oe)!==0){if(Lr(g),i)g.nodes?.a?.unfix(),(m??=new Set).delete(g)}if((g.f&ft)!==0)if(g.f^=ft,g===c)un(g,null,r);else{var L=h?h.next:c;if(g===e.effect.last)e.effect.last=g.prev;if(g.prev)g.prev.next=g.next;if(g.next)g.next.prev=g.prev;Zt(e,h,g),Zt(e,g,L),un(g,L,r),h=g,d=[],p=[],c=cn(h.next);continue}if(g!==c){if(v!==void 0&&v.has(g)){if(d.length<p.length){var ee=p[0],we;h=ee.prev;var St=d[0],qe=d[d.length-1];for(we=0;we<d.length;we+=1)un(d[we],ee,r);for(we=0;we<p.length;we+=1)v.delete(p[we]);Zt(e,St.prev,qe.next),Zt(e,h,St),Zt(e,qe,ee),c=ee,h=qe,R-=1,d=[],p=[]}else v.delete(g),un(g,c,r),Zt(e,g.prev,g.next),Zt(e,g,h===null?e.effect.first:h.next),Zt(e,h,g),h=g;continue}d=[],p=[];while(c!==null&&c!==g)(v??=new Set).add(c),p.push(c),c=cn(c.next);if(c===null)continue}if((g.f&ft)===0)d.push(g);h=g,c=cn(g.next)}if(e.outrogroups!==null){for(let le of e.outrogroups)if(le.pending.size===0)Di(e,yr(le.done)),e.outrogroups?.delete(le);if(e.outrogroups.size===0)e.outrogroups=null}if(c!==null||v!==void 0){var ae=[];if(v!==void 0){for(g of v)if((g.f&oe)===0)ae.push(g)}while(c!==null){if((c.f&oe)===0&&c!==e.fallback)ae.push(c);c=cn(c.next)}var Bt=ae.length;if(Bt>0){var T=(n&Zo)!==0&&s===0?r:null;if(i){for(R=0;R<Bt;R+=1)ae[R].nodes?.a?.measure();for(R=0;R<Bt;R+=1)ae[R].nodes?.a?.fix()}df(e,ae,T)}}if(i)se(()=>{if(m===void 0)return;for(g of m)g.nodes?.a?.apply()})}function hf(e,t,r,n,o,i,s,a){var c=(s&Ts)!==0?(s&qs)===0?pr(r,!1,!1):Oe(r):null,v=(s&As)!==0?Oe(o):null;if(u&&c)c.trace=()=>{a()[v?.v??o]};return{v:c,i:v,e:de(()=>(i(t,c??r,v??o,a),()=>{e.delete(n)}))}}function un(e,t,r){if(!e.nodes)return;var n=e.nodes.start,o=e.nodes.end,i=t&&(t.f&ft)===0?t.nodes.start:r;while(n!==null){var s=me(n);if(i.before(n),n===o)return;n=s}}function Zt(e,t,r){if(t===null)e.effect.first=r;else t.next=r;if(r===null)e.effect.last=t;else r.prev=t}function mf(e,t){let r=new Map,n=e.length;for(let o=0;o<n;o++){let i=t(e[o],o);if(r.has(i)){let s=String(r.get(i)),a=String(o),c=String(i);if(c.startsWith("[object "))c=null;ri(s,a,c)}r.set(i,o)}}var za=[...` 	
\r\f \v\uFEFF`];function Ka(e,t,r){var n=e==null?"":""+e;if(t)n=n?n+" "+t:t;if(r){for(var o of Object.keys(r))if(r[o])n=n?n+" "+o:o;else if(n.length){var i=o.length,s=0;while((s=n.indexOf(o,s))>=0){var a=s+i;if((s===0||za.includes(n[s-1]))&&(a===n.length||za.includes(n[a])))n=(s===0?"":n.substring(0,s))+n.substring(a+1);else s=a}}}return n===""?null:n}function Jt(e,t,r,n,o,i){var s=e[Gr];if(x||s!==r||s===void 0){var a=Ka(r,n,i);if(!x||a!==e.getAttribute("class"))if(a==null)e.removeAttribute("class");else if(t)e.className=a;else e.setAttribute("class",a);e[Gr]=r}else if(i&&o!==i)for(var c in i){var v=!!i[c];if(o==null||v!==!!o[c])e.classList.toggle(c,v)}return i}function Li(e,t){if(t){if(!e.hasAttribute("selected"))e.setAttribute("selected","")}else e.removeAttribute("selected")}function wf(e,t){var{__defaultValue:r,multiple:n}=e,o=n?r??[]:null;if(n&&!lt(o))return;var i=e.selectedIndex,s=t&&n?new Set(e.selectedOptions):null;for(var a of e.options){var c=Yr(a);Li(a,n?o.includes(c):Un(c,r))}if(!t)return;if(s!==null)for(a of e.options){var v=s.has(a);if(a.selected!==v)a.selected=v}else if(e.selectedIndex!==i)e.selectedIndex=i}function Jn(e,t,r=!1){if(e.multiple){if(t==null)return;if(!lt(t))return Ls();for(var n of e.options)n.selected=t.includes(Yr(n));return}for(n of e.options){var o=Yr(n);if(Un(o,t)){n.selected=!0;return}}if(!r||t!==void 0)e.selectedIndex=-1}function pn(e){var t=new MutationObserver((r)=>{if(r.every($f))return;if("__defaultValue"in e)wf(e,!1);if("__value"in e)Jn(e,e.__value)});t.observe(e,{childList:!0,subtree:!0,attributes:!0,attributeFilter:["value"]}),ve(()=>{t.disconnect()})}function Qn(e,t,r=t){var n=new WeakSet,o=!0;Jr(e,"change",(i)=>{var s=i?"[selected]":":checked",a;if(e.multiple)a=[].map.call(e.querySelectorAll(s),Yr);else{var c=e.querySelector(s)??e.querySelector("option:not([disabled])");a=c&&Yr(c)}if(r(a),e.__value=a,k!==null)n.add(k)}),Xe(()=>{var i=t();if(e===document.activeElement){var s=ie?Ke:k;if(n.has(s))return}if(Jn(e,i,o),o&&i===void 0){var a=e.querySelector(":checked");if(a!==null)i=Yr(a),r(i)}e.__value=i,o=!1})}function Yr(e){if("__value"in e)return e.__value;else return e.value}function $f(e){if(e.target.closest("selectedcontent")!==null)return!0;if(e.type==="childList"){var t=[...e.addedNodes,...e.removedNodes];return t.length>0&&t.every((r)=>r.nodeName==="SELECTEDCONTENT")}return!1}var Sf=Symbol("class"),qf=Symbol("style"),Nf=Symbol("is custom element"),Rf=Symbol("is html"),Cf=Xo?"link":"LINK";function at(e){if(!x)return;var t=!1,r=()=>{if(t)return;if(t=!0,e.hasAttribute("value")){var n=e.value;Hr(e,"value",null),e.value=n}if(e.hasAttribute("checked")){var o=e.checked;Hr(e,"checked",null),e.checked=o}};e[Vt]=r,se(r),Fn()}function eo(e,t){var r=Ja(e);if(r.checked===(r.checked=t??void 0))return;e.checked=t}function Hr(e,t,r,n){var o=Ja(e);if(x){if(o[t]=e.getAttribute(t),t==="src"||t==="srcset"||t==="href"&&e.nodeName===Cf){if(!n)If(e,t,r??"");return}}var i=o[t];if(i===(o[t]=r))return;if(t==="loading")e[ks]=r;if(r==null)e.removeAttribute(t);else if(typeof r!=="string"&&Of(e).has(t))e[t]=r;else if(!(x&&(typeof r==="number"||typeof r==="boolean")&&i===String(r)))e.setAttribute(t,r)}function Ja(e){return e[An]??={[Nf]:e.nodeName.includes("-"),[Rf]:e.namespaceURI===Sn}}var Wa=new Map;function Of(e){var t=e.getAttribute("is")||e.nodeName,r=Wa.get(t);if(r)return r;Wa.set(t,r=new Set);var n,o=e,i=Element.prototype;while(i!==o){n=Vo(o);for(var s in n)if(n[s].set&&s!=="innerHTML"&&s!=="textContent"&&s!=="innerText")r.add(s);o=xr(o)}return r}function If(e,t,r){if(!u)return;if(t==="srcset"&&Df(e,r))return;if(Fi(e.getAttribute(t)??"",r))return;Ds(t,e.outerHTML.replace(e.innerHTML,e.innerHTML&&"..."),String(r))}function Fi(e,t){if(e===t)return!0;return new URL(e,document.baseURI).href===new URL(t,document.baseURI).href}function Xa(e){return e.split(",").map((t)=>t.trim().split(" ").filter(Boolean))}function Df(e,t){var r=Xa(e.srcset),n=Xa(t);return n.length===r.length&&n.every(([o,i],s)=>i===r[s][1]&&(Fi(r[s][0],o)||Fi(o,r[s][0])))}function Pt(e,t,r=t){var n=new WeakSet;if(Jr(e,"input",async(o)=>{if(u&&e.type==="checkbox")ti();var i=o?e.defaultValue:e.value;if(i=Pi(e)?Bi(i):i,r(i),k!==null)n.add(k);if(await ln(),i!==(i=t())){var{selectionStart:s,selectionEnd:a}=e,c=e.value.length;if(e.value=i??"",a!==null){var v=e.value.length;if(s===a&&a===c&&v>c)e.selectionStart=v,e.selectionEnd=v;else e.selectionStart=s,e.selectionEnd=Math.min(a,v)}}}),x&&e.defaultValue!==e.value||G(t)==null&&e.value){if(r(Pi(e)?Bi(e.value):e.value),k!==null)n.add(k)}ye(()=>{if(u&&e.type==="checkbox")ti();var o=t();if(e===document.activeElement){var i=ie?Ke:k;if(n.has(i))return}if(Pi(e)&&o===Bi(e.value))return;if(e.type==="date"&&!o&&!e.value)return;if(o!==e.value)e.value=o??""})}function Yi(e,t,r=t){if(Jr(e,"change",(n)=>{var o=n?e.defaultChecked:e.checked;r(o)}),x&&e.defaultChecked!==e.checked||G(t)==null)r(e.checked);ye(()=>{var n=t();e.checked=Boolean(n)})}function Pi(e){var t=e.type;return t==="number"||t==="range"}function Bi(e){return e===""?null:+e}var Oy=Symbol("unmounted");function Qa(e){return new el(e)}class el{#e;#t;constructor(e){var t=new Map,r=(o,i)=>{var s=pr(i,!1,!1);return t.set(o,s),s};let n=new Proxy({...e.props||{},$$events:{}},{get(o,i){return f(t.get(i)??r(i,Reflect.get(o,i)))},has(o,i){if(i===kn)return!0;return f(t.get(i)??r(i,Reflect.get(o,i))),Reflect.has(o,i)},set(o,i,s){return q(t.get(i)??r(i,s),s),Reflect.set(o,i,s)}});if(this.#t=(e.hydrate?Oi:Br)(e.component,{target:e.target,anchor:e.anchor,props:n,context:e.context,intro:e.intro??!1,recover:e.recover,transformError:e.transformError}),!ie&&(!e?.props?.$$host||e.sync===!1))zt();this.#e=n.$$events;for(let o of Object.keys(this.#t)){if(o==="$set"||o==="$destroy"||o==="$on")continue;_e(this,o,{get(){return this.#t[o]},set(i){this.#t[o]=i},enumerable:!0})}this.#t.$set=(o)=>{Object.assign(n,o)},this.#t.$destroy=()=>{Ii(this.#t)}}$set(e){this.#t.$set(e)}$on(e,t){this.#e[e]=this.#e[e]||[];let r=(...n)=>t.call(this,...n);return this.#e[e].push(r),()=>{this.#e[e]=this.#e[e].filter((n)=>n!==r)}}$destroy(){this.#t.$destroy()}}var Uf;if(typeof HTMLElement==="function")Uf=class extends HTMLElement{$$ctor;$$s;$$c;$$cn=!1;$$d={};$$r=!1;$$p_d={};$$l={};$$l_u=new Map;$$me;$$shadowRoot=null;constructor(e,t,r){super();if(this.$$ctor=e,this.$$s=t,r)this.$$shadowRoot=this.attachShadow(r)}addEventListener(e,t,r){if(this.$$l[e]=this.$$l[e]||[],this.$$l[e].push(t),this.$$c){let n=this.$$c.$on(e,t);this.$$l_u.set(t,n)}super.addEventListener(e,t,r)}removeEventListener(e,t,r){if(super.removeEventListener(e,t,r),this.$$c){let n=this.$$l_u.get(t);if(n)n(),this.$$l_u.delete(t)}}async connectedCallback(){if(this.$$cn=!0,!this.$$c){let e=function(n){return(o)=>{let i=kt("slot");if(n!=="default")i.name=n;P(o,i)}};if(await Promise.resolve(),!this.$$cn||this.$$c)return;let t={},r=Gf(this);for(let n of this.$$s)if(n in r)if(n==="default"&&!this.$$d.children)this.$$d.children=e(n),t.default=!0;else t[n]=e(n);for(let n of this.attributes){let o=this.$$g_p(n.name);if(!(o in this.$$d))this.$$d[o]=Vi(o,n.value,this.$$p_d,"toProp")}for(let n in this.$$p_d)if(!(n in this.$$d)&&this[n]!==void 0)this.$$d[n]=this[n],delete this[n];this.$$c=Qa({component:this.$$ctor,target:this.$$shadowRoot||this,props:{...this.$$d,$$slots:t,$$host:this}}),this.$$me=xi(()=>{ye(()=>{this.$$r=!0;for(let n of Ho(this.$$c)){if(!this.$$p_d[n]?.reflect)continue;this.$$d[n]=this.$$c[n];let o=Vi(n,this.$$d[n],this.$$p_d,"toAttribute");if(o==null)this.removeAttribute(this.$$p_d[n].attribute||n);else this.setAttribute(this.$$p_d[n].attribute||n,o)}this.$$r=!1})});for(let n in this.$$l)for(let o of this.$$l[n]){let i=this.$$c.$on(n,o);this.$$l_u.set(o,i)}this.$$l={}}}attributeChangedCallback(e,t,r){if(this.$$r)return;e=this.$$g_p(e),this.$$d[e]=Vi(e,r,this.$$p_d,"toProp"),this.$$c?.$set({[e]:this.$$d[e]})}disconnectedCallback(){this.$$cn=!1,Promise.resolve().then(()=>{if(!this.$$cn&&this.$$c)this.$$c.$destroy(),this.$$me(),this.$$c=void 0})}$$g_p(e){return Ho(this.$$p_d).find((t)=>this.$$p_d[t].attribute===e||!this.$$p_d[t].attribute&&t.toLowerCase()===e)||e}};function Vi(e,t,r,n){let o=r[e]?.type;if(t=o==="Boolean"&&typeof t!=="boolean"?t!=null:t,!n||!r[e])return t;else if(n==="toAttribute")switch(o){case"Object":case"Array":return t==null?null:JSON.stringify(t);case"Boolean":return t?"":null;case"Number":return t==null?null:t;default:return t}else switch(o){case"Object":case"Array":return t&&JSON.parse(t);case"Boolean":return t;case"Number":return t!=null?+t:t;default:return t}}function Gf(e){let t={};return e.childNodes.forEach((r)=>{t[r.slot||"default"]=!0}),t}if(u){let e=function(t){if(!(t in globalThis)){let r;Object.defineProperty(globalThis,t,{configurable:!0,get:()=>{if(r!==void 0)return r;Ws(t)},set:(n)=>{r=n}})}};e("$state"),e("$effect"),e("$derived"),e("$inspect"),e("$props"),e("$bindable")}function tl(e){if(O===null)Cn("onMount");if(Ut&&O.l!==null)Xf(O).m.push(e);else Mr(()=>{let t=G(e);if(typeof t==="function")return t})}function Xf(e){var t=e.l;return t.u??={a:[],b:[],m:[]}}var rl="5";if(typeof window<"u")((window.__svelte??={}).v??=new Set).add(rl);var Zf=xe('<br class="svelte-2hlthq23qytbf"/> ',1),Jf=xe('<p class="waktu svelte-2hlthq23qytbf"> <!></p>'),Qf=xe('<span class="svelte-2hlthq23qytbf"> </span>'),ec=xe('<div class="peringatan svelte-2hlthq23qytbf"></div>'),vr=xe('<p class="ket svelte-2hlthq23qytbf"> </p>'),tc=xe('<img alt=""/> <!>',1),rc=xe('<button class="stop svelte-2hlthq23qytbf"> </button>'),nc=xe("<button> </button>"),oc=xe('<li class="svelte-2hlthq23qytbf"><b class="svelte-2hlthq23qytbf"> </b> <span class="svelte-2hlthq23qytbf"> <small class="svelte-2hlthq23qytbf"> </small></span> <button class="hapus svelte-2hlthq23qytbf">✕</button></li>'),ic=xe('<ul class="alarm svelte-2hlthq23qytbf"></ul>'),sc=xe('<label class="svelte-2hlthq23qytbf"><input type="checkbox" class="svelte-2hlthq23qytbf"/> </label>'),ac=xe('<div class="hari svelte-2hlthq23qytbf"></div>'),lc=xe('<small class="svelte-2hlthq23qytbf"> </small>'),fc=xe('<button><b class="svelte-2hlthq23qytbf"> </b> <span class="svelte-2hlthq23qytbf"> </span></button>'),cc=xe('<p class="status svelte-2hlthq23qytbf"> </p>'),uc=xe('<main class="svelte-2hlthq23qytbf"><header class="svelte-2hlthq23qytbf"><h1 class="svelte-2hlthq23qytbf"> </h1> <span></span> <span class="kecil svelte-2hlthq23qytbf"><!></span></header> <!> <!> <section class="bicara svelte-2hlthq23qytbf"><button> </button> <p class="ket kecil-ket svelte-2hlthq23qytbf">Ketuk, lalu bicara ke mikrofon robot. Robot berhenti merekam saat kamu diam (paling lama 6 detik).</p> <label class="saklar svelte-2hlthq23qytbf"><input type="checkbox" class="svelte-2hlthq23qytbf"/> </label></section> <section class="kamera svelte-2hlthq23qytbf"><!> <button class="kecil-btn svelte-2hlthq23qytbf"> </button></section> <section class="svelte-2hlthq23qytbf"><h2 class="svelte-2hlthq23qytbf">Gerak <small class="svelte-2hlthq23qytbf">tahan untuk jalan, lepas untuk berhenti</small></h2> <div class="dpad svelte-2hlthq23qytbf"></div> <label class="svelte-2hlthq23qytbf"> <input type="range" min="100" max="255" class="svelte-2hlthq23qytbf"/></label> <div class="baris belok svelte-2hlthq23qytbf"><button class="svelte-2hlthq23qytbf">⟲ 90° kiri</button> <button class="svelte-2hlthq23qytbf">↻ balik</button> <button class="svelte-2hlthq23qytbf">90° kanan ⟳</button></div> <p class="ket kecil-ket svelte-2hlthq23qytbf">Belok tepat memakai giroskop di badan; sudutnya tidak berubah walau baterai melemah.</p></section> <section class="svelte-2hlthq23qytbf"><h2 class="svelte-2hlthq23qytbf">Kepala</h2> <label class="svelte-2hlthq23qytbf"> <input type="range" min="-60" max="60" class="svelte-2hlthq23qytbf"/></label> <label class="svelte-2hlthq23qytbf"> <input type="range" min="40" max="150" class="svelte-2hlthq23qytbf"/></label> <button class="kecil-btn svelte-2hlthq23qytbf">Lurus ke depan</button></section> <section class="svelte-2hlthq23qytbf"><h2 class="svelte-2hlthq23qytbf">Musik dan suara</h2> <div class="baris svelte-2hlthq23qytbf"><button class="svelte-2hlthq23qytbf">Lagu berikutnya</button> <button class="svelte-2hlthq23qytbf">Radio berikutnya</button> <button class="svelte-2hlthq23qytbf">Diam</button></div> <label class="svelte-2hlthq23qytbf"> <input type="range" min="0" max="21" class="svelte-2hlthq23qytbf"/></label> <form class="svelte-2hlthq23qytbf"><input type="text" placeholder="Suruh Zond bilang..." maxlength="140" class="svelte-2hlthq23qytbf"/> <button type="submit" class="svelte-2hlthq23qytbf">Bilang</button></form></section> <section class="svelte-2hlthq23qytbf"><h2 class="svelte-2hlthq23qytbf">Alarm dan pengingat</h2> <!> <form class="form-alarm svelte-2hlthq23qytbf"><div class="baris svelte-2hlthq23qytbf"><input type="time" required="" class="svelte-2hlthq23qytbf"/> <select class="svelte-2hlthq23qytbf"><option class="svelte-2hlthq23qytbf">Sekali</option><option class="svelte-2hlthq23qytbf">Setiap hari</option><option class="svelte-2hlthq23qytbf">Senin-Jumat</option><option class="svelte-2hlthq23qytbf">Sabtu-Minggu</option><option class="svelte-2hlthq23qytbf">Pilih hari</option></select></div> <!> <input type="text" placeholder="Pesan, misalnya: bangun, minum obat" maxlength="79" class="svelte-2hlthq23qytbf"/> <div class="baris svelte-2hlthq23qytbf"><select class="svelte-2hlthq23qytbf"><option class="svelte-2hlthq23qytbf">Suara saja</option><option class="svelte-2hlthq23qytbf">Suara + radio</option><option class="svelte-2hlthq23qytbf">Suara + lagu SD</option></select> <button type="submit" class="svelte-2hlthq23qytbf">Pasang</button></div> <!></form> <p class="ket kecil-ket svelte-2hlthq23qytbf">Alarm sebelum jam 11 siang sekalian menyebut cuaca hari ini. Pengingat juga dikirim ke Telegram.</p></section> <section class="svelte-2hlthq23qytbf"><h2 class="svelte-2hlthq23qytbf">Fitur kamera AI</h2> <label class="saklar svelte-2hlthq23qytbf"><input type="checkbox" class="svelte-2hlthq23qytbf"/> Aktifkan "lihat sekeliling" dan "suit". Tiap kali dipakai, foto dikirim ke AI dan memakai jatah AI lebih cepat.</label> <div class="baris svelte-2hlthq23qytbf"><button class="svelte-2hlthq23qytbf">Lihat sekeliling</button> <button class="svelte-2hlthq23qytbf">Main suit</button></div> <!> <!></section> <section class="svelte-2hlthq23qytbf"><h2 class="svelte-2hlthq23qytbf">Otak AI <!></h2> <div class="ai svelte-2hlthq23qytbf"></div> <!> <p class="ket kecil-ket svelte-2hlthq23qytbf">Kalau pilihanmu gagal (jatah habis atau saldo kosong), robot otomatis memakai yang gratis.</p></section> <!></main>');function pc(e,t){Wr(t,!0);function r(l){re(`/api/belok?d=${l}`)}let n=["Min","Sen","Sel","Rab","Kam","Jum","Sab"],o=["suara","radio","lagu SD"],i=M(ht([])),s=M("06:00"),a=M("0"),c=ht([!1,!0,!0,!0,!0,!0,!1]),v=M(""),h=M(0),m=M("");async function d(){let l=await re("/api/alarm");if(l&&l.ok)q(i,(await l.json()).alarm,!0)}async function p(l){l.preventDefault();let _=f(a)==="pilih"?c.reduce((ne,X,fe)=>X?ne|1<<fe:ne,0):Number(f(a)),E=new URLSearchParams({jam:f(s),hari:String(_),pesan:f(v).trim(),bunyi:String(f(h))}),F=await re(`/api/alarm/tambah?${E}`);if(q(m,!F?"Robot tidak menjawab":F.ok?"":await F.text(),!0),F?.ok)q(v,"");await d()}async function b(l){await re(`/api/alarm/hapus?i=${l}`),await d()}let A=M("");async function g(l){let _=l.currentTarget.checked;await re(`/api/fiturkamera?on=${_?1:0}`),await Vr()}async function R(l,_){let E=await re(l);q(A,!E?"Robot tidak menjawab":E.ok?_:await E.text(),!0),setTimeout(()=>q(A,""),6000)}let L=M(!1);async function ee(){q(L,!0),await re("/api/bicara"),setTimeout(()=>q(L,!1),1500)}async function we(l){let _=l.currentTarget.checked;await re(`/api/dengar?on=${_?1:0}`),await Vr()}let St=[{id:"otomatis",nama:"Otomatis",ket:"gratis: Gemini, lalu Gemma, lalu Groq"},{id:"gemini",nama:"Gemini",ket:"gratis, jatah harian kecil"},{id:"gemma",nama:"Gemma",ket:"gratis, jatah besar"},{id:"groq",nama:"Groq",ket:"gratis, cepat"},{id:"haiku",nama:"Claude Haiku",ket:"berbayar per pemakaian"}],qe=M(""),ae=M("");async function Bt(l){q(qe,l,!0),q(ae,"");let _=await re(`/api/ai?m=${l}`);if(!_)q(ae,"Robot tidak menjawab");else if(!_.ok)q(ae,await _.text(),!0);await Vr(),q(qe,"")}let T=M(null),le=M(!1),Qe=M(160),qt=M(null),Ne=M(0),Qt=M(90),er=M(12),ro=!1,dn=M(""),tr=M(!0),_n=M(!1),ji=M("");async function re(l,_=1500){let E=new AbortController,F=setTimeout(()=>E.abort(),_);try{return await fetch(l,{signal:E.signal,cache:"no-store"})}catch{return null}finally{clearTimeout(F)}}async function Vr(){let l=await re("/api/status");if(!l||!l.ok){q(le,!1);return}if(q(le,!0),q(T,await l.json(),!0),!ro)q(er,f(T).vol,!0)}let ol={maju:[1,1],mundur:[-1,-1],kiri:[-1,1],kanan:[1,-1],majukiri:[0.35,1],majukanan:[1,0.35],mundurkiri:[-0.35,-1],mundurkanan:[-1,-0.35]},gr=null,no=!1;function Ui(){if(!f(qt)||no)return;let[l,_]=ol[f(qt)];no=!0,re(`/api/gerak?l=${Math.round(l*f(Qe))}&r=${Math.round(_*f(Qe))}`,600).finally(()=>no=!1)}function Gi(l){if(q(qt,l,!0),Ui(),gr)clearInterval(gr);gr=setInterval(Ui,150)}function Nt(){if(gr)clearInterval(gr);if(gr=null,f(qt))re("/api/stop");q(qt,null)}function il(l,_){l.currentTarget.setPointerCapture(l.pointerId),Gi(_)}let zi={ArrowUp:"maju",w:"maju",ArrowDown:"mundur",s:"mundur",ArrowLeft:"kiri",a:"kiri",ArrowRight:"kanan",d:"kanan"};function sl(l){if(l.target.tagName==="INPUT")return;if(l.key===" "){l.preventDefault(),Nt(),re("/api/stop");return}let _=zi[l.key];if(_&&!l.repeat)l.preventDefault(),Gi(_)}function al(l){if(zi[l.key])Nt()}let Ki=0,oo=null;function io(){let l=Date.now();if(oo)clearTimeout(oo);let _=()=>{Ki=Date.now(),re(`/api/kepala?p=${90-f(Ne)}&t=${f(Qt)}`)};if(l-Ki>150)_();else oo=setTimeout(_,150)}function ll(){q(Ne,0),q(Qt,90),io()}async function fl(){ro=!0,await re(`/api/volume?v=${f(er)}`),setTimeout(()=>ro=!1,1500)}async function cl(l){l.preventDefault();let _=f(dn).trim();if(!_)return;q(dn,""),await re(`/api/bilang?t=${encodeURIComponent(_)}`,3000)}function hn(l){setTimeout(()=>{if(!f(tr)||!f(T)?.kamera){if(f(tr))hn(1000);return}q(ji,`http://${f(T).kamera}/capture?res=vga&t=${Date.now()}`)},l)}function ul(l){let _=[];if(l&1)_.push("Tombol STOP terkunci");if(l&2)_.push("Baterai lemah");if(l&4)_.push("Halangan depan");if(l&8)_.push("Halangan belakang");if(l&16)_.push("Robot miring");return _}tl(()=>{Vr(),d();let l=setInterval(Vr,1000),_=setInterval(d,30000),E=()=>Nt(),F=()=>document.hidden&&Nt();return window.addEventListener("blur",E),document.addEventListener("visibilitychange",F),()=>{clearInterval(l),clearInterval(_),window.removeEventListener("blur",E),document.removeEventListener("visibilitychange",F),Nt()}}),Mr(()=>{if(f(tr))hn(0)});var so=uc();mt("keydown",Dr,sl),mt("keyup",Dr,al);var ao=D(so),Wi=D(ao),pl=te(Wi,!0),Xi=y(Wi,2);let Zi;var Ji=y(Xi,2),dl=D(Ji);{var _l=(l)=>{var _=fn("otak tidak terhubung");P(l,_)},hl=(l)=>{var _=fn("badan tidak menjawab");P(l,_)},ml=(l)=>{var _=fn();W((E)=>B(_,`${E??""} V · depan ${f(T).depan??""} cm · belakang ${f(T).belakang??""} cm`),[()=>f(T).baterai.toFixed(1)]),P(l,_)};Se(dl,(l)=>{if(!f(le))l(_l);else if(!f(T)?.hidup)l(hl,1);else l(ml,-1)})}C(Ji),C(ao);var Qi=y(ao,2);{var vl=(l)=>{var _=Jf(),E=D(_),F=y(E);{var ne=(X)=>{var fe=Zf(),et=y(sn(fe),1,!0);W(()=>B(et,f(T).cuaca)),P(X,fe)};Se(F,(X)=>{if(f(T).cuaca)X(ne)})}C(_),W(()=>B(E,`${f(T).jam??""} · ${f(T).tanggal??""}`)),P(l,_)};Se(Qi,(l)=>{if(f(T)?.jam)l(vl)})}var es=y(Qi,2);{var gl=(l)=>{var _=ec();mr(_,20,()=>ul(f(T).flag),(E)=>E,(E,F)=>{var ne=Qf(),X=te(ne,!0);W(()=>B(X,F)),P(E,ne)}),C(_),P(l,_)};Se(es,(l)=>{if(f(T)&&f(T).flag)l(gl)})}var lo=y(es,2),jr=D(lo);let ts;var bl=te(jr,!0),rs=y(jr,4),Ur=D(rs);at(Ur);var yl=y(Ur);C(rs),C(lo);var fo=y(lo,2),ns=D(fo);{var xl=(l)=>{var _=tc(),E=sn(_);let F;var ne=y(E,2);{var X=(fe)=>{var et=vr(),Rt=te(et);W(()=>B(Rt,`Kamera tidak terhubung (${f(T).kamera??""})`)),P(fe,et)};Se(ne,(fe)=>{if(f(_n))fe(X)})}W(()=>{Hr(E,"src",f(ji)),F=Jt(E,1,"svelte-2hlthq23qytbf",null,F,{redup:f(_n)})}),mt("load",E,()=>{q(_n,!1),hn(100)}),mt("error",E,()=>{q(_n,!0),hn(1500)}),Ti(E),P(l,_)},wl=(l)=>{var _=vr(),E=te(_,!0);W(()=>B(E,f(T)?.kamera?"Kamera dimatikan":"Alamat kamera belum diketahui")),P(l,_)};Se(ns,(l)=>{if(f(tr)&&f(T)?.kamera)l(xl);else l(wl,-1)})}var os=y(ns,2),$l=te(os,!0);C(fo);var co=y(fo,2),mn=y(D(co),2);mr(mn,20,()=>[["majukiri","↖"],["maju","▲"],["majukanan","↗"],["kiri","◀"],["stop","■"],["kanan","▶"],["mundurkiri","↙"],["mundur","▼"],["mundurkanan","↘"]],([l,_])=>l,(l,_)=>{var E=li(()=>Go(_,2));let F=()=>f(E)[0],ne=()=>f(E)[1];var X=qi(),fe=sn(X);{var et=(Ct)=>{var He=rc(),br=te(He,!0);W(()=>B(br,ne())),z("pointerdown",He,()=>{Nt(),re("/api/stop")}),P(Ct,He)},Rt=(Ct)=>{var He=nc();let br;var yn=te(He,!0);W(()=>{br=Jt(He,1,"svelte-2hlthq23qytbf",null,br,{aktif:f(qt)===F()}),B(yn,ne())}),z("pointerdown",He,(Yl)=>il(Yl,F())),z("pointerup",He,Nt),mt("pointercancel",He,Nt),mt("lostpointercapture",He,Nt),P(Ct,He)};Se(fe,(Ct)=>{if(F()==="stop")Ct(et);else Ct(Rt,-1)})}P(l,X)}),C(mn);var uo=y(mn,2),is=D(uo),ss=y(is);at(ss),C(uo);var as=y(uo,2),ls=D(as),fs=y(ls,2),El=y(fs,2);C(as),yt(2),C(co);var po=y(co,2),_o=y(D(po),2),cs=D(_o),ho=y(cs);at(ho),C(_o);var mo=y(_o,2),us=D(mo),vo=y(us);at(vo),C(mo);var kl=y(mo,2);C(po);var go=y(po,2),bo=y(D(go),2),ps=D(bo),ds=y(ps,2),Tl=y(ds,2);C(bo);var yo=y(bo,2),_s=D(yo),xo=y(_s);at(xo),C(yo);var wo=y(yo,2),hs=D(wo);at(hs),yt(2),C(wo),C(go);var $o=y(go,2),ms=y(D($o),2);{var Al=(l)=>{var _=ic();mr(_,21,()=>f(i),Mi,(E,F,ne)=>{var X=oc(),fe=D(X),et=te(fe,!0),Rt=y(fe,2),Ct=D(Rt,!0),He=y(Ct),br=te(He);C(Rt);var yn=y(Rt,2);C(X),W(()=>{B(et,f(F).jam),B(Ct,f(F).pesan),B(br,`${f(F).kapan??""} · ${o[f(F).bunyi]??"suara"??""}`),Hr(yn,"aria-label",`Hapus alarm ${f(F).jam??""}`)}),z("click",yn,()=>b(ne)),P(E,X)}),C(_),P(l,_)},Sl=(l)=>{var _=vr(),E=te(_);W(()=>B(E,`Belum ada alarm. Bisa juga lewat suara: "${f(T)?.nama??"Zond"??""}, ingatkan aku jam 5 sore minum obat".`)),P(l,_)};Se(ms,(l)=>{if(f(i).length)l(Al);else l(Sl,-1)})}var Eo=y(ms,2),ko=D(Eo),To=D(ko);at(To);var vn=y(To,2),Ao=D(vn);Ao.value=Ao.__value="0";var So=y(Ao);So.value=So.__value="127";var qo=y(So);qo.value=qo.__value="62";var No=y(qo);No.value=No.__value="65";var vs=y(No);vs.value=vs.__value="pilih",C(vn),pn(vn),C(ko);var gs=y(ko,2);{var ql=(l)=>{var _=ac();mr(_,22,()=>n,(E)=>E,(E,F,ne)=>{var X=sc(),fe=D(X);at(fe);var et=y(fe,1,!0);C(X),W(()=>B(et,F)),Yi(fe,()=>c[f(ne)],(Rt)=>c[f(ne)]=Rt),P(E,X)}),C(_),P(l,_)};Se(gs,(l)=>{if(f(a)==="pilih")l(ql)})}var Ro=y(gs,2);at(Ro);var Co=y(Ro,2),gn=D(Co),Oo=D(gn);Oo.value=Oo.__value=0;var Io=y(Oo);Io.value=Io.__value=1;var bs=y(Io);bs.value=bs.__value=2,C(gn),pn(gn),yt(2),C(Co);var Nl=y(Co,2);{var Rl=(l)=>{var _=vr(),E=te(_,!0);W(()=>B(E,f(m))),P(l,_)};Se(Nl,(l)=>{if(f(m))l(Rl)})}C(Eo),yt(2),C($o);var Do=y($o,2),Mo=y(D(Do),2),bn=D(Mo);at(bn),yt(),C(Mo);var Lo=y(Mo,2),Fo=D(Lo),ys=y(Fo,2);C(Lo);var xs=y(Lo,2);{var Cl=(l)=>{var _=vr(),E=te(_);W(()=>B(E,`Skor suit: kamu ${f(T).skorKamu??""}, ${f(T).nama??""} ${f(T).skorZond??""}`)),P(l,_)};Se(xs,(l)=>{if(f(T)?.fiturKamera&&(f(T).skorKamu||f(T).skorZond))l(Cl)})}var Ol=y(xs,2);{var Il=(l)=>{var _=vr(),E=te(_,!0);W(()=>B(E,f(A))),P(l,_)};Se(Ol,(l)=>{if(f(A))l(Il)})}C(Do);var Po=y(Do,2),Bo=D(Po),Dl=y(D(Bo));{var Ml=(l)=>{var _=lc(),E=te(_);W(()=>B(E,`terakhir menjawab: ${f(T).aiTerakhir??""}`)),P(l,_)};Se(Dl,(l)=>{if(f(T)?.aiTerakhir)l(Ml)})}C(Bo);var Yo=y(Bo,2);mr(Yo,21,()=>St,(l)=>l.id,(l,_)=>{var E=fc();let F;var ne=D(E),X=te(ne,!0),fe=y(ne,2),et=te(fe,!0);C(E),W(()=>{E.disabled=!f(T)?.aiTersedia?.[f(_).id]||f(qe)!=="",F=Jt(E,1,"svelte-2hlthq23qytbf",null,F,{pilih:f(T)?.ai===f(_).id}),B(X,f(_).nama),B(et,f(T)?.aiTersedia?.[f(_).id]===!1?"API key belum diisi":f(_).ket)}),z("click",E,()=>Bt(f(_).id)),P(l,E)}),C(Yo);var Ll=y(Yo,2);{var Fl=(l)=>{var _=vr(),E=te(_,!0);W(()=>B(E,f(ae))),P(l,_)};Se(Ll,(l)=>{if(f(ae))l(Fl)})}yt(2),C(Po);var Pl=y(Po,2);{var Bl=(l)=>{var _=cc(),E=te(_,!0);W(()=>B(E,f(T).status)),P(l,_)};Se(Pl,(l)=>{if(f(T)?.status)l(Bl)})}C(so),W(()=>{B(pl,f(T)?.nama??"Robot"),Zi=Jt(Xi,1,"titik svelte-2hlthq23qytbf",null,Zi,{on:f(le)&&f(T)?.hidup}),ts=Jt(jr,1,"mik svelte-2hlthq23qytbf",null,ts,{dengar:f(T)?.mendengar||f(L)}),jr.disabled=!f(le),B(bl,f(T)?.mendengar?"Mendengarkan... bicara sekarang":"Bicara ke "+(f(T)?.nama??"robot")),eo(Ur,f(T)?.dengarTerus??!1),Ur.disabled=!f(le),B(yl,` Dengar terus (cukup sebut "${f(T)?.nama??"nama robot"??""}"). Boros: setiap suara di ruangan memakai jatah suara-ke-teks.`),B($l,f(tr)?"Matikan kamera":"Nyalakan kamera"),B(is,`Kecepatan ${f(Qe)??""}`),B(cs,`Toleh ${f(Ne)<0?"kiri":f(Ne)>0?"kanan":"lurus"}`),B(us,`Angguk ${f(Qt)??""}°`),B(_s,`Volume ${f(er)??""}`),eo(bn,f(T)?.fiturKamera??!1),bn.disabled=!f(le),Fo.disabled=!f(T)?.fiturKamera,ys.disabled=!f(T)?.fiturKamera}),z("click",jr,ee),z("change",Ur,we),z("click",os,()=>q(tr,!f(tr))),z("contextmenu",mn,(l)=>l.preventDefault()),Pt(ss,()=>f(Qe),(l)=>q(Qe,l)),z("click",ls,()=>r(-90)),z("click",fs,()=>r(180)),z("click",El,()=>r(90)),z("input",ho,io),Pt(ho,()=>f(Ne),(l)=>q(Ne,l)),z("input",vo,io),Pt(vo,()=>f(Qt),(l)=>q(Qt,l)),z("click",kl,ll),z("click",ps,()=>re("/api/lagu",4000)),z("click",ds,()=>re("/api/radio",4000)),z("click",Tl,()=>re("/api/diam")),z("change",xo,fl),Pt(xo,()=>f(er),(l)=>q(er,l)),mt("submit",wo,cl),Pt(hs,()=>f(dn),(l)=>q(dn,l)),mt("submit",Eo,p),Pt(To,()=>f(s),(l)=>q(s,l)),Qn(vn,()=>f(a),(l)=>q(a,l)),Pt(Ro,()=>f(v),(l)=>q(v,l)),Qn(gn,()=>f(h),(l)=>q(h,l)),z("change",bn,g),z("click",Fo,()=>R("/api/sekeliling","Zond menoleh dan memotret. Ceritanya dikirim ke Telegram.")),z("click",ys,()=>R("/api/suit","Siapkan tanganmu di depan kamera!")),P(e,so),Xr()}var nl=pc;Wn(["click","change","contextmenu","pointerdown","pointerup","input"]);Br(nl,{target:document.body});
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
  antrianKirim = xQueueCreate(6, sizeof(PesanKirim));
  kunciAlarm = xSemaphoreCreateMutex();
  muatAlarm();

  pref.begin("robot", false);
  uint8_t arah = ARAH_LAYAR, geo = pref.getUChar("geo", 1), rgb = pref.getUChar("rgb", 1);   // geo dan rgb: pilihan di tes_layar
  if (pref.getUChar("rot", 255) != arah) pref.putUChar("rot", arah);   // tes_media, tes_sd, tes_musik ikut arah ini
  pilihanAI = pref.getUChar("ai", AI_OTOMATIS);                         // otak AI pilihan terakhir (panel web / /ai)
  dengarTerus = pref.getUChar("dengar", 0) != 0;                        // bawaan mati: hemat jatah suara-ke-teks
  fiturKamera = pref.getUChar("kamAI", 0) != 0;                         // lihat sekeliling dan suit: bawaan mati
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

  xTaskCreatePinnedToCore(tugasAudio, "audio", 12288, nullptr, 3, nullptr, 1);
  xTaskCreatePinnedToCore(tugasJaringan, "jaringan", 16384, nullptr, 1, nullptr, 0);
  xTaskCreatePinnedToCore(tugasDengar, "dengar", 6144, nullptr, 2, nullptr, 0);
  if (WiFi.status() == WL_CONNECTED) xTaskCreatePinnedToCore(tugasWeb, "web", 8192, nullptr, 1, nullptr, 0);
}

void loop() {
  bacaBadan();
  tanganiSentuh();
  jalankanMode();
  ekspresiOtomatis();
  cekAlarm();
  // radio/lagu dari alarm diputar setelah pengingat selesai diucapkan
  if (bunyiTertunda && millis() - tBunyiTertunda > 2000 && !audioJalan) {
    int b = bunyiTertunda; bunyiTertunda = 0;
    if (b == 1 && WiFi.status() == WL_CONNECTED) kirimAudio(A_URL, DAFTAR_RADIO[0]);
    else if (b == 2) tekanTombol(T_LAGU);
  }
  static unsigned long tWajah = 0;
  if (millis() - tWajah > 100) { tWajah = millis(); gambarWajah(); }
  delay(5);
}
