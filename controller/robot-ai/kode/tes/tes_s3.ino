// TES ESP32-S3: cek PSRAM dan flash. PSRAM harus sekitar 8 MB, flash 16 MB.
void setup() {
  Serial.begin(115200);
  delay(1000);
}

void loop() {
  Serial.printf("Chip: %s  rev %d  | Flash: %u MB | PSRAM: %u byte (bebas %u)\n",
                ESP.getChipModel(), ESP.getChipRevision(), ESP.getFlashChipSize() / 1048576,
                ESP.getPsramSize(), ESP.getFreePsram());
  delay(2000);
}
