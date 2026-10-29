# Isi halaman Elektronika Dasar. Tiap bab: penjelasan, gambar, kejadian nyata di robot, cek pemahaman, latihan.
import gambar as g
from buat_alat import (section, table, warn, ok, info, ul, fig, latihan, tanya, kejadian, rumus, kalk_ohm,
                       kalk_led, kalk_pembagi, kalk_warna, ROBOT)

BAB = [
    ('bab1', 'Tegangan, arus, hambatan'),
    ('bab2', 'Rangkaian tertutup dan GND'),
    ('bab3', 'Seri dan paralel'),
    ('bab4', 'Daya, kabel, sekering'),
    ('bab5', 'Baterai dan konverter'),
    ('bab6', 'Resistor dan kapasitor'),
    ('bab7', 'Dioda, LED, driver motor'),
    ('bab8', 'Sinyal: digital, PWM, servo'),
    ('bab9', 'Multimeter'),
    ('bab10', 'Solder dan sambungan'),
    ('bab11', 'Cara mencari kesalahan'),
    ('bab12', 'Keselamatan baterai'),
]


def pendahuluan():
    isi = ('<p>Halaman ini berisi konsep yang benar-benar kamu pakai saat merakit robot AI dan stasiun cuaca, bukan teori lengkap. '
           'Tiap bab diakhiri contoh dari kejadian nyata saat merakit robot, pertanyaan untuk menguji diri, dan latihan kecil dengan multimeter dan komponen kit.</p>'
           + ul(['<b>Baca berurutan.</b> Bab 1 dan 2 adalah dasar untuk semua bab lain. Bab 2 (GND) adalah penyebab paling sering rangkaian yang "aneh".',
                 '<b>Jawab dulu, baru buka jawabannya.</b> Mengingat sendiri sebelum melihat jawaban membuat konsep lebih menempel daripada membaca ulang.',
                 '<b>Centang latihan yang sudah kamu kerjakan.</b> Centangnya tersimpan di browser ini, dan label status di halaman utama ikut berubah.'])
           + info('Yang kamu butuhkan', 'Multimeter digital, satu sel 18650 (atau baterai robot), beberapa resistor dan LED dari kit, breadboard, kabel jumper. '
                  'Semua latihan memakai tegangan rendah (paling tinggi 8,4 V dari baterai robot). Jangan mencoba apa pun di listrik rumah 220 V.'))
    return section('mulai', 'Mulai di sini', 'Cara memakai halaman ini', isi)


def bab1():
    isi = ('<p><b>Tegangan</b> (volt, V) adalah selisih "dorongan" listrik di antara <b>dua titik</b>. Satu titik sendirian tidak punya tegangan. '
           'Saat panduan menulis "WAGO 6V", artinya titik itu 6 V lebih tinggi daripada WAGO −6 (GND).</p>'
           '<p><b>Arus</b> (ampere, A) adalah banyaknya muatan listrik yang mengalir lewat kabel tiap detik. 1 mA = 0,001 A. '
           'Arus hanya mengalir kalau ada jalur tertutup: keluar dari sumber, lewat beban, lalu kembali ke sumber.</p>'
           '<p><b>Hambatan</b> (ohm, Ω) adalah seberapa kuat sebuah benda menahan arus. 1 kΩ = 1.000 Ω.</p>'
           + table(['Besaran', 'Satuan', 'Diukur dengan', 'Contoh di robotmu'], [
               ['Tegangan', 'volt (V)', 'multimeter mode V⎓, di antara dua titik', 'baterai 7,4 V, WAGO 6 V, pin 3,3 V'],
               ['Arus', 'ampere (A), mA', 'multimeter mode A, disisipkan di jalur', 'dua servo bergerak sekitar 1 A; pin ESP32 hanya puluhan mA'],
               ['Hambatan', 'ohm (Ω), kΩ', 'multimeter mode Ω, daya mati', 'resistor 1 kΩ dan 2 kΩ di pembagi echo HC-SR04']])
           + '<p><b>Bayangkan seperti air di pipa:</b> tegangan seperti tekanan air, arus seperti banyaknya air yang mengalir, hambatan seperti pipa yang menyempit. '
           'Pakai gambaran ini hanya untuk membayangkan. Listrik tidak "habis" di jalan: arus yang keluar dari kutub + sama besar dengan yang kembali ke kutub −.</p>'
           + '<h3>Hukum Ohm</h3>'
           + rumus('V = I × R', 'atau I = V / R, atau R = V / I')
           + '<p>Contoh: LED merah dan resistor 220 Ω di baterai 6 V. LED "memakan" sekitar 2 V, jadi resistor menanggung 6 − 2 = 4 V. '
           'Arusnya I = 4 / 220 = 0,018 A = 18 mA. Tanpa resistor, hambatannya hampir nol dan arus melonjak sampai LED atau pin rusak.</p>'
           + fig(g.rangkaian(), 'Tegangan diukur di dua ujung resistor. Arusnya sama di setiap titik rangkaian yang tertutup.')
           + kalk_ohm()
           + kejadian('Saat kamu mengukur baterai dan saklar ON, layar menunjukkan 7,8 V. Itu tegangan di antara probe merah (kutub +) dan probe hitam (kutub −) dari dua sel seri, sekitar 3,9 V per sel: baterai cukup terisi.')
           + tanya([
               ('Multimeter menunjukkan 6,0 V saat probe merah di WAGO 6V. Di antara titik mana sebenarnya angka itu?',
                'Di antara probe merah (WAGO 6V) dan probe hitam (WAGO −6 atau GND). Tegangan selalu antara dua titik; pindah probe hitam ke tempat lain, angkanya bisa berubah.'),
               ('Resistor 1 kΩ dipasang langsung di 5 V. Berapa arusnya?', 'I = V / R = 5 / 1.000 = 0,005 A = 5 mA.'),
               ('Kenapa LED tidak boleh langsung ke 3,3 V tanpa resistor?', 'Hambatan LED yang sedang menyala sangat kecil, jadi arusnya tidak dibatasi apa pun dan melewati batas LED dan pin ESP32.')])
           + latihan(1, 'Mengukur tegangan dan hambatan', [
               'Putar kenop multimeter ke <b>V⎓</b> (rentang 20 V kalau multimetermu manual). Probe hitam di <b>COM</b>, merah di <b>VΩmA</b>.',
               'Ukur satu sel 18650: merah ke +, hitam ke −. Normalnya 3,0-4,2 V.',
               'Tukar probe merah dan hitam. Angkanya jadi minus dengan besar yang sama. Itu aman di DC, hanya berarti probe terbalik.',
               'Ukur resistor 1 kΩ (coklat-hitam-hitam-coklat-coklat) di mode <b>Ω</b>, tidak terpasang di rangkaian. Terbaca sekitar 0,99-1,01 kΩ.',
               'Hitung arus kalau resistor itu dipasang di 3,3 V, lalu cocokkan dengan kalkulator di atas (jawaban: 3,3 mA).']))
    return section('bab1', 'Bab 1', 'Tegangan, arus, dan hambatan', isi, 'Tiga besaran ini ada di balik setiap angka di panduan: 6 V, 1 A, 1 kΩ.')


