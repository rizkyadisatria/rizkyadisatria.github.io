# Tata letak berskala (mm) robot 3 lantai. x = belakang(0) -> depan(255), y = kiri(0) -> kanan(148).
# Ukuran modul: perkiraan dari foto (skala pin 2,54 mm / chip) dan ukuran umum modul sejenis.
import diag
PLAT_L, PLAT_W = 255, 148

# (nama, x, y, panjang_x, lebar_y, tinggi_mm, warna)
L1 = [  # lantai bawah, di ANTARA plat bawah dan plat atas: tinggi maks 27 mm
    ('motor', 15, 0, 70, 23, 19, '#f1c40f'), ('motor', 15, 125, 70, 23, 19, '#f1c40f'),
    ('motor', 170, 0, 70, 23, 19, '#f1c40f'), ('motor', 170, 125, 70, 23, 19, '#f1c40f'),
    ('HC-SR04 blk', 3, 51, 16, 46, 21, '#1d4f86'),
    ('LM2596 #1', 95, 26, 66, 36, 15, '#33240f'), ('LM2596 #2', 95, 64, 66, 36, 15, '#33240f'),
    ('XL4015', 100, 102, 54, 29, 20, '#241a3a'),
    ('TB6612', 182, 64, 20, 20, 25, '#8c1d18'),
    ('WAGO', 205, 38, 20, 18, 9, '#5a5a5a'), ('WAGO', 205, 92, 20, 18, 9, '#5a5a5a'),
    ('WAGO', 30, 34, 20, 18, 9, '#5a5a5a'), ('WAGO', 30, 96, 20, 18, 9, '#5a5a5a'),
    ('MPU', 115, 3, 21, 16, 8, '#1d3a6b'),
]
L2 = [  # di atas plat atas, di bawah lantai 3: tinggi maks ~57 mm
    ('holder 1', 8, 35, 21, 77, 19, '#3a1513'), ('holder 2', 31, 35, 21, 77, 19, '#3a1513'),
    ('BMS', 56, 49, 22, 50, 6, '#1d6b3a'), ('saklar', 58, 112, 15, 21, 25, '#7a1010'),
    ('DevKit + 30P', 92, 14, 72, 50, 45, '#1d3324'), ('breadboard 1', 92, 76, 83, 55, 25, '#d9d9cf'),
    ('HC-SR04 dpn', 236, 51, 16, 46, 21, '#1d4f86'),
]
L3 = [  # di atas lantai 3 (plat baru): tanpa batas tinggi
    ('S3 + ekspansi', 15, 10, 83, 81, 30, '#13283a'), ('breadboard 2', 102, 10, 55, 83, 20, '#d9d9cf'),
    ('speaker', 35, 92, 45, 45, 20, '#2a2a2a'), ('STOP', 85, 112, 16, 16, 12, '#7a1010'),
    ('layar TFT', 162, 38, 30, 72, 37, '#8c1d18'), ('kepala', 205, 54, 40, 40, 70, '#3a2a13'),
]
TINGGI = {'L1': 27, 'L2': 57}

def cek():
    import itertools
    out = []
    for nama, L in (('L1', L1), ('L2', L2), ('L3', L3)):
        luas = sum(w * h for n, x, y, w, h, t, c in L if n != 'motor')
        bebas = PLAT_L * PLAT_W * 0.85 - sum(w * h for n, x, y, w, h, t, c in L if n == 'motor')
        overl = [(a[0], b[0]) for a, b in itertools.combinations(L, 2)
                 if a[1] < b[1] + b[3] and b[1] < a[1] + a[3] and a[2] < b[2] + b[4] and b[2] < a[2] + a[4]]
        luar = [n for n, x, y, w, h, t, c in L if x < 0 or y < 0 or x + w > PLAT_L or y + h > PLAT_W]
        tinggi = [(n, t) for n, x, y, w, h, t, c in L if nama in TINGGI and t > TINGGI[nama]]
        out.append((nama, round(luas / bebas * 100), overl, luar, tinggi))
    return out

def svg():
    k = 1.55
    W = 30 + PLAT_L * k + 30
    s = ''
    y0 = 40
    judul = {'L3': 'LANTAI 3 (plat baru, 60 mm di atas): otak',
             'L2': 'LANTAI 2 (di atas plat atas): badan',
             'L1': 'LANTAI 1 (antara dua plat, tinggi 27 mm): daya'}
    for nama, L in (('L3', L3), ('L2', L2), ('L1', L1)):
        s += diag.text(30, y0 - 10, judul[nama], 13, weight=600)
        s += f'<rect x="30" y="{y0}" width="{PLAT_L * k}" height="{PLAT_W * k}" rx="{30 * k}" fill="#e9efe9" fill-opacity=".12" stroke="#e9ece9" stroke-width="1.5"/>'
        if nama == 'L3':
            cx, cy = 30 + 225 * k, y0 + 74 * k
            s += f'<circle cx="{cx}" cy="{cy}" r="{36 * k}" fill="none" stroke="#ffc27a" stroke-dasharray="5 5" stroke-width="1.4"/>'
        for n, x, y, w, h, t, c in L:
            X, Y = 30 + x * k, y0 + y * k
            if n == 'speaker':
                s += f'<circle cx="{X + w * k / 2}" cy="{Y + h * k / 2}" r="{w * k / 2}" fill="{c}" stroke="#bbb"/>'
            else:
                s += f'<rect x="{X}" y="{Y}" width="{w * k}" height="{h * k}" rx="3" fill="{c}" stroke="#e9ece9" stroke-opacity=".7"/>'
            fs = 10 if min(w, h) * k > 30 else 8.5
            col = '#222' if c in ('#f1c40f', '#d9d9cf') else '#f4f6f5'
            if n == 'motor':
                continue
            if h > w * 1.3 and w * k < 50:
                s += f'<text transform="translate({X + w * k / 2 + 3} {Y + h * k / 2}) rotate(-90)" text-anchor="middle" font-family="{diag.SANS}" font-size="{fs}" fill="{col}">{n}</text>'
            else:
                s += diag.text(X + w * k / 2, Y + h * k / 2 + 4, n, fs, anchor='middle', fill=col)
        # spacer lantai 3 (di atas 6 spacer kit)
        if nama in ('L2', 'L3'):
            for (sx, sy) in ((40, 6), (40, 142), (128, 6), (128, 142), (215, 6), (215, 142)):
                s += f'<circle cx="{30 + sx * k}" cy="{y0 + sy * k}" r="4" fill="#c9a227" stroke="#000"/>'
        s += diag.text(30 + PLAT_L * k, y0 - 10, 'depan →', 11, anchor='end', fill='#ffd8a8', weight=600)
        y0 += PLAT_W * k + 48
    s += diag.text(30, y0 - 22, 'Digambar berskala dari ukuran modul. Lingkaran kuning = spacer: lantai 3', 11, fill='#cfd8d4')
    s += diag.text(30, y0 - 6, 'berdiri di atas 6 spacer kit. Garis putus oranye = ruang kepala menoleh.', 11, fill='#cfd8d4')
    return diag.svg(int(W), int(y0 + 6), s, 'Tata letak berskala robot tiga lantai')

if __name__ == '__main__':
    for r in cek(): print(r)
