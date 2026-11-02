/*
  ROBOT AI 4WD - MATA (ESP32-CAM AI-Thinker)
  -------------------------------------------
  Menyambung ke WiFi rumah lalu menyediakan foto lewat alamat web:
    http://robotcam.local/capture          foto JPEG 640x480
    http://robotcam.local/capture?res=qvga foto 320x240 (untuk layar robot)
    http://robotcam.local/stream           video MJPEG (buka di browser)
    http://robotcam.local/flash?on=1       nyalakan LED flash (on=0 mematikan)
    http://robotcam.local/status           info singkat
  Kalau "robotcam.local" tidak bisa dibuka (sering di HP Android), pakai alamat IP
  yang dicetak di Serial Monitor saat menyala.

  Board di Arduino IDE : AI Thinker ESP32-CAM
  Partition Scheme     : Huge APP (3MB No OTA/1MB SPIFFS)
  Upload lewat papan ESP32-CAM-MB (port micro-USB). Tidak perlu library tambahan.
  Kamera OV2640 dan OV3660 sama-sama didukung (dideteksi otomatis).
*/

#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

// ================== ISI DENGAN WIFI RUMAHMU (2.4 GHz) ==================
const char* WIFI_NAMA     = "NAMA_WIFI";
const char* WIFI_PASSWORD = "PASSWORD_WIFI";
const char* NAMA_MDNS     = "robotcam";        // jadi robotcam.local
const bool  KAMERA_TERBALIK = false;           // true kalau gambar terbalik atas-bawah

// ================== PIN KAMERA AI-THINKER (jangan diubah) ==================
#define PWDN_GPIO_NUM  32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM   0
#define SIOD_GPIO_NUM  26
#define SIOC_GPIO_NUM  27
#define Y9_GPIO_NUM    35
#define Y8_GPIO_NUM    34
#define Y7_GPIO_NUM    39
#define Y6_GPIO_NUM    36
#define Y5_GPIO_NUM    21
#define Y4_GPIO_NUM    19
#define Y3_GPIO_NUM    18
#define Y2_GPIO_NUM     5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM  23
#define PCLK_GPIO_NUM  22
#define LED_FLASH       4

WebServer server(80);
SemaphoreHandle_t kunciKamera;

bool mulaiKamera() {
  camera_config_t c = {};
  c.ledc_channel = LEDC_CHANNEL_0;  c.ledc_timer = LEDC_TIMER_0;
  c.pin_d0 = Y2_GPIO_NUM; c.pin_d1 = Y3_GPIO_NUM; c.pin_d2 = Y4_GPIO_NUM; c.pin_d3 = Y5_GPIO_NUM;
  c.pin_d4 = Y6_GPIO_NUM; c.pin_d5 = Y7_GPIO_NUM; c.pin_d6 = Y8_GPIO_NUM; c.pin_d7 = Y9_GPIO_NUM;
  c.pin_xclk = XCLK_GPIO_NUM; c.pin_pclk = PCLK_GPIO_NUM;
  c.pin_vsync = VSYNC_GPIO_NUM; c.pin_href = HREF_GPIO_NUM;
  c.pin_sccb_sda = SIOD_GPIO_NUM; c.pin_sccb_scl = SIOC_GPIO_NUM;
  c.pin_pwdn = PWDN_GPIO_NUM; c.pin_reset = RESET_GPIO_NUM;
  c.xclk_freq_hz = 20000000;
  c.pixel_format = PIXFORMAT_JPEG;
  c.frame_size = FRAMESIZE_VGA;
  c.jpeg_quality = 12;                 // 10-15: makin kecil makin bagus, file makin besar
  c.fb_count = 2;
  c.fb_location = CAMERA_FB_IN_PSRAM;
  c.grab_mode = CAMERA_GRAB_LATEST;    // selalu ambil gambar terbaru
  if (!psramFound()) { c.fb_count = 1; c.fb_location = CAMERA_FB_IN_DRAM; c.frame_size = FRAMESIZE_QVGA; }

  esp_err_t err = esp_camera_init(&c);
  if (err != ESP_OK) { Serial.printf("Kamera gagal mulai, kode 0x%x\n", err); return false; }

  sensor_t* s = esp_camera_sensor_get();
  Serial.printf("Sensor kamera terdeteksi, PID 0x%x (%s)\n", s->id.PID,
                s->id.PID == OV2640_PID ? "OV2640" : s->id.PID == OV3660_PID ? "OV3660" :
                s->id.PID == OV5640_PID ? "OV5640" : "lainnya");
  if (s->id.PID == OV3660_PID) { s->set_brightness(s, 1); s->set_saturation(s, -1); }
  if (KAMERA_TERBALIK) { s->set_vflip(s, 1); s->set_hmirror(s, 1); }
  return true;
}