def bab2():
    isi = ('<p>Arus berjalan berkeliling: keluar dari kutub +, lewat beban, lalu kembali ke kutub −. Kalau jalurnya putus di satu titik saja '
           '(kabel lepas, solderan retak, saklar OFF, sekering putus), arus berhenti di seluruh jalur itu. Saklar dan sekering bekerja dengan cara ini.</p>'
           '<h3>Apa itu GND</h3>'
           '<p><b>GND</b> (ground) adalah titik yang disepakati sebagai 0 V. Semua tegangan lain diukur dari sana: "pin 3,3 V" berarti 3,3 V terhadap GND. '
           'Di robot, GND adalah kutub − yang dipakai bersama. Ini bukan tanah dan bukan kabel bumi listrik rumah, hanya nama untuk titik acuan.</p>'
           '<h3>Sinyal juga butuh jalur pulang</h3>'
           '<p>Saat ESP32 mengirim sinyal ke servo, sinyal itu adalah tegangan 3,3 V <b>terhadap GND ESP32</b>. Servo membacanya terhadap GND-nya sendiri. '
           'Kalau dua GND itu tidak tersambung, servo tidak tahu "3,3 V terhadap apa". Sinyalnya tidak punya jalur pulang, dan tegangan yang dibaca servo '
           '<b>melayang</b> (floating): berubah-ubah sendiri, dipengaruhi apa saja di sekitarnya, termasuk tanganmu.</p>'
           + fig(g.gnd_bersama(), 'Kiri: ESP32 dari USB dan servo dari XL4015 tanpa kabel GND bersama. Kanan: satu kabel GND menutup jalur pulang sinyal.')
           + ok('Aturan yang perlu diingat', 'Setiap dua papan yang saling mengirim sinyal harus punya <b>GND bersama</b>. Yang disatukan hanya GND. '
                'Kutub + dari sumber yang berbeda (USB 5 V, LM2596 5 V, XL4015 6 V) <b>jangan</b> disatukan: dua sumber dengan tegangan sedikit berbeda akan saling melawan.')
           + kejadian('Saat tes kepala pan-tilt, ESP32 mendapat daya dari USB dan servo dari XL4015, tanpa kabel di antara GND keduanya (tes bip tidak bunyi). '
                      'Gejalanya: tilt diam, pan bergetar, LED XL4015 berkedip, dan menyentuh logam di jalur daya membuat servo bergerak. '
                      'Tubuh manusia selalu menangkap sedikit tegangan dari kabel listrik rumah di sekitarnya, dan titik yang melayang mudah terpengaruh olehnya. '
                      'Satu kabel dari pin G ke WAGO −6 menyelesaikan semuanya, padahal kapasitor sudah dicoba dilepas dan tidak berpengaruh. '
                      'Pola yang sama terjadi saat motor TB6612 belum mau jalan sebelum GND DevKit tersambung ke GND TB6612.')
           + tanya([
               ('ESP32-S3 mendapat daya dari LM2596 #1, ESP32-CAM dari LM2596 #2, dan keduanya berbicara lewat UART. Apa yang wajib tersambung selain TX dan RX?',
                'GND keduanya. Di robotmu itu sudah terjadi lewat GND bersama sistem daya, tapi kalau satu kabel GND lepas, data UART terbaca kacau.'),
               ('Bolehkah 5 V dari USB disambung ke 5 V dari LM2596 supaya "lebih kuat"?',
                'Jangan. Dua sumber dengan tegangan sedikit berbeda saling mendorong arus, dan arus bisa mengalir balik ke port USB laptop. Yang disatukan hanya GND.'),
               ('Kenapa saklar OFF mematikan seluruh robot padahal hanya memutus satu kabel?',
                'Karena arus butuh jalur tertutup. Satu titik yang putus menghentikan arus di seluruh jalur itu.')])
           + latihan(2, 'Memeriksa GND bersama dengan tes bip', [
               'Saklar OFF dan USB dicabut. Kenop multimeter ke mode bip (ikon gelombang suara atau dioda).',
               'Tempelkan dua ujung probe satu sama lain: harus bunyi. Itu tanda mode bip bekerja.',
               'Satu probe di pin <b>G</b> papan ekspansi, satu di lubang <b>WAGO −6</b>. Harus bunyi.',
               'Pin G papan ekspansi ke pin GND TB6612 (keduanya). Harus bunyi.',
               'WAGO 6V ke WAGO −6. Tidak boleh bunyi terus. Bunyi sebentar lalu diam itu normal: kapasitor sedang terisi oleh baterai multimeter.']))
    return section('bab2', 'Bab 2', 'Rangkaian tertutup dan GND bersama', isi, 'Bab terpenting di halaman ini. Kebanyakan rangkaian yang "aneh" ternyata kehilangan jalur pulang.')


