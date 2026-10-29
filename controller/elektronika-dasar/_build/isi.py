# Isi halaman Elektronika Dasar untuk IoT dan mikrokontroler.
# Tiap bab: penjelasan, gambar, contoh kasus nyata, cek pemahaman, latihan dengan kit pemula.
import gambar as g
from buat_alat import (section, table, warn, ok, info, ul, fig, latihan, tanya, kejadian, rumus, kalk_ohm,
                       kalk_led, kalk_pembagi, kalk_warna, kalk_baterai)

BAB = [
    ('bab1', 'Tegangan, arus, daya'),
    ('bab2', 'Rangkaian tertutup dan GND'),
    ('bab3', 'Seri dan paralel'),
    ('bab4', 'Sumber daya dan regulator'),
    ('bab5', 'Umur baterai'),
    ('bab6', 'Resistor dan pembagi'),
    ('bab7', 'Kapasitor dan noise'),
    ('bab8', 'Dioda dan LED'),
    ('bab9', 'Transistor, MOSFET, relay'),
    ('bab10', 'Pin mikrokontroler'),
    ('bab11', 'PWM, servo, motor'),
    ('bab12', 'Komunikasi dan modul'),
    ('bab13', 'Multimeter'),
    ('bab14', 'Breadboard dan solder'),
    ('bab15', 'Mencari kesalahan'),
    ('bab16', 'Keselamatan'),
]


def pendahuluan():
    isi = ('<p>Halaman ini berisi elektronika yang benar-benar dipakai saat membuat proyek mikrokontroler dan IoT: '
           'ESP32, ESP8266, Arduino, Raspberry Pi Pico, beserta sensor, relay, motor, dan baterainya. Bukan teori lengkap, '
           'tapi cukup untuk memahami kenapa rangkaian bekerja, kenapa tidak, dan cara memeriksanya.</p>'
           + ul(['<b>Baca berurutan.</b> Bab 1 dan 2 adalah dasar untuk semua bab lain. Bab 2 (GND bersama) adalah penyebab paling sering rangkaian yang "aneh".',
                 '<b>Jawab dulu, baru buka jawabannya.</b> Mengingat sendiri sebelum melihat jawaban membuat konsep lebih menempel daripada membaca ulang.',
                 '<b>Kerjakan latihannya.</b> Kotak centang tersimpan di browser ini, dan label status di halaman utama ikut berubah.'])
           + info('Yang kamu butuhkan', 'Multimeter digital, satu papan mikrokontroler (ESP32 atau Arduino), breadboard, kabel jumper, LED, resistor (220 Ω, 1 kΩ, 10 kΩ), '
                  'tombol, dan baterai (AA atau 18650). Kit pemula ESP32 atau Arduino biasanya sudah berisi semuanya. '
                  'Semua latihan memakai tegangan rendah. Jangan mencoba apa pun di listrik rumah 220 V.'))
    return section('mulai', 'Mulai di sini', 'Cara memakai halaman ini', isi)


def bab1():
    isi = ('<p><b>Tegangan</b> (volt, V) adalah selisih "dorongan" listrik di antara <b>dua titik</b>. Satu titik sendirian tidak punya tegangan. '
           '"Pin 3,3 V" artinya pin itu 3,3 V lebih tinggi daripada GND.</p>'
           '<p><b>Arus</b> (ampere, A) adalah banyaknya muatan listrik yang mengalir lewat kabel tiap detik. 1 mA = 0,001 A. '
           'Arus hanya mengalir kalau ada jalur tertutup: keluar dari sumber, lewat beban, lalu kembali ke sumber.</p>'
           '<p><b>Hambatan</b> (ohm, Ω) adalah seberapa kuat sebuah benda menahan arus. 1 kΩ = 1.000 Ω.</p>'
           + table(['Besaran', 'Satuan', 'Diukur dengan', 'Contoh angka di proyek IoT'], [
               ['Tegangan', 'volt (V)', 'multimeter V⎓, di antara dua titik', 'USB 5 V, logika ESP32 3,3 V, satu sel Li-ion 3,0-4,2 V'],
               ['Arus', 'ampere (A), mA', 'multimeter A, disisipkan di jalur', 'LED 10 mA, sensor 1-5 mA, ESP32 saat WiFi kirim data ratusan mA'],
               ['Hambatan', 'ohm (Ω), kΩ', 'multimeter Ω, daya mati', 'resistor LED 220 Ω, pull-up 10 kΩ'],
               ['Daya', 'watt (W), mW', 'dihitung: V × I', 'papan ESP32 aktif sekitar 0,5 W']])
           + '<p><b>Bayangkan seperti air di pipa:</b> tegangan seperti tekanan air, arus seperti banyaknya air yang mengalir, hambatan seperti pipa yang menyempit. '
           'Pakai gambaran ini hanya untuk membayangkan. Listrik tidak "habis" di jalan: arus yang keluar dari kutub + sama besar dengan yang kembali ke kutub −.</p>'
           + '<h3>Hukum Ohm dan daya</h3>'
           + rumus('V = I × R     P = V × I', 'I = V / R, R = V / I. Daya (P) yang hilang di resistor berubah jadi panas.')
           + '<p>Contoh: LED merah dan resistor 220 Ω di 6 V. LED "memakan" sekitar 2 V, jadi resistor menanggung 6 − 2 = 4 V. '
           'Arusnya I = 4 / 220 = 0,018 A = 18 mA, dan resistor memanaskan 4 × 0,018 = 0,07 W. Tanpa resistor, arusnya tidak dibatasi apa pun dan LED atau pin rusak.</p>'
           + fig(g.rangkaian(), 'Tegangan diukur di dua ujung resistor. Arusnya sama di setiap titik rangkaian yang tertutup.')
           + kalk_ohm()
           + kejadian('Multimeter di baterai dua sel Li-ion seri menunjukkan 7,8 V. Itu tegangan di antara probe merah (kutub +) dan probe hitam (kutub −), '
                      'sekitar 3,9 V per sel: baterai cukup terisi. Satu sel Li-ion penuh 4,2 V.')
           + tanya([
               ('Multimeter menunjukkan 3,3 V saat probe merah di pin 3V3. Di antara titik mana sebenarnya angka itu?',
                'Di antara probe merah (pin 3V3) dan probe hitam (biasanya GND). Tegangan selalu antara dua titik; pindah probe hitam, angkanya bisa berubah.'),
               ('Resistor 1 kΩ dipasang langsung di 5 V. Berapa arus dan dayanya?', 'I = 5 / 1.000 = 5 mA. P = 5 × 0,005 = 0,025 W = 25 mW.'),
               ('Papan menarik 150 mA dari 5 V. Berapa dayanya?', '5 × 0,15 = 0,75 W.')])
           + latihan(1, 'Mengukur tegangan dan hambatan', [
               'Putar kenop multimeter ke <b>V⎓</b> (rentang 20 V kalau multimetermu manual). Probe hitam di <b>COM</b>, merah di <b>VΩmA</b>.',
               'Ukur satu baterai: merah ke +, hitam ke −. AA baru sekitar 1,5-1,6 V; sel Li-ion 3,0-4,2 V.',
               'Tukar probe merah dan hitam. Angkanya jadi minus dengan besar yang sama. Aman di DC, hanya berarti probe terbalik.',
               'Ukur pin 3V3 dan 5V/VIN papan mikrokontrolermu ke GND saat tersambung USB.',
               'Ukur resistor 1 kΩ di mode <b>Ω</b>, tidak terpasang di rangkaian. Terbaca sekitar 0,95-1,05 kΩ (toleransi 5%) atau 0,99-1,01 kΩ (toleransi 1%).']))
    return section('bab1', 'Bab 1', 'Tegangan, arus, hambatan, dan daya', isi, 'Empat besaran ini ada di balik setiap angka di lembar data modul.')


