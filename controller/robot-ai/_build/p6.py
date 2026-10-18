from build import *
import diag

S = '6-rakit'
IMG = '../img/'


def tata_letak():
    W, Hh = 900, 470
    s = ''
    s += diag.text(225, 30, 'PLAT BAWAH (dilihat dari atas)', 14, anchor='middle', weight=600)
    s += diag.text(675, 30, 'PLAT ATAS (dilihat dari atas)', 14, anchor='middle', weight=600)
    for x in (20, 470):
        s += f'<rect x="{x}" y="50" width="410" height="380" rx="40" fill="#e9efe9" fill-opacity=".14" stroke="#e9ece9" stroke-width="1.5"/>'
        s += diag.text(x + 205, 455, '↑ depan robot ada di atas', 12, anchor='middle', fill='#cfd8d4')
    # roda
    for (x, y) in ((8, 80), (8, 320), (418, 80), (418, 320)):
        s += f'<rect x="{x}" y="{y}" width="24" height="80" rx="6" fill="#222" stroke="#555"/>'
    b = diag.box
    s += b(150, 58, 150, 40, 'HC-SR04 depan', '', fill='#1d4f86', stroke='#9cc8ff')
    s += b(45, 115, 120, 70, 'XL4015', '6 V', fill='#241a3a', stroke='#b08cff')
    s += b(285, 115, 120, 70, 'LM2596 #1 + #2', '5 V', fill='#33240f', stroke='#f0a64b')
    s += b(170, 110, 105, 80, 'TB6612', '', fill='#8c1d18', stroke='#ffb4ab')
    s += b(45, 200, 360, 85, 'DevKit di papan 30P', ['+ breadboard kecil (pembagi, tombol)'], fill='#1d3324', stroke='#7fe0a0')
    s += b(170, 295, 110, 40, 'MPU6050', 'datar', fill='#1d3a6b', stroke='#9cc8ff')
    s += b(45, 345, 360, 45, 'Holder 18650 x2 + BMS + sekring', '', fill='#3a1513', stroke='#ff8a80')
    s += b(300, 293, 105, 44, 'Saklar', 'mudah dijangkau', fill='#3a1513', stroke='#ff8a80')
    s += b(150, 395, 150, 32, 'HC-SR04 belakang', '', fill='#1d4f86', stroke='#9cc8ff')
    s += b(600, 60, 150, 90, 'Pan-tilt', ['+ ESP32-CAM', 'menghadap depan'], fill='#3a2a13', stroke='#ffc27a')
    s += b(500, 165, 350, 80, 'Layar TFT (wajah)', ['berdiri miring menghadap depan'], fill='#8c1d18', stroke='#ffb4ab')
    s += b(500, 255, 230, 95, 'S3 di papan ekspansi', ['+ breadboard besar (bus SPI)'], fill='#13283a', stroke='#7fb6ff')
    s += b(745, 255, 105, 45, 'Speaker', '', fill='#2a2a2a', stroke='#bbb')
    s += b(745, 310, 105, 40, 'INMP441', 'jauh dari motor', fill='#2a2a2a', stroke='#bbb')
    s += b(500, 362, 350, 50, 'Tombol STOP 12 mm', ['ditempel di atas, mudah ditekan'], fill='#5a1010', stroke='#ff8a80')
    return diag.svg(W, Hh, s, 'Usulan tata letak komponen di dua plat chassis')


