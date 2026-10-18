# Pembuat diagram SVG untuk panduan robot. Dipanggil oleh build.py.
# Gaya: latar alas potong hijau (sama dengan panduan stasiun cuaca), warna kabel standar.
from html import escape as E

BG = '#2f5a4e'
WIRE = {
    '5v': '#e5392f', '3v3': '#f08a24', 'gnd': '#1b1b1b', '6v': '#9b6cf2', 'bat': '#ff5a4f',
    'kuning': '#f2c230', 'hijau': '#3fae4a', 'biru': '#3b82f6', 'putih': '#f2f2f2', 'cyan': '#26c6da',
    'pink': '#ec6fb4', 'coklat': '#a0682f', 'abu': '#9aa5a1', 'oranye': '#f08a24',
}
MONO = 'IBM Plex Mono,ui-monospace,monospace'
SANS = 'IBM Plex Sans,system-ui,sans-serif'


def grid(w, h):
    s = ''
    for x in range(0, w + 1, 20):
        s += f'<line x1="{x}" y1="0" x2="{x}" y2="{h}" stroke="#fff" stroke-opacity=".06"/>'
    for y in range(0, h + 1, 20):
        s += f'<line x1="0" y1="{y}" x2="{w}" y2="{y}" stroke="#fff" stroke-opacity=".06"/>'
    return s


def svg(w, h, body, label):
    return (f'<svg viewBox="0 0 {w} {h}" xmlns="http://www.w3.org/2000/svg" role="img" aria-label="{E(label)}">'
            f'<rect width="{w}" height="{h}" fill="{BG}"/>{grid(w, h)}{body}</svg>')


def text(x, y, t, size=13, fill='#f4f6f5', anchor='start', weight=400, font=SANS):
    return (f'<text x="{x}" y="{y}" font-family="{font}" font-size="{size}" fill="{fill}" '
            f'text-anchor="{anchor}" font-weight="{weight}">{E(t)}</text>')


def wire_path(x1, y1, x2, y2, col, width=3.6):
    dx = (x2 - x1) * 0.45
    d = f'M{x1} {y1} C{x1 + dx} {y1} {x2 - dx} {y2} {x2} {y2}'
    outline = '#f4f6f5' if col == WIRE['gnd'] else '#0b0d0c'
    op = '.55' if col == WIRE['gnd'] else '.45'
    return (f'<path d="{d}" fill="none" stroke="{outline}" stroke-opacity="{op}" stroke-width="{width + 2.6}" stroke-linecap="round"/>'
            f'<path d="{d}" fill="none" stroke="{col}" stroke-width="{width}" stroke-linecap="round"/>')


def dot(x, y):
    return f'<circle cx="{x}" cy="{y}" r="4" fill="#fff" stroke="#111" stroke-width="1.3"/>'