def bab2():
    isi = ('<p>Arus berjalan berkeliling: keluar dari kutub +, lewat beban, lalu kembali ke kutub −. Kalau jalurnya putus di satu titik saja '
           '(kabel lepas, solderan retak, saklar OFF, sekering putus), arus berhenti di seluruh jalur itu. Saklar dan sekering bekerja dengan cara ini.</p>'
           '<h3>Apa itu GND</h3>'
           '<p><b>GND</b> (ground) adalah titik yang disepakati sebagai 0 V. Semua tegangan lain diukur dari sana. '
           'Di proyek mikrokontroler, GND adalah kutub − yang dipakai bersama. Ini bukan tanah dan bukan kabel bumi listrik rumah, hanya nama untuk titik acuan.</p>'
           '<h3>Sinyal juga butuh jalur pulang</h3>'
           '<p>Saat mikrokontroler mengirim sinyal ke sebuah modul, sinyal itu adalah tegangan 3,3 V atau 5 V <b>terhadap GND mikrokontroler</b>. '
           'Modul membacanya terhadap GND-nya sendiri. Kalau dua GND itu tidak tersambung, modul tidak tahu "3,3 V terhadap apa". Sinyalnya tidak punya jalur pulang, '
           'dan tegangan yang dibaca modul <b>melayang</b> (floating): berubah-ubah sendiri, dipengaruhi apa saja di sekitarnya, termasuk tanganmu.</p>'
           + fig(g.gnd_bersama(), 'Kiri: mikrokontroler dari USB dan modul dari adaptor sendiri, tanpa kabel GND bersama. Kanan: satu kabel GND menutup jalur pulang sinyal.')
           + ok('Aturan yang perlu diingat', 'Setiap dua perangkat yang saling mengirim sinyal harus punya <b>GND bersama</b>, walaupun dayanya dari sumber berbeda. '
                'Yang disatukan hanya GND. Kutub + dari sumber yang berbeda (USB 5 V, adaptor 5 V, baterai) <b>jangan</b> disatukan: dua sumber dengan tegangan sedikit berbeda akan saling melawan.')
           + kejadian('Servo diberi daya dari konverter terpisah, mikrokontroler dari USB, dan tidak ada kabel di antara GND keduanya. '
                      'Gejalanya: satu servo diam, satu bergetar, dan menyentuh logam di jalur daya membuat servo bergerak. Tubuh manusia selalu menangkap sedikit tegangan '
                      'dari kabel listrik rumah di sekitarnya, dan titik yang melayang mudah terpengaruh olehnya. Satu kabel GND menyelesaikan semuanya. '
                      'Pola yang sama muncul di driver motor yang tidak mau jalan, modul relay yang klik sendiri, dan sensor yang membaca angka acak.')
           + tanya([
               ('Dua papan mikrokontroler saling berkirim data lewat UART (TX, RX). Masing-masing punya adaptor sendiri. Apa yang wajib disambung selain TX dan RX?',
                'GND kedua papan. Tanpa GND bersama, data UART terbaca kacau atau tidak terbaca sama sekali.'),
               ('Bolehkah 5 V dari USB disambung ke 5 V dari adaptor supaya "lebih kuat"?',
                'Jangan. Dua sumber dengan tegangan sedikit berbeda saling mendorong arus, dan arus bisa mengalir balik ke port USB laptop. Yang disatukan hanya GND.'),
               ('Kenapa saklar OFF mematikan seluruh rangkaian padahal hanya memutus satu kabel?',
                'Karena arus butuh jalur tertutup. Satu titik yang putus menghentikan arus di seluruh jalur itu.')])
           + latihan(2, 'Memeriksa sambungan dengan tes bip', [
               'Semua daya mati (USB dicabut, baterai dilepas). Kenop multimeter ke mode bip (ikon gelombang suara atau dioda).',
               'Tempelkan dua ujung probe satu sama lain: harus bunyi. Itu tanda mode bip bekerja.',
               'Tes bip pin GND mikrokontroler ke kutub − modul atau sumber daya lain di rangkaianmu. Harus bunyi.',
               'Tes bip satu kabel jumper dari ujung ke ujung. Jumper yang putus di dalam sering tidak terlihat dari luar.',
               'Tes bip pin 3V3 ke GND papan. Tidak boleh bunyi terus. Bunyi sebentar lalu diam itu normal: kapasitor di papan sedang terisi oleh baterai multimeter.']))
    return section('bab2', 'Bab 2', 'Rangkaian tertutup dan GND bersama', isi, 'Bab terpenting di halaman ini. Kebanyakan rangkaian yang "aneh" ternyata kehilangan jalur pulang.')


def bab3():
    isi = ('<p><b>Seri</b>: disambung berurutan, kutub + satu ke kutub − berikutnya. <b>Tegangan dijumlah</b>, arusnya sama di semua bagian. '
           'Dua sel Li-ion seri: 3,7 + 3,7 = 7,4 V (nominal), 8,4 V saat penuh. Empat baterai AA seri: 4 × 1,5 = 6 V.</p>'
           '<p><b>Paralel</b>: semua + jadi satu, semua − jadi satu. <b>Tegangannya sama</b>, arus dijumlah. Semua modul di rel 3,3 V atau 5 V terpasang paralel: '
           'masing-masing mendapat tegangan yang sama, dan sumber harus sanggup memberi jumlah arus semuanya.</p>'
           + fig(g.seri_paralel(), 'Seri menaikkan tegangan, paralel membagi arus ke beberapa beban dengan tegangan yang sama.')
           + table(['Resistor', 'Rumus', 'Contoh'], [
               ['Seri', 'R total = R1 + R2', '1 kΩ + 2 kΩ = 3 kΩ'],
               ['Paralel', '1 / R total = 1 / R1 + 1 / R2', '10 kΩ paralel 10 kΩ = 5 kΩ (selalu lebih kecil dari yang terkecil)']])
           + kejadian('Tiga modul I2C dipasang di bus yang sama, dan masing-masing membawa resistor pull-up 4,7 kΩ. Tiga resistor itu jadi paralel: totalnya sekitar 1,6 kΩ. '
                      'Biasanya masih bekerja, tapi makin banyak modul, makin kecil totalnya dan makin besar arus yang harus ditarik pin. '
                      'Contoh lain: beberapa LED paralel memakai satu resistor bersama. LED dengan Vf paling kecil mengambil arus terbanyak dan cepat rusak. Beri satu resistor per LED.')
           + tanya([
               ('Sensor 5 mA, layar OLED 20 mA, dan ESP32 rata-rata 80 mA dipasang di rel 3,3 V yang sama. Berapa arus dari regulator?', '105 mA. Paralel: arusnya dijumlah.'),
               ('Satu dari empat baterai AA seri terpasang terbalik. Kira-kira berapa tegangan totalnya?', 'Sekitar 3 V, bukan 6 V: satu baterai terbalik mengurangi, bukan menambah (1,5 + 1,5 + 1,5 − 1,5).'),
               ('Berapa total dua resistor 2 kΩ yang dipasang paralel?', '1 kΩ. Dua resistor sama besar yang paralel = setengah nilainya.')])
           + latihan(3, 'Seri dan paralel', [
               'Ukur dua baterai satu per satu, lalu total keduanya saat seri di holder. Total kira-kira sama dengan jumlah keduanya.',
               'Pasang dua resistor 1 kΩ seri di breadboard, ukur ujung ke ujung: sekitar 2 kΩ.',
               'Pasang keduanya paralel (kedua kakinya di kolom yang sama), ukur: sekitar 500 Ω.']))
    return section('bab3', 'Bab 3', 'Seri dan paralel', isi, 'Dua cara menyusun komponen, dan kenapa semua modul di rel yang sama berbagi arus dari satu sumber.')


def bab4():
    isi = ('<p>Hampir setiap proyek mikrokontroler butuh lebih dari satu tegangan: 3,3 V untuk chip, 5 V untuk sebagian modul, dan kadang 6-12 V untuk motor atau LED strip. '
           'Tegangan itu dibuat dari satu sumber dengan <b>regulator</b>.</p>'
           + table(['Sumber', 'Tegangan', 'Catatan'], [
               ['USB laptop atau charger', '5 V', 'port laptop biasanya 0,5-0,9 A; kabel tipis atau panjang menurunkan tegangan saat arus besar'],
               ['Adaptor DC', '5, 9, 12 V', 'cek tulisan arus maksimumnya; polaritas colokan bisa berbeda'],
               ['Baterai AA alkaline', '1,5 V per buah (turun ke sekitar 1,0 V)', 'tidak bisa diisi ulang'],
               ['Baterai NiMH', '1,2 V per buah', 'bisa diisi ulang dengan charger NiMH'],
               ['Li-ion 18650 / LiPo', '3,7 V nominal (3,0-4,2 V)', 'perlu modul pengisi khusus (misalnya TP4056) dan pelindung'],
               ['LiFePO4', '3,2 V nominal', 'lebih tahan, tegangan lebih rata'],
               ['Koin CR2032', '3 V', 'hanya untuk arus kecil, misalnya RTC atau sensor yang hampir selalu tidur']])
           + '<h3>Regulator linear dan konverter switching</h3>'
           + table(['Jenis', 'Contoh', 'Cara kerja', 'Kapan dipakai'], [
               ['Linear (LDO)', 'AMS1117, regulator di papan ESP32', 'kelebihan tegangan dibuang sebagai panas', 'selisih tegangan kecil, arus kecil'],
               ['Buck (penurun)', 'LM2596, MP1584, XL4015', 'transistor menyala-mati sangat cepat, dihaluskan koil dan kapasitor', 'selisih besar atau arus besar'],
               ['Boost (penaik)', 'MT3608', 'sama, tapi menaikkan tegangan', 'Li-ion 3,7 V ke 5 V'],
               ['Buck-boost', 'modul "auto buck-boost"', 'naik atau turun sesuai kebutuhan', 'tegangan baterai melewati tegangan target']])
           + rumus('Panas regulator linear = (Vin − Vout) × I', 'contoh: 12 V ke 5 V pada 0,5 A = 7 × 0,5 = 3,5 W, cukup untuk membuat regulator kecil terlalu panas')
           + '<p>Konverter buck untuk contoh yang sama hanya membuang sekitar 10-15% dayanya sebagai panas. Regulator linear juga butuh selisih minimum (dropout): '
           'AMS1117 butuh sekitar 1,1-1,3 V di atas keluarannya, jadi AMS1117-3.3 butuh masukan sekitar 4,5 V atau lebih.</p>'
           + warn('Ukur dulu, baru sambung', 'Atur trimpot konverter sampai tegangan keluarannya benar <b>sebelum</b> menyambung papan atau modul. Konverter baru dari pabrik bisa mengeluarkan tegangan hampir setinggi masukannya.')
           + '<h3>Pin 5V, VIN, dan 3V3 di papan</h3>'
           + ul(['<b>VIN atau 5V</b> masuk ke regulator papan. Di papan ESP32 umumnya diberi 5 V. Cek dokumentasi papanmu sebelum memberi tegangan lain.',
                 '<b>3V3</b> adalah keluaran regulator papan. Boleh dipakai untuk sensor kecil, tapi regulator kecil di papan cepat kewalahan kalau dibebani motor atau banyak modul.',
                 '<b>Jangan memberi daya dari USB dan dari pin 5V sekaligus</b>, kecuali papanmu jelas punya dioda pemisah. Arus bisa mengalir balik ke port USB.'])
           + kejadian('Papan ESP32 restart sendiri tepat saat WiFi mulai tersambung, dan Serial Monitor menulis <code>Brownout detector was triggered</code>. '
                      'Saat memancar, ESP32 menarik lonjakan arus sesaat (lembar datanya menyebut sampai sekitar 240 mA). Kabel USB yang tipis atau panjang, port yang lemah, '
                      'atau modul lain di rel yang sama membuat tegangan turun sesaat sampai chip reset. Solusinya: kabel USB yang lebih baik, sumber yang lebih kuat, '
                      'kapasitor 100-470 µF di dekat pin daya, dan beban besar di sumber terpisah (dengan GND bersama).')
           + tanya([
               ('Kamu memberi 12 V ke modul yang memakai AMS1117-3.3 dan menarik 200 mA. Berapa panas di regulator?', '(12 − 3,3) × 0,2 = 1,74 W. Untuk regulator kecil tanpa pendingin itu terlalu panas. Turunkan dulu ke 5 V dengan buck.'),
               ('Baterai Li-ion 3,7 V mau dipakai untuk modul yang butuh 5 V. Konverter apa?', 'Boost (penaik), misalnya MT3608. Atau pilih modul yang bisa langsung bekerja di 3,3 V.'),
               ('Kenapa motor dan ESP32 sebaiknya tidak dipasok dari regulator yang sama?', 'Lonjakan arus motor menurunkan tegangan sesaat. Kalau ESP32 ikut di jalur itu, ia bisa brownout dan reset.')])
           + latihan(4, 'Mengukur sumber daya', [
               'Ukur pin 5V papan saat tersambung USB, lalu bandingkan dengan kabel USB lain (lebih pendek atau lebih tebal).',
               'Kalau punya konverter buck, ukur keluarannya tanpa beban dan atur trimpotnya ke 5,0 V.',
               'Raba regulator di papanmu setelah papan menyala beberapa menit dengan WiFi aktif (hati-hati, bisa hangat). Hangat itu normal; terlalu panas untuk disentuh berarti bebannya terlalu besar.']))
    return section('bab4', 'Bab 4', 'Sumber daya dan regulator', isi, 'Dari mana 3,3 V dan 5 V di proyekmu berasal, dan kenapa regulator bisa panas.')


