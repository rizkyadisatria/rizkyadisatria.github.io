from build import *

IMG = 'img/'

KIT = [
    # (gambar, nama, fungsi, status)  status: 1 dipakai, 2 cadangan/opsional, 0 belum ada foto (katalog), 3 tidak dipakai di robot
    ('esp32s3', 'ESP32-S3 N16R8', 'Otak: layar, suara, WiFi, AI.', 1),
    ('s3_ekspansi', 'Papan ekspansi S3', 'Tempat S3 dicolok, baris G/3V3/S per GPIO.', 1),
    ('devkit', 'ESP32 DevKit V1', 'Badan: motor, servo, sensor.', 1),
    ('ekspansi30p', 'Papan ekspansi 30P', 'Tempat DevKit dicolok, ada blok 5V/3V/GND.', 1),
    ('chassis', 'Chassis 4WD', 'Dua plat akrilik, 4 motor TT, 4 roda.', 1),
    ('plat', 'Plat chassis Starlectric', 'Akrilik buram, ujung lebar 15,2 cm, pinggang 10 cm, dua sayap tengah.', 1),
    ('spacer30', 'Spacer kuningan 30+6 mm', 'Sekitar 8 buah beserta baut dan mur. Hitung ulang jumlahnya.', 1),
    ('kabel22', 'Kabel 22 AWG merah, hijau, hitam', 'Masing-masing 1 m. Jalur daya: merah +7,4 V, hijau +6 V, hitam GND.', 1),
    ('baterai', '2 sel 18650', 'RakkiPanda 3000 mAh, dirangkai seri 7,4 V.', 1),
    ('bms', 'BMS 2S 20A', 'Pelindung baterai: kosong, penuh, arus lebih.', 1),
    ('sekring', 'Sekring blade', 'Putus kalau ada korsleting.', 1),
    ('saklar', 'Saklar rocker', 'Saklar utama robot.', 1),
    ('lm2596', 'LM2596 + voltmeter', 'Konverter 5 V untuk logika (perlu 2).', 1),
    ('xl4015', 'XL4015', 'Konverter 6 V untuk motor dan servo.', 1),
    ('hcsr04', '2x HC-SR04', 'Sensor jarak depan dan belakang.', 1),
    ('mpu6050', 'MPU6050 (GY-521)', 'Sensor kemiringan.', 1),
    ('pantilt', 'Pan-tilt 2x MG90S', 'Kepala yang menoleh dan mengangguk.', 1),
    ('inmp441', 'INMP441', 'Mikrofon digital I2S.', 1),
    ('speaker_kotak', 'Speaker kotak', 'Suara robot dan musik. Ohm belum diketahui (4-8 Ω sama-sama aman).', 1),
    ('speaker', 'Speaker bulat 8 Ω 3 W', 'Cadangan; speaker kotak lebih nyaring karena punya rumah.', 2),
    ('resistor', 'Resistor 1%', '30 nilai, 20 buah tiap nilai.', 1),
    ('kapasitor', 'Kapasitor elektrolit', '10, 100, 470, 1000 µF, 50 V.', 1),
    ('dupont', 'Kabel jumper', 'M-M, M-F, F-F.', 1),
    ('breadboard', '2 breadboard 400 lubang', 'Satu untuk badan, satu untuk otak.', 1),
    ('header', 'Pin header', 'Disolder ke modul.', 1),
    ('tombol', 'Push button', 'Tombol STOP.', 1),
    ('multimeter', 'Multimeter', 'Wajib untuk mengukur sebelum menyambung.', 1),
    ('solder', 'Solder + timah + flux', 'Menyambung permanen.', 1),
    ('pinset', 'Pinset', 'Memegang komponen kecil.', 1),
    ('obeng', 'Set obeng', 'Trimpot, terminal, baut chassis.', 1),
    ('charger', 'Charger iMFG 2 slot', 'Mengisi sel 18650 di luar robot.', 1),
    ('adaptor', 'Adaptor 5 V 2 A', 'Daya charger.', 1),
    ('usb', 'Kabel USB-C dan micro-USB', 'Upload kode: USB-C ke ESP32-S3, micro-USB ke DevKit dan ESP32-CAM-MB.', 1),
    ('tb6612', 'TB6612FNG', 'Driver 4 motor (2 kanal). Pin header sudah terpasang.', 1),
    ('tft', 'TFT 2,4" sentuh + SD', 'Wajah robot. Slot SD ukuran penuh di belakang.', 1),
    ('sdcard', 'SanDisk Extreme SD 32 GB', 'Musik. Masuk langsung ke slot layar.', 1),
    ('max98357a', 'MAX98357A', 'Penguat speaker I2S. Pin header sudah terpasang.', 1),
    ('esp32cam', 'ESP32-CAM (OV3660)', 'Mata robot. Sensor OV3660 tertulis di pita kamera.', 1),
    ('esp32cam_mb', 'ESP32-CAM-MB', 'Papan upload ESP32-CAM (chip CH340).', 1),
    ('holder', '2 holder 18650 1 slot', 'Dirangkai seri jadi 7,4 V.', 1),
    ('lm2596_2', 'LM2596 kedua', 'Rel 5 V khusus kamera.', 1),
    ('kapasitor104', 'Kapasitor keramik 104 (100 nF)', 'Peredam noise di kaki motor.', 1),
    ('kabel', 'Kabel merah-hitam UL1007 (±26 AWG)', 'Terlalu tipis untuk daya. Pakai untuk sinyal, sensor, dan servo.', 1),
    ('cabletie', 'Cable ties 3 x 100 mm', '100 buah: merapikan kabel dan mengikat modul tanpa lubang.', 1),
    ('ov2640', 'OV2640 + breakout FPC', 'Cadangan; ESP32-CAM sudah punya kamera.', 3),
    ('lcd1602', 'LCD 1602', 'Untuk stasiun cuaca, bukan robot.', 3),
    ('relay', 'Modul relay', 'Tidak dipakai di robot.', 3),
    ('sg90', 'Servo SG90', 'Cadangan kalau MG90S rusak.', 3),
    ('ldr', 'LDR', 'Tidak dipakai di robot.', 3),
    ('pot', 'Potensiometer', 'Kontras LCD stasiun cuaca.', 3),
    ('led', 'LED', 'Uji rel daya di Bagian 1.', 2),
    ('dioda', 'Dioda 1N4001/1N4007', 'Tidak dipakai; bukan Schottky.', 3),
]
BADGE = {1: ('dipakai', ''), 2: ('dipakai sebentar', 'opt'), 0: ('foto katalog, belum ada fotomu', 'opt'), 3: ('tidak dipakai di robot', 'opt')}


