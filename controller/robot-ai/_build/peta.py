# Gambar sambungan kabel di atas tata letak asli (dilihat dari atas, depan di kanan, kiri robot di atas gambar).
# Posisi barang dari layout.py, panjang kabel dari kabel.py: gambar = tata letak = angka di langkah.
import math
import diag
import layout
import kabel as KB

WARNA = {'merah': '#ff5a4f', 'hitam': '#151515', 'hijau': '#43d07a', 'kuning': '#ffd54a', 'biru': '#6ab0ff', 'abu': '#c9c9c9'}
LANTAI = {1: layout.L1, 2: layout.L2, 3: layout.L3}


def _cari(lantai, nama):
    for it in LANTAI[lantai]:
        if it[0] == nama:
            return it
    raise KeyError(nama)


def gambar(lantai, x0, x1, barang, kabel, tanda=(), lubang=(), redup=(), k=4.0, label='Peta kabel', judul=''):
    """barang: nama di lantai ini (terang). redup: nama yang hanya digambar samar (motor dsb).
    kabel: (nomor, kode kabel.py, warna, (x,y) awal, (x,y) akhir, lengkung).
    tanda: (x, y, teks) terminal/pad. lubang: (x, y, teks) kabel pindah lantai."""
    m = 34
    W = (x1 - x0) * k + 2 * m
    H = layout.PLAT_W * k + 2 * m + 52
    X = lambda x: m + (x - x0) * k
    Y = lambda y: m + 30 + y * k
    cid = f'cp{lantai}_{x0}_{x1}'
    s = f'<defs><clipPath id="{cid}"><rect x="{m}" y="{Y(0) - 2}" width="{(x1 - x0) * k}" height="{layout.PLAT_W * k + 4}"/></clipPath></defs>'
    pts = ' '.join(f'{X(x)},{Y(y)}' for x, y in layout.OUTLINE)
    s += f'<polygon clip-path="url(#{cid})" points="{pts}" fill="#e9efe9" fill-opacity=".10" stroke="#e9ece9" stroke-width="1.5"/>'
    s += diag.text(m, 22, judul or f'LANTAI {lantai}, dilihat dari atas', 13, weight=600)
    s += diag.text(W - m, 22, 'depan →', 12, anchor='end', fill='#ffd8a8', weight=600)
    s += diag.text(m, Y(0) - 8, 'kiri robot', 11, fill='#cfd8d4')
    s += diag.text(m, Y(layout.PLAT_W) + 16, 'kanan robot', 11, fill='#cfd8d4')
    for n, x, y, w, h, t, c in [it for it in LANTAI[lantai] if it[0] in redup]:
        s += f'<rect x="{X(x)}" y="{Y(y)}" width="{w * k}" height="{h * k}" rx="4" fill="{c}" fill-opacity=".35" stroke="#e9ece9" stroke-opacity=".3"/>'
    for nama in barang:
        n, x, y, w, h, t, c = _cari(lantai, nama)
        s += f'<rect x="{X(x)}" y="{Y(y)}" width="{w * k}" height="{h * k}" rx="4" fill="{c}" stroke="#e9ece9" stroke-opacity=".8"/>'
        col = '#222' if c in ('#f1c40f', '#d9d9cf') else '#f4f6f5'
        fs = 11 if min(w, h) * k > 40 else 9.5
        if h > w * 1.3:
            s += f'<text transform="translate({X(x + w / 2) + 4} {Y(y + h / 2)}) rotate(-90)" text-anchor="middle" font-family="{diag.SANS}" font-size="{fs}" fill="{col}" fill-opacity=".85">{n}</text>'
        else:
            s += diag.text(X(x + w / 2), Y(y + h / 2) + 4, n, fs, anchor='middle', fill=col)
    for x, y, t, *_ in lubang:
        s += f'<circle cx="{X(x)}" cy="{Y(y)}" r="9" fill="#0d0f0e" stroke="#ffd8a8" stroke-width="1.6" stroke-dasharray="3 2"/>'
        s += diag.text(X(x), Y(y) + 4, '◎', 10, anchor='middle', fill='#ffd8a8')
    for x, y, t in tanda:
        s += f'<rect x="{X(x) - 7}" y="{Y(y) - 7}" width="14" height="14" rx="3" fill="#d9d2c4" stroke="#6d6250"/>'
    badges = ''
    for kode, warna, a, b, bow in kabel:
        no = KB.NO[kode]
        (ax, ay), (bx, by) = a, b
        dx, dy = bx - ax, by - ay
        L = math.hypot(dx, dy) or 1
        if isinstance(bow, tuple):
            cx, cy = bow
        else:
            cx, cy = (ax + bx) / 2 - dy / L * bow, (ay + by) / 2 + dx / L * bow
        lebar = 4 if KB.K[kode][2] == '22' else 2.6

        def garis(p0, c, p1, col, w):
            d = f'M{X(p0[0])},{Y(p0[1])} Q{X(c[0])},{Y(c[1])} {X(p1[0])},{Y(p1[1])}'
            out = ''
            if col == 'hitam':
                out += f'<path d="{d}" fill="none" stroke="#e9ece9" stroke-width="{w + 2.2}" stroke-linecap="round"/>'
            return out + f'<path d="{d}" fill="none" stroke="{WARNA[col]}" stroke-width="{w}" stroke-linecap="round"/>'

        def geser(o):   # kurva sejajar, digeser o mm ke samping
            nx, ny = -dy / L * o, dx / L * o
            return (ax + nx, ay + ny), (cx + nx, cy + ny), (bx + nx, by + ny)

        if warna == 'pasang':          # merah + hitam berdampingan (5 V dan GND, kabel motor)
            s += garis(*geser(-0.9), 'merah', lebar) + garis(*geser(0.9), 'hitam', lebar)
        elif warna == 'merah-hitam':   # separuh merah, separuh hitam, disambung di tengah (titik tengah holder)
            m0 = ((ax + cx) / 2, (ay + cy) / 2); m1 = ((cx + bx) / 2, (cy + by) / 2); mid = ((m0[0] + m1[0]) / 2, (m0[1] + m1[1]) / 2)
            s += garis((ax, ay), m0, mid, 'merah', lebar) + garis(mid, m1, (bx, by), 'hitam', lebar)
        elif warna in ('merah-kuning', 'merah-biru'):   # merah tipis dengan heat shrink berwarna di kedua ujung
            s += garis((ax, ay), (cx, cy), (bx, by), 'merah', lebar)
            tanda_w = WARNA[warna.split('-')[1]]
            for t in (0.06, 0.94):
                px = (1 - t) ** 2 * ax + 2 * (1 - t) * t * cx + t * t * bx
                py = (1 - t) ** 2 * ay + 2 * (1 - t) * t * cy + t * t * by
                s += f'<circle cx="{X(px)}" cy="{Y(py)}" r="4.5" fill="{tanda_w}" stroke="#0d0f0e"/>'
        else:
            s += garis((ax, ay), (cx, cy), (bx, by), warna, lebar)
        warna = {'pasang': 'merah', 'merah-hitam': 'merah', 'merah-kuning': 'merah', 'merah-biru': 'merah'}.get(warna, warna)
        mx, my = 0.25 * ax + 0.5 * cx + 0.25 * bx, 0.25 * ay + 0.5 * cy + 0.25 * by   # titik tengah kurva
        rim = '#e9ece9' if warna == 'hitam' else WARNA[warna]
        badges += f'<circle cx="{X(mx)}" cy="{Y(my)}" r="10.5" fill="#0d0f0e" stroke="{rim}" stroke-width="1.8"/>'
        badges += diag.text(X(mx), Y(my) + 4.5, str(no), 12, anchor='middle', weight=700)
    s += badges
    for x, y, t in tanda:
        s += diag.text(X(x), Y(y) - 11, t, 10.5, anchor='middle', weight=600, font=diag.MONO, fill='#fff6e0')
    for x, y, t, *arah in lubang:
        kiri = arah and arah[0] == 'kiri'
        s += diag.text(X(x) + (-13 if kiri else 13), Y(y) + 4, t, 10.5, anchor='end' if kiri else 'start', fill='#ffd8a8', weight=600)
    s += diag.text(m, H - 10, '◎ = lubang plat. Warna garis = warna kabelmu (abu-abu = jumper). Nomor = tabel.', 11, fill='#cfd8d4')
    return diag.svg(int(W), int(H), s, label)