def halaman():
    isi = ''
    t = ''
    t += stage(S, 1, 'Tata letak di chassis', ['Semua bagian yang sudah dites', 'Spacer dan baut M3', 'Selotip busa dua sisi', 'Cable ties'], [
        'Susun dulu di atas plat tanpa menempel apa pun. Gambar di bawah hanya usulan; sesuaikan dengan lubang di platmu.',
        'Plat bawah: baterai dan BMS di tengah-belakang (barang terberat di tengah membuat robot stabil), konverter dan TB6612 di depan, papan 30P di tengah.',
        'Pastikan holder baterai bisa dibuka tanpa membongkar plat atas, karena sel dikeluarkan setiap mengisi.',
        'MPU6050 ditempel datar dengan arah yang konsisten (tulisan menghadap ke depan robot), jauh dari motor.',
        'HC-SR04 depan dipasang di tepi depan plat bawah, lurus ke depan, minimal 5 cm di atas lantai supaya tidak "melihat" lantai. Yang belakang di tepi belakang.',
        'Plat atas: pan-tilt dan ESP32-CAM di depan, layar berdiri miring sebagai wajah, S3 dan breadboard besar di tengah, speaker dan mikrofon berjauhan.',
        'Pasang spacer kit di antara kedua plat. Kabel yang menyeberang (UART 3 kabel, 5 V ke S3, 5 V ke kamera, servo) lewat lubang tengah.',
    ], figs=fig(tata_letak(), 'Barang yang sering dibuka (holder baterai, saklar) harus mudah dijangkau.'),
        ok_='Semua board terpasang kuat, tidak ada kabel yang bisa tersangkut roda, dan holder baterai bisa dibuka.')

    t += stage(S, 2, 'Rapikan kabel', ['Cable ties', 'Selotip kertas + spidol'], [
        'Ikat kabel per kelompok: daya (merah-hitam tebal), motor, servo, sinyal. Kabel daya dan motor dijauhkan dari kabel SPI layar dan mikrofon.',
        'Beri label di ujung kabel penting: "5V S3", "5V CAM", "5V DEVKIT", "UART TX", "UART RX".',
        'Pastikan kabel servo cukup panjang untuk kepala menoleh penuh ke kiri dan kanan tanpa tertarik.',
        'Kencangkan semua terminal sekrup sekali lagi.',
    ], ok_='Robot bisa diangkat dan digoyang pelan tanpa ada kabel jumper yang lepas.')

    t += stage(S, 3, 'Pindah ke mode jalan', ['Multimeter'], [
        'Saklar OFF, cabut semua kabel USB.',
        'Pasang kabel 5 V: OUT+ LM2596 #1 → pin <b>5V</b> blok daya papan 30P dan pin <b>5V</b> blok daya papan S3. OUT+ LM2596 #2 → pin 5V ESP32-CAM.',
        'Pindahkan Vin MAX98357A dari rel 3,3 V ke pin 5V papan S3, supaya speaker lebih keras dan tidak membebani 3,3 V.',
        'Cek dengan kontinuitas: semua GND (BMS P−, ketiga konverter, kedua papan ekspansi, ESP32-CAM, TB6612) saling bunyi.',
        'Saklar ON. Dalam 10 detik: LED biru DevKit menyala, wajah muncul di layar, speaker menyapa, bot Telegram mengirim pesan.',
        'Ukur sekali lagi OUT LM2596 #1 saat semua menyala: masih di atas 4,9 V.',
    ], ok_='Robot menyala penuh dari baterai tanpa kabel USB.',
        gagal=['Layar berkedip atau S3 restart saat speaker berbunyi keras: 5 V turun. Kecilkan volume, pastikan LM2596 #1 tidak memasok kamera, atau pasang kapasitor 470 µF di pin 5V papan S3.',
               'Bot menyapa tapi "badan belum terhubung": DevKit tidak mendapat 5 V (cek kabel ke blok daya 30P) atau kabel UART lepas saat dirakit.'])

    t += stage(S, 4, 'Uji pertama di lantai', ['Ruang kosong 2x2 m', 'Baterai penuh', 'HP dengan Telegram'], [
        'Taruh robot di lantai, jauh dari tangga dan barang mudah pecah. Tangan dekat tombol STOP.',
        'Kirim <code>/status</code>. Cek jarak depan dan belakang masuk akal.',
        'Kirim <code>/maju 500</code>. Robot maju setengah detik. Kalau berbelok sendiri, satu sisi lebih lemah; itu wajar untuk motor TT murah.',
        'Kirim <code>/kiri</code> lalu <code>/kanan</code>: robot berputar di tempat.',
        'Taruh kotak 30 cm di depan robot, kirim <code>/maju 3000</code>. Robot harus berhenti sebelum sekitar 20 cm.',
        'Tekan tombol STOP saat robot bergerak: langsung berhenti, Telegram memberi tahu. Tekan lagi untuk melepas.',
        'Coba <code>/ikuti</code> lalu berjalan pelan di depan robot. Coba <code>/jaga</code> lalu lewat di depannya setelah 4 detik.',
        'Terakhir, ngobrol: "Robi, maju sedikit lalu lihat ke kiri, ada apa di sana?"',
    ], ok_='Robot bergerak sesuai perintah, berhenti sendiri di depan halangan, dan tombol STOP selalu menang.',
        learn='Sensor ultrasonik punya sudut pandang sempit (sekitar 15°) dan tidak melihat benda tipis (kaki kursi) atau benda lembut. Robot ini aman untuk ruang tamu, tapi jangan dibiarkan berjalan sendiri dekat tangga: tidak ada sensor yang melihat lantai hilang.')

    t += stage(S, 5, 'Penyesuaian', ['Laptop'], [
        'Kecepatan: angka 150 di perintah maju adalah 59% tenaga. Kalau terlalu cepat untuk ruanganmu, ubah angka di <code>tanganiPesan</code> dan <code>tanganiSentuh</code> kode otak.',
        'Jarak berhenti: <code>JARAK_STOP_DEPAN</code> di kode badan. Lantai licin butuh jarak lebih besar karena robot tergelincir.',
        'Kepala: <code>TILT_MIN</code>, <code>TILT_MAX</code> di kode badan. Arah pan di AI: kalau "lihat ke kiri" malah menoleh ke kanan, ubah keterangan "pan 30=kanan 150=kiri" di <code>promptSistem</code>.',
        'Baterai: ulangi kalibrasi <code>BATERAI_FAKTOR</code> dengan multimeter kalau angkanya meleset.',
        'Kamera terbalik: <code>KAMERA_TERBALIK</code> di kode mata. Kepribadian robot: ubah kalimat di <code>promptSistem</code>.',
    ], ok_='Robot terasa pas untuk ruanganmu.')

    isi += section('rakit', 'Langkah', 'Rakit dan uji akhir', '<div class="parts" style="gap:16px">' + t + '</div>')

    isi += section('masalah', 'Bantuan', 'Kalau ada yang tidak jalan', table(['Gejala', 'Penyebab paling mungkin', 'Coba'], [
        ['Robot mati sendiri saat motor mulai bergerak', 'baterai lemah, atau BMS memutus karena lonjakan arus', 'isi baterai; cek sekring; kecepatan awal lebih rendah'],
        ['ESP32 restart acak saat motor jalan', 'noise motor, GND kurang baik', 'kapasitor 100 nF di tiap motor, 470 µF di VM, satukan GND di satu titik'],
        ['Satu sisi roda tidak berputar', 'kabel motor lepas, atau kanal TB6612 tidak dapat sinyal', 'Bagian 2 Tahap 3 dengan tes_motor'],
        ['Robot selalu berbelok saat maju', 'motor TT tidak sama kuat', 'normal; kurangi kecepatan sisi yang lebih kuat di perintah'],
        ['Selalu "E DITOLAK" padahal tidak ada halangan', 'sensor depan melihat lantai atau bodi robot', 'pasang HC-SR04 lebih tinggi dan lurus'],
        ['Layar putih setelah dirakit', 'kabel SPI atau CS lepas', 'Bagian 3 Tahap 2'],
        ['Foto gagal', 'IP kamera berubah, kamera kurang daya', 'DHCP reservation di router; kapasitor 1000 µF'],
        ['AI tidak menjawab (kode 401/400/429)', 'kunci salah / saldo habis / terlalu sering', 'cek di platform.claude.com'],
        ['Robot tidak bersuara', 'volume 0, Vin MAX98357A lepas, Google TTS menolak', '/volume 15; tes_speaker; jawaban tetap ada di Telegram'],
        ['Sentuhan meleset', 'kalibrasi lama', 'sentuh layar saat S3 menyala untuk kalibrasi ulang'],
        ['BMS tidak mengeluarkan daya setelah sekring putus atau korsleting', 'proteksi terkunci', 'lepas kedua sel, tunggu, pasang lagi dengan urutan 0V → 4.2V → 8.4V'],
    ]) + '<p class="cap">Masih bingung? Foto rangkaianmu dari atas dengan cahaya terang, tulis bagian dan tahap berapa, dan apa yang muncul di Serial Monitor.</p>')

    isi += section('rawat', 'Perawatan', 'Merawat baterai dan robot', ul([
        'Matikan saklar setelah dipakai. BMS tetap menarik arus kecil; jangan simpan berminggu-minggu dengan sel di holder.',
        'Isi baterai kalau robot mengirim peringatan baterai lemah (6,4 V total). Jangan pernah membiarkan sel di bawah 3,0 V.',
        'Sel yang panas saat diisi, menggembung, atau berbau: hentikan, taruh di tempat aman, jangan dipakai lagi.',
        'Sesekali kencangkan baut motor dan terminal sekrup, getaran membuatnya kendur.',
        'Ide berikutnya: encoder roda (code wheel dari kit chassis) untuk jarak tempuh yang tepat, sensor garis untuk mengikuti jalur, atau sensor jurang di bawah depan.',
    ]))
    lead = 'Semua bagian sudah lulus tes di meja. Sekarang dipasang di chassis, dipindah ke baterai, lalu diuji di lantai dengan tangan selalu dekat tombol STOP.'
    return S, 'Rakit akhir dan uji', lead, isi, 'Merakit robot AI 4WD di chassis dua lantai, pindah ke daya baterai, uji di lantai, dan daftar masalah umum.'
