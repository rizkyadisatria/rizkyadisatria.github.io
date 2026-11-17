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

  SUARA COWOK (opsional): isi AZURE_KUNCI dari Azure Speech (gratis 500.000 huruf/bulan). Kosong = suara Google.

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
#define AZURE_KUNCI      ""                       // suara cowok Indonesia dari Azure Speech (Key 1). Kosong = suara Google
#define AZURE_WILAYAH    "southeastasia"          // "Location/Region" di halaman Keys and Endpoint Azure
#define AZURE_SUARA      "id-ID-ArdiNeural"       // cowok. id-ID-GadisNeural = cewek
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
#ifndef AZURE_KUNCI
#define AZURE_KUNCI ""
#endif
#ifndef AZURE_WILAYAH
#define AZURE_WILAYAH "southeastasia"
#endif
#ifndef AZURE_SUARA
#define AZURE_SUARA "id-ID-ArdiNeural"
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
volatile bool adaFFat = false;               // penyimpanan di flash (FATFS): tempat file suara Azure
volatile bool pakaiAzure = true;             // suara bicara: Azure (kalau kuncinya diisi) atau Google
volatile int azureGalat = 0;                 // kode galat Azure terakhir, 0 = belum pernah gagal
volatile uint32_t hurufAzure = 0;            // huruf yang dikirim ke Azure bulan ini (jatah gratis 500.000)
unsigned long azureIstirahat = 0;            // Azure tidak dicoba lagi sampai waktu ini (setelah ditolak)
volatile int persenBat = -1, sisaMenitBat = -1;   // isi baterai (%) dan perkiraan sisa waktu (menit)
float batHalus = -1;                         // tegangan baterai yang dihaluskan
volatile unsigned long jelajahSampai = 0;    // mode jelajah berhenti sendiri pada waktu ini

// ======================= AUDIO (task sendiri) =======================
enum JenisAudio { A_FILE, A_URL, A_TTS, A_STOP, A_VOL };
int volSekarang = VOLUME_AWAL;   // volume yang sedang dipakai, 0..21
struct PerintahAudio { JenisAudio jenis; char isi[320]; };

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

// Suara Azure dipakai kalau dipilih, kuncinya diisi, penyimpanan flash siap, dan tidak sedang diistirahatkan.
bool azureAktif() {
  return pakaiAzure && strlen(AZURE_KUNCI) > 0 && adaFFat && (!azureIstirahat || (long)(millis() - azureIstirahat) >= 0);
}

// Kalimat dipecah: Google TTS hanya mau potongan pendek (<= 150 huruf), Azure boleh lebih panjang (jeda lebih sedikit).
void ucapkan(const String& teks) {
  String t = teks; t.trim();
  int maks = azureAktif() ? 300 : 150;
  while (t.length()) kirimAudio(A_TTS, potongDepan(t, maks));
}

// ======================= SUARA AZURE (cowok Indonesia) =======================
// Teks dikirim ke Azure Speech, hasilnya MP3 disimpan di flash (/suara.mp3) lalu diputar.
// Kalau gagal (kunci salah, jatah habis, internet putus) kalimat itu diucapkan suara Google.
String xmlAman(const String& t) {
  String o; o.reserve(t.length() + 16);
  for (unsigned int i = 0; i < t.length(); i++) {
    char c = t[i];
    if (c == '&') o += "&amp;"; else if (c == '<') o += "&lt;"; else if (c == '>') o += "&gt;";
    else if (c == '"') o += "&quot;"; else if (c == '\'') o += "&apos;"; else o += c;
  }
  return o;
}

// Hitung huruf yang terpakai bulan ini (disimpan, jadi tetap tercatat walau robot dimatikan).
void catatHurufAzure(int n) {
  struct tm j;
  uint32_t bulan = jamSiap(j) ? (j.tm_year + 1900) * 100 + j.tm_mon + 1 : 0;
  Preferences q; q.begin("robot", false);
  if (bulan && q.getUInt("azB", 0) != bulan) { q.putUInt("azB", bulan); q.putUInt("azH", 0); }
  hurufAzure = q.getUInt("azH", 0) + n;
  q.putUInt("azH", hurufAzure);
  q.end();
}

// 0 = berhasil. Selain itu kode HTTP dari Azure, atau negatif kalau sambungan/penyimpanan gagal.
int unduhAzure(const String& teks) {
  static WiFiClientSecure klien;                       // sambungan dipakai ulang supaya kalimat berikutnya lebih cepat
  klien.setInsecure();
  HTTPClient http;
  http.setReuse(true);
  if (!http.begin(klien, String("https://") + AZURE_WILAYAH + ".tts.speech.microsoft.com/cognitiveservices/v1")) return -1;
  http.setTimeout(8000);
  http.addHeader("Ocp-Apim-Subscription-Key", AZURE_KUNCI);
  http.addHeader("Content-Type", "application/ssml+xml");
  http.addHeader("X-Microsoft-OutputFormat", "audio-24khz-48kbitrate-mono-mp3");
  http.setUserAgent("Zond");
  String ssml = String("<speak version='1.0' xml:lang='id-ID'><voice name='") + AZURE_SUARA + "'>" + xmlAman(teks) + "</voice></speak>";
  int kode = http.POST(ssml);
  if (kode != 200) { http.end(); return kode > 0 ? kode : -2; }
  File f = FFat.open("/suara.mp3", FILE_WRITE);
  if (!f) { http.end(); return -3; }
  int n = http.writeToStream(&f);
  f.close();
  http.end();
  return n > 0 ? 0 : -4;
}

// Dipanggil tugas audio. true = sedang diputar dengan suara Azure, false = pakai Google.
bool bicaraAzure(const String& teks) {
  if (!azureAktif() || WiFi.status() != WL_CONNECTED) return false;
  unsigned long t0 = millis();
  int h = unduhAzure(teks);
  if (h != 0) {
    Serial.printf("[suara] Azure gagal (%d), kalimat ini memakai suara Google\n", h);
    if (h != azureGalat && (h == 401 || h == 403 || h == 404))
      kirimTelegram("Suara Azure ditolak (kode " + String(h) + "). Cek AZURE_KUNCI dan AZURE_WILAYAH di kode, atau jatah gratis bulan ini habis. "
                    "Sementara memakai suara Google.");
    azureGalat = h;
    if (h == 401 || h == 403 || h == 404) azureIstirahat = millis() + 600000UL;   // coba lagi 10 menit kemudian
    else if (h == 429) azureIstirahat = millis() + 60000UL;                        // terlalu sering: tunggu 1 menit
    return false;
  }
  azureGalat = 0;
  catatHurufAzure(teks.length());
  Serial.printf("[suara] Azure %u ms, %u huruf bulan ini\n", (unsigned)(millis() - t0), (unsigned)hurufAzure);
  return audio.connecttoFS(FFat, "/suara.mp3");
}

void aturSuara(bool azure) {
  pakaiAzure = azure;
  Preferences q; q.begin("robot", false); q.putUChar("suara", azure ? 1 : 0); q.end();
}

