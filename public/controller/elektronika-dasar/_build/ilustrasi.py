# Ilustrasi perumpamaan (air, katup, keran) untuk konsep dasar. Titik air bergerak dengan animasi CSS (kelas .alir),
# yang berhenti otomatis kalau pembaca memilih "kurangi gerak" di sistemnya.
from gambar import svg, text, WIRE, MONO, KUNING_ARUS, MERAH_SALAH, HIJAU_OK, KAWAT

AIR1, AIR2 = '#5bb4ff', '#1f6fd1'
DINDING = '#dfe7ec'
_n = [0]


def _id(p):
    _n[0] += 1
    return f'{p}{_n[0]}'


def gradien_air(gid):
    return (f'<defs><linearGradient id="{gid}" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="{AIR1}"/>'
            f'<stop offset="1" stop-color="{AIR2}"/></linearGradient></defs>')


def tangki(x, y, w, h, air_y, gid, label='', sub='', kiri=None, kanan=None):
    """Tangki terbuka dari y sampai y+h, air dari air_y ke dasar. kiri/kanan = (y_tengah, tinggi) lubang pipa di dinding itu."""
    s = f'<rect x="{x}" y="{air_y}" width="{w}" height="{y + h - air_y}" fill="url(#{gid})"/>'

    def dinding(xx, lub):
        if not lub:
            return f'M{xx} {y} V{y + h}'
        yc, hh = lub
        return f'M{xx} {y} V{yc - hh / 2} M{xx} {yc + hh / 2} V{y + h}'
    s += (f'<path d="{dinding(x, kiri)} M{x} {y + h} H{x + w} {dinding(x + w, kanan)}" fill="none" stroke="{DINDING}" '
          f'stroke-width="5" stroke-linecap="round"/>')
    s += f'<line x1="{x + 4}" y1="{air_y}" x2="{x + w - 4}" y2="{air_y}" stroke="#bfe3ff" stroke-width="2"/>'
    if label:
        s += text(x + w / 2, y - 12, label, 13, anchor='middle', weight=700)
    if sub:
        s += text(x + w / 2, y - 30, sub, 11.5, anchor='middle', fill='#cfd8d4')
    return s


def pipa(x1, x2, yc, h, gid):
    """Pipa mendatar berisi air (latar biru tua), dinding atas dan bawah."""
    t, b = yc - h / 2, yc + h / 2
    return (f'<rect x="{x1}" y="{t}" width="{x2 - x1}" height="{h}" fill="url(#{gid})" opacity=".55"/>'
            f'<line x1="{x1}" y1="{t}" x2="{x2}" y2="{t}" stroke="{DINDING}" stroke-width="4"/>'
            f'<line x1="{x1}" y1="{b}" x2="{x2}" y2="{b}" stroke="{DINDING}" stroke-width="4"/>')


def aliran(x1, x2, yc, h, jarak, durasi, r=5, warna='#e8f6ff', arah=1, lambat_label=None):
    """Titik-titik yang bergeser satu jarak tiap durasi. Dipotong (clip) di dalam x1..x2 supaya alirannya terlihat menerus."""
    cid = _id('clip')
    s = f'<defs><clipPath id="{cid}"><rect x="{x1}" y="{yc - h / 2}" width="{x2 - x1}" height="{h}"/></clipPath></defs><g clip-path="url(#{cid})">'
    x = x1 - jarak
    while x <= x2 + jarak:
        s += (f'<circle class="alir" cx="{x:.1f}" cy="{yc}" r="{r}" fill="{warna}" '
              f'style="--g:{arah * jarak}px;--d:{durasi}s"/>')
        x += jarak
    return s + '</g>'