def bab5():
    isi = ('<p>Perangkat IoT yang memakai baterai jarang menyala terus. Biasanya ia bangun sebentar (membaca sensor, mengirim data lewat WiFi), lalu tidur. '
           'Yang menentukan umur baterai adalah <b>arus rata-rata</b>, bukan arus saat aktif.</p>'
           + rumus('I rata-rata = (I aktif × t aktif + I tidur × t tidur) / t siklus', 'umur (jam) ≈ kapasitas (mAh) / I rata-rata (mA), lalu dikali sekitar 0,7 sebagai cadangan')
           + '<p>Contoh: aktif 120 mA selama 5 detik, lalu tidur 10 menit. Kalau tidurnya benar-benar 0,01 mA (10 µA), rata-ratanya sekitar 1 mA, '
           'dan baterai 2000 mAh bertahan sekitar dua bulan. Kalau papan masih menarik 5 mA saat tidur, rata-ratanya sekitar 6 mA, dan baterai yang sama hanya bertahan sekitar sepuluh hari. '
           'Arus tidur yang kecil itu ternyata menentukan segalanya.</p>'
           + kalk_baterai()
           + '<h3>Pemakan arus yang tidak terlihat</h3>'
           + ul(['<b>LED daya</b> di papan yang selalu menyala: beberapa mA.',
                 '<b>Regulator linear</b> seperti AMS1117 menarik beberapa mA untuk dirinya sendiri (arus diam, <i>quiescent</i>), walaupun bebannya tidur. Untuk baterai, pilih regulator dengan arus diam beberapa µA.',
                 '<b>Chip USB-serial</b> di papan pengembangan (CP2102, CH340) yang tetap mendapat daya.',
                 '<b>Sensor dan modul</b> yang tetap menyala. Beri daya sensor dari pin GPIO atau lewat transistor supaya bisa dimatikan saat tidur, kalau arusnya kecil.',
                 '<b>Pembagi tegangan</b> untuk membaca baterai: 20 kΩ + 10 kΩ di 8,4 V terus menarik 0,28 mA. Pakai resistor lebih besar atau matikan saat tidur.'])
           + kejadian('Lembar data ESP32 menyebut sekitar 10 µA saat deep sleep, tapi satu pengukuran papan ESP32-DevKitC V4 menemukan sekitar 3,8 mA saat deep sleep: '
                      'hampir 400 kali lebih besar. Penyebabnya LED daya, regulator AMS1117, dan chip USB di papan itu, bukan ESP32-nya. Untuk proyek baterai, pilih papan yang memang dirancang hemat daya, atau ukur sendiri arusnya.')
           + tanya([
               ('Dua papan sama-sama aktif 5 detik tiap 10 menit. Papan A tidur 0,01 mA, papan B tidur 5 mA. Mana yang paling menentukan perbedaan umur baterainya?', 'Arus tidur. Papan tidur hampir 99% dari waktunya, jadi 5 mA saat tidur jauh lebih berpengaruh daripada 120 mA selama 5 detik.'),
               ('Baterai 1000 mAh dan arus rata-rata 2 mA. Kira-kira berapa lama?', '1000 / 2 = 500 jam, dikali 0,7 ≈ 350 jam, sekitar dua minggu.')])
           + latihan(5, 'Menghitung dan mengukur konsumsi', [
               'Cari arus aktif dan arus tidur papanmu di dokumentasinya, lalu masukkan ke kalkulator di atas.',
               'Ubah interval siklus dari 10 menit ke 60 menit dan lihat seberapa besar umur baterai berubah.',
               'Kalau berani mengukur arus: lepas kabel + dari baterai ke papan, pasang multimeter mode mA di antaranya (probe merah ke colokan mA). Catat arus saat aktif dan saat tidur. Kembalikan probe ke colokan VΩ setelah selesai.']))
    return section('bab5', 'Bab 5', 'Konsumsi daya dan umur baterai', isi, 'Hitungan yang membedakan perangkat IoT yang bertahan berbulan-bulan dari yang habis dalam seminggu.')


def bab6():
    isi = ('<p>Resistor membatasi arus (misalnya untuk LED) atau membagi tegangan. Nilainya ditulis dengan gelang warna: dua atau tiga gelang pertama angka, lalu pengali, '
           'lalu toleransi (seberapa jauh nilai aslinya boleh meleset). Resistor kecil di kit biasanya 1/4 W: cukup untuk sinyal dan LED, bukan untuk arus besar.</p>'
           + kalk_warna()
           + '<h3>Pembagi tegangan</h3>'
           '<p>Dua resistor seri membagi tegangan sesuai perbandingannya. Titik tengahnya memberi tegangan lebih kecil. Pemakaian yang paling sering: '
           'menurunkan sinyal 5 V ke pin 3,3 V (misalnya echo sensor jarak HC-SR04), dan membaca tegangan baterai yang lebih tinggi dari 3,3 V dengan ADC.</p>'
           + rumus('Vout = Vin × R2 / (R1 + R2)', 'R2 adalah resistor yang tersambung ke GND')
           + fig(g.pembagi(), 'Pembagi 1 kΩ dan 2 kΩ menurunkan 5 V menjadi 3,33 V.')
           + kalk_pembagi()
           + '<h3>Potensiometer dan sensor resistif</h3>'
           '<p>Potensiometer adalah pembagi tegangan yang bisa diputar: kaki tengahnya memberi tegangan di antara dua kaki luar. '
           'Banyak sensor bekerja sama: LDR (hambatan berubah oleh cahaya) dan termistor (berubah oleh suhu) dipasang seri dengan resistor tetap, dan titik tengahnya dibaca ADC.</p>'
           + kejadian('Modul sensor jarak HC-SR04 bekerja di 5 V dan mengeluarkan sinyal echo 5 V. Di Arduino Uno (5 V) itu langsung boleh. '
                      'Di ESP32 atau Pico (3,3 V), echo dilewatkan pembagi 1 kΩ dan 2 kΩ dulu, karena pin 3,3 V tidak dirancang untuk 5 V.')
           + tanya([
               ('Resistor 4 gelang merah-merah-coklat-emas. Berapa nilainya?', '22 × 10 = 220 Ω, toleransi ±5%.'),
               ('Kamu ingin membaca baterai sampai 8,4 V dengan ADC yang maksimal sekitar 3,1 V. Pembagi apa yang cocok?', 'Misalnya 20 kΩ (atas) dan 10 kΩ (bawah): 8,4 × 10 / 30 = 2,8 V. Kode lalu mengalikan hasilnya dengan 3.'),
               ('LDR dan resistor 10 kΩ dipasang sebagai pembagi. Kalau ruangan makin terang, hambatan LDR turun. Apa yang terjadi pada tegangan titik tengah kalau LDR di posisi atas (R1)?', 'Naik. R1 makin kecil, jadi bagian R2 dari total makin besar.')])
           + latihan(6, 'Resistor dan pembagi', [
               'Baca gelang warna dua resistor di kit, cocokkan dengan pembaca warna di atas, lalu ukur dengan mode Ω.',
               'Rangkai pembagi 1 kΩ + 2 kΩ di breadboard dari 5 V ke GND. Ukur titik tengah ke GND: sekitar 3,3 V.',
               'Ganti R2 dengan LDR (kalau ada di kit). Ukur titik tengah saat LDR ditutup tangan dan saat disinari senter.']))
    return section('bab6', 'Bab 6', 'Resistor dan pembagi tegangan', isi, 'Komponen paling sederhana, dan dasar hampir semua pembacaan sensor analog.')


