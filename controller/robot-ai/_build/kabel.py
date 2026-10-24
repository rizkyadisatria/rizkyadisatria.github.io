# Panjang potongan kabel, dihitung dari posisi akhir barang di robot (layout.py, dari foto susun kering).
# Satu sumber angka untuk semua bagian panduan, supaya tiap langkah menyebut panjang yang sama.
# Koordinat: x belakang(0) -> depan(256), y kiri(0) -> kanan(152), mm; lantai 1/2/3.
import math

Z = {1: 0, 2: 33, 3: 96}       # tinggi permukaan tiap lantai di atas plat bawah (spacer 30 mm + plat 3 mm, lalu 60 + 3)
P = {
    # lantai 2: baterai dan saklar
    'H1+': (22, 106, 2), 'H1-': (22, 33, 2),          # holder 1 (paling belakang): - di kiri, + di kanan
    'H2-': (46, 101, 2), 'H2+': (46, 28, 2),          # holder 2 (diputar): + di kiri, - di kanan
    # pad BMS (dari fotomu): 8.4V dan 0V di ujung kiri, P+ (⊕), P− (⊖), dan 4.2V di ujung kanan
    'B8': (73, 38, 2), 'B0': (87, 38, 2), 'B42': (80, 82, 2), 'BP+': (75, 74, 2), 'BP-': (86, 74, 2),
    'SEK1': (64, 113, 2), 'SEK2': (88, 113, 2),       # dua ujung dudukan sekring
    'SAK': (72, 99, 2), 'TT': (34, 104, 2),
    'XLIN': (163, 105, 2), 'XLOUT': (211, 105, 2),    # XL4015 diputar: IN di belakang, OUT di depan
    'P30': (175, 61, 2), 'BB1': (123, 75, 2), 'BB1b': (100, 60, 2), 'D30': (195, 60, 2),
    # lantai 1
    'W+': (37, 40, 1), 'WG': (37, 112, 1),            # WAGO baterai: plus di kiri, GND di kanan
    'LM1IN': (116, 77, 1), 'LM1OUT': (61, 77, 1), 'LM2IN': (127, 77, 1), 'LM2OUT': (181, 77, 1),
    'W6': (210, 39, 1), 'W6G': (210, 113, 1), 'TB': (234, 76, 1), 'MPU': (206, 74, 1),
    'MOTB': (113, 40, 1), 'MOTD': (127, 40, 1), 'HCB': (13, 76, 1),
    # lantai 3
    'S3': (77, 40, 3), 'S3U': (90, 60, 3), 'CAM': (230, 76, 3), 'KPL': (225, 82, 3), 'STOP': (136, 11, 3),
    'HCD': (245, 76, 2),
}


# Lubang plat yang dilewati kabel antar-lantai. Dipilih di titik yang kosong di KEDUA lantai yang disambungkan.
LUBANG = {
    'H7': (62, 55),    # lt1-lt2: celah holder 2 / BMS (lt2), celah motor kiri / LM2596 #1 (lt1)
    'H8': (90, 98),    # lt1-lt2: samping saklar (lt2), celah LM2596 #1 / motor kanan (lt1)
    'HXL': (154, 98),  # lt1-lt2: antara breadboard 1 dan XL4015
    'HX6': (218, 98),  # lt1-lt2: depan XL4015 (lt2), di atas WAGO 6V (lt1)
    'HSV': (230, 112), # lt1-lt2-lt3: kanan depan, kabel servo dan kamera ke kepala
    'H23': (96, 30),   # lt2-lt3: celah papan S3 / breadboard 2 (lt3)
    'HUA': (154, 30),  # lt2-lt3: celah breadboard 2 / TFT (lt3), celah breadboard 1 / DevKit (lt2)
    'HST': (115, 18),  # lt2-lt3: sayap tengah kiri, kabel tombol STOP
}
VIA = {
    'sakw': ['H7'], 'pminw': ['H8'], 'xl+': ['HXL'], 'xl-': ['HXL'], 'w6+': ['HX6'], 'w6-': ['HX6'],
    'servo+': ['HSV'], 'servo-': ['HSV'], '5v30': ['H7'], '5vs3': ['H7', 'H23'], '5vcam': ['HSV'],
    'bat': ['HXL'], 'stop': ['HST'], 'uart': ['HUA'], 'hcb': ['H7'], 'mpu': ['HX6'], 'tbsig': ['HX6'],
}


def _mm(a, b, extra=0, via=()):
    titik = [P[a][:2]] + [LUBANG[v] for v in via] + [P[b][:2]]
    datar = sum(abs(p[0] - q[0]) + abs(p[1] - q[1]) for p, q in zip(titik, titik[1:]))
    return datar + abs(Z[P[a][2]] - Z[P[b][2]]) + extra


def cm(a, b, extra=0, via=()):
    """Panjang potong: jalur lewat lubang plat (mendatar + naik-turun) + 3 cm untuk kupasan dan kelonggaran, dibulatkan ke atas."""
    return math.ceil(_mm(a, b, extra, via) / 10) + 3


