/* Perilaku bersama halaman panduan: centang langkah (tersimpan di browser), progres, tombol salin, mode tebak nama. */
(function () {
  var key = document.body.getAttribute('data-key') || location.pathname;
  var store = {};
  try { store = JSON.parse(localStorage.getItem(key) || '{}'); } catch (e) { store = {}; }
  function save() { try { localStorage.setItem(key, JSON.stringify(store)); } catch (e) {} }
  var boxes = Array.prototype.slice.call(document.querySelectorAll('ol.steps input[type=checkbox]'));
  boxes.forEach(function (b) { if (store[b.id]) b.checked = true; });
  function refresh() {
    var done = boxes.filter(function (b) { return b.checked; }).length;
    var p = document.getElementById('prog'), bar = document.getElementById('progbar');
    if (p) p.textContent = done + ' / ' + boxes.length + ' langkah';
    if (bar) bar.style.width = (boxes.length ? done / boxes.length * 100 : 0) + '%';
    document.querySelectorAll('.stage').forEach(function (st) {
      var b = st.querySelectorAll('ol.steps input'), d = st.querySelectorAll('ol.steps input:checked').length;
      var c = st.querySelector('[data-count]');
      if (c) c.textContent = d + '/' + b.length;
      st.classList.toggle('done', b.length > 0 && d === b.length);
    });
    try { localStorage.setItem(key + ':ringkas', JSON.stringify({ d: done, n: boxes.length })); } catch (e) {}
  }
  document.addEventListener('change', function (e) {
    if (e.target.matches && e.target.matches('ol.steps input[type=checkbox]')) { store[e.target.id] = e.target.checked; save(); refresh(); }
  });
  document.addEventListener('click', function (e) {
    var b = e.target.closest && e.target.closest('.copy');
    if (!b) return;
    var pre = b.parentNode.querySelector('pre');
    var txt = pre.textContent;
    function ok() { b.textContent = 'Tersalin'; setTimeout(function () { b.textContent = 'Salin'; }, 1500); }
    function pilih() { var r = document.createRange(); r.selectNodeContents(pre); var s = getSelection(); s.removeAllRanges(); s.addRange(r); b.textContent = 'Tekan Ctrl+C'; }
    try { navigator.clipboard.writeText(txt).then(ok, pilih); } catch (err) { pilih(); }
  });
  // mode tebak nama di galeri komponen
  var qb = document.getElementById('quizbtn'), grid = document.getElementById('kit');
  if (qb && grid) {
    var hint = document.getElementById('quizhint');
    function upd() {
      var n = grid.querySelectorAll('.card').length;
      if (!grid.classList.contains('quiz')) { hint.textContent = n + ' komponen'; return; }
      hint.textContent = 'Ketuk kartu untuk melihat nama · ' + grid.querySelectorAll('.card.shown').length + '/' + n + ' dibuka';
    }
    qb.addEventListener('click', function () {
      var on = !grid.classList.contains('quiz'); grid.classList.toggle('quiz', on); qb.setAttribute('aria-pressed', on);
      grid.querySelectorAll('.card').forEach(function (c) { c.classList.remove('shown'); }); upd();
    });
    grid.addEventListener('click', function (e) { var c = e.target.closest('.card'); if (c && grid.classList.contains('quiz')) { c.classList.toggle('shown'); upd(); } });
    grid.addEventListener('keydown', function (e) { if ((e.key === 'Enter' || e.key === ' ') && e.target.classList.contains('card')) { e.preventDefault(); e.target.click(); } });
    upd();
  }
  // progres tiap bagian di halaman utama robot
  document.querySelectorAll('[data-progres]').forEach(function (el) {
    try {
      var r = JSON.parse(localStorage.getItem(el.getAttribute('data-progres') + ':ringkas') || 'null');
      el.textContent = r ? (r.d + '/' + r.n + ' langkah') : 'belum mulai';
    } catch (e) { el.textContent = ''; }
  });
  refresh();
})();