def bab7():
    isi = ('<p>Kapasitor menyimpan muatan sebentar lalu melepaskannya dengan cepat. Di proyek mikrokontroler ada dua tugas utama:</p>'
           + ul(['<b>Decoupling (100 nF)</b> di dekat kaki daya setiap IC atau modul: menyerap noise cepat dan memberi arus sesaat saat chip berganti keadaan. Kebanyakan modul sudah membawanya.',
                 '<b>Penyangga (10-1000 µF)</b> di titik masuk daya, terutama di dekat motor, servo, relay, LED strip, atau modul radio: melepaskan muatan saat beban tiba-tiba menarik arus, sehingga tegangan tidak turun.'])
           + fig(g.kapasitor(), 'Elektrolit berkutub dan bernilai besar; keramik tidak berkutub dan bernilai kecil.')
           + table(['Kode keramik', 'Hitungan', 'Nilai'], [
               ['104', '10 × 10⁴ pF', '100.000 pF = 100 nF = 0,1 µF'],
               ['103', '10 × 10³ pF', '10 nF'],
               ['105', '10 × 10⁵ pF', '1 µF']])
           + warn('Kapasitor elektrolit', 'Kaki panjang (+) ke sisi positif, sisi bergaris (−) ke GND. Pakai kapasitor yang tegangan tertulisnya jelas di atas tegangan kerja, '
                  'misalnya 10 V atau 16 V untuk rel 5 V. Kapasitor terbalik bisa panas, bocor, menggembung, atau meletus.')
           + '<h3>Noise: dari mana dan cara meredamnya</h3>'
           + ul(['<b>Sumber noise:</b> motor (percikan di sikat), relay dan solenoid saat dimatikan, konverter switching, dan kabel panjang yang menangkap gangguan dari sekitarnya.',
                 '<b>Pisahkan jalur daya</b> beban besar dari jalur daya chip, lalu satukan GND di satu titik.',
                 '<b>Kabel sinyal sependek mungkin</b>, dan jauhkan dari kabel motor. Kabel sensor yang panjang boleh dipilin bersama kabel GND-nya.',
                 '<b>Kapasitor kecil (100 nF) di kaki motor DC</b> meredam percikan dari sikatnya.',
                 '<b>Di kode:</b> rata-ratakan beberapa pembacaan ADC, dan abaikan perubahan yang terlalu cepat untuk masuk akal.'])
           + kejadian('Servo bergerak acak, lalu kapasitor 470 µF di jalur dayanya dilepas untuk diuji. Tidak ada yang berubah. Itu masuk akal: kapasitor meredam lonjakan, '
                      'tapi tidak bisa menggantikan GND yang putus, sambungan yang longgar, atau servo yang menabrak batas geraknya. Kapasitor bukan obat untuk semua gejala aneh.')
           + tanya([
               ('Kapasitor keramik bertulisan 103. Berapa nilainya?', '10 × 10³ pF = 10.000 pF = 10 nF.'),
               ('Modul radio sering reset saat mengirim data. Di mana kapasitor penyangga sebaiknya dipasang?', 'Sedekat mungkin dengan pin daya modul itu (VCC ke GND), dengan kutub yang benar.'),
               ('Boleh memakai kapasitor elektrolit 6,3 V di rel 5 V?', 'Bisa bekerja, tapi tipis. Lebih aman 10 V atau 16 V, karena tegangan bisa sesaat naik dan kapasitor yang dekat batasnya lebih cepat rusak.')])
           + latihan(7, 'Mengenali kapasitor', [
               'Temukan sisi bergaris (−) dan kaki panjang (+) di kapasitor elektrolit di kit, dan baca nilai µF dan tegangannya.',
               'Baca kode tiga angka di kapasitor keramik kecil di kit dan hitung nilainya.',
               'Cari kapasitor di papan mikrokontrolermu: kotak-kotak kecil di dekat chip dan regulator itu adalah kapasitor decoupling.']))
    return section('bab7', 'Bab 7', 'Kapasitor dan noise', isi, 'Kenapa hampir setiap IC punya kapasitor kecil di sebelahnya.')


def bab8():
    isi = ('<p><b>Dioda</b> hanya mengalirkan arus satu arah: dari anoda ke katoda (sisi bergaris). Saat mengalir, dioda "memakan" sekitar 0,6-0,7 V '
           '(dioda silikon seperti 1N4007, 1N4148) atau sekitar 0,2-0,4 V (dioda Schottky). Tiga pemakaian yang sering:</p>'
           + ul(['<b>Pelindung polaritas:</b> dioda seri di jalur masuk daya mencegah kerusakan saat baterai atau adaptor dipasang terbalik, dengan biaya turun tegangan sedikit.',
                 '<b>Dioda flyback:</b> dipasang melintang di kumparan relay atau motor (garis ke +) untuk menyerap sentakan tegangan saat kumparan dimatikan. Lihat Bab 9.',
                 '<b>Pemisah sumber:</b> dua sumber disatukan lewat dioda supaya arus tidak mengalir balik dari satu ke yang lain.'])
           + '<p><b>LED</b> adalah dioda yang menyala. Kaki panjang = anoda (+). LED selalu butuh resistor untuk membatasi arus:</p>'
           + rumus('R = (Vsumber − Vf) / I', 'Vf = tegangan yang "dimakan" LED; I = arus LED, biasanya 5-15 mA sudah terang')
           + table(['Warna LED', 'Vf kira-kira'], [['Merah', '1,8-2,2 V'], ['Kuning, hijau biasa', '2,0-2,2 V'], ['Biru, putih, hijau terang', '2,8-3,4 V']])
           + kalk_led()
           + kejadian('LED putih dipasang dari pin 3,3 V dengan resistor 220 Ω dan menyala sangat redup. Vf-nya sekitar 3,1 V, jadi resistor hanya menanggung 0,2 V: '
                      'arusnya sekitar 1 mA. Untuk LED putih atau biru, sumber 3,3 V hampir tidak menyisakan ruang. Pakai resistor lebih kecil (misalnya 47-68 Ω) atau sumber 5 V lewat transistor.')
           + tanya([
               ('LED biru (Vf 3,0 V) di 5 V dengan arus 10 mA. Resistor berapa?', '(5 − 3) / 0,010 = 200 Ω. Pakai nilai standar terdekat di atasnya: 220 Ω.'),
               ('LED dipasang terbalik dengan resistor di 3,3 V. Apa yang terjadi?', 'Tidak menyala, karena dioda menahan arus arah terbalik. Pada tegangan serendah ini LED tidak rusak.'),
               ('Dioda 1N4007 dipasang seri sebagai pelindung polaritas di jalur 5 V. Berapa tegangan yang sampai ke papan?', 'Sekitar 5 − 0,7 = 4,3 V. Untuk papan yang butuh 5 V pas, pakai dioda Schottky atau MOSFET sebagai pelindung.')])
           + latihan(8, 'LED dan dioda', [
               'Rangkai LED dan resistor 220 Ω dari pin GPIO ke GND (kaki panjang ke sisi pin), lalu nyalakan dengan <code>digitalWrite(pin, HIGH)</code>.',
               'Balik LED-nya. Tidak menyala, dan tidak rusak.',
               'Ukur LED dengan mode dioda multimeter: satu arah terbaca angka (LED merah sekitar 1,6-2,0, kadang menyala redup), arah sebaliknya "OL" atau "1".',
               'Hitung resistor untuk LED merah di 5 V dan 10 mA dengan kalkulator (jawaban: 300 Ω, pakai 330 Ω).']))
    return section('bab8', 'Bab 8', 'Dioda dan LED', isi, 'Komponen yang hanya mengalirkan arus satu arah.')


