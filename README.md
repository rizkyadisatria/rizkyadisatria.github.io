# rizkyadisatria.github.io

Portfolio Rizky Adi Satria — **Bun + Svelte 5 + TypeScript**, siap deploy di **Vercel**.

Kontennya sama dengan versi HTML/JS lama (Inggris & Indonesia, tema terang/gelap, unduh CV PDF, form kontak).
Yang berubah hanya fondasinya.

## Perintah

```bash
bun install            # pasang dependensi
bun run dev            # build + server lokal di http://localhost:3000 (rebuild otomatis)
bun run build          # build produksi → dist/
bun run preview        # sajikan dist/ tanpa watch
bun run check          # svelte-check + tsc
bun run test           # build lalu tes hasil build (bun test)
```

## Struktur

```
src/              kode aplikasi (Svelte 5 + TS)
  components/     satu komponen per section
  data/           konten yang bukan terjemahan (proyek, skill, pengalaman)
  lib/            i18n, state (tema/bahasa), animasi scroll, pembuat PDF CV
public/           aset statis, disalin apa adanya ke dist/
  assets/         gambar, animasi Lottie, PDF portofolio
  controller/     panduan elektronika (HTML hasil generate, lihat catatan di bawah)
build.ts          Bun.build → dist/, robots.txt, sitemap.xml
vercel.json       konfigurasi deploy + header keamanan
```

## Deploy ke Vercel

1. Import repo di Vercel. Framework preset: **Other** (semua pengaturan sudah ada di `vercel.json`).
2. Opsional: set environment variable `SITE_URL` (mis. `https://rizkyadisatria.my.id`) agar canonical, Open Graph,
   JSON-LD, `robots.txt`, dan `sitemap.xml` memakai domain yang benar. Tanpa itu dipakai
   `VERCEL_PROJECT_PRODUCTION_URL`, lalu `https://rizkyadisatria.github.io` sebagai cadangan.
3. Versi Bun dikunci di `vercel.json` (`bun@1.4.4`) lewat `npx` agar build tidak bergantung pada versi bawaan Vercel.
   Kalau `bun.lock` diperbarui dengan Bun versi lain, samakan angkanya di sana.

## Keamanan

- Font dan ikon (Poppins, Boxicons), Lottie, dan html2pdf di-bundle dari npm dengan versi terkunci di `bun.lock`.
  Tidak ada lagi script/stylesheet dari CDN pihak ketiga (sebelumnya `boxicons@latest` tanpa pin).
- CSP ketat di halaman utama: `script-src 'self'` (tanpa `unsafe-inline`/`unsafe-eval`), `frame-ancestors 'none'`,
  `connect-src` hanya ke diri sendiri dan Formspree. Plus `nosniff`, `X-Frame-Options`, `Referrer-Policy`, `Permissions-Policy`.
- Form kontak memakai honeypot `_gotcha` dan batas panjang input.
- Alat penulisan panduan (`_build/`, `_alat/`, `*.py`) dan `secrets.yaml` tidak ikut dipublikasikan.

## Catatan panduan `/controller`

Halaman di `public/controller/` dibuat oleh skrip Python di `public/controller/*/_build/` dan diterbitkan apa adanya
(bukan komponen Svelte). Cara menjalankan skripnya tidak berubah; lokasinya saja yang pindah ke bawah `public/`.
Halaman ini tidak mendapat CSP ketat karena memakai script inline.

### Bahasa, cadangan progres, dan daftar barang di `/controller`

- **Bahasa ID/EN:** tombol di bilah atas, pilihannya disimpan di kunci `lang` yang sama dengan portofolio. Teks diterjemahkan lewat
  atribut `data-en` (dan `data-en-alt`, `data-en-aria-label`, …) serta kamus antarmuka di `assets/bahasa.js`. Halaman depan panduan
  dan daftar barang robot sudah dua bahasa; isi langkah demi langkah masih Indonesia dan halaman itu memberi catatan saat EN aktif.
  Panduan baru: beri objek `en` di `guides.js`.
- **Cadangan progres:** tombol "Cadangan" mengekspor/mengimpor progres ke file JSON (`assets/cadangan.js`, diuji di
  `tests/cadangan.test.ts`). Impor memvalidasi bentuk file dan hanya menerima kunci panduan yang dikenal. Panduan baru:
  tambahkan pola kuncinya di `KUNCI_PROGRES` (dan di `pindah/pindah.js`).
- **Daftar barang robot:** status "sudah punya" adalah kotak centang yang tersimpan di browser; daftar "masih perlu dibeli" dihitung dari
  centang itu. Barang yang tidak dipakai robot tidak ditampilkan. Sumbernya `robot-ai/_build/p0.py` dan `barang_en.py`; jalankan
  `python3 robot-ai/_build/build.py` lalu `git checkout robot-ai/kode/robot-ai-kode.zip` (zip hanya berubah stempel waktu).

## GitHub Pages

Situs utama ada di Vercel (`rizkyadisatria.vercel.app`). `index.html` dan `404.html` di akar repo hanya meneruskan
pengunjung alamat lama `rizkyadisatria.github.io` (termasuk tautan dalam seperti `/controller/...`) ke Vercel.
Berkas itu tidak ikut build (`dist/` hanya berisi hasil `src/` dan `public/`). Progres panduan `/controller` tersimpan di
localStorage per alamat; saat perangkat lama membuka alamat github.io, progresnya dibawa sekali ke Vercel lewat
`/controller/pindah/` (data lama di github.io tidak dihapus).

---

Dibuat oleh rizkyadisatria.