def conn(title_l, color_l, pins_l, title_r, color_r, pins_r, wires, note=None, label='Diagram sambungan', w=760):
    """pins_l / pins_r: daftar teks pin (urut atas ke bawah). pins_r boleh berisi ('#grup', 'nama grup').
    wires: (index_kiri, index_kanan, warna, teks_tengah_opsional)."""
    row = 30
    top = 64
    hl = len(pins_l) * row + 20
    hr = len(pins_r) * row + 20
    h = top + max(hl, hr) + (46 if note else 24)
    lx, lw = 30, 200
    rx, rw = w - 30 - 250, 250
    s = ''
    # kotak kiri (modul)
    s += f'<rect x="{lx}" y="{top}" width="{lw}" height="{hl}" rx="10" fill="{color_l}" stroke="#0008" stroke-width="1.5"/>'
    s += text(lx + lw / 2, top - 12, title_l, 15, anchor='middle', weight=600)
    yl = {}
    for i, p in enumerate(pins_l):
        y = top + 24 + i * row
        yl[i] = y
        s += f'<rect x="{lx + lw - 16}" y="{y - 7}" width="14" height="14" rx="2" fill="#d9b44a" stroke="#6d5513"/>'
        s += text(lx + lw - 24, y + 5, p, 13, anchor='end', font=MONO, fill='#fff')
    # kotak kanan (board / tujuan)
    s += f'<rect x="{rx}" y="{top}" width="{rw}" height="{hr}" rx="10" fill="{color_r}" stroke="#0008" stroke-width="1.5"/>'
    s += text(rx + rw / 2, top - 12, title_r, 15, anchor='middle', weight=600)
    yr = {}
    for i, p in enumerate(pins_r):
        y = top + 24 + i * row
        if isinstance(p, tuple) and p[0] == '#grup':
            s += text(rx + 14, y + 5, p[1], 11.5, fill='#c9d4cf', font=MONO)
            s += f'<line x1="{rx + 10}" y1="{y + 11}" x2="{rx + rw - 10}" y2="{y + 11}" stroke="#ffffff33"/>'
            continue
        yr[i] = y
        s += f'<rect x="{rx + 2}" y="{y - 7}" width="14" height="14" rx="2" fill="#d9b44a" stroke="#6d5513"/>'
        s += text(rx + 24, y + 5, p, 13, font=MONO, fill='#fff')
    for (a, b, col, mid) in [(wi + (None,))[:4] for wi in wires]:
        x1, y1 = lx + lw - 9, yl[a]
        x2, y2 = rx + 9, yr[b]
        c = WIRE.get(col, col)
        s += wire_path(x1, y1, x2, y2, c) + dot(x1, y1) + dot(x2, y2)
        if mid:
            mx, my = (x1 + x2) / 2, (y1 + y2) / 2
            tw = len(mid) * 7 + 14
            s += f'<rect x="{mx - tw / 2}" y="{my - 10}" width="{tw}" height="20" rx="4" fill="#0d0f0e" stroke="#e9ece9" stroke-width=".6"/>'
            s += text(mx, my + 4, mid, 11, anchor='middle', font=MONO)
    if note:
        s += text(w / 2, h - 16, note, 12.5, anchor='middle', fill='#e8efe9')
    return svg(w, h, s, label)


def box(x, y, w, h, title, sub='', fill='#151a18', stroke='#e9ece9', tcol='#f4f6f5'):
    s = f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="10" fill="{fill}" stroke="{stroke}" stroke-width="1.6"/>'
    lines = [title] + ([sub] if isinstance(sub, str) and sub else list(sub) if sub else [])
    n = len(lines)
    y0 = y + h / 2 - (n - 1) * 9 + 5
    for i, t in enumerate(lines):
        s += text(x + w / 2, y0 + i * 18, t, 14 if i == 0 else 11.5, anchor='middle', weight=600 if i == 0 else 400, fill=tcol if i == 0 else '#cfd8d4')
    return s


def line(pts, col, width=4):
    c = WIRE.get(col, col)
    d = 'M' + ' L'.join(f'{x} {y}' for x, y in pts)
    outline = '#f4f6f5' if c == WIRE['gnd'] else '#0b0d0c'
    return (f'<path d="{d}" fill="none" stroke="{outline}" stroke-opacity=".45" stroke-width="{width + 2.6}" stroke-linejoin="round" stroke-linecap="round"/>'
            f'<path d="{d}" fill="none" stroke="{c}" stroke-width="{width}" stroke-linejoin="round" stroke-linecap="round"/>')


def tag(x, y, t, fill='#0d0f0e'):
    tw = len(t) * 7 + 14
    return (f'<rect x="{x - tw / 2}" y="{y - 10}" width="{tw}" height="20" rx="4" fill="{fill}" stroke="#e9ece9" stroke-width=".6"/>'
            + text(x, y + 4, t, 11, anchor='middle', font=MONO))


# ---------------- diagram khusus ----------------