def bab3():
    isi = ('<p><b>Seri</b>: disambung berurutan, kutub + satu ke kutub − berikutnya. <b>Tegangan dijumlah</b>, arusnya sama di semua bagian. '
           'Dua sel 18650 seri: 3,7 + 3,7 = 7,4 V (nominal), dan 4,2 + 4,2 = 8,4 V saat penuh.</p>'
           '<p><b>Paralel</b>: semua + jadi satu, semua − jadi satu. <b>Tegangannya sama</b>, arus dijumlah. Dua servo di WAGO 6V dan dua motor satu sisi di satu kanal TB6612 dipasang paralel.</p>'
           + fig(g.seri_paralel(), 'Seri menaikkan tegangan, paralel membagi arus ke beberapa beban dengan tegangan yang sama.')
           + table(['Resistor', 'Rumus', 'Contoh'], [
               ['Seri', 'R total = R1 + R2', '1 kΩ + 2 kΩ = 3 kΩ'],
               ['Paralel', '1 / R total = 1 / R1 + 1 / R2', '1 kΩ paralel 1 kΩ = 500 Ω (selalu lebih kecil dari yang terkecil)']])
           + kejadian('Kabel tengah di BMS (B1 atau "4,2 V") tersambung ke titik di antara dua sel. Dari kutub − baterai ke titik itu terukur satu sel (sampai 4,2 V), '
                      'dari titik itu ke kutub + terukur sel kedua. BMS memakai titik itu untuk menjaga dua sel tetap seimbang.')
           + tanya([
               ('Dua servo masing-masing menarik 0,6 A saat bergerak bersamaan. Berapa arus dari XL4015?', '1,2 A. Servo dipasang paralel, jadi arusnya dijumlah.'),
               ('Kalau satu dari dua sel seri terlepas dari holder, berapa tegangan di kutub robot?', '0 V. Rangkaian seri putus di satu titik, jadi tidak ada jalur sama sekali.'),
               ('Berapa total dua resistor 2 kΩ yang dipasang paralel?', '1 kΩ. Dua resistor sama besar yang paralel = setengah nilainya.')])
           + latihan(3, 'Seri dan paralel', [
               'Ukur sel 1, sel 2, dan total baterai robot. Total kira-kira sama dengan jumlah kedua sel.',
               'Hitung selisih dua sel. Idealnya kecil, di bawah sekitar 0,1 V. Selisih besar berarti satu sel lebih lemah atau balance tidak bekerja.',
               'Pasang dua resistor 1 kΩ seri di breadboard, ukur ujung ke ujung: sekitar 2 kΩ.',
               'Pasang keduanya paralel (dua kaki di baris yang sama), ukur: sekitar 500 Ω.']))
    return section('bab3', 'Bab 3', 'Seri dan paralel', isi, 'Dua cara menyusun komponen, dan kenapa baterai robotmu 7,4 V sementara setiap servo tetap mendapat 6 V.')


def bab4():
    isi = ('<p><b>Daya</b> (watt, W) adalah tegangan dikali arus: <b>P = V × I</b>. Robot yang menarik 2 A dari baterai 7,4 V memakai sekitar 15 W.</p>'
           '<p>Kabel dan sambungan juga punya hambatan kecil. Sebagian daya berubah jadi panas di sana, dan panasnya naik jauh lebih cepat daripada arusnya: '
           'arus dua kali lipat membuat panas di kabel yang sama empat kali lipat. Karena itu jalur motor memakai kabel lebih besar daripada jalur sinyal.</p>'
           + table(['Ukuran kabel', 'Dipakai di robot untuk', 'Catatan'], [
               ['22 AWG', 'jalur daya: baterai, WAGO, konverter, motor', 'angka AWG kecil = kabel besar'],
               ['26 AWG', 'GND servo, kabel kecil', 'cukup untuk sekitar 1-2 A di kabel pendek'],
               ['Jumper breadboard', 'sinyal dan arus kecil', 'jangan untuk jalur motor']])
           + '<p>Tidak ada satu angka "maksimal ampere" yang berlaku untuk semua kabel. Batasnya tergantung panjang kabel, apakah dibundel, jenis isolasi, dan berapa '
           'turun tegangan yang bisa diterima. Tabel yang sering dikutip menyebut 22 AWG sanggup 7 A untuk kabel pendek di udara terbuka, tapi hanya 0,92 A untuk '
           'jalur panjang, padahal kawatnya sama. Pegangan praktisnya: kabel tidak boleh terasa hangat saat robot bekerja keras.</p>'
           '<h3>Sekering</h3>'
           '<p>Sekering berisi kawat tipis yang sengaja meleleh kalau arus melewati batasnya, lalu memutus rangkaian sebelum kabel atau baterai terlalu panas. '
           'Sekering tidak melompat keluar dari dudukannya: ia putus di dalam dan harus diganti. Nilainya dipilih di atas arus normal dan di bawah kemampuan kabel yang dilindungi.</p>'
           + warn('Korsleting', 'Kalau kutub + bertemu kutub − tanpa beban di antaranya, hambatannya hampir nol dan arusnya sangat besar. Sel 18650 bisa memberi puluhan ampere. '
                  'Kabel tipis bisa memerah dalam hitungan detik. Inilah yang dicegah sekering 7,5 A dan BMS di robotmu.')
           + tanya([
               ('Robot menarik 2 A dari 7,4 V. Berapa dayanya?', '7,4 × 2 = 14,8 W.'),
               ('Apakah sekering 7,5 A melindungi ESP32 dari salah colok 7,4 V ke pin 3,3 V?',
                'Tidak. ESP32 rusak oleh tegangan yang salah walaupun arusnya kecil, jauh di bawah 7,5 A. Sekering hanya putus saat arus besar, misalnya korsleting.'),
               ('Kenapa sekering dipasang dekat baterai, bukan dekat motor?', 'Supaya seluruh kabel setelah baterai ikut terlindungi. Korsleting di mana pun setelah sekering akan memutusnya.')])
           + latihan(4, 'Daya dan panas', [
               'Baca angka di badan sekering robotmu (7,5) dan perhatikan warnanya. Sekering tipe pisau memakai warna untuk menandai nilai.',
               'Hitung daya dua servo yang menarik 1 A dari 6 V (jawaban: 6 W).',
               'Jalankan motor sekitar satu menit, matikan saklar, lalu raba kabel daya dan WAGO. Tidak boleh hangat. Hangat berarti kabel terlalu kecil atau sambungan kurang rapat.']))
    return section('bab4', 'Bab 4', 'Daya, ukuran kabel, dan sekering', isi, 'Kenapa kabel motor lebih besar, dan apa sebenarnya yang dilindungi sekering.')