def bab9():
    isi = ('<p>Pin mikrokontroler hanya memberi arus kecil (Bab 10). Untuk menyalakan sesuatu yang lebih besar (relay, buzzer keras, LED strip, motor, pompa), '
           'pin cukup memberi perintah, lalu komponen lain yang menyambungkan beban ke sumber dayanya sendiri.</p>'
           + fig(g.transistor_relay(), 'Arus beban mengalir dari +5 V lewat kumparan dan transistor ke GND. Pin hanya mengendalikan basis.')
           + table(['Komponen', 'Cocok untuk', 'Yang perlu diperhatikan'], [
               ['Transistor NPN (BC547, 2N2222)', 'beban sampai sekitar 100-500 mA: relay, buzzer, LED kecil', 'resistor basis 1 kΩ dari pin; emitor ke GND; beban di antara + dan kolektor'],
               ['MOSFET logic-level (AO3400, IRLZ44N)', 'beban besar: LED strip, motor, pompa', 'pilih yang menyala penuh pada tegangan pinmu (3,3 V); resistor gate sekitar 100-220 Ω, dan 10 kΩ gate ke GND'],
               ['Modul relay', 'memutus-sambung beban lain yang terpisah listrik', 'banyak modul aktif LOW; modul 5 V belum tentu terpicu andal oleh sinyal 3,3 V'],
               ['Driver motor (L298N, TB6612, DRV8833)', 'motor DC dua arah', 'arah dan kecepatan dari pin; daya motor dari sumber sendiri']])
           + warn('Selalu pasang dioda flyback di kumparan', 'Saat arus di kumparan relay, solenoid, atau motor tiba-tiba diputus, kumparan menyentakkan tegangan balik yang tinggi. '
                  'Tanpa dioda (misalnya 1N4007, garis ke +), sentakan itu bisa merusak transistor atau membuat mikrokontroler reset. Modul relay yang baik sudah memasangnya.')
           + kejadian('Modul MOSFET IRF520 yang populer di toko daring sering dipakai dengan ESP32 dan hasilnya beban hanya menyala redup atau MOSFET-nya panas. '
                      'Lembar data IRF520 menyebut tegangan ambang gate 2-4 V, dan hambatan nyala rendahnya baru tercapai dengan gate 10 V. Dengan 3,3 V ia bisa hampir tidak menyala, dan dengan 5 V pun belum menyala penuh. '
                      'Pilih MOSFET berlabel <i>logic-level</i> yang lembar datanya menyebut hambatan nyala pada 2,5-4,5 V.')
           + tanya([
               ('Relay 5 V menarik 80 mA. Bolehkah kumparannya langsung disambung ke pin ESP32?', 'Jangan. Arusnya jauh di atas kemampuan pin, dan sentakan kumparan bisa merusak pin. Pakai transistor dan dioda flyback, atau modul relay.'),
               ('Modul relay menyala saat pin LOW dan mati saat pin HIGH. Kenapa?', 'Modulnya aktif LOW. Di kode, balik logikanya: LOW = nyala, HIGH = mati.'),
               ('Kenapa ada resistor 10 kΩ dari gate MOSFET ke GND?', 'Supaya gate tidak melayang saat pin belum diatur (misalnya waktu boot), sehingga beban tidak menyala sendiri.')])
           + latihan(9, 'Menyalakan beban lewat transistor', [
               'Kalau kit-mu punya modul relay: sambung VCC, GND, dan IN ke papan, lalu nyalakan-matikan IN dari kode. Dengarkan bunyi klik dan catat: aktif HIGH atau LOW?',
               'Cari dioda di modul relaymu (biasanya hitam kecil dengan garis perak, di dekat relay). Itu dioda flyback.',
               'Kalau punya transistor NPN: rangkai LED + resistor 220 Ω dari 5 V ke kolektor, emitor ke GND, basis lewat 1 kΩ ke pin GPIO. LED menyala saat pin HIGH.']))
    return section('bab9', 'Bab 9', 'Transistor, MOSFET, dan relay', isi, 'Cara pin kecil mengendalikan beban besar tanpa terbakar.')


def bab10():
    isi = ('<h3>Logika 3,3 V dan 5 V</h3>'
           '<p>Sinyal digital hanya punya dua keadaan: <b>HIGH</b> dan <b>LOW</b>, selalu terhadap GND. Tegangan HIGH tergantung papannya.</p>'
           + table(['Papan', 'Logika', 'Batas pin (lembar data)', 'ADC'], [
               ['Arduino Uno (ATmega328P)', '5 V', 'maksimum mutlak 40 mA per pin; total VCC 200 mA', '10 bit (0-1023), 0-5 V'],
               ['ESP32', '3,3 V', 'input sampai sekitar 3,6 V; sekitar 40 mA pada kekuatan maksimum', '12 bit (0-4095), sekitar 0-3,1 V'],
               ['ESP8266, Raspberry Pi Pico', '3,3 V', 'input tidak dirancang untuk 5 V', 'ESP8266 satu pin ADC; Pico 12 bit']])
           + '<p>Angka maksimum mutlak adalah batas sebelum rusak, bukan angka kerja. Pakai jauh di bawahnya: satu LED 10 mA tidak masalah, motor dan relay harus lewat transistor (Bab 9). '
           'Sinyal 5 V ke papan 3,3 V harus diturunkan dengan pembagi tegangan atau <b>level shifter</b>. Sebaliknya, sinyal 3,3 V ke modul 5 V sering terbaca HIGH, tapi tidak selalu.</p>'
           '<h3>Input yang melayang dan pull-up</h3>'
           '<p>Pin input yang tidak tersambung ke mana pun membaca HIGH dan LOW secara acak, persis seperti modul tanpa GND bersama di Bab 2. '
           'Untuk tombol, pakai resistor <b>pull-up</b> (ke 3,3 V) atau <b>pull-down</b> (ke GND), supaya pin selalu punya keadaan yang jelas.</p>'
           + fig(g.pullup(), 'Pull-up menahan pin di HIGH; tombol menariknya ke LOW saat ditekan.')
           + ul(['<code>pinMode(pin, INPUT_PULLUP)</code> menyalakan pull-up di dalam chip (di ESP32 sekitar 45 kΩ), jadi resistor luar boleh tidak dipasang.',
                 '<b>Bouncing:</b> kontak tombol memantul beberapa milidetik saat ditekan, sehingga satu tekanan bisa terbaca beberapa kali. Abaikan perubahan dalam 20-50 ms setelah perubahan pertama (debounce di kode).'])
           + '<h3>ADC: membaca tegangan</h3>'
           '<p>ADC mengubah tegangan menjadi angka. Rumusnya kira-kira <code>tegangan = angka / angka_maks × tegangan_acuan</code>. '
           'ADC ESP32 kurang akurat di dekat 0 V dan di dekat batas atasnya, jadi untuk pengukuran yang perlu tepat, kalibrasi dengan multimeter atau pakai ADC luar (misalnya ADS1115). '
           'Tegangan di atas batasnya harus diturunkan dulu dengan pembagi (Bab 6).</p>'
           '<h3>Pin khusus</h3>'
           + ul(['<b>Pin boot (strapping)</b> dibaca chip saat menyala untuk menentukan cara boot. Di ESP32: GPIO 0, 2, 5, 12, 15. GPIO 12 yang tertarik HIGH saat boot membuat chip memakai tegangan flash yang salah, dan papan gagal boot. Hindari memasang sesuatu yang menarik pin-pin ini saat menyala.',
                 '<b>Pin input saja:</b> di ESP32, GPIO 34-39 hanya bisa input dan tidak punya pull-up internal.',
                 '<b>Pin yang dipakai flash:</b> di banyak papan ESP32, GPIO 6-11 tersambung ke chip flash dan tidak boleh dipakai.',
                 '<b>Pin UART untuk upload:</b> TX0/RX0 dipakai saat upload dan Serial Monitor. Memasang modul di sana bisa membuat upload gagal.'])
           + tanya([
               ('Tombol di pin input tanpa resistor dan tanpa INPUT_PULLUP. Apa yang terjadi saat tombol tidak ditekan?', 'Pin melayang dan membaca HIGH/LOW acak.'),
               ('Sensor 5 V mengeluarkan sinyal digital 5 V. Bolehkah langsung ke pin ESP32?', 'Jangan. Turunkan dulu dengan pembagi tegangan (misalnya 1 kΩ dan 2 kΩ) atau level shifter.'),
               ('ADC ESP32 12 bit membaca 2048. Kira-kira berapa tegangannya kalau batas atasnya 3,1 V?', 'Sekitar 2048 / 4095 × 3,1 ≈ 1,55 V. Kira-kira saja, karena ADC ESP32 tidak sepenuhnya linear.')])
           + latihan(10, 'Tombol dan pembacaan pin', [
               'Pasang tombol dari pin GPIO ke GND, pakai <code>INPUT_PULLUP</code>, dan cetak <code>digitalRead</code> ke Serial Monitor. Lepas: 1, tekan: 0.',
               'Hapus <code>INPUT_PULLUP</code> (pakai <code>INPUT</code> saja) dan lepas tombolnya. Dekatkan jarimu ke kabel: angkanya ikut berubah acak.',
               'Hitung berapa kali satu tekanan tombol terbaca tanpa debounce, lalu tambahkan jeda 30 ms di kode dan bandingkan.',
               'Baca kaki tengah potensiometer atau pembagi LDR dengan <code>analogRead</code>, lalu bandingkan dengan tegangan yang diukur multimeter.']))
    return section('bab10', 'Bab 10', 'Pin mikrokontroler', isi, 'Apa yang boleh dan tidak boleh disambung ke pin, dan kenapa pin bisa membaca angka acak.')