def daya():
    W, H = 900, 560
    s = ''
    s += box(20, 230, 150, 90, 'Baterai 2S', ['2x 18650 + BMS', '7,4 - 8,4 V'], fill='#3a1513', stroke='#ff8a80')
    s += box(200, 245, 100, 60, 'Sekring', '5 A', fill='#3a1513', stroke='#ff8a80')
    s += box(330, 245, 110, 60, 'Saklar', 'utama', fill='#3a1513', stroke='#ff8a80')
    s += line([(170, 275), (200, 275)], 'bat') + line([(300, 275), (330, 275)], 'bat') + line([(440, 275), (480, 275)], 'bat')
    s += line([(480, 90), (480, 460)], 'bat') + line([(480, 90), (520, 90)], 'bat') + line([(480, 275), (520, 275)], 'bat') + line([(480, 460), (520, 460)], 'bat')
    s += box(520, 55, 150, 70, 'LM2596 #1', ['atur 5,0 V', 'logika'], fill='#33240f', stroke='#f0a64b')
    s += box(520, 240, 150, 70, 'LM2596 #2', ['atur 5,0 V', 'kamera'], fill='#33240f', stroke='#f0a64b')
    s += box(520, 425, 150, 70, 'XL4015', ['atur 6,0 V', 'motor + servo'], fill='#241a3a', stroke='#b08cff')
    s += line([(670, 90), (700, 90), (700, 45), (720, 45)], '5v') + line([(700, 90), (700, 135), (720, 135)], '5v')
    s += box(720, 18, 165, 56, 'ESP32-S3 (otak)', 'pin 5V papan ekspansi')
    s += box(720, 108, 165, 56, 'DevKit V1 (badan)', 'pin 5V papan 30P')
    s += line([(670, 275), (720, 275)], '5v')
    s += box(720, 245, 165, 60, 'ESP32-CAM (mata)', ['pin 5V + 1000 uF'])
    s += line([(670, 460), (700, 460), (700, 415), (720, 415)], '6v') + line([(700, 460), (700, 505), (720, 505)], '6v')
    s += box(720, 388, 165, 56, 'TB6612 (pin VM)', '4 motor TT')
    s += box(720, 478, 165, 56, 'Servo pan + tilt', '+ 470 uF')
    s += f'<rect x="20" y="360" width="440" height="135" rx="10" fill="#101312" stroke="#f4f6f5" stroke-width="2"/>'
    s += text(240, 388, 'GND BERSAMA', 15, anchor='middle', weight=600)
    for i, t in enumerate(['Semua minus ke satu titik: ⊖ BMS, IN− dan OUT− ketiga', 'konverter, GND S3, DevKit, ESP32-CAM, TB6612, servo.',
                           'Tanpa GND bersama, perintah UART dan PWM tidak terbaca.']):
        s += text(240, 414 + i * 20, t, 12, anchor='middle', fill='#cfd8d4')
    s += text(20, 30, 'Merah = baterai · Oranye-merah = 5 V · Ungu = 6 V · Hitam = GND', 12.5, fill='#e8efe9')
    s += text(20, 50, 'Setel tegangan tiap konverter TANPA beban dulu, baru sambung board.', 12.5, fill='#ffd8a8')
    return svg(W, H, s, 'Jalur daya robot dari baterai ke tiga konverter dan semua board')


