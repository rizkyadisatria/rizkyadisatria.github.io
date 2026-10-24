# Ilustrasi sampul (cover) tiap halaman panduan Robot AI 4WD. SVG buatan sendiri, tanpa berkas gambar.
# Latar gelap di dalam bingkai sendiri, jadi tampil sama di tema terang dan gelap.

W, H = 800, 280
INK = '#0f1413'
BG1, BG2 = '#1c2a25', '#13201c'
ORANYE, ORANYE2 = '#e58c51', '#a9531c'
KUNING = '#f1c40f'
BIRU = '#3f8de0'
HIJAU = '#2f8f5a'
MERAH = '#c0392b'
PUTIH = '#eef3f0'


def bingkai(isi, label, id_):
    grid = ''.join(f'<path d="M{x} 0V{H}" stroke="#ffffff" stroke-opacity=".04"/>' for x in range(0, W, 20))
    grid += ''.join(f'<path d="M0 {y}H{W}" stroke="#ffffff" stroke-opacity=".04"/>' for y in range(0, H, 20))
    return (f'<svg class="cover" viewBox="0 0 {W} {H}" role="img" aria-label="{label}" xmlns="http://www.w3.org/2000/svg">'
            f'<defs><linearGradient id="bg{id_}" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="{BG1}"/><stop offset="1" stop-color="{BG2}"/></linearGradient>'
            f'<radialGradient id="glow{id_}" cx=".72" cy=".45" r=".55"><stop offset="0" stop-color="{ORANYE}" stop-opacity=".22"/><stop offset="1" stop-color="{ORANYE}" stop-opacity="0"/></radialGradient></defs>'
            f'<rect width="{W}" height="{H}" rx="18" fill="url(#bg{id_})"/>{grid}<rect width="{W}" height="{H}" rx="18" fill="url(#glow{id_})"/>'
            f'<ellipse cx="520" cy="252" rx="250" ry="14" fill="#000" opacity=".28"/>{isi}</svg>')


# ---------- potongan gambar ----------
def roda(cx, cy, r=34):
    s = f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="#1b1b1b"/>'
    for i in range(16):
        s += f'<rect x="{cx - 3}" y="{cy - r}" width="6" height="7" rx="1.5" fill="#2d2d2d" transform="rotate({i * 22.5} {cx} {cy})"/>'
    s += f'<circle cx="{cx}" cy="{cy}" r="{r * .55}" fill="{KUNING}"/><circle cx="{cx}" cy="{cy}" r="{r * .2}" fill="#d4a80d"/>'
    return s


def plat(x, y, w, t=6):
    return f'<rect x="{x}" y="{y}" width="{w}" height="{t}" rx="3" fill="#dfe7e3" fill-opacity=".85"/>'


def spacer(x, y, h):
    return f'<rect x="{x}" y="{y}" width="6" height="{h}" fill="#c9a227"/><rect x="{x}" y="{y}" width="2" height="{h}" fill="#f3d36b" opacity=".6"/>'


def sel(x, y, w=110, h=30, label=''):
    s = f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{h / 2}" fill="{BIRU}"/>'
    s += f'<rect x="{x + w - 10}" y="{y + h * .3}" width="10" height="{h * .4}" rx="2" fill="#cfd6dd"/>'
    s += f'<rect x="{x + 10}" y="{y + 4}" width="{w - 30}" height="5" rx="2.5" fill="#ffffff" opacity=".25"/>'
    if label:
        s += f'<text x="{x + w / 2 - 4}" y="{y + h / 2 + 5}" text-anchor="middle" font-family="IBM Plex Mono,monospace" font-size="13" fill="#e8f1fb" font-weight="600">{label}</text>'
    return s


