// TES WIFI + TELEGRAM: bot membalas setiap pesanmu. Library: UniversalTelegramBot, ArduinoJson
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

const char* WIFI_NAMA      = "NAMA_WIFI";
const char* WIFI_PASSWORD  = "PASSWORD_WIFI";
const char* TOKEN          = "ISI_TOKEN_DARI_BOTFATHER";
const char* CHAT_ID        = "ISI_ANGKA_ID_KAMU";

WiFiClientSecure client;
UniversalTelegramBot bot(TOKEN, client);

void setup() {
  Serial.begin(115200);
  delay(1500);   // beri waktu Serial Monitor tersambung
  Serial.println("\n=== TES TELEGRAM ===");
  WiFi.begin(WIFI_NAMA, WIFI_PASSWORD);
  Serial.print("WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.print("\nTersambung, IP "); Serial.println(WiFi.localIP());
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  bool ok = bot.sendMessage(CHAT_ID, "Halo dari ESP32-S3! Kirim pesan apa saja.");
  Serial.println(ok ? "Pesan terkirim ke Telegram" : "GAGAL kirim. Cek token dan chat ID");
}

void loop() {
  int n = bot.getUpdates(bot.last_message_received + 1);
  for (int i = 0; i < n; i++) {
    Serial.println("Pesan masuk: " + bot.messages[i].text);
    bot.sendMessage(bot.messages[i].chat_id, "Kamu bilang: " + bot.messages[i].text);
  }
  delay(1000);
}