def bms():
    W, H = 860, 470
    s = ''
    # dua holder
    def holder(x, y, nama):
        r = f'<rect x="{x}" y="{y}" width="260" height="62" rx="8" fill="#141414" stroke="#555"/>'
        r += f'<rect x="{x + 20}" y="{y + 12}" width="220" height="38" rx="18" fill="#3f8de0" stroke="#173f6a"/>'
        r += f'<rect x="{x + 236}" y="{y + 22}" width="10" height="18" rx="2" fill="#cfd6dd"/>'
        r += text(x + 130, y + 37, nama, 13, anchor='middle', weight=600)
        r += text(x + 8, y + 37, '−', 18, weight=700)
        r += text(x + 252, y + 37, '+', 16, weight=700)
        return r
    s += holder(40, 70, 'SEL 1  (3,0-4,2 V)')
    s += holder(40, 230, 'SEL 2  (3,0-4,2 V)')
    # BMS
    bx, by = 520, 90
    s += f'<rect x="{bx}" y="{by}" width="300" height="200" rx="10" fill="#1d6b3a" stroke="#0c3a1d" stroke-width="2"/>'
    s += text(bx + 165, by + 188, 'BMS 2S 20A REV 2.1', 13, anchor='middle', weight=600)
    pads = {'0V': (bx + 270, by + 165), '4.2V': (bx + 30, by + 100), '8.4V': (bx + 270, by + 60), '⊕': (bx + 90, by + 70), '⊖': (bx + 90, by + 135)}
    for n, (x, y) in pads.items():
        s += f'<rect x="{x - 13}" y="{y - 13}" width="26" height="26" rx="4" fill="#d9d2c4" stroke="#6d6250"/>'
        s += text(x + (18 if n in ('⊕', '⊖') else (-18 if x > bx + 150 else 18)), y + 5, n, 13, anchor='start' if n in ('⊕', '⊖') or x < bx + 150 else 'end', weight=600, font=MONO)
    # kabel
    s += line([(40, 101), (20, 101), (20, 420), (bx + 270, 420), (bx + 270, by + 178)], 'gnd', 4)     # sel1 − ke 0V
    s += tag(250, 420, '1. sel 1 (−) → 0V')
    s += line([(300, 101), (330, 101), (330, 205), (30, 205), (30, 261), (40, 261)], 'oranye', 4)  # sel1+ ke sel2−
    s += line([(330, 160), (470, 160), (470, by + 100), (bx + 17, by + 100)], 'oranye', 3)              # titik tengah ke 4.2V
    s += tag(400, 160, '2. titik tengah → 4.2V')
    s += line([(300, 261), (360, 261), (360, 330), (bx + 300 + 20, 330), (bx + 300 + 20, by + 60), (bx + 283, by + 60)], 'bat', 4)
    s += tag(470, 330, '3. sel 2 (+) → 8.4V')
    # keluaran
    s += line([(bx + 90, by + 57), (bx + 90, 62), (bx + 120, 62)], '5v', 4)
    s += text(bx + 126, 67, 'P+ → sekring → saklar', 12.5, weight=600)
    s += line([(bx + 77, by + 135), (bx + 50, by + 135), (bx + 50, 380), (bx + 210, 380)], 'gnd', 4)
    s += text(bx + 216, 385, 'P− ke GND bersama', 12.5, weight=600)
    s += text(40, 40, 'Sambung dengan urutan 1 → 2 → 3. Lepas dengan urutan terbalik.', 13, fill='#ffd8a8', weight=600)
    s += text(40, 455, 'Kabel hitam/oranye/merah tebal (≥ 22 AWG) untuk jalur 1, 3, P+ dan P−. Jalur 2 boleh kabel tipis.', 12, fill='#e8efe9')
    return svg(W, H, s, 'Cara menyambung dua sel 18650 ke BMS 2S')


def arsitektur():
    W, H = 900, 430
    s = ''
    s += box(330, 20, 240, 120, 'ESP32-S3 (OTAK)', ['layar sentuh + wajah', 'speaker, mikrofon, SD', 'WiFi, Telegram, AI Claude'], fill='#13283a', stroke='#7fb6ff')
    s += box(20, 230, 240, 120, 'ESP32 DevKit V1 (BADAN)', ['4 motor (TB6612), 2 servo', '2 sensor jarak, MPU6050', 'cuaca, tombol STOP, baterai'], fill='#1d3324', stroke='#7fe0a0')
    s += box(640, 230, 240, 120, 'ESP32-CAM (MATA)', ['kamera OV2640/OV3660', 'foto lewat WiFi', 'di kepala pan-tilt'], fill='#3a2a13', stroke='#ffc27a')
    s += line([(330, 110), (140, 110), (140, 230)], 'kuning', 4)
    s += tag(235, 98, 'UART: 3 kabel (TX, RX, GND)')
    s += line([(570, 110), (760, 110), (760, 230)], 'cyan', 4)
    s += f'<path d="M570 110 L760 110 L760 230" fill="none" stroke="#0d0f0e" stroke-width="2" stroke-dasharray="6 6"/>'
    s += tag(665, 98, 'WiFi (tanpa kabel)')
    s += box(330, 260, 240, 70, 'Internet', ['Telegram · Claude · Groq'], fill='#2a2a2a', stroke='#bbbbbb')
    s += f'<path d="M450 140 L450 260" fill="none" stroke="#26c6da" stroke-width="3" stroke-dasharray="6 6"/>'
    s += text(20, 395, 'Aturan keselamatan (berhenti saat ada halangan, gerakan berbatas waktu, tombol STOP)', 12.5, fill='#e8efe9')
    s += text(20, 414, 'ada di BADAN. Kalau otak atau WiFi macet, robot tetap berhenti sendiri.', 12.5, fill='#e8efe9')
    return svg(W, H, s, 'Pembagian tugas tiga board robot')


