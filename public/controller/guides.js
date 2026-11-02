/* Daftar panduan di home page.
   Menambah panduan baru:
   1. Buat folder baru di dalam folder ini (controller/), mis. "sensor-jarak/" berisi index.html (tautkan ../assets/tema.css).
   2. Tambahkan satu objek di bawah. Urutan bebas: home page menampilkan yang terbaru dulu (berdasarkan "diperbarui").
   Field "gambar" opsional (path relatif dari folder ini).
   Field "en" opsional: terjemahan judul, ringkas, level, dan tahap untuk tombol bahasa EN.
   Label status di kartu dibuat otomatis dari <slug>/langkah.json dan centang langkah di browser pembaca
   (lihat _alat/langkah.py), jadi tidak perlu ditulis di sini. */
window.SITE = {
  judul: "Panduan Elektronika",
  repo: "https://github.com/rizkyadisatria/rizkyadisatria.github.io/tree/master/public/controller"
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
    diperbarui: "2026-10-29",
    en: {
      "judul": "Basic Electronics for IoT and Microcontrollers",
      "ringkas": "The basics that apply to any ESP32, Arduino or IoT project: voltage and current, common ground, regulators, battery life, resistors and capacitors, LEDs, transistors and relays, microcontroller pins, PWM and servos, I2C/SPI/UART, multimeters, breadboards, and how to find faults. With calculators, questions and exercises.",
      "level": "Beginner",
      "tahap": "16 chapters"
    }
  },
  {
    slug: "robot-ai",
    judul: "Robot AI 4WD",
    ringkas: "Robot beroda empat yang bisa diajak ngobrol lewat AI Claude, digerakkan dari layar sentuh atau Telegram, melihat lewat ESP32-CAM, memutar musik, mengikuti benda di depannya, dan berjaga. Enam bagian: daya baterai, badan, otak, mata, kecerdasan, rakit akhir.",
    level: "Menengah",
    tahap: "6 bagian, 38 tahap",
    komponen: ["ESP32-S3", "ESP32 DevKit", "ESP32-CAM", "TB6612", "TFT sentuh", "18650", "Claude"],
    gambar: "robot-ai/img/sampul.svg",
    diperbarui: "2026-10-24",
    en: {
      "judul": "Robot AI 4WD",
      "ringkas": "A four-wheeled robot you can talk to through Claude AI, driven from the touch screen or Telegram. It sees through an ESP32-CAM, plays music, follows objects in front of it, and stands guard. Six parts: battery power, body, brain, eyes, intelligence, final assembly.",
      "level": "Intermediate",
      "tahap": "6 parts, 38 stages"
    }
  },
  {
    slug: "stasiun-cuaca",
    judul: "Stasiun Cuaca Kamar + Bot Telegram",
    ringkas: "Ukur suhu, kelembapan, dan cahaya kamar dengan ESP32, tampilkan lewat bot Telegram, dan beri peringatan dengan LED dan buzzer kalau terlalu panas. Dimulai dari mengenal setiap komponen sampai kode final.",
    level: "Pemula",
    tahap: "8 tahap (tahap 7 opsional)",
    komponen: ["ESP32", "DHT11", "LDR", "LED", "Buzzer", "Breadboard"],
    gambar: "stasiun-cuaca/img/sampul.svg",
    diperbarui: "2026-10-18",
    en: {
      "judul": "Room Weather Station + Telegram Bot",
      "ringkas": "Measure room temperature, humidity and light with an ESP32, show them through a Telegram bot, and warn with an LED and buzzer when it gets too hot. From getting to know each component all the way to the final code.",
      "level": "Beginner",
      "tahap": "8 stages (stage 7 is optional)"
    }
  }
];
