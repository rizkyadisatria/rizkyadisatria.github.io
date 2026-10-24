# Tata letak berskala (mm) robot 3 lantai untuk plat chassis Starlectric.
# x = belakang(0) -> depan(256), y = kiri(0) -> kanan(152).
# Ukuran resmi di listing: 25,6 x 10 cm, lebar ujung 15,2 cm. Bentuk "pinggang + ujung lebar" di bawah
# adalah PERKIRAAN dari foto listing (panjang ujung lebar ~ 52 mm), belum diukur di plat asli.
# Ukuran modul: perkiraan dari foto (skala pin 2,54 mm / chip) dan ukuran umum modul sejenis.
import diag
PLAT_L, PLAT_W = 257, 152
# Bentuk plat dibaca dari FOTO platmu (kotak cutting mat sebagai skala, ±1-2 mm):
#  - sayap ujung berbentuk busur, panjang ±34 mm, lebar penuh 152 mm
#  - pinggang 101 mm lebar (y 25,5 s.d. 126,5) sepanjang ±190 mm
#  - dua sayap samping di tengah (x 109-145) yang melebar lagi ke 152 mm
def _ok(x, y):
    d = abs(y - 76)
    if x < 0 or x > PLAT_L or d > 76:
        return False
    if x <= 34:
        return x >= 31 * (d / 76) ** 2
    if x >= PLAT_L - 33:
        return (PLAT_L - x) >= 31 * (d / 76) ** 2
    if 109 <= x <= 145:
        return True
    return d <= 50.5

def _outline():
    p = [(31 * (abs(y - 76) / 76) ** 2, y) for y in range(0, 153, 4)]
    p += [(34, 152), (34, 126.5), (109, 126.5), (109, 152), (145, 152), (145, 126.5), (PLAT_L - 33, 126.5), (PLAT_L - 33, 152)]
    p += [(PLAT_L - 31 * (abs(y - 76) / 76) ** 2, y) for y in range(152, -1, -4)]
    p += [(PLAT_L - 33, 0), (PLAT_L - 33, 25.5), (145, 25.5), (145, 0), (109, 0), (109, 25.5), (34, 25.5), (34, 0)]
    return p
OUTLINE = _outline()
LUAS_PLAT = sum(1 for x in range(PLAT_L) for y in range(PLAT_W) if _ok(x + .5, y + .5))   # ~28.200 mm2
LUAS_L3 = LUAS_PLAT                                              # lantai 3 = plat yang sama dengan chassis

# (nama, x, y, panjang_x, lebar_y, tinggi_mm, warna)
L1 = [  # lantai bawah, di ANTARA plat bawah dan plat atas: tinggi maks 27 mm
    # posisi motor dan LM2596 diukur dari FOTO susun keringmu (galat beberapa mm)
    ('motor', 49, 27, 64, 24, 22, '#f1c40f'), ('motor', 127, 27, 65, 24, 22, '#f1c40f'),
    ('motor', 49, 102, 64, 24, 22, '#f1c40f'), ('motor', 127, 102, 65, 24, 22, '#f1c40f'),
    ('LM2596 #1', 57, 59, 63, 36, 15, '#1d3f7a'), ('LM2596 #2', 123, 59, 62, 36, 15, '#1d3f7a'),
    ('HC-SR04 blk', 4, 53, 18, 46, 21, '#1d4f86'),
    ('WAGO +', 28, 30, 18, 20, 9, '#8a2a2a'), ('WAGO −', 28, 102, 18, 20, 9, '#4a4a4a'),
    ('MPU', 196, 66, 21, 16, 8, '#1d3a6b'),
    ('WAGO −6', 200, 30, 20, 18, 9, '#4a4a4a'), ('WAGO 6V', 200, 104, 20, 18, 9, '#2a6a3a'),
    ('TB6612', 224, 66, 20, 20, 25, '#8c1d18'),
]
L2 = [  # di atas plat atas, di bawah lantai 3: tinggi maks ~57 mm (posisi dari foto susun keringmu)
    ('holder 1', 12, 31, 21, 77, 19, '#3a1513'), ('holder 2', 36, 26, 21, 77, 19, '#3a1513'),
    ('BMS', 68, 32, 24, 53, 6, '#1d6b3a'), ('saklar', 63, 88, 18, 15, 25, '#7a1010'),
    ('sekring', 62, 108, 30, 14, 15, '#b8860b'),
    ('breadboard 1', 96, 34, 55, 83, 25, '#d9d9cf'), ('DevKit + 30P', 157, 36, 73, 50, 45, '#1d3324'),
    ('XL4015', 160, 92, 54, 27, 20, '#7a1d1d'),
    ('HC-SR04 dpn', 236, 53, 18, 46, 21, '#1d4f86'),
]
L3 = [  # di atas lantai 3 (plat ketiga, sama dengan chassis): tanpa batas tinggi (posisi dari foto)
    ('S3 + ekspansi', 13, 28, 81, 82, 30, '#13283a'), ('breadboard 2', 98, 27, 54, 82, 20, '#d9d9cf'),
    ('TFT (rebah)', 156, 30, 42, 78, 10, '#8c1d18'), ('speaker (berdiri)', 77, 114, 77, 20, 30, '#2a2a2a'),
    ('STOP', 129, 4, 15, 15, 12, '#7a1010'),
    ('kepala', 212, 58, 36, 36, 70, '#3a2a13'),
]
MENONJOL = {'speaker (berdiri)'}   # boleh menonjol sedikit dari tepi pinggang, asal diikat ke sayap tengah
HEAD = (230, 76, 30)      # pusat x, y dan jari-jari ruang putar kepala (barang setinggi >15 mm tidak boleh di sini)
TINGGI = {'L1': 27, 'L2': 57}
GAP = 3                   # jarak minimal antar-barang (mm) untuk kabel