def pembagi_echo():
    W, H = 760, 300
    s = ''
    s += box(30, 90, 170, 130, 'HC-SR04', ['pin ECHO', '(keluar 5 V)'], fill='#1d4f86', stroke='#9cc8ff')
    s += box(560, 90, 170, 130, 'DevKit V1', ['GPIO 19 (depan)', 'GPIO 35 (belakang)'], fill='#151a18')
    # resistor
    def res(x, y, vertical, warna, label):
        r = ''
        if vertical:
            r += f'<line x1="{x}" y1="{y - 34}" x2="{x}" y2="{y + 34}" stroke="#9ea4a7" stroke-width="2.4"/>'
            r += f'<rect x="{x - 8}" y="{y - 22}" width="16" height="44" rx="7" fill="#7fb0dd" stroke="#3f6f9c"/>'
            for i, c in enumerate(warna):
                r += f'<rect x="{x - 8}" y="{y - 17 + i * 8}" width="16" height="3.4" fill="{c}"/>'
            r += text(x + 16, y + 4, label, 12, font=MONO)
        else:
            r += f'<line x1="{x - 40}" y1="{y}" x2="{x + 40}" y2="{y}" stroke="#9ea4a7" stroke-width="2.4"/>'
            r += f'<rect x="{x - 24}" y="{y - 8}" width="48" height="16" rx="7" fill="#7fb0dd" stroke="#3f6f9c"/>'
            for i, c in enumerate(warna):
                r += f'<rect x="{x - 18 + i * 8}" y="{y - 8}" width="3.4" height="16" fill="{c}"/>'
            r += text(x, y - 16, label, 12, anchor='middle', font=MONO)
        return r
    n, k, m = '#7a4a22', '#1a1a1a', '#d33'
    s += line([(200, 175), (300, 175)], 'hijau', 4)
    s += res(340, 175, False, [n, k, k, n, n], '1 kΩ')
    s += line([(380, 175), (560, 175)], 'hijau', 4)
    s += f'<circle cx="460" cy="175" r="6" fill="#fff"/>'
    s += line([(460, 175), (460, 205)], 'hijau', 3)
    s += res(460, 240, True, [m, k, k, n, n], '2 kΩ')
    s += line([(460, 274), (460, 288), (700, 288)], 'gnd', 3)
    s += text(706, 292, 'GND', 12, font=MONO, weight=600)
    s += text(30, 40, 'ECHO mengeluarkan 5 V. Pin ESP32 hanya tahan 3,3 V.', 13.5, weight=600, fill='#ffd8a8')
    s += text(30, 60, 'Pembagi 1k + 2k menurunkan 5 V menjadi 5 × 2/3 = 3,3 V.', 13, fill='#e8efe9')
    s += tag(460, 150, 'titik tengah ke GPIO')
    return svg(W, H, s, 'Pembagi tegangan 1 kilo ohm dan 2 kilo ohm untuk pin ECHO')


def pembagi_baterai():
    W, H = 760, 280
    s = ''
    s += box(30, 100, 160, 80, 'Rel baterai', ['setelah saklar', '7,4 - 8,4 V'], fill='#3a1513', stroke='#ff8a80')
    s += line([(190, 140), (280, 140)], 'bat', 4)
    s += f'<line x1="280" y1="140" x2="360" y2="140" stroke="#9ea4a7" stroke-width="2.4"/><rect x="296" y="132" width="48" height="16" rx="7" fill="#7fb0dd" stroke="#3f6f9c"/>'
    s += text(320, 122, '20 kΩ', 12, anchor='middle', font=MONO)
    s += line([(360, 140), (560, 140)], 'kuning', 4)
    s += f'<circle cx="430" cy="140" r="6" fill="#fff"/>'
    s += f'<line x1="430" y1="140" x2="430" y2="220" stroke="#9ea4a7" stroke-width="2.4"/><rect x="422" y="156" width="16" height="48" rx="7" fill="#7fb0dd" stroke="#3f6f9c"/>'
    s += text(446, 184, '10 kΩ', 12, font=MONO)
    s += line([(430, 220), (430, 240), (700, 240)], 'gnd', 3)
    s += text(706, 244, 'GND', 12, font=MONO, weight=600)
    s += box(560, 100, 170, 80, 'DevKit V1', 'GPIO 34', fill='#151a18')
    s += text(30, 40, '8,4 V × 10/(20+10) = 2,8 V di GPIO 34. Aman (di bawah 3,3 V).', 13.5, weight=600, fill='#ffd8a8')
    s += text(30, 60, 'Kode mengalikan balik dengan 3 untuk mendapat tegangan baterai.', 13, fill='#e8efe9')
    return svg(W, H, s, 'Pembagi tegangan 20k dan 10k untuk membaca tegangan baterai')


