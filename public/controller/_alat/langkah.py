"""Hitung daftar langkah (kotak centang) tiap panduan untuk label progres otomatis di halaman utama.

Hasilnya <slug>/langkah.json: {"total": N, "halaman": [{"kunci": "<kunci localStorage>", "id": [...]}, ...]}
Halaman utama membaca centang yang tersimpan di browser pembaca dengan kunci itu, lalu menulis
"Belum mulai", "Sedang dikerjakan", atau "Selesai" sendiri. Tidak ada status yang ditulis manual.

Robot AI: dibuat otomatis oleh robot-ai/_build/build.py (tulis_langkah di file ini).
Panduan yang langkahnya dibuat JavaScript (stasiun cuaca): jalankan
    python3 controller/_alat/langkah.py stasiun-cuaca
setelah mengubah langkahnya. Butuh Playwright (sudah terpasang di lingkungan build).
"""
import json, os, re, sys, threading, functools, http.server

CTRL = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))


def tulis(slug, halaman):
    data = {'total': sum(len(h['id']) for h in halaman), 'halaman': halaman}
    with open(os.path.join(CTRL, slug, 'langkah.json'), 'w', encoding='utf-8') as f:
        json.dump(data, f, ensure_ascii=False, separators=(',', ':'))
    return data['total']


def tulis_langkah(slug, files):
    """Untuk halaman statis: baca data-key di <body> dan id kotak centang di <ol class="steps">."""
    halaman = []
    for fp in files:
        s = open(fp, encoding='utf-8').read()
        m = re.search(r'<body[^>]*data-key="([^"]+)"', s)
        ids = re.findall(r'<li><input type="checkbox" id="([^"]+)">', s)
        if m and ids:
            halaman.append({'kunci': m.group(1), 'id': ids})
    return tulis(slug, halaman)


def dari_browser(slug):
    """Untuk halaman yang langkahnya dibuat JavaScript: buka di browser, cari kunci penyimpanan dengan mencentang satu kotak."""
    from playwright.sync_api import sync_playwright
    root = os.path.dirname(CTRL)
    class Diam(http.server.SimpleHTTPRequestHandler):
        def log_message(self, *a): pass
    h = functools.partial(Diam, directory=root)
    srv = http.server.ThreadingHTTPServer(('127.0.0.1', 0), h)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    url = f'http://127.0.0.1:{srv.server_port}/controller/{slug}/'
    with sync_playwright() as p:
        b = p.chromium.launch(); pg = b.new_page()
        pg.route(re.compile(r'^https?://(?!127\.0\.0\.1)'), lambda r: r.abort())
        pg.goto(url); pg.wait_for_selector('ol.steps input[type=checkbox]')
        ids = pg.eval_on_selector_all('ol.steps input[type=checkbox]', 'els => els.map(e => e.id)')
        pg.evaluate('localStorage.clear()')
        pg.click(f'label[for="{ids[0]}"]')
        kunci = pg.evaluate('''id => Object.keys(localStorage).find(k => { try { return id in JSON.parse(localStorage.getItem(k)) } catch (e) { return false } })''', ids[0])
        b.close()
    srv.shutdown()
    if not kunci:
        sys.exit('Kunci penyimpanan tidak ditemukan')
    return tulis(slug, [{'kunci': kunci, 'id': ids}])


if __name__ == '__main__':
    for slug in sys.argv[1:]:
        print(slug, dari_browser(slug), 'langkah')
