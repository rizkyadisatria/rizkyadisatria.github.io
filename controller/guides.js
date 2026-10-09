/* Daftar panduan di home page.
   Menambah panduan baru:
   1. Buat folder baru di dalam folder ini (controller/), mis. "sensor-jarak/" berisi index.html (tautkan ../assets/tema.css).
   2. Tambahkan satu objek di bawah. Urutan bebas: home page menampilkan yang terbaru dulu (berdasarkan "diperbarui").
   Field "gambar" opsional (path relatif dari folder ini). */
window.SITE = {
  judul: "Panduan Elektronika",
  repo: "https://github.com/rizkyadisatria/rizkyadisatria.github.io/tree/main/controller"
};

window.GUIDES = [
  {
    slug: "listrik-pintar",
    judul: "Listrik Pintar dan Biaya Rupiah",
    ringkas: "Nyalakan dan matikan lampu atau alat lain dari HP, ukur watt dan kWh tiap alat, dan hitung biaya dalam Rupiah untuk listrik prabayar (token) 900 VA. Memakai colokan pintar ber-meter dan Home Assistant, plus opsi ESP32 membaca meteran.",
    level: "Menengah",
    tahap: "6 tahap + kalkulator",
    komponen: ["Colokan pintar", "Home Assistant", "ESP32", "LDR"],
    status: "Belum diuji di rumah, kalkulator sudah dicek",
    gambar: "stasiun-cuaca/img/esp32.jpg",
    diperbarui: "2026-10-09"
  },
  {
    slug: "home-assistant",
    judul: "Stasiun Cuaca di Home Assistant",
    ringkas: "Sambungkan stasiun cuaca ke Home Assistant dengan ESPHome: dashboard suhu, kelembapan, cahaya, kontrol LED dan buzzer, serta otomasi alarm kalau kamar panas. Dipandu dari install di Windows.",
    level: "Menengah",
    tahap: "7 tahap (tahap 7 opsional)",
    komponen: ["ESP32", "ESPHome", "Home Assistant", "Docker"],
    status: "Konfigurasi sudah divalidasi, belum diuji di kit",
    gambar: "stasiun-cuaca/img/esp32.jpg",
    diperbarui: "2026-10-09"
  },
  {
    slug: "stasiun-cuaca",
    judul: "Stasiun Cuaca Kamar + Bot Telegram",
    ringkas: "Ukur suhu, kelembapan, dan cahaya kamar dengan ESP32, tampilkan lewat bot Telegram, dan beri peringatan dengan LED dan buzzer kalau terlalu panas. Dimulai dari mengenal setiap komponen sampai kode final.",
    level: "Pemula",
    tahap: "8 tahap (tahap 7 opsional)",
    komponen: ["ESP32", "DHT11", "LDR", "LED", "Buzzer", "Breadboard"],
    status: "Tahap 0–6 sudah dicoba di kit nyata",
    gambar: "stasiun-cuaca/img/esp32.jpg",
    diperbarui: "2026-10-09"
  }
];
