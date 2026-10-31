from build import *
import kabel as KB
import peta
from diag import C, H, RP, RY, bwire, bres, btag, bltag, bdot, breadboard

S = '2-badan'
IMG = '../img/'

# ---------- isi breadboard kecil di badan ----------
def _rel():
    s = bltag(70, '3V') + bwire((50, 70), RP('tp', 1), '3v3', 0, True, False)
    s += bltag(50, 'G') + bwire((50, 50), RP('tm', 1), 'gnd', 0, True, False)
    # rel bawah: GND dan 3,3 V disambung dari rel atas lewat sisi kanan
    a, b = RP('tm', 29), RP('bm', 29)
    s += (f'<path d="M{a[0]} {a[1]} C 676 {a[1]} 676 {b[1]} {b[0]} {b[1]}" fill="none" stroke="#f4f6f5" stroke-opacity=".45" stroke-width="6"/>'
          f'<path d="M{a[0]} {a[1]} C 676 {a[1]} 676 {b[1]} {b[0]} {b[1]}" fill="none" stroke="#1b1b1b" stroke-width="3.6"/>' + bdot(a) + bdot(b))
    a, b = RP('tp', 28), RP('bp', 28)
    s += (f'<path d="M{a[0]} {a[1]} C 662 {a[1]} 662 {b[1]} {b[0]} {b[1]}" fill="none" stroke="#0b0d0c" stroke-opacity=".45" stroke-width="6"/>'
          f'<path d="M{a[0]} {a[1]} C 662 {a[1]} 662 {b[1]} {b[0]} {b[1]}" fill="none" stroke="#f08a24" stroke-width="3.6"/>' + bdot(a) + bdot(b))
    return s

def _top(col, t, colr, start='a'):
    return bwire(H(f'{start}{col}'), (C(col), 27), colr, 0, False) + btag(C(col), 18, t)

def _5v():
    return (bwire(H('a30'), (C(30), 27), '5v', 0, False) + btag(C(30) - 6, 18, '5V'))

PARTS = [
    (5, _rel),
    (5, _5v),
    (5, lambda: _top(3, 'ECHO dpn', 'hijau')),
    (5, lambda: bres(H('b3'), H('b7'), ['n', 'k', 'k', 'n', 'n'])),
    (5, lambda: bres(H('c7'), RP('tm', 9), ['r', 'k', 'k', 'n', 'n'])),
    (5, lambda: _top(7, 'D19', 'kuning')),
    (5, lambda: _top(12, 'ECHO blk', 'hijau')),
    (5, lambda: bres(H('b12'), H('b16'), ['n', 'k', 'k', 'n', 'n'])),
    (5, lambda: bres(H('c16'), RP('tm', 18), ['r', 'k', 'k', 'n', 'n'])),
    (5, lambda: _top(16, 'D35', 'cyan')),
    # DHT11 (hanya kalau tidak ada BME280)
    (6, lambda: ''.join(f'<line x1="{C(c)}" y1="214" x2="{C(c)}" y2="{RY["f"]}" stroke="#9ea4a7" stroke-width="2"/>' for c in (10, 11, 12, 13))
        + f'<rect x="{C(10) - 10}" y="190" width="{C(13) - C(10) + 20}" height="26" rx="3" fill="#43a9d8" stroke="#1d6f94"/>'
        + ''.join(f'<rect x="{C(10) - 4 + i * 11}" y="{195 + j * 9}" width="6" height="5" fill="#1f6f93"/>' for i in range(6) for j in range(2))
        + ''.join(bdot(H(f'f{c}')) for c in (10, 11, 12, 13))),
    (6, lambda: bwire(H('j10'), RP('bp', 10), '3v3')),
    (6, lambda: bres(H('j11'), RP('bp', 11), ['n', 'k', 'k', 'r', 'n'])),
    (6, lambda: bwire(H('j13'), RP('bm', 13), 'gnd')),
    (6, lambda: bwire(H('h11'), (C(9), 395), 'biru', -8, False) + btag(C(9), 404, 'D5')),
    # pembagi baterai + tombol STOP
    (7, lambda: bwire(H('a20'), (C(20), 27), 'bat', 0, False) + btag(C(20), 18, 'BAT+')),
    (7, lambda: bres(H('b20'), H('b24'), ['r', 'k', 'k', 'r', 'n'])),
    (7, lambda: bres(H('c24'), RP('tm', 25), ['n', 'k', 'k', 'r', 'n'])),
    (7, lambda: _top(24, 'D34', 'kuning')),
    (7, lambda: f'<rect x="{C(27) - 9}" y="181" width="{C(29) - C(27) + 18}" height="58" rx="4" fill="#2b2b2b" stroke="#000"/>'
        f'<circle cx="{(C(27) + C(29)) / 2}" cy="210" r="11" fill="#111" stroke="#666"/>'
        + bdot(H('e27')) + bdot(H('e29')) + bdot(H('f27')) + bdot(H('f29'))),
    (7, lambda: bres(H('a27'), RP('tp', 27), ['n', 'k', 'k', 'r', 'n'])),
    (7, lambda: bwire(H('c27'), (C(26) - 4, 27), 'pink', 12, False) + btag(C(26) - 4, 18, 'D39')),
    (7, lambda: bwire(H('j29'), RP('bm', 27), 'gnd')),
]

