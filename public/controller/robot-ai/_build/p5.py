from build import *

S = '5-ai'


def halaman():
    isi = ''
    isi += section('biaya', 'Sebelum mulai', 'Tiga layanan internet yang dipakai', table(
        ['Layanan', 'Untuk apa', 'Biaya', 'Wajib?'],
        [['Bot Telegram', 'perintah dan foto dari HP', 'gratis', 'ya'],
         ['Claude API (Anthropic)', 'otak AI: ngobrol, memutuskan gerakan, melihat foto', 'berbayar per pemakaian, isi saldo dulu', 'tidak, tanpa ini robot tetap bisa dikendalikan lewat perintah'],
         ['Groq (Whisper)', 'mengubah suaramu jadi teks', 'ada tingkat gratis dengan batas pemakaian', 'tidak, hanya untuk tombol BICARA'],
         ['Google TTS (lewat library audio)', 'robot berbicara', 'gratis, tapi bukan layanan resmi', 'tidak; kalau berhenti bekerja, robot tetap menjawab lewat Telegram']]) + '''
<div class="panel">
  <h3>Perkiraan biaya Claude</h3>
  <p>Model bawaan di kode adalah <code>claude-haiku-5-5</code>, model Claude paling murah. Halaman model Anthropic mencantumkan harga mulai $0,10 per juta token masukan dan $0,50 per juta token keluaran. Satu obrolan robot memakai sekitar 1.000-1.500 token masukan (aturan robot + riwayat + data sensor) dan sekitar 100 token jawaban, jadi kira-kira $0,0002 atau sekitar Rp3 per obrolan dengan kurs Rp16.000-an. Foto 640x480 menambah sekitar 400 token. Harga dan kurs bisa berubah; cek <a href="https://platform.claude.com/docs/en/about-claude/pricing">halaman harga</a>.</p>
  <p>Wajib: pasang <b>batas pengeluaran bulanan</b> di console (Settings → Limits), misalnya $5, supaya kode yang macet berulang tidak menghabiskan saldo.</p>
</div>''')

    t = ''
    t += stage(S, 1, 'Buat bot Telegram', ['HP dengan Telegram', 'ESP32-S3', 'Library UniversalTelegramBot dan ArduinoJson'], [
        'Di Telegram, cari <b>@BotFather</b>, kirim <code>/newbot</code>, beri nama (misalnya Robi Robot) dan username yang diakhiri "bot". Simpan <b>token</b> yang diberikan.',
        'Cari <b>@userinfobot</b>, tekan Start, simpan angka <b>Id</b>-mu. Robot hanya akan menurut ke ID ini.',
        'Buka bot barumu dan tekan <b>Start</b>.',
        'Sudah membuat bot untuk stasiun cuaca? Buat bot baru untuk robot, supaya keduanya tidak berebut pesan.',
        'Install library <b>UniversalTelegramBot</b> (Brian Lough) dan <b>ArduinoJson</b> (Benoit Blanchon).',
        'Isi WiFi, token, dan chat ID di kode tes, upload ke S3, lalu kirim pesan ke bot.',
    ], code=kode_tes('tes_telegram'),
        ok_='Bot mengirim "Halo dari ESP32-S3!" dan membalas setiap pesanmu.',
        gagal=['"GAGAL kirim": token salah ketik (harus lengkap dengan angka dan titik dua), chat ID salah, atau belum menekan Start di bot.', 'WiFi tidak tersambung: ESP32 hanya bisa 2.4 GHz.'])

    t += stage(S, 2, 'API key Claude', ['Akun Claude Console', 'Kartu atau metode bayar untuk isi saldo'], [
        'Buka <a href="https://platform.claude.com">platform.claude.com</a> (alamat lama console.anthropic.com mengarah ke sini), daftar atau masuk.',
        '<b>Settings → Billing</b>: tambahkan metode bayar dan isi saldo kecil (misalnya $5). Tanpa saldo, API menolak semua permintaan.',
        '<b>Settings → Limits</b>: pasang batas pengeluaran bulanan.',
        '<b>Settings → API Keys → Create Key</b>. Beri nama "robot", salin kuncinya (diawali <code>sk-ant-</code>). Kunci hanya ditampilkan sekali.',
        'Isi kunci di kode tes, upload ke S3, ketik pertanyaan di Serial Monitor.',
    ], code=kode_tes('tes_claude'),
        ok_='Jawaban AI muncul dalam beberapa detik. Kode 401 = kunci salah, 400 dengan pesan saldo = saldo kosong, 429 = terlalu sering.',
        learn='Kode ini mengirim pertanyaan dalam format JSON ke alamat <code>api.anthropic.com/v1/messages</code> lewat HTTPS. Koneksinya terenkripsi, tapi demi kesederhanaan sertifikat server tidak diperiksa (<code>setInsecure</code>). Untuk proyek rumahan ini cukup; jangan pakai pola ini untuk data sensitif.')

    t += stage(S, 3, 'Suara ke teks dengan Groq (opsional)', ['Akun Groq'], [
        'Buka <a href="https://console.groq.com">console.groq.com</a>, daftar, lalu <b>API Keys → Create API Key</b>. Salin kuncinya (diawali <code>gsk_</code>).',
        'Kode otak mengirim rekaman 4 detik (WAV 16 kHz) ke model <code>whisper-large-v3-turbo</code> dengan bahasa Indonesia, dan menerima teksnya.',
        'Rekamanmu dikirim ke server Groq. Baca kebijakan privasi mereka kalau itu penting bagimu. Tidak mengisi kunci ini juga tidak apa-apa: tombol BICARA saja yang tidak berfungsi.',
    ], ok_='Kamu punya kunci gsk_ yang siap diisi di kode otak.')

    t += stage(S, 4, 'Pasang library audio dan upload kode otak', ['Semua rangkaian Bagian 3', 'Badan dan kamera menyala'], [
        'Library <b>ESP32-audioI2S</b> tidak ada di Library Manager. Buka <a href="https://github.com/schreibfaul1/ESP32-audioI2S/releases">halaman rilisnya</a>, unduh ZIP versi <b>4.0.0</b> (Source code zip). Di Arduino IDE: <b>Sketch → Include Library → Add .ZIP Library</b>, pilih file ZIP itu.',
        'Pastikan LovyanGFX, UniversalTelegramBot, dan ArduinoJson sudah terpasang.',
        'Buka <a href="../kode/robot_otak/robot_otak.ino" download>robot_otak.ino</a>. Isi bagian KONFIGURASI: WiFi, token, chat ID, API key Claude, API key Groq (boleh kosong), dan <code>KAMERA_ALAMAT</code> dengan IP kamera dari Bagian 4.',
        'Ganti <code>NAMA_ROBOT</code> dan <code>NAMA_PEMILIK</code> sesukamu.',
        'Atur Tools seperti Bagian 3 Tahap 1 (<b>PSRAM: OPI PSRAM</b>, <b>Huge APP</b>). Upload. Kompilasi pertama bisa beberapa menit.',
        'Setelah menyala pertama kali, layar meminta kalibrasi sentuh: sentuh ujung panah di tiap sudut. Hasilnya disimpan. Untuk kalibrasi ulang, sentuh layar saat S3 baru dinyalakan.',
    ], code=kode_teks(open(KODE + 'robot_otak/robot_otak.ino', encoding='utf-8').read().split('// ======================= PIN')[0], 'robot_otak.ino (bagian atas: pengaturan dan konfigurasi)')
        + '<p class="cap">Kode lengkap sekitar 770 baris: <a href="../kode/robot_otak/robot_otak.ino" download>unduh robot_otak.ino</a>. Jangan unggah file yang sudah berisi token dan API key ke tempat publik.</p>',
        ok_='Layar menampilkan wajah dengan dua mata yang berkedip, baris status di atas berisi baterai dan jarak dari badan, speaker berkata "Halo, aku Robi. Aku siap.", dan bot mengirim "Robi menyala".',
        gagal=['Error kompilasi "Audio.h: No such file": library audio belum terpasang dari ZIP.', 'Restart terus dengan pesan PSRAM: menu PSRAM belum OPI PSRAM.',
               '"badan belum terhubung" di layar: cek kabel UART dan GND (Bagian 3 Tahap 6).', 'Tidak bersuara tapi tes speaker berhasil: Google TTS mungkin sedang menolak. Jawaban tetap dikirim lewat Telegram.'])

    fitur = table(['Coba ini', 'Yang terjadi'], [
        ['Tahan tombol MAJU/MUNDUR/KIRI/KANAN di layar', 'robot bergerak selama jari menempel, berhenti saat dilepas'],
        ['Ketuk STOP di layar', 'motor dan suara berhenti, mode kembali biasa'],
        ['Ketuk wajah robot', 'layar menampilkan kamera selama 10 detik (ketuk lagi untuk keluar)'],
        ['Ketuk BICARA, lalu bicara 4 detik', 'suara diubah jadi teks, dijawab AI, diucapkan dan dikirim ke Telegram'],
        ['Telegram: <code>/status</code>', 'jarak, baterai, suhu, kelembapan, kemiringan'],
        ['Telegram: <code>/maju 800</code>, <code>/kiri</code>, <code>/stop</code>', 'gerak langsung tanpa AI'],
        ['Telegram: <code>/kepala 45 90</code>', 'kepala menoleh'],
        ['Telegram: <code>/foto</code>', 'foto dari kamera'],
        ['Telegram: <code>/lihat ada apa di depanmu?</code>', 'foto dikirim, AI menjelaskan isinya'],
        ['Telegram: pesan biasa, misalnya "maju sedikit lalu lihat ke kiri"', 'AI menjawab dan menggerakkan badan'],
        ['Telegram: <code>/lagu</code>, <code>/putar lagu1.mp3</code>, <code>/volume 15</code>, <code>/diam</code>', 'musik dari SD card'],
        ['Telegram: <code>/radio</code> + alamat stream', 'memutar radio internet (alamat stream MP3/AAC dari situs radio)'],
        ['Telegram: <code>/bilang selamat pagi</code>', 'robot mengucapkan teks'],
        ['Telegram: <code>/ikuti</code>', 'robot menjaga jarak 35-70 cm ke benda di depannya, misalnya kakimu'],
        ['Telegram: <code>/jaga</code>', 'robot diam; kalau jarak di depannya berubah jauh, ia berkata "Siapa itu?" dan mengirim foto'],
        ['Telegram: <code>/biasa</code>', 'keluar dari mode ikuti/jaga'],
    ])
    t += stage(S, 5, 'Coba semua kemampuan', ['Robot lengkap di meja, roda terangkat'], [
        'Mulai dari <code>/bantuan</code> di Telegram: bot mengirim daftar perintah.',
        'Coba satu per satu fitur di tabel di bawah. Roda tetap terangkat sampai Bagian 6.',
        'Ngobrol bebas lewat Telegram. AI tahu data sensor terbaru, jadi coba tanya "berapa suhu di sini?" atau "baterainya masih cukup?".',
    ], after=fitur,
        ok_='Semua perintah langsung (diawali /) berjalan. Obrolan AI menjawab dalam beberapa detik.',
        learn='AI diminta menjawab dalam format JSON: kalimat untuk diucapkan, ditambah daftar aksi seperti {"m":[150,150,800]} untuk maju. Kode otak menjalankan aksi itu satu per satu, dan badan tetap menolak gerakan yang berbahaya. Mode ikuti dan jaga hanya memakai sensor ultrasonik, jadi robot tidak bisa membedakan orang dan benda; ia mengikuti atau menjaga apa pun yang ada di depannya.',
        gagal=['AI menjawab tapi robot tidak bergerak: lihat Serial Monitor. Kalau ada "E DITOLAK", ada halangan atau tombol STOP terkunci.', 'Foto gagal: kamera menyala? IP di KAMERA_ALAMAT benar? Tab /stream di browser sudah ditutup?',
               'BICARA menghasilkan "Tidak terdengar jelas": bicara lebih dekat ke mikrofon, atau GROQ_API_KEY belum diisi.'])

    isi += section('rakit', 'Langkah', 'Pasang kecerdasan', '<div class="parts" style="gap:16px">' + t + '</div>')
    isi += section('aman', 'Penting', 'Keamanan dan privasi', ul([
        'Token bot, API key Claude, dan API key Groq sama dengan password. Jangan unggah ke GitHub atau kirim ke orang lain. Kalau bocor, hapus kuncinya di console dan buat yang baru.',
        'Robot hanya menurut ke chat ID-mu. Orang lain yang menemukan bot-mu akan dibalas "aku hanya menurut ke pemilikku".',
        'Kamera dan mikrofon bisa merekam orang lain di rumah. Beritahu mereka, dan matikan robot kalau tidak dipakai.',
        'Mode jaga bukan alarm keamanan sungguhan: sensor ultrasonik bisa terkecoh tirai yang bergerak, dan bisa melewatkan orang di samping.',
    ]))
    lead = 'Bagian ini menyalakan kecerdasan robot: Telegram untuk perintah dari HP, Claude untuk berpikir dan melihat, Groq untuk mendengar. Setiap layanan dites sendiri sebelum kode otak lengkap di-upload.'
    return S, 'Kecerdasan', lead, isi, 'Menyalakan kecerdasan robot: bot Telegram, Claude API untuk ngobrol dan melihat foto, suara ke teks, mode ikuti dan jaga.'