def bab5():
    isi = ('<h3>Baterai 18650 (Li-ion)</h3>'
           + table(['Keadaan', 'Tegangan per sel', 'Robotmu (2 sel)'], [
               ['Penuh', '4,2 V', '8,4 V'],
               ['Nominal (tertulis di badan)', '3,6-3,7 V', '7,2-7,4 V'],
               ['Disimpan lama', 'sekitar setengah penuh (sekitar 3,7-3,8 V)', 'sekitar 7,5 V'],
               ['Diputus BMS-mu', '3,3 V', '6,6 V']])
           + '<p>Batas bawah yang aman berbeda antar merek sel; cek lembar data selmu. Sel yang dikosongkan di bawah batasnya bisa rusak permanen dan tidak aman diisi ulang. '
           '<b>BMS</b> memutus baterai saat terlalu penuh, terlalu kosong, arus terlalu besar, atau korsleting. BMS balance juga menjaga dua sel tetap setara.</p>'
           '<h3>Konverter buck: LM2596 dan XL4015</h3>'
           '<p>Konverter buck menurunkan tegangan dengan menyalakan-mematikan transistor ratusan ribu kali per detik, lalu menghaluskan hasilnya dengan kumparan (koil kuning) dan kapasitor. '
           'Cara ini efisien dan tidak banyak panas. Tegangan keluarannya diatur dengan trimpot, dan tegangan masuknya harus lebih tinggi beberapa volt; LM2596 butuh sekitar 1,5-2 V lebih tinggi dari keluarannya.</p>'
           + table(['Istilah', 'Artinya'], [
               ['CV (constant voltage)', 'modul menahan tegangan keluaran tetap, misalnya 6,0 V'],
               ['CC (constant current)', 'modul membatasi arus maksimum; kalau beban minta lebih, tegangannya diturunkan. XL4015 punya trimpot CC, LM2596 tidak'],
               ['Brownout', 'tegangan papan turun di bawah batas kerja chip sesaat, lalu chip reset sendiri']])
           + '<p>Robotmu memakai tiga konverter terpisah karena motor dan servo menarik lonjakan arus yang membuat tegangan turun sesaat. '
           'Kalau papan logika ikut di jalur yang sama, ia bisa brownout. Dengan jalur terpisah, lonjakan di 6 V tidak ikut menurunkan 5 V untuk ESP32.</p>'
           + warn('Ukur dulu, baru sambung', 'Atur trimpot konverter sampai tegangan keluarannya benar <b>sebelum</b> menyambung papan atau servo. Konverter baru dari pabrik bisa mengeluarkan tegangan hampir setinggi tegangan masuknya.')
           + kejadian('Saat servo bergerak, keluaran XL4015 turun dari 6,0 ke 5,8 V. Itu turun 3% dan normal. LED modul XL4015 punya arti berbeda antar model (lampu daya, atau tanda pembatas arus aktif), '
                      'jadi jangan menilai dari LED: ukur tegangannya. Log bootloop yang berisi "invalid magic number" juga pola khas daya yang tidak stabil saat papan menyala.')
           + tanya([
               ('LM2596 diberi 6,6 V (baterai hampir habis) dan diminta keluaran 5 V. Apa risikonya?', 'Selisihnya hanya 1,6 V, di batas bawah kebutuhan LM2596. Keluaran bisa turun di bawah 5 V saat beban naik, dan papan bisa reset.'),
               ('Kenapa ESP32 tidak diberi daya dari XL4015 6 V yang sama dengan servo?', 'Lonjakan arus servo dan motor menurunkan tegangan jalur itu sesaat. Kalau ESP32 ikut di jalur yang sama, ia bisa brownout dan reset. Dengan jalur 5 V sendiri, papan tetap stabil.'),
               ('Baterai robot terbaca 7,5 V setelah disimpan sebulan. Bagaimana kondisinya?', 'Baik: sekitar 3,75 V per sel, kira-kira setengah penuh, cocok untuk disimpan.')])
           + latihan(5, 'Mengukur sistem daya robot', [
               'Saklar ON, ukur total baterai di WAGO + dan −: harus 6,6-8,4 V.',
               'Ukur keluaran LM2596 #1 dan #2 tanpa beban: masing-masing 5,0 V.',
               'Ukur keluaran XL4015 tanpa beban: 6,0 V (5,9-6,1).',
               'Ukur keluaran XL4015 saat servo bergerak. Turun sedikit (misalnya ke 5,8 V) itu normal; turun lebih dari 0,5 V berarti dayanya mulai kurang.']))
    return section('bab5', 'Bab 5', 'Baterai, BMS, dan konverter tegangan', isi, 'Dari mana 8,4 V baterai berubah jadi 6 V untuk motor dan 5 V untuk papan.')