def bab11():
    isi = ('<h3>PWM</h3>'
           '<p>PWM menyalakan dan mematikan sinyal sangat cepat. <b>Duty cycle</b> adalah persen waktu menyala. LED terlihat lebih redup karena mata merata-ratakan kedipannya; '
           'motor berputar lebih lambat karena ia merasakan rata-ratanya. Pin mikrokontroler hanya memberi sinyal PWM-nya; beban besar dinyalakan lewat MOSFET atau driver motor.</p>'
           + fig(g.pwm(), 'Duty 50% dari 5 V terasa sekitar 2,5 V bagi beban.')
           + '<h3>Servo</h3>'
           '<p>Servo tidak membaca rata-rata, tapi <b>lebar pulsa</b> yang diulang 50 kali per detik. Di dalam servo ada potensiometer yang membaca posisi lengan, '
           'dan servo terus mengoreksi sampai posisinya sama dengan perintah. Kalau diperintah ke sudut yang tidak bisa dicapai (lengan menabrak rangka), '
           'servo terus mendorong: bergetar, berdengung, dan menarik arus besar.</p>'
           + fig(g.servo_pulsa(), 'Makin lebar pulsanya, makin besar sudutnya.')
           + ul(['<b>Daya servo dari sumber terpisah</b> (5-6 V), bukan pin 3V3 papan. Servo kecil seperti SG90 bisa menarik ratusan mA saat bergerak atau tertahan.',
                 '<b>GND servo dan GND papan disatukan</b> (Bab 2), dan pin papan hanya ke kabel sinyal (oranye atau kuning).'])
           + '<h3>Motor DC dan stepper</h3>'
           + ul(['<b>Motor DC</b> berputar dua arah dengan <b>H-bridge</b> (driver motor): empat saklar elektronik yang membalik arah arus. Arah dari dua pin, kecepatan dari PWM. Membalik dua kabel motor membalik arah putarnya.',
                 '<b>Motor DC menarik arus paling besar saat mulai berputar dan saat tertahan</b>, beberapa kali arus normalnya. Pilih driver dan sumber yang sanggup arus itu.',
                 '<b>Stepper</b> bergerak per langkah dan butuh driver khusus (A4988, DRV8825, ULN2003 untuk 28BYJ-48).'])
           + kejadian('Servo diperintah ke 180° lalu terus bergerak kecil di ujung, berdengung, dan lampu modul dayanya berkedip. Servo itu tidak bisa mencapai 180° karena rangkanya, '
                      'dan terus mencoba. Solusinya batas sudut yang bisa dicapai tanpa getar, bukan menambah daya.')
           + tanya([
               ('Duty 25% dari 5 V. Kira-kira berapa yang dirasakan motor?', 'Sekitar 1,25 V rata-rata. Motor kecil mungkin hanya berdengung tanpa berputar.'),
               ('Servo diam dengan tenang di 90° tapi bergetar terus di 180°. Apa artinya?', 'Di 180° lengan menabrak batas mekanis, dan servo terus mencoba mencapai sudut itu. Kecilkan batas sudut di kode.'),
               ('Satu roda robot berputar terbalik dari yang diharapkan. Dua cara memperbaikinya?', 'Tukar dua kabel motor itu di driver, atau balik arahnya di kode.')])
           + latihan(11, 'PWM dan servo', [
               'Redupkan LED dengan PWM (<code>analogWrite</code> di Arduino, <code>ledcWrite</code> atau <code>analogWrite</code> di ESP32 core 3.x) dari 0 sampai nilai maksimum.',
               'Ukur pin PWM itu dengan multimeter V⎓ saat duty 50%. Angkanya sekitar setengah tegangan pin, karena multimeter biasa menampilkan rata-rata.',
               'Kalau punya servo SG90: beri daya 5 V dari sumber terpisah dengan GND bersama, lalu naikkan sudutnya pelan-pelan. Catat sudut terbesar yang tidak bergetar.']))
    return section('bab11', 'Bab 11', 'PWM, servo, dan motor', isi, 'Cara mikrokontroler mengatur kecerahan, kecepatan, dan posisi.')


def bab12():
    isi = (table(['Antarmuka', 'Kabel', 'Yang perlu diingat'], [
               ['UART (Serial)', 'TX, RX, GND', 'TX satu perangkat ke RX perangkat lain (disilang); kecepatan (baud rate) dua sisi harus sama'],
               ['I2C', 'SDA, SCL, GND', 'banyak perangkat di dua kabel, dibedakan alamat; butuh pull-up; kabel pendek (puluhan cm)'],
               ['SPI', 'SCK, MOSI, MISO, CS', 'cepat; tiga kabel dipakai bersama, tiap perangkat punya pin CS sendiri'],
               ['1-Wire', 'DATA, GND (dan VCC)', 'DS18B20 dan sejenisnya; butuh pull-up sekitar 4,7 kΩ di DATA'],
               ['Sinyal satu kabel khusus', 'DATA', 'DHT11/DHT22 memakai protokolnya sendiri; modul biasanya sudah membawa pull-up'],
               ['Analog', 'satu pin ADC', 'tegangan langsung mewakili nilai; peka noise dan kabel panjang']])
           + fig(g.i2c_bus(), 'Semua perangkat I2C berbagi SDA dan SCL. Dua perangkat dengan alamat sama tidak bisa di bus yang sama tanpa diubah alamatnya.')
           + '<h3>Membaca spesifikasi modul sebelum menyambung</h3>'
           + ul(['<b>Tegangan kerja (VCC):</b> 3,3 V saja, 5 V saja, atau 3,3-5 V? Modul 5 V yang diberi 3,3 V sering bekerja setengah-setengah; modul 3,3 V yang diberi 5 V bisa rusak.',
                 '<b>Level logika sinyal:</b> modul yang diberi 5 V biasanya juga mengeluarkan sinyal 5 V. Untuk papan 3,3 V, cek apakah modul punya pengubah level.',
                 '<b>Arus:</b> berapa yang ditarik saat aktif dan saat diam. Ini menentukan sumber daya dan umur baterai.',
                 '<b>Antarmuka dan alamat:</b> I2C berapa alamatnya, SPI butuh pin CS mana, UART berapa baud rate bawaannya.',
                 '<b>Nama pin:</b> baca tulisan di papan, jangan menebak dari posisi. Modul yang mirip dari penjual berbeda bisa punya urutan pin berbeda.'])
           + kejadian('Layar OLED I2C tidak terdeteksi oleh program pemindai I2C. Penyebab yang paling sering, berurutan: SDA dan SCL tertukar, VCC atau GND belum tersambung, '
                      'pin I2C di kode tidak sama dengan pin yang dipakai, dan alamat di kode salah (0x3C dan 0x3D sama-sama umum). '
                      'Di UART, karakter aneh di Serial Monitor hampir selalu berarti baud rate berbeda.')
           + tanya([
               ('Dua papan dihubungkan lewat UART tapi tidak ada data yang diterima. Apa dua hal pertama yang dicek?', 'TX ke RX disilang (bukan TX ke TX), dan GND kedua papan tersambung. Lalu baud rate yang sama.'),
               ('Kamu memasang dua sensor I2C yang sama persis. Apa masalahnya?', 'Alamatnya sama. Banyak sensor punya pin atau jumper untuk memilih alamat kedua; kalau tidak ada, perlu multiplexer I2C atau bus I2C kedua.'),
               ('Sensor suhu DS18B20 selalu membaca −127 atau 85. Komponen apa yang sering terlupa?', 'Resistor pull-up 4,7 kΩ dari DATA ke VCC.')])
           + latihan(12, 'Komunikasi dan modul', [
               'Pilih satu modul di kit-mu. Dari tulisan di papan dan halaman penjualnya, catat: tegangan kerja, level logika, arus, antarmuka, dan alamat (kalau I2C).',
               'Kalau punya modul I2C, jalankan program pemindai I2C dan catat alamat yang ditemukan.',
               'Tukar SDA dan SCL dengan sengaja, jalankan pemindai lagi, lalu kembalikan. Ingat gejala ini.']))
    return section('bab12', 'Bab 12', 'Komunikasi dan membaca spesifikasi modul', isi, 'Cara papan berbicara dengan sensor dan modul, dan apa yang dicek sebelum menyambung.')


