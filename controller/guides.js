/* Daftar panduan di home page.
   Menambah panduan baru:
   1. Buat folder baru di dalam folder ini (controller/), mis. "sensor-jarak/" berisi index.html (tautkan ../assets/tema.css).
   2. Tambahkan satu objek di bawah. Urutan bebas: home page menampilkan yang terbaru dulu (berdasarkan "diperbarui").
   Field "gambar" opsional (path relatif dari folder ini). */
window.SITE = {
  judul: "Panduan Elektronika",
  repo: "https://github.com/rizkyadisatria/rizkyadisatria.github.io/tree/master/controller"
};

window.GUIDES = [
  {
    slug: "robot-ai",
    judul: "Robot AI 4WD",
    ringkas: "Robot beroda empat yang bisa diajak ngobrol lewat AI Claude, digerakkan dari layar sentuh atau Telegram, melihat lewat ESP32-CAM, memutar musik, mengikuti benda di depannya, dan berjaga. Enam bagian: daya baterai, badan, otak, mata, kecerdasan, rakit akhir.",
    level: "Menengah",
    tahap: "6 bagian, 35 tahap",
    komponen: ["ESP32-S3", "ESP32 DevKit", "ESP32-CAM", "TB6612", "TFT sentuh", "18650", "Claude"],
    status: "Kode sudah dikompilasi, belum diuji di robot nyata",
    gambar: "robot-ai/img/chassis.jpg",
    diperbarui: "2026-10-18"
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
    diperbarui: "2026-10-18"
  }
];