def bab6():
    isi = ('<h3>Resistor</h3>'
           '<p>Resistor membatasi arus (misalnya untuk LED) atau membagi tegangan. Nilainya ditulis dengan gelang warna: tiga atau empat gelang pertama angka dan pengali, '
           'gelang terakhir toleransi (seberapa jauh nilai aslinya boleh meleset). Resistor kecil di kit biasanya 1/4 W: cukup untuk sinyal, bukan untuk arus motor.</p>'
           + kalk_warna()
           + '<h3>Pembagi tegangan</h3>'
           '<p>Dua resistor seri membagi tegangan sesuai perbandingannya. Titik tengahnya memberi tegangan lebih kecil. '
           'Dipakai di robotmu untuk menurunkan sinyal echo HC-SR04 dari 5 V ke 3,3 V, dan untuk membaca tegangan baterai (20 kΩ dan 10 kΩ: 8,4 V jadi 2,8 V).</p>'
           + rumus('Vout = Vin × R2 / (R1 + R2)', 'R2 adalah resistor yang tersambung ke GND')
           + fig(g.pembagi(), 'Pembagi 1 kΩ dan 2 kΩ di pin echo sensor jarak.')
           + kalk_pembagi()
           + '<h3>Kapasitor</h3>'
           '<p>Kapasitor menyimpan muatan sebentar lalu melepaskannya. Di robotmu ada dua tugas: <b>penyangga lonjakan</b> (470 µF di rel 6 V, melepaskan muatan saat servo atau motor tiba-tiba menarik arus) '
           'dan <b>peredam noise cepat</b> (100 nF di kaki motor, menyerap percikan listrik dari sikat motor).</p>'
           + fig(g.kapasitor(), 'Elektrolit berkutub dan bernilai besar; keramik tidak berkutub dan bernilai kecil.')
           + table(['Kode keramik', 'Hitungan', 'Nilai'], [
               ['104', '10 × 10⁴ pF', '100.000 pF = 100 nF = 0,1 µF'],
               ['103', '10 × 10³ pF', '10 nF'],
               ['105', '10 × 10⁵ pF', '1 µF']])
           + warn('Kapasitor elektrolit', 'Kaki panjang (+) ke sisi positif, sisi bergaris (−) ke GND. Pakai kapasitor yang tegangan tertulisnya lebih tinggi dari tegangan kerja: '
                  'untuk rel 6 V minimal 10 V, lebih aman 16 V atau lebih (punyamu 50 V). Kapasitor terbalik bisa panas, bocor, menggembung, atau meletus.')
           + kejadian('Saat servo berperilaku aneh, kapasitor 470 µF dilepas dan tidak ada yang berubah. Itu masuk akal: kapasitor meredam lonjakan, '
                      'tapi tidak bisa menggantikan GND yang putus, sambungan yang longgar, atau servo yang menabrak rangka.')
           + tanya([
               ('Resistor 4 gelang merah-merah-coklat-emas. Berapa nilainya?', '22 × 10 = 220 Ω, toleransi ±5%.'),
               ('Kapasitor keramik bertulisan 103. Berapa nilainya?', '10 × 10³ pF = 10.000 pF = 10 nF.'),
               ('Kenapa echo HC-SR04 tidak boleh langsung ke pin ESP32?', 'Echo mengeluarkan 5 V, sedangkan pin ESP32 hanya tahan sampai sekitar 3,6 V. Pembagi 1 kΩ/2 kΩ menurunkannya ke 3,33 V.')])
           + latihan(6, 'Resistor dan kapasitor', [
               'Baca gelang warna dua resistor di kit, cocokkan dengan kalkulator, lalu ukur dengan mode Ω.',
               'Rangkai pembagi 1 kΩ + 2 kΩ di breadboard dari pin 5V ke G papan ekspansi. Ukur titik tengahnya ke G: sekitar 3,3 V.',
               'Temukan sisi bergaris (−) dan kaki panjang (+) di kapasitor 470 µF-mu.',
               'Baca kode tiga angka di kapasitor keramik kecil di kit dan hitung nilainya.']))
    return section('bab6', 'Bab 6', 'Resistor dan kapasitor', isi, 'Dua komponen kecil yang ada di hampir setiap rangkaian.')


def bab7():
    isi = ('<p><b>Dioda</b> hanya mengalirkan arus satu arah: dari anoda ke katoda (sisi bergaris). Saat mengalir, dioda "memakan" sekitar 0,6-0,7 V (dioda silikon biasa) atau lebih kecil (dioda Schottky).</p>'
           '<p><b>LED</b> adalah dioda yang menyala. Kaki panjang = anoda (+). LED selalu butuh resistor untuk membatasi arus:</p>'
           + rumus('R = (Vsumber − Vf) / I', 'Vf = tegangan yang "dimakan" LED; I = arus LED, biasanya 5-15 mA')
           + table(['Warna LED', 'Vf kira-kira'], [['Merah', '1,8-2,2 V'], ['Kuning, hijau biasa', '2,0-2,2 V'], ['Biru, putih, hijau terang', '2,8-3,4 V']])
           + kalk_led()
           + '<h3>Pin ESP32 bukan sumber daya</h3>'
           '<p>Menurut lembar data, pin ESP32 bisa memberi sekitar 40 mA pada kekuatan maksimum. Pakai jauh di bawah itu. Cukup untuk LED, tidak cukup untuk motor atau servo. '
           'Karena itu servo mendapat daya dari XL4015 dan pin ESP32 hanya memberi sinyal, sama seperti motor yang mendapat daya lewat TB6612.</p>'
           '<h3>Driver motor (TB6612)</h3>'
           '<p>TB6612 berisi empat saklar elektronik (H-bridge) yang menyambungkan motor ke baterai, arahnya bisa dibalik. ESP32 hanya memberi perintah: '
           'AIN1/AIN2 untuk arah, PWMA untuk kecepatan. Motor DC menarik arus besar saat mulai berputar dan saat tertahan, dan sikatnya memercikkan noise, '
           'karena itu ada kapasitor 100 nF di kakinya.</p>'
           + kejadian('Roda kiri berputar terbalik. Membalik dua kabel motor (AO1 dan AO2) membalik arah putarnya, karena arus di motor DC berbalik. Bisa juga dibalik lewat kode (MOTOR_KIRI_TERBALIK).')
           + tanya([
               ('LED biru (Vf 3,0 V) di 5 V dengan arus 10 mA. Resistor berapa?', '(5 − 3) / 0,010 = 200 Ω. Pakai nilai standar terdekat di atasnya: 220 Ω.'),
               ('LED dipasang terbalik dengan resistor di 3,3 V. Apa yang terjadi?', 'Tidak menyala, karena dioda menahan arus arah terbalik. Pada tegangan serendah ini LED tidak rusak.'),
               ('Kenapa motor TT tidak boleh langsung ke pin ESP32?', 'Motor butuh ratusan mA sampai lebih dari 1 A saat tertahan, jauh di atas kemampuan pin. Motor juga memercikkan noise yang bisa merusak pin.')])
           + latihan(7, 'LED dan dioda', [
               'Rangkai LED dan resistor 220 Ω dari pin 3V ke G papan ekspansi, kaki panjang ke sisi 3V. LED menyala.',
               'Balik LED-nya. Tidak menyala, dan tidak rusak.',
               'Ukur LED dengan mode dioda multimeter: satu arah terbaca angka (LED merah sekitar 1,6-2,0, kadang menyala redup), arah sebaliknya "OL" atau "1".',
               'Hitung resistor untuk LED merah di 5 V dan 10 mA dengan kalkulator (jawaban: 300 Ω, pakai 330 Ω).']))
    return section('bab7', 'Bab 7', 'Dioda, LED, dan driver motor', isi, 'Komponen yang hanya mengalirkan arus satu arah, dan kenapa motor butuh perantara.')