def _di_plat(x, y, w, h):
    """True kalau seluruh kotak berada di dalam bentuk plat (titik tepi tiap 1 mm)."""
    pts = [(px, py) for px in range(int(x), int(x + w) + 1) for py in (y, y + h)]
    pts += [(px, py) for py in range(int(y), int(y + h) + 1) for px in (x, x + w)]
    return all(_ok(px, py) for px, py in pts)


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
                if n == 'kepala' or t <= 15:     # barang rebah ada di bawah bagian kepala yang berputar
                    continue
                cx, cy = min(max(hx, x), x + w), min(max(hy, y), y + h)   # titik kotak terdekat ke pusat kepala
                if (cx - hx) ** 2 + (cy - hy) ** 2 < hr ** 2:
                    overl.append((n, 'ruang putar kepala'))
            luar = [n for n, x, y, w, h, t, c in L if n != 'kepala' and n not in MENONJOL and not _di_plat(x, y, w, h)]
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
    judul = {'L3': 'LANTAI 3 (plat ketiga, 60 mm di atas): otak',
             'L2': 'LANTAI 2 (di atas plat atas): badan',
             'L1': 'LANTAI 1 (antara dua plat, tinggi 27 mm): daya'}
    for nama, L in (('L3', L3), ('L2', L2), ('L1', L1)):
        s += diag.text(30, y0 - 10, judul[nama], 13, weight=600)
        pts = ' '.join(f'{30 + x * k},{y0 + y * k}' for x, y in OUTLINE)
        s += f'<polygon points="{pts}" fill="#e9efe9" fill-opacity=".12" stroke="#e9ece9" stroke-width="1.5" stroke-linejoin="round"/>'
        if nama == 'L3':
            hx, hy, hr = HEAD
            s += f'<circle cx="{30 + hx * k}" cy="{y0 + hy * k}" r="{hr * k}" fill="none" stroke="#ffc27a" stroke-dasharray="5 5" stroke-width="1.4"/>'
        for n, x, y, w, h, t, c in L:
            X, Y = 30 + x * k, y0 + (y if True else 0) * k
            if n == 'speaker' and w == h:
                s += f'<circle cx="{X + w * k / 2}" cy="{Y + h * k / 2}" r="{w * k / 2}" fill="{c}" stroke="#bbb"/>'
            else:
                s += f'<rect x="{X}" y="{Y}" width="{w * k}" height="{h * k}" rx="3" fill="{c}" stroke="#e9ece9" stroke-opacity=".7"/>'
            fs = 10 if min(w, h) * k > 30 else (8.5 if w * k > 36 else (7.5 if w * k > 34 else 6.5))
            if n != 'motor' and not (h > w * 1.3 and w * k < 50) and len(n) * fs * 0.58 > w * k - 4:
                fs = max(6, (w * k - 4) / (len(n) * 0.58))
            col = '#222' if c in ('#f1c40f', '#d9d9cf') else '#f4f6f5'
            if n == 'motor':
                continue
            if h > w * 1.3 and w * k < 50:
                s += f'<text transform="translate({X + w * k / 2 + 3} {Y + h * k / 2}) rotate(-90)" text-anchor="middle" font-family="{diag.SANS}" font-size="{fs}" fill="{col}">{n}</text>'
            else:
                s += diag.text(X + w * k / 2, Y + h * k / 2 + 4, n, fs, anchor='middle', fill=col)
        # kaki lantai 3 (spacer bertumpuk), di lubang plat chassis: posisi contoh
        if nama in ('L2', 'L3'):
            for (sx, sy) in ((16, 29), (15, 131), (125, 24), (125, 139), (230, 24), (230, 128)):   # 6 kaki, posisi dari foto lantai 1
                s += f'<circle cx="{30 + sx * k}" cy="{y0 + sy * k}" r="4" fill="#c9a227" stroke="#000"/>'
        s += diag.text(30 + PLAT_L * k, y0 - 10, 'depan →', 11, anchor='end', fill='#ffd8a8', weight=600)
        y0 += PLAT_W * k + 48
    caps = ['Berskala; bentuk plat dan posisi barang dibaca dari foto susun keringmu.',
            'Motor (kuning) ada di sisi kiri-kanan lantai 1.',
            'Lingkaran kuning = 6 kaki lantai 3 (posisi dari fotomu).',
            'Garis putus oranye = ruang kepala menoleh.']
    for n, c in enumerate(caps):
        s += diag.text(30, y0 - 4 + 16 * n, c, 11, fill='#cfd8d4')
    y0 += 16 * len(caps)
    return diag.svg(int(W), int(y0 + 6), s, 'Tata letak berskala robot tiga lantai')


if __name__ == '__main__':
    print('luas plat', LUAS_PLAT, 'luas lantai 3', LUAS_L3)
    for r in cek():
        print(r)
