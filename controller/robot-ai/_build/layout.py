# Tata letak berskala (mm) robot 3 lantai untuk plat chassis Starlectric.
# x = belakang(0) -> depan(256), y = kiri(0) -> kanan(152).
# Ukuran resmi di listing: 25,6 x 10 cm, lebar ujung 15,2 cm. Bentuk "pinggang + ujung lebar" di bawah
# adalah PERKIRAAN dari foto listing (panjang ujung lebar ~ 52 mm), belum diukur di plat asli.
# Ukuran modul: perkiraan dari foto (skala pin 2,54 mm / chip) dan ukuran umum modul sejenis.
import diag
PLAT_L, PLAT_W = 256, 152
# persegi-persegi yang membentuk plat chassis: (x1, y1, x2, y2)
PLAT = [(0, 22, 10, 130), (10, 0, 52, 152), (52, 26, 204, 126), (204, 0, 246, 152), (246, 22, 256, 130)]
OUTLINE = [(0, 22), (10, 22), (10, 0), (52, 0), (52, 26), (204, 26), (204, 0), (246, 0), (246, 22), (256, 22),
           (256, 130), (246, 130), (246, 152), (204, 152), (204, 126), (52, 126), (52, 152), (10, 152), (10, 130), (0, 130)]
LUAS_PLAT = sum((c - a) * (d - b) for a, b, c, d in PLAT)      # ~30.100 mm2
LUAS_L3 = PLAT_L * PLAT_W                                       # lantai 3 = papan persegi 256 x 152

# (nama, x, y, panjang_x, lebar_y, tinggi_mm, warna)
L1 = [  # lantai bawah, di ANTARA plat bawah dan plat atas: tinggi maks 27 mm
    ('motor', 54, 26, 70, 23, 19, '#f1c40f'), ('motor', 132, 26, 70, 23, 19, '#f1c40f'),
    ('motor', 54, 103, 70, 23, 19, '#f1c40f'), ('motor', 132, 103, 70, 23, 19, '#f1c40f'),
    ('HC-SR04 blk', 1, 53, 16, 46, 21, '#1d4f86'),
    ('XL4015', 21, 49, 29, 54, 20, '#241a3a'),
    ('LM2596 #1', 60, 58, 66, 36, 15, '#33240f'), ('LM2596 #2', 132, 58, 66, 36, 15, '#33240f'),
    ('TB6612', 206, 66, 20, 20, 25, '#8c1d18'), ('MPU', 232, 66, 21, 16, 8, '#1d3a6b'),
    ('WAGO', 24, 18, 20, 18, 9, '#5a5a5a'), ('WAGO', 24, 116, 20, 18, 9, '#5a5a5a'),
    ('WAGO', 206, 20, 20, 18, 9, '#5a5a5a'), ('WAGO', 206, 114, 20, 18, 9, '#5a5a5a'),
]
L2 = [  # di atas plat atas, di bawah lantai 3: tinggi maks ~57 mm
    ('holder 1', 6, 38, 21, 77, 19, '#3a1513'), ('holder 2', 32, 38, 21, 77, 19, '#3a1513'),
    ('BMS', 59, 40, 22, 50, 6, '#1d6b3a'), ('saklar', 61, 100, 15, 21, 25, '#7a1010'),
    ('breadboard 1', 88, 33, 55, 83, 25, '#d9d9cf'), ('DevKit + 30P', 151, 38, 72, 50, 45, '#1d3324'),
    ('HC-SR04 dpn', 236, 53, 16, 46, 21, '#1d4f86'),
]
L3 = [  # di atas lantai 3 (papan persegi baru): tanpa batas tinggi
    ('S3 + ekspansi', 2, 4, 83, 81, 30, '#13283a'), ('breadboard 2', 90, 34, 55, 83, 20, '#d9d9cf'),
    ('layar TFT', 152, 40, 30, 72, 37, '#8c1d18'), ('speaker', 6, 102, 58, 30, 25, '#2a2a2a'),
    ('STOP', 95, 125, 16, 16, 12, '#7a1010'), ('kepala', 202, 56, 40, 40, 70, '#3a2a13'),
]
HEAD = (222, 76, 34)      # pusat x, y dan jari-jari ruang putar kepala (tidak boleh ada barang lain di sini)
TINGGI = {'L1': 27, 'L2': 57}
GAP = 4                   # jarak minimal antar-barang (mm) untuk kabel


def _di_plat(x, y, w, h):
    """True kalau seluruh kotak berada di dalam bentuk plat (cek titik di sepanjang tepi, tiap 1 mm)."""
    def ok(px, py):
        return any(a <= px <= c and b <= py <= d for a, b, c, d in PLAT)
    pts = [(px, py) for px in range(int(x), int(x + w) + 1) for py in (y, y + h)]
    pts += [(px, py) for py in range(int(y), int(y + h) + 1) for px in (x, x + w)]
    return all(ok(px, py) for px, py in pts)


