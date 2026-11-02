/* Cadangan progres panduan: ekspor ke file JSON dan impor kembali.
   Progres (centang langkah, centang barang) hanya tersimpan di browser ini, jadi file ini adalah satu-satunya cadangannya.
   Logika validasi dipisah dari tampilan supaya bisa diuji (tests/cadangan.test.ts). Dimuat oleh tema.js.
   Panduan baru: tambahkan polanya di KUNCI_PROGRES (dan di pindah/pindah.js). */
(function (root, factory) {
  var api = factory();
  if (typeof module === 'object' && module.exports) module.exports = api;
  else { root.panduanCadangan = api; api.mulaiUI(); }
})(typeof self !== 'undefined' ? self : this, function () {
  var JENIS = 'panduan-elektronika-progres';
  var VERSI = 1;
  var MAKS_BYTE = 1000000;
  var KUNCI_PROGRES = [/^robot-ai\/[a-z0-9-]+$/, /^elektronika-dasar$/, /^rakit-cuaca$/];
  var ID_LANGKAH = /^[A-Za-z0-9_-]{1,80}$/;

  function dasar(k) { return k.replace(/:ringkas$/, ''); }
  function adalahRingkas(k) { return /:ringkas$/.test(k); }
  function kunciProgres(k) {
    if (typeof k !== 'string' || k.length > 120) return false;
    var d = dasar(k);
    return KUNCI_PROGRES.some(function (re) { return re.test(d); });
  }
  function objekBiasa(v) { return v !== null && typeof v === 'object' && !Array.isArray(v); }
  function bulat(v) { return typeof v === 'number' && isFinite(v) && Math.floor(v) === v && v >= 0 && v <= 100000; }

  // nilai di file -> nilai bersih, atau null bila bentuknya tidak sah
  function bersihkan(kunci, nilai) {
    if (!objekBiasa(nilai)) return null;
    var ks = Object.keys(nilai);
    if (adalahRingkas(kunci)) {
      if (!bulat(nilai.d) || !bulat(nilai.n) || nilai.d > nilai.n) return null;
      return { d: nilai.d, n: nilai.n };
    }
    if (ks.length > 5000) return null;
    var out = {};
    for (var i = 0; i < ks.length; i++) {
      if (!ID_LANGKAH.test(ks[i]) || ks[i] === '__proto__' || typeof nilai[ks[i]] !== 'boolean') return null;
      out[ks[i]] = nilai[ks[i]];
    }
    return out;
  }

  function bacaJson(storage, k) {
    try { var v = JSON.parse(storage.getItem(k) || 'null'); return objekBiasa(v) ? v : null; } catch (e) { return null; }
  }
  function semuaKunci(storage) {
    var out = [];
    for (var i = 0; i < storage.length; i++) { var k = storage.key(i); if (kunciProgres(k)) out.push(k); }
    return out.sort();
  }

  function buatBerkas(storage, sekarang) {
    var data = {};
    semuaKunci(storage).forEach(function (k) {
      var v = bersihkan(k, bacaJson(storage, k));
      if (v) data[k] = v;
    });
    return { jenis: JENIS, versi: VERSI, dibuat: (sekarang || new Date()).toISOString(), data: data };
  }

  // teks file -> { ok, data, ditolak } atau { ok:false, galat }
  function periksa(teks) {
    if (typeof teks !== 'string' || !teks.length) return { ok: false, galat: 'kosong' };
    if (teks.length > MAKS_BYTE) return { ok: false, galat: 'terlalu-besar' };
    var obj;
    try { obj = JSON.parse(teks); } catch (e) { return { ok: false, galat: 'bukan-json' }; }
    if (!objekBiasa(obj) || obj.jenis !== JENIS) return { ok: false, galat: 'bukan-cadangan' };
    if (obj.versi !== VERSI) return { ok: false, galat: 'versi' };
    if (!objekBiasa(obj.data)) return { ok: false, galat: 'bukan-cadangan' };
    var ks = Object.keys(obj.data);
    if (ks.length > 200) return { ok: false, galat: 'terlalu-besar' };
    var data = {}, ditolak = 0;
    ks.forEach(function (k) {
      var v = kunciProgres(k) ? bersihkan(k, obj.data[k]) : null;
      if (v) data[k] = v; else ditolak++;
    });
    if (!Object.keys(data).length) return { ok: false, galat: 'tidak-ada-data' };
    return { ok: true, data: data, ditolak: ditolak };
  }

  function tulis(storage, k, v) { storage.setItem(k, JSON.stringify(v)); }

  // hitung ulang ringkasan "d/n langkah" dari centang yang sudah digabung
  function hitungRingkas(storage, data) {
    Object.keys(data).forEach(function (k) {
      if (adalahRingkas(k)) return;
      var gabung = bacaJson(storage, k) || {};
      var n = null, r = data[k + ':ringkas'] || bacaJson(storage, k + ':ringkas');
      if (r && bulat(r.n)) n = r.n;
      if (n === null) return;
      var d = 0;
      Object.keys(gabung).forEach(function (id) { if (gabung[id] === true && id.indexOf('have-') !== 0) d++; });
      tulis(storage, k + ':ringkas', { d: Math.min(d, n), n: n });
    });
  }

  // gabung: yang sudah tercentang di mana pun tetap tercentang; tidak ada centang yang dicabut
  function gabung(storage, data) {
    var halaman = 0;
    Object.keys(data).forEach(function (k) {
      if (adalahRingkas(k)) return;
      var ada = bacaJson(storage, k) || {};
      Object.keys(data[k]).forEach(function (id) { if (data[k][id] === true || !(id in ada)) ada[id] = data[k][id] || ada[id] === true; });
      tulis(storage, k, ada); halaman++;
    });
    hitungRingkas(storage, data);
    return halaman;
  }

  // ganti: hapus semua progres di browser ini, lalu tulis isi file
  function ganti(storage, data) {
    semuaKunci(storage).forEach(function (k) { storage.removeItem(k); });
    var halaman = 0;
    Object.keys(data).forEach(function (k) { if (!adalahRingkas(k)) { tulis(storage, k, data[k]); halaman++; } });
    Object.keys(data).forEach(function (k) { if (adalahRingkas(k)) tulis(storage, k, data[k]); });
    hitungRingkas(storage, data);
    return halaman;
  }

  /* ---------------- tampilan ---------------- */
  function mulaiUI() {
    if (typeof document === 'undefined') return;
    function t(id, en) { return window.panduanBahasa ? window.panduanBahasa.t(id, en) : id; }
    var tombol, panel, status, tertunda = null, fokusAwal = null;

    var PESAN = {
      'kosong': ['File kosong.', 'The file is empty.'],
      'terlalu-besar': ['File terlalu besar untuk cadangan progres.', 'The file is too large to be a progress backup.'],
      'bukan-json': ['File ini bukan JSON yang valid.', 'This file is not valid JSON.'],
      'bukan-cadangan': ['Ini bukan file cadangan progres panduan.', 'This is not a guide progress backup file.'],
      'versi': ['Versi file cadangan ini belum didukung.', 'This backup file version is not supported.'],
      'tidak-ada-data': ['File tidak berisi progres yang dikenali.', 'The file contains no recognised progress.']
    };

    function isi() {
      panel.textContent = '';
      function el(tag, teks, kelas) { var e = document.createElement(tag); if (teks) e.textContent = teks; if (kelas) e.className = kelas; return e; }
      panel.appendChild(el('h2', t('Cadangan progres', 'Progress backup')));
      panel.appendChild(el('p', t('Progres dan centang barangmu hanya tersimpan di browser ini. Simpan ke file supaya tidak hilang kalau data situs dibersihkan atau kamu pindah perangkat.',
        'Your progress and item checks are stored only in this browser. Save them to a file so they survive clearing site data or switching devices.')));
      var ek = el('button', t('Ekspor progres', 'Export progress'), 'bk-aksi'); ek.type = 'button';
      ek.addEventListener('click', ekspor); panel.appendChild(ek);

      var gb = el('fieldset', '', 'bk-mode');
      gb.appendChild(el('legend', t('Saat impor', 'When importing')));
      [['gabung', t('Gabungkan (disarankan): centang yang sudah ada tidak dicabut', 'Merge (recommended): existing checks are kept')],
       ['ganti', t('Ganti: hapus progres di browser ini, pakai isi file', 'Replace: clear progress in this browser, use the file')]].forEach(function (o, i) {
        var lb = el('label'); var r = document.createElement('input'); r.type = 'radio'; r.name = 'bk-mode'; r.value = o[0]; if (i === 0) r.checked = true;
        lb.appendChild(r); lb.appendChild(document.createTextNode(' ' + o[1])); gb.appendChild(lb);
      });
      panel.appendChild(gb);

      var lbf = el('label', '', 'bk-aksi bk-file');
      lbf.appendChild(document.createTextNode(t('Impor dari file…', 'Import from file…')));
      var inp = document.createElement('input'); inp.type = 'file'; inp.accept = '.json,application/json'; inp.addEventListener('change', impor);
      lbf.appendChild(inp); panel.appendChild(lbf);

      status = el('p', '', 'bk-status'); status.setAttribute('role', 'status'); status.setAttribute('aria-live', 'polite');
      panel.appendChild(status);
      var tutup = el('button', t('Tutup', 'Close'), 'bk-tutup'); tutup.type = 'button'; tutup.addEventListener('click', sembunyi); panel.appendChild(tutup);
      tertunda = null;
    }

    function pesan(teks, galat) { status.textContent = teks; status.classList.toggle('bk-galat', !!galat); }

    function ekspor() {
      var berkas = buatBerkas(localStorage);
      var jumlah = Object.keys(berkas.data).filter(function (k) { return !adalahRingkas(k); }).length;
      if (!jumlah) { pesan(t('Belum ada progres untuk diekspor.', 'There is no progress to export yet.'), true); return; }
      var blob = new Blob([JSON.stringify(berkas, null, 2)], { type: 'application/json' });
      var url = URL.createObjectURL(blob), a = document.createElement('a');
      a.href = url; a.download = 'progres-panduan-' + berkas.dibuat.slice(0, 10) + '.json';
      document.body.appendChild(a); a.click(); a.remove();
      setTimeout(function () { URL.revokeObjectURL(url); }, 2000);
      pesan(t('File cadangan disimpan (' + jumlah + ' halaman).', 'Backup file saved (' + jumlah + ' pages).'));
    }

    function terapkanImpor(hasil, mode) {
      var n = mode === 'ganti' ? ganti(localStorage, hasil.data) : gabung(localStorage, hasil.data);
      pesan(t('Selesai: ' + n + ' halaman dipulihkan. Memuat ulang…', 'Done: ' + n + ' pages restored. Reloading…'));
      document.dispatchEvent(new CustomEvent('panduan:pulih'));
      setTimeout(function () { location.reload(); }, 900);
    }

    function impor(e) {
      var f = e.target.files && e.target.files[0]; if (!f) return;
      if (f.size > MAKS_BYTE) { pesan(t(PESAN['terlalu-besar'][0], PESAN['terlalu-besar'][1]), true); e.target.value = ''; return; }
      var mode = (panel.querySelector('input[name=bk-mode]:checked') || {}).value || 'gabung';
      var fr = new FileReader();
      fr.onerror = function () { pesan(t('File tidak bisa dibaca.', 'The file could not be read.'), true); };
      fr.onload = function () {
        var hasil = periksa(String(fr.result));
        if (!hasil.ok) { var p = PESAN[hasil.galat] || PESAN['bukan-cadangan']; pesan(t(p[0], p[1]), true); return; }
        if (mode === 'ganti') {
          tertunda = hasil;
          pesan(t('File sah (' + Object.keys(hasil.data).length + ' kunci). Tekan konfirmasi untuk MENGGANTI semua progres di browser ini.', 'File is valid (' + Object.keys(hasil.data).length + ' keys). Press confirm to REPLACE all progress in this browser.'));
          var ok = document.createElement('button'); ok.type = 'button'; ok.className = 'bk-aksi bk-bahaya'; ok.textContent = t('Ya, ganti semua', 'Yes, replace everything');
          ok.addEventListener('click', function () { ok.remove(); terapkanImpor(tertunda, 'ganti'); });
          status.parentNode.insertBefore(ok, status.nextSibling);
        } else terapkanImpor(hasil, 'gabung');
      };
      fr.readAsText(f);
    }

    function tampil() { fokusAwal = document.activeElement; isi(); panel.hidden = false; tombol.setAttribute('aria-expanded', 'true'); var b = panel.querySelector('button'); if (b) b.focus(); }
    function sembunyi() { panel.hidden = true; tombol.setAttribute('aria-expanded', 'false'); if (fokusAwal && fokusAwal.focus) fokusAwal.focus(); }

    function pasang() {
      var grup = document.querySelector('.topbar .tools');
      if (!grup || grup.querySelector('.backup-btn')) return;
      tombol = document.createElement('button'); tombol.type = 'button'; tombol.className = 'theme-btn backup-btn';
      tombol.setAttribute('aria-haspopup', 'dialog'); tombol.setAttribute('aria-expanded', 'false');
      tombol.innerHTML = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M12 4v10M8 10l4 4 4-4M5 19h14"/></svg>';
      var span = document.createElement('span'); tombol.appendChild(span);
      function label() { span.textContent = t('Cadangan', 'Backup'); tombol.setAttribute('aria-label', t('Cadangan progres: ekspor atau impor', 'Progress backup: export or import')); }
      label();
      panel = document.createElement('div'); panel.className = 'backup-panel'; panel.hidden = true; panel.setAttribute('role', 'dialog'); panel.setAttribute('aria-label', t('Cadangan progres', 'Progress backup'));
      tombol.addEventListener('click', function () { if (panel.hidden) tampil(); else sembunyi(); });
      grup.insertBefore(tombol, grup.firstChild);
      grup.parentNode.appendChild(panel);
      document.addEventListener('keydown', function (e) { if (e.key === 'Escape' && !panel.hidden) sembunyi(); });
      document.addEventListener('click', function (e) { if (!panel.hidden && !panel.contains(e.target) && !tombol.contains(e.target)) sembunyi(); });
      document.addEventListener('panduan:bahasa', function () { label(); panel.setAttribute('aria-label', t('Cadangan progres', 'Progress backup')); if (!panel.hidden) isi(); });
    }
    if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', pasang); else pasang();
  }

  return { JENIS: JENIS, VERSI: VERSI, kunciProgres: kunciProgres, buatBerkas: buatBerkas, periksa: periksa, gabung: gabung, ganti: ganti, mulaiUI: mulaiUI };
});