def tegangan():
    W, H = 760, 380
    g = _id('air')
    s = gradien_air(g) + text(30, 32, 'Tegangan seperti beda ketinggian air', 16, weight=600)
    s += tangki(60, 70, 160, 200, 96, g, 'kutub + (tinggi)', kanan=(250, 26))
    s += tangki(540, 150, 160, 120, 226, g, 'kutub − (rendah)', kiri=(250, 26))
    s += pipa(220, 540, 250, 26, g)
    s += aliran(220, 540, 250, 22, 30, 0.6)
    s += f'<path d="M224 96 H470" stroke="#fff" stroke-opacity=".55" stroke-width="1.5" stroke-dasharray="6 5"/><path d="M470 226 H540" stroke="#fff" stroke-opacity=".55" stroke-width="1.5" stroke-dasharray="6 5"/>'
    s += f'<line x1="470" y1="100" x2="470" y2="222" stroke="{KUNING_ARUS}" stroke-width="3"/>'
    s += f'<polygon points="470,96 463,110 477,110" fill="{KUNING_ARUS}"/><polygon points="470,226 463,212 477,212" fill="{KUNING_ARUS}"/>'
    s += text(456, 152, 'beda tinggi', 13, anchor='end', weight=700, fill=KUNING_ARUS) + text(456, 170, '= tegangan (V)', 13, anchor='end', weight=700, fill=KUNING_ARUS)
    s += text(380, 284, 'air mengalir dari tinggi ke rendah', 12, anchor='middle', fill='#e8efe9')
    s += text(30, 330, 'Yang membuat air mengalir adalah beda tinggi di antara dua tangki, bukan tinggi satu tangki saja.', 12.5, fill='#e8efe9')
    s += text(30, 352, 'Sama dengan listrik: tegangan selalu antara dua titik. Kalau dua titik sama tingginya (0 V beda), tidak ada aliran.', 12.5, fill='#ffd8a8')
    return svg(W, H, s, 'Dua tangki air dengan ketinggian berbeda yang terhubung pipa, sebagai perumpamaan tegangan')


def arus():
    W, H = 760, 400
    g = _id('air')
    s = gradien_air(g) + text(30, 32, 'Arus seperti banyaknya air yang lewat tiap detik', 16, weight=600)
    for k, (y, jarak, dur, a, b) in enumerate(((100, 70, 1.6, 'arus kecil', 'sedikit yang lewat tiap detik'), (180, 22, 0.28, 'arus besar', 'banyak yang lewat tiap detik'))):
        s += pipa(60, 560, y, 34, g) + aliran(60, 560, y, 30, jarak, dur, 6)
        s += text(580, y - 4, a, 14, weight=700, fill=KUNING_ARUS if k else '#e8efe9') + text(580, y + 14, b, 11.5, fill='#cfd8d4')
    s += '<line x1="400" y1="68" x2="400" y2="212" stroke="#fff" stroke-width="2" stroke-dasharray="4 4"/>' + text(400, 60, 'titik hitung', 11.5, anchor='middle', fill='#cfd8d4')
    # kabel dan elektron
    s += text(30, 262, 'Di kabel, yang bergerak adalah elektron', 13.5, weight=700)
    s += f'<rect x="60" y="282" width="500" height="34" rx="17" fill="#c98a3c" stroke="#7a4a1d" stroke-width="2"/>'
    cid = _id('clip')
    s += f'<defs><clipPath id="{cid}"><rect x="64" y="284" width="492" height="30" rx="15"/></clipPath></defs><g clip-path="url(#{cid})">'
    x = 40
    while x <= 600:
        s += f'<g class="alir" style="--g:-36px;--d:0.9s"><circle cx="{x}" cy="299" r="7" fill="#8ec5ff" stroke="#1f4e79" stroke-width="1.5"/>'
        s += f'<line x1="{x - 3.5}" y1="299" x2="{x + 3.5}" y2="299" stroke="#1f4e79" stroke-width="2"/></g>'
        x += 36
    s += '</g>'
    s += text(580, 296, '← elektron (−)', 12, fill='#8ec5ff', weight=600) + text(580, 314, 'arah arus (+ ke −) →', 12, fill=KUNING_ARUS, weight=600)
    s += text(30, 352, 'Elektron bergerak dari − ke +. Gambar rangkaian memakai arah sebaliknya (dari + ke −) sebagai kesepakatan;', 12.5, fill='#e8efe9')
    s += text(30, 374, 'untuk merakit cukup pakai arah + ke −. Lampu menyala seketika karena dorongannya merambat sangat cepat.', 12.5, fill='#e8efe9')
    return svg(W, H, s, 'Dua pipa dengan aliran sedikit dan banyak, dan kabel dengan elektron bergerak, sebagai perumpamaan arus')