def bab8():
    isi = ('<h3>Digital dan logika 3,3 V</h3>'
           '<p>Sinyal digital hanya punya dua keadaan: <b>HIGH</b> (sekitar 3,3 V di ESP32) dan <b>LOW</b> (0 V), selalu terhadap GND. '
           'Pin input ESP32 tidak boleh diberi lebih dari sekitar 3,6 V. Sinyal 5 V (seperti echo HC-SR04) harus diturunkan dulu dengan pembagi tegangan atau level shifter.</p>'
           '<p><b>Input yang melayang.</b> Pin input yang tidak tersambung ke mana pun membaca HIGH dan LOW secara acak, persis seperti servo tanpa GND bersama di Bab 2. '
           'Untuk tombol, pakai <b>pull-up</b>: <code>pinMode(pin, INPUT_PULLUP)</code> menyambungkan pin ke 3,3 V lewat resistor sekitar 45 kΩ di dalam chip, dan tombol menariknya ke GND saat ditekan.</p>'
           '<h3>PWM</h3>'
           '<p>PWM menyalakan dan mematikan sinyal sangat cepat. <b>Duty cycle</b> adalah persen waktu menyala. Motor terlalu lambat untuk mengikuti kedipan itu, jadi ia merasakan rata-ratanya.</p>'
           + fig(g.pwm(), 'Duty 50% dari 6 V terasa sekitar 3 V bagi motor.')
           + '<h3>Servo</h3>'
           '<p>Servo tidak membaca rata-rata, tapi <b>lebar pulsa</b> yang diulang 50 kali per detik. Di dalam servo ada potensiometer yang membaca posisi lengan, '
           'dan servo terus mengoreksi sampai posisinya sama dengan perintah. Kalau diperintah ke sudut yang tidak bisa dicapai (lengan menabrak rangka), '
           'servo terus mendorong: bergetar, berdengung, dan menarik arus besar.</p>'
           + fig(g.servo_pulsa(), 'Makin lebar pulsanya, makin besar sudutnya.')
           + kejadian('Pan diperintah ke 180° lalu terus bergerak kecil di ujung dan LED XL4015 terang-redup. Itu tanda servo tidak bisa mencapai sudut yang diperintahkan dan terus mencoba. '
                      'Solusinya memakai batas sudut yang bisa dicapai tanpa getar, bukan menambah daya.')
           + '<h3>Pin boot dan ADC</h3>'
           + ul(['<b>Pin boot (strapping):</b> GPIO 0, 2, 5, 12, dan 15 dibaca ESP32 saat menyala untuk menentukan cara boot. GPIO 12 yang ditarik HIGH saat boot membuat chip memakai tegangan flash yang salah, dan papan gagal boot. Panduan robot menghindari pin-pin ini untuk perangkat yang bisa menariknya saat menyala.',
                 '<b>ADC (analogRead):</b> membaca tegangan 0 sampai sekitar 3,1 V menjadi angka 0-4095. ADC ESP32 kurang akurat di dekat 0 V dan di dekat batas atasnya, jadi tegangan baterai di layar robot adalah perkiraan, bukan alat ukur.'])
           + table(['Komunikasi', 'Kabel', 'Yang perlu diingat'], [
               ['UART (Serial)', 'TX, RX, GND', 'TX satu papan ke RX papan lain (disilang), dan GND bersama'],
               ['I2C', 'SDA, SCL, GND', 'banyak perangkat di dua kabel yang sama, dibedakan alamat; butuh resistor pull-up (biasanya sudah ada di modul)'],
               ['SPI', 'SCK, MOSI, MISO, CS', 'tiga kabel dipakai bersama; tiap perangkat punya pin CS sendiri']])
           + tanya([
               ('Duty 25% dari rel motor 6 V. Kira-kira berapa yang dirasakan motor?', 'Sekitar 1,5 V rata-rata. Kemungkinan tidak cukup untuk memutar roda TT; motor hanya berdengung.'),
               ('Servo diam dengan tenang di 90° tapi bergetar terus di 180°. Apa artinya?', 'Di 180° lengan menabrak batas mekanis, dan servo terus mencoba mencapai sudut itu. Kecilkan batas sudut di kode.'),
               ('Tombol di pin input tanpa resistor dan tanpa INPUT_PULLUP. Apa yang terjadi saat tombol tidak ditekan?', 'Pin melayang dan membaca HIGH/LOW acak.')])
           + latihan(8, 'Sinyal dan servo', [
               f'Buka tes servo di <a href="{ROBOT}2-badan/#tahap4">Bagian 2 Tahap 4</a>. Ketik <code>90 90</code>, lalu naikkan pan sedikit demi sedikit (120, 140, 160).',
               'Catat sudut terbesar yang tidak bergetar dan tidak berdengung untuk pan dan tilt.',
               'Cari pin TX dan RX DevKit dan ESP32-S3 di panduan robot, dan pastikan sambungannya disilang (TX ke RX).']))
    return section('bab8', 'Bab 8', 'Sinyal digital, PWM, dan servo', isi, 'Cara ESP32 "berbicara" dengan servo, motor, dan papan lain.')


def bab9():
    isi = (fig(g.multimeter(), 'Kenop memilih apa yang diukur, colokan menentukan jalur probe merah.')
           + table(['Mode', 'Untuk', 'Syarat'], [
               ['V⎓ (DC volt)', 'baterai, keluaran konverter, pin papan', 'rangkaian menyala, probe di dua titik'],
               ['V~ (AC volt)', 'listrik rumah', 'tidak dipakai di proyek ini'],
               ['Ω (ohm)', 'resistor, kabel', 'daya MATI, komponen sebaiknya dilepas'],
               ['Bip (continuity)', 'apakah dua titik tersambung', 'daya MATI'],
               ['Dioda', 'LED, dioda, arah kutub', 'daya MATI'],
               ['A / mA', 'arus', 'rangkaian dibuka, multimeter disisipkan seri; probe merah pindah colokan']])
           + ul(['<b>COM</b> selalu untuk probe hitam. <b>VΩmA</b> untuk hampir semua pengukuran. <b>10A</b> hanya untuk mengukur arus besar secara seri.',
                 '<b>Angka minus</b> berarti probe terbalik. Besarnya tetap benar.',
                 '<b>"OL" atau "1" di kiri layar</b> berarti di luar rentang atau tidak tersambung.',
                 '<b>Multimeter manual</b> (ada angka 2, 20, 200 di kenop): pilih rentang di atas angka yang kamu harapkan, misalnya 20 V untuk baterai 7,4 V.',
                 'Mode Ω pada komponen yang masih terpasang ikut mengukur komponen lain yang tersambung, jadi hasilnya bisa menyesatkan.'])
           + warn('Jangan ukur tegangan di mode arus', 'Di mode A atau colokan 10A, multimeter hampir tanpa hambatan. Menempelkannya ke dua kutub baterai sama dengan korsleting: sekering multimeter putus, atau lebih parah.')
           + kejadian('Saat pertama mengukur baterai, layar menunjukkan 0 terus walaupun saklar ON dan OFF. Ternyata kenop menunjuk ke 10 A, bukan V⎓. '
                      'Setelah diputar ke V⎓, saklar ON terbaca 7,8 V dan OFF 0 V. Probe di lubang WAGO yang sudah berisi kabel atau yang kosong sama saja, karena semua lubang satu WAGO tersambung di dalam.')
           + tanya([
               ('Mengapa multimeter tidak boleh ditempel ke baterai saat probe merah di colokan 10A?', 'Mode arus hampir tanpa hambatan, jadi terjadi korsleting lewat multimeter.'),
               ('Layar menunjukkan −6,0 V. Apa artinya?', 'Probe merah dan hitam terbalik. Tegangannya tetap 6,0 V.'),
               ('Kenapa tes bip harus saat daya mati?', 'Tes bip mengalirkan arus kecil dari baterai multimeter sendiri. Tegangan dari rangkaian bisa membuat hasilnya salah atau merusak multimeter.')])
           + latihan(9, 'Kenali multimetermu', [
               'Lihat kenopmu: auto-range (hanya simbol V, Ω) atau manual (ada angka 2, 20, 200)?',
               'Tes bip satu kabel jumper dari ujung ke ujung. Kabel jumper yang rusak di dalam sering tidak terlihat dari luar.',
               'Ukur baterai, keluaran LM2596, dan keluaran XL4015 dengan rentang yang benar, lalu kembalikan kenop ke OFF.']))
    return section('bab9', 'Bab 9', 'Memakai multimeter', isi, 'Alat paling penting untuk merakit. Kebanyakan kesalahan baca berasal dari kenop dan colokan.')