def tabel(kabel, catatan=''):
    from build import table
    rows, sudah = [], set()
    for kode, warna, a, b, bow in sorted(kabel, key=lambda e: KB.NO[e[0]]):
        if kode in sudah:
            continue
        sudah.add(kode)
        no = KB.NO[kode]
        d = KB.K[kode]
        uk = {'22': '22 AWG', '26': '26 AWG', '-': '', 'jumper': ''}[d[2]]
        rows.append([f'<b>{no}</b>', f'{d[1]} {uk}'.strip(), f'<b>{KB.p(kode)}</b>', d[6]])
    return table(['No', 'Kabel', 'Panjang', 'Dari → ke'], rows, catatan)


L = KB.LUBANG
H7, H8, HXL, HX6, HSV, H23, HUA, HST = (L[k] for k in ('H7', 'H8', 'HXL', 'HX6', 'HSV', 'H23', 'HUA', 'HST'))

# ================= lantai 2 belakang: baterai (Bagian 1 Tahap 2-4) =================
BATERAI = [
    ('tengah', 'merah-hitam', (22, 106), (46, 101), 6),
    ('b42', 'merah-kuning', (80, 82), (44, 104), (56, 80)),
    ('b0', 'hitam', (22, 33), (87, 38), -14),
    ('b8', 'merah', (46, 28), (73, 38), -6),
    ('sek1', 'merah', (75, 74), (92, 115), (106, 96)),
    ('sek2', 'merah', (62, 115), (67, 103), -6),
    ('sakw', 'merah', (64, 95), H7, (60, 76)),
    ('pminw', 'hitam', (86, 74), H8, 0),
]
TANDA_BAT = [(22, 33, '−'), (22, 106, '+'), (46, 28, '+'), (46, 101, '−'),
             (73, 38, '8.4V'), (87, 38, '0V'), (75, 74, '⊕'), (86, 74, '⊖'), (80, 82, '4.2V')]