def hambatan():
    W, H = 760, 400
    g = _id('air')
    s = gradien_air(g) + text(30, 32, 'Hambatan seperti pipa yang menyempit', 16, weight=600)
    for k, y in enumerate((120, 260)):
        s += tangki(40, y - 70, 80, 100, y - 46, g, kanan=(y + 10, 30))
        if k == 0:
            s += pipa(120, 600, y + 10, 30, g) + aliran(120, 600, y + 10, 26, 22, 0.25, 5.5)
            s += text(616, y + 2, 'hambatan kecil', 14, weight=700, fill=HIJAU_OK) + text(616, y + 20, '→ arus besar', 12, fill='#e8efe9')
        else:
            yc = y + 10
            s += pipa(120, 290, yc, 30, g) + pipa(290, 430, yc, 10, g) + pipa(430, 600, yc, 30, g)
            s += f'<path d="M290 {yc - 15} V{yc - 5} M290 {yc + 5} V{yc + 15} M430 {yc - 15} V{yc - 5} M430 {yc + 5} V{yc + 15}" stroke="{DINDING}" stroke-width="4"/>'
            s += aliran(120, 600, yc, 26, 64, 1.1, 4)
            s += f'<rect x="300" y="{yc - 30}" width="120" height="14" rx="6" fill="#7fb0dd" stroke="#3f6f9c"/>'
            for i, cl in enumerate(('#7a4a1d', '#111', '#d7261e', '#c9a227')):
                s += f'<rect x="{318 + i * 22}" y="{yc - 30}" width="7" height="14" fill="{cl}"/>'
            s += text(360, yc - 38, 'bagian sempit = resistor', 12, anchor='middle', fill='#ffd8a8', weight=600)
            s += text(616, y + 2, 'hambatan besar', 14, weight=700, fill=MERAH_SALAH) + text(616, y + 20, '→ arus kecil', 12, fill='#e8efe9')
    s += text(30, 172, 'tekanan sama', 11.5, fill='#cfd8d4')
    s += text(30, 354, 'Dua tangki sama tinggi (tegangan sama). Pipa yang menyempit menahan aliran, jadi lebih sedikit air yang lewat.', 12.5, fill='#e8efe9')
    s += text(30, 376, 'Di bagian sempit, dorongan yang tertahan berubah jadi panas. Itulah daya yang hilang di resistor.', 12.5, fill='#ffd8a8')
    return svg(W, H, s, 'Pipa lebar dan pipa yang menyempit dengan tekanan sama, sebagai perumpamaan hambatan')


def gnd_lantai():
    W, H = 760, 380
    s = text(30, 32, 'GND seperti lantai tempat kita mengukur tinggi', 16, weight=600)

    def tiang(x, lantai, nama, warna, nama_y=None):
        r = f'<rect x="{x - 40}" y="{lantai}" width="80" height="10" fill="#8b7a55"/>'
        r += f'<rect x="{x - 4}" y="{lantai - 110}" width="8" height="110" fill="#f2c230"/>'
        for i in range(0, 111, 22):
            r += f'<line x1="{x + 4}" y1="{lantai - i}" x2="{x + 12}" y2="{lantai - i}" stroke="#f2c230" stroke-width="2"/>'
        r += f'<circle cx="{x}" cy="{lantai - 110}" r="7" fill="{warna}"/>' + text(x + 16, lantai - 106, '3,3', 13, weight=700, font=MONO)
        r += text(x, nama_y or lantai + 30, nama, 12.5, anchor='middle', weight=600)
        return r
    # kiri: lantai sendiri-sendiri
    s += text(40, 70, 'Lantai sendiri-sendiri', 14, weight=700, fill=MERAH_SALAH)
    s += tiang(110, 290, 'papan A', WIRE['oranye'])
    s += f'<rect x="232" y="230" width="16" height="70" fill="#5b5040"/><rect x="292" y="230" width="16" height="70" fill="#5b5040"/>'
    s += tiang(270, 220, 'modul B', '#8ec5ff', 320)
    s += f'<line x1="152" y1="180" x2="250" y2="180" stroke="{WIRE["oranye"]}" stroke-width="2" stroke-dasharray="5 4"/>'
    s += text(204, 172, '?', 18, anchor='middle', weight=700, fill=MERAH_SALAH)
    s += text(40, 340, '"3,3" dari lantai A tidak sama tingginya', 12, fill='#e8efe9') + text(40, 358, 'dengan "3,3" dari lantai B.', 12, fill='#e8efe9')
    s += '<line x1="380" y1="54" x2="380" y2="364" stroke="#fff" stroke-opacity=".25" stroke-dasharray="4 5"/>'
    # kanan: lantai bersama
    s += text(410, 70, 'Lantai bersama (GND bersama)', 14, weight=700, fill=HIJAU_OK)
    s += f'<rect x="430" y="290" width="300" height="10" fill="#8b7a55"/>'
    s += tiang(500, 290, 'papan A', WIRE['oranye']) + tiang(660, 290, 'modul B', '#8ec5ff')
    s += f'<line x1="544" y1="180" x2="652" y2="180" stroke="{HIJAU_OK}" stroke-width="2" stroke-dasharray="5 4"/>' + text(598, 172, 'sama', 13, anchor='middle', weight=700, fill=HIJAU_OK)
    s += text(410, 340, 'Diukur dari lantai yang sama, "3,3" berarti', 12, fill='#e8efe9') + text(410, 358, 'tinggi yang sama bagi keduanya.', 12, fill='#e8efe9')
    return svg(W, H, s, 'Dua tiang ukur di lantai berbeda dan di lantai yang sama, sebagai perumpamaan GND bersama')