String teksSuara() {
  if (!strlen(AZURE_KUNCI)) return "Suara: Google (AZURE_KUNCI di kode masih kosong).";
  if (!adaFFat) return "Suara: Google (penyimpanan flash tidak siap. Partition Scheme harus 16M Flash (3MB APP/9.9MB FATFS)).";
  String t = String("Suara: ") + (pakaiAzure ? "Azure " AZURE_SUARA : "Google") + ". Azure bulan ini " + String((uint32_t)hurufAzure) + " dari 500000 huruf gratis.";
  if (azureGalat) t += " Galat terakhir: kode " + String(azureGalat) + ".";
  return t;
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
      pakaiSD = false; sedangBicara = true; audioJalan = true;
      if (!bicaraAzure(s)) {
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
    "{\"mode\":\"jaga\"} diam dan berjaga, {\"mode\":\"jelajah\"} jalan-jalan sendiri keliling ruangan selama 5 menit, "
    "{\"mode\":\"biasa\"} kembali normal (juga menghentikan jelajah); "
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
      if (md == "jelajah") jelajah(5);
      else gantiMode(md == "ikuti" ? M_IKUTI : md == "jaga" ? M_JAGA : M_BIASA);
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
  "/ikuti  ikuti benda di depan\n/jaga  berjaga, kirim foto kalau ada gerakan\n/jelajah [menit]  jalan-jalan sendiri (bawaan 5 menit)\n/biasa  kembali normal\n"
  "/baterai  isi dan perkiraan sisa baterai\n/suara  suara bicara (/suara azure atau /suara google)\n\n"
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
    if (arg == "azure") aturSuara(true); else if (arg == "google") aturSuara(false);
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
  char pb[8] = "";
  if (persenBat >= 0) snprintf(pb, sizeof(pb), "%d%% ", (int)persenBat);
  if (hidup && adaSensorSuhu()) snprintf(atas, sizeof(atas), "%s%.1fV  depan %dcm  %.0fC", pb, badan.baterai, badan.depan, badan.suhu);
  else if (hidup) snprintf(atas, sizeof(atas), "%s%.1fV  depan %dcm", pb, badan.baterai, badan.depan);
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
const char PANEL_HTML[] = R"PANEL(<!doctype html><html lang="id"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1"><title>Panel Zond</title><style>html{color:#e6e9ee;background:#10141a;font-family:system-ui,-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,Noto Sans,Ubuntu,Cantarell,Helvetica Neue,sans-serif}body{margin:0}main.svelte-1xjvnya8h1aa3{max-width:480px;margin:0 auto;padding:12px 16px 32px}header.svelte-1xjvnya8h1aa3{display:flex;flex-wrap:wrap;align-items: center;gap:8px}h1.svelte-1xjvnya8h1aa3{margin:0;font-size:1.4rem}h2.svelte-1xjvnya8h1aa3{margin:0 0 8px;font-size:1rem}h2.svelte-1xjvnya8h1aa3 small:where(.svelte-1xjvnya8h1aa3){color:#8a94a3;font-weight:400}.kecil.svelte-1xjvnya8h1aa3{color:#8a94a3;font-size:.85rem}.titik.svelte-1xjvnya8h1aa3{background:#c0392b;border-radius:50%;width:10px;height:10px}.titik.on.svelte-1xjvnya8h1aa3{background:#2ecc71}.peringatan.svelte-1xjvnya8h1aa3{display:flex;flex-wrap:wrap;gap:6px;margin-top:8px}.peringatan.svelte-1xjvnya8h1aa3 span:where(.svelte-1xjvnya8h1aa3){background:#7a1d1d;border-radius:6px;padding:3px 8px;font-size:.85rem}section.svelte-1xjvnya8h1aa3{background:#18202a;border-radius:12px;margin-top:12px;padding:12px}.kamera.svelte-1xjvnya8h1aa3{text-align:center}.kamera.svelte-1xjvnya8h1aa3 img:where(.svelte-1xjvnya8h1aa3){display:block;aspect-ratio:4/3;object-fit:cover;background:#000;border-radius:8px;width:100%}.kamera.svelte-1xjvnya8h1aa3 img.redup:where(.svelte-1xjvnya8h1aa3){opacity:.15}.ket.svelte-1xjvnya8h1aa3{color:#8a94a3;margin:8px 0}.dpad.svelte-1xjvnya8h1aa3{display:grid;grid-template-columns:repeat(3,1fr);touch-action:none;user-select:none;-webkit-user-select:none;gap:8px;max-width:300px;margin:0 auto 12px}.dpad.svelte-1xjvnya8h1aa3 button:where(.svelte-1xjvnya8h1aa3){aspect-ratio:1;touch-action:none;background:#2a3a4d;border-radius:14px;font-size:1.6rem}.dpad.svelte-1xjvnya8h1aa3 button.aktif:where(.svelte-1xjvnya8h1aa3){background:#3d7bd9}.dpad.svelte-1xjvnya8h1aa3 button.stop:where(.svelte-1xjvnya8h1aa3){background:#a0302a}button.svelte-1xjvnya8h1aa3{color:inherit;cursor:pointer;background:#2a3a4d;border:0;border-radius:10px;padding:10px 12px;font-size:.95rem}button.svelte-1xjvnya8h1aa3:active{filter:brightness(1.3)}.kecil-btn.svelte-1xjvnya8h1aa3{margin-top:8px;padding:6px 10px;font-size:.85rem}.baris.svelte-1xjvnya8h1aa3{display:flex;flex-wrap:wrap;gap:8px}.baris.svelte-1xjvnya8h1aa3 button:where(.svelte-1xjvnya8h1aa3){flex:1}label.svelte-1xjvnya8h1aa3{display:block;margin-top:10px;font-size:.9rem}input[type=range].svelte-1xjvnya8h1aa3{width:100%}form.svelte-1xjvnya8h1aa3{display:flex;gap:8px;margin-top:12px}input[type=text].svelte-1xjvnya8h1aa3{color:inherit;background:#10141a;border:1px solid #2a3a4d;border-radius:10px;flex:1;min-width:0;padding:10px;font-size:1rem}.mik.svelte-1xjvnya8h1aa3{background:#1f6f3a;border-radius:14px;width:100%;padding:16px;font-size:1.1rem;font-weight:600}.mik.dengar.svelte-1xjvnya8h1aa3{animation:1s ease-in-out infinite svelte-1xjvnya8h1aa3-denyut;background:#c2410c}@keyframes svelte-1xjvnya8h1aa3-denyut{50%{filter:brightness(1.3)}}.mik.svelte-1xjvnya8h1aa3:disabled{opacity:.5}.saklar.svelte-1xjvnya8h1aa3{display:flex;color:#8a94a3;align-items: flex-start;gap:8px;font-size:.8rem}.saklar.svelte-1xjvnya8h1aa3 input:where(.svelte-1xjvnya8h1aa3){margin-top:2px}.ai.svelte-1xjvnya8h1aa3{display:grid;grid-template-columns:1fr 1fr;gap:8px}.ai.svelte-1xjvnya8h1aa3 button:where(.svelte-1xjvnya8h1aa3){text-align:left;display:flex;flex-direction:column;gap:2px}.ai.svelte-1xjvnya8h1aa3 button:where(.svelte-1xjvnya8h1aa3):first-child{grid-column:1/-1}.ai.svelte-1xjvnya8h1aa3 button:where(.svelte-1xjvnya8h1aa3) span:where(.svelte-1xjvnya8h1aa3){color:#8a94a3;font-size:.75rem}.ai.svelte-1xjvnya8h1aa3 button.pilih:where(.svelte-1xjvnya8h1aa3){outline:2px solid #5fcf72;background:#2f6a3a}.ai.svelte-1xjvnya8h1aa3 button.pilih:where(.svelte-1xjvnya8h1aa3) span:where(.svelte-1xjvnya8h1aa3){color:#cfe9d4}.ai.svelte-1xjvnya8h1aa3 button:where(.svelte-1xjvnya8h1aa3):disabled{opacity:.45;cursor:default}.kecil-ket.svelte-1xjvnya8h1aa3{margin-bottom:0;font-size:.8rem}.waktu.svelte-1xjvnya8h1aa3{color:#8a94a3;margin:6px 0 0;font-size:.85rem}.belok.svelte-1xjvnya8h1aa3{margin-top:10px}.alarm.svelte-1xjvnya8h1aa3{list-style:none;margin:0 0 8px;padding:0}.alarm.svelte-1xjvnya8h1aa3 li:where(.svelte-1xjvnya8h1aa3){display:flex;border-bottom:1px solid #243041;align-items: center;gap:10px;padding:8px 0}.alarm.svelte-1xjvnya8h1aa3 li:where(.svelte-1xjvnya8h1aa3) b:where(.svelte-1xjvnya8h1aa3){font-size:1.1rem}.alarm.svelte-1xjvnya8h1aa3 li:where(.svelte-1xjvnya8h1aa3) span:where(.svelte-1xjvnya8h1aa3){display:flex;flex-direction:column;flex:1}.alarm.svelte-1xjvnya8h1aa3 li:where(.svelte-1xjvnya8h1aa3) small:where(.svelte-1xjvnya8h1aa3){color:#8a94a3;font-size:.75rem}.hapus.svelte-1xjvnya8h1aa3{background:#3a2430;padding:4px 10px}.form-alarm.svelte-1xjvnya8h1aa3{flex-direction:column;gap:8px}.form-alarm.svelte-1xjvnya8h1aa3 .baris:where(.svelte-1xjvnya8h1aa3)>:where(.svelte-1xjvnya8h1aa3){flex:1}input[type=time].svelte-1xjvnya8h1aa3,select.svelte-1xjvnya8h1aa3{color:inherit;background:#10141a;border:1px solid #2a3a4d;border-radius:10px;min-width:0;padding:9px;font-size:.95rem}.hari.svelte-1xjvnya8h1aa3{display:flex;flex-wrap:wrap;gap:6px 12px;font-size:.85rem}.hari.svelte-1xjvnya8h1aa3 label:where(.svelte-1xjvnya8h1aa3){display:flex;align-items: center;gap:4px;margin:0}.jelajah.svelte-1xjvnya8h1aa3{margin-top:12px}.jelajah.svelte-1xjvnya8h1aa3 select:where(.svelte-1xjvnya8h1aa3){flex:none}.aktif-jelajah.svelte-1xjvnya8h1aa3{background:#3d7bd9}.grafik.svelte-1xjvnya8h1aa3{display:block;background:#10141a;border-radius:8px;width:100%;height:auto}.grafik.svelte-1xjvnya8h1aa3 line:where(.svelte-1xjvnya8h1aa3){stroke:#243041;stroke-width:1}.grafik.svelte-1xjvnya8h1aa3 text:where(.svelte-1xjvnya8h1aa3){fill:#8a94a3;font-size:9px}.grafik.svelte-1xjvnya8h1aa3 path:where(.svelte-1xjvnya8h1aa3){fill:none;stroke:#5fcf72;stroke-width:2;stroke-linejoin:round}.sumbu.svelte-1xjvnya8h1aa3{display:flex;color:#8a94a3;justify-content:space-between;margin-top:4px;font-size:.75rem}.suara.svelte-1xjvnya8h1aa3{display:flex;align-items: center;gap:8px}.suara.svelte-1xjvnya8h1aa3 select:where(.svelte-1xjvnya8h1aa3){flex:1}.status.svelte-1xjvnya8h1aa3{text-align:center;color:#8a94a3;margin-top:12px}
</style></head><body><script type="module">var p=!1;var vt=Array.isArray,qa=Array.prototype.indexOf,Mt=Array.prototype.includes,Nr=Array.from,{keys:li,defineProperty:me,getOwnPropertyDescriptor:ze,getOwnPropertyDescriptors:fi,prototype:ci}=Object,Ga=Array.prototype,{getPrototypeOf:Rr,isExtensible:ui}=Object;var Et=()=>{};function In(e){for(var t=0;t<e.length;t++)e[t]()}function Dn(){var e,t,r=new Promise((n,o)=>{e=n,t=o});return{promise:r,resolve:e,reject:t}}function pi(e,t){if(Array.isArray(e))return e;if(t===void 0||!(Symbol.iterator in e))return Array.from(e);let r=[];for(let n of e)if(r.push(n),r.length===t)break;return r}var te=2,kt=4,zt=8,cr=16777216,Se=16,ge=32,He=64,Cr=128,Mn=256,Fe=512,q=1024,P=2048,oe=4096,fe=8192,W=16384,Pe=32768,Lt=33554432,Ve=65536,Kt=131072,di=262144,Tt=524288,za=1048576,ht=33554432,jr=2097152,Ke=4194304,nt=8388608,We=Symbol("$state"),Ln=Symbol("component"),Fn=Symbol("legacy props"),Ka=Symbol(""),Pn=Symbol("proxy path"),Bn=Symbol("attributes"),an=Symbol("class"),_i=Symbol("style"),sn=Symbol("text"),Wt=Symbol("form reset"),vi=Symbol("hmr anchor"),Ft=new class extends Error{name="StaleReactionError";message="The reaction that called `getAbortSignal()` was re-run or destroyed"},hi=!!globalThis.document?.contentType&&globalThis.document.contentType.includes("xml");var ur=3,ot=8;var Wa=1,Za=2,mi=4,Xa=8,Ja=16;var gi=1,Qa=2;var Or="[",Zt="[!",xi="[?",pr="]",Ze={};var Z=Symbol("uninitialized"),it=Symbol("filename"),bi=Symbol("hmr"),Yn="http://www.w3.org/1999/xhtml";var Pt="font-weight: bold",Bt="font-weight: normal";function es(e){if(p)console.warn(`%c[svelte] await_reactivity_loss
%cDetected reactivity loss when reading \`${e}\`. This happens when state is read in an async function after an earlier \`await\`
https://svelte.dev/e/await_reactivity_loss`,Pt,Bt);else console.warn("https://svelte.dev/e/await_reactivity_loss")}function ts(e,t){if(p)console.warn(`%c[svelte] await_waterfall
%cAn async derived, \`${e}\` (${t}) was not read immediately after it resolved. This often indicates an unnecessary waterfall, which can slow down your app
https://svelte.dev/e/await_waterfall`,Pt,Bt);else console.warn("https://svelte.dev/e/await_waterfall")}function rs(){if(p)console.warn(`%c[svelte] derived_inert
%cReading a derived belonging to a now-destroyed effect may result in stale values
https://svelte.dev/e/derived_inert`,Pt,Bt);else console.warn("https://svelte.dev/e/derived_inert")}function ns(e,t,r){if(p)console.warn(`%c[svelte] hydration_attribute_changed
%cThe \`${e}\` attribute on \`${t}\` changed its value between server and client renders. The client value, \`${r}\`, will be ignored in favour of the server value
https://svelte.dev/e/hydration_attribute_changed`,Pt,Bt);else console.warn("https://svelte.dev/e/hydration_attribute_changed")}function dr(e){if(p)console.warn(`%c[svelte] hydration_mismatch
%c${e?`Hydration failed because the initial UI does not match what was rendered on the server. The error occurred near ${e}`:"Hydration failed because the initial UI does not match what was rendered on the server"}
https://svelte.dev/e/hydration_mismatch`,Pt,Bt);else console.warn("https://svelte.dev/e/hydration_mismatch")}function os(){if(p)console.warn(`%c[svelte] lifecycle_double_unmount
%cTried to unmount a component that was not mounted
https://svelte.dev/e/lifecycle_double_unmount`,Pt,Bt);else console.warn("https://svelte.dev/e/lifecycle_double_unmount")}function is(){if(p)console.warn("%c[svelte] select_multiple_invalid_value\n%cThe `value` property of a `<select multiple>` element should be an array, but it received a non-array value. The selection will be kept as is.\nhttps://svelte.dev/e/select_multiple_invalid_value",Pt,Bt);else console.warn("https://svelte.dev/e/select_multiple_invalid_value")}function Hn(e){if(p)console.warn(`%c[svelte] state_proxy_equality_mismatch
%cReactive \`$state(...)\` proxies and the values they proxy have different identities. Because of this, comparisons with \`${e}\` will produce unexpected results
https://svelte.dev/e/state_proxy_equality_mismatch`,Pt,Bt);else console.warn("https://svelte.dev/e/state_proxy_equality_mismatch")}function as(){if(p)console.warn("%c[svelte] svelte_boundary_reset_noop\n%cA `<svelte:boundary>` `reset` function only resets the boundary the first time it is called\nhttps://svelte.dev/e/svelte_boundary_reset_noop",Pt,Bt);else console.warn("https://svelte.dev/e/svelte_boundary_reset_noop")}var w=!1;function pe(e){w=e}var C;function U(e){if(e===null)throw dr(),Ze;return C=e}function Oe(){return U(xe(C))}function A(e){if(!w)return;if(xe(C)!==null)throw dr(),Ze;C=e}function at(e=1){if(w){var t=e,r=C;while(t--)r=xe(r);C=r}}function At(e=!0){var t=0,r=C;while(!0){if(r.nodeType===ot){var n=r.data;if(n===pr){if(t===0)return r;t-=1}else if(n===Or||n===Zt||n[0]==="["&&!isNaN(Number(n.slice(1))))t+=1}var o=xe(r);if(e)r.remove();r=o}}function _r(e){if(!e||e.nodeType!==ot)throw dr(),Ze;return e.data}function Vn(e){return e===this.v}function yi(e,t){return e!=e?t==t:e!==t||e!==null&&typeof e==="object"||typeof e==="function"}function Un(e){return!yi(e,this.v)}function ss(e){if(p){let t=Error(`invariant_violation
An invariant violation occurred, meaning Svelte's internal assumptions were flawed. This is a bug in Svelte, not your app — please open an issue at https://github.com/sveltejs/svelte, citing the following message: "${e}"
https://svelte.dev/e/invariant_violation`);throw t.name="Svelte error",t}else throw Error("https://svelte.dev/e/invariant_violation")}function qn(e){if(p){let t=Error(`lifecycle_outside_component
\`${e}(...)\` can only be used during component initialisation
https://svelte.dev/e/lifecycle_outside_component`);throw t.name="Svelte error",t}else throw Error("https://svelte.dev/e/lifecycle_outside_component")}function fs(){if(p){let e=Error("async_derived_orphan\nCannot create a `$derived(...)` with an `await` expression outside of an effect tree\nhttps://svelte.dev/e/async_derived_orphan");throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/async_derived_orphan")}function $i(){if(p){let e=Error("bind_invalid_checkbox_value\nUsing `bind:value` together with a checkbox input is not allowed. Use `bind:checked` instead\nhttps://svelte.dev/e/bind_invalid_checkbox_value");throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/bind_invalid_checkbox_value")}function cs(){if(p){let e=Error(`derived_references_self
A derived value cannot reference itself recursively
https://svelte.dev/e/derived_references_self`);throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/derived_references_self")}function wi(e,t,r){if(p){let n=Error(`each_key_duplicate
${r?`Keyed each block has duplicate key \`${r}\` at indexes ${e} and ${t}`:`Keyed each block has duplicate key at indexes ${e} and ${t}`}
https://svelte.dev/e/each_key_duplicate`);throw n.name="Svelte error",n}else throw Error("https://svelte.dev/e/each_key_duplicate")}function us(e,t,r){if(p){let n=Error(`each_key_volatile
Keyed each block has key that is not idempotent — the key for item at index ${e} was \`${t}\` but is now \`${r}\`. Keys must be the same each time for a given item
https://svelte.dev/e/each_key_volatile`);throw n.name="Svelte error",n}else throw Error("https://svelte.dev/e/each_key_volatile")}function ps(e){if(p){let t=Error(`effect_in_teardown
\`${e}\` cannot be used inside an effect cleanup function
https://svelte.dev/e/effect_in_teardown`);throw t.name="Svelte error",t}else throw Error("https://svelte.dev/e/effect_in_teardown")}function ds(){if(p){let e=Error("effect_in_unowned_derived\nEffect cannot be created inside a `$derived` value that was not itself created inside an effect\nhttps://svelte.dev/e/effect_in_unowned_derived");throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/effect_in_unowned_derived")}function _s(e){if(p){let t=Error(`effect_orphan
\`${e}\` can only be used inside an effect (e.g. during component initialisation)
https://svelte.dev/e/effect_orphan`);throw t.name="Svelte error",t}else throw Error("https://svelte.dev/e/effect_orphan")}function vs(){if(p){let e=Error(`effect_update_depth_exceeded
Maximum update depth exceeded. This typically indicates that an effect reads and writes the same piece of state
https://svelte.dev/e/effect_update_depth_exceeded`);throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/effect_update_depth_exceeded")}function hs(){if(p){let e=Error(`hydration_failed
Failed to hydrate the application
https://svelte.dev/e/hydration_failed`);throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/hydration_failed")}function ms(e){if(p){let t=Error(`rune_outside_svelte
The \`${e}\` rune is only available inside \`.svelte\` and \`.svelte.js/ts\` files
https://svelte.dev/e/rune_outside_svelte`);throw t.name="Svelte error",t}else throw Error("https://svelte.dev/e/rune_outside_svelte")}function gs(){if(p){let e=Error("state_descriptors_fixed\nProperty descriptors defined on `$state` objects must contain `value` and always be `enumerable`, `configurable` and `writable`.\nhttps://svelte.dev/e/state_descriptors_fixed");throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/state_descriptors_fixed")}function xs(){if(p){let e=Error("state_prototype_fixed\nCannot set prototype of `$state` object\nhttps://svelte.dev/e/state_prototype_fixed");throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/state_prototype_fixed")}function bs(){if(p){let e=Error("state_unsafe_mutation\nUpdating state inside `$derived(...)`, `$inspect(...)` or a template expression is forbidden. If the value should not be reactive, declare it without `$state`\nhttps://svelte.dev/e/state_unsafe_mutation");throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/state_unsafe_mutation")}function ys(){if(p){let e=Error("svelte_boundary_reset_onerror\nA `<svelte:boundary>` `reset` function cannot be called while an error is still being handled\nhttps://svelte.dev/e/svelte_boundary_reset_onerror");throw e.name="Svelte error",e}else throw Error("https://svelte.dev/e/svelte_boundary_reset_onerror")}var ce=!1,Xt=!1,mt=!1;var ln=null;function Ie(e,t){return e.label=t,zn(e.v,t),e}function zn(e,t){return e?.[Pn]?.(t),e}function gt(e){let t=Error(),r=jf();if(r.length===0)return null;return r.unshift(`
`),me(t,"stack",{value:r.join(`
`)}),me(t,"name",{value:e}),t}function jf(){let e=Error.stackTraceLimit;Error.stackTraceLimit=1/0;let t=Error().stack;if(Error.stackTraceLimit=e,!t)return[];let r=t.split(`
`),n=[];for(let o=0;o<r.length;o++){let i=r[o],a=i.replaceAll("\\","/");if(i.trim()==="Error")continue;if(i.includes("validate_each_keys"))return[];if(a.includes("svelte/src/internal")||a.includes("node_modules/.vite"))continue;n.push(i)}return n}function ws(e,t){if(!p)throw Error("invariant(...) was not guarded by if (DEV)");if(!e)ss(t)}var M=null;function Yt(e){M=e}var St=null;function Ir(e){St=e}var xt=null;function Kn(e){xt=e}function fn(e,t=!1,r){if(M={p:M,i:!1,c:null,e:null,s:e,x:null,r:k,l:Xt&&!t?{s:null,u:null,$:[]}:null},p)M.function=r,xt=r}function cn(e){var t=M,r=t.e;if(r!==null){t.e=null;for(var n of r)Ei(n)}if(e!==void 0)t.x=e;if(t.i=!0,M=t.p,p)xt=M?.function??null;return Wn(e)}function Wn(e={}){return me(e,Ln,{value:!0}),e}function Nt(){return!Xt||M!==null&&M.l===null}var vr=[];function Es(){var e=vr;vr=[],In(e)}function ue(e){if(vr.length===0&&!hr){var t=vr;queueMicrotask(()=>{if(t===vr)Es()})}vr.push(e)}function ks(){while(vr.length>0)Es()}var Of=~(P|oe|q);function G(e,t){e.f=e.f&Of|t}function Dr(e){if((e.f&Fe)!==0||e.deps===null)G(e,q);else G(e,oe)}function Zn(e,t){if((e.f&P)!==0)t.set(e,P);else if((e.f&oe)!==0&&t.get(e)!==P)t.set(e,oe);G(e,q)}function Ts(e){return`{${e.toString().replace(/^async \(\) => /,"").replace(/^\(/,"").replace(/\$\.get\((.+?)\)/g,(t,r)=>r).replace(/\$\.track_reactivity_loss\((.+?)\)\(\)/g,(t,r)=>r).replace(/\)$/,"")}}`}var As=!1;function Xn(){if(!As)As=!0,document.addEventListener("reset",(e)=>{Promise.resolve().then(()=>{if(!e.defaultPrevented)for(let t of e.target.elements)t[Wt]?.()})},{capture:!0})}function Xe(e){var t=S,r=k;de(null),_e(null);try{return e()}finally{de(t),_e(r)}}function pn(e,t,r,n=r){e.addEventListener(t,()=>Xe(r));let o=e[Wt];if(o)e[Wt]=()=>{o(),n(!0)};else e[Wt]=()=>n(!0);Xn()}function Jn(e,t,r,n){let o=Nt()?Lr:Fr;var i=e.filter((d)=>!d.settled),a=t.map(o);if(p)a.forEach((d,y)=>{d.label=t[y].toString().replace("() => ","").replaceAll("$.eager(() => ","$state.eager(").replace(/\$\.get\((.+?)\)/g,(R,b)=>b)});if(r.length===0&&i.length===0){n(a);return}var f=k,c=Ss(),m=i.length===1?i[0].promise:i.length>1?Promise.all(i.map((d)=>d.promise)):null;function h(d){if((f.f&W)!==0)return;c();try{n([...a,...d])}catch(y){Ue(y,f)}Mr()}var v=ki();if(r.length===0){m.then(()=>h([])).finally(v);return}function _(){Promise.all(r.map((d)=>Ai(d))).then(h).catch((d)=>Ue(d,f)).finally(v)}if(m)m.then(()=>{if((f.f&W)!==0){v();return}c(),_(),Mr()});else _()}function Ss(){var e=k,t=S,r=M,n=T;if(p)var o=St;return function(a=!0){if(_e(e),de(t),Yt(r),a&&(e.f&W)===0)n?.activate(),n?.apply();if(p)Ti(null),Ir(o)}}var If=!1;function Mr(e=!0){if(If=!1,_e(null),de(null),Yt(null),e)T?.deactivate();if(p)Ti(null),Ir(null)}function ki(){var e=k,t=e.b,r=T,n=!!t?.is_rendered();return t?.update_pending_count(1,r),r.increment(n,e),()=>{t?.update_pending_count(-1,r),r.decrement(n,e)}}var qe=null;function Ti(e){qe=e}var dn=new Set;function Lr(e){var t=te|P;if(k!==null)k.f|=Tt;let r={ctx:M,deps:null,effects:null,equals:Vn,f:t,fn:e,reactions:null,rv:0,v:Z,wv:0,parent:k,ac:null};if(p&&mt)r.created=gt("created at");return r}var Pr=Symbol("obsolete");function Ai(e,t,r){let n=k;if(n===null)fs();var o=void 0,i=De(Z);if(p)i.label=t??Ts(e);var a=!S,f=new Set;if(Cs(()=>{var c=k;if(p)qe={effect:c,effect_deps:new Set,warned:!1};var m=Dn();o=m.promise;try{Promise.resolve(e()).then(m.resolve,(d)=>{if(d!==Ft)m.reject(d)}).finally(Mr)}catch(d){m.reject(d),Mr()}if(p){if(qe){if(c.deps!==null)for(let d=0;d<Be;d+=1)qe.effect_deps.add(c.deps[d]);if(ye!==null)for(let d=0;d<ye.length;d+=1)qe.effect_deps.add(ye[d])}qe=null}var h=T;if(a){if((c.f&Pe)!==0)var v=ki();if(n.b?.is_rendered())h.async_deriveds.get(c)?.reject(Pr);else for(let d of f.values())d.reject(Pr);f.add(m),h.async_deriveds.set(c,m)}let _=(d,y=void 0)=>{if(p)qe=null;if(v?.(),f.delete(m),y===Pr)return;if(h.activate(),y)i.f|=nt,bt(i,y);else{if((i.f&nt)!==0)i.f^=nt;if(p&&r!==void 0&&!i.equals(d))dn.add(i),setTimeout(()=>{if(dn.has(i)&&(c.f&W)===0)ts(i.label,r),dn.delete(i)});bt(i,d)}h.deactivate()};m.promise.then(_,(d)=>_(null,d||"unknown"))}),be(()=>{for(let c of f)c.reject(Pr)}),p)i.f|=Ke;return new Promise((c)=>{function m(h){function v(){if(h===o)c(i);else m(o)}h.then(v,v)}m(o)})}function to(e){let t=Lr(e);if(!ce)ro(t);return t}function Fr(e){let t=Lr(e);return t.equals=Un,t}function Ns(e){var t=e.effects;if(t!==null){e.effects=null;for(var r=0;r<t.length;r+=1)ae(t[r])}}var Si=[];function _n(e){var t,r=k,n=e.parent;if(!st&&n!==null&&e.v!==Z&&(n.f&(W|fe))!==0)return rs(),e.v;if(_e(n),p){let o=mr;Qn(new Set);try{if(Mt.call(Si,e))cs();Si.push(e),Ns(e),t=eo(e)}finally{_e(r),Qn(o),Si.pop()}}else try{Ns(e),t=eo(e)}finally{_e(r)}return t}function Ni(e){var t=_n(e);if(!e.equals(t)){if(e.wv=Br(),!T?.is_fork||e.deps===null){if(T!==null)T.capture(e,t,!0),Je?.capture(e,t,!0);else e.v=t;if(e.deps===null){G(e,q);return}}}if(st)return;if($e!==null){if(Jt()||T?.is_fork)$e.set(e,t)}else Dr(e)}function Rs(e){if(e.effects===null)return;for(let t of e.effects)if(t.teardown||t.ac){if(t.teardown?.(),t.ac!==null)Xe(()=>{t.ac.abort(Ft),t.ac=null});if(t.fn!==null)t.teardown=Et;gr(t,0),vn(t)}}function Ri(e){if(e.effects===null)return;for(let t of e.effects)if(t.teardown&&t.fn!==null)Rt(t)}var no=null,Yr=null,T=null,Je=null,$e=null,Oi=null,hr=!1,Ci=!1,Hr=null,hn=null,js=0,ji=new Set,Lf=1;class Ce{id=Lf++;started=!1;linked=!0;prev=null;next=null;async_deriveds=new Map;current=new Map;previous=new Map;#e=new Set;#t=new Set;#r=0;#s=new Map;#o=null;#n=[];#a=[];#i=new Map;#l=new Map;#d=new Set;is_fork=!1;#f=!1;constructor(){if(Yr===null)no=Yr=this;else Yr.next=this,this.prev=Yr;Yr=this}#v(){if(this.is_fork)return!0;for(let e of this.#s.keys())if(!this.#g(e))return!0;return!1}#g(e){for(var t=e;t!==null;t=t.parent)if(this.#l.has(t))return!0;return!1}skip_effect(e){if(!this.#l.has(e))this.#l.set(e,{d:[],m:[]});this.#d.delete(e)}unskip_effect(e,t=(r)=>this.schedule(r)){var r=this.#l.get(e);if(r){this.#l.delete(e);for(var n of r.d)G(n,P),t(n);for(n of r.m)G(n,oe),t(n)}this.#d.add(e)}#c(){var e=[];for(let o of this.#n){if((o.f&W)!==0||(o.f&(P|oe))===0)continue;var t=o,r=!1;while(t.parent!==null){t=t.parent;var n=t.f;if((n&(He|ge))!==0){if((n&q)===0){r=!0;break}t.f^=q}}if(!r)e.push(t)}return this.#n=[],e}#h(){if(this.started=!0,p)for(let a of this.current.keys())ji.add(a);for(let[a,f]of this.#i)G(a,f),this.schedule(a);this.apply();var e=Hr=[],t=[],r=hn=[];while(this.#n.length>0){if(js++>1000)this.#p(),Ff();for(let a of this.#c())try{this.#x(a,e,t)}catch(f){if(Ms(a),!this.#v())this.discard();throw f}}if(T=null,r.length>0){var n=Ce.ensure();for(let a of r)n.schedule(a)}if(Hr=null,hn=null,this.#v()){this.#_(t),this.#_(e);for(let[a,f]of this.#l)Ds(a,f);if(r.length>0)T.#h();return}let o=this.#y();if(o){this.#_(t),this.#_(e),o.#u(this);return}this.#i.clear();for(let a of this.#e)a(this);this.#e.clear(),Je=this,Os(t),Os(e),Je=null,this.#o?.resolve();var i=T;if(this.#r===0&&(this.#n.length===0||i!==null)){if(this.#p(),ce)this.#b(),T=i}if(i!==null)ft.clear(),i.#h()}#x(e,t,r){e.f^=q;var n=e.first;while(n!==null){var o=n.f,i=(o&(ge|He))!==0,a=i&&(o&q)!==0,f=a||(o&fe)!==0||this.#l.has(n);if(!f&&n.fn!==null){if(i)n.f^=q;else if((o&kt)!==0)t.push(n);else if(ce&&(o&(zt|cr))!==0)r.push(n);else if(er(n)){if((o&Se)!==0)this.add_dirty_reaction(n,oe);Rt(n)}var c=n.first;if(c!==null){n=c;continue}}while(n!==null){var m=n.next;if(m!==null){n=m;break}n=n.parent}}}#y(){for(var e=this.prev;e!==null;e=e.prev)if(!e.is_fork&&this.#m(e))return e;return null}#m(e){for(let[t,[,r]]of this.current){if(r)continue;if(e.current.has(t))return!0}return!1}#u(e){for(let[r,n]of e.current){if(!this.previous.has(r)&&e.previous.has(r))this.previous.set(r,e.previous.get(r));this.current.set(r,n)}for(let[r,n]of e.async_deriveds){let o=this.async_deriveds.get(r);if(o)n.promise.then(o.resolve).catch(o.reject)}e.async_deriveds.clear(),this.transfer_reactions(e.#i);let t=(r)=>{var n=r.reactions;if(n===null)return;if((r.f&te)!==0&&(r.f&(P|oe))===0)return;for(let a of n){var o=a.f;if((o&te)!==0)t(a);else{var i=a;if(o&(Ke|Se)&&!this.async_deriveds.has(i))this.#i.delete(i),G(i,P),this.schedule(i)}}};for(let r of this.current.keys())t(r);this.oncommit(()=>e.discard()),e.#p(),T=this,this.#h()}#_(e){for(var t=0;t<e.length;t+=1)Zn(e[t],this.#i)}capture(e,t,r=!1){if(e.v!==Z&&!this.previous.has(e))this.previous.set(e,e.v);if((e.f&nt)===0)this.current.set(e,[t,r]),$e?.set(e,t);if(!this.is_fork)e.v=t}activate(){T=this}deactivate(){T=null,$e=null}flush(){try{if(p)ji.clear();Ci=!0,T=this,this.#h()}finally{if(js=0,Oi=null,Hr=null,hn=null,Ci=!1,T=null,$e=null,ft.clear(),p)for(let e of ji)e.updated=null}}discard(){for(let e of this.#t)e(this);this.#t.clear();for(let e of this.async_deriveds.values())e.reject(Pr);this.#p(),this.#o?.resolve()}register_created_effect(e){this.#a.push(e)}#b(){for(let h=no;h!==null;h=h.next){var e=h.id<this.id,t=[];for(let[v,[_,d]]of this.current){if(h.current.has(v)){var r=h.current.get(v)[0];if(e&&_!==r)h.current.set(v,[_,d]);else continue}t.push(v)}if(e)for(let[v,_]of this.async_deriveds){let d=h.async_deriveds.get(v);if(d)_.promise.then(d.resolve).catch(d.reject)}var n=[...h.current.keys()].filter((v)=>!h.current.get(v)[1]);if(!h.started||n.length===0)continue;var o=n.filter((v)=>!this.current.has(v));if(o.length===0){if(e)h.discard()}else if(t.length>0){if(p&&!h.#f)ws(h.#n.length===0,"Batch has scheduled effects");if(e)for(let v of this.#d)h.unskip_effect(v,(_)=>{if((_.f&(Se|Ke))!==0)h.schedule(_);else h.#_([_])});h.activate();var i=new Set,a=new Map;for(var f of t)Is(f,o,i,a);a=new Map;var c=[...h.current].filter(([v,_])=>{let d=this.current.get(v);if(!d)return!0;return d[0]!==_[0]||d[1]!==_[1]}).map(([v])=>v);if(c.length>0){for(let v of this.#a)if((v.f&(W|fe|Kt))===0&&Ii(v,c,a))if((v.f&(Ke|Se))!==0)G(v,P),h.schedule(v);else h.add_dirty_reaction(v,P)}if(h.#n.length>0&&!h.#f){h.apply();for(var m of h.#c())h.#x(m,[],[])}h.deactivate()}}}increment(e,t){if(this.#r+=1,e){let r=this.#s.get(t)??0;this.#s.set(t,r+1)}}decrement(e,t){if(this.#r-=1,e){let r=this.#s.get(t)??0;if(r===1)this.#s.delete(t);else this.#s.set(t,r-1)}if(this.#f)return;this.#f=!0,ue(()=>{if(this.#f=!1,this.linked)this.flush()})}transfer_reactions(e){for(let[t,r]of e)this.add_dirty_reaction(t,r);e.clear()}add_dirty_reaction(e,t){if(t===P||this.#i.get(e)!==P)this.#i.set(e,t)}oncommit(e){this.#e.add(e)}ondiscard(e){this.#t.add(e)}settled(){return(this.#o??=Dn()).promise}static ensure(){if(T===null){let e=T=new Ce;if(!Ci&&!hr)ue(()=>{if(!e.started)e.flush()})}return T}apply(){if(!ce||!this.is_fork&&this.prev===null&&this.next===null){$e=null;return}$e=new Map;for(let[e,[t]]of this.current)$e.set(e,t);for(let e=no;e!==null;e=e.next){if(e===this||e.is_fork)continue;if(e.id>=this.id||!e.#m(this)){for(let[t,r]of e.previous)if(!$e.has(t))$e.set(t,r)}}}schedule(e){if(Oi=e,e.b?.is_pending&&(e.f&(kt|zt|cr))!==0&&(e.f&Pe)===0){e.b.defer_effect(e);return}this.#n.push(e)}#p(){if(!this.linked)return;var e=this.prev,t=this.next;if(e===null)no=t;else e.next=t;if(t===null)Yr=e;else t.prev=e;this.linked=!1}}function Qt(e){var t=hr,r=Je;Je=null,hr=!0;try{var n;if(e)Qt(),n=e();while(!0){if(ks(),T===null)return n;T.flush()}}finally{hr=t,Je=r}}function Ff(){if(p){var e=new Map;for(let r of T.current.keys())for(let[n,o]of r.updated??[]){var t=e.get(n);if(!t)t={error:o.error,count:0},e.set(n,t);t.count+=o.count}for(let r of e.values())if(r.error)console.error(r.error)}try{vs()}catch(r){if(p)me(r,"stack",{value:""});Ue(r,Oi)}}var lt=null;function Os(e){var t=e.length;if(t===0)return;var r=0;while(r<t){var n=e[r++];if((n.f&(W|fe))===0&&er(n)){if(lt=new Set,Rt(n),n.deps===null&&n.first===null&&n.nodes===null&&n.teardown===null&&n.ac===null)Di(n);if(lt?.size>0){ft.clear();for(let o of lt){if((o.f&(W|fe))!==0)continue;let i=[o],a=o.parent;while(a!==null){if(lt.has(a))lt.delete(a),i.push(a);a=a.parent}for(let f=i.length-1;f>=0;f--){let c=i[f];if((c.f&(W|fe))!==0)continue;Rt(c)}}lt.clear()}}}lt=null}function Is(e,t,r,n){if(r.has(e))return;if(r.add(e),e.reactions!==null)for(let o of e.reactions){let i=o.f;if((i&te)!==0)Is(o,t,r,n);else if((i&(Ke|Se))!==0&&(i&P)===0&&Ii(o,t,n))G(o,P),mn(o)}}function Ii(e,t,r){let n=r.get(e);if(n!==void 0)return n;if(e.deps!==null)for(let o of e.deps){if(Mt.call(t,o))return!0;if((o.f&te)!==0&&Ii(o,t,r))return r.set(o,!0),!0}return r.set(e,!1),!1}function mn(e){T.schedule(e)}function Ds(e,t){if((e.f&ge)!==0&&(e.f&q)!==0)return;if((e.f&P)!==0)t.d.push(e);else if((e.f&oe)!==0)t.m.push(e);G(e,q);var r=e.first;while(r!==null)Ds(r,t),r=r.next}function Ms(e){G(e,q);var t=e.first;while(t!==null)Ms(t),t=t.next}var mr=new Set,ft=new Map;function Qn(e){mr=e}var Li=!1;function Fs(){Li=!0}function De(e,t){var r={f:0,v:e,reactions:null,equals:Vn,rv:0,wv:0};if(p&&mt)r.created=t??gt("created at"),r.updated=null,r.set_during_effect=!1,r.trace=null;return r}function B(e,t){let r=De(e,t);return ro(r),r}function br(e,t=!1,r=!0){let n=De(e);if(!t)n.equals=Un;if(Xt&&r&&M!==null&&M.l!==null)(M.l.s??=[]).push(n);return n}function N(e,t,r=!1){if(S!==null&&(!Ye||(S.f&Kt)!==0)&&Nt()&&(S.f&(te|Se|Ke|Kt))!==0&&(yt===null||!yt.has(e)))bs();let n=r?ct(t):t;if(p)zn(n,e.label);return bt(e,n,hn)}var xr=null,Mi=0;function bt(e,t,r=null){if(!e.equals(t)){if(st)ft.set(e,t);else if(!ft.has(e))ft.set(e,e.v);var n=Ce.ensure();if(n.capture(e,t),p){if(mt||k!==null){e.updated??=new Map;let o=(e.updated.get("")?.count??0)+1;if(e.updated.set("",{error:null,count:o}),mt||o>5){let i=gt("updated at");if(i!==null){let a=e.updated.get(i.stack);if(!a)a={error:i,count:0},e.updated.set(i.stack,a);a.count++}}}if(k!==null)e.set_during_effect=!0}if((e.f&te)!==0){let o=e;if((e.f&P)!==0)_n(o);if($e===null)Dr(o)}if(e.wv=Br(),xr=null,Mi=0,Ps(e,P,r),xr=null,Nt()&&k!==null&&(k.f&q)!==0&&(k.f&(ge|He))===0)if(Qe===null)Bs([e]);else Qe.push(e);if(!n.is_fork&&mr.size>0&&!Li)oo()}return t}function oo(){Li=!1;for(let e of mr){if((e.f&q)!==0)G(e,oe);let t;try{t=er(e)}catch{t=!0}if(t)Rt(e)}mr.clear()}function yr(e){N(e,e.v+1)}function Ps(e,t,r){var n=e.reactions;if(n===null)return;var o=Nt(),i=n.length;if(Mi+=i,Mi>1e5&&xr===null)xr=new Set;if(xr!==null){if(xr.has(e))return;xr.add(e)}for(var a=0;a<i;a++){var f=n[a],c=f.f;if(!o&&f===k)continue;var m=(c&P)===0;if(m)G(f,t);if((c&Kt)!==0)mr.add(f);else if((c&te)!==0){var h=f;$e?.delete(h),Ps(h,oe,r)}else if(m){var v=f;if((c&Se)!==0&&lt!==null)lt.add(v);if(r!==null)r.push(v);else mn(v)}}}var Bf=/^[a-zA-Z_$][a-zA-Z_$0-9]*$/;function ct(e){if(typeof e!=="object"||e===null||We in e||Ln in e)return e;let t=Rr(e);if(t!==ci&&t!==Ga)return e;var r=new Map,n=vt(e),o=B(0),i=p&&mt?gt("created at"):null,a=tr,f=(v)=>{if(tr===a)return v();var _=S,d=tr;de(null),Fi(a);var y=v();return de(_),Fi(d),y};if(n){if(r.set("length",B(e.length,i)),p)e=Hf(e)}var c="";let m=!1;function h(v){if(m)return;m=!0,c=v,Ie(o,`${c} version`);for(let[_,d]of r)Ie(d,$r(c,_));m=!1}return new Proxy(e,{defineProperty(v,_,d){if(!("value"in d)||d.configurable===!1||d.enumerable===!1||d.writable===!1)gs();var y=r.get(_);if(y===void 0)f(()=>{var R=B(d.value,i);if(r.set(_,R),p&&typeof _==="string")Ie(R,$r(c,_));return R});else N(y,d.value,!0);return!0},deleteProperty(v,_){var d=r.get(_);if(d===void 0){if(_ in v){let y=f(()=>B(Z,i));if(r.set(_,y),yr(o),p)Ie(y,$r(c,_))}}else N(d,Z),yr(o);return!0},get(v,_,d){if(_===We)return e;if(p&&_===Pn)return h;var y=r.get(_),R=_ in v;if(y===void 0&&(!R||ze(v,_)?.writable))y=f(()=>{var j=ct(R?v[_]:Z),Y=B(j,i);if(p)Ie(Y,$r(c,_));return Y}),r.set(_,y);if(y!==void 0){var b=l(y);return b===Z?void 0:b}return Reflect.get(v,_,d)},getOwnPropertyDescriptor(v,_){this.has?.(v,_);var d=Reflect.getOwnPropertyDescriptor(v,_),y=r.get(_);if(y!==void 0){var R=l(y);if(R===Z)return;if(d&&"value"in d)d.value=R;else return{enumerable:!0,configurable:!0,value:R,writable:!0}}return d},has(v,_){if(_===We)return!0;var d=r.get(_),y=d!==void 0&&d.v!==Z||Reflect.has(v,_);if(d!==void 0||(k!==null||S!==null)&&(!y||ze(v,_)?.writable)){if(d===void 0)d=f(()=>{var b=y?ct(v[_]):Z,j=B(b,i);if(p)Ie(j,$r(c,_));return j}),r.set(_,d);var R=l(d);if(R===Z)return!1}return y},set(v,_,d,y){var R=r.get(_),b=_ in v;if(n&&_==="length")for(var j=d;j<R.v;j+=1){var Y=r.get(j+"");if(Y!==void 0)N(Y,Z);else if(j in v){if(Y=f(()=>B(Z,i)),r.set(j+"",Y),p)Ie(Y,$r(c,j))}}if(R===void 0){if(!b||ze(v,_)?.writable){if(R=f(()=>B(void 0,i)),p)Ie(R,$r(c,_));N(R,ct(d)),r.set(_,R)}}else{b=R.v!==Z;var Q=f(()=>ct(d));N(R,Q)}var le=Reflect.getOwnPropertyDescriptor(v,_);if(le?.set)le.set.call(y,d);if(!b){if(n&&typeof _==="string"){var pt=r.get("length"),Ee=Number(_);if(Number.isInteger(Ee)&&Ee>=pt.v)N(pt,Ee+1)}yr(o)}return!0},ownKeys(v){l(o);var _=Reflect.ownKeys(v).filter((R)=>{var b=r.get(R);return b===void 0||b.v!==Z});for(var[d,y]of r)if(y.v!==Z&&!(d in v))_.push(d);return _},setPrototypeOf(){xs()}})}function $r(e,t){if(typeof t==="symbol")return`${e}[Symbol(${t.description??""})]`;if(Bf.test(t))return`${e}.${t}`;return/^\d+$/.test(t)?`${e}[${t}]`:`${e}['${t}']`}function Vr(e){try{if(e!==null&&typeof e==="object"&&We in e)return e[We]}catch{}return e}function io(e,t){return Object.is(Vr(e),Vr(t))}var Yf=new Set(["copyWithin","fill","pop","push","reverse","shift","sort","splice","unshift"]);function Hf(e){return new Proxy(e,{get(t,r,n){var o=Reflect.get(t,r,n);if(!Yf.has(r))return o;return function(...i){Fs();var a=o.apply(this,i);return oo(),a}}})}function Ys(){let{prototype:e,__svelte_cleanup:t}=Array;if(t)t();let{indexOf:r,lastIndexOf:n,includes:o}=e;e.indexOf=function(i,a){let f=r.call(this,i,a);if(f===-1){for(let c=a??0;c<this.length;c+=1)if(Vr(this[c])===i){Hn("array.indexOf(...)");break}}return f},e.lastIndexOf=function(i,a){let f=n.call(this,i,a??this.length-1);if(f===-1){for(let c=0;c<=(a??this.length-1);c+=1)if(Vr(this[c])===i){Hn("array.lastIndexOf(...)");break}}return f},e.includes=function(i,a){let f=o.call(this,i,a);if(!f){for(let c=0;c<this.length;c+=1)if(Vr(this[c])===i){Hn("array.includes(...)");break}}return f},Array.__svelte_cleanup=()=>{e.indexOf=r,e.lastIndexOf=n,e.includes=o}}var Ur,Hs,gn,Vs,Us;function ao(){if(Ur!==void 0)return;Ur=window,Hs=document,gn=/Firefox/.test(navigator.userAgent);var e=Element.prototype,t=Node.prototype,r=Text.prototype;if(Vs=ze(t,"firstChild").get,Us=ze(t,"nextSibling").get,ui(e))e[an]=void 0,e[Bn]=null,e[_i]=void 0,e.__e=void 0;if(ui(r))r[sn]=void 0;if(p)e.__svelte_meta=null,Ys()}function se(e=""){return document.createTextNode(e)}function ie(e){return Vs.call(e)}function xe(e){return Us.call(e)}function O(e,t){if(!w)return ie(e);var r=ie(C);if(r===null)r=C.appendChild(se());else if(t&&r.nodeType!==ur){var n=se();return r?.before(n),U(n),n}if(t)xn(r);return U(r),r}function Ht(e,t=!1){if(!w){var r=ie(e);if(r instanceof Comment&&r.data==="")return xe(r);return r}if(t){if(C?.nodeType!==ur){var n=se();return C?.before(n),U(n),n}xn(C)}return C}function J(e,t=!1){if(!w)return ie(e);var r=O(e,t);return A(e),r}function x(e,t=1,r=!1){let n=w?C:e;var o;while(t--)o=n,n=xe(n);if(!w)return n;if(r){if(n?.nodeType!==ur){var i=se();if(n===null)o?.after(i);else n.before(i);return U(i),i}xn(n)}return U(n),n}function un(e){e.textContent=""}function so(){if(!ce)return!1;if(lt!==null)return!1;var e=k.f;return(e&Pe)!==0}function Ct(e,t,r){if(t==null||t===Yn)return r?document.createElement(e,{is:r}):document.createElement(e);return r?document.createElementNS(t,e,{is:r}):document.createElementNS(t,e)}function xn(e){if(e.nodeValue.length<65536)return;let t=e.nextSibling;while(t!==null&&t.nodeType===ur)t.remove(),e.nodeValue+=t.nodeValue,t=e.nextSibling}var Pi=new WeakMap;function qs(e){var t=k;if(t===null)return S.f|=nt,e;if(p&&e instanceof Error&&!Pi.has(e))Pi.set(e,Vf(e,t));if((t.f&Pe)===0&&(t.f&kt)===0){if(p&&!t.parent&&e instanceof Error)Gs(e);throw e}Ue(e,t)}function Ue(e,t){if(e===Ze)throw e;if(t!==null&&(t.f&W)!==0)return;while(t!==null){if((t.f&Cr)!==0&&(t.f&(W|Lt))===0){if((t.f&Pe)===0)throw e;try{t.b.error(e);return}catch(r){e=r}}t=t.parent}if(p&&e instanceof Error)Gs(e);throw e}function Vf(e,t){let r=ze(e,"message");if(r&&!r.configurable)return;var n=gn?"  ":"\t",o=`
${n}in ${t.fn?.name||"<unknown>"}`,i=t.ctx;while(i!==null)o+=`
${n}in ${i.function?.[it].split("/").pop()}`,i=i.p;return{message:e.message+`
${o}
`,stack:e.stack?.split(`
`).filter((a)=>!a.includes("svelte/src/internal")).join(`
`)}}function Gs(e){let t=Pi.get(e);if(t)me(e,"message",{value:t.message}),me(e,"stack",{value:t.stack})}function zs(e){if(k===null){if(S===null)_s(e);ds()}if(st)ps(e)}function Uf(e,t){var r=t.last;if(r===null)t.last=t.first=e;else r.next=e,e.prev=r,t.last=e}function jt(e,t){var r=k;if(p)while(r!==null&&(r.f&Kt)!==0)r=r.parent;if(r!==null&&(r.f&fe)!==0)e|=fe;var n={ctx:M,deps:null,nodes:null,f:e|P|Fe,first:null,fn:t,last:null,next:null,parent:r,b:r&&r.b,prev:null,teardown:null,wv:0,ac:null};if(p)n.component_function=xt;T?.register_created_effect(n);var o=n;if((e&kt)!==0)if(Hr!==null)Hr.push(n);else Ce.ensure().schedule(n);else if(t!==null){try{Rt(n)}catch(a){throw ae(n),a}if(o.deps===null&&o.teardown===null&&o.nodes===null&&o.first===o.last&&(o.f&Tt)===0){if(o=o.first,(e&Se)!==0&&(e&Ve)!==0&&o!==null)o.f|=Ve}}if(o!==null){if(o.parent=r,r!==null)Uf(o,r);if(S!==null&&(S.f&te)!==0&&(e&He)===0){var i=S;(i.effects??=[]).push(o)}}return n}function Jt(){return S!==null&&!Ye}function be(e){let t=jt(zt,null);return G(t,q),t.teardown=e,t}function qr(e){if(zs("$effect"),p)me(e,"name",{value:"$effect"});var t=k.f,r=!S&&(t&ge)!==0&&M!==null&&!M.i;if(r){var n=M;(n.e??=[]).push(e)}else return Ei(e)}function Ei(e){return jt(kt|za,e)}function Yi(e){Ce.ensure();let t=jt(He|Tt,e);return()=>{ae(t)}}function Ks(e){Ce.ensure();let t=jt(He|Tt,e);return(r={})=>new Promise((n)=>{if(r.outro)Ot(t,()=>{ae(t),n(void 0)});else ae(t),n(void 0)})}function et(e){return jt(kt,e)}function Cs(e){return jt(Ke|Tt,e)}function we(e,t=0){return jt(zt|t,e)}function V(e,t=[],r=[],n=[]){Jn(n,t,r,(o)=>{jt(zt,()=>{e(...o.map(l))})})}function tt(e,t=0){var r=jt(Se|t,e);if(p)r.dev_stack=St;return r}function ve(e){return jt(ge|Tt,e)}function Hi(e){var t=e.teardown;if(t!==null){let r=st,n=S;Bi(!0),de(null);try{t.call(null)}catch(o){Ue(o,e.parent)}finally{Bi(r),de(n)}}}function vn(e,t=!1){var r=e.first;e.first=e.last=null;while(r!==null){let o=r.ac;if(o!==null)Xe(()=>{o.abort(Ft)});var n=r.next;if((r.f&He)!==0)r.parent=null;else ae(r,t);r=n}}function Ws(e){var t=e.first;while(t!==null){var r=t.next;if((t.f&ge)===0)ae(t);t=r}}function ae(e,t=!0){var r=!1;if((t||(e.f&di)!==0)&&e.nodes!==null&&e.nodes.end!==null)Zs(e.nodes.start,e.nodes.end),r=!0;e.f|=Lt,vn(e,t&&!r),gr(e,0);var n=e.nodes&&e.nodes.t;if(n!==null)for(let i of n)i.stop();Hi(e),e.f^=Lt,e.f|=W;var o=e.parent;if(o!==null&&o.first!==null)Di(e);if(p)e.component_function=null;e.next=e.prev=e.teardown=e.ctx=e.deps=e.fn=e.nodes=e.ac=e.b=null}function Zs(e,t){while(e!==null){var r=e===t?null:xe(e);e.remove(),e=r}}function Di(e){var{parent:t,prev:r,next:n}=e;if(r!==null)r.next=n;if(n!==null)n.prev=r;if(t!==null){if(t.first===e)t.first=n;if(t.last===e)t.last=r}}function Ot(e,t,r=!0){var n=[];e.f|=Mn,Xs(e,n,!0);var o=()=>{if(r)ae(e);if(t)t()},i=n.length;if(i>0){var a=()=>--i||o();for(var f of n)f.out(a)}else o()}function Xs(e,t,r){if((e.f&fe)!==0)return;e.f^=fe;var n=e.nodes&&e.nodes.t;if(n!==null){for(let f of n)if(f.is_global||r)t.push(f)}var o=e.first;while(o!==null){var i=o.next;if((o.f&He)===0){var a=(o.f&Ve)!==0||(o.f&ge)!==0&&(e.f&Se)!==0;Xs(o,t,a?r:!1)}o=i}}function Gr(e){e.f&=~Mn,Js(e,!0)}function Js(e,t){if((e.f&Mn)!==0)return;if((e.f&fe)===0)return;if(e.f^=fe,(e.f&q)===0)G(e,P),Ce.ensure().schedule(e);var r=e.first;while(r!==null){var n=r.next,o=(r.f&Ve)!==0||(r.f&ge)!==0;Js(r,o?t:!1),r=n}var i=e.nodes&&e.nodes.t;if(i!==null){for(let a of i)if(a.is_global||t)a.in()}}function zr(e,t){if(!e.nodes)return;var r=e.nodes.start,n=e.nodes.end;while(r!==null){var o=r===n?null:xe(r);t.append(r),r=o}}var Qs=null;var st=!1;function Bi(e){st=e}var S=null,Ye=!1;function de(e){S=e}var k=null;function _e(e){k=e}var yt=null;function ro(e){if(S!==null&&(!ce&&(S.f&jr)!==0||(S.f&te)!==0))(yt??=new Set).add(e)}var ye=null,Be=0,Qe=null;function Bs(e){Qe=e}var tl=1,wr=0,tr=wr;function Fi(e){tr=e}function Br(){return++tl}function er(e){var t=e.f;if((t&P)!==0)return!0;if((t&oe)!==0){var r=e.deps,n=r.length;for(var o=0;o<n;o++){var i=r[o];if(er(i))Ni(i);if(i.wv>e.wv)return!0}if((t&Fe)!==0&&$e===null)G(e,q)}return!1}function rl(e,t,r=!0){var n=e.reactions;if(n===null)return;if(!ce&&yt!==null&&yt.has(e))return;for(var o=0;o<n.length;o++){var i=n[o];if((i.f&te)!==0)rl(i,t,!1);else if(t===i){if(r)G(i,P);else if((i.f&q)!==0)G(i,oe);mn(i)}}}function eo(e){var t=ye,r=Be,n=Qe,o=S,i=yt,a=M,f=Ye,c=tr,m=e.f;if(ye=null,Be=0,Qe=null,S=(m&(ge|He))===0?e:null,yt=null,Yt(e.ctx),Ye=!1,tr=++wr,e.ac!==null)Xe(()=>{e.ac.abort(Ft)}),e.ac=null;try{e.f|=jr;var h=e.fn,v=h();e.f|=Pe;var _=el(e);if(Nt()&&Qe!==null&&!Ye&&_!==null&&(e.f&(te|oe|P))===0)for(var d=0;d<Qe.length;d++)rl(Qe[d],e);if(o!==null&&o!==e){if(wr++,o.deps!==null)for(let y=0;y<r;y+=1)o.deps[y].rv=wr;if(t!==null)for(let y of t)y.rv=wr;if(Qe!==null)if(n===null)n=Qe;else n.push(...Qe)}if((e.f&nt)!==0)e.f^=nt;return v}catch(y){return el(e),qs(y)}finally{e.f^=jr,ye=t,Be=r,Qe=n,S=o,yt=i,Yt(a),Ye=f,tr=c}}function el(e){var t=e.deps,r=T?.is_fork;if(ye!==null){var n;if(!r)gr(e,Be);if(t!==null&&Be>0){t.length=Be+ye.length;for(n=0;n<ye.length;n++)t[Be+n]=ye[n]}else e.deps=t=ye;if(Jt()&&(e.f&Fe)!==0)for(n=Be;n<t.length;n++)(t[n].reactions??=[]).push(e)}else if(!r&&t!==null&&Be<t.length)gr(e,Be),t.length=Be;return t}function qf(e,t){let r=t.reactions;if(r!==null){var n=qa.call(r,e);if(n!==-1){var o=r.length-1;if(o===0)r=t.reactions=null;else r[n]=r[o],r.pop()}}if(r===null&&(t.f&te)!==0&&(ye===null||!Mt.call(ye,t))){var i=t;if((i.f&Fe)!==0)i.f^=Fe;if(i.v!==Z)Dr(i);if(i.ac!==null)Xe(()=>{i.ac.abort(Ft),i.ac=null,G(i,P)});Rs(i),gr(i,0)}}function gr(e,t){var r=e.deps;if(r===null)return;for(var n=t;n<r.length;n++)qf(e,r[n])}function Rt(e){var t=e.f;if((t&W)!==0)return;G(e,q);var r=k;if(k=e,p){var n=xt;Kn(e.component_function);var o=St;Ir(e.dev_stack??St)}try{if((t&(Se|cr))!==0)Ws(e);else vn(e);Hi(e);var i=eo(e);if(e.teardown=typeof i==="function"?i:null,e.wv=tl,p&&mt&&(e.f&P)!==0&&e.deps!==null){for(var a of e.deps)if(a.set_during_effect)a.wv=Br(),a.set_during_effect=!1}}finally{if(k=r,p)Kn(n),Ir(o)}}async function bn(){if(ce)return new Promise((e)=>{requestAnimationFrame(()=>e()),setTimeout(()=>e())});await Promise.resolve(),Qt()}function l(e){var t=e.f,r=(t&te)!==0;if(Qs?.add(e),S!==null&&!Ye){var n=k!==null&&(k.f&W)!==0;if(!n&&(yt===null||!yt.has(e))){var o=S.deps;if((S.f&jr)!==0){if(e.rv<wr)if(e.rv=wr,ye===null&&o!==null&&o[Be]===e)Be++;else if(ye===null)ye=[e];else ye.push(e)}else{if(S.deps??=[],!Mt.call(S.deps,e))S.deps.push(e);var i=e.reactions;if(i===null)e.reactions=[S];else if(!Mt.call(i,S))i.push(S)}}}if(p){if(!Ye&&qe&&T===null&&Je===null&&!qe.warned&&(qe.effect.f&jr)===0&&!qe.effect_deps.has(e)){qe.warned=!0,es(e.label);var a=gt("traced at");if(a)console.warn(a)}if(dn.delete(e),mt&&!Ye&&ln!==null&&S!==null&&ln.reaction===S){if(e.trace)e.trace();else if(a=gt("traced at"),a){var f=ln.entries.get(e);if(f===void 0)f={traces:[]},ln.entries.set(e,f);var c=f.traces[f.traces.length-1];if(a.stack!==c?.stack)f.traces.push(a)}}}if(st&&ft.has(e))return ft.get(e);if(r){var m=e;if(st){var h=m.v;if((m.f&q)===0&&m.reactions!==null||ol(m))h=_n(m);return ft.set(m,h),h}var v=(m.f&Fe)===0&&!Ye&&S!==null&&(S.f&Fe)!==0,_=(m.f&Pe)===0;if(er(m)){if(v)m.f|=Fe;Ni(m)}if(v&&!_)Ri(m),nl(m)}if($e?.has(e))return $e.get(e);if((e.f&nt)!==0)throw e.v;return e.v}function nl(e){if(e.f|=Fe,e.deps===null)return;for(let r of e.deps){var t=r.reactions;if(t===null)r.reactions=[e];else if(!Mt.call(t,e))t.push(e);if((r.f&te)!==0&&(r.f&Fe)===0)Ri(r),nl(r)}}function ol(e){if(e.v===Z)return!0;if(e.deps===null)return!1;for(let t of e.deps){if(ft.has(t))return!0;if((t.f&te)!==0&&ol(t))return!0}return!1}function X(e){var t=Ye;try{return Ye=!0,e()}finally{Ye=t}}var zf=["allowfullscreen","async","autofocus","autoplay","checked","controls","default","disabled","formnovalidate","indeterminate","inert","ismap","loop","multiple","muted","nomodule","novalidate","open","playsinline","readonly","required","reversed","seamless","selected","webkitdirectory","defer","disablepictureinpicture","disableremoteplayback"];var D_=[...zf,"formNoValidate","isMap","noModule","playsInline","readOnly","value","volume","defaultValue","defaultChecked","srcObject","noValidate","allowFullscreen","disablePictureInPicture","disableRemotePlayback"];var Kf=["touchstart","touchmove"];function il(e){return Kf.includes(e)}var Wf=["$state","$state.raw","$derived","$derived.by"],M_=[...Wf,"$state.eager","$state.snapshot","$props","$props.id","$bindable","$effect","$effect.pre","$effect.tracking","$effect.root","$effect.pending","$inspect","$inspect().with","$inspect.trace","$host"];var Kr=Symbol("events"),qi=new Set,lo=new Set;function Gi(e){if(!w)return;e.removeAttribute("onload"),e.removeAttribute("onerror");let t=e.__e;if(t!==void 0)e.__e=void 0,queueMicrotask(()=>{if(e.isConnected)e.dispatchEvent(t)})}function ll(e,t,r,n={}){function o(i){if(!n.capture)co.call(t,i);if(!i.cancelBubble)return Xe(()=>r?.call(this,i))}if(e.startsWith("pointer")||e.startsWith("touch")||e==="wheel")o.__removed=!1,ue(()=>{if(!o.__removed)t.addEventListener(e,o,n)});else t.addEventListener(e,o,n);return o}function $t(e,t,r,n,o){var i={capture:n,passive:o},a=ll(e,t,r,i);if(t===document.body||t===window||t===document||t instanceof HTMLMediaElement)be(()=>{a.__removed=!0,t.removeEventListener(e,a,i)})}function z(e,t,r){(t[Kr]??={})[e]=r}function fo(e){for(var t=0;t<e.length;t++)qi.add(e[t]);for(var r of lo)r(e)}var Vi=null,Ui=!1;function co(e){var t=this,r=t.ownerDocument,n=e.type,o=e.composedPath?.()||[],i=o[0]||e.target;if(Vi=e,!Ui)Ui=!0,setTimeout(()=>{Ui=!1,Vi=null});var a=0,f=Vi===e&&e[Kr];if(f){var c=o.indexOf(f);if(c!==-1&&(t===document||t===window)){e[Kr]=t;return}var m=o.indexOf(t);if(m===-1)return;if(c<=m)a=c}if(i=o[a]||e.target,i===t)return;me(e,"currentTarget",{configurable:!0,get(){return i||r}});var h=S,v=k;de(null),_e(null);try{var _,d=[];while(i!==null){if(i===t)break;try{var y=i[Kr]?.[n];if(y!=null&&(!i.disabled||e.target===i))y.call(i,e)}catch(R){if(_)d.push(R);else _=R}if(e.cancelBubble)break;a++,i=a<o.length?o[a]:null}if(_){for(let R of d)queueMicrotask(()=>{throw R});throw _}}finally{e[Kr]=t,delete e.currentTarget,de(h),_e(v)}}var Xf=globalThis?.window?.trustedTypes&&globalThis.window.trustedTypes.createPolicy("svelte-trusted-html",{createHTML:(e)=>e});function fl(e){return Xf?.createHTML(e)??e}function uo(e){var t=Ct("template");return t.innerHTML=fl(e.replaceAll("<!>","<!---->")),t.content}function Me(e,t){var r=k;if(r.nodes===null)r.nodes={start:e,end:t,a:null,t:null}}function re(e,t){var r=(t&gi)!==0,n=(t&Qa)!==0,o,i=!e.startsWith("<!>");return()=>{if(w)return Me(C,null),C;if(o===void 0){if(o=uo(i?e:"<!>"+e),!r)o=ie(o)}var a=n||gn?document.importNode(o,!0):o.cloneNode(!0);if(r){var f=ie(a),c=a.lastChild;Me(f,c)}else Me(a,a);return a}}function ec(e,t,r="svg"){var n=!e.startsWith("<!>"),o=(t&gi)!==0,i=`<${r}>${n?e:"<!>"+e}</${r}>`,a;return()=>{if(w)return Me(C,null),C;if(!a){var f=uo(i),c=ie(f);if(o){a=document.createDocumentFragment();while(ie(c))a.appendChild(ie(c))}else a=ie(c)}var m=a.cloneNode(!0);if(o){var h=ie(m),v=m.lastChild;Me(h,v)}else Me(m,m);return m}}function Ki(e,t){return ec(e,t,"svg")}function Vt(e=""){if(!w){var t=se(e+"");return Me(t,t),t}var r=C;if(r.nodeType!==ur)r.before(r=se()),U(r);else xn(r);return Me(r,r),r}function Wi(){if(w)return Me(C,null),C;var e=document.createDocumentFragment(),t=document.createComment(""),r=se();return e.append(t,r),Me(t,r),e}function D(e,t){if(w){var r=k;if((r.f&Pe)===0||r.nodes.end===null)r.nodes.end=C;Oe();return}if(e===null)return;e.before(t)}function cl(e){let t=0,r=De(0),n;if(p)Ie(r,"createSubscriber version");return()=>{if(Jt())l(r),we(()=>{if(t===0)n=X(()=>e(()=>yr(r)));return t+=1,()=>{ue(()=>{if(t-=1,t===0)n?.(),n=void 0,yr(r)})}})}}var tc=Ve|Tt;function Zi(e,t,r,n){new ul(e,t,r,n)}class ul{parent;is_pending=!1;transform_error;#e;#t=w?C:null;#r;#s;#o;#n=null;#a=null;#i=null;#l=null;#d=0;#f=0;#v=!1;#g=new Map;#c=null;#h=cl(()=>{if(this.#c=De(this.#d),p)Ie(this.#c,"$effect.pending()");return()=>{this.#c=null}});constructor(e,t,r,n){if(this.#e=e,this.#r=t,this.#s=(o)=>{var i=k;i.b=this,i.f|=Cr,r(o)},this.parent=k.b,this.transform_error=n??this.parent?.transform_error??((o)=>o),this.#o=tt(()=>{if(w){let o=_r(this.#t);Oe();let i=o===Zt;if(o.startsWith(xi)){let f=JSON.parse(o.slice(xi.length));this.#y(f)}else if(i)this.#_();else this.#x()}else this.#b()},tc),w)this.#e=C}#x(){try{this.#n=ve(()=>this.#s(this.#e))}catch(e){this.error(e)}}#y(e){let t=this.#r.failed,{reset:r,invoke_onerror:n}=this.#m(e);if(ue(n),!t)return;this.#i=ve(()=>{t(this.#e,()=>e,()=>r)})}#m(e){var t=!1,r=!1;let n=()=>{if(this.#u())return;if(t){as();return}if(t=!0,r)ys();if(this.#i!==null)Ot(this.#i,()=>{this.#i=null});this.#$(()=>{this.#b()})};return{reset:n,invoke_onerror:()=>{if(this.#u())return;try{r=!0,this.#r.onerror?.(e,n),r=!1}catch(i){Ue(i,this.#o&&this.#o.parent)}}}}#u(){return(this.#o.f&(W|Lt))!==0}#_(){let e=this.#r.pending;if(!e)return;this.is_pending=!0,this.#a=ve(()=>e(this.#e)),ue(()=>{if(this.#u())return;var t=this.#l=document.createDocumentFragment(),r=se(),n=!1;if(t.append(r),this.#n=this.#$(()=>{try{return ve(()=>this.#s(r))}catch(o){try{this.error(o),n=!0}catch(i){Ue(i,this.#o.parent)}return null}}),this.#n===null){if(this.#l=null,n)this.#p(T);return}if(this.#f===0)this.#e.before(t),this.#l=null,Ot(this.#a,()=>{this.#a=null}),this.#p(T)})}#b(){try{if(this.is_pending=this.has_pending_snippet(),this.#f=0,this.#d=0,this.#n=ve(()=>{this.#s(this.#e)}),this.#f>0){var e=this.#l=document.createDocumentFragment();zr(this.#n,e);let t=this.#r.pending;this.#a=ve(()=>t(this.#e))}else this.#p(T)}catch(t){this.error(t)}}#p(e){this.is_pending=!1,e.transfer_reactions(this.#g)}defer_effect(e){Zn(e,this.#g)}is_rendered(){return!this.is_pending&&(!this.parent||this.parent.is_rendered())}has_pending_snippet(){return!!this.#r.pending}#$(e){var t=k,r=S,n=M;_e(this.#o),de(this.#o),Yt(this.#o.ctx);try{return Ce.ensure(),e()}finally{_e(t),de(r),Yt(n)}}#w(e,t){if(!this.has_pending_snippet()){if(this.parent)this.parent.#w(e,t);return}if(this.#f+=e,this.#f===0){if(this.#p(t),this.#a)Ot(this.#a,()=>{this.#a=null});if(this.#l)this.#e.before(this.#l),this.#l=null}}update_pending_count(e,t){if(this.#w(e,t),this.#d+=e,!this.#c||this.#v)return;this.#v=!0,ue(()=>{if(this.#v=!1,this.#c)bt(this.#c,this.#d)})}get_effect_pending(){return this.#h(),l(this.#c)}error(e){if(e===Ze)throw e;if(!this.#r.onerror&&!this.#r.failed)throw e;if(T?.is_fork){if(this.#n)T.skip_effect(this.#n);if(this.#a)T.skip_effect(this.#a);if(this.#i)T.skip_effect(this.#i);T.oncommit(()=>{if(!this.#u())this.#E(e)})}else this.#E(e)}#E(e){if(this.#n)ae(this.#n),this.#n=null;if(this.#a)ae(this.#a),this.#a=null;if(this.#i)ae(this.#i),this.#i=null;if(w)U(this.#t),at(),U(At());let t=this.#r.failed,r=(n)=>{if(this.#u())return;let{reset:o,invoke_onerror:i}=this.#m(n);if(i(),t&&!this.#u())this.#i=this.#$(()=>{try{return ve(()=>{var a=k;a.b=this,a.f|=Cr,t(this.#e,()=>n,()=>o)})}catch(a){return Ue(a,this.#o.parent),null}})};ue(()=>{if(this.#u())return;var n;try{n=this.transform_error(e)}catch(o){Ue(o,this.#o&&this.#o.parent);return}if(n!==null&&typeof n==="object"&&typeof n.then==="function")n.then(r,(o)=>Ue(o,this.#o&&this.#o.parent));else r(n)})}}var Xi=!0;function L(e,t){var r=t==null?"":typeof t==="object"?`${t}`:t;if(r!==(e[sn]??=e.nodeValue))e[sn]=r,e.nodeValue=`${r}`}function Wr(e,t){return pl(e,t)}function Qi(e,t){ao(),t.intro=t.intro??!1;let r=t.target,n=w,o=C;try{var i=ie(r);while(i&&(i.nodeType!==ot||i.data!==Or))i=xe(i);if(!i)throw Ze;pe(!0),U(i);let a=pl(e,{...t,anchor:i});return pe(!1),a}catch(a){if(a instanceof Error&&a.message.split(`
`).some((f)=>f.startsWith("https://svelte.dev/e/")))throw a;if(a!==Ze)console.warn("Failed to hydrate: ",a);if(t.recover===!1)hs();return ao(),un(r),pe(!1),Wr(e,t)}finally{pe(n),U(o)}}var po=new Map;function pl(e,{target:t,anchor:r,props:n={},events:o,context:i,intro:a=!0,transformError:f}){ao();var c=void 0,m=Ks(()=>{var h=r??t.appendChild(se());Zi(h,{pending:()=>{}},(d)=>{fn({});var y=M;if(i)y.c=i;if(o)n.$$events=o;if(w)Me(d,null);if(Xi=a,c=e(d,n)||Wn(),Xi=!0,w){if(k.nodes.end=C,C===null||C.nodeType!==ot||C.data!==pr)throw dr(),Ze}cn()},f);var v=new Set,_=(d)=>{for(var y=0;y<d.length;y++){var R=d[y];if(v.has(R))continue;v.add(R);var b=il(R);for(let Q of[t,document]){var j=po.get(Q);if(j===void 0)j=new Map,po.set(Q,j);var Y=j.get(R);if(Y===void 0)Q.addEventListener(R,co,{passive:b}),j.set(R,1);else j.set(R,Y+1)}}};return _(Nr(qi)),lo.add(_),()=>{for(var d of v)for(let b of[t,document]){var y=po.get(b),R=y.get(d);if(--R==0){if(b.removeEventListener(d,co),y.delete(d),y.size===0)po.delete(b)}else y.set(d,R)}if(lo.delete(_),h!==r)h.parentNode?.removeChild(h)}});return Ji.set(c,m),c}var Ji=new WeakMap;function ea(e,t){let r=Ji.get(e);if(r)return Ji.delete(e),r(t);if(p)os();return Promise.resolve()}class rr{anchor;#e=new Map;#t=new Map;#r=new Map;#s=new Set;#o=!0;constructor(e,t=!0){this.anchor=e,this.#o=t}#n=(e)=>{if(!this.#e.has(e))return;var t=this.#e.get(e),r=this.#t.get(t);if(r)Gr(r),this.#s.delete(t);else{var n=this.#r.get(t);if(n){if(Gr(n.effect),this.#t.set(t,n.effect),this.#r.delete(t),p)n.fragment.lastChild[vi]=this.anchor;n.fragment.lastChild.remove(),this.anchor.before(n.fragment),r=n.effect}}for(let[o,i]of this.#e){if(this.#e.delete(o),o===e)break;let a=this.#r.get(i);if(a)ae(a.effect),this.#r.delete(i)}for(let[o,i]of this.#t){if(o===t||this.#s.has(o))continue;let a=()=>{if(Array.from(this.#e.values()).includes(o)){var c=document.createDocumentFragment();zr(i,c),c.append(se()),this.#r.set(o,{effect:i,fragment:c})}else ae(i);this.#s.delete(o),this.#t.delete(o)};if(this.#o||!r)this.#s.add(o),Ot(i,a,!1);else a()}};#a=(e)=>{this.#e.delete(e);let t=Array.from(this.#e.values());for(let[r,n]of this.#r)if(!t.includes(r))ae(n.effect),this.#r.delete(r)};ensure(e,t){var r=T,n=so();if(t&&!this.#t.has(e)&&!this.#r.has(e))if(n){var o=document.createDocumentFragment(),i=se();o.append(i),this.#r.set(e,{effect:ve(()=>t(i)),fragment:o})}else this.#t.set(e,ve(()=>t(this.anchor)));if(this.#e.set(r,e),n){for(let[a,f]of this.#t)if(a===e)r.unskip_effect(f);else r.skip_effect(f);for(let[a,f]of this.#r)if(a===e)r.unskip_effect(f.effect);else r.skip_effect(f.effect);r.oncommit(this.#n),r.ondiscard(this.#a)}else{if(w)this.anchor=C;this.#n(r)}}}function ne(e,t,r=!1){var n;if(w)n=C,Oe();var o=new rr(e),i=r?Ve:0;function a(f,c){if(w){var m=_r(n);if(f!==parseInt(m.substring(1))){var h=At();U(h),o.anchor=h,pe(!1),o.ensure(f,c),pe(!0);return}}o.ensure(f,c)}tt(()=>{var f=!1;if(t((c,m=0)=>{f=!0,a(m,c)}),!f)a(-1,null)},i)}var cm=Symbol("NaN");function ra(e,t){return t}function nc(e,t,r){var n=[],o=t.length,i,a=t.length;for(var f=0;f<o;f++){let v=t[f];Ot(v,()=>{if(i){if(i.pending.delete(v),i.done.add(v),i.pending.size===0){var _=e.outrogroups;if(ta(e,Nr(i.done)),_.delete(i),_.size===0)e.outrogroups=null}}else a-=1},!1)}if(a===0){var c=n.length===0&&r!==null&&e.pending.size===0;if(c){var m=r,h=m.parentNode;un(h),h.append(m),e.items.clear()}ta(e,t,!c)}else i={pending:new Set(t),done:new Set},(e.outrogroups??=new Set).add(i)}function ta(e,t,r=!0){var n;if(e.pending.size>0){n=new Set;for(let a of e.pending.values())for(let f of a)n.add(e.items.get(f).e)}for(var o=0;o<t.length;o++){var i=t[o];if(n?.has(i)){i.f|=ht;let a=document.createDocumentFragment();zr(i,a)}else ae(t[o],r)}}var dl;function or(e,t,r,n,o,i=null){var a=e,f=new Map,c=(t&mi)!==0;if(c){var m=e;a=w?U(ie(m)):m.appendChild(se())}if(w)Oe();var h=null,v=Fr(()=>{var Y=r();return vt(Y)?Y:Y==null?[]:Nr(Y)});if(p)Ie(v,"{#each ...}");var _=new Map,d=!0;function y(Y){if((j.effect.f&W)!==0)return;j.pending.delete(Y);var Q=l(v);if(j.fallback=h,oc(j,Q,a,t,n),h!==null)if(Q.length===0)if((h.f&ht)===0)Gr(h);else h.f^=ht,$n(h,null,a);else Ot(h,()=>{h=null})}function R(Y){j.pending.delete(Y)}var b=tt(()=>{var Y=l(v),Q=Y.length;let le=!1;if(w){var pt=_r(a)===Zt;if(pt!==(Q===0))a=At(),U(a),pe(!1),le=!0}var Ee=new Set,ke=T,wt=so();for(var Le=0;Le<Q;Le+=1){if(w&&C.nodeType===ot&&C.data===pr)a=C,le=!0,pe(!1);var Ge=Y[Le],qt=n(Ge,Le);if(p){var ar=n(Ge,Le);if(qt!==ar)us(String(Le),String(qt),String(ar))}var dt=d?null:f.get(qt);if(dt){if(dt.v)bt(dt.v,Ge);if(dt.i)bt(dt.i,Le);if(wt)ke.unskip_effect(dt.e)}else{if(dt=ic(f,d?a:dl??=se(),Ge,qt,Le,o,t,r),!d)dt.e.f|=ht;f.set(qt,dt)}Ee.add(qt)}if(Q===0&&i&&!h)if(d)h=ve(()=>i(a));else h=ve(()=>i(dl??=se())),h.f|=ht;if(Q>Ee.size)if(p)ac(Y,n);else wi("","","");if(w&&Q>0)U(At());if(!d)if(_.set(ke,Ee),wt){for(let[En,Jr]of f)if(!Ee.has(En))ke.skip_effect(Jr.e);ke.oncommit(y),ke.ondiscard(R)}else y(ke);if(le)pe(!0);l(v)}),j={effect:b,flags:t,items:f,pending:_,outrogroups:null,fallback:h};if(d=!1,w)a=C}function yn(e){while(e!==null&&(e.f&ge)===0)e=e.next;return e}function oc(e,t,r,n,o){var i=(n&Xa)!==0,a=t.length,f=e.items,c=yn(e.effect.first),m,h=null,v,_=[],d=[],y,R,b,j;if(i){for(j=0;j<a;j+=1)if(y=t[j],R=o(y,j),b=f.get(R).e,(b.f&ht)===0)b.nodes?.a?.measure(),(v??=new Set).add(b)}for(j=0;j<a;j+=1){if(y=t[j],R=o(y,j),b=f.get(R).e,e.outrogroups!==null)for(let Ge of e.outrogroups)Ge.pending.delete(b),Ge.done.delete(b);if((b.f&fe)!==0){if(Gr(b),i)b.nodes?.a?.unfix(),(v??=new Set).delete(b)}if((b.f&ht)!==0)if(b.f^=ht,b===c)$n(b,null,r);else{var Y=h?h.next:c;if(b===e.effect.last)e.effect.last=b.prev;if(b.prev)b.prev.next=b.next;if(b.next)b.next.prev=b.prev;nr(e,h,b),nr(e,b,Y),$n(b,Y,r),h=b,_=[],d=[],c=yn(h.next);continue}if(b!==c){if(m!==void 0&&m.has(b)){if(_.length<d.length){var Q=d[0],le;h=Q.prev;var pt=_[0],Ee=_[_.length-1];for(le=0;le<_.length;le+=1)$n(_[le],Q,r);for(le=0;le<d.length;le+=1)m.delete(d[le]);nr(e,pt.prev,Ee.next),nr(e,h,pt),nr(e,Ee,Q),c=Q,h=Ee,j-=1,_=[],d=[]}else m.delete(b),$n(b,c,r),nr(e,b.prev,b.next),nr(e,b,h===null?e.effect.first:h.next),nr(e,h,b),h=b;continue}_=[],d=[];while(c!==null&&c!==b)(m??=new Set).add(c),d.push(c),c=yn(c.next);if(c===null)continue}if((b.f&ht)===0)_.push(b);h=b,c=yn(b.next)}if(e.outrogroups!==null){for(let Ge of e.outrogroups)if(Ge.pending.size===0)ta(e,Nr(Ge.done)),e.outrogroups?.delete(Ge);if(e.outrogroups.size===0)e.outrogroups=null}if(c!==null||m!==void 0){var ke=[];if(m!==void 0){for(b of m)if((b.f&fe)===0)ke.push(b)}while(c!==null){if((c.f&fe)===0&&c!==e.fallback)ke.push(c);c=yn(c.next)}var wt=ke.length;if(wt>0){var Le=(n&mi)!==0&&a===0?r:null;if(i){for(j=0;j<wt;j+=1)ke[j].nodes?.a?.measure();for(j=0;j<wt;j+=1)ke[j].nodes?.a?.fix()}nc(e,ke,Le)}}if(i)ue(()=>{if(v===void 0)return;for(b of v)b.nodes?.a?.apply()})}function ic(e,t,r,n,o,i,a,f){var c=(a&Wa)!==0?(a&Ja)===0?br(r,!1,!1):De(r):null,m=(a&Za)!==0?De(o):null;if(p&&c)c.trace=()=>{f()[m?.v??o]};return{v:c,i:m,e:ve(()=>(i(t,c??r,m??o,f),()=>{e.delete(n)}))}}function $n(e,t,r){if(!e.nodes)return;var n=e.nodes.start,o=e.nodes.end,i=t&&(t.f&ht)===0?t.nodes.start:r;while(n!==null){var a=xe(n);if(i.before(n),n===o)return;n=a}}function nr(e,t,r){if(t===null)e.effect.first=r;else t.next=r;if(r===null)e.effect.last=t;else r.prev=t}function ac(e,t){let r=new Map,n=e.length;for(let o=0;o<n;o++){let i=t(e[o],o);if(r.has(i)){let a=String(r.get(i)),f=String(o),c=String(i);if(c.startsWith("[object "))c=null;wi(a,f,c)}r.set(i,o)}}var vl=[...` 	
\r\f \v\uFEFF`];function hl(e,t,r){var n=e==null?"":""+e;if(t)n=n?n+" "+t:t;if(r){for(var o of Object.keys(r))if(r[o])n=n?n+" "+o:o;else if(n.length){var i=o.length,a=0;while((a=n.indexOf(o,a))>=0){var f=a+i;if((a===0||vl.includes(n[a-1]))&&(f===n.length||vl.includes(n[f])))n=(a===0?"":n.substring(0,a))+n.substring(f+1);else a=f}}}return n===""?null:n}function ir(e,t,r,n,o,i){var a=e[an];if(w||a!==r||a===void 0){var f=hl(r,n,i);if(!w||f!==e.getAttribute("class"))if(f==null)e.removeAttribute("class");else if(t)e.className=f;else e.setAttribute("class",f);e[an]=r}else if(i&&o!==i)for(var c in i){var m=!!i[c];if(o==null||m!==!!o[c])e.classList.toggle(c,m)}return i}function na(e,t){if(t){if(!e.hasAttribute("selected"))e.setAttribute("selected","")}else e.removeAttribute("selected")}function pc(e,t){var{__defaultValue:r,multiple:n}=e,o=n?r??[]:null;if(n&&!vt(o))return;var i=e.selectedIndex,a=t&&n?new Set(e.selectedOptions):null;for(var f of e.options){var c=Zr(f);na(f,n?o.includes(c):io(c,r))}if(!t)return;if(a!==null)for(f of e.options){var m=a.has(f);if(f.selected!==m)f.selected=m}else if(e.selectedIndex!==i)e.selectedIndex=i}function Xr(e,t,r=!1){if(e.multiple){if(t==null)return;if(!vt(t))return is();for(var n of e.options)n.selected=t.includes(Zr(n));return}for(n of e.options){var o=Zr(n);if(io(o,t)){n.selected=!0;return}}if(!r||t!==void 0)e.selectedIndex=-1}function Er(e){var t=new MutationObserver((r)=>{if(r.every(dc))return;if("__defaultValue"in e)pc(e,!1);if("__value"in e)Xr(e,e.__value)});t.observe(e,{childList:!0,subtree:!0,attributes:!0,attributeFilter:["value"]}),be(()=>{t.disconnect()})}function wn(e,t,r=t){var n=new WeakSet,o=!0;pn(e,"change",(i)=>{var a=i?"[selected]":":checked",f;if(e.multiple)f=[].map.call(e.querySelectorAll(a),Zr);else{var c=e.querySelector(a)??e.querySelector("option:not([disabled])");f=c&&Zr(c)}if(r(f),e.__value=f,T!==null)n.add(T)}),et(()=>{var i=t();if(e===document.activeElement){var a=ce?Je:T;if(n.has(a))return}if(Xr(e,i,o),o&&i===void 0){var f=e.querySelector(":checked");if(f!==null)i=Zr(f),r(i)}e.__value=i,o=!1})}function Zr(e){if("__value"in e)return e.__value;else return e.value}function dc(e){if(e.target.closest("selectedcontent")!==null)return!0;if(e.type==="childList"){var t=[...e.addedNodes,...e.removedNodes];return t.length>0&&t.every((r)=>r.nodeName==="SELECTEDCONTENT")}return!1}var gc=Symbol("class"),xc=Symbol("style"),bc=Symbol("is custom element"),yc=Symbol("is html"),$c=hi?"link":"LINK";function ut(e){if(!w)return;var t=!1,r=()=>{if(t)return;if(t=!0,e.hasAttribute("value")){var n=e.value;rt(e,"value",null),e.value=n}if(e.hasAttribute("checked")){var o=e.checked;rt(e,"checked",null),e.checked=o}};e[Wt]=r,ue(r),Xn()}function _o(e,t){var r=bl(e);if(r.checked===(r.checked=t??void 0))return;e.checked=t}function rt(e,t,r,n){var o=bl(e);if(w){if(o[t]=e.getAttribute(t),t==="src"||t==="srcset"||t==="href"&&e.nodeName===$c){if(!n)Ec(e,t,r??"");return}}var i=o[t];if(i===(o[t]=r))return;if(t==="loading")e[Ka]=r;if(r==null)e.removeAttribute(t);else if(typeof r!=="string"&&wc(e).has(t))e[t]=r;else if(!(w&&(typeof r==="number"||typeof r==="boolean")&&i===String(r)))e.setAttribute(t,r)}function bl(e){return e[Bn]??={[bc]:e.nodeName.includes("-"),[yc]:e.namespaceURI===Yn}}var ml=new Map;function wc(e){var t=e.getAttribute("is")||e.nodeName,r=ml.get(t);if(r)return r;ml.set(t,r=new Set);var n,o=e,i=Element.prototype;while(i!==o){n=fi(o);for(var a in n)if(n[a].set&&a!=="innerHTML"&&a!=="textContent"&&a!=="innerText")r.add(a);o=Rr(o)}return r}function Ec(e,t,r){if(!p)return;if(t==="srcset"&&kc(e,r))return;if(oa(e.getAttribute(t)??"",r))return;ns(t,e.outerHTML.replace(e.innerHTML,e.innerHTML&&"..."),String(r))}function oa(e,t){if(e===t)return!0;return new URL(e,document.baseURI).href===new URL(t,document.baseURI).href}function gl(e){return e.split(",").map((t)=>t.trim().split(" ").filter(Boolean))}function kc(e,t){var r=gl(e.srcset),n=gl(t);return n.length===r.length&&n.every(([o,i],a)=>i===r[a][1]&&(oa(r[a][0],o)||oa(o,r[a][0])))}function Ut(e,t,r=t){var n=new WeakSet;if(pn(e,"input",async(o)=>{if(p&&e.type==="checkbox")$i();var i=o?e.defaultValue:e.value;if(i=ia(e)?aa(i):i,r(i),T!==null)n.add(T);if(await bn(),i!==(i=t())){var{selectionStart:a,selectionEnd:f}=e,c=e.value.length;if(e.value=i??"",f!==null){var m=e.value.length;if(a===f&&f===c&&m>c)e.selectionStart=m,e.selectionEnd=m;else e.selectionStart=a,e.selectionEnd=Math.min(f,m)}}}),w&&e.defaultValue!==e.value||X(t)==null&&e.value){if(r(ia(e)?aa(e.value):e.value),T!==null)n.add(T)}we(()=>{if(p&&e.type==="checkbox")$i();var o=t();if(e===document.activeElement){var i=ce?Je:T;if(n.has(i))return}if(ia(e)&&o===aa(e.value))return;if(e.type==="date"&&!o&&!e.value)return;if(o!==e.value)e.value=o??""})}function sa(e,t,r=t){if(pn(e,"change",(n)=>{var o=n?e.defaultChecked:e.checked;r(o)}),w&&e.defaultChecked!==e.checked||X(t)==null)r(e.checked);we(()=>{var n=t();e.checked=Boolean(n)})}function ia(e){var t=e.type;return t==="number"||t==="range"}function aa(e){return e===""?null:+e}var Sy=Symbol("unmounted");function yl(e){return new $l(e)}class $l{#e;#t;constructor(e){var t=new Map,r=(o,i)=>{var a=br(i,!1,!1);return t.set(o,a),a};let n=new Proxy({...e.props||{},$$events:{}},{get(o,i){return l(t.get(i)??r(i,Reflect.get(o,i)))},has(o,i){if(i===Fn)return!0;return l(t.get(i)??r(i,Reflect.get(o,i))),Reflect.has(o,i)},set(o,i,a){return N(t.get(i)??r(i,a),a),Reflect.set(o,i,a)}});if(this.#t=(e.hydrate?Qi:Wr)(e.component,{target:e.target,anchor:e.anchor,props:n,context:e.context,intro:e.intro??!1,recover:e.recover,transformError:e.transformError}),!ce&&(!e?.props?.$$host||e.sync===!1))Qt();this.#e=n.$$events;for(let o of Object.keys(this.#t)){if(o==="$set"||o==="$destroy"||o==="$on")continue;me(this,o,{get(){return this.#t[o]},set(i){this.#t[o]=i},enumerable:!0})}this.#t.$set=(o)=>{Object.assign(n,o)},this.#t.$destroy=()=>{ea(this.#t)}}$set(e){this.#t.$set(e)}$on(e,t){this.#e[e]=this.#e[e]||[];let r=(...n)=>t.call(this,...n);return this.#e[e].push(r),()=>{this.#e[e]=this.#e[e].filter((n)=>n!==r)}}$destroy(){this.#t.$destroy()}}var Dc;if(typeof HTMLElement==="function")Dc=class extends HTMLElement{$$ctor;$$s;$$c;$$cn=!1;$$d={};$$r=!1;$$p_d={};$$l={};$$l_u=new Map;$$me;$$shadowRoot=null;constructor(e,t,r){super();if(this.$$ctor=e,this.$$s=t,r)this.$$shadowRoot=this.attachShadow(r)}addEventListener(e,t,r){if(this.$$l[e]=this.$$l[e]||[],this.$$l[e].push(t),this.$$c){let n=this.$$c.$on(e,t);this.$$l_u.set(t,n)}super.addEventListener(e,t,r)}removeEventListener(e,t,r){if(super.removeEventListener(e,t,r),this.$$c){let n=this.$$l_u.get(t);if(n)n(),this.$$l_u.delete(t)}}async connectedCallback(){if(this.$$cn=!0,!this.$$c){let e=function(n){return(o)=>{let i=Ct("slot");if(n!=="default")i.name=n;D(o,i)}};if(await Promise.resolve(),!this.$$cn||this.$$c)return;let t={},r=Mc(this);for(let n of this.$$s)if(n in r)if(n==="default"&&!this.$$d.children)this.$$d.children=e(n),t.default=!0;else t[n]=e(n);for(let n of this.attributes){let o=this.$$g_p(n.name);if(!(o in this.$$d))this.$$d[o]=fa(o,n.value,this.$$p_d,"toProp")}for(let n in this.$$p_d)if(!(n in this.$$d)&&this[n]!==void 0)this.$$d[n]=this[n],delete this[n];this.$$c=yl({component:this.$$ctor,target:this.$$shadowRoot||this,props:{...this.$$d,$$slots:t,$$host:this}}),this.$$me=Yi(()=>{we(()=>{this.$$r=!0;for(let n of li(this.$$c)){if(!this.$$p_d[n]?.reflect)continue;this.$$d[n]=this.$$c[n];let o=fa(n,this.$$d[n],this.$$p_d,"toAttribute");if(o==null)this.removeAttribute(this.$$p_d[n].attribute||n);else this.setAttribute(this.$$p_d[n].attribute||n,o)}this.$$r=!1})});for(let n in this.$$l)for(let o of this.$$l[n]){let i=this.$$c.$on(n,o);this.$$l_u.set(o,i)}this.$$l={}}}attributeChangedCallback(e,t,r){if(this.$$r)return;e=this.$$g_p(e),this.$$d[e]=fa(e,r,this.$$p_d,"toProp"),this.$$c?.$set({[e]:this.$$d[e]})}disconnectedCallback(){this.$$cn=!1,Promise.resolve().then(()=>{if(!this.$$cn&&this.$$c)this.$$c.$destroy(),this.$$me(),this.$$c=void 0})}$$g_p(e){return li(this.$$p_d).find((t)=>this.$$p_d[t].attribute===e||!this.$$p_d[t].attribute&&t.toLowerCase()===e)||e}};function fa(e,t,r,n){let o=r[e]?.type;if(t=o==="Boolean"&&typeof t!=="boolean"?t!=null:t,!n||!r[e])return t;else if(n==="toAttribute")switch(o){case"Object":case"Array":return t==null?null:JSON.stringify(t);case"Boolean":return t?"":null;case"Number":return t==null?null:t;default:return t}else switch(o){case"Object":case"Array":return t&&JSON.parse(t);case"Boolean":return t;case"Number":return t!=null?+t:t;default:return t}}function Mc(e){let t={};return e.childNodes.forEach((r)=>{t[r.slot||"default"]=!0}),t}if(p){let e=function(t){if(!(t in globalThis)){let r;Object.defineProperty(globalThis,t,{configurable:!0,get:()=>{if(r!==void 0)return r;ms(t)},set:(n)=>{r=n}})}};e("$state"),e("$effect"),e("$derived"),e("$inspect"),e("$props"),e("$bindable")}function wl(e){if(M===null)qn("onMount");if(Xt&&M.l!==null)Bc(M).m.push(e);else qr(()=>{let t=X(e);if(typeof t==="function")return t})}function Bc(e){var t=e.l;return t.u??={a:[],b:[],m:[]}}var El="5";if(typeof window<"u")((window.__svelte??={}).v??=new Set).add(El);var Yc=re('<br class="svelte-1xjvnya8h1aa3"/> ',1),Hc=re('<p class="waktu svelte-1xjvnya8h1aa3"> <!></p>'),Vc=re('<span class="svelte-1xjvnya8h1aa3"> </span>'),Uc=re('<div class="peringatan svelte-1xjvnya8h1aa3"></div>'),kr=re('<p class="ket svelte-1xjvnya8h1aa3"> </p>'),qc=re('<img alt=""/> <!>',1),Gc=re('<button class="stop svelte-1xjvnya8h1aa3"> </button>'),zc=re("<button> </button>"),Kc=re('<button class="aktif-jelajah svelte-1xjvnya8h1aa3"> </button>'),Wc=re('<select aria-label="Lama jelajah" class="svelte-1xjvnya8h1aa3"><option class="svelte-1xjvnya8h1aa3">5 menit</option><option class="svelte-1xjvnya8h1aa3">10 menit</option><option class="svelte-1xjvnya8h1aa3">15 menit</option><option class="svelte-1xjvnya8h1aa3">30 menit</option></select> <button class="svelte-1xjvnya8h1aa3">Jelajah sendiri</button>',1),kl=re('<small class="svelte-1xjvnya8h1aa3"> </small>'),Zc=Ki('<line x1="0" class="svelte-1xjvnya8h1aa3"></line><text x="2" class="svelte-1xjvnya8h1aa3"> </text>',1),Xc=re('<svg class="grafik svelte-1xjvnya8h1aa3" role="img"><!><path class="svelte-1xjvnya8h1aa3"></path></svg> <div class="sumbu svelte-1xjvnya8h1aa3"><span class="svelte-1xjvnya8h1aa3"> </span><span class="svelte-1xjvnya8h1aa3">sekarang</span></div>',1),Jc=re('<p class="ket svelte-1xjvnya8h1aa3">Grafik muncul setelah robot menyala 2 menit (satu titik per menit, paling lama 6 jam).</p>'),Qc=re(" <!>",1),eu=re('<li class="svelte-1xjvnya8h1aa3"><b class="svelte-1xjvnya8h1aa3"> </b> <span class="svelte-1xjvnya8h1aa3"> <small class="svelte-1xjvnya8h1aa3"> </small></span> <button class="hapus svelte-1xjvnya8h1aa3">✕</button></li>'),tu=re('<ul class="alarm svelte-1xjvnya8h1aa3"></ul>'),ru=re('<label class="svelte-1xjvnya8h1aa3"><input type="checkbox" class="svelte-1xjvnya8h1aa3"/> </label>'),nu=re('<div class="hari svelte-1xjvnya8h1aa3"></div>'),ou=re('<button><b class="svelte-1xjvnya8h1aa3"> </b> <span class="svelte-1xjvnya8h1aa3"> </span></button>'),iu=re('<p class="status svelte-1xjvnya8h1aa3"> </p>'),au=re('<main class="svelte-1xjvnya8h1aa3"><header class="svelte-1xjvnya8h1aa3"><h1 class="svelte-1xjvnya8h1aa3"> </h1> <span></span> <span class="kecil svelte-1xjvnya8h1aa3"><!></span></header> <!> <!> <section class="bicara svelte-1xjvnya8h1aa3"><button> </button> <p class="ket kecil-ket svelte-1xjvnya8h1aa3">Ketuk, lalu bicara ke mikrofon robot. Robot berhenti merekam saat kamu diam (paling lama 6 detik).</p> <label class="saklar svelte-1xjvnya8h1aa3"><input type="checkbox" class="svelte-1xjvnya8h1aa3"/> </label></section> <section class="kamera svelte-1xjvnya8h1aa3"><!> <button class="kecil-btn svelte-1xjvnya8h1aa3"> </button></section> <section class="svelte-1xjvnya8h1aa3"><h2 class="svelte-1xjvnya8h1aa3">Gerak <small class="svelte-1xjvnya8h1aa3">tahan untuk jalan, lepas untuk berhenti</small></h2> <div class="dpad svelte-1xjvnya8h1aa3"></div> <label class="svelte-1xjvnya8h1aa3"> <input type="range" min="100" max="255" class="svelte-1xjvnya8h1aa3"/></label> <div class="baris belok svelte-1xjvnya8h1aa3"><button class="svelte-1xjvnya8h1aa3">⟲ 90° kiri</button> <button class="svelte-1xjvnya8h1aa3">↻ balik</button> <button class="svelte-1xjvnya8h1aa3">90° kanan ⟳</button></div> <p class="ket kecil-ket svelte-1xjvnya8h1aa3">Belok tepat memakai giroskop di badan; sudutnya tidak berubah walau baterai melemah.</p> <div class="baris jelajah svelte-1xjvnya8h1aa3"><!></div> <p class="ket kecil-ket svelte-1xjvnya8h1aa3">Maju sampai ada halangan, lalu belok ke arah acak. Berhenti sendiri kalau waktunya habis, baterai tinggal 10%, atau tombol STOP ditekan. Tombol arah juga menghentikannya.</p></section> <section class="svelte-1xjvnya8h1aa3"><h2 class="svelte-1xjvnya8h1aa3">Baterai <!></h2> <!> <p class="ket kecil-ket svelte-1xjvnya8h1aa3"><!> Peringatan dikirim ke Telegram saat tinggal 20% dan 10%.</p></section> <section class="svelte-1xjvnya8h1aa3"><h2 class="svelte-1xjvnya8h1aa3">Kepala</h2> <label class="svelte-1xjvnya8h1aa3"> <input type="range" min="-60" max="60" class="svelte-1xjvnya8h1aa3"/></label> <label class="svelte-1xjvnya8h1aa3"> <input type="range" min="40" max="150" class="svelte-1xjvnya8h1aa3"/></label> <button class="kecil-btn svelte-1xjvnya8h1aa3">Lurus ke depan</button></section> <section class="svelte-1xjvnya8h1aa3"><h2 class="svelte-1xjvnya8h1aa3">Musik dan suara</h2> <div class="baris svelte-1xjvnya8h1aa3"><button class="svelte-1xjvnya8h1aa3">Lagu berikutnya</button> <button class="svelte-1xjvnya8h1aa3">Radio berikutnya</button> <button class="svelte-1xjvnya8h1aa3">Diam</button></div> <label class="svelte-1xjvnya8h1aa3"> <input type="range" min="0" max="21" class="svelte-1xjvnya8h1aa3"/></label> <label class="suara svelte-1xjvnya8h1aa3">Suara bicara <select class="svelte-1xjvnya8h1aa3"><option class="svelte-1xjvnya8h1aa3">Cowok (Azure)</option><option class="svelte-1xjvnya8h1aa3">Google</option></select></label> <p class="ket kecil-ket svelte-1xjvnya8h1aa3"><!></p> <form class="svelte-1xjvnya8h1aa3"><input type="text" placeholder="Suruh Zond bilang..." maxlength="140" class="svelte-1xjvnya8h1aa3"/> <button type="submit" class="svelte-1xjvnya8h1aa3">Bilang</button></form></section> <section class="svelte-1xjvnya8h1aa3"><h2 class="svelte-1xjvnya8h1aa3">Alarm dan pengingat</h2> <!> <form class="form-alarm svelte-1xjvnya8h1aa3"><div class="baris svelte-1xjvnya8h1aa3"><input type="time" required="" class="svelte-1xjvnya8h1aa3"/> <select class="svelte-1xjvnya8h1aa3"><option class="svelte-1xjvnya8h1aa3">Sekali</option><option class="svelte-1xjvnya8h1aa3">Setiap hari</option><option class="svelte-1xjvnya8h1aa3">Senin-Jumat</option><option class="svelte-1xjvnya8h1aa3">Sabtu-Minggu</option><option class="svelte-1xjvnya8h1aa3">Pilih hari</option></select></div> <!> <input type="text" placeholder="Pesan, misalnya: bangun, minum obat" maxlength="79" class="svelte-1xjvnya8h1aa3"/> <div class="baris svelte-1xjvnya8h1aa3"><select class="svelte-1xjvnya8h1aa3"><option class="svelte-1xjvnya8h1aa3">Suara saja</option><option class="svelte-1xjvnya8h1aa3">Suara + radio</option><option class="svelte-1xjvnya8h1aa3">Suara + lagu SD</option></select> <button type="submit" class="svelte-1xjvnya8h1aa3">Pasang</button></div> <!></form> <p class="ket kecil-ket svelte-1xjvnya8h1aa3">Alarm sebelum jam 11 siang sekalian menyebut cuaca hari ini. Pengingat juga dikirim ke Telegram.</p></section> <section class="svelte-1xjvnya8h1aa3"><h2 class="svelte-1xjvnya8h1aa3">Fitur kamera AI</h2> <label class="saklar svelte-1xjvnya8h1aa3"><input type="checkbox" class="svelte-1xjvnya8h1aa3"/> Aktifkan "lihat sekeliling" dan "suit". Tiap kali dipakai, foto dikirim ke AI dan memakai jatah AI lebih cepat.</label> <div class="baris svelte-1xjvnya8h1aa3"><button class="svelte-1xjvnya8h1aa3">Lihat sekeliling</button> <button class="svelte-1xjvnya8h1aa3">Main suit</button></div> <!> <!></section> <section class="svelte-1xjvnya8h1aa3"><h2 class="svelte-1xjvnya8h1aa3">Otak AI <!></h2> <div class="ai svelte-1xjvnya8h1aa3"></div> <!> <p class="ket kecil-ket svelte-1xjvnya8h1aa3">Kalau pilihanmu gagal (jatah habis atau saldo kosong), robot otomatis memakai yang gratis.</p></section> <!></main>');function su(e,t){fn(t,!0);let r=B(5);async function n(s){await ee(`/api/jelajah?menit=${s}`),await lr()}function o(s){let u=Math.max(0,s);return`${Math.floor(u/60)}:${String(u%60).padStart(2,"0")}`}let i=B(ct([]));async function a(){let s=await ee("/api/baterai");if(s&&s.ok)N(i,(await s.json()).v,!0)}let f=6.4,c=8.4,m=320,h=120;function v(s){let u=(Math.min(c,Math.max(f,s))-f)/(c-f);return h-4-u*(h-8)}let _=to(()=>{let s=l(i).length;if(s<2)return"";return l(i).map((u,g)=>`${g?"L":"M"}${(g/(s-1)*m).toFixed(1)},${v(u/100).toFixed(1)}`).join("")});function d(s){if(s<60)return`${s} menit`;return`${Math.floor(s/60)} jam${s%60?` ${s%60} menit`:""}`}async function y(s){await ee(`/api/suara?pakai=${s.currentTarget.value}`),await lr()}function R(s){ee(`/api/belok?d=${s}`)}let b=["Min","Sen","Sel","Rab","Kam","Jum","Sab"],j=["suara","radio","lagu SD"],Y=B(ct([])),Q=B("06:00"),le=B("0"),pt=ct([!1,!0,!0,!0,!0,!0,!1]),Ee=B(""),ke=B(0),wt=B("");async function Le(){let s=await ee("/api/alarm");if(s&&s.ok)N(Y,(await s.json()).alarm,!0)}async function Ge(s){s.preventDefault();let u=l(le)==="pilih"?pt.reduce((H,F,K)=>F?H|1<<K:H,0):Number(l(le)),g=new URLSearchParams({jam:l(Q),hari:String(u),pesan:l(Ee).trim(),bunyi:String(l(ke))}),I=await ee(`/api/alarm/tambah?${g}`);if(N(wt,!I?"Robot tidak menjawab":I.ok?"":await I.text(),!0),I?.ok)N(Ee,"");await Le()}async function qt(s){await ee(`/api/alarm/hapus?i=${s}`),await Le()}let ar=B("");async function dt(s){let u=s.currentTarget.checked;await ee(`/api/fiturkamera?on=${u?1:0}`),await lr()}async function En(s,u){let g=await ee(s);N(ar,!g?"Robot tidak menjawab":g.ok?u:await g.text(),!0),setTimeout(()=>N(ar,""),6000)}let Jr=B(!1);async function Al(){N(Jr,!0),await ee("/api/bicara"),setTimeout(()=>N(Jr,!1),1500)}async function Sl(s){let u=s.currentTarget.checked;await ee(`/api/dengar?on=${u?1:0}`),await lr()}let Nl=[{id:"otomatis",nama:"Otomatis",ket:"gratis: Gemini, lalu Gemma, lalu Groq"},{id:"gemini",nama:"Gemini",ket:"gratis, jatah harian kecil"},{id:"gemma",nama:"Gemma",ket:"gratis, jatah besar"},{id:"groq",nama:"Groq",ket:"gratis, cepat"},{id:"haiku",nama:"Claude Haiku",ket:"berbayar per pemakaian"}],ho=B(""),Qr=B("");async function Rl(s){N(ho,s,!0),N(Qr,"");let u=await ee(`/api/ai?m=${s}`);if(!u)N(Qr,"Robot tidak menjawab");else if(!u.ok)N(Qr,await u.text(),!0);await lr(),N(ho,"")}let E=B(null),It=B(!1),en=B(160),Tr=B(null),Ar=B(0),tn=B(90),rn=B(12),mo=!1,kn=B(""),sr=B(!0),Tn=B(!1),ca=B("");async function ee(s,u=1500){let g=new AbortController,I=setTimeout(()=>g.abort(),u);try{return await fetch(s,{signal:g.signal,cache:"no-store"})}catch{return null}finally{clearTimeout(I)}}async function lr(){let s=await ee("/api/status");if(!s||!s.ok){N(It,!1);return}if(N(It,!0),N(E,await s.json(),!0),!mo)N(rn,l(E).vol,!0)}let Cl={maju:[1,1],mundur:[-1,-1],kiri:[-1,1],kanan:[1,-1],majukiri:[0.35,1],majukanan:[1,0.35],mundurkiri:[-0.35,-1],mundurkanan:[-1,-0.35]},Sr=null,go=!1;function ua(){if(!l(Tr)||go)return;let[s,u]=Cl[l(Tr)];go=!0,ee(`/api/gerak?l=${Math.round(s*l(en))}&r=${Math.round(u*l(en))}`,600).finally(()=>go=!1)}function pa(s){if(N(Tr,s,!0),ua(),Sr)clearInterval(Sr);Sr=setInterval(ua,150)}function Dt(){if(Sr)clearInterval(Sr);if(Sr=null,l(Tr))ee("/api/stop");N(Tr,null)}function jl(s,u){s.currentTarget.setPointerCapture(s.pointerId),pa(u)}let da={ArrowUp:"maju",w:"maju",ArrowDown:"mundur",s:"mundur",ArrowLeft:"kiri",a:"kiri",ArrowRight:"kanan",d:"kanan"};function Ol(s){if(s.target.tagName==="INPUT")return;if(s.key===" "){s.preventDefault(),Dt(),ee("/api/stop");return}let u=da[s.key];if(u&&!s.repeat)s.preventDefault(),pa(u)}function Il(s){if(da[s.key])Dt()}let _a=0,xo=null;function bo(){let s=Date.now();if(xo)clearTimeout(xo);let u=()=>{_a=Date.now(),ee(`/api/kepala?p=${90-l(Ar)}&t=${l(tn)}`)};if(s-_a>150)u();else xo=setTimeout(u,150)}function Dl(){N(Ar,0),N(tn,90),bo()}async function Ml(){mo=!0,await ee(`/api/volume?v=${l(rn)}`),setTimeout(()=>mo=!1,1500)}async function Ll(s){s.preventDefault();let u=l(kn).trim();if(!u)return;N(kn,""),await ee(`/api/bilang?t=${encodeURIComponent(u)}`,3000)}function An(s){setTimeout(()=>{if(!l(sr)||!l(E)?.kamera){if(l(sr))An(1000);return}N(ca,`http://${l(E).kamera}/capture?res=vga&t=${Date.now()}`)},s)}function Fl(s){let u=[];if(s&1)u.push("Tombol STOP terkunci");if(s&2)u.push("Baterai lemah");if(s&4)u.push("Halangan depan");if(s&8)u.push("Halangan belakang");if(s&16)u.push("Robot miring");return u}wl(()=>{lr(),Le(),a();let s=setInterval(lr,1000),u=setInterval(Le,30000),g=setInterval(a,60000),I=()=>Dt(),H=()=>document.hidden&&Dt();return window.addEventListener("blur",I),document.addEventListener("visibilitychange",H),()=>{clearInterval(s),clearInterval(u),clearInterval(g),window.removeEventListener("blur",I),document.removeEventListener("visibilitychange",H),Dt()}}),qr(()=>{if(l(sr))An(0)});var yo=au();$t("keydown",Ur,Ol),$t("keyup",Ur,Il);var $o=O(yo),va=O($o),Pl=J(va,!0),ha=x(va,2);let ma;var ga=x(ha,2),Bl=O(ga);{var Yl=(s)=>{var u=Vt("otak tidak terhubung");D(s,u)},Hl=(s)=>{var u=Vt("badan tidak menjawab");D(s,u)},Vl=(s)=>{var u=Vt();V((g)=>L(u,`${l(E).bateraiPersen>=0?`${l(E).bateraiPersen}% · `:""}${g??""} V · depan ${l(E).depan??""} cm · belakang ${l(E).belakang??""} cm`),[()=>l(E).baterai.toFixed(1)]),D(s,u)};ne(Bl,(s)=>{if(!l(It))s(Yl);else if(!l(E)?.hidup)s(Hl,1);else s(Vl,-1)})}A(ga),A($o);var xa=x($o,2);{var Ul=(s)=>{var u=Hc(),g=O(u),I=x(g);{var H=(F)=>{var K=Yc(),Te=x(Ht(K),1,!0);V(()=>L(Te,l(E).cuaca)),D(F,K)};ne(I,(F)=>{if(l(E).cuaca)F(H)})}A(u),V(()=>L(g,`${l(E).jam??""} · ${l(E).tanggal??""}`)),D(s,u)};ne(xa,(s)=>{if(l(E)?.jam)s(Ul)})}var ba=x(xa,2);{var ql=(s)=>{var u=Uc();or(u,20,()=>Fl(l(E).flag),(g)=>g,(g,I)=>{var H=Vc(),F=J(H,!0);V(()=>L(F,I)),D(g,H)}),A(u),D(s,u)};ne(ba,(s)=>{if(l(E)&&l(E).flag)s(ql)})}var wo=x(ba,2),nn=O(wo);let ya;var Gl=J(nn,!0),$a=x(nn,4),on=O($a);ut(on);var zl=x(on);A($a),A(wo);var Eo=x(wo,2),wa=O(Eo);{var Kl=(s)=>{var u=qc(),g=Ht(u);let I;var H=x(g,2);{var F=(K)=>{var Te=kr(),Ae=J(Te);V(()=>L(Ae,`Kamera tidak terhubung (${l(E).kamera??""})`)),D(K,Te)};ne(H,(K)=>{if(l(Tn))K(F)})}V(()=>{rt(g,"src",l(ca)),I=ir(g,1,"svelte-1xjvnya8h1aa3",null,I,{redup:l(Tn)})}),$t("load",g,()=>{N(Tn,!1),An(100)}),$t("error",g,()=>{N(Tn,!0),An(1500)}),Gi(g),D(s,u)},Wl=(s)=>{var u=kr(),g=J(u,!0);V(()=>L(g,l(E)?.kamera?"Kamera dimatikan":"Alamat kamera belum diketahui")),D(s,u)};ne(wa,(s)=>{if(l(sr)&&l(E)?.kamera)s(Kl);else s(Wl,-1)})}var Ea=x(wa,2),Zl=J(Ea,!0);A(Eo);var ko=x(Eo,2),Sn=x(O(ko),2);or(Sn,20,()=>[["majukiri","↖"],["maju","▲"],["majukanan","↗"],["kiri","◀"],["stop","■"],["kanan","▶"],["mundurkiri","↙"],["mundur","▼"],["mundurkanan","↘"]],([s,u])=>s,(s,u)=>{var g=to(()=>pi(u,2));let I=()=>l(g)[0],H=()=>l(g)[1];var F=Wi(),K=Ht(F);{var Te=(he)=>{var je=Gc(),_t=J(je,!0);V(()=>L(_t,H())),z("pointerdown",je,()=>{Dt(),ee("/api/stop")}),D(he,je)},Ae=(he)=>{var je=zc();let _t;var fr=J(je,!0);V(()=>{_t=ir(je,1,"svelte-1xjvnya8h1aa3",null,_t,{aktif:l(Tr)===I()}),L(fr,H())}),z("pointerdown",je,(si)=>jl(si,I())),z("pointerup",je,Dt),$t("pointercancel",je,Dt),$t("lostpointercapture",je,Dt),D(he,je)};ne(K,(he)=>{if(I()==="stop")he(Te);else he(Ae,-1)})}D(s,F)}),A(Sn);var To=x(Sn,2),ka=O(To),Ta=x(ka);ut(Ta),A(To);var Ao=x(To,2),Aa=O(Ao),Sa=x(Aa,2),Xl=x(Sa,2);A(Ao);var Na=x(Ao,4),Jl=O(Na);{var Ql=(s)=>{var u=Kc(),g=J(u);V((I)=>L(g,`Berhenti menjelajah (sisa ${I??""})`),[()=>o(l(E).jelajahSisa)]),z("click",u,()=>n(0)),D(s,u)},ef=(s)=>{var u=Wc(),g=Ht(u),I=O(g);I.value=I.__value=5;var H=x(I);H.value=H.__value=10;var F=x(H);F.value=F.__value=15;var K=x(F);K.value=K.__value=30,A(g),Er(g);var Te=x(g,2);V(()=>Te.disabled=!l(It)||!l(E)?.hidup),wn(g,()=>l(r),(Ae)=>N(r,Ae)),z("click",Te,()=>n(l(r))),D(s,u)};ne(Jl,(s)=>{if(l(E)?.mode==="jelajah")s(Ql);else s(ef,-1)})}A(Na),at(2),A(ko);var So=x(ko,2),No=O(So),tf=x(O(No));{var rf=(s)=>{var u=kl(),g=J(u);V((I)=>L(g,`${l(E).bateraiPersen??""}% · ${I??""} V`),[()=>l(E).baterai.toFixed(2)]),D(s,u)};ne(tf,(s)=>{if(l(E)&&l(E).bateraiPersen>=0)s(rf)})}A(No);var Ra=x(No,2);{var nf=(s)=>{var u=Xc(),g=Ht(u);rt(g,"viewBox","0 0 320 120");var I=O(g);or(I,16,()=>[8.4,7.8,7.2,6.6],(Ae)=>Ae,(Ae,he)=>{var je=Zc(),_t=Ht(je);rt(_t,"x2",m);var fr=x(_t),si=J(fr);V((Sf,Nf,Rf,Cf)=>{rt(_t,"y1",Sf),rt(_t,"y2",Nf),rt(fr,"y",Rf),L(si,`${Cf??""} V`)},[()=>v(he),()=>v(he),()=>he===c?v(he)+11:v(he)-3,()=>he.toFixed(1)]),D(Ae,je)});var H=x(I);A(g);var F=x(g,2),K=O(F),Te=J(K);at(),A(F),V((Ae,he)=>{rt(g,"aria-label",`Tegangan baterai ${Ae??""} terakhir`),rt(H,"d",l(_)),L(Te,`${he??""} lalu`)},[()=>d(l(i).length),()=>d(l(i).length)]),D(s,u)},of=(s)=>{var u=Jc();D(s,u)};ne(Ra,(s)=>{if(l(i).length>=2)s(nf);else s(of,-1)})}var Ca=x(Ra,2),af=O(Ca);{var sf=(s)=>{var u=Vt();V((g)=>L(u,`Perkiraan sisa ${g??""}, kalau pemakaiannya seperti setengah jam terakhir.`),[()=>d(l(E).sisaMenit)]),D(s,u)},lf=(s)=>{var u=Vt("Perkiraan sisa waktu muncul setelah sekitar 10 menit menyala.");D(s,u)};ne(af,(s)=>{if(l(E)&&l(E).sisaMenit>=0)s(sf);else s(lf,-1)})}at(),A(Ca),A(So);var Ro=x(So,2),Co=x(O(Ro),2),ja=O(Co),jo=x(ja);ut(jo),A(Co);var Oo=x(Co,2),Oa=O(Oo),Io=x(Oa);ut(Io),A(Oo);var ff=x(Oo,2);A(Ro);var Do=x(Ro,2),Mo=x(O(Do),2),Ia=O(Mo),Da=x(Ia,2),cf=x(Da,2);A(Mo);var Lo=x(Mo,2),Ma=O(Lo),Fo=x(Ma);ut(Fo),A(Lo);var Po=x(Lo,2),Gt=x(O(Po)),Nn=O(Gt);Nn.value=Nn.__value="azure";var La=x(Nn);La.value=La.__value="google",A(Gt);var Rn;Er(Gt),A(Po);var Bo=x(Po,2),uf=O(Bo);{var pf=(s)=>{var u=Vt("Suara cowok belum aktif: isi AZURE_KUNCI di kode.");D(s,u)},df=(s)=>{var u=Qc(),g=Ht(u),I=x(g);{var H=(F)=>{var K=Vt();V(()=>L(K,`Galat terakhir: kode ${l(E).azureGalat??""}, sementara memakai suara Google.`)),D(F,K)};ne(I,(F)=>{if(l(E).azureGalat)F(H)})}V((F)=>L(g,`Azure bulan ini ${F??""} dari 500.000 huruf gratis.`),[()=>l(E).azureHuruf.toLocaleString("id-ID")]),D(s,u)};ne(uf,(s)=>{if(!l(E)?.azureSiap)s(pf);else s(df,-1)})}A(Bo);var Yo=x(Bo,2),Fa=O(Yo);ut(Fa),at(2),A(Yo),A(Do);var Ho=x(Do,2),Pa=x(O(Ho),2);{var _f=(s)=>{var u=tu();or(u,21,()=>l(Y),ra,(g,I,H)=>{var F=eu(),K=O(F),Te=J(K,!0),Ae=x(K,2),he=O(Ae,!0),je=x(he),_t=J(je);A(Ae);var fr=x(Ae,2);A(F),V(()=>{L(Te,l(I).jam),L(he,l(I).pesan),L(_t,`${l(I).kapan??""} · ${j[l(I).bunyi]??"suara"??""}`),rt(fr,"aria-label",`Hapus alarm ${l(I).jam??""}`)}),z("click",fr,()=>qt(H)),D(g,F)}),A(u),D(s,u)},vf=(s)=>{var u=kr(),g=J(u);V(()=>L(g,`Belum ada alarm. Bisa juga lewat suara: "${l(E)?.nama??"Zond"??""}, ingatkan aku jam 5 sore minum obat".`)),D(s,u)};ne(Pa,(s)=>{if(l(Y).length)s(_f);else s(vf,-1)})}var Vo=x(Pa,2),Uo=O(Vo),qo=O(Uo);ut(qo);var Cn=x(qo,2),Go=O(Cn);Go.value=Go.__value="0";var zo=x(Go);zo.value=zo.__value="127";var Ko=x(zo);Ko.value=Ko.__value="62";var Wo=x(Ko);Wo.value=Wo.__value="65";var Ba=x(Wo);Ba.value=Ba.__value="pilih",A(Cn),Er(Cn),A(Uo);var Ya=x(Uo,2);{var hf=(s)=>{var u=nu();or(u,22,()=>b,(g)=>g,(g,I,H)=>{var F=ru(),K=O(F);ut(K);var Te=x(K,1,!0);A(F),V(()=>L(Te,I)),sa(K,()=>pt[l(H)],(Ae)=>pt[l(H)]=Ae),D(g,F)}),A(u),D(s,u)};ne(Ya,(s)=>{if(l(le)==="pilih")s(hf)})}var Zo=x(Ya,2);ut(Zo);var Xo=x(Zo,2),jn=O(Xo),Jo=O(jn);Jo.value=Jo.__value=0;var Qo=x(Jo);Qo.value=Qo.__value=1;var Ha=x(Qo);Ha.value=Ha.__value=2,A(jn),Er(jn),at(2),A(Xo);var mf=x(Xo,2);{var gf=(s)=>{var u=kr(),g=J(u,!0);V(()=>L(g,l(wt))),D(s,u)};ne(mf,(s)=>{if(l(wt))s(gf)})}A(Vo),at(2),A(Ho);var ei=x(Ho,2),ti=x(O(ei),2),On=O(ti);ut(On),at(),A(ti);var ri=x(ti,2),ni=O(ri),Va=x(ni,2);A(ri);var Ua=x(ri,2);{var xf=(s)=>{var u=kr(),g=J(u);V(()=>L(g,`Skor suit: kamu ${l(E).skorKamu??""}, ${l(E).nama??""} ${l(E).skorZond??""}`)),D(s,u)};ne(Ua,(s)=>{if(l(E)?.fiturKamera&&(l(E).skorKamu||l(E).skorZond))s(xf)})}var bf=x(Ua,2);{var yf=(s)=>{var u=kr(),g=J(u,!0);V(()=>L(g,l(ar))),D(s,u)};ne(bf,(s)=>{if(l(ar))s(yf)})}A(ei);var oi=x(ei,2),ii=O(oi),$f=x(O(ii));{var wf=(s)=>{var u=kl(),g=J(u);V(()=>L(g,`terakhir menjawab: ${l(E).aiTerakhir??""}`)),D(s,u)};ne($f,(s)=>{if(l(E)?.aiTerakhir)s(wf)})}A(ii);var ai=x(ii,2);or(ai,21,()=>Nl,(s)=>s.id,(s,u)=>{var g=ou();let I;var H=O(g),F=J(H,!0),K=x(H,2),Te=J(K,!0);A(g),V(()=>{g.disabled=!l(E)?.aiTersedia?.[l(u).id]||l(ho)!=="",I=ir(g,1,"svelte-1xjvnya8h1aa3",null,I,{pilih:l(E)?.ai===l(u).id}),L(F,l(u).nama),L(Te,l(E)?.aiTersedia?.[l(u).id]===!1?"API key belum diisi":l(u).ket)}),z("click",g,()=>Rl(l(u).id)),D(s,g)}),A(ai);var Ef=x(ai,2);{var kf=(s)=>{var u=kr(),g=J(u,!0);V(()=>L(g,l(Qr))),D(s,u)};ne(Ef,(s)=>{if(l(Qr))s(kf)})}at(2),A(oi);var Tf=x(oi,2);{var Af=(s)=>{var u=iu(),g=J(u,!0);V(()=>L(g,l(E).status)),D(s,u)};ne(Tf,(s)=>{if(l(E)?.status)s(Af)})}A(yo),V(()=>{if(L(Pl,l(E)?.nama??"Robot"),ma=ir(ha,1,"titik svelte-1xjvnya8h1aa3",null,ma,{on:l(It)&&l(E)?.hidup}),ya=ir(nn,1,"mik svelte-1xjvnya8h1aa3",null,ya,{dengar:l(E)?.mendengar||l(Jr)}),nn.disabled=!l(It),L(Gl,l(E)?.mendengar?"Mendengarkan... bicara sekarang":"Bicara ke "+(l(E)?.nama??"robot")),_o(on,l(E)?.dengarTerus??!1),on.disabled=!l(It),L(zl,` Dengar terus (cukup sebut "${l(E)?.nama??"nama robot"??""}"). Boros: setiap suara di ruangan memakai jatah suara-ke-teks.`),L(Zl,l(sr)?"Matikan kamera":"Nyalakan kamera"),L(ka,`Kecepatan ${l(en)??""}`),L(ja,`Toleh ${l(Ar)<0?"kiri":l(Ar)>0?"kanan":"lurus"}`),L(Oa,`Angguk ${l(tn)??""}°`),L(Ma,`Volume ${l(rn)??""}`),Gt.disabled=!l(It),Nn.disabled=!l(E)?.azureSiap,Rn!==(Rn=l(E)?.suara??"azure"))Gt.value=(Gt.__value=Rn)??"",Xr(Gt,Rn);_o(On,l(E)?.fiturKamera??!1),On.disabled=!l(It),ni.disabled=!l(E)?.fiturKamera,Va.disabled=!l(E)?.fiturKamera}),z("click",nn,Al),z("change",on,Sl),z("click",Ea,()=>N(sr,!l(sr))),z("contextmenu",Sn,(s)=>s.preventDefault()),Ut(Ta,()=>l(en),(s)=>N(en,s)),z("click",Aa,()=>R(-90)),z("click",Sa,()=>R(180)),z("click",Xl,()=>R(90)),z("input",jo,bo),Ut(jo,()=>l(Ar),(s)=>N(Ar,s)),z("input",Io,bo),Ut(Io,()=>l(tn),(s)=>N(tn,s)),z("click",ff,Dl),z("click",Ia,()=>ee("/api/lagu",4000)),z("click",Da,()=>ee("/api/radio",4000)),z("click",cf,()=>ee("/api/diam")),z("change",Fo,Ml),Ut(Fo,()=>l(rn),(s)=>N(rn,s)),z("change",Gt,y),$t("submit",Yo,Ll),Ut(Fa,()=>l(kn),(s)=>N(kn,s)),$t("submit",Vo,Ge),Ut(qo,()=>l(Q),(s)=>N(Q,s)),wn(Cn,()=>l(le),(s)=>N(le,s)),Ut(Zo,()=>l(Ee),(s)=>N(Ee,s)),wn(jn,()=>l(ke),(s)=>N(ke,s)),z("change",On,dt),z("click",ni,()=>En("/api/sekeliling","Zond menoleh dan memotret. Ceritanya dikirim ke Telegram.")),z("click",Va,()=>En("/api/suit","Siapkan tanganmu di depan kamera!")),D(e,yo),cn()}var Tl=su;fo(["click","change","contextmenu","pointerdown","pointerup","input"]);Wr(Tl,{target:document.body});
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
    d["suara"] = pakaiAzure ? "azure" : "google"; d["azureSiap"] = strlen(AZURE_KUNCI) > 0 && adaFFat;
    d["azureHuruf"] = (uint32_t)hurufAzure; d["azureGalat"] = (int)azureGalat;
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
  web.on("/api/jelajah", HTTP_GET, [] { jelajah(argAngka("menit", 5, 0, 60)); jawabOk(); });   // menit=0: berhenti
  web.on("/api/suara", HTTP_GET, [] { aturSuara(web.arg("pakai") != "google"); jawabOk(); });
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
  pakaiAzure = pref.getUChar("suara", 1) != 0;
  hurufAzure = pref.getUInt("azH", 0);
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
  // Penyimpanan di flash untuk file suara Azure. Pertama kali diformat dulu (beberapa detik, sekali saja).
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