def bab13():
    isi = (fig(g.multimeter(), 'Kenop memilih apa yang diukur, colokan menentukan jalur probe merah.')
           + table(['Mode', 'Untuk', 'Syarat'], [
               ['V⎓ (DC volt)', 'baterai, regulator, pin papan', 'rangkaian menyala, probe di dua titik'],
               ['V~ (AC volt)', 'listrik rumah', 'tidak dipakai di proyek tegangan rendah'],
               ['Ω (ohm)', 'resistor, kabel, sensor resistif', 'daya MATI, komponen sebaiknya dilepas'],
               ['Bip (continuity)', 'apakah dua titik tersambung', 'daya MATI'],
               ['Dioda', 'LED, dioda, arah kutub', 'daya MATI'],
               ['A / mA', 'arus', 'rangkaian dibuka, multimeter disisipkan seri; probe merah pindah colokan']])
           + ul(['<b>COM</b> selalu untuk probe hitam. <b>VΩmA</b> untuk hampir semua pengukuran. <b>10A</b> hanya untuk mengukur arus besar secara seri.',
                 '<b>Angka minus</b> berarti probe terbalik. Besarnya tetap benar.',
                 '<b>"OL" atau "1" di kiri layar</b> berarti di luar rentang atau tidak tersambung.',
                 '<b>Multimeter manual</b> (ada angka 2, 20, 200 di kenop): pilih rentang di atas angka yang kamu harapkan, misalnya 20 V untuk baterai 7,4 V.',
                 'Mode Ω pada komponen yang masih terpasang ikut mengukur komponen lain yang tersambung, jadi hasilnya bisa menyesatkan.',
                 '<b>Setelah mengukur arus, kembalikan probe merah ke colokan VΩ.</b> Lupa langkah ini adalah cara paling umum merusak sekering multimeter.'])
           + warn('Jangan ukur tegangan di mode arus', 'Di mode A atau colokan 10A, multimeter hampir tanpa hambatan. Menempelkannya ke dua kutub baterai sama dengan korsleting: sekering multimeter putus, atau lebih parah.')
           + '<h3>Alat ukur berikutnya</h3>'
           + ul(['<b>Logic analyzer</b> murah (versi 8 kanal mudah dicari) menampilkan sinyal digital I2C, SPI, dan UART di laptop: sangat membantu saat modul "tidak mau bicara".',
                 '<b>Osiloskop</b> menampilkan bentuk tegangan terhadap waktu: noise, PWM, dan tegangan yang turun sesaat (brownout) yang tidak tertangkap multimeter.',
                 '<b>Pengukur USB (USB meter)</b> menunjukkan tegangan dan arus dari port USB: cara mudah mengukur konsumsi papan.'])
           + kejadian('Saat mengukur baterai, layar menunjukkan 0 terus walaupun saklar ON dan OFF. Ternyata kenop menunjuk ke 10 A, bukan V⎓. '
                      'Setelah diputar ke V⎓, saklar ON terbaca 7,8 V dan OFF 0 V.')
           + tanya([
               ('Mengapa multimeter tidak boleh ditempel ke baterai saat probe merah di colokan 10A?', 'Mode arus hampir tanpa hambatan, jadi terjadi korsleting lewat multimeter.'),
               ('Layar menunjukkan −3,3 V. Apa artinya?', 'Probe merah dan hitam terbalik. Tegangannya tetap 3,3 V.'),
               ('Kenapa tes bip harus saat daya mati?', 'Tes bip mengalirkan arus kecil dari baterai multimeter sendiri. Tegangan dari rangkaian bisa membuat hasilnya salah atau merusak multimeter.')])
           + latihan(13, 'Kenali multimetermu', [
               'Lihat kenopmu: auto-range (hanya simbol V, Ω) atau manual (ada angka 2, 20, 200)?',
               'Temukan colokan mA dan 10A, dan baca nilai sekering yang tertulis di dekatnya (kalau ada).',
               'Ukur tiga hal dengan rentang yang benar: baterai, pin 3V3 papan, pin 5V papan. Kembalikan kenop ke OFF setelah selesai.']))
    return section('bab13', 'Bab 13', 'Multimeter dan alat ukur', isi, 'Alat paling penting untuk merakit. Kebanyakan kesalahan baca berasal dari kenop dan colokan.')


def bab14():
    isi = (fig(g.breadboard(), 'Lima lubang dalam satu kolom tersambung; celah tengah memisahkan dua sisi; rel daya tersambung sepanjang baris.')
           + ul(['<b>Celah tengah</b> dibuat untuk IC dan modul berkaki dua baris, supaya kaki kiri dan kanannya tidak tersambung.',
                 '<b>Rel daya di papan panjang</b> kadang terputus di tengah (lihat garis merah-biru yang putus). Ukur dengan tes bip kalau ragu.',
                 '<b>Papan ESP32 DevKit</b> sering terlalu lebar untuk satu breadboard: menutup semua lubang di satu sisi. Pakai dua breadboard berdampingan atau papan ekspansi.',
                 '<b>Breadboard tidak cocok untuk arus besar</b> (di atas sekitar 1 A) dan kontaknya bisa aus. Untuk jalur motor, pakai terminal sekrup atau konektor.'])
           + '<h3>Konektor</h3>'
           + table(['Konektor', 'Dipakai untuk', 'Catatan'], [
               ['Jumper Dupont (M-M, M-F, F-F)', 'sinyal di breadboard dan pin header', 'mudah lepas; kawatnya tipis; bisa putus di dalam'],
               ['JST (PH, XH)', 'baterai LiPo, sensor, modul', 'pasti tidak terbalik; cek urutan merah-hitam, beda antar penjual'],
               ['Terminal sekrup', 'jalur daya, motor', 'menjepit tembaga, bukan isolasi; kencangkan ulang setelah beberapa hari'],
               ['WAGO / konektor tuas', 'menyatukan beberapa kabel daya', 'semua lubang satu konektor tersambung di dalam']])
           + '<h3>Solder</h3>'
           + fig(g.solder(), 'Solderan yang baik membungkus kaki dan pad; solderan dingin hanya menempel.')
           + ul(['<b>Panaskan benda yang disolder, bukan timahnya.</b> Tempelkan ujung solder ke kaki dan pad bersama 1-2 detik, sentuhkan timah ke sambungan sampai mengalir, angkat, lalu jangan digerakkan sampai membeku.',
                 '<b>Kabel:</b> kupas, puntir serabutnya, lapisi tipis dengan timah dulu (tinning), baru disatukan.',
                 '<b>Heat shrink dimasukkan sebelum menyolder</b>, karena tidak bisa dimasukkan setelah kedua ujung tersambung.',
                 '<b>Setiap sambungan ditarik pelan setelah selesai.</b> Sambungan yang lepas saat ditarik pelan pasti lepas saat dipakai.'])
           + info('Kalau disentuh, rangkaian berubah', 'Ada dua penyebab yang sering tertukar. <b>Sambungan longgar:</b> sentuhan menekan kontak sampai tersambung atau terputus. '
                  '<b>Titik yang melayang:</b> GND atau input tidak tersambung, dan tanganmu menyalurkan noise. Bedakan dengan tes bip (Bab 2) dan dengan menekan memakai benda tidak menghantar seperti tusuk gigi.')
           + tanya([
               ('LED di breadboard tidak menyala. Kaki LED dan resistor ada di kolom 10, baris a dan f. Apa salahnya?', 'Baris a ada di sisi atas celah, f di sisi bawah: tidak tersambung. Pindahkan ke sisi yang sama (a-e).'),
               ('Solderan berbentuk bola buram dan kabelnya sedikit bergoyang. Apa yang perlu dilakukan?', 'Itu solderan dingin. Panaskan ulang sampai timah mengalir membungkus kaki dan pad, tambahkan sedikit timah baru kalau perlu.')])
           + latihan(14, 'Breadboard dan sambungan', [
               'Dengan tes bip, buktikan sendiri: lima lubang satu kolom tersambung, dan kolom yang sama di seberang celah tidak.',
               'Cek apakah rel daya breadboard-mu tersambung sepanjang papan atau putus di tengah.',
               'Tarik pelan semua jumper di rangkaianmu yang sekarang. Ganti yang longgar.']))
    return section('bab14', 'Bab 14', 'Breadboard, konektor, dan solder', isi, 'Banyak masalah "elektronik" ternyata sambungan yang kurang rapat.')


