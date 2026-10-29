/* Kalkulator kecil di halaman Elektronika Dasar: hukum Ohm, resistor LED, pembagi tegangan, gelang warna resistor. */
(function () {
  function angka(s) {
    s = String(s || '').trim().replace(/\s/g, '').replace(/Ω|ohm/gi, '');
    if (!s) return null;
    var kali = 1, akhir = s.slice(-1).toLowerCase();
    if (akhir === 'k') { kali = 1e3; s = s.slice(0, -1); }
    else if (s.slice(-1) === 'M') { kali = 1e6; s = s.slice(0, -1); }
    s = s.replace(',', '.');
    var n = Number(s);
    return isFinite(n) && s !== '' ? n * kali : NaN;
  }
  function fmt(n, d) { return n.toLocaleString('id-ID', { maximumFractionDigits: d === undefined ? 2 : d }); }
  function isian(n, d) { return n.toLocaleString('id-ID', { maximumFractionDigits: d, useGrouping: false }); }
  function ohm(r) { return r >= 1e6 ? fmt(r / 1e6) + ' MΩ' : r >= 1e3 ? fmt(r / 1e3) + ' kΩ' : fmt(r) + ' Ω'; }
  function amp(a) { return a < 1 ? fmt(a * 1000, a < 0.01 ? 2 : 1) + ' mA' : fmt(a) + ' A'; }
  function watt(p) { return p < 1 ? fmt(p * 1000, 1) + ' mW' : fmt(p) + ' W'; }
  function tulis(out, html, salah) { out.innerHTML = html; out.classList.toggle('salah', !!salah); }
  var E12 = [1.0, 1.2, 1.5, 1.8, 2.2, 2.7, 3.3, 3.9, 4.7, 5.6, 6.8, 8.2];
  function standarAtas(r) {
    var dek = Math.pow(10, Math.floor(Math.log10(r)));
    for (var k = 0; k < 3; k++) {
      for (var i = 0; i < E12.length; i++) { var v = E12[i] * dek * Math.pow(10, k); if (v >= r - 1e-9) return Math.round(v * 100) / 100; }
    }
    return r;
  }

  // Hukum Ohm
  document.querySelectorAll('[data-kalk="ohm"]').forEach(function (k) {
    var f = function (n) { return k.querySelector('[name="' + n + '"]'); };
    var out = k.querySelector('output');
    k.querySelector('[data-reset]').addEventListener('click', function () { ['v', 'i', 'r'].forEach(function (n) { f(n).value = ''; }); tulis(out, ''); });
    k.querySelector('[data-hitung]').addEventListener('click', function () {
      var v = angka(f('v').value), i = angka(f('i').value), r = angka(f('r').value);
      var kosong = [v, i, r].filter(function (x) { return x === null; }).length;
      if ([v, i, r].some(function (x) { return x !== null && (isNaN(x) || x <= 0); })) return tulis(out, 'Isi angka yang lebih besar dari nol.', true);
      if (kosong !== 1) return tulis(out, 'Kosongkan tepat satu kotak, yang ingin dihitung.', true);
      if (i !== null) i = i / 1000;
      if (v === null) { v = i * r; f('v').value = isian(v, 3); }
      else if (i === null) { i = v / r; f('i').value = isian(i * 1000, 3); }
      else { r = v / i; f('r').value = isian(r, 1); }
      tulis(out, 'V = <b>' + fmt(v, 3) + ' V</b>, I = <b>' + amp(i) + '</b>, R = <b>' + ohm(r) + '</b>. Daya yang berubah jadi panas: <b>' + watt(v * i) + '</b>.');
    });
  });

  // Resistor LED
  document.querySelectorAll('[data-kalk="led"]').forEach(function (k) {
    var out = k.querySelector('output');
    function hitung() {
      var vs = angka(k.querySelector('[name="vs"]').value), vf = Number(k.querySelector('[name="vf"]').value), i = angka(k.querySelector('[name="i"]').value);
      if (vs === null || i === null || isNaN(vs) || isNaN(i) || i <= 0) return tulis(out, 'Isi tegangan sumber dan arus LED.', true);
      if (vs <= vf) return tulis(out, 'Tegangan sumber harus lebih tinggi dari Vf LED (' + fmt(vf, 1) + ' V), kalau tidak LED tidak menyala.', true);
      i = i / 1000;
      var r = (vs - vf) / i, rs = standarAtas(r), ia = (vs - vf) / rs, p = ia * ia * rs;
      tulis(out, 'Hitungan: (' + fmt(vs, 2) + ' − ' + fmt(vf, 1) + ') / ' + amp(i) + ' = <b>' + ohm(r) + '</b>. Pakai nilai standar <b>' + ohm(rs) +
        '</b>: arus jadi ' + amp(ia) + ', panas di resistor ' + watt(p) + (p > 0.125 ? ' (lebih dari setengah batas resistor 1/4 W; pakai resistor 1/2 W).' : ' (aman untuk resistor 1/4 W).'));
    }
    k.addEventListener('input', hitung); k.addEventListener('change', hitung); hitung();
  });

  // Pembagi tegangan
  document.querySelectorAll('[data-kalk="pembagi"]').forEach(function (k) {
    var out = k.querySelector('output');
    function hitung() {
      var vin = angka(k.querySelector('[name="vin"]').value), r1 = angka(k.querySelector('[name="r1"]').value), r2 = angka(k.querySelector('[name="r2"]').value);
      if ([vin, r1, r2].some(function (x) { return x === null || isNaN(x) || x <= 0; })) return tulis(out, 'Isi ketiga kotak dengan angka lebih dari nol.', true);
      var vo = vin * r2 / (r1 + r2), i = vin / (r1 + r2);
      var cat = vo > 3.6 ? ' Terlalu tinggi untuk pin ESP32 (batasnya sekitar 3,6 V).' : vo > 3.3 ? ' Sedikit di atas 3,3 V: masih di bawah batas 3,6 V, tapi tipis.' : ' Aman untuk pin ESP32.';
      tulis(out, 'Vout = ' + fmt(vin, 2) + ' × ' + ohm(r2) + ' / (' + ohm(r1) + ' + ' + ohm(r2) + ') = <b>' + fmt(vo, 2) + ' V</b>. Arus lewat pembagi ' + amp(i) + '.' + cat, vo > 3.6);
    }
    k.addEventListener('input', hitung); hitung();
  });

  // Umur baterai
  document.querySelectorAll('[data-kalk="baterai"]').forEach(function (k) {
    var out = k.querySelector('output');
    function v(n) { return angka(k.querySelector('[name="' + n + '"]').value); }
    function hitung() {
      var cap = v('cap'), ia = v('ia'), ta = v('ta'), is = v('is'), per = v('per');
      if ([cap, ia, ta, is, per].some(function (x) { return x === null || isNaN(x) || x < 0; }) || cap <= 0 || per <= 0) return tulis(out, 'Isi semua kotak dengan angka (boleh 0 untuk arus tidur).', true);
      var T = per * 60, ts = T - ta;
      if (ts < 0) return tulis(out, 'Lama aktif lebih panjang dari satu siklus.', true);
      var muatanAktif = ia * ta, muatanTidur = is * ts, avg = (muatanAktif + muatanTidur) / T;
      var jam = cap / avg, nyata = jam * 0.7;
      var lama = function (h) { return h >= 48 ? fmt(h / 24, 0) + ' hari' : fmt(h, 1) + ' jam'; };
      tulis(out, 'Arus rata-rata <b>' + fmt(avg, 3) + ' mA</b> (' + fmt(muatanTidur / (muatanAktif + muatanTidur) * 100, 0) + '% di antaranya terpakai saat tidur). ' +
        'Hitungan kasar ' + lama(jam) + '; perkiraan yang lebih jujur, dikali 0,7 untuk regulator, suhu, dan baterai yang tidak bisa dikosongkan penuh: <b>' + lama(nyata) + '</b>.');
    }
    k.addEventListener('input', hitung); hitung();
  });

  // Simulasi hukum Ohm bergerak
  document.querySelectorAll('[data-kalk="hidup"]').forEach(function (k) {
    var NS = 'http://www.w3.org/2000/svg';
    var sv = k.querySelector('[name="v"]'), sr = k.querySelector('[name="r"]'), out = k.querySelector('output');
    var g = k.querySelector('[data-titik]'), res = k.querySelector('[data-res]'), cahaya = k.querySelector('[data-cahaya]');
    var N = 30, L = 1400, titik = [];
    for (var i = 0; i < N; i++) {
      var c = document.createElementNS(NS, 'circle');
      c.setAttribute('r', '5'); c.setAttribute('fill', '#ffd166'); g.appendChild(c); titik.push(c);
    }
    function posisi(s) {   // keliling persegi panjang searah jarum jam dari pojok kiri atas
      s = ((s % L) + L) % L;
      if (s < 520) return [120 + s, 60];
      if (s < 700) return [640, 60 + (s - 520)];
      if (s < 1220) return [640 - (s - 700), 240];
      return [120, 240 - (s - 1220)];
    }
    var geser = 0, laju = 0, diam = window.matchMedia && matchMedia('(prefers-reduced-motion: reduce)').matches;
    function terdekatE12(x) {   // nilai resistor standar terdekat, seperti yang dijual di toko
      var dek = Math.pow(10, Math.floor(Math.log10(x))), m = x / dek, best = 1, d = 99;
      E12.concat([10]).forEach(function (e) { var dd = Math.abs(Math.log(m / e)); if (dd < d) { d = dd; best = e; } });
      return Math.round(best * dek);
    }
    function campur(a, b, t) { return Math.round(a + (b - a) * t); }
    function hitung() {
      var v = Number(sv.value), r = terdekatE12(10 * Math.pow(10, 3 * Number(sr.value) / 100));
      var i = v / r, p = v * i;
      k.querySelector('[data-lv]').textContent = fmt(v, 1) + ' V';
      k.querySelector('[data-lr]').textContent = ohm(r);
      k.querySelector('[data-tv]').textContent = fmt(v, 1) + ' V';
      k.querySelector('[data-tr]').textContent = ohm(r);
      k.querySelector('[data-ti]').textContent = amp(i);
      laju = i <= 0 ? 0 : Math.max(6, Math.min(320, 40 + 75 * Math.log10(i / 0.001)));
      var panas = Math.min(1, p / 1.5);
      res.setAttribute('fill', 'rgb(' + campur(127, 255, panas) + ',' + campur(176, 90, panas) + ',' + campur(221, 42, panas) + ')');
      cahaya.setAttribute('opacity', (panas * 0.55).toFixed(2));
      var cat = v === 0 ? 'Tanpa tegangan tidak ada dorongan, jadi tidak ada arus.' :
        p > 0.25 ? 'Lebih dari 0,25 W: resistor 1/4 W biasa akan terlalu panas dan bisa terbakar. Pakai resistor berdaya lebih besar atau hambatan lebih besar.' :
        'Aman untuk resistor 1/4 W.';
      tulis(out, 'I = V / R = ' + fmt(v, 1) + ' V / ' + ohm(r) + ' = <b>' + amp(i) + '</b>. Panas di resistor P = V × I = <b>' + watt(p) + '</b>. ' + cat, p > 0.25);
      if (diam) gambar();
    }
    function gambar() {
      for (var j = 0; j < N; j++) { var q = posisi(geser + j * L / N); titik[j].setAttribute('cx', q[0]); titik[j].setAttribute('cy', q[1]); }
    }
    var sebelum = null;
    function jalan(t) {
      if (sebelum !== null) geser += laju * Math.min(0.05, (t - sebelum) / 1000);
      sebelum = t; gambar(); requestAnimationFrame(jalan);
    }
    sv.addEventListener('input', hitung); sr.addEventListener('input', hitung);
    hitung(); gambar();
    if (!diam) requestAnimationFrame(jalan);
  });

  // Gelang warna resistor
  var WARNA = [
    ['hitam', '#1b1b1b', 0, 1, null], ['coklat', '#7a4a1d', 1, 10, 1], ['merah', '#d7261e', 2, 100, 2], ['oranye', '#f08a24', 3, 1e3, null],
    ['kuning', '#f2c230', 4, 1e4, null], ['hijau', '#2e9e4a', 5, 1e5, 0.5], ['biru', '#2f6fd6', 6, 1e6, 0.25], ['ungu', '#8a4fd1', 7, 1e7, 0.1],
    ['abu-abu', '#8f9897', 8, 1e8, 0.05], ['putih', '#f4f4f4', 9, 1e9, null], ['emas', '#c9a227', null, 0.1, 5], ['perak', '#c0c4c8', null, 0.01, 10]];
  function opsi(sel, jenis, pilih) {
    sel.innerHTML = WARNA.map(function (w, idx) {
      var boleh = jenis === 'angka' ? w[2] !== null : jenis === 'kali' ? true : w[4] !== null;
      return boleh ? '<option value="' + idx + '"' + (w[0] === pilih ? ' selected' : '') + '>' + w[0] + '</option>' : '';
    }).join('');
  }
  document.querySelectorAll('[data-kalk="warna"]').forEach(function (k) {
    var out = k.querySelector('output'), nSel = k.querySelector('[name="n"]');
    var sel = [0, 1, 2, 3, 4].map(function (i) { return k.querySelector('[name="g' + i + '"]'); });
    var lab = [0, 1, 2, 3, 4].map(function (i) { return k.querySelector('label[data-g="' + i + '"]'); });
    var gl = [0, 1, 2, 3, 4].map(function (i) { return k.querySelector('rect.gl[data-g="' + i + '"]'); });
    function susun() {
      var n = Number(nSel.value);
      var awal = n === 5 ? ['coklat', 'hitam', 'hitam', 'coklat', 'coklat'] : ['merah', 'merah', 'coklat', 'emas'];
      var jenis = n === 5 ? ['angka', 'angka', 'angka', 'kali', 'tol'] : ['angka', 'angka', 'kali', 'tol'];
      var nama = n === 5 ? ['Angka 1', 'Angka 2', 'Angka 3', 'Pengali', 'Toleransi'] : ['Angka 1', 'Angka 2', 'Pengali', 'Toleransi'];
      var posisi = n === 5 ? [58, 80, 102, 124, 168] : [62, 86, 110, 168];
      for (var i = 0; i < 5; i++) {
        var ada = i < n;
        lab[i].hidden = !ada; gl[i].style.display = ada ? '' : 'none';
        if (ada) { lab[i].firstChild.textContent = nama[i]; opsi(sel[i], jenis[i], awal[i]); gl[i].setAttribute('x', posisi[i]); }
      }
      hitung();
    }
    function hitung() {
      var n = Number(nSel.value), w = sel.slice(0, n).map(function (s) { return WARNA[Number(s.value)]; });
      w.forEach(function (x, i) { gl[i].setAttribute('fill', x[1]); });
      var digit = w.slice(0, n - 2).reduce(function (a, x) { return a * 10 + x[2]; }, 0);
      var r = digit * w[n - 2][3], tol = w[n - 1][4];
      tulis(out, w.map(function (x) { return x[0]; }).join(' - ') + ' = <b>' + ohm(Math.round(r * 1000) / 1000) + ' ±' + fmt(tol, 2) + '%</b>');
    }
    nSel.addEventListener('change', susun);
    sel.forEach(function (s) { s.addEventListener('change', hitung); });
    susun();
  });
})();
