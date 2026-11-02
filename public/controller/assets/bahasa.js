/* Bahasa Indonesia / English untuk halaman panduan.
   - Pilihan disimpan di localStorage dengan kunci "lang" (sama dengan portofolio: "id" atau "en").
   - Elemen dengan data-en="..." diganti teksnya; atribut lain lewat data-en-alt, data-en-aria-label, data-en-title, data-en-placeholder.
   - Teks antarmuka yang berulang (tombol, navigasi, judul kotak) diterjemahkan lewat kamus di bawah, tanpa mengubah HTML.
   - Isi langkah demi langkah yang belum diterjemahkan tetap Indonesia; halaman seperti itu diberi catatan di atas (lihat CATATAN).
   Dimuat oleh tema.js setelah bilah atas dibuat. */
(function () {
  var KEY = 'lang';
  var LANG_ATTR = ['alt', 'aria-label', 'title', 'placeholder'];

  function baca() { try { return localStorage.getItem(KEY) === 'en' ? 'en' : 'id'; } catch (e) { return 'id'; } }
  var bhs = baca();

  var BAGIAN = { 'Daya': 'Power', 'Badan': 'Body', 'Otak': 'Brain', 'Mata': 'Eyes', 'Kecerdasan': 'Intelligence', 'Rakit akhir': 'Final assembly' };

  // teks utuh satu simpul (setelah dirapikan spasinya) -> terjemahan
  var KAMUS = {
    'Semua panduan': 'All guides', '← Semua panduan': '← All guides', '← Robot AI 4WD': '← Robot AI 4WD',
    'Mulai': 'Start', 'Mulai: rencana dan barang': 'Start: plan and parts', 'Kembali ke halaman utama robot': 'Back to the robot home page',
    '← Sebelumnya': '← Previous', 'Berikutnya →': 'Next →',
    '1. Daya': '1. Power', '2. Badan': '2. Body', '3. Otak': '3. Brain', '4. Mata': '4. Eyes', '5. Kecerdasan': '5. Intelligence', '6. Rakit akhir': '6. Final assembly',
    'Geser gambar ke samping untuk melihat semuanya.': 'Swipe the image sideways to see all of it.',
    'Setelah lulus: boleh dipatenkan atau belum?': 'After passing: lock it in, or not yet?',
    'Boleh dipatenkan': 'OK to lock in', 'Jangan dulu': 'Not yet',
    'Berhasil kalau': 'It works if', 'Kalau tidak berhasil': 'If it does not work', 'Kenapa begitu?': 'Why?',
    'Langkah': 'Step', 'Gambaran': 'Overview', 'Sebelum mulai': 'Before you start', 'Dari → ke': 'From → to', 'Panjang': 'Length', 'Ke': 'To', 'No': 'No.',
    'Salin': 'Copy', 'Tersalin': 'Copied', 'Tekan Ctrl+C': 'Press Ctrl+C', 'Unduh': 'Download', 'Buka': 'Open', 'Buka panduan': 'Open guide',
    'Barang': 'Items', 'Yang sudah kamu punya, dan yang masih kurang': 'What you have, and what is still missing',
    'belum mulai': 'not started', 'Mode tebak nama': 'Guess-the-name mode',
    'Ketuk kartu untuk melihat nama': 'Tap a card to see its name',
    'Kode sudah dikompilasi untuk ESP32 core 3.3.12, tapi robot lengkapnya belum diuji di perangkat nyata. Cabut daya sebelum mengubah kabel.':
      'The code compiles for ESP32 core 3.3.12, but the complete robot has not been tested on real hardware yet. Unplug power before changing wires.',
    'Dibuat untuk belajar. Kode sudah dikompilasi, robot lengkapnya belum diuji di perangkat nyata.':
      'Made for learning. The code compiles, but the complete robot has not been tested on real hardware yet.',
    'Dibuat untuk belajar. Cek ulang setiap sambungan kabel sebelum mencolok USB.': 'Made for learning. Re-check every wire before plugging in USB.',
    'Sumber di GitHub': 'Source on GitHub',
    'Sambung ke': 'Connect to',
    '4 gelang': '4 bands',
    '5 gelang': '5 bands'
  };

  var POLA = [
    [/^Tahap (\d+)$/, 'Stage $1'],
    [/^ke ([A-Z0-9_ ,+\-]+)$/, 'to $1'],
    [/^(\d+) \/ (\d+) langkah$/, '$1 / $2 steps'],
    [/^(\d+)\/(\d+) langkah$/, '$1/$2 steps'],
    [/^(\d+) komponen$/, '$1 items'],
    [/^Ketuk kartu untuk melihat nama · (\d+)\/(\d+) dibuka$/, 'Tap a card to see its name · $1/$2 revealed'],
    [/^Bagian (\d+): (.+)$/, function (m, n, nama) { return 'Part ' + n + ': ' + (BAGIAN[nama] || nama); }],
    [/^(.*) · Bagian (\d+) dari (\d+)$/, '$1 · Part $2 of $3']
  ];

  function terjemah(t) {
    if (Object.prototype.hasOwnProperty.call(KAMUS, t)) return KAMUS[t];
    for (var i = 0; i < POLA.length; i++) if (POLA[i][0].test(t)) return t.replace(POLA[i][0], POLA[i][1]);
    return null;
  }

  var ABAIKAN = { SCRIPT: 1, STYLE: 1, PRE: 1, CODE: 1, TEXTAREA: 1, SVG: 1, svg: 1, NOSCRIPT: 1 };
  var diubah = []; // [simpul, teks asli, teks terjemahan]

  function simpulTeks(root, fn) {
    var w = document.createTreeWalker(root, NodeFilter.SHOW_TEXT, {
      acceptNode: function (n) {
        for (var p = n.parentNode; p && p !== document; p = p.parentNode) if (p.nodeType === 1 && ABAIKAN[p.nodeName]) return NodeFilter.FILTER_REJECT;
        return NodeFilter.FILTER_ACCEPT;
      }
    });
    var n; while ((n = w.nextNode())) fn(n);
  }

  function terapkanTeks(root) {
    simpulTeks(root, function (n) {
      var asli = n.nodeValue, t = asli.replace(/\s+/g, ' ').trim();
      if (!t) return;
      var en = terjemah(t);
      if (en === null || en === t) return;
      var hasil = asli.replace(t, en);
      // spasi di tepi dipertahankan; teks dalam simpul dirapikan hanya bila cocok persis
      if (hasil === asli) hasil = en;
      diubah.push([n, asli, hasil]);
      n.nodeValue = hasil;
    });
  }

  function terapkanAtribut(root) {
    var els = root.querySelectorAll ? root.querySelectorAll('[data-en],[data-en-alt],[data-en-aria-label],[data-en-title],[data-en-placeholder]') : [];
    if (root.nodeType === 1 && root.matches && root.matches('[data-en],[data-en-alt],[data-en-aria-label],[data-en-title],[data-en-placeholder]')) els = [root].concat([].slice.call(els));
    [].forEach.call(els, function (el) {
      if (el.hasAttribute('data-en')) {
        if (!el.hasAttribute('data-id')) el.setAttribute('data-id', el.textContent);
        el.textContent = bhs === 'en' ? el.getAttribute('data-en') : el.getAttribute('data-id');
      }
      LANG_ATTR.forEach(function (a) {
        var en = el.getAttribute('data-en-' + a);
        if (en === null) return;
        if (!el.hasAttribute('data-id-' + a)) el.setAttribute('data-id-' + a, el.getAttribute(a) || '');
        el.setAttribute(a, bhs === 'en' ? en : el.getAttribute('data-id-' + a));
      });
    });
  }

  function pulihkanTeks() {
    diubah.forEach(function (d) { if (d[0].parentNode && d[0].nodeValue === d[2]) d[0].nodeValue = d[1]; });
    diubah = [];
  }

  var judulAsli = document.title;
  function terapkanJudul() {
    if (bhs !== 'en') { document.title = judulAsli; return; }
    document.title = judulAsli.split(' · ').map(function (s) { var en = terjemah(s.trim()); return en === null ? s : en; }).join(' · ');
  }

  function teksCatatan(st) {
    if (st && st.total && st.ada) return 'English: ' + st.ada + ' of ' + st.total + ' text blocks on this page are translated; the rest is still in Indonesian.';
    return 'English: the interface, the home page and the parts list are translated. The step-by-step instructions on this page are still in Indonesian.';
  }
  function catatan(st) {
    var ada = document.querySelector('.bahasa-note');
    var penuh = document.body.getAttribute('data-terjemahan') === 'penuh' || (st && st.total > 0 && st.ada >= st.total);
    if (bhs !== 'en' || penuh) { if (ada) ada.remove(); return; }
    if (ada) { ada.textContent = teksCatatan(st); return; }
    var p = document.createElement('p');
    p.className = 'bahasa-note'; p.setAttribute('role', 'note'); p.lang = 'en';
    p.textContent = teksCatatan(st);
    var wrap = document.querySelector('.wrap') || document.body;
    var tujuan = wrap.querySelector('header') || wrap.firstChild;
    wrap.insertBefore(p, tujuan);
  }

  // katalog terjemahan isi (en.json per panduan): blok ber-data-t diganti HTML Inggrisnya, yang belum ada tetap Indonesia
  var katalog = null, katalogMuat = null;
  function muatKatalog(lanjut) {
    if (katalog) return lanjut();
    var a = document.body.getAttribute('data-katalog');
    if (!a || !window.fetch) { katalog = {}; return lanjut(); }
    if (!katalogMuat) {
      katalogMuat = fetch(new URL(a, location.href).href).then(function (r) { return r.ok ? r.json() : {}; })
        .catch(function () { return {}; }).then(function (j) { katalog = (j && typeof j === 'object') ? j : {}; });
    }
    katalogMuat.then(lanjut);
  }
  function terapkanBlok() {
    var els = document.querySelectorAll('[data-t]'), ada = 0;
    [].forEach.call(els, function (el) {
      if (el.__id === undefined) el.__id = el.innerHTML;
      var en = bhs === 'en' && katalog && katalog[el.getAttribute('data-t')];
      if (en) { ada++; if (el.innerHTML !== en) el.innerHTML = en; }
      else if (el.innerHTML !== el.__id) el.innerHTML = el.__id;
    });
    return { total: els.length, ada: ada };
  }

  var pengamat = null;
  function amati() {
    if (!window.MutationObserver) return;
    if (!pengamat) {
      pengamat = new MutationObserver(function (muts) {
        if (bhs !== 'en') return;
        pengamat.disconnect();
        muts.forEach(function (m) {
          if (m.type === 'characterData') { var n = m.target, t = n.nodeValue.replace(/\s+/g, ' ').trim(), en = t && terjemah(t); if (en && en !== t) { diubah.push([n, n.nodeValue, en]); n.nodeValue = en; } return; }
          [].forEach.call(m.addedNodes, function (n) {
            if (n.nodeType === 3) { var t = n.nodeValue.replace(/\s+/g, ' ').trim(), en = t && terjemah(t); if (en && en !== t) { diubah.push([n, n.nodeValue, en]); n.nodeValue = en; } }
            else if (n.nodeType === 1) { terapkanTeks(n); terapkanAtribut(n); }
          });
        });
        pengamat.observe(document.body, { childList: true, characterData: true, subtree: true });
      });
    }
    pengamat.observe(document.body, { childList: true, characterData: true, subtree: true });
  }

  function terapkan(umum) {
    if (pengamat) pengamat.disconnect();
    document.documentElement.setAttribute('lang', bhs);
    if (bhs !== 'en') terapkanBlok();
    pulihkanTeks();
    if (bhs === 'en') terapkanTeks(document.body);
    terapkanAtribut(document.body);
    terapkanJudul();
    label();
    if (bhs === 'en') {
      catatan(null);
      amati();
      muatKatalog(function () {
        if (bhs !== 'en') return;
        if (pengamat) pengamat.disconnect();
        var st = terapkanBlok();
        terapkanTeks(document.body); terapkanAtribut(document.body);
        catatan(st);
        amati();
        document.dispatchEvent(new CustomEvent('panduan:terjemah', { detail: st }));
      });
    } else {
      terapkanBlok(); catatan(null);
      amati();
    }
    if (umum) document.dispatchEvent(new CustomEvent('panduan:bahasa', { detail: bhs }));
  }

  var tombol = null;
  function label() {
    if (!tombol) return;
    tombol.textContent = bhs.toUpperCase();
    tombol.setAttribute('aria-label', bhs === 'en' ? 'Language: English. Tap to switch to Bahasa Indonesia' : 'Bahasa: Indonesia. Ketuk untuk ganti ke English');
    tombol.title = bhs === 'en' ? 'Switch to Bahasa Indonesia' : 'Ganti ke English';
  }

  function atur(l) {
    bhs = l === 'en' ? 'en' : 'id';
    try { localStorage.setItem(KEY, bhs); } catch (e) {}
    terapkan(true);
  }

  function pasangTombol() {
    var grup = document.querySelector('.topbar .tools');
    if (!grup || grup.querySelector('.lang-btn')) return;
    tombol = document.createElement('button');
    tombol.type = 'button'; tombol.className = 'theme-btn lang-btn';
    tombol.addEventListener('click', function () { atur(bhs === 'en' ? 'id' : 'en'); });
    grup.insertBefore(tombol, grup.querySelector('.theme-btn'));
  }

  window.panduanBahasa = {
    bahasa: function () { return bhs; },
    atur: atur,
    // teks dua bahasa untuk skrip lain: t('Sudah punya', 'I have it')
    t: function (id, en) { return bhs === 'en' ? en : id; },
    terapkan: function (root) { if (bhs === 'en') terapkanTeks(root || document.body); terapkanAtribut(root || document.body); }
  };

  function mulai() {
    pasangTombol();
    terapkan(true);
    addEventListener('storage', function (e) { if (e.key === KEY) { bhs = baca(); terapkan(true); } });
  }
  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', mulai); else mulai();
})();