def pcb(x, y, w, h, warna=HIJAU, pin=True):
    s = f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="5" fill="{warna}"/>'
    if pin:
        for i in range(int((w - 16) // 10)):
            s += f'<rect x="{x + 10 + i * 10}" y="{y + 4}" width="4" height="4" fill="#e6c35c"/><rect x="{x + 10 + i * 10}" y="{y + h - 8}" width="4" height="4" fill="#e6c35c"/>'
    return s


def chip(x, y, w=34, h=26):
    return (f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="3" fill="#1d1f22"/>'
            f'<rect x="{x + 4}" y="{y + 4}" width="{w - 8}" height="{h - 8}" rx="2" fill="#2c3036"/>')


def wajah(x, y, w, h):
    """Layar TFT dengan wajah robot."""
    s = f'<rect x="{x - 8}" y="{y - 8}" width="{w + 16}" height="{h + 16}" rx="6" fill="{MERAH}"/>'
    s += f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="4" fill="#0b1416"/>'
    ew, eh = w * .2, h * .34
    s += f'<rect x="{x + w * .2}" y="{y + h * .22}" width="{ew}" height="{eh}" rx="{ew / 2.4}" fill="#5ff2ff"/>'
    s += f'<rect x="{x + w * .6}" y="{y + h * .22}" width="{ew}" height="{eh}" rx="{ew / 2.4}" fill="#5ff2ff"/>'
    s += f'<path d="M{x + w * .32} {y + h * .72} Q{x + w / 2} {y + h * .86} {x + w * .68} {y + h * .72}" stroke="#5ff2ff" stroke-width="4" fill="none" stroke-linecap="round"/>'
    return s


def hcsr(x, y, s_=1):
    s = f'<rect x="{x}" y="{y}" width="{70 * s_}" height="{30 * s_}" rx="3" fill="#1d4f86"/>'
    for cx in (x + 17 * s_, x + 53 * s_):
        s += f'<circle cx="{cx}" cy="{y + 15 * s_}" r="{13 * s_}" fill="#cfd6dd"/><circle cx="{cx}" cy="{y + 15 * s_}" r="{9 * s_}" fill="#5b6066"/>'
    return s


def kepala(x, y, s_=1):
    """Pan-tilt dengan ESP32-CAM, dilihat dari samping depan."""
    s = f'<rect x="{x}" y="{y + 52 * s_}" width="{44 * s_}" height="{24 * s_}" rx="3" fill="#22262a"/>'      # servo pan
    s += f'<rect x="{x + 6 * s_}" y="{y + 20 * s_}" width="{32 * s_}" height="{34 * s_}" rx="3" fill="#2d3238"/>'  # bracket
    s += f'<rect x="{x - 4 * s_}" y="{y}" width="{52 * s_}" height="{30 * s_}" rx="4" fill="#20242a"/>'        # dudukan kamera
    s += f'<circle cx="{x + 22 * s_}" cy="{y + 15 * s_}" r="{11 * s_}" fill="#0c0e10" stroke="#5b6066" stroke-width="{3 * s_}"/>'
    s += f'<circle cx="{x + 25 * s_}" cy="{y + 12 * s_}" r="{3 * s_}" fill="#6fa8ff" opacity=".8"/>'
    s += f'<circle cx="{x + 42 * s_}" cy="{y + 8 * s_}" r="{3 * s_}" fill="#fff6c0"/>'
    return s


def motor(x, y, w=64, h=20):
    return (f'<rect x="{x}" y="{y}" width="{w * .62}" height="{h}" rx="3" fill="{KUNING}"/>'
            f'<rect x="{x + w * .62}" y="{y + 2}" width="{w * .38}" height="{h - 4}" rx="{(h - 4) / 2}" fill="#c9ced3"/>')


def baut(x, y, t='+'):
    return f'<circle cx="{x}" cy="{y}" r="9" fill="#fff" opacity=".1"/><text x="{x}" y="{y + 5}" text-anchor="middle" font-family="IBM Plex Mono,monospace" font-size="15" fill="{ORANYE}" font-weight="600">{t}</text>'


def kilat(x, y, s_=1, warna=ORANYE):
    return f'<path transform="translate({x} {y}) scale({s_})" d="M22 0 L4 34 H18 L12 62 L36 22 H22 L30 0Z" fill="{warna}"/>'


def gelombang(x, y, n=3, warna=ORANYE, arah=1):
    s = ''
    for i in range(n):
        r = 14 + i * 12
        s += f'<path d="M{x} {y - r} A{r} {r} 0 0 {1 if arah > 0 else 0} {x} {y + r}" stroke="{warna}" stroke-width="4" fill="none" stroke-linecap="round" opacity="{1 - i * .25}"/>'
    return s


def robot(x, y, s_=1):
    """Robot tiga lantai dilihat dari samping (gambar utama)."""
    g = f'<g transform="translate({x} {y}) scale({s_})">'
    g += plat(0, 150, 300) + plat(0, 118, 300) + plat(0, 56, 300)
    for sx in (14, 150, 280):
        g += spacer(sx, 124, 26) + spacer(sx, 62, 56)
    g += motor(30, 156) + motor(206, 156, 64)
    g += sel(20, 88, 92, 28) + pcb(122, 92, 46, 22, '#1d6b3a', False)
    g += pcb(176, 80, 76, 34, '#1d3324') + chip(196, 88)
    g += hcsr(258, 84, .7)
    g += pcb(10, 26, 90, 30, '#13283a') + chip(36, 30, 40, 22)
    g += wajah(126, 4, 70, 44)
    g += kepala(232, -34, .95)
    g += roda(62, 188) + roda(238, 188)
    g += '</g>'
    return g


# ---------- sampul tiap halaman ----------
def utama():
    isi = robot(340, 58, .95)
    isi += gelombang(700, 90, 3, ORANYE, 1)
    isi += f'<g transform="translate(70 70)"><rect width="170" height="64" rx="16" fill="#26332e" stroke="#3a4a44"/><path d="M28 64 L20 84 L46 64Z" fill="#26332e"/>'
    isi += f'<text x="85" y="40" text-anchor="middle" font-family="IBM Plex Sans,sans-serif" font-size="20" fill="{PUTIH}" font-weight="600">Halo, aku Robi!</text></g>'
    isi += f'<g transform="translate(90 160)"><rect width="120" height="44" rx="14" fill="{ORANYE2}"/><text x="60" y="29" text-anchor="middle" font-family="IBM Plex Sans,sans-serif" font-size="16" fill="#fff" font-weight="600">maju 1 m</text></g>'
    return bingkai(isi, 'Ilustrasi robot AI 4WD tiga lantai dengan layar wajah dan kepala kamera', 'u')


def daya():
    isi = sel(300, 70, 190, 46, '18650 · 3,7 V') + sel(300, 132, 190, 46, '18650 · 3,7 V')
    isi += pcb(510, 86, 110, 76, '#1d6b3a', False) + chip(530, 104, 30, 22) + chip(572, 104, 30, 22)
    isi += f'<text x="565" y="150" text-anchor="middle" font-family="IBM Plex Mono,monospace" font-size="12" fill="#cfe9d8">BMS 2S</text>'
    isi += f'<path d="M490 93 H510 M490 155 H510" stroke="#ff5a4f" stroke-width="5"/>'
    # multimeter
    isi += f'<g transform="translate(650 50)"><rect width="104" height="170" rx="14" fill="#f2c230"/><rect x="12" y="14" width="80" height="44" rx="6" fill="#c9dccc"/>'
    isi += f'<text x="52" y="45" text-anchor="middle" font-family="IBM Plex Mono,monospace" font-size="22" fill="#1b2a22" font-weight="600">7.40</text>'
    isi += f'<circle cx="52" cy="108" r="30" fill="#2a2a2a"/><rect x="49" y="80" width="6" height="26" rx="3" fill="#ddd" transform="rotate(35 52 108)"/>'
    isi += f'<circle cx="30" cy="154" r="7" fill="#111"/><circle cx="74" cy="154" r="7" fill="{MERAH}"/></g>'
    isi += kilat(170, 70, 2.2)
    return bingkai(isi, 'Ilustrasi dua sel 18650, BMS, dan multimeter membaca 7,40 V', 'd')


def badan():
    g = '<g transform="translate(250 30)">'
    g += plat(0, 150, 360, 8) + plat(0, 112, 360, 7) + motor(40, 160, 90, 26) + motor(230, 160, 90, 26)
    for sx in (12, 176, 342):
        g += spacer(sx, 119, 31)
    g += pcb(140, 122, 70, 26, '#8c1d18', False) + chip(160, 124, 30, 22)
    g += hcsr(300, 120, .8)
    g += kepala(290, 28, 1.1)
    g += roda(80, 200, 44) + roda(280, 200, 44)
    g += '</g>'
    g += gelombang(718, 153, 3, '#6ab0ff', 1)
    g += f'<path d="M150 200 H220 M150 220 H200" stroke="{ORANYE}" stroke-width="6" stroke-linecap="round" opacity=".8"/><path d="M150 180 H240" stroke="{ORANYE}" stroke-width="6" stroke-linecap="round"/>'
    return bingkai(g, 'Ilustrasi chassis empat roda dengan motor, driver, sensor jarak, dan kepala pan-tilt', 'b')


def otak():
    isi = pcb(260, 70, 200, 150, '#13283a') + chip(320, 110, 80, 60)
    isi += f'<text x="360" y="146" text-anchor="middle" font-family="IBM Plex Mono,monospace" font-size="13" fill="#9fb8cc">ESP32-S3</text>'
    isi += wajah(500, 66, 150, 104)
    isi += f'<rect x="680" y="96" width="60" height="90" rx="10" fill="#2a2a2a"/><circle cx="710" cy="141" r="20" fill="#111" stroke="#555" stroke-width="3"/>'
    isi += gelombang(752, 141, 3, ORANYE, 1)
    isi += f'<rect x="150" y="110" width="70" height="40" rx="5" fill="#2c3e50"/><circle cx="185" cy="130" r="9" fill="#cfd6dd"/>'
    isi += gelombang(140, 130, 2, '#6ab0ff', -1)
    isi += f'<path d="M220 130 H260 M460 118 H492" stroke="#e6c35c" stroke-width="4" stroke-dasharray="6 5"/>'
    return bingkai(isi, 'Ilustrasi ESP32-S3, layar wajah robot, speaker, dan mikrofon', 'o')


def mata():
    isi = pcb(300, 70, 150, 160, '#1a1a1a', False)
    isi += f'<rect x="340" y="96" width="70" height="70" rx="6" fill="#2a2e33"/><circle cx="375" cy="131" r="27" fill="#0b0d10" stroke="#5b6066" stroke-width="6"/><circle cx="384" cy="122" r="7" fill="#6fa8ff" opacity=".8"/>'
    isi += f'<rect x="318" y="190" width="18" height="18" rx="3" fill="#fff6c0"/>'
    isi += f'<text x="375" y="218" text-anchor="middle" font-family="IBM Plex Mono,monospace" font-size="12" fill="#9aa">ESP32-CAM</text>'
    isi += ''.join(f'<path d="M{480 + i * 22} {150 - 26 - i * 18} A{30 + i * 22} {30 + i * 22} 0 0 1 {480 + i * 22} {150 + 26 + i * 18}" stroke="{ORANYE}" stroke-width="5" fill="none" stroke-linecap="round" opacity="{1 - i * .25}"/>' for i in range(3))
    isi += f'<g transform="translate(600 70) rotate(4)"><rect width="150" height="120" rx="8" fill="{PUTIH}"/><rect x="10" y="10" width="130" height="84" fill="#7fb3d5"/>'
    isi += f'<path d="M10 94 L60 44 L96 80 L114 62 L140 88 V94Z" fill="#3c7a52"/><circle cx="112" cy="32" r="10" fill="#ffd54a"/></g>'
    return bingkai(isi, 'Ilustrasi ESP32-CAM mengirim foto lewat WiFi', 'm')


def ai():
    isi = f'<g transform="translate(250 54)"><rect width="250" height="66" rx="18" fill="#26332e" stroke="#3a4a44"/><path d="M30 66 L22 86 L52 66Z" fill="#26332e"/>'
    isi += f'<text x="125" y="41" text-anchor="middle" font-family="IBM Plex Sans,sans-serif" font-size="19" fill="{PUTIH}">"Robi, lihat ke kiri"</text></g>'
    isi += f'<g transform="translate(300 150)"><rect width="270" height="66" rx="18" fill="{ORANYE2}"/><path d="M240 66 L250 86 L216 66Z" fill="{ORANYE2}"/>'
    isi += f'<text x="135" y="41" text-anchor="middle" font-family="IBM Plex Sans,sans-serif" font-size="19" fill="#fff">"Ada kucing di sofa."</text></g>'
    isi += wajah(610, 80, 120, 84)
    isi += f'<g transform="translate(110 96)"><circle cx="40" cy="40" r="40" fill="#2aa3d6"/><path d="M18 40 L62 22 L52 62 L40 48 L30 56 L32 44Z" fill="#fff"/></g>'
    isi += ''.join(f'<path transform="translate({x} {y}) scale({k})" d="M0 -14 L4 -4 L14 0 L4 4 L0 14 L-4 4 L-14 0 L-4 -4Z" fill="#ffd54a"/>' for x, y, k in ((600, 60, 1), (750, 200, .8), (230, 220, .7)))
    return bingkai(isi, 'Ilustrasi obrolan Telegram dengan robot dan jawaban AI', 'a')


def rakit():
    g = '<g transform="translate(300 18)">'
    # tiga lantai dalam tampilan miring (terurai)
    for i, (yy, isi_) in enumerate(((180, 'lantai 1'), (110, 'lantai 2'), (40, 'lantai 3'))):
        g += f'<path d="M0 {yy} L260 {yy} L320 {yy + 34} L60 {yy + 34}Z" fill="#dfe7e3" fill-opacity="{.20 + i * .05}" stroke="#dfe7e3" stroke-opacity=".8"/>'
        g += f'<text x="-14" y="{yy + 24}" text-anchor="end" font-family="IBM Plex Mono,monospace" font-size="13" fill="#cfd8d4">{isi_}</text>'
    for x, y in ((18, 214), (258, 214), (62, 214), (302, 214)):
        pass
    g += f'<path d="M60 190 h60 v14 h-60z M150 190 h60 v14 h-60z" fill="#1d3f7a"/>' + motor(40, 200, 50, 12) + motor(210, 200, 50, 12)
    g += sel(40, 118, 70, 18) + f'<rect x="130" y="120" width="90" height="16" rx="3" fill="#d9d9cf"/>' + f'<rect x="230" y="118" width="50" height="22" rx="3" fill="#1d3324"/>'
    g += f'<rect x="40" y="46" width="70" height="22" rx="3" fill="#13283a"/><rect x="130" y="48" width="56" height="18" rx="3" fill="{MERAH}"/>' + kepala(232, -14, .6)
    for x in (10, 250):
        g += f'<path d="M{x} 74 V180 M{x + 60} 108 V214" stroke="#c9a227" stroke-width="4" stroke-dasharray="2 0"/>'
    g += '</g>'
    g += f'<g transform="translate(130 100)"><circle cx="40" cy="40" r="40" fill="{HIJAU}"/><path d="M20 42 L34 56 L62 26" stroke="#fff" stroke-width="9" fill="none" stroke-linecap="round" stroke-linejoin="round"/></g>'
    return bingkai(g, 'Ilustrasi tiga lantai robot yang dirakit bertumpuk', 'r')


SAMPUL = {'': utama, '1-daya': daya, '2-badan': badan, '3-otak': otak, '4-mata': mata, '5-ai': ai, '6-rakit': rakit}