def baterai():
    return gambar(2, -2, 102, ['holder 1', 'holder 2', 'BMS', 'saklar', 'sekring'], BATERAI, TANDA_BAT,
                  [(*H7, '7 turun ke WAGO +', 'kiri'), (*H8, '8 turun', 'kiri')], redup=['breadboard 1'], k=4.6,
                  label='Peta kabel baterai di lantai 2, posisi sama dengan di robot',
                  judul='LANTAI 2 bagian belakang: baterai, BMS, sekring, saklar')


# ================= lantai 1: daya dan motor =================
LANTAI1 = [
    ('sakw', 'merah', H7, (46, 46), 3),
    ('pminw', 'hitam', H8, (46, 104), (70, 102)),
    ('lm1+', 'merah', (46, 34), (116, 70), (90, 30)),
    ('xl+', 'merah', (46, 40), HXL, (100, 56)),
    ('lm2+', 'merah', (46, 37), (127, 70), (112, 46)),
    ('lm1-', 'hitam', (46, 118), (116, 84), (90, 122)),
    ('xl-', 'hitam', (46, 110), HXL, (100, 104)),
    ('lm2-', 'hitam', (46, 114), (127, 84), (112, 110)),
    ('w6+', 'hijau', HX6, (214, 48), (224, 66)),
    ('w6-', 'hitam', HX6, (210, 104), 2),
    ('vm', 'hijau', (220, 44), (226, 72), (226, 52)),
    ('vmg', 'hitam', (220, 108), (226, 78), (228, 98)),
    ('servo+', 'hijau', (216, 46), HSV, (252, 70)),
    ('servo-', 'hitam', (218, 120), HSV, 3),
    ('5v30', 'pasang', (61, 70), H7, -3),
    ('5vs3', 'pasang', (61, 74), H7, 5),
    ('5vcam', 'pasang', (181, 74), HSV, (196, 104)),
    ('bat', 'merah-biru', (116, 66), HXL, (140, 62)),
    ('motor', 'pasang', (111, 39), (240, 70), (180, 52)),
    ('motor', 'pasang', (111, 114), (240, 82), (170, 96)),
    ('motord', 'pasang', (129, 39), (138, 44), 2),
    ('motord', 'pasang', (129, 114), (135, 106), -2),
    ('hcb', 'abu', (22, 76), H7, -8),
    ('mpu', 'abu', (206, 82), HX6, 4),
    ('tbsig', 'abu', (234, 86), HX6, -4),
]
TANDA_L1 = [(116, 70, 'IN+'), (116, 84, 'IN−'), (127, 70, 'IN+'), (127, 84, 'IN−'), (61, 72, 'OUT'), (181, 76, 'OUT'),
            (226, 72, 'VM'), (226, 78, 'GND'), (240, 70, 'A'), (240, 82, 'B')]


