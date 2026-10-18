// TES CLAUDE API: ketik pertanyaan di Serial Monitor, jawaban AI muncul di bawahnya.
// Library: ArduinoJson. Butuh API key dari console Anthropic (berbayar per pemakaian).
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const char* WIFI_NAMA     = "NAMA_WIFI";
const char* WIFI_PASSWORD = "PASSWORD_WIFI";
const char* API_KEY       = "sk-ant-ISI_API_KEY";
const char* MODEL         = "claude-haiku-5-5";

String tanya(const String& q) {
  JsonDocument doc;
  doc["model"] = MODEL;
  doc["max_tokens"] = 200;
  doc["system"] = "Jawab dalam Bahasa Indonesia, singkat, paling banyak 2 kalimat.";
  JsonObject m = doc["messages"].to<JsonArray>().add<JsonObject>();
  m["role"] = "user"; m["content"] = q;
  String body; serializeJson(doc, body);

  WiFiClientSecure c; c.setInsecure();     // terenkripsi, tapi sertifikat server tidak diperiksa
  HTTPClient http;
  http.begin(c, "https://api.anthropic.com/v1/messages");
  http.addHeader("content-type", "application/json");
  http.addHeader("x-api-key", API_KEY);
  http.addHeader("anthropic-version", "2023-06-01");
  http.setTimeout(30000);
  int code = http.POST(body);
  String resp = http.getString();
  http.end();
  if (code != 200) return "GAGAL (kode " + String(code) + "): " + resp;
  JsonDocument r; deserializeJson(r, resp);
  return r["content"][0]["text"] | "(kosong)";
}

void setup() {
  Serial.begin(115200);
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
