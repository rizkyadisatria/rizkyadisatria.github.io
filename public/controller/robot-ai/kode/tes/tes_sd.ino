// TES SD CARD: daftar file di SD (SD memakai kabel SPI yang sama dengan layar).
// Format SD sebagai FAT32. Library: LovyanGFX (SD sudah bawaan).
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <SPI.h>
#include <SD.h>
#include <Preferences.h>

#define TFT_SCK   12
#define TFT_MOSI  11
#define TFT_MISO  13
#define TFT_CS    10
#define TFT_DC     9
#define TFT_RST    8
#define TOUCH_CS  14
#define SD_CS     21

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

bool sdOk = false;

void setup() {
  Serial.begin(115200);
  delay(1500);   // beri waktu Serial Monitor tersambung
  Serial.println("\n=== TES SD ===");
  Preferences pref; pref.begin("robot", false);
  uint8_t arah = pref.getUChar("rot", 6), geo = pref.getUChar("geo", 1), rgb = pref.getUChar("rgb", 1);
  pref.end();                                                     // pilihan di tes_layar
  if (arah > 7) arah = 6;
  lcd.aturPanel(geo != 0, rgb != 0);
  lcd.init(); lcd.setRotation(arah); lcd.fillScreen(TFT_BLACK);   // layar dulu, baru SD
  lcd.setTextColor(TFT_WHITE); lcd.setFont(&fonts::Font2);
  SPI.begin(TFT_SCK, TFT_MISO, TFT_MOSI);
  if (!SD.begin(SD_CS, SPI, 20000000)) {
    Serial.println("SD GAGAL. Cek kabel SD_CS ke GPIO 21 dan format FAT32.");
    lcd.drawString("SD GAGAL", 10, 10);
    return;
  }
  sdOk = true;
  Serial.printf("SD terbaca, ukuran %llu MB\n", SD.cardSize() / 1048576);
  lcd.drawString("SD terbaca. Isi folder utama:", 10, 10);
  File root = SD.open("/");
  int y = 34;
  for (File f = root.openNextFile(); f; f = root.openNextFile()) {
    Serial.printf("  %s  (%u byte)\n", f.name(), f.size());
    if (y < 230) { lcd.drawString(f.name(), 16, y); y += 18; }
  }
}

void loop() {   // ulangi hasilnya supaya tetap terlihat walau Serial Monitor dibuka terlambat
  Serial.println(sdOk ? "SD terbaca. Daftar file dicetak sekali di awal; tekan RST untuk melihatnya lagi."
                      : "SD GAGAL. Cek kabel SD_CS ke GPIO 21 dan format FAT32.");
  delay(3000);
}