L1_DAYA = {'sakw', 'pminw', 'lm1+', 'lm1-', 'lm2+', 'lm2-', 'xl+', 'xl-', 'w6+', 'w6-', '5v30', '5vs3', '5vcam', 'bat'}
L1_MOTOR = {'w6+', 'w6-', 'vm', 'vmg', 'servo+', 'servo-', 'motor', 'motord', 'mpu', 'tbsig', 'hcb'}


def kabel_l1(pilih=None):
    return [e for e in LANTAI1 if pilih is None or e[0] in pilih]


def lantai1(pilih=None, judul='LANTAI 1: WAGO, konverter, TB6612, motor'):
    return gambar(1, -2, 258, ['LM2596 #1', 'LM2596 #2', 'WAGO +', 'WAGO −', 'WAGO 6V', 'WAGO −6', 'TB6612', 'MPU', 'HC-SR04 blk'],
                  kabel_l1(pilih), TANDA_L1,
                  [(*H7, 'H7'), (*H8, 'H8'), (*HXL, 'HXL'), (*HX6, 'HX6', 'kiri'), (*HSV, 'HSV', 'kiri')],
                  redup=['motor'], k=3.6, label='Peta kabel lantai 1, posisi sama dengan di robot', judul=judul)


# ================= lantai 2 depan: badan =================
LANTAI2 = [
    ('xl+', 'merah', HXL, (163, 99), 2),
    ('xl-', 'hitam', HXL, (163, 112), -2),
    ('w6+', 'hijau', (211, 99), HX6, -2),
    ('w6-', 'hitam', (211, 112), HX6, 2),
    ('5v30', 'pasang', H7, (172, 60), (110, 26)),
    ('5vs3', 'pasang', H7, H23, -6),
    ('bat', 'merah-biru', HXL, (123, 75), 6),
    ('hcb', 'abu', H7, (100, 60), 4),
    ('hctrig', 'abu', (104, 62), (195, 64), (150, 70)),
    ('stop', 'abu', HST, (130, 75), (118, 50)),
    ('uart', 'abu', (195, 60), HUA, (180, 30)),
    ('mpu', 'abu', HX6, (205, 42), (226, 66)),
    ('tbsig', 'abu', HX6, (190, 50), (210, 80)),
    ('hcd', 'abu', (240, 76), (150, 80), (200, 92)),
]


def lantai2():
    return gambar(2, 88, 258, ['breadboard 1', 'DevKit + 30P', 'XL4015', 'HC-SR04 dpn'], LANTAI2,
                  [(163, 99, 'IN+'), (163, 112, 'IN−'), (211, 99, 'OUT+'), (211, 112, 'OUT−'), (172, 60, '5V'), (195, 60, 'D16'), (123, 75, 'a20')],
                  [(*H7, 'H7'), (*HXL, 'HXL'), (*HX6, 'HX6'), (*HSV, 'HSV'), (*H23, 'H23'), (*HUA, 'HUA'), (*HST, 'HST')],
                  redup=['sekring', 'saklar', 'BMS'], k=4.4, label='Peta kabel lantai 2 bagian depan, posisi sama dengan di robot',
                  judul='LANTAI 2 bagian depan: DevKit, XL4015, breadboard 1')


# ================= lantai 3: otak dan kepala =================
LANTAI3 = [
    ('5vs3', 'pasang', H23, (77, 40), 3),
    ('uart', 'abu', HUA, (90, 60), (120, 40)),
    ('stop', 'abu', (136, 11), HST, 2),
    ('servo+', 'hijau', HSV, (225, 84), -4),
    ('servo-', 'hitam', HSV, (229, 84), 4),
    ('5vcam', 'pasang', HSV, (232, 76), (246, 104)),
]


def lantai3():
    return gambar(3, -2, 258, ['S3 + ekspansi', 'breadboard 2', 'TFT (rebah)', 'speaker (berdiri)', 'STOP', 'kepala'], LANTAI3,
                  [(77, 40, '5V'), (90, 60, 'IO17'), (225, 84, ''), (232, 76, 'CAM')],
                  [(*H23, 'H23'), (*HUA, 'HUA'), (*HST, 'HST'), (*HSV, 'HSV')],
                  k=3.6, label='Peta kabel lantai 3, posisi sama dengan di robot',
                  judul='LANTAI 3: otak, layar, kepala')
