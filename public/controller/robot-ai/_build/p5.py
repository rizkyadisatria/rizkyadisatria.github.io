from build import *

S = '5-ai'


def halaman():
    isi = ''
    isi += section('biaya', 'Sebelum mulai', 'Tiga layanan internet yang dipakai', table(
        ['Layanan', 'Untuk apa', 'Biaya', 'Wajib?'],
        [['Bot Telegram', 'perintah dan foto dari HP', 'gratis', 'ya'],
         ['Gemini API (Google AI Studio)', 'otak AI: ngobrol, memutuskan gerakan, melihat foto', 'gratis dengan batas harian', 'tidak, tanpa ini robot tetap bisa dikendalikan lewat perintah'],
         ['Groq', 'mengubah suaramu jadi teks, dan AI cadangan kalau jatah Gemini habis', 'gratis dengan batas harian', 'tidak, tapi disarankan'],
         ['Google TTS (lewat library audio)', 'robot berbicara', 'gratis, tapi bukan layanan resmi', 'tidak; kalau berhenti bekerja, robot tetap menjawab lewat Telegram']]) + '''
<div class="panel">
  <h3>Batas gratis Gemini</h3>
  <p>Gemini dipakai lewat <b>Google AI Studio</b> tanpa kartu kredit. Batasnya dihitung per proyek dan per model (permintaan per menit dan per hari), dan Google tidak lagi mengumumkannya di dokumentasi: angka yang berlaku untukmu hanya terlihat di <a href="https://aistudio.google.com/rate-limit">halaman batas AI Studio</a> dan bisa berubah sewaktu-waktu. Jatah harian kembali penuh tengah malam waktu Pasifik, sekitar pukul 15.00-16.00 WITA.</p>
  <p>Contoh jatah gratis yang terlihat di satu akun pada November 2026: <b>Gemini 3.5 Flash Lite</b> 15 per menit dan 500 per hari, <b>Gemma 4 31B</b> 30 per menit dan 14.400 per hari, sedangkan Gemini 2.5 Flash Lite hanya 20 per hari. Kode memakai Gemini 3.5 Flash Lite sebagai otak utama dan Gemma 4 31B (kunci yang sama) sebagai cadangan pertama.</p>
  <p>Satu obrolan robot = satu permintaan; pertanyaan yang membuat robot memotret biasanya dua (memutuskan melihat, lalu menjelaskan fotonya). Kalau jatah Gemini habis, robot otomatis pindah ke Gemma, lalu ke <b>Groq</b> kalau kunci Groq diisi (Tahap 3). Di tingkat gratis, Google boleh memakai isi pertanyaan dan foto untuk memperbaiki layanannya; jangan kirim hal yang pribadi.</p>
</div>''')

    t = ''
    t += stage(S, 1, 'Buat bot Telegram', ['HP dengan Telegram', 'ESP32-S3', 'Library UniversalTelegramBot dan ArduinoJson'], [
        'Di Telegram, cari <b>@BotFather</b>, kirim <code>/newbot</code>, beri nama (misalnya Zond Robot) dan username yang diakhiri "bot". Simpan <b>token</b> yang diberikan.',
        'Cari <b>@userinfobot</b>, tekan Start, simpan angka <b>Id</b>-mu. Robot hanya akan menurut ke ID ini.',
        'Buka bot barumu dan tekan <b>Start</b>.',
        'Sudah membuat bot untuk stasiun cuaca? Buat bot baru untuk robot, supaya keduanya tidak berebut pesan.',
        'Install library <b>UniversalTelegramBot</b> (Brian Lough) dan <b>ArduinoJson</b> (Benoit Blanchon).',
        'Isi WiFi, token, dan chat ID di kode tes, upload ke S3, lalu kirim pesan ke bot.',
    ], code=kode_tes('tes_telegram'),
        ok_='Bot mengirim "Halo dari ESP32-S3!" dan membalas setiap pesanmu.',
        gagal=['"GAGAL kirim": token salah ketik (harus lengkap dengan angka dan titik dua), chat ID salah, atau belum menekan Start di bot.', 'WiFi tidak tersambung: ESP32 hanya bisa 2.4 GHz.'])

    t += stage(S, 2, 'API key Gemini (gratis)', ['Akun Google'], [
        'Buka <a href="https://aistudio.google.com">aistudio.google.com</a> dan masuk dengan akun Google.',
        '<b>Get API key → Create API key</b>, pilih atau buat proyek baru. Salin kuncinya (diawali <code>AIza</code>). Tidak perlu kartu kredit.',
        'Buka <a href="https://aistudio.google.com/rate-limit">halaman batas</a> dan lihat jatah per hari tiap model. Kode memakai <code>gemini-3.5-flash-lite</code> (cepat, bisa melihat foto) dengan cadangan <code>gemma-4-31b-it</code>. Kalau jatahmu berbeda, tulis nama model yang jatah per harinya paling besar di <code>MODEL</code> (kode tes) dan <code>GEMINI_MODEL</code> (kode otak).',
        'Isi kunci di kode tes, upload ke S3, ketik pertanyaan di Serial Monitor.',
    ], code=kode_tes('tes_gemini'),
        ok_='Jawaban AI muncul dalam beberapa detik. Kode 400 dengan API_KEY_INVALID = kunci salah, 403 = kunci tidak diizinkan, 404 = nama model salah atau sudah tidak tersedia, 429 = jatah habis atau terlalu sering.',
        learn='Kode ini mengirim pertanyaan dalam format JSON ke alamat <code>generativelanguage.googleapis.com</code> lewat HTTPS, dengan kunci di header <code>x-goog-api-key</code>. Koneksinya terenkripsi, tapi demi kesederhanaan sertifikat server tidak diperiksa (<code>setInsecure</code>). Untuk proyek rumahan ini cukup; jangan pakai pola ini untuk data sensitif.')

    t += stage(S, 3, 'Groq: suara ke teks dan AI cadangan (opsional)', ['Akun Groq'], [
        'Buka <a href="https://console.groq.com">console.groq.com</a>, daftar, lalu <b>API Keys → Create API Key</b>. Salin kuncinya (diawali <code>gsk_</code>).',
        'Kode otak memotong suaramu per kalimat (WAV 16 kHz, paling lama 6 detik), mengirimnya ke model <code>whisper-large-v3-turbo</code> dengan bahasa Indonesia, dan menerima teksnya. Robot hanya menanggapi kalimat yang menyebut namanya.',
        'Kunci yang sama juga jadi cadangan otak AI: kalau Gemini menolak (misalnya jatah harian habis), kode otak bertanya ke model <code>GROQ_MODEL</code> di Groq, yang juga bisa melihat foto.',
        'Rekamanmu dikirim ke server Groq. Baca kebijakan privasi mereka kalau itu penting bagimu. Tidak mengisi kunci ini juga tidak apa-apa: tombol BICARA tidak berfungsi dan tidak ada cadangan AI.',
    ], ok_='Kamu punya kunci gsk_ yang siap diisi di kode otak.')

    t += stage(S, 4, 'Pasang library audio dan upload kode otak', ['Semua rangkaian Bagian 3', 'Badan dan kamera menyala'], [
        'Library <b>ESP32-audioI2S</b> tidak ada di Library Manager. Buka <a href="https://github.com/schreibfaul1/ESP32-audioI2S/releases">halaman rilisnya</a>, unduh ZIP versi <b>4.0.0</b> (Source code zip). Di Arduino IDE: <b>Sketch → Include Library → Add .ZIP Library</b>, pilih file ZIP itu.',
        'Pastikan LovyanGFX, UniversalTelegramBot, dan ArduinoJson sudah terpasang.',
        'Buka <a href="../kode/robot_otak/robot_otak.ino" download>robot_otak.ino</a>. Isi bagian KONFIGURASI: WiFi, token, chat ID, API key Gemini, API key Groq (boleh kosong, tapi disarankan sebagai cadangan), dan <code>KAMERA_ALAMAT</code> dengan IP kamera dari Bagian 4.',
        'Ganti <code>NAMA_ROBOT</code> (bawaan <code>Zond</code>) dan <code>NAMA_PEMILIK</code> sesukamu. Pilih nama robot yang pendek dan jarang muncul di obrolan sehari-hari, karena robot hanya menanggapi perintah suara yang menyebut namanya.',
        'Atur Tools seperti Bagian 3 Tahap 1 (<b>PSRAM: OPI PSRAM</b>, <b>Huge APP</b>). Upload. Kompilasi pertama bisa beberapa menit.',
        'Setelah menyala pertama kali, layar meminta kalibrasi sentuh: sentuh ujung panah di tiap sudut. Hasilnya disimpan. Untuk kalibrasi ulang, sentuh layar saat S3 baru dinyalakan.',
    ], code=kode_teks(open(KODE + 'robot_otak/robot_otak.ino', encoding='utf-8').read().split('// ======================= PIN')[0], 'robot_otak.ino (bagian atas: pengaturan dan konfigurasi)')
        + '<p class="cap">Kode lengkap sekitar 770 baris: <a href="../kode/robot_otak/robot_otak.ino" download>unduh robot_otak.ino</a>. Jangan unggah file yang sudah berisi token dan API key ke tempat publik.</p>',
        ok_='Layar menampilkan wajah dengan dua mata yang berkedip, baris status di atas berisi baterai dan jarak dari badan, speaker berkata "Halo, aku Zond. Aku siap.", dan bot mengirim "Zond menyala".',
        gagal=['Error kompilasi "Audio.h: No such file": library audio belum terpasang dari ZIP.', 'Restart terus dengan pesan PSRAM: menu PSRAM belum OPI PSRAM.',
               '"badan belum terhubung" di layar: cek kabel UART dan GND (Bagian 3 Tahap 6).', 'Tidak bersuara tapi tes speaker berhasil: Google TTS mungkin sedang menolak. Jawaban tetap dikirim lewat Telegram.'])

    fitur = table(['Coba ini', 'Yang terjadi'], [
        ['Tahan tombol MAJU/MUNDUR/KIRI/KANAN di layar', 'robot bergerak selama jari menempel, berhenti saat dilepas'],
        ['Ketuk STOP di layar', 'motor dan suara berhenti, mode kembali biasa'],
        ['Ketuk wajah robot', 'layar menampilkan kamera selama 10 detik (ketuk lagi untuk keluar)'],
        ['Ucapkan "Zond, maju sedikit"', 'robot mendengar namanya, menjawab, bergerak, dan mengirim percakapan ke Telegram'],
        ['Ucapkan "Zond" saja, tunggu robot berkata "Ya?", lalu bicara', 'perintah sesudah "Ya?" tidak perlu menyebut nama lagi (selama sekitar 8 detik)'],
        ['Ketuk BICARA, lalu bicara', 'sama, tanpa perlu menyebut nama'],
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
               'BICARA menghasilkan "Tidak terdengar jelas": bicara lebih dekat ke mikrofon, atau GROQ_API_KEY belum diisi.',
               'Robot tidak menanggapi saat namanya disebut: lihat Serial Monitor. Baris "[dengar] tanpa nama, diabaikan" menunjukkan apa yang didengar; kalau namanya ditulis jauh berbeda, ganti NAMA_ROBOT dengan nama yang lebih jelas diucapkan. Robot juga tidak mendengar selama speaker berbunyi, termasuk saat musik atau radio diputar.',
               'Robot berkata "Jatah AI gratis sedang habis": batas harian Gemini (dan Groq kalau diisi) sudah terpakai. Tunggu jatahnya kembali sekitar pukul 15.00-16.00 WITA, atau pilih model lain dengan jatah lebih besar di halaman batas AI Studio.'])

    isi += section('rakit', 'Langkah', 'Pasang kecerdasan', '<div class="parts" style="gap:16px">' + t + '</div>')
    isi += section('aman', 'Penting', 'Keamanan dan privasi', ul([
        'Token bot, API key Gemini, dan API key Groq sama dengan password. Jangan unggah ke GitHub atau kirim ke orang lain. Kalau bocor, hapus kuncinya di AI Studio atau console Groq dan buat yang baru.',
        'Robot hanya menurut ke chat ID-mu. Orang lain yang menemukan bot-mu akan dibalas "aku hanya menurut ke pemilikku".',
        'Kamera dan mikrofon bisa merekam orang lain di rumah. Dengan <code>DENGAR_NAMA</code> aktif, setiap kalimat yang terdengar dikirim ke Groq untuk dicek apakah menyebut nama robot (paling banyak 10 kalimat per menit). Beritahu orang di rumah, matikan robot kalau tidak dipakai, atau isi <code>DENGAR_NAMA = false</code> supaya robot hanya mendengar setelah tombol BICARA.',
        'Mode jaga bukan alarm keamanan sungguhan: sensor ultrasonik bisa terkecoh tirai yang bergerak, dan bisa melewatkan orang di samping.',
    ]))
    lead = 'Bagian ini menyalakan kecerdasan robot: Telegram untuk perintah dari HP, Gemini untuk berpikir dan melihat, Groq untuk mendengar dan sebagai AI cadangan. Setiap layanan dites sendiri sebelum kode otak lengkap di-upload.'
    return S, 'Kecerdasan', lead, isi, 'Menyalakan kecerdasan robot: bot Telegram, Gemini API gratis untuk ngobrol dan melihat foto, suara ke teks, mode ikuti dan jaga.'
