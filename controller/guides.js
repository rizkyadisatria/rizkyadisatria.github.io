/* Daftar panduan di home page.
   Menambah panduan baru:
   1. Buat folder baru di dalam folder ini (controller/), mis. "sensor-jarak/" berisi index.html (tautkan ../assets/tema.css).
   2. Tambahkan satu objek di bawah. Urutan bebas: home page menampilkan yang terbaru dulu (berdasarkan "diperbarui").
   Field "gambar" opsional (path relatif dari folder ini).
   Label status di kartu dibuat otomatis dari <slug>/langkah.json dan centang langkah di browser pembaca
   (lihat _alat/langkah.py), jadi tidak perlu ditulis di sini. */
window.SITE = {
  judul: "Panduan Elektronika",
  repo: "https://github.com/rizkyadisatria/rizkyadisatria.github.io/tree/master/controller"
};

window.GUIDES = [
  {
    slug: "elektronika-dasar",
    judul: "Elektronika Dasar untuk IoT dan Mikrokontroler",
    ringkas: "Dasar yang berlaku untuk proyek ESP32, Arduino, dan IoT apa pun: tegangan dan arus, GND bersama, regulator, umur baterai, resistor dan kapasitor, LED, transistor dan relay, pin mikrokontroler, PWM dan servo, I2C/SPI/UART, multimeter, breadboard, dan cara mencari kesalahan. Dengan kalkulator, pertanyaan, dan latihan.",
    level: "Pemula",
    tahap: "16 bab",
    komponen: ["Multimeter", "GND", "Regulator", "Baterai", "Transistor", "Relay", "I2C", "PWM"],
    gambar: "elektronika-dasar/img/sampul.svg",
    diperbarui: "2026-10-29"
  },
  {
    slug: "robot-ai",
    judul: "Robot AI 4WD",
    ringkas: "Robot beroda empat yang bisa diajak ngobrol lewat AI Claude, digerakkan dari layar sentuh atau Telegram, melihat lewat ESP32-CAM, memutar musik, mengikuti benda di depannya, dan berjaga. Enam bagian: daya baterai, badan, otak, mata, kecerdasan, rakit akhir.",
    level: "Menengah",
    tahap: "6 bagian, 38 tahap",
    komponen: ["ESP32-S3", "ESP32 DevKit", "ESP32-CAM", "TB6612", "TFT sentuh", "18650", "Claude"],
    gambar: "robot-ai/img/sampul.svg",
    diperbarui: "2026-10-24"
  },
  {
    slug: "stasiun-cuaca",
    judul: "Stasiun Cuaca Kamar + Bot Telegram",
    ringkas: "Ukur suhu, kelembapan, dan cahaya kamar dengan ESP32, tampilkan lewat bot Telegram, dan beri peringatan dengan LED dan buzzer kalau terlalu panas. Dimulai dari mengenal setiap komponen sampai kode final.",
    level: "Pemula",
    tahap: "8 tahap (tahap 7 opsional)",
    komponen: ["ESP32", "DHT11", "LDR", "LED", "Buzzer", "Breadboard"],
    gambar: "stasiun-cuaca/img/sampul.svg",
    diperbarui: "2026-10-18"
  }
];