def bab10():
    isi = (fig(g.solder(), 'Solderan yang baik membungkus kaki dan pad; solderan dingin hanya menempel.')
           + ul(['<b>Panaskan benda yang disolder, bukan timahnya.</b> Tempelkan ujung solder ke kaki dan pad bersama 1-2 detik, sentuhkan timah ke sambungan sampai mengalir, angkat, lalu jangan digerakkan sampai membeku.',
                 '<b>Kabel:</b> kupas, puntir serabutnya, lapisi tipis dengan timah dulu (tinning), baru disatukan. Sambungan yang serabutnya banyak putus lebih lemah; potong dan kupas ulang.',
                 '<b>Heat shrink dimasukkan sebelum menyolder</b>, karena tidak bisa dimasukkan setelah kedua ujung tersambung.',
                 '<b>Sambungan tanpa solder</b> (jumper, WAGO, terminal sekrup): tarik pelan setelah dipasang. Terminal sekrup harus menjepit tembaga, bukan isolasi.'])
           + info('Kalau disentuh, rangkaian berubah', 'Ada dua penyebab yang sering tertukar. <b>Sambungan longgar:</b> sentuhan menekan kontak sampai tersambung atau terputus. '
                  '<b>Titik yang melayang:</b> GND atau input tidak tersambung, dan tanganmu menyalurkan noise. Bedakan dengan tes bip (Bab 2) dan dengan menekan memakai benda tidak menghantar seperti tusuk gigi.')
           + tanya([
               ('Solderan berbentuk bola buram dan kabelnya sedikit bergoyang. Apa yang perlu dilakukan?', 'Itu solderan dingin. Panaskan ulang sampai timah mengalir membungkus kaki dan pad, tambahkan sedikit timah baru kalau perlu.'),
               ('Kenapa sambungan kabel motor perlu ditarik setelah disolder?', 'Robot bergetar saat jalan. Sambungan yang lepas saat ditarik pelan pasti lepas saat dipakai.')])
           + latihan(10, 'Periksa sambunganmu', [
               'Tarik pelan setiap kabel jumper F-F di TB6612 dan papan ekspansi. Tidak boleh lepas.',
               'Kencangkan semua terminal sekrup LM2596 dan XL4015 (daya mati).',
               'Periksa solderan kepala servo dan kaki motor di bawah lampu terang: mengkilap dan tidak retak.']))
    return section('bab10', 'Bab 10', 'Solder dan sambungan', isi, 'Banyak masalah "elektronik" ternyata sambungan yang kurang rapat.')


def bab11():
    isi = (rumus('Amati → ukur → pisahkan → tukar → ubah satu hal', 'lalu catat hasilnya')
           + ul(['<b>Tulis gejalanya persis.</b> "Tilt diam di semua sudut, pan bergetar di 180°" lebih berguna daripada "servo rusak".',
                 '<b>Ukur, jangan menebak.</b> Ukur tegangan di titik pemakaian (kaki servo, pin VM), bukan hanya di sumber.',
                 '<b>Pisahkan.</b> Lepas semua beban, nyalakan, lalu pasang satu per satu sampai masalahnya muncul.',
                 '<b>Tukar dengan yang diketahui baik.</b> Colokkan servo tilt ke pin pan: kalau jalan, servonya sehat.',
                 '<b>Ubah satu hal setiap kali.</b> Mengganti kapasitor, kode, dan kabel sekaligus membuatmu tidak tahu mana penyebabnya.'])
           + '<p>Urutan cek yang paling sering benar: <b>daya → GND bersama → polaritas → sambungan → pin dan kode → mekanis</b>.</p>'
           + table(['Gejala', 'Penyebab yang sering', 'Cek'], [
               ['Multimeter 0 di baterai yang terisi', 'kenop atau colokan salah', 'kenop ke V⎓, merah di VΩmA'],
               ['Motor diam, tidak ada suara', 'GND DevKit–TB6612 putus, atau VM tidak dapat tegangan', 'tes bip G ke GND TB6612; ukur VM ke GND: 6 V'],
               ['Satu roda berputar terbalik', 'polaritas kabel motor', 'tukar AO1/AO2, atau balik di kode'],
               ['Servo acak, diam, atau bergerak saat disentuh', 'GND bersama hilang', 'tes bip G ke WAGO −6'],
               ['Servo bergetar hanya di ujung gerak', 'lengan menabrak rangka', 'kecilkan batas sudut'],
               ['ESP32 restart saat motor atau servo mulai', 'brownout atau noise', 'daya terpisah, kapasitor, GND bersama'],
               ['Bootloop, "invalid magic number" di Serial Monitor', 'daya tidak stabil saat boot, atau pin boot ditarik', 'lepas beban, cek GPIO 12, hapus flash lalu upload ulang'],
               ['Kompilasi gagal: ledcAttach tidak dikenal', 'ESP32 core versi lama', 'perbarui ESP32 core ke 3.x di Boards Manager'],
               ['Kapasitor panas atau menggembung', 'terbalik, atau tegangannya terlalu rendah', 'matikan daya, ganti, pasang dengan kutub benar']])
           + tanya([
               ('Kamu mengganti kapasitor, kode, dan kabel sekaligus, lalu masalahnya hilang. Apa kekurangannya?', 'Kamu tidak tahu mana penyebabnya, dan masalah yang sama bisa kembali tanpa kamu tahu harus mencari di mana.'),
               ('Tilt tidak bergerak, tapi pan bergerak. Apa yang sudah terbukti baik?', 'Kode, papan ESP32, dan daya bersama ke kepala. Yang tersisa: kabel sinyal tilt, pin D4, sambungan daya servo tilt, atau servonya.')])
           + latihan(11, 'Buat catatanmu sendiri', [
               'Pilih satu masalah yang pernah kamu alami saat merakit. Tulis: gejala, apa yang diukur, penyebab, dan perbaikannya.',
               'Siapkan buku atau catatan di HP khusus untuk proyek ini, dan biasakan menulis sebelum mengubah sesuatu.']))
    return section('bab11', 'Bab 11', 'Cara mencari kesalahan', isi, 'Cara kerja yang membuat masalah ketemu dalam menit, bukan jam.')