def kapasitor_tangki():
    W, H = 760, 400
    g = _id('air')
    s = gradien_air(g) + text(30, 32, 'Kapasitor seperti tangki kecil di dekat pemakai', 16, weight=600)
    s += tangki(40, 80, 140, 180, 100, g, 'sumber daya', 'jauh, lewat kabel', kanan=(240, 12))
    s += pipa(180, 470, 240, 12, g) + aliran(180, 470, 240, 10, 40, 1.2, 3.5)
    s += text(325, 226, 'pipa panjang dan kecil (kabel)', 11.5, anchor='middle', fill='#cfd8d4')
    s += tangki(470, 170, 90, 90, 196, g, 'kapasitor', 'dekat beban', kiri=(240, 12), kanan=(240, 24))
    s += pipa(560, 620, 240, 24, g) + aliran(560, 620, 240, 20, 18, 0.22, 5)
    s += f'<rect x="620" y="222" width="22" height="36" rx="4" fill="#c9d1d6"/><rect x="626" y="258" width="10" height="20" fill="#c9d1d6"/>'
    for i in range(3):
        s += f'<circle class="tetes" cx="631" cy="{288 + i * 14}" r="4" fill="{AIR1}" style="animation-delay:{-i * .25}s"/>'
    s += text(652, 236, 'beban tiba-tiba', 12, weight=600) + text(652, 252, 'minta banyak', 12, weight=600)
    s += text(30, 336, 'Saat beban (motor, servo, modul WiFi) tiba-tiba menarik banyak arus, pipa panjang dari sumber tidak cukup cepat.', 12.5, fill='#e8efe9')
    s += text(30, 358, 'Tangki kecil di dekat beban menutup kekurangan sesaat itu, lalu terisi lagi. Tanpa tangki itu, "tekanan" turun: brownout.', 12.5, fill='#e8efe9')
    s += text(30, 380, '100 nF: tangki mungil yang sangat cepat (noise). 470 µF: tangki lebih besar untuk lonjakan beban.', 12.5, fill='#ffd8a8')
    return svg(W, H, s, 'Tangki sumber jauh dan tangki kecil dekat keran, sebagai perumpamaan kapasitor')


