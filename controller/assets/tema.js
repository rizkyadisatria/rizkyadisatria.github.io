/* Mode terang/gelap untuk semua halaman panduan.
   Dimuat di <head> (tanpa defer) supaya tema terpasang sebelum halaman tergambar.
   Pilihan disimpan di browser: otomatis (ikut sistem) -> terang -> gelap. */
(function () {
  var KEY = 'panduan-tema';
  var root = document.documentElement;
  var me = document.currentScript;
  var hub = me ? new URL('../', me.src).href : '/';
  function baca() { try { return localStorage.getItem(KEY) || 'auto'; } catch (e) { return 'auto'; } }
  function simpan(v) { try { if (v === 'auto') localStorage.removeItem(KEY); else localStorage.setItem(KEY, v); } catch (e) {} }
  function gelap(m) { return m === 'dark' || (m === 'auto' && window.matchMedia && matchMedia('(prefers-color-scheme: dark)').matches); }
  function pasang(m) {
    if (m === 'auto') root.removeAttribute('data-theme'); else root.setAttribute('data-theme', m);
    var meta = document.querySelector('meta[name="theme-color"]');
    if (meta) meta.setAttribute('content', gelap(m) ? '#0b0b0c' : '#fafaf9');
  }
  var mode = baca();
  pasang(mode);

  var IKON = {
    auto: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" aria-hidden="true"><circle cx="12" cy="12" r="8"/><path d="M12 4a8 8 0 0 1 0 16z" fill="currentColor"/></svg>',
    light: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" aria-hidden="true"><circle cx="12" cy="12" r="4"/><path d="M12 2v2M12 20v2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M2 12h2M20 12h2M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4"/></svg>',
    dark: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linejoin="round" aria-hidden="true"><path d="M20 14.5A8 8 0 1 1 9.5 4a6.5 6.5 0 0 0 10.5 10.5z"/></svg>'
  };
  var NAMA = { auto: 'Otomatis', light: 'Terang', dark: 'Gelap' };
  var URUT = { auto: 'light', light: 'dark', dark: 'auto' };

  function bilah() {
    if (document.querySelector('.topbar')) return;
    var wrap = document.querySelector('.wrap');
    var bar = document.createElement('div');
    bar.className = 'topbar';
    var lebar = wrap ? getComputedStyle(wrap).maxWidth : '980px';
    bar.innerHTML = '<div class="in" style="max-width:' + lebar + '"><a class="brand" href="' + hub + '"><i></i>Panduan Elektronika</a>' +
      '<button type="button" class="theme-btn"></button></div>';
    document.body.insertBefore(bar, document.body.firstChild);
    var btn = bar.querySelector('button');
    function label() {
      btn.innerHTML = IKON[mode] + '<span>' + NAMA[mode] + '</span>';
      btn.setAttribute('aria-label', 'Tema: ' + NAMA[mode] + '. Ketuk untuk ganti ke ' + NAMA[URUT[mode]]);
      btn.title = 'Ganti ke ' + NAMA[URUT[mode]];
    }
    btn.addEventListener('click', function () { mode = URUT[mode]; simpan(mode); pasang(mode); label(); });
    label();
    var on = false;
    function gulir() { var s = window.scrollY > 4; if (s !== on) { on = s; bar.classList.toggle('scrolled', s); } }
    addEventListener('scroll', gulir, { passive: true }); gulir();
  }
  if (window.matchMedia) {
    var mq = matchMedia('(prefers-color-scheme: dark)');
    var ubah = function () { if (mode === 'auto') pasang('auto'); };
    if (mq.addEventListener) mq.addEventListener('change', ubah); else if (mq.addListener) mq.addListener(ubah);
  }
  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', bilah); else bilah();
})();