framesize_t ukuranDariParam() {
  String r = server.hasArg("res") ? server.arg("res") : "vga";
  if (r == "qvga") return FRAMESIZE_QVGA;   // 320x240
  if (r == "svga") return FRAMESIZE_SVGA;   // 800x600
  if (r == "hd")   return FRAMESIZE_HD;     // 1280x720
  return FRAMESIZE_VGA;                     // 640x480
}

void handleCapture() {
  xSemaphoreTake(kunciKamera, portMAX_DELAY);
  sensor_t* s = esp_camera_sensor_get();
  framesize_t f = ukuranDariParam();
  if (s->status.framesize != f) {
    s->set_framesize(s, f);
    for (int i = 0; i < 2; i++) { camera_fb_t* b = esp_camera_fb_get(); if (b) esp_camera_fb_return(b); } // buang gambar lama
  }
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) { xSemaphoreGive(kunciKamera); server.send(503, "text/plain", "gagal ambil foto"); return; }
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Cache-Control", "no-store");
  server.setContentLength(fb->len);
  server.send(200, "image/jpeg", "");
  server.client().write(fb->buf, fb->len);
  esp_camera_fb_return(fb);
  xSemaphoreGive(kunciKamera);
}

void handleStream() {
  WiFiClient cl = server.client();
  cl.print("HTTP/1.1 200 OK\r\nContent-Type: multipart/x-mixed-replace; boundary=frame\r\nCache-Control: no-store\r\n\r\n");
  while (cl.connected()) {
    xSemaphoreTake(kunciKamera, portMAX_DELAY);
    camera_fb_t* fb = esp_camera_fb_get();
    if (fb) {
      cl.printf("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", fb->len);
      cl.write(fb->buf, fb->len);
      cl.print("\r\n");
      esp_camera_fb_return(fb);
    }
    xSemaphoreGive(kunciKamera);
    delay(30);
  }
}

void handleFlash() {
  bool on = server.hasArg("on") && server.arg("on") == "1";
  analogWrite(LED_FLASH, on ? 60 : 0);     // 60/255: cukup terang, tidak terlalu panas
  server.send(200, "text/plain", on ? "flash nyala" : "flash mati");
}

void handleStatus() {
  String j = "{\"ip\":\"" + WiFi.localIP().toString() + "\",\"rssi\":" + String(WiFi.RSSI()) +
             ",\"psram\":" + String(ESP.getPsramSize()) + ",\"uptime_s\":" + String(millis() / 1000) + "}";
  server.send(200, "application/json", j);
}

void handleRoot() {
  server.send(200, "text/html",
    "<meta name=viewport content='width=device-width'><h3>Mata robot</h3>"
    "<p><a href=/capture>Foto</a> | <a href=/stream>Video</a> | "
    "<a href='/flash?on=1'>Flash nyala</a> | <a href='/flash?on=0'>Flash mati</a></p>"
    "<img src=/capture style='max-width:100%'>");
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== MATA ROBOT ===");
  pinMode(LED_FLASH, OUTPUT); analogWrite(LED_FLASH, 0);
  kunciKamera = xSemaphoreCreateMutex();
  if (!mulaiKamera()) {
    Serial.println("Cek: pita kamera terpasang rapat dan kuncinya ditekan, daya 5V cukup kuat.");
    delay(5000); ESP.restart();
  }
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);                 // respon lebih cepat
  WiFi.begin(WIFI_NAMA, WIFI_PASSWORD);
  Serial.print("Menyambung WiFi");
  for (int i = 0; WiFi.status() != WL_CONNECTED; i++) {
    delay(500); Serial.print(".");
    if (i > 40) { Serial.println("\nWiFi gagal. Cek nama/password dan pastikan 2.4 GHz."); delay(3000); ESP.restart(); }
  }
  Serial.println();
  Serial.print("Tersambung. Alamat kamera: http://"); Serial.println(WiFi.localIP());
  if (MDNS.begin(NAMA_MDNS)) { MDNS.addService("http", "tcp", 80); Serial.printf("Atau: http://%s.local\n", NAMA_MDNS); }

  server.on("/", handleRoot);
  server.on("/capture", handleCapture);
  server.on("/stream", handleStream);
  server.on("/flash", handleFlash);
  server.on("/status", handleStatus);
  server.begin();
}

void loop() {
  server.handleClient();
  if (WiFi.status() != WL_CONNECTED) {      // WiFi putus: coba lagi
    static unsigned long t = 0;
    if (millis() - t > 10000) { t = millis(); WiFi.reconnect(); }
  }
  delay(2);
}
