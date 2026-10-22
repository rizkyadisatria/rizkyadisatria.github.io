from build import *

S = '3-otak'
IMG = '../img/'


def halaman():
    isi = ''
    leg = ul([
        '<b>1</b> Blok <b>5V</b> (merah) dan GND (hitam). Di mode jalan, 5 V dari LM2596 #1 masuk ke sini. Saat USB tercolok, pin 5V papanmu hanya terbaca sekitar 1,5-2,9 V (sudah kamu ukur), jadi <b>jangan</b> memakainya sebagai sumber 5 V di meja.',
        '<b>2</b> Blok <b>3V3</b> dan GND: sumber 3,3 V untuk layar, mikrofon, dan speaker selama di meja.',
        '<b>3</b> dan <b>4</b>: tiga kolom untuk tiap GPIO. <b>Kuning = S (sinyal)</b>, merah = 3,3 V, hitam = GND. Nama GPIO tercetak di tepi: IO4, IO5, ... IO21.',
        '<b>5</b> Header kuning lurus di dekat soket: sambungan langsung ke pin S3 apa adanya.',
        '<b>6</b> Jack DC 6,5-9 V: <b>tidak dipakai</b>. <b>7</b> Simbol USB: port USB-C S3 menghadap ke sini saat dipasang.',
    ])
    isi += section('peta', 'Gambaran', 'Pin otak (ESP32-S3)', f'''
<div class="two">{photo(IMG + 's3_ekspansi_anot.jpg', 'Papan ekspansi ESP32-S3 dengan nomor bagian')}<div>{leg}</div></div>''' + table(
        ['Bagian', 'Pin modul', 'GPIO S3'],
        [['Bus SPI bersama', 'SCK / MOSI / MISO', f'{pin("IO12")} / {pin("IO11")} / {pin("IO13")} lewat breadboard'],
         ['Layar TFT', 'CS, DC, RESET', f'{pin("IO10")}, {pin("IO9")}, {pin("IO8")}'],
         ['Sentuhan', 'T_CS', pin('IO14')], ['SD card', 'SD_CS', pin('IO21')],
         ['Speaker MAX98357A', 'BCLK, LRC, DIN', f'{pin("IO4")}, {pin("IO5")}, {pin("IO7")}'],
         ['Mikrofon INMP441', 'SCK, WS, SD', f'{pin("IO15")}, {pin("IO16")}, {pin("IO6")}'],
         ['Ke badan', 'TX, RX', f'{pin("IO17")} → D16 DevKit, {pin("IO18")} ← D17 DevKit']],
        'Pin 35-37 (dipakai PSRAM), 19-20 (USB), 0, 3, 45, 46 (pin boot) sengaja tidak dipakai. Hampir semua pin yang dipakai ada di sisi kiri papan.'))

    t = ''
    t += stage(S, 1, 'Siapkan S3 dan cek PSRAM', ['ESP32-S3 N16R8', 'Papan ekspansi S3', 'Kabel USB-C data', 'Multimeter'], [
        'Pasang S3 ke papan ekspansi: port USB-C menghadap simbol USB (nomor 7). Cocokkan pin 3V3 di S3 dengan baris 3V3 papan.',
        'Colok kabel USB-C ke port S3 yang bertulisan <b>COM</b> atau <b>UART</b>. Kalau tulisannya tidak terbaca, coba salah satu, lalu yang lain di langkah 4 kalau tidak ada tulisan muncul.',
        'Atur Arduino IDE seperti tabel di bawah, lalu pilih port yang muncul.',
        'Upload kode tes. Serial Monitor 115200.',
        'Ukur blok 3V3 (nomor 2) ke GND: harus sekitar 3,3 V.',
    ], after=table(['Menu Tools', 'Pilihan'], [['Board', 'ESP32S3 Dev Module'], ['Flash Size', '16MB (128Mb)'], ['PSRAM', '<b>OPI PSRAM</b>'],
                                              ['Partition Scheme', 'Huge APP (3MB No OTA/1MB SPIFFS)'], ['USB CDC On Boot', 'Disabled (port COM). Kalau memakai port USB native: Enabled']],
                   'Salah memilih PSRAM adalah penyebab paling umum kode otak gagal nanti.'),
        figs=photo(IMG + 'esp32s3.jpg', 'ESP32-S3 N16R8 dengan dua port USB-C', 'ESP32-S3 N16R8 milikmu. Dua port USB-C: satu lewat chip USB-serial (COM), satu USB bawaan chip.', 360),
        code=kode_tes('tes_s3'),
        ok_='Muncul <code>Flash: 16 MB</code> dan <code>PSRAM: 8388608 byte</code> (sekitar 8 juta).',
        gagal=['PSRAM 0: menu PSRAM belum OPI PSRAM.', 'Upload gagal "Connecting...": tahan tombol BOOT di S3, tekan RST sekali, lepas BOOT, lalu upload lagi.',
               'Upload berhasil tapi Serial Monitor kosong: coba port USB-C yang satunya, atau ubah USB CDC On Boot.'])

    pins_tft = ['VCC', 'GND', 'CS', 'RESET', 'DC', 'SDI(MOSI)', 'SCK', 'LED', 'SDO(MISO) ✕', 'T_CLK', 'T_CS', 'T_DIN', 'T_DO', 'T_IRQ ✕']
    tujuan = [('#grup', 'breadboard 2 (400 lubang)'), 'rel merah (3V3)', 'rel biru (GND)', 'kolom 5 (MOSI)', 'kolom 2 (SCK)', 'kolom 8 (MISO)', ('#grup', 'papan ekspansi S3 (pin S)'), 'IO10', 'IO8', 'IO9', 'IO14']
    wires = [(0, 1, '3v3'), (1, 2, 'gnd'), (2, 7, 'putih'), (3, 8, 'coklat'), (4, 9, 'pink'), (5, 3, 'biru'), (6, 4, 'kuning'), (7, 1, '3v3'),
             (9, 4, 'kuning'), (10, 10, 'cyan'), (11, 3, 'biru'), (12, 5, 'hijau')]
    t += stage(S, 2, 'Layar TFT dan sentuhan', ['TFT 2,4" SPI 240x320', 'Breadboard 2 (400 lubang)', 'Kabel F-F, F-M, M-M', 'Library LovyanGFX'], [
        'Cabut USB. Install library <b>LovyanGFX</b> (lovyan03) dari Library Manager.',
        'Siapkan <b>breadboard 2</b> (breadboard 400 lubang kedua; yang pertama dipakai badan): blok <b>3V3</b> S3 → garis merah atas, GND → garis biru atas. MAX98357A dan INMP441 nanti juga ditancapkan di breadboard ini, di kolom 12 ke atas.',
        'Tiga kolom jadi bus SPI: kabel dari pin S <b>IO12</b> → <span class="pin">a2</span>, <b>IO11</b> → <span class="pin">a5</span>, <b>IO13</b> → <span class="pin">a8</span>.',
        'Sambungkan pin layar sesuai diagram. Baca nama pin di belakang layar: di modulmu urutannya dari bawah VCC, GND, CS, RESET, DC, SDI(MOSI), SCK, LED, SDO(MISO), T_CLK, T_CS, T_DIN, T_DO, T_IRQ. Satu kolom breadboard punya 5 lubang (a-e), jadi SCK layar ke <span class="pin">b2</span>, T_CLK ke <span class="pin">c2</span>, dan seterusnya.',
        '<b>SDO(MISO) layar dibiarkan tidak tersambung.</b> Pada banyak modul merah ini, pin SDO layar tidak mau "diam" saat layar tidak dipakai, sehingga mengacaukan sentuhan dan SD. Layar tidak perlu dibaca.',
        'Colok USB, upload kode tes. Layar berganti merah-hijau-biru lalu meminta kalibrasi: sentuh ujung panah di tiap sudut dengan ujung jari atau stylus.',
    ], figs=f'<div class="photos">{photo(IMG + "tft.jpg", "Layar TFT 2,4 inci dilihat dari depan", "Depan layar milikmu: 14 pin di kiri, 4 lubang SD di kanan.")}{photo(IMG + "tft_belakang.jpg", "Belakang layar TFT dengan nama pin dan slot SD", "Belakang: nama pin dari T_IRQ (atas) sampai VCC (bawah), slot SD ukuran penuh.")}</div>'
        + fig(diag.spi_bus()) + fig(diag.conn('TFT 2,4" SPI', '#8c1d18', pins_tft, 'Sambung ke', '#151a18', tujuan, wires, label='Sambungan pin layar TFT'),
                'Pin bertanda ✕ (SDO(MISO) dan T_IRQ) dibiarkan tanpa kabel.'),
        code=kode_tes('tes_layar'),
        ok_='Layar menampilkan "Layar berhasil!", dan setelah kalibrasi, titik kuning mengikuti jarimu.',
        learn='Layar, sentuhan, dan SD memakai tiga kabel yang sama (SCK, MOSI, MISO). ESP32 memilih siapa yang diajak bicara dengan menurunkan pin CS milik perangkat itu. Itulah kenapa tiap perangkat punya kabel CS sendiri.',
        gagal=['Layar putih saja: RESET, DC, atau CS belum tersambung, atau SCK/MOSI tertukar.', 'Warna terbalik (hitam jadi putih): di kode, ubah <code>c.invert = false</code> menjadi <code>true</code>. Merah dan biru tertukar: ubah <code>c.rgb_order</code>.',
               'Gambar bergeser atau bergaris: kabel SPI terlalu panjang. Pakai kabel 10-20 cm, atau turunkan <code>freq_write</code> ke 20000000.', 'Sentuhan tidak merespons: cek T_CS ke IO14 dan T_DO ke kolom 8.'])

    t += stage(S, 3, 'SD card di belakang layar', ['Kartu SD ukuran penuh (SanDisk 32 GB-mu)', 'Pin header 4 pin + solder', 'Kabel F-M'], [
        'Slot di belakang layarmu berukuran <b>SD penuh</b>, bukan micro SD, jadi SanDisk 32 GB-mu masuk langsung tanpa adaptor. Geser kunci kecil di sisi kiri kartu ke atas (bukan LOCK).',
        'Di laptop, cek format kartu: kartu 32 GB biasanya sudah <b>FAT32</b>. Kalau tertulis exFAT atau NTFS, format ulang sebagai FAT32 (isi kartu terhapus, pindahkan dulu).',
        'Salin satu atau dua lagu MP3 ke folder utama, nama pendek tanpa spasi (misalnya <code>lagu1.mp3</code>).',
        'Di tepi kanan layarmu ada 4 lubang kosong bertuliskan (dari atas) SD_SCK, SD_MISO, SD_MOSI, SD_CS. Solder 4 pin header di situ.',
        f'Sambung: {pin("SD_SCK")} → kolom 2 (<span class="pin">d2</span>), {pin("SD_MOSI")} → kolom 5 (<span class="pin">d5</span>), {pin("SD_MISO")} → kolom 8 (<span class="pin">d8</span>), {pin("SD_CS")} → pin S <b>IO21</b>.',
        'Masukkan kartu SD ke slot di belakang layar sampai berbunyi klik, kontak emas menghadap ke papan. Upload kode tes.',
    ], code=kode_tes('tes_sd'),
        ok_='Serial Monitor menulis ukuran SD dan daftar file, dan layar menampilkan nama file.',
        gagal=['SD GAGAL: format FAT32 (bukan exFAT), cek SD_CS ke IO21, coba kartu lain. Kartu di atas 32 GB biasanya exFAT.', 'Layar bekerja tapi SD gagal hanya saat layar dinyalakan: SDO(MISO) layar mungkin tersambung. Cabut.'])

    t += stage(S, 4, 'Speaker MAX98357A', ['MAX98357A (pin header dan terminal sudah terpasang)', 'Speaker kotak', 'Gunting/tang kupas', 'Kabel F-M dan F-F'], [
        'MAX98357A-mu sudah berpin header dan berterminal sekrup, jadi tidak perlu menyolder. Nama pin tercetak di sisi depan (LRC, BCLK, DIN, GAIN, SD, GND, Vin).',
        'Cek dulu ohm speaker kotakmu: lihat stiker di belakangnya, atau ukur dua kabelnya dengan multimeter di skala 200 Ω. Hasil sekitar 3-4 Ω berarti speaker 4 Ω, sekitar 6-8 Ω berarti 8 Ω. Keduanya aman untuk MAX98357A (4-8 Ω).',
        'Konektor putih di ujung kabel speaker tidak cocok dengan terminal. Potong konektornya, kupas 5 mm tiap kabel, pilin seratnya, lalu jepit di terminal: <b>merah ke ⊕</b>, hitam ke ⊖. Kencangkan sekrup lalu tarik pelan untuk mengecek.',
        f'{pin("Vin")} → garis merah (3,3 V, untuk di meja), {pin("GND")} → garis biru.',
        f'{pin("BCLK")} → S <b>IO4</b>, {pin("LRC")} → S <b>IO5</b>, {pin("DIN")} → S <b>IO7</b>. {pin("GAIN")} dan {pin("SD")} dibiarkan kosong.',
        'Upload kode tes (tanpa library tambahan).',
    ], figs=f'<div class="photos">{photo(IMG + "speaker_kotak.jpg", "Speaker kotak dengan kabel merah hitam dan konektor putih", "Speaker kotak milikmu. Speaker bulat 8 Ω 3 W jadi cadangan.")}{photo(IMG + "max98357a.jpg", "MAX98357A dengan pin header kuning", "MAX98357A milikmu: 4-8 Ω, 3 W, Vin 2,5-5,5 V.")}</div>' + fig(diag.conn('MAX98357A', '#5b2a86', ['LRC', 'BCLK', 'DIN', 'GAIN ✕', 'SD ✕', 'GND', 'Vin'], 'Sambung ke', '#151a18',
            [('#grup', 'papan S3 (pin S)'), 'IO5', 'IO4', 'IO7', ('#grup', 'breadboard'), 'rel biru (GND)', 'rel merah (3V3)'],
            [(0, 1, 'kuning'), (1, 2, 'hijau'), (2, 3, 'biru'), (5, 5, 'gnd'), (6, 6, '3v3')], label='Sambungan MAX98357A')),
        code=kode_tes('tes_speaker'),
        ok_='Terdengar do-re-mi-fa-sol lalu diam sedetik, berulang.',
        learn='MAX98357A menerima suara digital (I2S) dan langsung menguatkannya ke speaker. Pin GAIN kosong = penguatan 9 dB, pin SD kosong = menyala. Di robot jadi, Vin dipindah ke 5 V supaya lebih keras.',
        gagal=['Diam: cek BCLK/LRC/DIN tidak tertukar dan kabel speaker terjepit kuat di terminal (bukan isolasinya yang terjepit).', 'Bunyi kresek: kabel GND kurang kuat, atau kabel data terlalu panjang.'])

    t += stage(S, 5, 'Mikrofon INMP441', ['INMP441', 'Pin header + solder', 'Kabel F-M dan F-F'], [
        'Solder pin header ke INMP441 kalau belum ada. Bagian bulat dengan lubang kecil adalah lubang suara, jangan ditutup.',
        f'{pin("VDD")} → garis merah (3,3 V; INMP441 tidak boleh 5 V), {pin("GND")} dan {pin("L/R")} → garis biru.',
        f'{pin("SCK")} → S <b>IO15</b>, {pin("WS")} → S <b>IO16</b>, {pin("SD")} → S <b>IO6</b>.',
        'Upload kode tes, buka <b>Tools → Serial Plotter</b>. Diam, lalu tepuk tangan dekat mikrofon.',
    ], figs=photo(IMG + 'inmp441.jpg', 'Mikrofon INMP441 dengan label pin', 'INMP441 milikmu: SCK, WS, L/R di satu sisi, SD, VDD, GND di sisi lain.', 300),
        code=kode_tes('tes_mic'),
        ok_='Garis di Serial Plotter rendah saat diam (puluhan sampai ratusan) dan melonjak ke ribuan saat kamu tepuk tangan atau bicara.',
        learn='L/R ke GND membuat mikrofon mengirim suara di jalur kiri. Kode membaca jalur kiri saja. Mikrofon dan speaker memakai dua jalur I2S terpisah, jadi bisa diatur sendiri-sendiri.',
        gagal=['Selalu 0: SD dan WS tertukar, atau L/R tidak ke GND.', 'Angka besar terus walau diam: kabel GND lepas.'])

    t += stage(S, 6, 'Sambung otak ke badan (UART)', ['Badan sudah berisi robot_badan.ino', '3 kabel F-F'], [
        'Cabut USB kedua board.',
        f'S3 <b>IO17</b> (S) → DevKit baris {pin("D16")} / RX2 (S). S3 <b>IO18</b> (S) → DevKit baris {pin("D17")} / TX2 (S).',
        'Pin G papan S3 → pin G papan 30P. Tanpa kabel GND ini, data UART tidak terbaca.',
        'Colok USB ke S3 dan DevKit (boleh ke laptop yang sama, dua port). Upload kode tes ke <b>S3</b>.',
        'Di Serial Monitor S3 (akhir baris: Newline) ketik <code>PING</code> lalu <code>Q</code>.',
        'Roda terangkat dan saklar baterai ON, ketik <code>M 120 120 800</code>.',
    ], figs=fig(diag.conn('ESP32-S3', '#13283a', ['IO17 (TX)', 'IO18 (RX)', 'GND'], 'DevKit V1', '#1d3324', ['D16 (RX2)', 'D17 (TX2)', 'GND'],
                          [(0, 0, 'kuning', 'TX → RX'), (1, 1, 'hijau', 'RX ← TX'), (2, 2, 'gnd')], label='Sambungan UART otak ke badan'),
                'TX satu board selalu ke RX board lain (silang).'),
        code=kode_tes('tes_uart'),
        ok_='Muncul <code>badan: PONG</code> dan baris data <code>badan: D ...</code>. Perintah M menggerakkan roda.',
        gagal=['Tidak ada balasan: TX-RX tidak disilang, GND belum disambung, atau badan belum berisi robot_badan.ino.', 'Huruf aneh: kedua board harus 115200 (sudah di kode).'])

    isi += section('rakit', 'Langkah', 'Rakit otak', '<div class="parts" style="gap:16px">' + t + '</div>',
                   'Semua tahap di bagian ini memakai mode meja: S3 dapat daya dari USB, modul-modul dari blok 3V3.')
    isi += section('daftar', 'Cek ulang', 'Daftar kabel otak', table(
        ['Dari', 'Ke'],
        [['Blok 3V3 S3', 'rel merah breadboard 2'], ['GND S3', 'rel biru'], ['IO12 / IO11 / IO13', 'a2 / a5 / a8'],
         ['TFT VCC, LED', 'rel merah'], ['TFT GND', 'rel biru'], ['TFT SCK, T_CLK, SD_SCK', 'b2, c2, d2'], ['TFT SDI(MOSI), T_DIN, SD_MOSI', 'b5, c5, d5'],
         ['T_DO, SD_MISO', 'c8, d8'], ['TFT SDO(MISO), T_IRQ', 'tidak disambung'], ['TFT CS, DC, RESET', 'IO10, IO9, IO8'], ['T_CS', 'IO14'], ['SD_CS', 'IO21'],
         ['MAX98357A Vin / GND', 'rel merah (mode jalan: blok 5V) / rel biru'], ['MAX98357A BCLK, LRC, DIN', 'IO4, IO5, IO7'],
         ['INMP441 VDD / GND, L/R', 'rel merah / rel biru'], ['INMP441 SCK, WS, SD', 'IO15, IO16, IO6'],
         ['IO17, IO18, G', 'DevKit D16, D17, G']]))
    lead = 'Otak memegang layar sentuh, suara, dan koneksi internet. Setiap modul dites sendiri dulu, lalu otak disambung ke badan.'
    return S, 'Otak robot', lead, isi, 'Merakit otak robot ESP32-S3: layar TFT sentuh, SD card, speaker MAX98357A, mikrofon INMP441, dan UART ke badan.'