def bab15():
    isi = (rumus('Amati → ukur → pisahkan → tukar → ubah satu hal', 'lalu catat hasilnya')
           + ul(['<b>Tulis gejalanya persis.</b> "Sensor membaca 0 terus sejak dipindah ke baterai" lebih berguna daripada "sensor rusak".',
                 '<b>Ukur, jangan menebak.</b> Ukur tegangan di titik pemakaian (kaki VCC modul), bukan hanya di sumber.',
                 '<b>Pisahkan.</b> Lepas semua modul, nyalakan, lalu pasang satu per satu sampai masalahnya muncul.',
                 '<b>Tukar dengan yang diketahui baik.</b> Pindahkan modul ke pin lain atau ke papan lain; ganti kabel jumper.',
                 '<b>Ubah satu hal setiap kali.</b> Mengganti kapasitor, kode, dan kabel sekaligus membuatmu tidak tahu mana penyebabnya.'])
           + '<p>Urutan cek yang paling sering benar: <b>daya → GND bersama → polaritas → sambungan → pin dan kode → mekanis</b>.</p>'
           + table(['Gejala', 'Penyebab yang sering', 'Cek'], [
               ['Papan tidak muncul di port USB', 'kabel USB hanya untuk mengisi daya, driver CH340/CP210x belum terpasang', 'coba kabel data lain; pasang driver'],
               ['Upload ESP32 gagal "Failed to connect"', 'chip tidak masuk mode upload, atau ada modul di pin boot/TX0/RX0', 'tahan tombol BOOT saat upload dimulai; lepas modul di pin itu'],
               ['Restart acak, "Brownout detector was triggered"', 'daya turun sesaat (WiFi, motor, servo)', 'kabel USB lebih baik, sumber lebih kuat, kapasitor, beban besar di sumber terpisah'],
               ['Bootloop dengan pesan error flash saat menyala', 'daya tidak stabil saat boot, atau pin boot tertarik', 'lepas semua modul, cek GPIO 12, hapus flash lalu upload ulang'],
               ['Sensor I2C tidak ditemukan', 'SDA/SCL tertukar, alamat salah, VCC salah, pin di kode beda', 'jalankan pemindai I2C, cek kabel satu per satu'],
               ['Angka sensor acak atau berubah saat disentuh', 'input melayang atau GND bersama hilang', 'tes bip GND; pastikan ada pull-up'],
               ['Relay tidak klik', 'modul aktif LOW, sinyal 3,3 V terlalu lemah untuk modul 5 V, daya kumparan kurang', 'balik logika; ukur VCC modul; coba sinyal dari transistor'],
               ['LED tidak menyala', 'terbalik, resistor terlalu besar, salah kolom breadboard', 'cek kaki panjang; ukur tegangan di LED'],
               ['Karakter aneh di Serial Monitor', 'baud rate berbeda', 'samakan angka di <code>Serial.begin</code> dan Serial Monitor'],
               ['Kompilasi gagal: fungsi tidak dikenal (misalnya <code>ledcAttach</code>)', 'versi core papan berbeda dengan contoh kode', 'perbarui core papan di Boards Manager, atau pakai contoh untuk versimu'],
               ['Modul atau regulator panas', 'tegangan terlalu tinggi, korsleting, regulator linear dengan selisih besar', 'matikan daya, ukur tegangan masuk, cek kabel']])
           + tanya([
               ('Kamu mengganti kapasitor, kode, dan kabel sekaligus, lalu masalahnya hilang. Apa kekurangannya?', 'Kamu tidak tahu mana penyebabnya, dan masalah yang sama bisa kembali tanpa kamu tahu harus mencari di mana.'),
               ('Proyek bekerja saat USB tersambung, tapi tidak saat memakai baterai. Apa yang pertama diukur?', 'Tegangan di pin daya papan dan modul saat memakai baterai, terutama saat WiFi atau beban aktif. Lalu GND bersama antara baterai dan modul.')])
           + latihan(15, 'Buat catatanmu sendiri', [
               'Pilih satu masalah yang pernah kamu alami di proyek mana pun. Tulis: gejala, apa yang diukur, penyebab, dan perbaikannya.',
               'Siapkan buku atau catatan di HP khusus untuk proyek elektronik, dan biasakan menulis sebelum mengubah sesuatu.']))
    return section('bab15', 'Bab 15', 'Cara mencari kesalahan', isi, 'Cara kerja yang membuat masalah ketemu dalam menit, bukan jam.')


def bab16():
    isi = (ul(['<b>Listrik rumah 220 V bisa membunuh.</b> Jangan menyambung relay, modul, atau kabel ke listrik rumah sebelum benar-benar paham isolasi, kotak tertutup, sekering, dan jarak aman antar jalur. '
               'Untuk mengendalikan lampu atau alat rumah, smart plug atau modul bersertifikat jauh lebih aman.',
               '<b>Baterai Li-ion dan LiPo</b>: korsleting bisa memanaskan kabel dalam hitungan detik dan membakar sel. Pasang sekering dekat baterai untuk proyek berarus besar, dan selalu pakai pelindung (BMS atau modul dengan pelindung).',
               '<b>Isi daya sambil diawasi</b>, di atas permukaan yang tidak mudah terbakar, dengan pengisi yang sesuai jenis baterainya. Jangan mengisi sel yang menggembung, penyok, bocor, atau sudah lama sangat kosong.',
               '<b>Simpan baterai sekitar setengah penuh</b> untuk penyimpanan lama, dan lepas dari rangkaian.',
               '<b>Cabut daya</b> sebelum menyolder atau memindah kabel.',
               '<b>Regulator, resistor daya, dan motor bisa panas.</b> Sentuh dengan punggung jari dulu.',
               '<b>Kapasitor elektrolit besar</b> masih menyimpan muatan sebentar setelah daya mati. Di tegangan rendah tidak berbahaya bagi tubuh, tapi bisa memercik kalau kakinya dihubung singkat.',
               '<b>Solder dan timah:</b> kerjakan di ruang berventilasi, dan cuci tangan setelahnya.'])
           + latihan(16, 'Keselamatan', [
               'Siapkan tempat mengisi baterai yang tidak mudah terbakar dan bisa kamu awasi.',
               'Periksa baterai Li-ion yang kamu punya: tidak menggembung, tidak penyok, pembungkusnya tidak sobek.',
               'Pastikan setiap proyek berbaterai Li-ion punya pelindung, dan proyek berarus besar punya sekering dekat baterai.']))
    return section('bab16', 'Bab 16', 'Keselamatan', isi, 'Tegangan rendah itu aman kalau diperlakukan dengan benar. Listrik rumah tidak.')


def istilah():
    rows = [
        ['GND', 'titik acuan 0 V yang dipakai bersama'], ['VCC, V+, 3V3, 5V', 'kutub positif daya, dengan tegangan tertentu'],
        ['VIN', 'pin masukan daya papan, biasanya ke regulator di papan'], ['Polaritas', 'arah kutub + dan −'],
        ['Korsleting', 'kutub + bertemu − tanpa beban; arus sangat besar'],
        ['Floating (melayang)', 'titik yang tidak tersambung ke tegangan tertentu, sehingga nilainya acak'],
        ['Pull-up, pull-down', 'resistor yang menarik input ke HIGH atau LOW saat tidak ada sinyal'],
        ['Debounce', 'mengabaikan pantulan kontak tombol beberapa milidetik'],
        ['PWM, duty cycle', 'sinyal menyala-mati cepat; persen waktu menyala'], ['ADC', 'pengubah tegangan menjadi angka'],
        ['Brownout', 'tegangan turun sesaat sampai chip reset'], ['Regulator linear, LDO', 'penurun tegangan yang membuang kelebihannya sebagai panas'],
        ['Buck, boost', 'konverter switching penurun dan penaik tegangan'], ['Dropout', 'selisih minimum masukan di atas keluaran regulator'],
        ['Arus diam (quiescent)', 'arus yang dipakai regulator atau modul untuk dirinya sendiri'], ['Deep sleep', 'mode tidur mikrokontroler dengan arus sangat kecil'],
        ['Decoupling', 'kapasitor kecil di dekat kaki daya IC untuk meredam noise'], ['Vf', 'tegangan yang dimakan LED atau dioda saat menyala'],
        ['Dioda flyback', 'dioda di kumparan untuk menyerap sentakan saat dimatikan'], ['Logic-level MOSFET', 'MOSFET yang menyala penuh dengan tegangan pin mikrokontroler'],
        ['Aktif LOW', 'menyala saat sinyalnya LOW'], ['H-bridge', 'susunan empat saklar yang bisa membalik arah motor'],
        ['Level shifter', 'pengubah level sinyal antara 3,3 V dan 5 V'], ['Strapping pin', 'pin yang dibaca chip saat boot'],
        ['UART, I2C, SPI, 1-Wire', 'cara mikrokontroler berkomunikasi dengan perangkat lain'], ['Baud rate', 'kecepatan UART; dua sisi harus sama'],
        ['Continuity (bip)', 'tes apakah dua titik tersambung'], ['AWG', 'ukuran kawat; angka kecil = kawat besar'],
        ['mA, µA, µF, nF, kΩ', 'mili (seperseribu), mikro (sepersejuta), nano (sepermiliar), kilo (seribu)']]
    return section('istilah', 'Rujukan', 'Daftar istilah', table(['Istilah', 'Artinya'], rows))


def sumber():
    isi = ul([
        'Karakteristik pin ESP32 (input sampai VDD + 0,3 V, arus pin 40 mA pada kekuatan maksimum, pull-up sekitar 45 kΩ): ringkasan lembar data di '
        '<a href="https://deneyapkart.org/en/docs/elektriksel-ozellikler-esp32-2">Deneyap Kart, Electrical Specifications (ESP32)</a>.',
        'Batas arus pin ATmega328P (Arduino Uno): <a href="https://www.arduino.cc/playground/Main/ArduinoPinCurrentLimitations/">Arduino Playground, Arduino Pin Current Limitations</a>.',
        'Arus deep sleep papan ESP32-DevKitC V4 terhadap angka lembar data: <a href="https://lucidar.me/en/esp32/power-consumption-of-esp32-devkitc-v4/">Lucidar, ESP32-DevKitC V4 real current consumption in deep sleep mode</a>.',
        'Dropout dan arus diam AMS1117: <a href="https://www.lcsc.com/blog/ams1117-voltage-regulator-complete-guide/">LCSC, AMS1117 Voltage Regulator guide</a>.',
        'IRF520 dan tegangan gate: <a href="https://forum.arduino.cc/t/irf520-mosfet-not-a-good-choice-for-arduino-use/702391">Arduino Forum, IRF520 MOSFET not a good choice for Arduino use</a>.',
        'Penyimpanan dan batas tegangan Li-ion: <a href="https://forum.digikey.com/t/li-ion-lipo-battery-storage-and-permanent-capacity-loss/19946">DigiKey TechForum, Li-ion / LiPo Battery Storage and Permanent Capacity Loss</a>.',
        'Angka Vf LED, tegangan dioda, lebar pulsa servo, dan arus relay adalah nilai umum yang berbeda antar produk. Untuk komponenmu, lembar data atau keterangan penjual lebih tepat.'])
    return section('sumber', 'Rujukan', 'Sumber', isi)