# ---------------- breadboard (sama dengan panduan stasiun cuaca) ----------------
def C(c): return 70 + (c - 1) * 19
RY = {'a': 110, 'b': 130, 'c': 150, 'd': 170, 'e': 190, 'f': 230, 'g': 250, 'h': 270, 'i': 290, 'j': 310}
RAIL = {'tm': 50, 'tp': 70, 'bm': 350, 'bp': 370}
def H(s): return (C(int(s[1:])), RY[s[0]])
def RP(r, c): return (C(c), RAIL[r])
def bdot(p): return f'<circle cx="{p[0]}" cy="{p[1]}" r="3.3" fill="#fff" stroke="#111" stroke-width="1.3"/>'

def bwire(a, b, col, bend=0, end_dot=True, start_dot=True):
    c = WIRE.get(col, col)
    mx, my = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2
    dx, dy = b[0] - a[0], b[1] - a[1]
    L = (dx * dx + dy * dy) ** .5 or 1
    cx, cy = mx - dy / L * bend, my + dx / L * bend
    d = f'M{a[0]} {a[1]} Q{cx} {cy} {b[0]} {b[1]}'
    outline = '#f4f6f5' if c == WIRE['gnd'] else '#0b0d0c'
    s = (f'<path d="{d}" fill="none" stroke="{outline}" stroke-opacity=".45" stroke-width="6" stroke-linecap="round"/>'
         f'<path d="{d}" fill="none" stroke="{c}" stroke-width="3.6" stroke-linecap="round"/>')
    return s + (bdot(a) if start_dot else '') + (bdot(b) if end_dot else '')

BC = {'k': '#1a1a1a', 'n': '#7a4a22', 'r': '#d33', 'o': '#f08a24', 'y': '#f2d03b'}
def bres(a, b, bands):
    import math
    mx, my = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2
    ang = math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))
    s = f'<line x1="{a[0]}" y1="{a[1]}" x2="{b[0]}" y2="{b[1]}" stroke="#9ea4a7" stroke-width="2.2"/>'
    s += f'<g transform="translate({mx} {my}) rotate({ang})"><rect x="-14" y="-5.5" width="28" height="11" rx="5" fill="#7fb0dd" stroke="#3f6f9c" stroke-width=".8"/>'
    for i, c in enumerate(bands):
        s += f'<rect x="{-10.5 + i * 4.6 + (1.5 if i == 4 else 0)}" y="-5.5" width="2.6" height="11" fill="{BC[c]}"/>'
    return s + '</g>' + bdot(a) + bdot(b)

def btag(x, y, t):
    w = max(30, len(t) * 7.4 + 12)
    return (f'<rect x="{x - w / 2}" y="{y - 9}" width="{w}" height="18" rx="3" fill="#0d0f0e" stroke="#e9ece9" stroke-width=".6"/>'
            f'<text x="{x}" y="{y + 4}" text-anchor="middle" font-family="{MONO}" font-size="11" fill="#f4f6f5">{E(t)}</text>')

def bltag(y, t):
    return (f'<rect x="4" y="{y - 9}" width="46" height="18" rx="3" fill="#0d0f0e" stroke="#e9ece9" stroke-width=".6"/>'
            f'<text x="27" y="{y + 4}" text-anchor="middle" font-family="{MONO}" font-size="11" fill="#f4f6f5">{E(t)}</text>')