def bb(stage, label):
    return fig(breadboard(PARTS, stage, label), 'Bagian yang baru dipasang tampil terang, bagian tahap sebelumnya pudar. Titik putih = ujung kaki atau kabel di lubang. Tag di tepi atas/bawah = kabel keluar ke board atau sensor.')


def halaman():
    isi = ''
    isi += section('peta', 'Gambaran', 'Apa yang dikerjakan badan', f'''
<div class="panel">
  <p>Badan robot dikendalikan <b>ESP32 DevKit V1</b> di papan ekspansi 30P. Ia menggerakkan roda dan kepala, membaca semua sensor, dan menjalankan aturan keselamatan sendiri. Otak (ESP32-S3) hanya mengirim perintah singkat seperti <code>M 150 150 800</code> (maju 0,8 detik).</p>
  <div class="photos">{photo(IMG + 'devkit.jpg', 'ESP32 DevKit V1', 'ESP32 DevKit V1 (30 pin).')}{photo(IMG + 'ekspansi30p.jpg', 'Papan ekspansi 30P', 'Papan ekspansi 30P.')}{photo(IMG + 'chassis.jpg', 'Chassis 4WD dua lantai', 'Chassis 4WD dua lantai.')}</div>
</div>''' + table(['Fungsi', 'Pin DevKit (baris di papan 30P)', 'Ke'],
        [['Motor kiri', f'{pin("D25")} PWMA, {pin("D26")} AIN1, {pin("D27")} AIN2', 'TB6612'],
         ['Motor kanan', f'{pin("D13")} PWMB, {pin("D32")} BIN1, {pin("D33")} BIN2', 'TB6612'],
         ['Servo pan / tilt', f'{pin("D23")} / {pin("D4")}', 'kabel oranye servo'],
         ['Jarak depan', f'{pin("D18")} TRIG, {pin("D19")} ECHO (lewat pembagi)', 'HC-SR04 depan'],
         ['Jarak belakang', f'{pin("D14")} TRIG, {pin("D35")} ECHO (lewat pembagi)', 'HC-SR04 belakang'],
         ['I2C', f'{pin("D21")} SDA, {pin("D22")} SCL', 'MPU6050, BME280'],
         ['DHT11 (kalau tanpa BME280)', pin('D5'), 'DATA DHT11'],
         ['Tombol STOP', pin('D39') + ' (VN)', 'tombol + pull-up 10k'],
         ['Tegangan baterai', pin('D34'), 'pembagi 20k/10k'],
         ['Ke otak (UART2)', f'{pin("RX2")} = D16, {pin("TX2")} = D17', 'ESP32-S3'],
         ], 'Semua sinyal memakai pin S (paling luar) di baris yang namanya sama. Daya memakai blok GND / 5V / 3V di kiri bawah papan.'))

    tahap = ''
    leg = ul([
        '<b>1</b> Blok daya: kolom kiri semua <b>GND</b>, kolom kanan berisi pin <b>5V</b> dan <b>3V</b> (cek di langkah 3). Di mode jalan, 5 V dari LM2596 masuk ke salah satu pin 5V ini.',
        '<b>2</b> Jumper pemilih tegangan pin V: pindahkan ke posisi <b>3.3V–V</b>. Semua pin V di papan lalu bertegangan 3,3 V.',
        '<b>3</b> dan <b>4</b>: baris pin tiap GPIO (S = sinyal paling luar, V, G). Nama baris tercetak di sampingnya: D25, D26, D23, D22, dan seterusnya.',
        '<b>5</b> Header I2C: D22, D21, VCC, GND. Dipakai MPU6050 di Tahap 6.',
        '<b>6</b> Jack DC 6,5-16 V: <b>tidak dipakai</b>. <b>7</b> Port USB papan: hanya daya, tidak dipakai. Upload lewat port micro-USB di modul DevKit.',
    ])
    tahap += stage(S, 1, 'Siapkan DevKit di papan 30P', ['DevKit V1', 'Papan ekspansi 30P', 'Kabel micro-USB data', 'Multimeter'], [
        'Pindahkan jumper pemilih tegangan ke posisi <b>3.3V–V</b> (nomor 2 di foto).',
        'Pasang DevKit ke papan: port micro-USB DevKit menghadap ke sisi jack DC, tulisan D23 di DevKit sejajar baris D23 papan, ke-30 pin masuk soket. Tekan rata pelan-pelan.',
        'Belum pernah memakai Arduino IDE dengan ESP32? Kerjakan dulu <a href="../../stasiun-cuaca/#tahap0">Tahap 0 panduan stasiun cuaca</a>: install Arduino IDE 2 dan board <b>esp32 by Espressif</b>, pilih <b>ESP32 Dev Module</b>, upload kode kedip LED.',
        'Colok USB ke DevKit. Multimeter V⎓ 20, probe hitam di pin GND blok daya. Ukur tiap pin di kolom kanan blok daya: catat mana yang terbaca sekitar <b>4,6-5 V</b> (pin 5V) dan mana yang <b>3,3 V</b> (pin 3V). Tandai dengan spidol.',
        'Ukur juga pin V di baris mana saja: harus 3,3 V. Kalau 5 V, jumper belum pindah.',
        'Install library di Arduino IDE (<b>Tools → Manage Libraries</b>): <b>Adafruit BME280 Library</b> dan <b>DHT sensor library</b>. Pilih <b>Install All</b> saat ditanya.',
    ], figs=photo(IMG + 'ekspansi30p_anot.jpg', 'Papan ekspansi 30P dengan nomor bagian') + leg,
        ok_='Kode kedip berjalan di DevKit, dan kamu sudah tahu pin mana yang 5 V dan mana yang 3,3 V di blok daya.',
        gagal=['Port tidak muncul: kabel USB-mu mungkin kabel charge saja. Coba kabel lain. Chip USB di DevKit V1 umumnya CP2102: install driver CP210x kalau perlu.'])

    tahap += stage(S, 2, 'Rakit chassis dan siapkan motor', ['Chassis 4WD + 4 motor TT', 'Obeng', 'Solder', 'Kabel motor (bawaan atau 26 AWG)', '1 kabel jumper F-F', 'Heat shrink', '4 kapasitor 100 nF'], [
        'Lepas kertas pelindung di kedua plat akrilik (dari sudut, pakai kuku).',
        'Pasang keempat motor ke plat <b>bawah</b> dengan dudukan akrilik berbentuk T dan baut panjang M3. Poros motor menghadap keluar, kaleng motor (yang ada dua kaki logamnya) menghadap ke tengah plat, seperti di foto susun kering lantai 1 (Bagian 6). Kencangkan secukupnya, akrilik bisa retak kalau terlalu kencang.',
        f'Panjang kabel motor (dari kaki motor): motor <b>belakang {KB.pn("motor")}</b> (sampai TB6612 di ujung depan lantai 1), motor <b>depan {KB.pn("motord")}</b>. Kabel bawaan yang lebih panjang dipotong; yang lebih pendek disambung 26 AWG. Belum tersolder: solder satu merah dan satu hitam ke dua kaki logam tiap motor. Panaskan sebentar saja, plastik motor mudah meleleh.',
        'Kalau kapasitor 100 nF sudah ada, solder satu di antara dua kaki tiap motor (kapasitor keramik tidak berkutub). Ini meredam percikan sikat motor yang bisa membuat ESP32 reset.',
        'Pilin kabel merah-hitam tiap motor supaya rapi dan tidak menjadi "antena" noise.',
        'Gabungkan per sisi: kabel motor <b>depan kiri</b> disolder ke kabel motor <b>belakang kiri</b> yang lewat di sebelahnya, merah dengan merah, hitam dengan hitam, lalu heat shrink. Lakukan juga di kanan. Sekarang tiap sisi punya satu pasang kabel yang ujungnya sampai ke depan.',
        'Potong kabel jumper F-F menjadi empat potongan ujung betina sepanjang 3 cm (dua dari tiap ujung; sisanya simpan). Solder ke ujung depan keempat kabel motor dan heat shrink, supaya bisa ditancap ke pin TB6612.',
        'Pasang roda ke poros motor: tekan lurus sampai rapat. Belum perlu memasang plat atas.',
    ], figs=photo(IMG + 'chassis.jpg', 'Chassis dengan motor dan roda', 'Plat bawah dengan empat motor TT dan roda.', 420) + fig(diag.motor_kapasitor(), 'Pasangan kapasitor 100 nF di tiap motor: melintang di antara dua kaki logam, satu kaki kapasitor di tiap kaki motor.') + fig(peta.lantai1(peta.L1_MOTOR, 'LANTAI 1: 6 V, TB6612, motor (Bagian 2)'), 'Dilihat dari atas, posisi barang sama persis dengan di robot (gambar tata letak Bagian 6). Nomor di gambar = nomor di tabel dan di langkah.' + ' Kabel abu-abu = kabel motor (25, 26) dan kabel jumper sinyal (30-32).') + peta.tabel(peta.kabel_l1(peta.L1_MOTOR)),
        ok_='Empat motor terpasang kuat, dan ada dua pasang kabel: satu untuk sisi kiri, satu untuk sisi kanan.',
        learn='Motor TT dirancang 3-6 V. Dua motor satu sisi selalu bergerak bersama (robot belok dengan memutar sisi kiri dan kanan berbeda arah), jadi cukup satu kanal driver per sisi.')

    tahap += stage(S, 3, 'TB6612FNG dan tes motor', ['TB6612FNG', '9 kabel F-F', 'Kabel 22 AWG hijau dan hitam', '2 potongan jumper betina 3 cm', 'Kapasitor 470 µF', 'WAGO 6V dan WAGO −6 dari Bagian 1'], [
        'Pin header TB6612-mu sudah terpasang, jadi tidak perlu menyolder. Balik papan: <b>nama pin tercetak di bawah</b>. Di modulmu urutannya sisi kiri PWMA, <b>AIN2, AIN1</b>, STBY, BIN1, BIN2, PWMB, GND dan sisi kanan VM, VCC, GND, AO1, AO2, BO2, BO1, GND. Perhatikan AIN2 ada di atas AIN1: cocokkan nama, jangan menebak dari posisi.',
        f'Kabel F-F dari TB6612 ke pin S papan 30P: {pin("PWMA")}→D25, {pin("AIN1")}→D26, {pin("AIN2")}→D27, {pin("PWMB")}→D13, {pin("BIN1")}→D32, {pin("BIN2")}→D33.',
        f'{pin("STBY")} dan {pin("VCC")} ke dua pin <b>3V</b> di blok daya. {pin("GND")} (yang di sisi pin logika) ke pin G mana saja.',
        f'Saklar baterai OFF. Potong hijau 22 AWG <b>{KB.pn("vm")}</b> dan hitam <b>{KB.pn("vmg")}</b>. Solder satu ujung masing-masing ke potongan jumper betina 3 cm, heat shrink. Hijau: WAGO 6V → {pin("VM")}. Hitam: WAGO −6 → {pin("GND")} di sebelah VM.',
        'Kapasitor 470 µF: <b>kaki panjang (+) ke WAGO 6V</b>, sisi bergaris (−) ke WAGO −6, di lubang yang masih kosong. Terbalik bisa meletus.',
        f'Kabel motor kiri (ujung jumper betina) ke {pin("AO1")} dan {pin("AO2")}, motor kanan ke {pin("BO1")} dan {pin("BO2")}.',
        '<b>Angkat robot</b>: taruh chassis di atas kotak atau gelas supaya keempat roda menggantung.',
        'Colok USB ke DevKit (mode meja), lalu saklar baterai ON. Upload kode tes, buka Serial Monitor 115200.',
    ], figs=f'<div class="photos">{photo(IMG + "tb6612_label.jpg", "TB6612FNG dilihat dari bawah dengan nama pin", "Bawah TB6612 milikmu: nama pin.")}{photo(IMG + "tb6612.jpg", "TB6612FNG dilihat dari atas", "Atas: chip dan kapasitor.")}</div>'
        + fig(diag.conn('TB6612FNG', '#b3261e', ['PWMA', 'AIN1', 'AIN2', 'PWMB', 'BIN1', 'BIN2', 'STBY', 'VCC', 'GND'],
                          'Papan 30P', '#151a18', ['D25 (S)', 'D26 (S)', 'D27 (S)', 'D13 (S)', 'D32 (S)', 'D33 (S)', ('#grup', 'blok daya'), '3V', '3V', 'GND'],
                          [(0, 0, 'kuning'), (1, 1, 'hijau'), (2, 2, 'biru'), (3, 3, 'kuning'), (4, 4, 'hijau'), (5, 5, 'biru'), (6, 7, '3v3'), (7, 8, '3v3'), (8, 9, 'gnd')],
                          label='Sambungan sinyal TB6612 ke papan 30P')) + '<p class="cap">Sisi daya TB6612 (kabel 17, 18) dan kabel motor (25, 26): peta lantai 1 di Tahap 2.</p>',
        code=kode_tes('tes_motor'),
        ok_='Serial Monitor menulis KIRI maju, KIRI mundur, KANAN maju, KANAN mundur, dan roda yang sesuai berputar ke arah itu. "Maju" = roda berputar seperti robot berjalan ke depan.',
        learn='PWMA mengatur kecepatan (0-255) dengan menyalakan-mematikan sangat cepat (20.000 kali per detik). AIN1 dan AIN2 mengatur arah: satu HIGH satu LOW = berputar, keduanya sama = berhenti.',
        gagal=['Satu sisi berputar terbalik: tukar dua kabel motor sisi itu di AO1/AO2 (atau BO1/BO2). Atau nanti ubah MOTOR_KIRI_TERBALIK di kode badan.',
               'Satu roda di sisi yang sama berputar berlawanan dengan roda lainnya: tukar merah-hitam motor itu saja.',
               'Tidak ada roda yang berputar: STBY belum ke 3V, VM belum dapat 6 V, atau GND DevKit tidak tersambung ke GND TB6612.',
               'Roda berdengung tapi tidak berputar: kecepatan 150 terlalu kecil untuk motormu, atau XL4015 kurang dari 6 V.',
               'DevKit restart saat motor mulai: noise motor. Pasang kapasitor 100 nF di motor dan cek GND bersama.'])

    tahap += stage(S, 4, 'Kepala pan-tilt (servo MG90S)', ['Pan-tilt dengan 2 MG90S', 'Kabel M-F', 'Kabel 22 AWG hijau dan hitam', 'Kapasitor 470 µF', 'Heat shrink', 'WAGO 6V dan WAGO −6'], [
        'Saklar OFF. Kabel servo: <b>coklat = GND</b>, <b>merah = daya</b>, <b>oranye = sinyal</b>.',
        f'Servo bawah (pan, menoleh kiri-kanan): oranye ke pin S baris {pin("D23")}. Servo atas (tilt, mengangguk): oranye ke pin S baris {pin("D4")}. Pakai kabel M-F: ujung jantan masuk ke <b>lubang oranye</b> colokan servo (lubang paling ujung), ujung betina ke pin S.',
        f'Daya servo dari WAGO 6V/−6 ke kepala di lantai 3: potong hijau 22 AWG <b>{KB.pn("servo+")}</b> dan hitam <b>26 AWG</b> (kabel UL1007-mu) <b>{KB.pn("servo-")}</b>. Arus kedua servo paling besar sekitar 1,5 A, jadi 26 AWG cukup untuk GND-nya, dan hitam 22 AWG tidak kurang.',
        'Potong kabel <b>merah dan coklat</b> tiap servo 10 cm dari badan servo. Kabel <b>oranye</b> dan colokannya jangan dipotong. Colokan servo tetap satu blok 3 lubang: sisa merah-coklat di sisi colokan <b>tetap menempel di colokan</b>, cukup diisolasi selotip di ujungnya. Yang dicolok nanti hanya lubang oranye; lubang merah dan coklat dibiarkan kosong.',
        'Masukkan heat shrink dulu. Solder kedua merah servo ke satu ujung kabel hijau, kedua coklat ke satu ujung kabel hitam. Di sambungan yang sama, solder kapasitor 470 µF kedua: kaki panjang ke merah-hijau, sisi bergaris ke coklat-hitam. Tutup heat shrink.',
        'Ujung lain kabel hijau → WAGO 6V, hitam → WAGO −6. Kabel 26 AWG sedikit lebih kecil dari ukuran terkecil yang dijepit kuat oleh WAGO, jadi ujung hitamnya dikupas 22 mm lalu dilipat dua (11 mm berlapis dua) sebelum dimasukkan. Tarik pelan: tidak boleh lepas.',
        '<b>Jangan</b> memberi daya servo dari pin V atau 5V papan ekspansi. Saat bergerak, servo menarik arus besar dan bisa membuat DevKit restart.',
        '<b>Cek GND bersama sebelum menyalakan.</b> Saklar OFF dan USB dicabut. Multimeter mode bip (continuity): satu probe di pin <b>G papan ekspansi</b> (GND DevKit), probe lain di lubang <b>WAGO −6</b>. <b>Harus bunyi.</b> Kalau tidak bunyi, pasang satu kabel dari pin G papan ekspansi ke WAGO −6. Tanpa GND bersama, sinyal servo tidak punya acuan: servo bergerak acak atau bergetar, tilt bisa diam sama sekali, dan menyentuh logam di jalur daya bisa membuat servo bergerak sendiri.',
        'Lepas sekrup lengan servo dulu. Upload kode tes. Setelah servo diam di 90°, pasang lengan sehingga kepala lurus ke depan dan tegak, baru kencangkan sekrup.',
    ], figs=photo(IMG + 'pantilt.jpg', 'Pan-tilt dengan dua servo MG90S yang sudah dirakit', 'Pan-tilt rakitanmu. Servo bawah = pan, servo atas = tilt.', 420) + fig(diag.servo_kapasitor(), 'Sambungan daya kepala: dua kabel merah servo jadi satu ke hijau, dua coklat jadi satu ke hitam, kapasitor 470 µF melintang di antara dua sambungan (kaki panjang + ke sisi merah). Kabel oranye tidak dipotong dan colokan servo tetap satu blok 3 lubang.'),
        code=kode_tes('tes_servo'),
        ok_='Kepala menoleh ke kiri, tengah, kanan, lalu mengangguk. Ketik <code>90 90</code> di Serial Monitor: kepala lurus ke depan.',
        learn='Servo membaca lebar pulsa 50 kali per detik: 0,6 ms = 0°, 1,5 ms = 90°, 2,4 ms = 180°. Sudut tilt dibatasi 40-150° di kode supaya kepala tidak membentur bracket.',
        gagal=['Servo bergerak acak, satu servo diam, atau bergerak sendiri saat kabel daya disentuh: GND servo belum bersambung ke GND DevKit. Ulangi tes bip di langkah GND bersama.', 'Servo bergetar atau berdengung terus: daya kurang. Pastikan merah ke 6 V (bukan 3,3 V) dan kapasitor terpasang.', 'Servo bergetar di ujung gerak saja: lengan menabrak bracket. Kecilkan batas sudut di kode dan jangan biarkan servo menahan di ujung.', 'Kepala menabrak bracket di sudut tertentu: catat sudutnya, nanti ubah TILT_MIN dan TILT_MAX di kode badan.'])

    tahap += stage(S, 5, 'Dua sensor jarak HC-SR04', ['2 HC-SR04', 'Breadboard kecil', '2 resistor 1 kΩ', '2 resistor 2 kΩ', 'Kabel F-F, F-M, M-M'], [
        'Saklar OFF, USB dicabut. Siapkan rel breadboard: pin <b>3V</b> blok daya → garis <span class="r">merah</span> atas, pin <b>G</b> → garis <span class="bl">biru</span> atas. Dua kabel di sisi kanan menyambung rel atas ke rel bawah (biru ke biru, merah ke merah).',
        'Pin <b>5V</b> blok daya → lubang <span class="pin">a30</span> (kolom 30 jadi titik 5 V untuk kedua sensor).',
        'Pembagi depan: resistor <b>1 kΩ</b> (coklat-hitam-hitam-coklat-coklat) di <span class="pin">b3</span>–<span class="pin">b7</span>. Resistor <b>2 kΩ</b> (merah-hitam-hitam-coklat-coklat) dari <span class="pin">c7</span> ke garis biru atas.',
        f'Kabel <span class="pin">a7</span> → pin S baris {pin("D19")}.',
        'Pembagi belakang: 1 kΩ di <span class="pin">b12</span>–<span class="pin">b16</span>, 2 kΩ dari <span class="pin">c16</span> ke garis biru. Kabel <span class="pin">a16</span> → pin S baris ' + pin('D35') + '.',
        f'Sensor depan (kabel F-F/F-M): {pin("VCC")} → <span class="pin">b30</span>, {pin("GND")} → pin G mana saja, {pin("Trig")} → S baris {pin("D18")}, {pin("Echo")} → <span class="pin">a3</span>.',
        f'Sensor belakang: {pin("VCC")} → <span class="pin">c30</span>, {pin("GND")} → G, {pin("Trig")} → S baris {pin("D14")}, {pin("Echo")} → <span class="pin">a12</span>.',
        'Colok USB, upload kode tes. Gerakkan tangan di depan tiap sensor.',
        'Ini susunan meja. Di robot, sensor belakang ada di lantai 1 ujung belakang dan kabelnya diatur ulang di Bagian 6 (TRIG lewat breadboard karena jauh dari papan 30P).',
    ], figs=photo(IMG + 'hcsr04.jpg', 'Dua sensor HC-SR04', 'Dua HC-SR04: satu untuk depan, satu untuk belakang.', 380) + bb(5, 'Breadboard badan tahap 5: rel daya dan dua pembagi ECHO') + fig(diag.pembagi_echo()),
        code=kode_tes('tes_jarak'),
        ok_='Angka depan dan belakang berubah saat tanganmu mendekat. Coba ke tembok: angkanya mendekati jarak yang diukur dengan penggaris (selisih 1-2 cm wajar). -1 berarti tidak ada pantulan (terlalu jauh, atau sensor tidak tersambung).',
        learn='Sensor mengirim bunyi ultrasonik, lalu pin ECHO menyala selama bunyi pergi-pulang. Suara menempuh sekitar 1 cm per 29 µs, pergi-pulang 58 µs per cm. Dua sensor dibaca bergantian supaya gema yang satu tidak terbaca yang lain.',
        gagal=['Selalu -1: VCC belum dapat 5 V (cek a30), atau TRIG/ECHO tertukar.', 'Angka melompat-lompat: permukaan miring atau lembut (kain, sofa) memantulkan bunyi buruk. Itu sifat sensor ini.',
               'Selalu 0 atau angka kecil tetap: resistor 2 kΩ belum ke garis biru.'])

    cuaca = ('<details><summary>Opsional: sensor cuaca (BME280 atau DHT11)</summary><div>'
             '<p>Lewati bagian ini kalau tidak memakai sensor cuaca. Robot tetap jalan tanpa sensor cuaca; nilai suhu dan kelembapan hanya kosong.</p>'
             + ul([f'Punya BME280: {pin("VIN")} → pin V baris D21, {pin("GND")} → G baris D21, {pin("SDA")} → S baris D21, {pin("SCL")} → S baris D22. Satu bus I2C bisa dipakai banyak sensor selama alamatnya beda.',
                   'Atau DHT11 dari kit stasiun cuaca di breadboard. Kaki DHT11 (sisi berlubang menghadapmu) ke <span class="pin">f10</span>–<span class="pin">f13</span>. Kabel <span class="pin">j10</span> → rel merah bawah, <span class="pin">j13</span> → rel biru bawah, resistor 10 kΩ dari <span class="pin">j11</span> ke rel merah bawah, kabel <span class="pin">h11</span> → S baris ' + pin('D5') + '.',
                   'Uji dengan kode di bawah: selain MPU6050, ia membaca BME280 (alamat 0x76 atau 0x77) atau DHT11 di GPIO 5.',
                   'BME280 tidak terbaca padahal alamat ada: modulmu mungkin BMP280 (tanpa kelembapan). "nan" untuk DHT11: cek urutan kaki dan resistor 10 kΩ.'])
             + bb(6, 'Breadboard badan tahap 6 (opsional): DHT11 di f10 sampai f13') + kode_tes('tes_i2c') + '</div></details>')
    tahap += stage(S, 6, 'MPU6050 (sensor kemiringan)', ['MPU6050 (GY-521)', '4 kabel F-F'], [
        'Saklar OFF, USB dicabut. Solder pin header ke MPU6050 kalau belum. Tempel MPU6050 <b>rata</b> di chassis nanti (Bagian 6). Untuk sekarang taruh di meja, datar.',
        f'MPU6050 ke header I2C papan 30P (nomor 5 di foto Tahap 1) dengan 4 kabel F-F: {pin("VCC")}→VCC, {pin("GND")}→GND, {pin("SCL")}→D22, {pin("SDA")}→D21. Pin lain (XDA, XCL, AD0, INT) dibiarkan.',
        'Colok USB, upload kode tes di bawah (tidak butuh library tambahan), buka Serial Monitor 115200.',
        'Pastikan pencarian menemukan <code>0x68</code>. Dengan MPU6050 datar, pitch dan roll mendekati 0 dan aks z sekitar 1,00 g.',
        'Miringkan ke depan-belakang (pitch) dan ke kiri-kanan (roll). Lewat 40° muncul tulisan MIRING: di kode badan, itu batas robot berhenti.',
    ], figs=photo(IMG + 'mpu6050.jpg', 'MPU6050 GY-521', 'MPU6050 (GY-521). Nama pin tercetak di sebelah lubang.', 380),
        code=kode_tes('tes_mpu'), after=cuaca,
        ok_='Pencarian menemukan <code>0x68</code>. Saat datar pitch dan roll mendekati 0 dan aks z sekitar 1 g; saat dimiringkan angkanya ikut berubah. Gyro mendekati 0 saat diam dan melonjak saat diputar.',
        learn='MPU6050 mengukur percepatan, termasuk gravitasi. Dari arah gravitasi, kode menghitung seberapa miring robot. Badan memakai ini untuk berhenti kalau robot terbalik atau terangkat miring.',
        gagal=['Tidak ada alamat ditemukan: SDA dan SCL tertukar, VCC atau GND belum tersambung, atau kabel F-F longgar.',
               'Ditemukan 0x69, bukan 0x68: pin AD0 tersambung ke VCC. Biarkan AD0 tidak tersambung.',
               'WHO_AM_I bukan 0x68 (misalnya 0x70 atau 0x98): modul tiruan. Selama pitch dan roll masuk akal, tetap bisa dipakai.',
               'Angka diam di nilai yang sama atau semuanya 0: modul belum bangun atau rusak. Cabut-colok USB, cek solderan pin header.'])

    tahap += stage(S, 7, 'Pengukur baterai dan tombol STOP', ['Resistor 20 kΩ dan 10 kΩ', 'Resistor 10 kΩ untuk tombol', 'Push button', 'Kabel M-M dan M-F'], [
        'Saklar OFF. Rel atas breadboard dari Tahap 5 harus masih terpasang: pin <b>3V</b> → garis merah atas, pin <b>G</b> → garis biru atas, dan rel atas tersambung ke rel bawah. Ukur merah atas ke biru atas dengan USB terpasang: harus sekitar 3,3 V. Resistor <b>20 kΩ</b> (merah-hitam-hitam-merah-coklat) di <span class="pin">b20</span>–<span class="pin">b24</span>. Resistor <b>10 kΩ</b> (coklat-hitam-hitam-merah-coklat) dari <span class="pin">c24</span> ke garis biru atas.',
        f'Kabel <span class="pin">a24</span> → S baris {pin("D34")}.',
        f'Potong kabel 26 AWG merah <b>{KB.pn("bat")}</b>, beri heat shrink <b>biru</b> 2 mm di kedua ujung sebagai tanda kabel ukur baterai: satu ujung dijepit di terminal <b>IN+ LM2596 #1</b> bersama kabel merahnya (rel plus baterai setelah saklar), ujung lain ke <span class="pin">a20</span>. <b>Bukan</b> dari P+ sebelum sekring.',
        'GND bersama: pembagi ini mengukur tegangan terhadap GND DevKit, jadi GND baterai harus disambung ke GND DevKit. Saklar OFF, sambung <b>WAGO −</b> ke pin <b>G</b> papan ekspansi dengan satu kabel (ujung kabel dikupas 11 mm untuk masuk ke WAGO, bukan pin jumper biasa). Cek mode bunyi: WAGO − ke pin G harus berbunyi.',
        'Tombol 6 mm menyeberangi parit tengah: kaki di <span class="pin">e27</span>, <span class="pin">e29</span>, <span class="pin">f27</span>, <span class="pin">f29</span>. Yang dipakai dua kaki <b>diagonal</b>: e27 dan f29.',
        'Resistor 10 kΩ dari <span class="pin">a27</span> ke garis merah atas (3,3 V). Kabel <span class="pin">c27</span> → S baris ' + pin('D39') + ' (di papan tertulis VN). Kabel <span class="pin">j29</span> → garis biru bawah.',
        'Colok USB, saklar baterai ON, upload kode tes. Ukur juga tegangan baterai dengan multimeter.',
        'Kalibrasi: kalau multimeter membaca <b>Vm</b> dan Serial Monitor <b>Vk</b>, faktor baru = 3,0 × Vm ÷ Vk. Contoh: 3,0 × 8,10 ÷ 7,95 = 3,06. Catat angka ini untuk <code>BATERAI_FAKTOR</code> di kode badan.',
    ], figs=bb(7, 'Breadboard badan tahap 7: pembagi baterai dan tombol STOP') + fig(diag.pembagi_baterai()),
        code=kode_tes('tes_baterai'),
        ok_='Tegangan baterai di Serial Monitor sama dengan multimeter (selisih di bawah 0,1 V setelah kalibrasi), dan tulisan berubah ke DITEKAN saat tombol ditekan.',
        learn='GPIO 39 hanya bisa input dan tidak punya resistor pull-up di dalam chip, jadi kita pasang 10 kΩ ke 3,3 V. Tanpa ditekan pin membaca HIGH; ditekan, pin tersambung ke GND dan membaca LOW. Untuk robot jadi, tombol 6 mm diganti tombol 12 mm di sayap tengah kiri lantai 3, disambung dua kabel ke kolom 27 dan 29 (panjangnya di Bagian 6).',
        gagal=['Baterai terbaca 0: kabel BAT+ belum masuk a20, atau saklar OFF.', 'Baterai terbaca 0 padahal multimeter di WAGO membaca 8 V, dan a20 ke GND breadboard juga 0 V: GND baterai belum disambung ke GND DevKit (langkah GND bersama).', 'Angka baterai melonjak ke sekitar 3100 mV atau 140 mV sendiri, dan tombol berganti DITEKAN/lepas tanpa disentuh: rel atas breadboard tidak mendapat 3,3 V (kabel pin 3V dan G dari Tahap 5 belum dipasang), sehingga pull-up tombol tidak bekerja dan input melayang.', 'Tombol selalu DITEKAN: kaki tombol yang dipakai bukan diagonal. Putar tombol 90 derajat.'])

    tahap += stage(S, 8, 'Upload kode badan dan uji keselamatan', ['Semua rangkaian Tahap 3-7', 'Robot terangkat'], [
        'Buka <a href="../kode/robot_badan/robot_badan.ino" download>robot_badan.ino</a>. Isi <code>BATERAI_FAKTOR</code> dari Tahap 7. Kalau di Tahap 3 satu sisi terbalik dan kamu tidak menukar kabel, ubah <code>MOTOR_KIRI_TERBALIK</code> atau <code>MOTOR_KANAN_TERBALIK</code> ke <code>true</code>.',
        'Upload ke DevKit (board ESP32 Dev Module). Buka Serial Monitor 115200, pilih akhir baris <b>Newline</b> di pojok kanan bawah.',
        'Pastikan robot terangkat dan saklar baterai ON. Ketik <code>M 150 150 1000</code> lalu Enter: semua roda maju 1 detik lalu berhenti sendiri.',
        '<b>Boleh ditunda:</b> upload kode di atas wajib sebelum Bagian 3. Uji keselamatan di bawah (halangan, tombol STOP, batas waktu) boleh kamu jalankan nanti di Bagian 6 Tahap 7 dengan robot terangkat, kalau rangkaian meja sudah dibongkar. Jangan jalankan robot di lantai sebelum ketiganya terbukti jalan.',
        'Ketik <code>M 150 -150 500</code>: robot (kalau di lantai) akan berputar ke kanan. Ketik <code>P 45 90</code>: kepala menoleh.',
        'Uji halangan: taruh tanganmu 10 cm di depan sensor depan, ketik <code>M 150 150 1000</code>. Harus muncul <code>E DITOLAK</code> dan roda diam. Mundur (<code>M -150 -150 1000</code>) tetap boleh.',
        'Uji tombol STOP: tekan sekali, muncul <code>E STOP_DIKUNCI</code> dan LED biru DevKit berkedip cepat. Semua perintah gerak ditolak. Tekan lagi untuk melepas.',
        'Uji batas waktu: ketik <code>M 150 150 3000</code> lalu jangan ketik apa-apa. Roda berhenti sendiri setelah 3 detik. Itulah yang terjadi kalau otak macet atau kabel UART lepas: perintah terakhir habis dan robot diam.',
    ], code=kode_teks(open(KODE + 'robot_badan/robot_badan.ino', encoding='utf-8').read().split('// ================== VARIABEL')[0], 'robot_badan.ino (bagian awal: penjelasan, pengaturan, pin)')
        + f'<p class="cap">Kode lengkap: <a href="../kode/robot_badan/robot_badan.ino" download>unduh robot_badan.ino</a>.</p>',
        ok_='Badan menuruti perintah dan menolak gerakan berbahaya. Di Serial Monitor tiap detik muncul baris data seperti <code>D 45 120 7.92 29.5 70 1 -2 0</code>.',
        learn='Baris D berisi jarak depan, jarak belakang, baterai, suhu, kelembapan, pitch, roll, dan flag. Otak membaca baris yang sama lewat UART tiap 250 ms. Semua gerakan berhenti sendiri setelah waktunya habis (maks 3 detik), jadi kalau otak macet atau kabel UART lepas, robot tidak lari.')

    isi += section('rakit', 'Langkah', 'Rakit badan', '<div class="parts" style="gap:16px">' + tahap + '</div>',
                   'Tahap 3 dan 4 butuh rel 6 V dari Bagian 1. Saat USB tercolok ke DevKit, kabel 5 V dari LM2596 ke papan 30P dilepas (mode meja).')

    isi += section('daftar', 'Cek ulang', 'Daftar kabel breadboard badan', table(
        ['Komponen / kabel', 'Lubang', 'Ke'],
        [['Rel merah atas', 'kolom 1', 'pin 3V blok daya'], ['Rel biru atas', 'kolom 1', 'pin G'], ['Rel bawah', 'kanan', 'disambung ke rel atas (merah-merah, biru-biru)'],
         ['Titik 5 V', 'a30, b30, c30', 'pin 5V blok daya, VCC kedua HC-SR04'],
         ['1 kΩ depan', 'b3–b7', ''], ['2 kΩ depan', 'c7 → rel biru', ''], ['ECHO depan', 'a3', 'HC-SR04 depan'], ['ke D19', 'a7', 'pin S D19'],
         ['1 kΩ belakang', 'b12–b16', ''], ['2 kΩ belakang', 'c16 → rel biru', ''], ['ECHO belakang', 'a12', 'HC-SR04 belakang'], ['ke D35', 'a16', 'pin S D35'],
         ['DHT11 (opsional)', 'f10–f13', 'j10 → merah bawah, j13 → biru bawah, 10 kΩ j11 → merah bawah, h11 → D5'],
         ['20 kΩ', 'b20–b24', ''], ['10 kΩ', 'c24 → rel biru', ''], ['BAT+', 'a20', 'rel plus baterai setelah saklar'], ['ke D34', 'a24', 'pin S D34'],
         ['Tombol', 'e27, e29, f27, f29', 'dipakai e27 dan f29'], ['10 kΩ pull-up', 'a27 → rel merah', ''], ['ke D39', 'c27', 'pin S D39 (VN)'], ['GND tombol', 'j29', 'rel biru bawah']]))

    lead = 'Badan adalah bagian yang menyentuh dunia nyata: roda, kepala, dan sensor. Kerjakan dengan robot terangkat, sampai Tahap 8 lulus.'
    return S, 'Badan robot', lead, isi, 'Merakit badan robot 4WD: chassis, TB6612, servo pan-tilt, HC-SR04, MPU6050, sensor cuaca, tombol STOP dan kode badan ESP32.'
