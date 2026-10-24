from build import *
import kabel as KB

S = '4-mata'
IMG = '../img/'


def halaman():
    isi = ''
    isi += section('peta', 'Gambaran', 'Kenapa kamera punya board sendiri', '''
<div class="panel">
  <p>Modul kamera OV2640 telanjang yang kamu punya butuh tegangan 2,8 V dan 1,2 V yang tidak tersedia di papan S3. ESP32-CAM sudah membawa pengatur tegangan itu, kameranya, dan WiFi sendiri. Jadi kamera robot menjadi board ketiga yang mengirim foto lewat WiFi: otak tinggal meminta <code>http://alamat-kamera/capture</code>.</p>
  <p><b>ESP32-CAM-MB</b> adalah papan downloader: ESP32-CAM ditancapkan di atasnya supaya bisa di-upload lewat micro-USB. Di robot, ESP32-CAM dilepas dari MB dan diberi daya 5 V langsung.</p>
</div>''' + table(['Alamat (ganti IP dengan alamat kameramu)', 'Hasil'],
                  [['<code>http://IP/</code>', 'halaman uji dengan foto dan tombol'], ['<code>http://IP/capture</code>', 'foto 640x480'], ['<code>http://IP/capture?res=qvga</code>', 'foto 320x240 (untuk layar robot)'],
                   ['<code>http://IP/stream</code>', 'video langsung'], ['<code>http://IP/flash?on=1</code> / <code>on=0</code>', 'LED flash nyala / mati'], ['<code>http://IP/status</code>', 'IP, kekuatan WiFi, waktu menyala']],
                  'Selama ada satu browser membuka /stream, alamat lain menunggu. Tutup tab stream sebelum robot meminta foto.'))

    t = ''
    t += stage(S, 1, 'Upload kode kamera lewat MB', ['ESP32-CAM', 'ESP32-CAM-MB', 'Kabel micro-USB data'], [
        'Cek pita kamera di ESP32-CAM: ujungnya masuk rapat ke soket dan kunci hitam soket ditekan turun. Di pita kameramu tertulis <b>OV3660</b> (3 MP); kode sudah mendukungnya.',
        'Tancapkan ESP32-CAM ke MB: kamera menghadap ke atas, semua 16 pin masuk ke soket MB. Tulisan 5V dan GND di ESP32-CAM sejajar dengan pin yang sama di MB.',
        'Colok micro-USB MB ke laptop. Port tidak muncul? MB memakai chip CH340: install driver CH340.',
        'Di Arduino IDE: <b>Board → AI Thinker ESP32-CAM</b>, <b>Partition Scheme → Huge APP</b>, pilih port.',
        'Buka <a href="../kode/robot_mata/robot_mata.ino" download>robot_mata.ino</a>. Isi <code>WIFI_NAMA</code> dan <code>WIFI_PASSWORD</code> dengan WiFi 2.4 GHz rumahmu.',
        'Upload. Kalau berhenti di "Connecting....": tahan tombol <b>IO0</b> di MB, tekan <b>RST</b> sekali, lepas IO0 setelah persentase upload muncul.',
        'Setelah selesai, tekan <b>RST</b> di MB. Buka Serial Monitor 115200 dan catat alamat IP yang muncul.',
    ], figs=f'<div class="photos">{photo(IMG + "esp32cam.jpg", "ESP32-CAM dengan kamera OV3660", "ESP32-CAM milikmu: pin 5V dan GND di kiri atas, LED flash di kanan bawah.")}{photo(IMG + "esp32cam_mb.jpg", "Papan ESP32-CAM-MB dengan tombol IO0 dan RST", "ESP32-CAM-MB: port micro-USB, tombol IO0 (kiri) dan RST (kanan).")}</div>',
        code=kode_teks(open(KODE + 'robot_mata/robot_mata.ino', encoding='utf-8').read().split('// ================== PIN KAMERA')[0], 'robot_mata.ino (bagian atas)')
        + '<p class="cap">Kode lengkap: <a href="../kode/robot_mata/robot_mata.ino" download>unduh robot_mata.ino</a>. Tidak perlu library tambahan.</p>',
        ok_='Serial Monitor menulis jenis sensor (untuk kameramu: OV3660) dan <code>Tersambung. Alamat kamera: http://192.168.x.x</code>.',
        gagal=['"Kamera gagal mulai, kode 0x105" atau 0x20004: pita kamera tidak rapat. Cabut USB, buka kunci soket, masukkan ulang pita, tekan kunci.',
               '"Brownout detector was triggered" lalu restart terus: daya USB kurang kuat. Pakai port USB lain di laptop atau kabel lebih pendek.',
               'WiFi gagal: kamera hanya bisa WiFi 2.4 GHz, dan antena kecilnya lemah. Dekatkan ke router saat mencoba.'])

    t += stage(S, 2, 'Coba kamera di browser', ['HP atau laptop di WiFi yang sama'], [
        'Buka alamat IP kamera di browser HP atau laptop (contoh <code>http://192.168.1.50</code>). Muncul halaman "Mata robot" dengan satu foto.',
        'Coba <code>/stream</code>: video berjalan, sekitar 5-15 gambar per detik.',
        'Coba <code>/flash?on=1</code> lalu <code>/flash?on=0</code>.',
        'Gambar terbalik? Ubah <code>KAMERA_TERBALIK</code> ke <code>true</code>, upload lagi.',
        'Supaya alamat kamera tidak berubah, buka pengaturan router dan beri alamat tetap (DHCP reservation) untuk perangkat bernama esp32-... atau robotcam. Catat alamat itu untuk Bagian 5.',
    ], ok_='Foto dan video tampil. <code>robotcam.local</code> mungkin juga bisa dibuka di laptop, tapi di banyak HP Android tidak; alamat IP selalu bisa.')

    t += stage(S, 3, 'Pindah ke daya robot dan pasang di kepala', ['LM2596 #2 (5,0 V)', 'Kapasitor 1000 µF', 'Kabel 26 AWG merah dan hitam', '2 potongan jumper betina 3 cm', 'Heat shrink', '2 cable ties'], [
        'Saklar baterai OFF. Cabut ESP32-CAM dari MB.',
        f'Potong 26 AWG merah dan hitam masing-masing <b>{KB.p("5vcam")}</b> (dari LM2596 #2 di lantai 1 naik ke kepala di lantai 3, dengan kelonggaran untuk kepala menoleh). Solder satu ujung masing-masing ke potongan jumper betina 3 cm, heat shrink. Merah: OUT+ LM2596 #2 → pin {pin("5V")} ESP32-CAM. Hitam: OUT− → pin {pin("GND")} di sebelahnya. Ujung tanpa jumper dijepit di terminal sekrup.',
        f'<b>Jangan</b> memakai pin {pin("VCC")} atau {pin("3V3")} ESP32-CAM untuk memasukkan daya. VCC adalah keluaran, bukan masukan.',
        'Kapasitor 1000 µF di dekat ESP32-CAM, di antara 5V dan GND: solder kakinya ke kabel merah dan hitam sekitar 3 cm sebelum jumper betina (kaki panjang ke merah), lalu isolasi. Kabelnya lebih dari 20 cm, jadi kapasitor harus di ujung kamera, bukan di LM2596.',
        'Saklar ON. Tunggu 10 detik, lalu buka alamat IP kamera di browser: foto tetap tampil.',
        'ESP32-CAM tidak punya lubang baut, jadi ikat ke dudukan depan kepala pan-tilt dengan 2 cable ties: satu melintang di tepi paling atas (di atas kamera, lewat slot SD), satu melintang di area tulisan ESP32-CAM di bawah konektor pita. Jangan melintasi lensa, LED flash, atau konektor pita. Nilon tidak mengganggu antena; yang mengganggu hanya logam. Kencangkan secukupnya sampai tidak goyang.',
    ], ok_='Foto bisa dibuka dari browser walau ESP32-CAM hanya mendapat daya dari baterai. Saat kepala menoleh, kamera ikut menoleh.',
        learn='Saat WiFi mengirim, ESP32-CAM bisa menarik arus sesaat yang besar. Kapasitor 1000 µF memasok lonjakan itu sehingga tegangan tidak drop dan kamera tidak restart. Untuk upload kode lagi nanti, tancapkan kembali ke MB.')

    isi += section('rakit', 'Langkah', 'Pasang kamera', '<div class="parts" style="gap:16px">' + t + '</div>')
    lead = 'Kamera robot berjalan di ESP32-CAM sendiri dan mengirim foto lewat WiFi. Bagian ini singkat: upload, coba di browser, lalu pindah ke kepala robot.'
    return S, 'Mata robot', lead, isi, 'Memasang ESP32-CAM sebagai mata robot: upload lewat ESP32-CAM-MB, foto dan video lewat browser, daya dari baterai robot.'
