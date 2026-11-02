/* Mode terang/gelap untuk semua halaman panduan.
   Dimuat di <head> (tanpa defer) supaya tema terpasang sebelum halaman tergambar.
   Pilihan disimpan di browser: otomatis (ikut sistem) -> terang -> gelap.
   Juga membuat bilah atas dan memuat bahasa.js (ID/EN) dan cadangan.js (ekspor/impor progres). */
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
  var NAMA_ID = { auto: 'Otomatis', light: 'Terang', dark: 'Gelap' };
  var NAMA_EN = { auto: 'Auto', light: 'Light', dark: 'Dark' };
  var URUT = { auto: 'light', light: 'dark', dark: 'auto' };
  function en() { try { return localStorage.getItem('lang') === 'en'; } catch (e) { return false; } }
  function nama(m) { return (en() ? NAMA_EN : NAMA_ID)[m]; }
  // bahasa.js dan cadangan.js ada di folder yang sama dengan berkas ini
  function muat(nm) {
    if (!me) return;
    var s = document.createElement('script');
    s.src = new URL(nm, me.src).href; s.async = false;
    document.head.appendChild(s);
  }

  function bilah() {
    if (document.querySelector('.topbar')) return;
    var wrap = document.querySelector('.wrap');
    var bar = document.createElement('div');
    bar.className = 'topbar';
    var lebar = wrap ? getComputedStyle(wrap).maxWidth : '980px';
    bar.innerHTML = '<div class="in" style="max-width:' + lebar + '"><a class="brand" href="' + hub + '"><i></i>Panduan Elektronika</a>' +
      '<div class="tools"><button type="button" class="theme-btn"></button></div></div>';
    document.body.insertBefore(bar, document.body.firstChild);
    var btn = bar.querySelector('.theme-btn');
    function label() {
      btn.innerHTML = IKON[mode] + '<span>' + nama(mode) + '</span>';
      btn.setAttribute('aria-label', en() ? 'Theme: ' + nama(mode) + '. Tap to switch to ' + nama(URUT[mode]) : 'Tema: ' + nama(mode) + '. Ketuk untuk ganti ke ' + nama(URUT[mode]));
      btn.title = (en() ? 'Switch to ' : 'Ganti ke ') + nama(URUT[mode]);
    }
    btn.addEventListener('click', function () { mode = URUT[mode]; simpan(mode); pasang(mode); label(); });
    document.addEventListener('panduan:bahasa', label);
    label();
    muat('bahasa.js'); muat('cadangan.js');
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