def kartu(g, n, f, s):
    im = f'<img src="{IMG}{g}.jpg" alt="Foto {esc(n)} milikmu" loading="lazy">' if g else '<div class="noimg">belum ada foto<br>barang asli</div>'
    b, c = BADGE[s]
    return f'<article class="card" tabindex="0">{im}<div class="body"><span class="name">{esc(n)}</span><p class="what">{f}</p><span class="badge {c}">{b}</span></div></article>'


def isi():
    o = '<header class="top"><a class="back" href="../">← Semua panduan</a><span class="eyebrow">ESP32-S3 · DevKit V1 · ESP32-CAM · Claude</span>'
    o += '<h1>Robot AI 4WD</h1>'
    o += ('<p class="lead">Robot beroda empat yang bisa diajak ngobrol, digerakkan dari layar sentuh atau Telegram, melihat lewat kamera, '
          'memutar musik, mengikuti benda di depannya, dan berjaga. Dirakit bertahap dari baterai sampai AI, dengan tes kecil di setiap langkah.</p>')
    o += toc('', '') + '</header>'

    o += section('status', 'Jujur dulu', 'Status panduan ini', '''
<div class="two">
  <div class="panel"><h3>Sudah dicek</h3><ul class="plain"><li>Ketiga kode lengkap dan 13 kode tes <b>berhasil dikompilasi</b> dengan Arduino ESP32 core 3.3.12 dan library terbaru.</li><li>Pin dipilih untuk menghindari pin boot, PSRAM, dan USB di kedua ESP32.</li><li>Tegangan tiap modul dicocokkan dengan lembar datanya.</li></ul></div>
  <div class="panel"><h3>Belum dicek</h3><ul class="plain"><li>Robot lengkapnya belum dirakit dan diuji di perangkat nyata.</li><li>Urutan pin TB6612 dan layar berbeda antar-penjual. Panduan ini sudah dicocokkan dengan foto modulmu, tapi tetap cocokkan <b>nama</b> pin yang tercetak saat menyambung.</li><li>Suara robot memakai layanan Google TTS tidak resmi yang bisa berhenti kapan saja.</li></ul></div>
</div>''')

    o += section('cara', 'Gambaran', 'Tiga board, satu robot', fig(diag.arsitektur()) + '''
<div class="panel"><p>Robot dibagi tiga karena satu ESP32 tidak punya cukup pin untuk semuanya. <b>Badan</b> (DevKit V1) mengurus hal yang harus cepat dan aman: motor, sensor, berhenti sendiri. <b>Otak</b> (ESP32-S3) mengurus layar, suara, dan internet. <b>Mata</b> (ESP32-CAM) hanya mengambil foto. Otak dan badan bicara lewat 3 kabel UART dengan perintah teks sederhana, jadi kamu bisa mengetes badan sendirian dari Serial Monitor.</p></div>''')

    parts = ''
    for i, (slug, nama, ket) in enumerate(PARTS, 1):
        parts += (f'<a class="partcard" href="{slug}/"><span class="num">{i}</span><div><h3>{nama}</h3><p>{ket}</p></div>'
                  f'<span class="pp" data-progres="robot-ai/{slug}">belum mulai</span></a>')
    o += section('bagian', 'Urutan', 'Enam bagian, kerjakan berurutan', f'<div class="parts">{parts}</div>',
                 'Setiap bagian punya tahap-tahap kecil dengan tanda berhasil. Centang langkah yang selesai; progres tersimpan di browser-mu.')

    o += section('aturan', 'Sebelum mulai', 'Aturan yang berlaku di semua bagian', '''
<div class="rules">
  <div class="rule"><b>Ukur, baru sambung</b><p>Setiap tegangan diukur multimeter dulu. Konverter yang belum disetel bisa mengeluarkan 8 V.</p></div>
  <div class="rule"><b>Daya mati saat merakit</b><p>Saklar baterai OFF dan USB dicabut setiap kali memindah kabel.</p></div>
  <div class="rule"><b>Satu perubahan, satu tes</b><p>Tambah satu modul, tes, baru lanjut. Kalau gagal, kamu tahu penyebabnya ada di modul terakhir.</p></div>
  <div class="rule"><b>Roda terangkat</b><p>Semua tes motor dilakukan dengan robot di atas kotak, sampai Bagian 6.</p></div>
</div>''')

    belum = table(['Barang', 'Jumlah', 'Untuk', 'Tanpa barang ini'], [
        ['BME280', '1 (opsional)', 'suhu, kelembapan, tekanan', 'pakai DHT11 dari kit cuaca'],
        ['Tambahan kabel 22 AWG hitam (opsional)', '1 m', 'kabel 22 AWG-mu 1 m per warna kira-kira cukup untuk merah dan hijau, tapi hitam (GND) dipakai di paling banyak titik, jadi mungkin kurang', 'jalur GND dipendekkan atau disambung dari potongan 26 AWG (tidak disarankan untuk arus motor)'],
        ['Konektor WAGO 221-415 (5 lubang) atau tiruannya', '4 (+1 cadangan)', '2 untuk rel baterai (+ dan GND ke 3 konverter), 2 untuk rel 6 V (+ dan GND ke TB6612, 2 servo, kapasitor)', 'harus menyolder sambungan Y'],
        ['Heat shrink kit ukuran campur (2-10 mm)', '1 kotak isi ~100-160', 'sekitar 30 sambungan: 3 mm untuk kabel servo/tipis, 5 mm untuk kabel 20-22 AWG, 8 mm untuk kaki saklar dan sekring', 'sambungan mudah korsleting'],
        ['Kit spacer nilon M3 180 pcs (4-12 mm, baut, mur)', '1 kit', 'memasang 2 papan ekspansi, 2 LM2596, XL4015, MPU6050, layar ke plat', 'board hanya bisa diikat cable ties, lebih mudah goyang'],
        ['Spacer kuningan M3 30+6 mm (jantan-betina)', 'sudah ±8; total perlu 12 (6 untuk plat atas + 6 untuk kaki lantai 3, minimal 8 untuk mulai)', 'kaki lantai 3: 2 bertumpuk per kaki = 60 mm, di atas 6 spacer plat atas', 'tiga lantai dibutuhkan karena platnya sempit dan pendek'],
        ['Plat chassis 4WD Starlectric (25,6 x 10 cm)', '2 (1 kalau kitmu sudah berisi dua plat)', 'plat bawah (motor) dan plat atas (lantai 2). Satu lembar di listing = satu plat', 'periksa isi kit chassismu dulu'],
        ['Papan lantai 3, persegi 256 x 152 mm, tebal 3-4 mm', '1', 'triplek, MDF, atau akrilik; dipotong persegi karena plat berpinggang tidak muat layar dan speaker', 'sama seperti di atas'],
        ['Dudukan (bracket) HC-SR04', '2 (opsional)', 'memegang HC-SR04 tegak di tepi depan dan belakang, dibaut ke plat', 'HC-SR04 diikat cable ties; lebih mudah miring dan salah baca'],
    ])
    kartu_html = ''.join(kartu(*k) for k in KIT)
    o += section('barang', 'Barang', 'Yang sudah kamu punya, dan yang masih kurang', f'''
<div class="panel"><h3>Masih perlu dibeli</h3>{belum}<p class="cap">Sudah datang dan dicoret dari daftar ini: TB6612FNG, layar TFT, MAX98357A, ESP32-CAM + MB, LM2596 kedua, holder kedua, kapasitor 100 nF, cable ties, dan kartu SD. Kabel USB-C dan micro-USB sudah ada sejak awal. Kabel 22 AWG merah, hijau, hitam masing-masing 1 m sudah datang; kabel 26 AWG lama hanya untuk sinyal.</p></div>
<div class="toolbar"><button id="quizbtn" aria-pressed="false" type="button">Mode tebak nama</button><span class="cap" id="quizhint"></span></div>
<div class="grid" id="kit">{kartu_html}</div>
<p class="cap">Foto dari barang milikmu sendiri. Mode tebak nama menyamarkan nama tiap barang; ketuk kartunya untuk mengecek.</p>''')

    pins_badan = table(['Fungsi', 'DevKit V1'], [
        ['Motor kiri: PWMA, AIN1, AIN2', 'D25, D26, D27'], ['Motor kanan: PWMB, BIN1, BIN2', 'D13, D32, D33'], ['STBY, VCC TB6612', '3V'],
        ['Servo pan, tilt', 'D23, D4'], ['HC-SR04 depan TRIG, ECHO', 'D18, D19 (pembagi)'], ['HC-SR04 belakang TRIG, ECHO', 'D14, D35 (pembagi)'],
        ['I2C SDA, SCL', 'D21, D22'], ['DHT11 (tanpa BME280)', 'D5'], ['Tombol STOP', 'D39 + 10k pull-up'], ['Baterai', 'D34 (20k/10k)'], ['UART RX2, TX2', 'D16, D17']])
    pins_otak = table(['Fungsi', 'ESP32-S3'], [
        ['SPI SCK, MOSI, MISO', 'IO12, IO11, IO13'], ['TFT CS, DC, RESET', 'IO10, IO9, IO8'], ['Sentuhan T_CS', 'IO14'], ['SD_CS', 'IO21'],
        ['Speaker BCLK, LRC, DIN', 'IO4, IO5, IO7'], ['Mikrofon SCK, WS, SD', 'IO15, IO16, IO6'], ['UART TX, RX', 'IO17, IO18']])
    o += section('pin', 'Ringkasan', 'Semua pin dalam satu tempat', f'<div class="two"><div class="panel"><h3>Badan</h3>{pins_badan}</div><div class="panel"><h3>Otak</h3>{pins_otak}</div></div><p class="cap">ESP32-CAM tidak butuh kabel sinyal: hanya 5 V dan GND. Pin kameranya sudah tetap di papan.</p>')

    o += section('kode', 'Berkas', 'Kode', ul([
        '<a href="kode/robot-ai-kode.zip" download>robot-ai-kode.zip</a>: semua kode dalam satu file (3 kode lengkap + 13 kode tes, masing-masing di foldernya sendiri, siap dibuka Arduino IDE).',
        '<a href="kode/robot_badan/robot_badan.ino" download>robot_badan.ino</a>: untuk DevKit V1 (board: ESP32 Dev Module).',
        '<a href="kode/robot_otak/robot_otak.ino" download>robot_otak.ino</a>: untuk ESP32-S3 (board: ESP32S3 Dev Module, PSRAM OPI, Huge APP).',
        '<a href="kode/robot_mata/robot_mata.ino" download>robot_mata.ino</a>: untuk ESP32-CAM (board: AI Thinker ESP32-CAM, Huge APP).',
        'Isi WiFi, token, dan API key hanya di komputermu. Jangan unggah file yang sudah terisi ke GitHub.',
    ]))
    return o
