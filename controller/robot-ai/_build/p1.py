from build import *
import kabel as KB

S = '1-daya'
IMG = '../img/'


def halaman():
    isi = ''
    isi += section('aturan', 'Sebelum mulai', 'Empat aturan daya', '''
<div class="rules">
  <div class="rule"><b>Ukur dulu, baru sambung</b><p>Setiap konverter disetel dan diukur tanpa beban. Konverter baru sering keluar dengan tegangan sama dengan masukan (8 V), dan itu cukup untuk merusak ESP32.</p></div>
  <div class="rule"><b>Merah +7,4 V, hijau +6 V, hitam GND</b><p>Kabel daya 22 AWG memakai tiga warna ini saja. Kabel 5 V memakai 26 AWG merah-hitam. Modul konverter dan BMS tidak punya pelindung polaritas terbalik.</p></div>
  <div class="rule"><b>Saklar OFF saat merakit</b><p>Ubah kabel hanya saat saklar utama mati. Kalau ada yang panas, berasap, atau berbau, matikan saklar dan cabut sel.</p></div>
  <div class="rule"><b>Baterai bukan mainan</b><p>Dua sel 18650 bisa mengeluarkan puluhan ampere kalau dihubung singkat. Jangan biarkan ujung kabel plus dan minus bersentuhan.</p></div>
</div>''')

    isi += section('alat', 'Dasar', 'Dua keterampilan yang dipakai terus', f'''
<div class="panel">
  <h3>Memakai multimeter</h3>
  <div class="two">
    {photo(IMG + 'multimeter.jpg', 'Multimeter digital ISKU', 'Multimeter-mu. Kabel hitam selalu di soket COM.')}
    <div>{ul([
      '<b>Tegangan DC</b> (simbol V dengan garis lurus): kabel merah di soket <b>VΩmA</b>, putar ke angka <b>20</b> di area V⎓. Probe hitam ke minus, merah ke plus. Angka negatif berarti probe terbalik, tidak merusak.',
      '<b>Kontinuitas</b> (simbol nada/dioda): saat daya MATI, tempelkan dua probe ke dua titik. Bunyi = tersambung langsung.',
      '<b>Hambatan Ω</b>: untuk memastikan nilai resistor, misalnya 2k terbaca sekitar 1,98-2,02 kΩ.',
      '<b>Jangan</b> mengukur tegangan dengan kabel merah di soket <b>10A</b>. Soket itu nyaris 0 Ω, jadi menempelkannya ke baterai sama dengan korsleting. Soket 10A di multimeter ini juga tanpa sekring.',
    ])}</div>
  </div>
</div>
<div class="panel">
  <h3>Menyolder</h3>
  <div class="two">
    {photo(IMG + 'solder.jpg', 'Solder, timah 0,8 mm, flux, dan kawat penyedot timah', 'Solder, timah 0,8 mm, flux RMA-223, dan kawat penyedot timah milikmu.')}
    <div>{ul([
      'Panaskan solder 2-3 menit. Bersihkan ujungnya, lalu lapisi sedikit timah supaya mengkilap.',
      'Oleskan sedikit flux di titik yang akan disolder. Flux membuat timah mengalir rata.',
      'Tempelkan ujung solder ke <b>pad dan kaki sekaligus</b> 1-2 detik, lalu sentuhkan timah ke pad (bukan ke ujung solder). Angkat timah dulu, baru solder.',
      'Hasil yang baik berbentuk kerucut kecil mengkilap. Kusam atau berbentuk bola = "solder dingin", panaskan ulang.',
      'Timah menyambung dua pin? Tempelkan kawat penyedot di atasnya, panaskan, timah terserap.',
      'Asap flux jangan dihirup. Kerjakan dekat jendela atau kipas.',
    ])}</div>
  </div>
  {info('Trik pin header lurus', 'Tancapkan pin header ke breadboard dulu, letakkan modul di atasnya, lalu solder. Breadboard menahan pin tetap tegak lurus. Solder satu pin dulu, cek modul rata, baru lanjutkan sisanya.')}
  {info('Menyambung dua kabel', 'Kupas 5-8 mm, masukkan heat shrink lebih dulu, pilin kedua ujung, solder, lalu geser heat shrink menutupi sambungan dan panaskan. Belum punya heat shrink? Lilit isolasi listrik rapat-rapat.')}
</div>''')

    tahap = ''
    tahap += stage(S, 1, 'Cek kedua sel 18650', ['2 sel RakkiPanda 18650', 'Multimeter'], [
        'Lihat fisik kedua sel: plastik pembungkus tidak sobek, tidak penyok, tidak menggembung. Sel rusak jangan dipakai.',
        f'Multimeter ke V⎓ 20. Ukur sel 1: probe merah ke ujung {pin("+")} (yang menonjol), hitam ke ujung {pin("−")} (datar). Catat.',
        'Ukur sel 2 dengan cara sama. Catat.',
        'Kalau selisih kedua angka lebih dari 0,1 V, isi kedua sel sampai penuh di charger iMFG dulu (lihat <a href="#isi">cara mengisi</a>), lalu ukur lagi.',
    ], figs=photo(IMG + 'baterai.jpg', 'Dua sel 18650 RakkiPanda 3000 mAh', 'Dua sel 18650 milikmu. Keduanya harus diperlakukan sebagai satu pasangan.', 420),
        ok_='Kedua sel di antara 3,0 dan 4,2 V, dan selisihnya kurang dari 0,1 V. Sel yang baru penuh terbaca sekitar 4,15-4,2 V.',
        learn='Dua sel dirangkai seri, jadi arus yang sama lewat keduanya. Sel yang lebih kosong akan habis duluan dan BMS memutus daya lebih cepat. Mulai dari dua sel yang setara membuat keduanya berumur panjang.',
        gagal=['Terbaca di bawah 2,5 V: sel sudah terlalu kosong. Jangan dipakai di robot.', 'Terbaca 0 V: cek probe di soket yang benar (COM dan VΩmA) dan dial di V⎓, bukan V~.'])

    tahap += stage(S, 2, 'Potong kabel dan solder ke BMS', ['BMS 2S 20A', 'Kabel 22 AWG merah dan hitam', 'Kabel tipis 26 AWG (oranye atau warna lain)', 'Gunting kabel + penggaris', 'Solder, timah, flux', 'Multimeter'], [
        'Semua panjang kabel di bagian ini sudah dihitung dari posisi akhir barang di robot (tata letak dan tabel kabel di <a href="../6-rakit/">Bagian 6</a>), jadi langsung potong sepanjang yang tertulis. Angkanya sudah termasuk 3 cm untuk kupasan dan kelonggaran. Kupas 5 mm untuk ujung yang disolder, 11 mm untuk ujung yang masuk WAGO.',
        'Sebelum menyolder, tes dulu: multimeter ke mode kontinuitas. Tempel probe ke pad <b>8.4V</b> dan pad <b>⊕</b>: harus bunyi. Tempel ke pad <b>0V</b> dan <b>⊖</b>: harus <b>tidak</b> bunyi. Hasilnya berbeda? Berhenti dulu dan foto kedua sisi BMS.',
        f'Potong merah 22 AWG <b>{KB.p("b8")}</b>. Solder ke pad <b>8.4V</b> (nanti ke + holder 2). Beri label selotip "8.4V".',
        f'Potong hitam 22 AWG <b>{KB.p("b0")}</b>. Solder ke pad <b>0V</b> (nanti ke − holder 1). Label "0V".',
        f'Potong kabel tipis <b>{KB.p("b42")}</b>. Solder ke pad <b>4.2V</b> (nanti ke titik tengah).',
        f'Potong hitam 22 AWG <b>{KB.p("pminw")}</b>. Solder ke pad <b>⊖</b> (keluaran −). Label "P−". Ujung lainnya masuk WAGO − di Tahap 4.',
        'Pad <b>⊕</b> (keluaran +) dibiarkan dulu: kabelnya dari dudukan sekring di Tahap 4.',
        'Pad lebar butuh panas lebih lama. Beri flux, tahan solder sampai timah meleleh rata, tapi jangan lebih dari sekitar 4 detik per pad supaya MOSFET di sebelahnya tidak kepanasan.',
    ], figs=photo(IMG + 'bms_label.jpg', 'BMS dengan label pad 8.4V, 0V, 4.2V, plus dan minus', 'Pad BMS-mu. Tulisan 充电/放电 di samping ⊕ ⊖ artinya "isi/keluar".'),
        ok_='Empat kabel (8.4V, 0V, 4.2V, P−) terpasang kuat (tarik pelan, tidak lepas), dan tidak ada timah yang menyambung dua pad.',
        learn='BMS memotong jalur minus (⊖) kalau sel terlalu kosong, terlalu penuh, atau arus terlalu besar. Pad 4.2V dipakai untuk mengukur dan menyeimbangkan tiap sel, jadi arusnya kecil dan kabel tipis cukup.')

    tahap += stage(S, 3, 'Rangkai dua holder dan pasang sel', ['2 holder 18650 1 slot', 'BMS yang sudah berkabel', 'Kabel 22 AWG merah', 'Heat shrink', 'Multimeter'], [
        '<b>Letakkan dulu seperti nanti di lantai 2</b>, supaya kabel yang sudah dipotong pas. Kiri dan kanan di panduan ini dilihat dari belakang robot, menghadap ke depan. Holder 1 paling belakang, ujung <b>pegas (−) di kiri</b>. Holder 2 di depannya <b>dibalik</b>: ujung pegas (−) di kanan. BMS di depan holder 2, pad 8.4V dan 0V di sisi kiri (lihat lantai 2 di gambar tata letak Bagian 6).',
        'Kabel bawaan holder tipis dan terlalu panjang. Potong keempatnya sehingga tersisa <b>2 cm</b> dari badan holder, lalu kupas 5 mm.',
        f'Potong merah 22 AWG <b>{KB.p("tengah")}</b> untuk <b>titik tengah</b>: satu ujung ke holder 1 kabel merah (+), ujung lain ke holder 2 kabel hitam (−). Keduanya ada di sisi kanan. Ikut sertakan kabel tipis dari pad 4.2V di sambungan holder 2 (−). Solder, lalu tutup heat shrink (lihat "Menyambung dua kabel" di atas). Sambungan antar-sel ini dilewati semua arus robot, jadi wajib 22 AWG.',
        'Holder masih kosong. Pasang kabel BMS dengan urutan: <b>1.</b> kabel 0V BMS ke holder 1 kabel hitam (−, kiri), <b>2.</b> titik tengah sudah tersambung ke 4.2V, <b>3.</b> kabel 8.4V BMS ke holder 2 kabel merah (+, kiri). Solder dan heat shrink semua sambungan.',
        'Masukkan sel 1 ke holder 1, lalu sel 2 ke holder 2. Perhatikan tanda + di holder (ujung menonjol sel ke sisi +, sisi pegas ke −).',
        'Ukur: kabel 0V ke titik tengah = tegangan sel 1. Kabel 0V ke kabel 8.4V = jumlah kedua sel.',
        'Ukur keluaran: pad ⊕ (P+) ke kabel P−. Harus sama dengan jumlah kedua sel, sekitar 7,4-8,4 V.',
    ], figs=photo(IMG + 'holder.jpg', 'Dua holder 18650 satu slot dengan kabel merah dan hitam', 'Dua holder milikmu. Ujung pegas = − (kabel hitam), ujung pelat datar bertanda + = + (kabel merah).', 420)
        + fig(diag.bms(), 'Urutan menyambung: 0V dulu, lalu titik tengah, lalu 8.4V. Melepas dengan urutan terbalik.'),
        ok_='P+ ke P− terbaca 7,4-8,4 V. Selama ini pad ⊕ dan kabel P− jangan sampai bersentuhan.',
        gagal=['P+ ke P− terbaca 0 V padahal 0V ke 8.4V benar: proteksi BMS sedang terkunci (sering terjadi saat sel pertama kali dipasang). Lepas kedua sel, tunggu 10 detik, pasang lagi sel 1 lalu sel 2. Kalau tetap 0 V, dengan P+ dan P− tidak tersambung ke apa pun, sentuhkan sebentar seutas kabel antara pad ⊖ dan pad 0V untuk "membangunkan" BMS. Masih 0 V: cek kabel 0V dan 8.4V tidak tertukar.',
               'Tegangan total sekitar 0 V atau hanya satu sel: sambungan titik tengah lepas, atau satu sel terbalik. Cabut sel, cek tanda +.',
               'Ada yang panas saat sel dimasukkan: cabut sel segera. Biasanya ada korsleting atau sel terbalik.'])

    tahap += stage(S, 4, 'Pasang sekring, saklar utama, dan WAGO baterai', ['Dudukan sekring blade + sekring', 'Saklar rocker merah', 'Kabel 22 AWG merah', '2 WAGO 221-415 (5 lubang)', 'Selotip merah dan hitam', 'Multimeter'], [
        'Lihat angka di badan sekring. Sekring blade warna kuning kecoklatan biasanya <b>5 A</b>, dan itu pas untuk robot ini.',
        '<b>Keluarkan kedua sel dari holder</b> sebelum menyolder di BMS.',
        f'Dudukan sekringmu berbentuk lingkaran kabel merah. Potong lingkaran itu sehingga ujung 1 sepanjang <b>{KB.p("sek1")}</b> dan ujung 2 sepanjang <b>{KB.p("sek2")}</b>, diukur dari badan dudukan. Kalau lingkarannya lebih pendek, sambung dengan merah 22 AWG.',
        'Solder ujung 1 ke pad <b>⊕ (P+)</b> BMS. Label "P+".',
        'Solder ujung 2 ke salah satu kaki saklar rocker. Kalau saklarnya 3 kaki, pakai kaki tengah dan satu kaki ujung (tes dengan kontinuitas: bunyi saat ON, diam saat OFF).',
        f'Potong merah 22 AWG <b>{KB.p("sakw")}</b>, solder ke kaki saklar yang lain. Ini <b>rel plus baterai</b>. Kaki saklar yang disolder ditutup heat shrink.',
        'WAGO: tempel selotip merah di satu WAGO (<b>WAGO +</b>) dan hitam di satu lagi (<b>WAGO −</b>). Cara memasukkan kabel: angkat tuas oranye, masukkan ujung yang dikupas 11 mm sampai mentok, tutup tuas, tarik pelan untuk mengecek.',
        'Kabel rel plus dari saklar → WAGO +. Kabel P− dari BMS → WAGO −. Lubang lainnya untuk ketiga konverter di Tahap 5-7.',
        'Masukkan lagi sel 1 lalu sel 2. Saklar OFF, ukur WAGO + ke WAGO − (probe masuk lubang uji kecil di samping tuas): sekitar 0 V. Saklar ON: 7,4-8,4 V. Saklar OFF lagi.',
    ], figs=f'<div class="photos">{photo(IMG + "sekring.jpg", "Dudukan sekring blade dengan kabel merah", "Dudukan sekring dan sekring blade.")}{photo(IMG + "saklar.jpg", "Saklar rocker merah", "Saklar rocker. Cek tulisan arus maksimum di badannya.")}</div>',
        ok_='Saklar benar-benar memutus dan menyambung rel plus, sekring terpasang di antara BMS dan saklar, dan WAGO + / WAGO − terbaca 7,4-8,4 V saat saklar ON.',
        learn='Sekring dipasang sedekat mungkin dengan baterai, supaya kalau ada kabel yang terkelupas dan korsleting di mana pun setelahnya, sekring putus sebelum kabel terbakar.',
        gagal=['Belum punya WAGO: sambungan Y. Kupas tengah kabel rel plus 1 cm tanpa memotongnya, lilitkan ujung kabel konverter di situ, solder, tutup heat shrink (masukkan heat shrink sebelum menyolder). Panjang kabel tetap sama.'])

    lm_legend = ul([
        f'<b>1</b> {pin("IN+")} masukan plus (dari rel plus baterai). <b>2</b> {pin("IN−")} masukan minus (rel GND). Tulisan VIN+ dan VIN− ada di PCB.',
        f'<b>3</b> {pin("OUT+")} keluaran 5 V. <b>4</b> {pin("OUT−")} keluaran minus.',
        '<b>5</b> Trimpot biru: diputar dengan obeng minus kecil untuk mengatur tegangan keluaran. Perlu belasan putaran untuk perubahan besar.',
        '<b>6</b> Tombol: menukar layar antara tegangan masuk (LED IN menyala) dan keluar (LED OUT). <b>7</b> Layar voltmeter. Layar ini perkiraan; percayai multimeter.',
    ])
    tahap += stage(S, 5, 'Setel LM2596 #1 ke 5,0 V (rel logika)', ['LM2596 dengan voltmeter', 'Obeng minus kecil', 'Multimeter'], [
        f'Saklar OFF. Potong merah 22 AWG <b>{KB.p("lm1+")}</b> dan hitam <b>{KB.p("lm1-")}</b>. Merah: WAGO + → <b>IN+</b>. Hitam: WAGO − → <b>IN−</b>. Kencangkan sekrup terminal, tarik kabel pelan untuk mengecek.',
        '<b>Belum ada apa pun di OUT+ / OUT−.</b> Saklar ON.',
        'Tekan tombol sampai LED OUT menyala. Ukur juga OUT+ ke OUT− dengan multimeter.',
        'Putar trimpot pelan sambil melihat multimeter. Kalau angkanya naik, putar ke arah sebaliknya. Berhenti di <b>5,0 V</b> (boleh 4,95-5,1).',
        'Saklar OFF lalu ON lagi. Ukur ulang: tetap 5,0 V.',
        'Beri label selotip "5V LOGIKA" di modul ini.',
    ], figs=photo(IMG + 'lm2596_anot.jpg', 'LM2596 dengan nomor bagian') + lm_legend,
        ok_='Multimeter membaca 4,95-5,1 V di OUT+ ke OUT−, dan tetap sama setelah dimatikan-dinyalakan.',
        learn='LM2596 adalah konverter buck: memotong-motong tegangan baterai dengan cepat lalu menghaluskannya, jadi jauh lebih efisien dari regulator biasa. Keluarannya harus sedikit di atas kebutuhan ESP32 (masukan 5 V), tapi tidak lebih dari 5,5 V.',
        gagal=['Tegangan tidak berubah walau trimpot sudah diputar puluhan kali: putar ke arah sebaliknya. Trimpot ini tidak punya batas yang terasa.', 'Keluaran sama dengan masukan (sekitar 8 V): trimpot masih di posisi maksimum. Teruskan memutar ke arah yang menurunkan tegangan.'])

    xl_legend = ul([
        '<b>1</b> dan <b>2</b>: dua terminal sekrup. Tulisan IN dan OUT ada di bawah papan. Biasanya masukan di sisi kapasitor 100 µF 50 V (<b>5</b>), keluaran di sisi kapasitor 220 µF 35 V. <b>Balik papan dan baca tulisannya sebelum menyambung.</b>',
        '<b>3</b> dan <b>4</b>: dua trimpot. Satu mengatur tegangan (CV), satu membatasi arus (CC). Yang mana, kita cari di langkah 4.',
    ])
    tahap += stage(S, 6, 'Setel XL4015 ke 6,0 V (rel motor dan servo)', ['XL4015', 'Obeng minus kecil', 'Multimeter'], [
        'Balik papan XL4015 dan baca tulisan IN+, IN−, OUT+, OUT− di bawah terminal. Tandai dengan spidol di atas papan.',
        f'Saklar OFF. Potong merah 22 AWG <b>{KB.p("xl+")}</b> dan hitam <b>{KB.p("xl-")}</b> (lebih panjang karena XL4015 nanti di lantai 2, depan). Merah: WAGO + → IN+. Hitam: WAGO − → IN−. Belum ada beban di OUT.',
        'Saklar ON. Ukur OUT+ ke OUT−.',
        'Putar trimpot 3 beberapa putaran sambil melihat multimeter. Kalau tegangan berubah, itulah trimpot tegangan (CV). Kalau tidak berubah, kembalikan, lalu coba trimpot 4.',
        'Atur trimpot CV sampai <b>6,0 V</b> (5,9-6,1). Jangan lebih dari 6,2 V: motor TT dan servo MG90S dirancang untuk paling tinggi 6 V.',
        'Trimpot CC jangan diubah dulu. Beri label "6V MOTOR" di modul.',
        f'Saklar OFF. Siapkan dua WAGO lagi: selotip hijau (<b>WAGO 6V</b>) dan hitam bertulisan 6 (<b>WAGO −6</b>). Potong hijau 22 AWG <b>{KB.p("w6+")}</b>: OUT+ → WAGO 6V. Potong hitam <b>{KB.p("w6-")}</b>: OUT− → WAGO −6. Saklar ON, ukur WAGO 6V ke WAGO −6: tetap 5,9-6,1 V.',
    ], figs=photo(IMG + 'xl4015_anot.jpg', 'XL4015 dengan nomor bagian') + xl_legend,
        ok_='OUT+ ke OUT− terbaca 5,9-6,1 V tanpa beban.',
        gagal=['Nanti saat motor jalan tegangan turun jauh dan ada LED merah menyala di XL4015: batas arus (CC) terlalu rendah. Putar trimpot CC beberapa putaran searah jarum jam, lalu cek lagi.'])

    tahap += stage(S, 7, 'Setel LM2596 #2 ke 5,0 V (rel kamera)', ['LM2596 kedua (kalau sudah ada)', 'Multimeter'], [
        f'Saklar OFF. Potong merah 22 AWG <b>{KB.p("lm2+")}</b> dan hitam <b>{KB.p("lm2-")}</b>. Merah: WAGO + → IN+, hitam: WAGO − → IN−.',
        'Setel ke <b>5,0 V</b> tanpa beban seperti langkah 2-5 Tahap 5.',
        'Beri label "5V KAMERA".',
    ], ok_='Keluaran 5,0 V. Rel kamera terpisah membuat lonjakan arus WiFi kamera tidak mengganggu ESP32-S3.')

    tahap += stage(S, 8, 'Uji tiap rel dengan beban kecil', ['LED', 'Resistor 330 Ω (oranye-oranye-hitam-hitam-coklat)', 'Breadboard', 'Kabel jumper'], [
        'Saklar OFF. Di breadboard, pasang LED dan resistor 330 Ω seri: kaki panjang LED ke satu ujung resistor.',
        'Sambung ujung lain resistor ke OUT+ LM2596 #1, kaki pendek LED ke OUT−.',
        'Saklar ON: LED menyala. Ukur OUT lagi: tetap sekitar 5,0 V.',
        'Pindahkan ke WAGO 6V dan WAGO −6 (XL4015): LED tetap menyala, sedikit lebih terang.',
        'Saklar OFF. Lepas LED dan resistor.',
    ], ok_='LED menyala di kedua rel dan tegangan tidak berubah. Kamu sekarang punya sumber daya yang aman untuk ketiga board.',
        learn='Resistor 330 Ω membatasi arus LED sekitar (5 - 2) / 330 = 9 mA. Tanpa resistor, LED langsung putus.')

    isi += section('rakit', 'Langkah', 'Rakit sistem daya', '<div class="parts" style="gap:16px">' + tahap + '</div>',
                   'Kerjakan berurutan. Setiap tahap punya tanda berhasil sebelum lanjut ke tahap berikutnya.')

    isi += section('peta', 'Ringkasan', 'Peta jalur daya', fig(diag.daya(), 'Empat WAGO 221-415: WAGO + (masuk dari saklar, keluar ke 3 konverter, 1 cadangan), WAGO − (masuk dari P−, keluar ke 3 konverter), WAGO 6V dan WAGO −6 (dari XL4015 ke TB6612, kedua servo, kapasitor 470 µF). Kabel tipis pengukur baterai jangan ke WAGO, jepit di terminal IN+ LM2596 #1. Panjang tiap kabel ada di tabel Bagian 6.') + table(
        ['Titik ukur', 'Harus terbaca', 'Kalau tidak'],
        [['P+ ke P− BMS', '7,4 - 8,4 V', 'Tahap 3'], ['Rel plus ke GND, saklar ON', '7,4 - 8,4 V', 'Tahap 4'], ['OUT LM2596 #1', '4,95 - 5,1 V', 'Tahap 5'],
         ['OUT XL4015', '5,9 - 6,1 V', 'Tahap 6'], ['OUT LM2596 #2', '4,95 - 5,1 V', 'Tahap 7'], ['Rel plus, saklar OFF', '0 V', 'saklar salah kaki']]))

    isi += section('mode', 'Penting', 'Mode meja dan mode jalan', '''
<div class="two">
  <div class="panel"><h3>Mode meja (USB ke laptop)</h3><p>Dipakai saat upload kode dan membaca Serial Monitor. Board mendapat daya dari USB. Kabel <b class="r">5 V merah</b> dari LM2596 ke board <b>dilepas</b>, tapi kabel <b>GND tetap tersambung</b>. Motor dan servo tetap boleh dapat daya dari XL4015.</p></div>
  <div class="panel"><h3>Mode jalan (baterai)</h3><p>USB dicabut, kabel <b class="r">5 V merah</b> dipasang ke pin 5V papan ekspansi. Jangan pernah mencolok USB dan kabel 5 V dari LM2596 bersamaan: dua sumber 5 V yang sedikit berbeda saling melawan, dan arusnya bisa mengalir balik ke port USB laptop.</p></div>
</div>
<p class="cap">Supaya mudah, pakai kabel jumper betina khusus (misalnya warna merah) untuk 5 V ke tiap board. Itulah satu-satunya kabel yang dicabut-pasang saat pindah mode.</p>''')

    isi += section('isi', 'Perawatan', 'Cara mengisi baterai', f'''
<div class="two">
  {photo(IMG + 'charger.jpg', 'Charger 18650 iMFG dua slot', 'Charger iMFG 2 slot: masukan 5 V, keluaran 4,2 V 1 A per sel.')}
  <div>{ul([
    'Matikan saklar, keluarkan <b>kedua</b> sel dari holder.',
    'Masukkan ke charger dengan arah + sesuai tanda di slot. Colok charger ke adaptor 5 V 2 A milikmu.',
    'Tunggu sampai lampu menandakan penuh. Isi dekat dirimu, di permukaan yang tidak mudah terbakar, jangan ditinggal tidur.',
    'Ukur: tiap sel sekitar 4,15-4,2 V. Masukkan lagi ke holder dengan urutan sel 1 lalu sel 2.',
    'Selalu pakai dua sel yang sama sebagai pasangan. Kalau nanti punya sel ketiga, jadikan cadangan, jangan dicampur.',
    'Simpan lama (lebih dari sebulan) di sekitar 3,7-3,8 V per sel, di tempat sejuk.',
  ])}</div>
</div>''')

    lead = 'Bagian paling penting untuk keselamatan board-mu. Di akhir bagian ini kamu punya tiga sumber tegangan yang sudah diukur: 5 V untuk otak dan badan, 5 V untuk kamera, dan 6 V untuk motor dan servo.'
    return S, 'Sistem daya', lead, isi, 'Panduan merakit daya robot: dua sel 18650, BMS 2S, sekring, saklar, LM2596 dan XL4015, diukur dengan multimeter.'