def cek():
    import itertools
    out = []
    for nama, L in (('L1', L1), ('L2', L2), ('L3', L3)):
        luas_plat = LUAS_L3 if nama == 'L3' else LUAS_PLAT
        luas = sum(w * h for n, x, y, w, h, t, c in L if n != 'motor')
        if nama == 'L3':
            luas += 3.14159 * HEAD[2] ** 2 - 40 * 40            # kepala memakai lingkaran ruang putar
        bebas = luas_plat * 0.9 - sum(w * h for n, x, y, w, h, t, c in L if n == 'motor')
        overl = [(a[0], b[0]) for a, b in itertools.combinations(L, 2) if n_motor(a, b) is False and
                 a[1] < b[1] + b[3] + GAP and b[1] < a[1] + a[3] + GAP and a[2] < b[2] + b[4] + GAP and b[2] < a[2] + a[4] + GAP]
        if nama == 'L3':
            hx, hy, hr = HEAD
            for n, x, y, w, h, t, c in L:
                if n == 'kepala':
                    continue
                cx, cy = min(max(hx, x), x + w), min(max(hy, y), y + h)   # titik kotak terdekat ke pusat kepala
                if (cx - hx) ** 2 + (cy - hy) ** 2 < hr ** 2:
                    overl.append((n, 'ruang putar kepala'))
            luar = [n for n, x, y, w, h, t, c in L if x < 0 or y < 0 or x + w > PLAT_L or y + h > PLAT_W and n != 'kepala']
        else:
            luar = [n for n, x, y, w, h, t, c in L if not _di_plat(x, y, w, h)]
        tinggi = [(n, t) for n, x, y, w, h, t, c in L if nama in TINGGI and t > TINGGI[nama]]
        out.append((nama, round(luas / bebas * 100), overl, luar, tinggi))
    return out


def n_motor(a, b):
    return a[0] == 'motor' and b[0] == 'motor'


def svg():
    k = 1.55
    W = 30 + PLAT_L * k + 30
    s = ''
    y0 = 40
    judul = {'L3': 'LANTAI 3 (papan persegi, 60 mm di atas): otak',
             'L2': 'LANTAI 2 (di atas plat atas): badan',
             'L1': 'LANTAI 1 (antara dua plat, tinggi 27 mm): daya'}
    for nama, L in (('L3', L3), ('L2', L2), ('L1', L1)):
        s += diag.text(30, y0 - 10, judul[nama], 13, weight=600)
        pts = ' '.join(f'{30 + x * k},{y0 + y * k}' for x, y in OUTLINE)
        if nama == 'L3':
            s += f'<rect x="30" y="{y0}" width="{PLAT_L * k}" height="{PLAT_W * k}" rx="6" fill="#e9efe9" fill-opacity=".12" stroke="#e9ece9" stroke-width="1.5"/>'
            hx, hy, hr = HEAD
            s += f'<circle cx="{30 + hx * k}" cy="{y0 + hy * k}" r="{hr * k}" fill="none" stroke="#ffc27a" stroke-dasharray="5 5" stroke-width="1.4"/>'
        else:
            s += f'<polygon points="{pts}" fill="#e9efe9" fill-opacity=".12" stroke="#e9ece9" stroke-width="1.5" stroke-linejoin="round"/>'
        for n, x, y, w, h, t, c in L:
            X, Y = 30 + x * k, y0 + (y if True else 0) * k
            if n == 'speaker' and w == h:
                s += f'<circle cx="{X + w * k / 2}" cy="{Y + h * k / 2}" r="{w * k / 2}" fill="{c}" stroke="#bbb"/>'
            else:
                s += f'<rect x="{X}" y="{Y}" width="{w * k}" height="{h * k}" rx="3" fill="{c}" stroke="#e9ece9" stroke-opacity=".7"/>'
            fs = 10 if min(w, h) * k > 30 else (8.5 if w * k > 36 else 7.5)
            col = '#222' if c in ('#f1c40f', '#d9d9cf') else '#f4f6f5'
            if n == 'motor':
                continue
            if h > w * 1.3 and w * k < 50:
                s += f'<text transform="translate({X + w * k / 2 + 3} {Y + h * k / 2}) rotate(-90)" text-anchor="middle" font-family="{diag.SANS}" font-size="{fs}" fill="{col}">{n}</text>'
            else:
                s += diag.text(X + w * k / 2, Y + h * k / 2 + 4, n, fs, anchor='middle', fill=col)
        # kaki lantai 3 (spacer bertumpuk), di lubang plat chassis: posisi contoh
        if nama in ('L2', 'L3'):
            for (sx, sy) in ((14, 10), (14, 142), (128, 32), (128, 120), (240, 10), (240, 142)):
                s += f'<circle cx="{30 + sx * k}" cy="{y0 + sy * k}" r="4" fill="#c9a227" stroke="#000"/>'
        s += diag.text(30 + PLAT_L * k, y0 - 10, 'depan →', 11, anchor='end', fill='#ffd8a8', weight=600)
        y0 += PLAT_W * k + 48
    caps = ['Berskala, dari ukuran listing plat (25,6 x 10 cm, ujung 15,2 cm).',
            'Bentuk ujung lebar adalah perkiraan dari foto.',
            'Lingkaran kuning = contoh kaki lantai 3 (pakai lubang platmu).',
            'Garis putus oranye = ruang kepala menoleh.']
    for n, c in enumerate(caps):
        s += diag.text(30, y0 - 4 + 16 * n, c, 11, fill='#cfd8d4')
    y0 += 16 * len(caps)
    return diag.svg(int(W), int(y0 + 6), s, 'Tata letak berskala robot tiga lantai')


if __name__ == '__main__':
    print('luas plat', LUAS_PLAT, 'luas lantai 3', LUAS_L3)
    for r in cek():
        print(r)