def breadboard(parts, stage, label, explain=False):
    s = f'<svg viewBox="0 0 680 430" xmlns="http://www.w3.org/2000/svg" role="img" aria-label="{E(label)}"><rect width="680" height="430" fill="{BG}"/>'
    s += grid(680, 430)
    s += '<rect x="56" y="34" width="588" height="352" rx="6" fill="#f4f4ef" stroke="#c9c9c1"/>'
    s += '<rect x="58" y="203" width="584" height="14" fill="#e3e3dc"/>'
    s += '<line x1="62" y1="40" x2="638" y2="40" stroke="#2f6fd0" stroke-width="1.6"/><line x1="62" y1="80" x2="638" y2="80" stroke="#d9473f" stroke-width="1.6"/>'
    s += '<line x1="62" y1="340" x2="638" y2="340" stroke="#2f6fd0" stroke-width="1.6"/><line x1="62" y1="380" x2="638" y2="380" stroke="#d9473f" stroke-width="1.6"/>'
    for c in range(1, 31):
        for r, y in RY.items():
            s += f'<rect x="{C(c) - 3}" y="{y - 3}" width="6" height="6" rx="1" fill="#55554f"/>'
        if c < 30:
            for r, y in RAIL.items():
                s += f'<rect x="{C(c) - 3}" y="{y - 3}" width="6" height="6" rx="1" fill="#55554f"/>'
    for r, y in RY.items():
        s += f'<text x="{C(30) + 16}" y="{y + 4}" font-size="10" fill="#7d7d76" font-family="{MONO}">{r}</text>'
    for c in (1, 5, 10, 15, 20, 25, 30):
        s += f'<text x="{C(c)}" y="97" font-size="9" fill="#7d7d76" text-anchor="middle" font-family="{MONO}">{c}</text>'
        s += f'<text x="{C(c)}" y="329" font-size="9" fill="#7d7d76" text-anchor="middle" font-family="{MONO}">{c}</text>'
    has_new = any(p[0] == stage for p in parts)
    for st, fn in parts:
        if st <= stage:
            g = fn()
            s += f'<g opacity=".35">{g}</g>' if (has_new and st < stage) else g
    return s + '</svg>'


def spi_bus():
    W, H = 880, 420
    s = ''
    s += f'<rect x="20" y="70" width="170" height="280" rx="10" fill="#151a18" stroke="#e9ece9" stroke-width="1.6"/>' + text(105, 96, 'ESP32-S3', 14, anchor='middle', weight=600)
    pins = [('IO12', 'SCK', 130), ('IO11', 'MOSI', 210), ('IO13', 'MISO', 290)]
    for p, n, y in pins:
        s += f'<rect x="176" y="{y - 7}" width="14" height="14" rx="2" fill="#d9b44a" stroke="#6d5513"/>'
        s += text(166, y + 5, p, 13, anchor='end', font=MONO)
    cols = {'SCK': ('kuning', 130, 'kolom 2'), 'MOSI': ('biru', 210, 'kolom 5'), 'MISO': ('hijau', 290, 'kolom 8')}
    for n, (c, y, k) in cols.items():
        s += f'<rect x="300" y="{y - 16}" width="200" height="32" rx="6" fill="#f4f4ef" stroke="#c9c9c1"/>'
        s += text(400, y + 5, f'breadboard {k}', 12, anchor='middle', fill='#333', font=MONO)
        s += line([(190, y), (300, y)], c, 4)
    s += box(640, 40, 220, 100, 'Layar (TFT)', ['SCK, SDI(MOSI)', 'SDO(MISO) TIDAK disambung'], fill='#8c1d18', stroke='#ffb4ab')
    s += box(640, 160, 220, 100, 'Sentuhan', ['T_CLK, T_DIN, T_DO'], fill='#8c1d18', stroke='#ffb4ab')
    s += box(640, 280, 220, 100, 'SD card (4 pad atas)', ['SD_SCK, SD_MOSI, SD_MISO'], fill='#8c1d18', stroke='#ffb4ab')
    # SCK ke TFT, touch, SD
    s += line([(500, 124), (640, 70)], 'kuning', 3) + line([(500, 128), (640, 180)], 'kuning', 3) + line([(500, 132), (640, 300)], 'kuning', 3)
    s += line([(500, 206), (640, 95)], 'biru', 3) + line([(500, 210), (640, 205)], 'biru', 3) + line([(500, 214), (640, 325)], 'biru', 3)
    s += line([(500, 288), (640, 230)], 'hijau', 3) + line([(500, 292), (640, 350)], 'hijau', 3)
    s += text(20, 30, 'Tiga kabel SPI dipakai bersama oleh layar, sentuhan, dan SD. Satu kolom breadboard = satu titik sambungan.', 12.5, fill='#e8efe9')
    s += text(20, 400, 'Yang membedakan siapa yang diajak bicara: pin CS masing-masing (TFT_CS = IO10, T_CS = IO14, SD_CS = IO21), kabel langsung.', 12, fill='#ffd8a8')
    return svg(W, H, s, 'Bus SPI bersama untuk layar, sentuhan dan SD')