def dioda_katup():
    W, H = 760, 360
    g = _id('air')
    s = gradien_air(g) + text(30, 32, 'Dioda seperti katup satu arah', 16, weight=600)
    for k, y in enumerate((110, 230)):
        maju = k == 0
        s += pipa(60, 260, y, 34, g) + pipa(300, 500, y, 34, g)
        s += f'<rect x="260" y="{y - 17}" width="40" height="34" fill="url(#{g})" opacity=".55"/>'
        s += f'<line x1="260" y1="{y - 17}" x2="300" y2="{y - 17}" stroke="{DINDING}" stroke-width="4"/><line x1="260" y1="{y + 17}" x2="300" y2="{y + 17}" stroke="{DINDING}" stroke-width="4"/>'
        s += f'<line x1="270" y1="{y + 17}" x2="270" y2="{y + 6}" stroke="#ff8a80" stroke-width="5"/>'   # penahan
        if maju:
            s += aliran(60, 500, y, 30, 30, 0.5, 5.5)
            s += f'<line x1="276" y1="{y - 17}" x2="300" y2="{y - 2}" stroke="#f4f6f5" stroke-width="5" stroke-linecap="round"/>'
            s += text(520, y - 4, 'arah maju: mengalir', 14, weight=700, fill=HIJAU_OK) + text(520, y + 14, 'katup terdorong terbuka', 11.5, fill='#cfd8d4')
            s += text(520, y + 30, 'butuh sedikit dorongan: sekitar 0,7 V', 11.5, fill='#ffd8a8')
        else:
            for x in range(296, 500, 22):   # air menekan katup dari kanan, tapi diam
                s += f'<circle cx="{x}" cy="{y}" r="5.5" fill="#e8f6ff" opacity=".9"/>'
            s += f'<polygon points="480,{y - 30} 480,{y - 22} 440,{y - 26}" fill="#ff8a80"/>' + text(462, y - 34, 'dorongan dari kanan', 11, anchor='middle', fill='#ff8a80')
            s += f'<line x1="276" y1="{y - 17}" x2="276" y2="{y + 14}" stroke="#f4f6f5" stroke-width="5" stroke-linecap="round"/>'
            s += text(520, y - 4, 'arah balik: tertahan', 14, weight=700, fill=MERAH_SALAH) + text(520, y + 14, 'katup tertekan ke penahan', 11.5, fill='#cfd8d4')
        # simbol dioda kecil
        sx = 160
        s += f'<g transform="translate({sx} {y + 46}) scale({1 if maju else -1} 1)"><line x1="-30" y1="0" x2="30" y2="0" stroke="{KAWAT}" stroke-width="2.4"/>'
        s += '<polygon points="-10,-10 -10,10 8,0" fill="#f4f6f5"/><line x1="9" y1="-11" x2="9" y2="11" stroke="#f4f6f5" stroke-width="3"/></g>'
    s += text(30, 336, 'Simbol dioda: segitiga menunjuk arah arus boleh mengalir; garis adalah katoda, sisi bergaris di badan dioda.', 12.5, fill='#e8efe9')
    return svg(W, H, s, 'Katup satu arah yang terbuka untuk aliran maju dan tertutup untuk aliran balik, sebagai perumpamaan dioda')


def transistor_keran():
    W, H = 760, 390
    g = _id('air')
    s = gradien_air(g) + text(30, 32, 'Transistor seperti keran yang dibuka tuas kecil', 16, weight=600)
    # pipa besar (kolektor ke emitor)
    s += pipa(40, 330, 240, 44, g) + pipa(390, 600, 240, 44, g)
    s += aliran(40, 600, 240, 40, 20, 0.22, 7)
    # gerbang
    s += f'<rect x="330" y="186" width="60" height="32" rx="4" fill="#5b6b74" stroke="{DINDING}" stroke-width="3"/>'
    s += f'<rect x="336" y="222" width="48" height="10" fill="#5b6b74"/>'
    s += f'<line x1="330" y1="218" x2="330" y2="262" stroke="{DINDING}" stroke-width="4"/><line x1="390" y1="218" x2="390" y2="262" stroke="{DINDING}" stroke-width="4"/>'
    s += f'<line x1="330" y1="262" x2="390" y2="262" stroke="{DINDING}" stroke-width="4"/>'
    # pipa kecil (basis) menekan tuas
    s += f'<path d="M360 186 V120" stroke="{DINDING}" stroke-width="3"/>'
    s += pipa(150, 352, 110, 12, g) + aliran(150, 352, 110, 10, 50, 1.4, 3.5)
    s += f'<circle cx="360" cy="110" r="10" fill="#f2c230" stroke="#7a5b0c" stroke-width="2"/>'
    s += text(150, 88, 'pipa kecil: basis atau gate', 12.5, weight=700, fill=WIRE['oranye']) + text(150, 140, 'arus kecil dari pin (beberapa mA)', 11.5, fill='#cfd8d4')
    s += text(40, 296, 'kolektor (dari beban)', 12.5, weight=600) + text(600, 296, 'emitor (ke GND)', 12.5, weight=600, anchor='end')
    s += text(620, 236, 'arus besar', 14, weight=700, fill=KUNING_ARUS) + text(620, 254, 'ratusan mA', 11.5, fill='#cfd8d4')
    s += text(410, 180, 'tuas mengangkat gerbang', 11.5, fill='#cfd8d4')
    s += text(30, 336, 'Aliran kecil di pipa atas mengangkat gerbang, dan aliran besar di pipa bawah ikut mengalir. Tanpa aliran kecil,', 12.5, fill='#e8efe9')
    s += text(30, 358, 'gerbang tertutup. Begitulah pin mikrokontroler menyalakan relay atau motor: pin hanya membuka, bebannya mendapat', 12.5, fill='#e8efe9')
    s += text(30, 380, 'arus dari sumbernya sendiri.', 12.5, fill='#e8efe9')
    return svg(W, H, s, 'Pipa besar dengan gerbang yang diangkat aliran pipa kecil, sebagai perumpamaan transistor')
