from build import *
import diag
import layout

S = '6-rakit'
IMG = '../img/'


def halaman():
    isi = ''
    t = ''
    tinggi = table(['Lantai', 'Ruang tinggi', 'Barang tertinggi', 'Sisa'], [
        ['1 (antara plat bawah dan atas)', '30 mm (spacer kit)', 'TB6612 + kabel, sekitar 25 mm', '± 5 mm'],
        ['2 (antara plat atas dan lantai 3)', '60 mm (2 spacer 30 mm bertumpuk)', 'DevKit di papan 30P + kabel tegak, sekitar 45 mm', '± 15 mm'],
        ['3 (paling atas)', 'tanpa batas', 'kepala pan-tilt + kamera, sekitar 70 mm', '-']])
    t += stage(S, 1, 'Tata letak tiga lantai', ['Semua bagian yang sudah dites', '12 spacer kuningan M3 30+6 mm (minimal 8)', 'Papan lantai 3 (persegi 256 x 152 mm)', 'Kit spacer nilon M3', 'Cable ties 3 x 100 mm', 'Bor kecil 3 mm', 'Selotip kertas'], [
        '<b>Buat papan lantai 3.</b> Bentuknya <b>persegi panjang 256 x 152 mm</b>, bukan seperti plat chassis: plat chassis berpinggang sempit (10 cm), sehingga layar dan speaker tidak muat di atasnya. Pakai triplek, MDF, atau akrilik 3-4 mm (toko bahan akrilik atau tukang kayu bisa memotongnya; minta sudut dibulatkan). Letakkan plat atas chassis di atas papan, tandai 6 lubang spacer lewat lubang plat, geser papan sampai plat ada di tengah, lalu bor lubang 3 mm.',
        '<b>Pasang kaki lantai 3.</b> Lepas baut di atas tiap spacer kit (yang menahan plat atas). Ganti dengan spacer kuningan baru: ulir jantannya masuk lewat lubang plat atas ke spacer kit. Tumpuk satu spacer lagi di atasnya, jadi tiap kaki 60 mm. Plat lantai 3 belum dipasang dulu, supaya lantai 2 masih mudah diisi.',
        '<b>Lantai 1</b> (di atas plat bawah, di antara motor): dua LM2596 berjajar di pinggang di antara motor kiri dan kanan, XL4015 berdiri di sayap belakang, TB6612 dan MPU6050 di sayap depan (MPU6050 datar, tulisan menghadap ke depan), 2 WAGO baterai di sayap belakang dan 2 WAGO 6 V di sayap depan, HC-SR04 belakang di tepi belakang dan sedikit mendongak (sekitar 5°) supaya tidak membaca lantai.',
        '<b>Lantai 2</b> (di atas plat atas): dua holder di sayap belakang, BMS dan saklar di sebelahnya, lalu breadboard 1 dan DevKit di papan 30P berurutan ke depan (port micro-USB DevKit menghadap ke samping), HC-SR04 depan di tepi depan (sekitar 6 cm dari lantai, lurus ke depan).',
        '<b>Lantai 3</b>: papan S3 di belakang dengan port USB-C menghadap ke tepi, breadboard 2 di sebelahnya (MAX98357A dan INMP441 ditancapkan di breadboard ini), layar TFT berdiri miring di depannya menghadap ke atas-belakang, speaker kotak di belakang-kanan dengan corong menghadap ke atas (dua baut M3 di telinganya), tombol STOP 12 mm di tepi yang mudah ditekan, kepala pan-tilt dan ESP32-CAM paling depan.',
        '<b>Susun kering dulu</b>: taruh semua barang tanpa perekat dan tanpa baut, pasang lantai 3, lalu lihat dari samping. Tidak boleh ada yang menyentuh plat di atasnya, dan sel baterai harus bisa dicabut dari holder tanpa membongkar lantai 3.',
        'Kalau semua pas, kencangkan board yang berlubang dengan spacer nilon (papan ekspansi, LM2596, XL4015, MPU6050).',
        '<b>Holder 18650</b>: tiap holder punya satu lubang di tengah dasarnya. Pasang baut M3 pendek dari dalam holder dan mur di bawah plat. Jangan pakai cable tie melintang di atas holder, karena sel jadi tidak bisa dicabut.',
        '<b>Modul tanpa lubang</b> (TB6612, BMS, HC-SR04) diikat cable ties: masukkan tie lewat dua lubang atau celah plat di kiri-kanan modul, lingkarkan di atas modul, kunci di bawah plat. Plat chassis punya banyak lubang dan celah, jadi geser modul sampai ada di antara dua lubang yang ada. HC-SR04 berdiri tegak: dua tie, masing-masing di sebelah luar tabung sensor; lebih kokoh lagi dengan dudukan HC-SR04.',
        'Kencangkan tie sampai modul tidak goyang, jangan lebih: tie yang terlalu kencang bisa menekan komponen kecil atau melengkungkan papan. Potong sisa ekor tie rata dengan kepalanya supaya tidak tajam.',
        'Kalau tidak ada lubang yang pas di <b>plat akrilik</b>: tempel selotip kertas di titik bor, bor pelan 3-4 mm tanpa ditekan, jangan lebih dekat dari 5 mm ke tepi plat, karena akrilik mudah retak. Papan lantai 3 (triplek/MDF) bebas dibor.',
        'Kabel antar-lantai (UART 3 kabel, 5 V ke papan 30P, 5 V ke papan S3, 5 V kamera, servo, ECHO dan TRIG sensor) lewat lubang di tengah plat. Sisakan sedikit kelonggaran supaya lantai 3 bisa diangkat saat perlu.',
    ], pre=info('Seberapa yakin ini muat', 'Gambar di bawah berskala dan dihitung dari ukuran di listing plat Starlectric: 25,6 x 10 cm, lebar ujung 15,2 cm (jadi pinggang plat hanya 10 cm, lebih sempit dari chassis 4WD umum). Dengan ukuran itu, lantai 1 dan 2 muat (terisi 45% dan 50% dari luas yang bisa dipakai, jarak antar-barang minimal 4 mm), tetapi lantai 3 <b>tidak muat di plat berpinggang</b>, sehingga lantai 3 memakai papan persegi 256 x 152 mm (terisi 55%). Yang belum pasti: bentuk persis ujung plat (kuperkirakan dari foto), posisi lubang dan motor, serta ukuran tiap modul. Karena itu susun kering dulu di langkah 6 sebelum menempel apa pun.'),
        figs=fig(layout.svg(), 'Tiga lantai dilihat dari atas, depan robot di kanan. Lantai 1 dan 2 berbentuk plat chassis (pinggang sempit, ujung lebar); lantai 3 papan persegi. Layar, tombol, dan kepala ada di lantai paling atas.') + tinggi,
        ok_='Semua lantai terpasang, tidak ada barang yang menyentuh plat di atasnya, kabel tidak bisa tersangkut roda, dan sel baterai bisa dikeluarkan untuk diisi.',
        learn='Lantai 1 hanya setinggi 30 mm, jadi diisi barang rendah yang jarang disentuh: konverter dan sambungan daya. DevKit dan papan S3 butuh ruang tinggi untuk kabel jumper yang berdiri, jadi naik ke lantai 2 dan 3. Breadboard besar diganti dua breadboard 400 lubang, satu per board, supaya kabel tiap board pendek.')

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