# (kode, warna, ukuran, dari, ke, tambahan mm untuk naik ke kepala/gerak)
DAFTAR = [
    ('tengah', 'merah', '22', 'H1+', 'H2-', 0, 'titik tengah: holder 1 (+) → holder 2 (−)'),
    ('b8', 'merah', '22', 'H2+', 'B8', 0, 'holder 2 (+) → pad 8.4V BMS'),
    ('b0', 'hitam', '22', 'H1-', 'B0', 0, 'holder 1 (−) → pad 0V BMS'),
    ('b42', 'merah tipis bertanda kuning', '26', 'B42', 'TT', 0, 'pad 4.2V BMS → titik tengah'),
    ('sek1', 'merah (kabel dudukan sekring)', '-', 'BP+', 'SEK1', 0, 'pad ⊕ (P+) BMS → ujung 1 dudukan sekring'),
    ('sek2', 'merah (kabel dudukan sekring)', '-', 'SEK2', 'SAK', 0, 'ujung 2 dudukan sekring → saklar'),
    ('sakw', 'merah', '22', 'SAK', 'W+', 0, 'kaki saklar yang lain → WAGO + (lantai 1)'),
    ('pminw', 'hitam', '22', 'BP-', 'WG', 0, 'pad ⊖ (P−) BMS → WAGO − (lantai 1)'),
    ('lm1+', 'merah', '22', 'W+', 'LM1IN', 0, 'WAGO + → IN+ LM2596 #1'),
    ('lm1-', 'hitam', '22', 'WG', 'LM1IN', 0, 'WAGO − → IN− LM2596 #1'),
    ('xl+', 'merah', '22', 'W+', 'XLIN', 0, 'WAGO + → IN+ XL4015 (naik ke lantai 2)'),
    ('xl-', 'hitam', '22', 'WG', 'XLIN', 0, 'WAGO − → IN− XL4015 (naik ke lantai 2)'),
    ('lm2+', 'merah', '22', 'W+', 'LM2IN', 0, 'WAGO + → IN+ LM2596 #2'),
    ('lm2-', 'hitam', '22', 'WG', 'LM2IN', 0, 'WAGO − → IN− LM2596 #2'),
    ('w6+', 'hijau', '22', 'XLOUT', 'W6', 0, 'OUT+ XL4015 → WAGO 6V (turun ke lantai 1)'),
    ('w6-', 'hitam', '22', 'XLOUT', 'W6G', 0, 'OUT− XL4015 → WAGO −6 (turun ke lantai 1)'),
    ('vm', 'hijau', '22', 'W6', 'TB', 0, 'WAGO 6V → VM TB6612'),
    ('vmg', 'hitam', '22', 'W6G', 'TB', 0, 'WAGO −6 → GND (sebelah VM) TB6612'),
    ('servo+', 'hijau', '22', 'W6', 'KPL', 0, 'WAGO 6V → kepala (merah kedua servo)'),
    ('servo-', 'hitam', '22', 'W6G', 'KPL', 0, 'WAGO −6 → kepala (coklat kedua servo)'),
    # 5 V dan sinyal: 26 AWG atau kabel jumper
    ('5v30', 'merah + hitam', '26', 'LM1OUT', 'P30', 0, 'OUT LM2596 #1 → blok daya papan 30P (5V dan GND)'),
    ('5vs3', 'merah + hitam', '26', 'LM1OUT', 'S3', 0, 'OUT LM2596 #1 → blok daya papan S3 (5V dan GND)'),
    ('5vcam', 'merah + hitam', '26', 'LM2OUT', 'CAM', 60, 'OUT LM2596 #2 → pin 5V dan GND ESP32-CAM'),
    ('bat', 'merah tipis bertanda biru', '26', 'LM1IN', 'BB1', 0, 'terminal IN+ LM2596 #1 → lubang a20 breadboard 1 (ukur baterai)'),
    ('motor', 'kabel bawaan motor belakang', '-', 'MOTB', 'TB', 0, 'motor belakang → AO / BO TB6612'),
    ('motord', 'kabel bawaan motor depan', '-', 'MOTD', 'MOTD', 30, 'motor depan → disambung ke kabel motor belakang yang lewat'),
    ('hctrig', '1 kabel', 'jumper', 'BB1b', 'D30', 0, 'kolom 18 breadboard 1 → pin S D14 (TRIG belakang)'),
    ('stop', '2 kabel', '26', 'STOP', 'BB1', 0, 'tombol STOP 12 mm → kolom 27 dan 29 breadboard 1'),
    ('uart', '3 kabel F-F', 'jumper', 'D30', 'S3U', 0, 'UART: papan 30P (D16, D17, G) → papan S3 (IO17, IO18, G)'),
    ('hcb', '4 kabel', 'jumper', 'HCB', 'BB1b', 0, 'HC-SR04 belakang → breadboard 1'),
    ('mpu', '4 kabel F-F', 'jumper', 'MPU', 'P30', 0, 'MPU6050 → header I2C papan 30P'),
    ('tbsig', '9 kabel F-F', 'jumper', 'TB', 'P30', 0, 'TB6612 (PWMA, AIN1, AIN2, PWMB, BIN1, BIN2, STBY, VCC, GND) → papan 30P'),
    ('hcd', '4 kabel', 'jumper', 'HCD', 'BB1', 0, 'HC-SR04 depan → papan 30P dan breadboard 1'),
]
K = {d[0]: d for d in DAFTAR}
NO = {d[0]: i + 1 for i, d in enumerate(DAFTAR)}   # nomor kabel, sama di semua gambar dan tabel


def p(kode):
    """Panjang potong sebagai teks, misalnya '12 cm'."""
    d = K[kode]
    return f'{cm(d[3], d[4], d[5], VIA.get(kode, ()))} cm'


def pn(kode):
    """Panjang + nomor kabel di peta, misalnya '12 cm (no. 7)'."""
    return f'{p(kode)} (no. {NO[kode]})'


def total(ukuran='22'):
    t = {}
    for d in DAFTAR:
        if d[2] != ukuran:
            continue
        n = cm(d[3], d[4], d[5], VIA.get(d[0], ()))
        t[d[1]] = t.get(d[1], 0) + n
    return t


if __name__ == '__main__':
    for d in DAFTAR:
        print(f'{d[0]:7} {d[1]:30} {d[2]:6} {p(d[0]):>6}  {d[6]}')
    print('22 AWG', total('22'))
    print('26 AWG', total('26'))