def motor():
    W, H = 860, 380
    s = ''
    s += f'<rect x="300" y="90" width="220" height="200" rx="10" fill="#b3261e" stroke="#0008"/>'
    s += text(410, 80, 'TB6612FNG', 15, anchor='middle', weight=600)
    for i, p in enumerate(['VM', 'VCC', 'GND', 'AO1', 'AO2', 'BO2', 'BO1', 'GND']):
        y = 110 + i * 22
        if i < 3:   # pin daya digambar di sisi kiri supaya kabelnya tidak menutupi tulisan
            s += f'<rect x="302" y="{y - 6}" width="12" height="12" rx="2" fill="#d9b44a"/>' + text(322, y + 4, p, 12, font=MONO)
        else:
            s += f'<rect x="506" y="{y - 6}" width="12" height="12" rx="2" fill="#d9b44a"/>' + text(498, y + 4, p, 12, anchor='end', font=MONO)
    def mot(x, y, nama):
        r = f'<rect x="{x}" y="{y}" width="120" height="44" rx="8" fill="#f1c40f" stroke="#7d6608"/>'
        r += f'<circle cx="{x + 140}" cy="{y + 22}" r="22" fill="#222" stroke="#555" stroke-width="4"/>'
        r += text(x + 60, y + 27, nama, 12, anchor='middle', fill='#222', weight=600)
        return r
    s += mot(700 - 40, 40, 'kiri depan') + mot(700 - 40, 110, 'kiri belakang')
    s += mot(700 - 40, 220, 'kanan depan') + mot(700 - 40, 290, 'kanan belakang')
    # AO1/AO2 ke dua motor kiri paralel
    s += line([(518, 176), (600, 176), (600, 54), (660, 54)], 'merah' if False else '#e5392f', 3) + line([(600, 124), (660, 124)], '#e5392f', 3)
    s += line([(518, 198), (620, 198), (620, 74), (660, 74)], '#1b1b1b', 3) + line([(620, 144), (660, 144)], '#1b1b1b', 3)
    s += line([(518, 242), (600, 242), (600, 234), (660, 234)], '#e5392f', 3) + line([(600, 242), (600, 304), (660, 304)], '#e5392f', 3)
    s += line([(518, 220), (630, 220), (630, 254), (660, 254)], '#1b1b1b', 3) + line([(630, 254), (630, 324), (660, 324)], '#1b1b1b', 3)
    # VM dan GND
    s += box(20, 60, 200, 70, 'XL4015 OUT+ (6 V)', 'lewat kabel tebal', fill='#241a3a', stroke='#b08cff')
    s += line([(220, 95), (260, 95), (260, 110), (302, 110)], '6v', 4)
    s += box(20, 150, 200, 60, 'GND bersama', '', fill='#101312')
    s += line([(220, 180), (270, 180), (270, 154), (302, 154)], 'gnd', 4)
    s += box(20, 230, 200, 60, 'DevKit pin 3V', '', fill='#151a18')
    s += line([(220, 260), (285, 260), (285, 132), (302, 132)], '3v3', 3)
    s += f'<rect x="232" y="50" width="22" height="34" rx="4" fill="#20262a" stroke="#999"/>' + text(243, 44, '470 µF', 11, anchor='middle', font=MONO)
    s += text(20, 335, 'Dua motor satu sisi dipasang paralel ke satu kanal. Kabel merah/hitam motor', 12.5, fill='#e8efe9')
    s += text(20, 354, 'tidak menentukan arah: tukar saja kalau satu roda berputar terbalik.', 12.5, fill='#e8efe9')
    return svg(W, H, s, 'TB6612FNG ke empat motor TT')
