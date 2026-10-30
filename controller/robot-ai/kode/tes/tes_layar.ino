// TES LAYAR + SENTUHAN: layar menampilkan warna dan tulisan, lalu titik mengikuti jarimu.
// Library: LovyanGFX
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

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
};

LGFX lcd;

void setup() {
  Serial.begin(115200);
  delay(1500);   // beri waktu Serial Monitor tersambung
  Serial.println("\n=== TES LAYAR ===");
  lcd.init();
  lcd.setRotation(1);                       // mendatar, 320 x 240
  lcd.fillScreen(TFT_RED);   delay(400);
  lcd.fillScreen(TFT_GREEN); delay(400);
  lcd.fillScreen(TFT_BLUE);  delay(400);
  lcd.fillScreen(TFT_BLACK);
  lcd.setTextColor(TFT_WHITE);
  lcd.setFont(&fonts::Font4);
  lcd.drawString("Layar berhasil!", 20, 20);
  lcd.setFont(&fonts::Font2);
  lcd.drawString("Sentuh 4 ujung panah untuk kalibrasi", 20, 70);
  delay(1500);
  uint16_t cal[8];
  lcd.calibrateTouch(cal, TFT_WHITE, TFT_BLACK, 15);
  lcd.fillScreen(TFT_BLACK);
  lcd.drawString("Gambar dengan jarimu", 20, 10);
}

void loop() {
  int32_t x, y;
  if (lcd.getTouch(&x, &y)) {
    lcd.fillCircle(x, y, 4, TFT_YELLOW);
    Serial.printf("sentuh x=%d y=%d\n", (int)x, (int)y);
  }
}