def bab12():
    isi = (ul(['<b>Selalu ada sekering dekat baterai.</b> Korsleting di sel 18650 bisa memanaskan kabel dalam hitungan detik.',
               '<b>Cabut baterai atau sekering</b> saat menyolder atau mengubah kabel. Saklar hanya memutus sebagian jalur.',
               '<b>Jangan menaruh alat logam</b> (obeng, pinset, gunting) di dekat terminal baterai yang terbuka.',
               '<b>Isi daya sambil diawasi</b>, di atas permukaan yang tidak mudah terbakar. Jangan mengisi sel yang menggembung, penyok, bocor, atau sudah lama sangat kosong.',
               '<b>Simpan sekitar setengah penuh.</b> Untuk penyimpanan lebih dari seminggu, lepas sel dari holder.',
               '<b>Kapasitor elektrolit besar</b> masih menyimpan muatan sebentar setelah daya mati. Di 6-8 V ini tidak berbahaya bagi tubuh, tapi bisa memercik kalau kakinya dihubung singkat.',
               '<b>Listrik rumah 220 V di luar cakupan halaman ini.</b> Jangan membuka adaptor atau charger.'])
           + latihan(12, 'Keselamatan', [
               'Pastikan sekering terpasang dekat baterai robot dan nilainya 7,5 A.',
               'Siapkan tempat mengisi baterai yang tidak mudah terbakar dan bisa kamu awasi.',
               'Periksa kedua sel: tidak menggembung, tidak penyok, pembungkusnya tidak sobek.']))
    return section('bab12', 'Bab 12', 'Keselamatan baterai', isi, 'Baterai Li-ion aman kalau diperlakukan dengan benar, dan berbahaya kalau dihubung singkat.')


def istilah():
    rows = [
        ['GND', 'titik acuan 0 V yang dipakai bersama'], ['VCC, V+, 3V3, 5V', 'kutub positif daya, dengan tegangan tertentu'],
        ['VIN', 'pin masukan daya papan, biasanya ke regulator di papan'], ['VM', 'pin daya motor di TB6612'],
        ['Polaritas', 'arah kutub + dan −'], ['Korsleting', 'kutub + bertemu − tanpa beban; arus sangat besar'],
        ['Floating (melayang)', 'titik yang tidak tersambung ke tegangan tertentu, sehingga nilainya acak'],
        ['Pull-up, pull-down', 'resistor yang menarik input ke HIGH atau LOW saat tidak ada sinyal'],
        ['PWM, duty cycle', 'sinyal menyala-mati cepat; persen waktu menyala'], ['Brownout', 'tegangan turun sesaat sampai chip reset'],
        ['Buck converter', 'modul penurun tegangan yang efisien (LM2596, XL4015)'], ['CV, CC', 'mode tegangan tetap dan mode batas arus'],
        ['BMS', 'pelindung baterai Li-ion'], ['Vf', 'tegangan yang dimakan LED atau dioda saat menyala'],
        ['H-bridge', 'susunan empat saklar yang bisa membalik arah motor'], ['Strapping pin', 'pin yang dibaca ESP32 saat boot'],
        ['Continuity (bip)', 'tes apakah dua titik tersambung'], ['AWG', 'ukuran kawat; angka kecil = kawat besar'],
        ['mA, µF, nF, kΩ', 'mili (seperseribu), mikro (sepersejuta), nano (sepermiliar), kilo (seribu)']]
    return section('istilah', 'Rujukan', 'Daftar istilah', table(['Istilah', 'Artinya'], rows))


def sumber():
    isi = ul([
        'Karakteristik pin ESP32 (batas input VDD + 0,3 V, arus pin 40 mA, pull-up sekitar 45 kΩ): ringkasan lembar data ESP32 di '
        '<a href="https://deneyapkart.org/en/docs/elektriksel-ozellikler-esp32-2">Deneyap Kart, Electrical Specifications (ESP32)</a>.',
        'Penyimpanan dan batas tegangan Li-ion: <a href="https://forum.digikey.com/t/li-ion-lipo-battery-storage-and-permanent-capacity-loss/19946">DigiKey TechForum, Li-ion / LiPo Battery Storage and Permanent Capacity Loss</a>.',
        'Perbedaan angka arus kabel 22 AWG untuk kabel pendek dan jalur panjang: <a href="https://www.eevblog.com/forum/beginners/_maximum-amps-for-chassis-wiring_-vs-_maximum-amps-for-power-transmission_/">EEVblog Forum, chassis wiring vs power transmission</a>.',
        'Angka Vf LED, tegangan dioda, dan lebar pulsa servo adalah nilai umum yang berbeda antar produk. Untuk komponenmu, lembar data atau keterangan penjual lebih tepat.'])
    return section('sumber', 'Rujukan', 'Sumber', isi)
