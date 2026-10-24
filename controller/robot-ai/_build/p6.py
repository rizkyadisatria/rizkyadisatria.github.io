from build import *
import diag
import layout
import kabel as KB
import peta

S = '6-rakit'
IMG = '../img/'


def halaman():
    isi = ''
    t = ''
    tinggi = table(['Lantai', 'Ruang tinggi', 'Barang tertinggi', 'Sisa'], [
        ['1 (antara plat bawah dan atas)', '30 mm (spacer kit)', 'TB6612 + kabel, sekitar 25 mm', '± 5 mm'],
        ['2 (antara plat atas dan lantai 3)', '60 mm (2 spacer 30 mm bertumpuk)', 'DevKit di papan 30P + kabel tegak, sekitar 45 mm', '± 15 mm'],
        ['3 (paling atas)', 'tanpa batas', 'kepala pan-tilt + kamera, sekitar 70 mm', '-']])

    def baris(ukuran):
        return [[f'<b>{KB.NO[d[0]]}</b>', d[1], KB.p(d[0]), d[6]] for d in KB.DAFTAR if d[2] == ukuran]
    t22 = KB.total('22')
    kabel = table(['No', 'Kabel 22 AWG', 'Potong', 'Dari → ke'], baris('22') + [
        ['', '<b>Total</b>', '<b>' + ', '.join(f'{w} {n} cm' for w, n in t22.items()) + '</b>',
         f'ketiga warna cukup dari 1 m (sisa hitam sekitar {100 - t22["hitam"]} cm, jadi potong yang terpanjang dulu)']],
        'Perkiraan dari tata letak di atas: jarak mendatar + naik-turun antar-lantai (permukaan lantai 2 sekitar 3,3 cm di atas lantai 1, lantai 3 sekitar 6,3 cm di atas lantai 2), ditambah 3 cm untuk kupasan dan kelonggaran, galat sekitar ±2 cm. Kabel antar-lantai dihitung lewat lubang plat yang ada di peta tiap tahap. Angka yang sama dipakai di tiap langkah Bagian 1-4.') + table(
        ['No', 'Kabel 26 AWG dan jumper', 'Panjang', 'Dari → ke'], baris('26') + baris('jumper') + baris('-'),
        'Jalur 5 V cukup 26 AWG: arusnya paling besar sekitar 1 A, dan 25 cm pergi-pulang hanya menurunkan sekitar 0,07 V. Kabel jumper biasanya 20 cm; yang tertulis 20 cm ke atas pas-pasan, jadi sambung dengan kabel M-F kalau kurang.')

    t += stage(S, 1, 'Susun kering dan cek', ['Semua bagian yang sudah dites di Bagian 1-5', 'Plat chassis ketiga', '12 spacer kuningan M3 30+6 mm (6 kaki x 2)', 'Penggaris'], [
        'Saklar OFF dan keluarkan kedua sel selama merakit. Kalau plat atas chassis sudah terpasang, lepas dulu: barang lantai 1 dipasang saat plat atas masih terbuka.',
        'Kiri dan kanan di bagian ini dilihat dari belakang robot, menghadap ke depan. Taruh semua barang di tempatnya mengikuti gambar tata letak, <b>tanpa baut dan tanpa ikatan</b>: lantai 1 di plat bawah, lantai 2 di plat atas, lantai 3 di plat ketiga (plat atas dan plat ketiga dijajarkan di meja).',
        '<b>Lantai 1</b> (plat bawah): empat motor di sisi kiri-kanan, kaleng motor menghadap ke tengah. LM2596 #1 dan #2 rebah berurutan di jalur tengah. #1 di belakang dengan OUT menghadap ke belakang; <b>#2 diputar 180° dari foto susun kering</b> sehingga IN-nya menghadap ke belakang dan OUT ke depan. Ujung depan: MPU6050 datar (tulisan menghadap ke depan), TB6612 di depannya, WAGO 6V di kiri dan WAGO −6 di kanan. Ujung belakang: HC-SR04 belakang menghadap ke belakang, WAGO + di kiri dan WAGO − di kanan.',
        '<b>Lantai 2</b> (plat atas): holder 1 paling belakang dengan pegas (−) di kiri, holder 2 di depannya <b>dibalik</b> (pegas di kanan), BMS di depannya (pad 8.4V dan 0V di kiri), saklar di depan BMS sebelah kanan, sekring di depan saklar, breadboard 1 melintang di tengah, DevKit di papan 30P di kiri depan, XL4015 di kanan depan <b>diputar 180° dari foto</b> sehingga terminal IN (sisi kapasitor 100 µF 50 V) menghadap ke belakang, HC-SR04 depan di tepi depan, lurus ke depan.',
        '<b>Lantai 3</b> (plat ketiga): papan S3 di ujung belakang dengan port USB-C menghadap ke samping, breadboard 2 melintang di depannya (MAX98357A dan INMP441 di sini), TFT <b>rebah</b> menghadap ke atas di depannya, kepala pan-tilt dengan ESP32-CAM di ujung depan, speaker kotak berdiri di sisi kanan, tombol STOP di sayap tengah kiri.',
        'Cek tinggi: pasang plat atas sementara di spacer kit, letakkan barang lantai 2, tumpuk dua spacer kuningan di tiap kaki, taruh plat ketiga di atasnya. Lihat dari samping: tidak ada yang menyentuh plat di atasnya, dan sel baterai bisa ditarik keluar dari holder tanpa membuka lantai 3.',
        'Cek kabel: rentangkan tiap kabel yang sudah dipotong di Bagian 1-4 di jalurnya. Semua harus sampai tanpa tegang. Kalau ada yang kurang, sambung sekarang, sebelum plat ditutup.',
        'Foto dari atas dan dari samping, lalu angkat lagi plat ketiga dan plat atas.',
    ], pre=info('Seberapa yakin ini muat', 'Gambar di bawah berskala. Bentuk plat dibaca dari fotomu dengan kotak cutting mat sebagai penggaris (galat sekitar 1-2 mm), dan posisi motor serta barang diukur dari tiga foto susun keringmu (galat beberapa mm). Hasil hitungan: lantai 1 terisi 39%, lantai 2 62%, lantai 3 73% dari luas yang bisa dipakai, dengan jarak antar-barang minimal 3 mm dan tidak ada yang bertumpuk. Satu-satunya yang keluar dari tepi plat adalah speaker di lantai 3 (sekitar 8 mm), dan itu disengaja karena lebar robot tetap ditentukan roda. Yang belum terlihat dari foto atas: tinggi tiap lantai, karena itu ada langkah cek tinggi.'),
        figs=fig(layout.svg(), 'Tiga lantai dilihat dari atas, depan robot di kanan, kiri robot di atas gambar. Ketiga lantai memakai plat chassis yang sama.') + tinggi + kabel + '<div class="photos">' + photo(IMG + 'susun_l1.jpg', 'Susun kering lantai 1', 'Susun kering lantai 1 (depan di bawah). HC-SR04 di foto ini masih di depan; pindahkan ke ujung belakang.') + photo(IMG + 'susun_l2.jpg', 'Susun kering lantai 2', 'Lantai 2 (depan di bawah, jadi kiri robot di kanan foto). Holder 2 dan XL4015 nanti diputar 180°.') + photo(IMG + 'susun_l3.jpg', 'Susun kering lantai 3', 'Lantai 3: S3, breadboard 2, speaker, TFT rebah, kepala. Kepala di foto masih rebah; nanti berdiri dan dibaut ke plat.') + '</div>',
        ok_='Semua barang punya tempat, tidak ada yang menyentuh plat di atasnya, sel bisa dicabut, dan semua kabel yang sudah dipotong sampai ke ujungnya.',
        learn='Lantai 1 hanya setinggi 30 mm, jadi diisi barang rendah yang jarang disentuh: konverter dan sambungan daya. DevKit dan papan S3 butuh ruang tinggi untuk kabel jumper yang berdiri, jadi naik ke lantai 2 dan 3. Breadboard besar diganti dua breadboard 400 lubang, satu per board, supaya kabel tiap board pendek.')

    t += stage(S, 2, 'Pasang lantai 1', ['Plat bawah dengan motor', 'Kit spacer nilon M3 (6 mm)', 'Cable ties 3 x 100 mm', 'Selotip kertas + spidol', 'Bor 3 mm (kalau perlu)'], [
        'Plat atas masih dilepas, saklar OFF, sel di luar holder.',
        'Cara memasang spacer nilon: ulir spacer masuk lewat lubang plat dari atas, mur di bawah plat; modul di atas spacer, dikunci baut M3x6. LM2596 #1 dan #2 di jalur tengah dengan spacer <b>6 mm</b> (M3*6+6) di lubang pojoknya (#2 sudah diputar 180°). MPU6050 dengan spacer 6 mm, datar, tulisan menghadap ke depan.',
        '<b>Modul tanpa lubang</b> diikat cable ties: TB6612, keempat WAGO (tuas menghadap ke atas supaya masih bisa dibuka), dan HC-SR04 belakang (dua tie di sebelah luar tabung sensor, sedikit mendongak sekitar 5° supaya tidak membaca lantai). Masukkan tie lewat dua lubang atau celah plat di kiri-kanan modul, lingkarkan di atas modul, kunci di bawah plat. Kencangkan sampai tidak goyang, jangan lebih, lalu potong sisa ekornya rata.',
        'Kalau tidak ada lubang yang pas: tempel selotip kertas di titik bor, bor pelan 3 mm tanpa ditekan, jangan lebih dekat dari 5 mm ke tepi, karena akrilik mudah retak.',
        'Sambungan di lantai 1: kabel motor ke AO1/AO2 dan BO1/BO2 TB6612, hijau-hitam WAGO 6V/−6 ke VM/GND TB6612 dan kapasitor 470 µF di WAGO 6V/−6 (Bagian 2), kabel WAGO +/− ke IN kedua LM2596 (Bagian 1).',
        f'Pasang ujung lantai-1 dari kabel yang naik ke atas, lalu beri label di ujung atasnya: kabel 26 AWG 5 V + GND ke papan 30P ({KB.pn("5v30")}) dan ke papan S3 ({KB.pn("5vs3")}) di terminal OUT LM2596 #1, kabel 5 V + GND kamera ({KB.pn("5vcam")}) di OUT LM2596 #2 (Bagian 4), kabel pengukur baterai ({KB.pn("bat")}) di IN+ LM2596 #1, kabel daya servo ({KB.pn("servo+")}) di WAGO 6V/−6 (Bagian 2).',
        f'Kabel sinyal yang naik: 9 kabel F-F TB6612 (ke papan 30P, cukup 20 cm), 4 kabel F-F MPU6050 (ke header I2C papan 30P, {KB.pn("mpu")}), 4 kabel HC-SR04 belakang (ke breadboard 1, {KB.pn("hcb")}).',
        'Kabel XL4015 (merah-hitam dari WAGO +/−, hijau-hitam ke WAGO 6V/−6) juga dibiarkan menjulur ke atas.',
        'Kumpulkan kabel daya yang naik di lubang besar tengah plat atas, dan kabel sinyal di celah dekat TB6612. Ikat longgar per kelompok supaya tidak menyentuh roda.',
    ], figs=fig(peta.lantai1(), 'Dilihat dari atas, posisi barang sama persis dengan di robot (gambar tata letak Bagian 6). Nomor di gambar = nomor di tabel dan di langkah.') + peta.tabel(peta.LANTAI1),
        ok_='Semua barang lantai 1 tidak goyang, setiap kabel yang naik sudah berlabel di ujung atasnya, dan tidak ada kabel yang menyentuh roda.')

    t += stage(S, 3, 'Tutup plat atas dan pasang lantai 2', ['Plat atas', '12 spacer kuningan', 'Baut M3 pendek + mur', 'Kit spacer nilon M3', 'Cable ties'], [
        'Holder, BMS, saklar, dan sekring sudah tersambung ke WAGO +/− di lantai 1 sejak Bagian 1. Angkat rangkaian baterai itu ke atas plat atas, lalu turunkan plat pelan sambil memasukkan semua kabel yang naik lewat lubang dan celahnya. Jangan sampai ada kabel terjepit di antara plat dan spacer.',
        '<b>Kaki lantai 3</b>: tiap baut yang menahan plat atas di spacer kit diganti spacer kuningan: ulir jantannya masuk lewat lubang plat atas ke spacer kit. Tumpuk satu spacer lagi, jadi tiap kaki 60 mm. Pasang di keenam titik: 4 pojok dan 2 di sayap tengah, sehingga plat lantai 3 tidak melentur saat tombol STOP atau layar ditekan.',
        '<b>Holder 18650</b>: baut M3 pendek dari dalam holder lewat lubang tengah dasarnya, mur di bawah plat (holder 2 dibalik). Jangan pakai cable tie melintang di atas holder, karena sel jadi tidak bisa dicabut.',
        'BMS, saklar, dan sekring diikat cable ties. Breadboard 1 dipindah bersama kabel-kabel yang sudah menancap dari Bagian 2, lalu diikat dua cable tie melintang di kedua ujungnya (di atas rel yang kosong). Papan 30P dan XL4015 (sudah diputar 180°) dengan spacer nilon <b>8 mm</b> (M3*8+6). DevKit dengan kabel tegak jadi sekitar 53 mm, masih di bawah plat lantai 3 (60 mm). HC-SR04 depan berdiri di tepi depan dengan dua tie, lurus ke depan, sekitar 6 cm dari lantai.',
        'Sambung kabel daya yang naik: merah-hitam dari WAGO +/− ke IN+/IN− XL4015, hijau-hitam dari WAGO 6V/−6 ke OUT+/OUT− XL4015. Kencangkan terminal sekrup.',
        'Hitam GND dari LM2596 #1 ke pin GND blok daya papan 30P. Merah 5 V-nya <b>belum</b> ditancap (nanti di Tahap 6).',
        'Kabel sinyal: TB6612 ke pin S D25, D26, D27, D13, D32, D33 dan STBY/VCC ke 3V seperti Bagian 2 Tahap 3. MPU6050 ke header I2C. Kabel pengukur baterai ke <span class="pin">a20</span>.',
        '<b>HC-SR04 belakang</b> (jauh dari papan 30P, jadi lewat breadboard): VCC → <span class="pin">c30</span>, GND → garis biru, ECHO → <span class="pin">a12</span>, TRIG → <span class="pin">e18</span>. Kabel baru ' + KB.pn("hctrig") + ' dari <span class="pin">a18</span> → pin S baris D14. HC-SR04 depan seperti Bagian 2 Tahap 5.',
        'Lepas tombol 6 mm dari breadboard 1 (nanti diganti tombol 12 mm di lantai 3). Resistor pull-up 10 kΩ tetap di tempatnya.',
        'Kabel yang lanjut ke lantai 3 dikumpulkan di depan, dekat kaki spacer depan: 5 V + GND papan S3, 5 V + GND kamera, daya servo, dan 3 kabel UART (ujung betinanya ditancap sekarang ke D16, D17, dan G papan 30P).',
    ], figs=fig(peta.baterai(), 'Lantai 2 belakang.') + fig(peta.lantai2(), 'Dilihat dari atas, posisi barang sama persis dengan di robot (gambar tata letak Bagian 6). Nomor di gambar = nomor di tabel dan di langkah.') + peta.tabel(peta.BATERAI + peta.LANTAI2),
        ok_='Lantai 2 terpasang, semua barang tidak goyang, dan sel bisa dicabut dari holder tanpa membuka apa pun.')

    t += stage(S, 4, 'Pasang lantai 3', ['Plat ketiga', 'Baut M3 + kit spacer nilon', 'Cable ties', 'Tombol 12 mm', 'Kabel 26 AWG', 'Kabel M-F'], [
        'Isi plat ketiga selagi masih di meja: papan S3 (bersama breadboard 2 dan kabel-kabel dari Bagian 3) dengan spacer nilon <b>10 mm</b>, TFT rebah dengan spacer <b>20 mm</b> (M3*20+6) di lubang pojoknya, karena pin header dan colokan jumper ada di bawah layar, breadboard 2 diikat cable tie, kepala pan-tilt dibaut di depan dengan sumbu pan di tengah lebar plat, speaker berdiri diikat dua cable tie lewat lubang sayap tengah kanan.',
        f'Tombol STOP 12 mm di sayap tengah kiri: bor lubang pelan dengan mata bor bertingkat, atau ikat tombol pada potongan plat kecil. Solder dua kabel 26 AWG <b>{KB.pn("stop")}</b> ke dua kakinya, ujung lainnya ke potongan jumper jantan 3 cm.',
        'Naikkan kabel dari lantai 2 lewat lubang plat ketiga, lalu pasang plat ketiga di atas kaki dengan baut M3.',
        'Hitam GND dari LM2596 #1 ke GND blok daya papan S3 (merah 5 V-nya belum). UART (kabel 29, ' + KB.p("uart") + ', lebih panjang dari kabel jumper 20 cm: sambung F-F dengan M-F): D16 → IO17, D17 → IO18, G → G papan S3. Kabel 5 V kamera ke ESP32-CAM, daya servo sudah di kepala.',
        'Kabel oranye kedua servo turun ke lantai 2 lewat kabel M-F: pan → pin S D23, tilt → pin S D4. Kabel tombol STOP turun ke breadboard 1: satu ke <span class="pin">d27</span>, satu ke <span class="pin">g29</span> (menggantikan kaki diagonal tombol 6 mm).',
        'Saklar OFF: putar kepala dengan tangan penuh ke kiri dan kanan, lalu mengangguk. Kabel kamera dan servo tidak tertarik, dan kepala tidak menyentuh TFT.',
    ], figs=fig(peta.lantai3(), 'Dilihat dari atas, posisi barang sama persis dengan di robot (gambar tata letak Bagian 6). Nomor di gambar = nomor di tabel dan di langkah.') + peta.tabel(peta.LANTAI3),
        ok_='Lantai 3 terpasang dan kepala bisa bergerak penuh tanpa menarik kabel.')

    t += stage(S, 5, 'Rapikan kabel', ['Cable ties', 'Selotip kertas + spidol'], [
        'Ikat kabel per kelompok: daya (merah, hijau, hitam), motor, servo, sinyal. Kabel daya dan motor dijauhkan dari kabel SPI layar dan mikrofon.',
        'Beri label di ujung kabel penting yang belum berlabel: "5V S3", "5V CAM", "5V DEVKIT", "UART TX", "UART RX".',
        'Kencangkan semua terminal sekrup sekali lagi. Masukkan kedua sel (sel 1 lalu sel 2).',
    ], ok_='Robot bisa diangkat dan digoyang pelan tanpa ada kabel jumper yang lepas.')

    t += stage(S, 6, 'Pindah ke mode jalan', ['Multimeter'], [
        'Saklar OFF, cabut semua kabel USB.',
        'Tancapkan ujung merah 5 V dari LM2596 #1: satu ke pin <b>5V</b> blok daya papan 30P, satu ke pin <b>5V</b> blok daya papan S3. Kabel 5 V kamera dari LM2596 #2 sudah terpasang sejak Bagian 4.',
        'Pindahkan Vin MAX98357A dari rel 3,3 V ke pin 5V papan S3, supaya speaker lebih keras dan tidak membebani 3,3 V.',
        'Cek dengan kontinuitas: semua GND (BMS P−, ketiga konverter, kedua papan ekspansi, ESP32-CAM, TB6612) saling bunyi.',
        'Saklar ON. Dalam 10 detik: LED biru DevKit menyala, wajah muncul di layar, speaker menyapa, bot Telegram mengirim pesan.',
        'Ukur sekali lagi OUT LM2596 #1 saat semua menyala: masih di atas 4,9 V.',
    ], ok_='Robot menyala penuh dari baterai tanpa kabel USB.',
        gagal=['Layar berkedip atau S3 restart saat speaker berbunyi keras: 5 V turun. Kecilkan volume, pastikan LM2596 #1 tidak memasok kamera, atau pasang kapasitor 470 µF di pin 5V papan S3.',
               'Bot menyapa tapi "badan belum terhubung": DevKit tidak mendapat 5 V (cek kabel ke blok daya 30P) atau kabel UART lepas saat dirakit.'])

    t += stage(S, 7, 'Uji pertama di lantai', ['Ruang kosong 2x2 m', 'Baterai penuh', 'HP dengan Telegram'], [
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

    t += stage(S, 8, 'Penyesuaian', ['Laptop'], [
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
