// TES GEMINI API: ketik pertanyaan di Serial Monitor, jawaban AI muncul di bawahnya.
// Library: ArduinoJson. Butuh API key gratis dari aistudio.google.com (diawali AIza).
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const char* WIFI_NAMA     = "NAMA_WIFI";
const char* WIFI_PASSWORD = "PASSWORD_WIFI";
const char* API_KEY       = "ISI_API_KEY_GEMINI";
const char* MODEL         = "gemini-2.5-flash-lite";   // samakan dengan GEMINI_MODEL di robot_otak

String tanya(const String& q) {
  JsonDocument doc;
  doc["system_instruction"]["parts"][0]["text"] = "Jawab dalam Bahasa Indonesia, singkat, paling banyak 2 kalimat.";
  JsonObject m = doc["contents"].to<JsonArray>().add<JsonObject>();
  m["role"] = "user"; m["parts"][0]["text"] = q;
  String body; serializeJson(doc, body);

  WiFiClientSecure c; c.setInsecure();     // terenkripsi, tapi sertifikat server tidak diperiksa
  HTTPClient http;
  http.begin(c, String("https://generativelanguage.googleapis.com/v1beta/models/") + MODEL + ":generateContent");
  http.addHeader("content-type", "application/json");
  http.addHeader("x-goog-api-key", API_KEY);
  http.setTimeout(30000);
  int code = http.POST(body);
  String resp = http.getString();
  http.end();
  if (code != 200) return "GAGAL (kode " + String(code) + "): " + resp;
  JsonDocument r; deserializeJson(r, resp);
  String teks;
  for (JsonObject p : r["candidates"][0]["content"]["parts"].as<JsonArray>())
    if (!(p["thought"] | false)) teks += p["text"] | "";
  return teks.length() ? teks : "(kosong): " + resp;
}

void setup() {
  Serial.begin(115200);
  delay(1500);   // beri waktu Serial Monitor tersambung
  Serial.println("\n=== TES GEMINI ===");
  WiFi.begin(WIFI_NAMA, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nSiap. Ketik pertanyaan lalu Enter.");
}

void loop() {
  if (Serial.available()) {
    String q = Serial.readStringUntil('\n'); q.trim();
    if (q.length()) { Serial.println("Kamu: " + q); Serial.println("AI  : " + tanya(q)); }
  }
}
