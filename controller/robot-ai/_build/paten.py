# Keterangan "boleh dipatenkan atau belum" untuk tiap tahap, ditampilkan setelah kotak "Berhasil kalau".
# Kunci: (bagian, nomor tahap). Nilai: (boleh, jangan_dulu).
#
# Arti "dipatenkan": disolder dan heat shrink ditutup, setelan trimpot dikunci, nilai di kode dianggap final,
# atau modul dibaut / diikat cable tie ke plat chassis.
# Aturan yang dipakai di semua tahap:
#   1. tesnya sudah lulus,
#   2. tidak ada tes lain yang masih perlu menjangkau atau mengubahnya,
#   3. posisinya sudah ditentukan tata letak Bagian 6, dan susun kering Bagian 6 Tahap 1 sudah lulus
#      (sebelum itu, jangan membaut, mengikat, atau mengebor apa pun ke plat).

PATEN = {
    # ---------------- Bagian 1: daya ----------------
    ('1-daya', 1): (
        'Belum ada yang dipasang di tahap ini. Hanya pengecekan.',
        'Jangan menyolder atau memasukkan sel ke holder dulu. Pakai kedua sel ini selalu sebagai pasangan.'),
    ('1-daya', 2): (
        'Solderan kabel 4.2V (heat shrink kuning) dan P− ke pad BMS. Panjangnya sudah dihitung dari posisi akhir di robot, jadi ini sambungan final.',
        'Pad ⊕, 8.4V, dan 0V masih kosong (Tahap 3 dan 4). BMS belum diikat ke plat atas: itu di Bagian 6 Tahap 3.'),
    ('1-daya', 3): (
        'Solderan holder ke pad 0V, titik tengah (heat shrink sudah menutup), dan pad 8.4V.',
        'Holder belum dibaut ke plat atas (Bagian 6 Tahap 3). Kedua sel dikeluarkan lagi di Tahap 4 sebelum menyolder sekring.'),
    ('1-daya', 4): (
        'Semua solderan sekring dan saklar dengan heat shrink tertutup, serta kabel rel plus dan P− di WAGO + dan WAGO − (tuas tertutup, ditarik pelan tidak lepas).',
        'Sekring, saklar, dan WAGO belum diikat ke plat. Lubang kosong di WAGO sengaja dibiarkan untuk konverter dan kapasitor.'),
    ('1-daya', 5): (
        'Setelan 5,0 V LM2596 #1: trimpot jangan diputar lagi. Kabel WAGO + dan WAGO − ke IN+ dan IN−.',
        'Modul belum dipasang di plat lantai 1 (Bagian 6 Tahap 2). OUT baru dihubungkan ke papan 30P dan S3 di Bagian 6 Tahap 6; sebelum itu DevKit dan S3 hidup dari USB (Tahap 8 hanya memakainya untuk uji LED).'),
    ('1-daya', 6): (
        'Setelan 6,0 V XL4015 (trimpot tegangan jangan diputar lagi, trimpot batas arus jangan diubah) dan kabel OUT ke WAGO 6V dan WAGO −6.',
        'XL4015 belum dipasang: tempatnya di lantai 2 (Bagian 6 Tahap 3). Kabel masukannya sengaja panjang karena harus naik ke lantai 2.'),
    ('1-daya', 7): (
        'Setelan 5,0 V LM2596 #2 dan kabel WAGO + dan WAGO − ke IN+ dan IN−.',
        'Modul belum dipasang di plat. OUT-nya baru dihubungkan ke kamera di Bagian 4 Tahap 3.'),
    ('1-daya', 8): (
        'Seluruh sistem daya sudah final secara listrik: setelan tegangan, solderan, dan isi WAGO. Jangan diubah lagi.',
        'Pemasangan ke chassis menunggu susun kering di Bagian 6 Tahap 1 lulus. Baterai dan modul masih boleh diangkat dan dipindah.'),

    # ---------------- Bagian 2: badan ----------------
    ('2-badan', 1): (
        'Posisi jumper 3.3V–V dan catatan pin 5V dan 3V di blok daya. DevKit tetap di papan 30P: keduanya naik ke lantai 2 sebagai satu unit.',
        'Papan 30P belum dibaut. Di robot ia memakai spacer nilon 8 mm di lantai 2 (Bagian 6 Tahap 3).'),
    ('2-badan', 2): (
        'Motor dan roda sudah terpasang di plat bawah, kapasitor 100 nF disolder, kabel kiri dan kanan digabung, heat shrink tertutup. Bagian ini tidak berubah lagi sampai robot jadi.',
        'Plat atas belum dipasang (Bagian 6 Tahap 3). Ujung jumper betina di kabel motor tetap dicolok ke TB6612, tidak perlu disolder.'),
    ('2-badan', 3): (
        'Kabel hijau dan hitam dari WAGO 6V dan WAGO −6 ke VM dan GND TB6612 (jumper betina disolder, heat shrink tertutup) dan kapasitor 470 µF di WAGO 6V dan WAGO −6.',
        'TB6612 belum diikat ke plat (cable tie, Bagian 6 Tahap 2). Sembilan kabel sinyal F-F ke papan 30P masih kabel meja.'),
    ('2-badan', 4): (
        'Solderan merah-hijau, coklat-hitam, dan kapasitor 470 µF dengan heat shrink tertutup. Sekrup lengan servo setelah servo diam di 90°.',
        'Kepala belum dibaut ke plat ketiga (Bagian 6 Tahap 4). Batas TILT_MIN dan TILT_MAX baru final setelah kepala terpasang di robot, karena bergantung pada bracket dan posisi kepala. Kabel oranye ke D23 dan D4 masih kabel meja.'),
    ('2-badan', 5): (
        'Susunan resistor pembagi 1 kΩ dan 2 kΩ di breadboard 1. Breadboard ini ikut naik ke lantai 2 bersama kabel yang sudah menancap (Bagian 6 Tahap 3).',
        'Kedua sensor jangan dipaten: yang belakang pindah ke lantai 1 dan kabelnya diatur ulang (TRIG lewat breadboard), yang depan dipasang di tepi depan lantai 2.'),
    ('2-badan', 6): (
        'Solderan pin header MPU6050.',
        'MPU6050 belum ditempel. Di robot ia harus datar di lantai 1 dengan spacer 6 mm, tulisan menghadap ke depan (Bagian 6 Tahap 2). Kalau miring, pitch dan roll tidak mulai dari nol.'),
    ('2-badan', 7): (
        'Kabel pengukur baterai 26 AWG (heat shrink biru) dari IN+ LM2596 #1 ke a20, serta resistor pembagi 20 kΩ dan 10 kΩ. Nilai BATERAI_FAKTOR yang kamu hitung.',
        'Tombol 6 mm hanya untuk tes: di Bagian 6 Tahap 3 ia dilepas dan diganti tombol STOP 12 mm di lantai 3. Kabel GND meja (WAGO − ke pin G) juga hanya untuk tes: di robot GND bersama datang dari kabel hitam LM2596 #1 ke papan 30P.'),
    ('2-badan', 8): (
        'Isi BATERAI_FAKTOR dan pilihan arah putar motor di robot_badan.ino. Kode ini dipakai apa adanya di robot. Upload kode wajib sebelum Bagian 3.',
        'JARAK_STOP_DEPAN, TILT_MIN, dan TILT_MAX baru disetel setelah robot jadi (Bagian 6 Tahap 8). Uji keselamatan boleh ditunda ke Bagian 6, tapi jangan sampai terlewat.'),

    # ---------------- Bagian 3: otak ----------------
    ('3-otak', 1): (
        'Pengaturan Tools Arduino (OPI PSRAM, Huge APP) dan S3 yang tetap menancap di papan ekspansinya: keduanya naik ke lantai 3 sebagai satu unit.',
        'Papan S3 belum dibaut. Di robot ia memakai spacer nilon 10 mm, port USB-C menghadap ke samping (Bagian 6 Tahap 4).'),
    ('3-otak', 2): (
        'Susunan kabel di breadboard 2 (bus SPI kolom 2, 5, 8 dan pin layar). Breadboard ini ikut naik ke lantai 3 bersama kabelnya.',
        'Layar belum dibaut. Di robot ia rebah menghadap atas dengan spacer 20 mm, dan kabel yang menuju pinnya ada di bawah layar, jadi harus sudah menancap sebelum layar dibaut.'),
    ('3-otak', 3): (
        'Solderan empat pin header SD, format FAT32 dan lagu di kartu.',
        'Tidak ada yang dipasang ke plat di tahap ini.'),
    ('3-otak', 4): (
        'Kabel speaker yang sudah terjepit di terminal ⊕ dan ⊖.',
        'Vin MAX98357A masih di rel 3,3 V. Di robot ia dipindah ke pin 5V papan S3 (Bagian 6 Tahap 6). Speaker belum diikat ke plat (Bagian 6 Tahap 4).'),
    ('3-otak', 5): (
        'Solderan pin header INMP441, dengan lubang suara tidak tertutup.',
        'VDD tetap ke 3,3 V. Jangan ikut dipindah ke 5 V saat mode jalan seperti Vin speaker: INMP441 tidak boleh 5 V.'),
    ('3-otak', 6): (
        'Pasangan UART: IO17 ke D16, IO18 ke D17, G ke G. Di robot sama persis, hanya kabelnya lebih panjang.',
        'Kabel jumper 20 cm di meja belum final: di robot kabel UART lebih panjang dan disambung F-F dengan M-F (Bagian 6 Tahap 4).'),

    # ---------------- Bagian 4: mata ----------------
    ('4-mata', 1): (
        'WIFI_NAMA dan WIFI_PASSWORD di robot_mata.ino.',
        'ESP32-CAM masih tertancap di papan MB. Ia harus dicabut dari MB di Tahap 3, karena di robot dayanya dari pin 5V.'),
    ('4-mata', 2): (
        'Alamat IP tetap di router dan nilai KAMERA_TERBALIK. Catat alamatnya untuk Bagian 5.',
        'Tutup tab /stream di browser sebelum Bagian 5, supaya kamera bisa melayani otak.'),
    ('4-mata', 3): (
        'Kabel 5 V kamera dari LM2596 #2 (jumper betina disolder, heat shrink), kapasitor 1000 µF, dan ikatan cable tie ESP32-CAM di dudukan depan kepala.',
        'Kepala belum dibaut ke plat ketiga (Bagian 6 Tahap 4); sisakan kelonggaran kabel supaya kepala bisa menoleh penuh. LM2596 #2 belum dipasang di plat lantai 1.'),

    # ---------------- Bagian 5: kecerdasan ----------------
    ('5-ai', 1): (
        'Token bot dan chat ID.',
        'Jangan diunggah ke GitHub atau dikirim ke orang lain. Kalau bocor, buat bot baru.'),
    ('5-ai', 2): (
        'API key Claude dan batas pengeluaran bulanan.',
        'Kunci hanya ditampilkan sekali. Jangan taruh di berkas yang akan diunggah.'),
    ('5-ai', 3): (
        'Kunci Groq (boleh dikosongkan).',
        'Rekaman suara dikirim ke server Groq. Kalau keberatan, kosongkan kuncinya.'),
    ('5-ai', 4): (
        'robot_otak.ino yang sudah terisi (WiFi, token, chat ID, API key, KAMERA_ALAMAT, nama robot). Kalibrasi sentuh tersimpan di S3.',
        'Berkas yang sudah berisi kunci jangan diunggah ke tempat publik. Roda tetap terangkat.'),
    ('5-ai', 5): (
        'Semua kode dianggap final setelah tahap ini. Bagian 6 hanya menyetel konstanta (Tahap 8).',
        'Roda tetap terangkat sampai Bagian 6 Tahap 7. Jangan membongkar rangkaian meja sebelum semua fitur di tabel terbukti jalan, karena mencari penyebab sesudah dibongkar lebih sulit.'),

    # ---------------- Bagian 6: rakit akhir ----------------
    ('6-rakit', 1): (
        'Posisi tiap barang di tiga lantai, setelah difoto. Ini gerbang: begitu lulus, semua tata letak dianggap final.',
        'Belum ada baut, cable tie, atau bor: tahap ini sengaja tanpa baut dan tanpa ikatan. Setelah lulus, memasang barang lantai 1 (Tahap 2: modul, bukan sambungan kabel yang belum dibuat) tidak bergantung pada Bagian 3-5, jadi boleh dikerjakan lebih awal asalkan plat atas tetap dilepas. Kabel daya servo (Bagian 2 Tahap 4) dan kabel kamera (Bagian 4 Tahap 3) disambungkan ke lantai 1 setelah dibuat.'),
    ('6-rakit', 2): (
        'Semua barang lantai 1 (LM2596, MPU6050, TB6612, empat WAGO, HC-SR04 belakang), kabel sudah berlabel di ujung atasnya.',
        'Plat atas jangan ditutup dulu. Kabel lantai 1 yang naik harus sudah lengkap, berlabel, dan sambungannya dicek.'),
    ('6-rakit', 3): (
        'Plat atas dengan keenam kaki, holder, BMS, saklar, sekring, breadboard 1, papan 30P, XL4015, dan HC-SR04 depan.',
        'Kabel merah 5 V LM2596 #1 belum ditancap (Tahap 6). Tombol 6 mm sudah dilepas; jangan menutup lantai 3 sebelum kabel naik terkumpul di depan.'),
    ('6-rakit', 4): (
        'Plat ketiga lengkap: S3, TFT, breadboard 2, kepala, speaker, dan tombol STOP.',
        'Kabel merah 5 V S3 belum ditancap. Putar kepala hanya dengan tangan saat saklar OFF.'),
    ('6-rakit', 5): (
        'Ikatan cable tie per kelompok dan label kabel.',
        'Jauhkan kabel daya dan motor dari kabel SPI layar dan mikrofon. Sel dimasukkan paling akhir.'),
    ('6-rakit', 6): (
        'Kabel 5 V LM2596 #1 ke papan 30P dan S3, serta Vin MAX98357A di pin 5V papan S3.',
        'Setelah tahap ini rangkaian keras dianggap selesai. Jangan membongkar untuk menyetel kode; itu urusan Tahap 8.'),
    ('6-rakit', 7): (
        'Hasil uji di lantai: robot menurut perintah, berhenti di halangan, dan tombol STOP menang.',
        'Jangan tinggalkan robot berjalan sendiri dekat tangga: tidak ada sensor yang melihat lantai hilang.'),
    ('6-rakit', 8): (
        'Konstanta di kode: kecepatan, jarak berhenti, batas tilt, faktor baterai, arah kamera, dan kepribadian robot.',
        'Perangkat keras tidak diubah di tahap ini.'),
}
