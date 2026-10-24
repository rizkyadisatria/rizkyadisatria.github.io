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
    t += stage(S, 1, 'Tata letak tiga lantai', ['Semua bagian yang sudah dites', '12 spacer kuningan M3 30+6 mm (minimal 8)', 'Plat chassis ketiga (sama dengan dua yang lain)', 'Kit spacer nilon M3', 'Cable ties 3 x 100 mm', 'Bor kecil 3 mm', 'Selotip kertas'], [
        '<b>Siapkan plat lantai 3.</b> Lantai 3 memakai plat yang sama dengan chassis (berpinggang), jadi lubang dan celahnya sudah ada dan tidak perlu memotong papan baru. Karena bentuknya sempit, susunannya rapat (lihat kotak di bawah): kalau mau lebih longgar, ganti dengan papan persegi 256 x 152 mm (triplek, MDF, atau akrilik 3-4 mm) dan bor sendiri 6 lubangnya di titik yang sama dengan lubang spacer plat atas.',
        '<b>Pasang kaki lantai 3.</b> Lepas baut di atas tiap spacer kit (yang menahan plat atas). Ganti dengan spacer kuningan baru: ulir jantannya masuk lewat lubang plat atas ke spacer kit. Tumpuk satu spacer lagi di atasnya, jadi tiap kaki 60 mm. Plat lantai 3 belum dipasang dulu, supaya lantai 2 masih mudah diisi.',
        '<b>Lantai 1</b> (di atas plat bawah): empat motor ada di sisi kiri-kanan, jadi jalur tengah di antaranya kosong. <b>LM2596 #1 dan #2 rebah berurutan di jalur tengah</b>, memanjang ke depan (seperti di foto susun kering). Ujung depan: MPU6050 datar di tengah (tulisan menghadap ke depan), TB6612 di depannya, 2 WAGO 6 V di kiri-kanan. Ujung belakang: <b>HC-SR04 belakang</b> menghadap ke belakang dan sedikit mendongak (sekitar 5°) supaya tidak membaca lantai, 2 WAGO baterai di kiri-kanannya.',
        '<b>Lantai 2</b> (di atas plat atas): dua holder melintang di ujung belakang, saklar dan BMS di depannya, breadboard 1 melintang di tengah, lalu DevKit di papan 30P (kanan) dan XL4015 6 V (kiri) di bagian depan, HC-SR04 depan di tepi depan (sekitar 6 cm dari lantai, lurus ke depan). Port USB DevKit menghadap ke breadboard 1: colokan micro-USB masih bisa masuk karena letaknya lebih tinggi dari breadboard, tapi coba colok sekali sebelum diikat.',
        '<b>Lantai 3</b> (plat ketiga): papan S3 di ujung belakang dengan port USB-C menghadap ke samping, breadboard 2 melintang di depannya (MAX98357A dan INMP441 ditancapkan di sini), layar TFT <b>rebah</b> di depannya dengan layar menghadap ke atas, kepala pan-tilt dan ESP32-CAM di ujung depan. TFT yang rebah lebih rendah dari bagian kepala yang berputar, jadi tidak tertabrak saat kepala menoleh. <b>Speaker kotak berdiri</b> di sisi kanan, sebagian menonjol dari pinggang plat: ikat dengan dua cable tie lewat lubang sayap tengah kanan. Tombol STOP di sayap tengah kiri, di sebelah kaki spacer. <b>Kabel 5 V ke ESP32-CAM</b> ditarik dari LM2596 #2 di lantai 1 lewat lubang plat (sekitar 15 cm); pakai kabel 22 AWG.',
        '<b>Susun kering dulu</b>: taruh semua barang tanpa perekat dan tanpa baut, pasang lantai 3, lalu lihat dari samping. Tidak boleh ada yang menyentuh plat di atasnya, dan sel baterai harus bisa dicabut dari holder tanpa membongkar lantai 3.',
        'Kalau semua pas, kencangkan board yang berlubang dengan spacer nilon (papan ekspansi, LM2596, XL4015, MPU6050).',
        '<b>Holder 18650</b>: tiap holder punya satu lubang di tengah dasarnya. Pasang baut M3 pendek dari dalam holder dan mur di bawah plat. Jangan pakai cable tie melintang di atas holder, karena sel jadi tidak bisa dicabut.',
        '<b>Modul tanpa lubang</b> (TB6612, BMS, HC-SR04) diikat cable ties: masukkan tie lewat dua lubang atau celah plat di kiri-kanan modul, lingkarkan di atas modul, kunci di bawah plat. Plat chassis punya banyak lubang dan celah, jadi geser modul sampai ada di antara dua lubang yang ada. HC-SR04 berdiri tegak: dua tie, masing-masing di sebelah luar tabung sensor; lebih kokoh lagi dengan dudukan HC-SR04.',
        'Kencangkan tie sampai modul tidak goyang, jangan lebih: tie yang terlalu kencang bisa menekan komponen kecil atau melengkungkan papan. Potong sisa ekor tie rata dengan kepalanya supaya tidak tajam.',
        'Kalau tidak ada lubang yang pas di <b>plat akrilik</b>: tempel selotip kertas di titik bor, bor pelan 3-4 mm tanpa ditekan, jangan lebih dekat dari 5 mm ke tepi plat, karena akrilik mudah retak. Plat lantai 3 sudah punya lubang, jadi hampir tidak perlu dibor.',
        'Kabel antar-lantai (UART 3 kabel, 5 V ke papan 30P, 5 V ke papan S3, 5 V kamera, servo, ECHO dan TRIG sensor) lewat lubang di tengah plat. Sisakan sedikit kelonggaran supaya lantai 3 bisa diangkat saat perlu.',
    ], pre=info('Seberapa yakin ini muat', 'Gambar di bawah berskala. Bentuk plat dibaca dari fotomu dengan kotak cutting mat sebagai penggaris (galat sekitar 1-2 mm): ujung berbentuk busur sepanjang ±34 mm dan selebar 15,2 cm, pinggang selebar ±10 cm, dan dua sayap samping di tengah yang melebar lagi ke 15,2 cm. Posisi motor dan barang diukur dari tiga foto susun keringmu (galat beberapa mm). Hasil hitungan: lantai 1 terisi 39%, lantai 2 60%, lantai 3 73% dari luas yang bisa dipakai, dengan jarak antar-barang minimal 3 mm dan tidak ada yang bertumpuk. Satu-satunya yang keluar dari tepi plat adalah speaker di lantai 3 (sekitar 8 mm), dan itu disengaja karena lebar robot tetap ditentukan roda. Yang belum terlihat dari foto atas: tinggi tiap lantai. Karena itu susun kering dulu di langkah 6 sebelum menempel apa pun.'),
        figs=fig(layout.svg(), 'Tiga lantai dilihat dari atas, depan robot di kanan. Ketiga lantai memakai plat chassis yang sama. Layar, tombol, dan kepala ada di lantai paling atas.') + tinggi + '<div class="photos">' + photo(IMG + 'susun_l1.jpg', 'Susun kering lantai 1', 'Susun kering lantai 1 (depan di bawah). HC-SR04 di foto ini masih di depan; pindahkan ke ujung belakang.') + photo(IMG + 'susun_l2.jpg', 'Susun kering lantai 2', 'Lantai 2: holder, saklar, BMS, breadboard 1, XL4015, DevKit, HC-SR04 depan.') + photo(IMG + 'susun_l3.jpg', 'Susun kering lantai 3', 'Lantai 3: S3, breadboard 2, speaker, TFT rebah, kepala. Kepala di foto masih rebah; nanti berdiri dan dibaut ke plat.') + '</div>',
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
