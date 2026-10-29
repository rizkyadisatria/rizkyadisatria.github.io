# Diagram SVG untuk halaman Elektronika Dasar. Gaya sama dengan diagram panduan robot (diag.py).
import math, os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', '..', 'robot-ai', '_build'))
from diag import svg, text, line, box, tag, WIRE, MONO  # noqa: E402
import cover as cv  # noqa: E402

KAWAT = '#9ea4a7'
KUNING_ARUS = '#ffd166'
MERAH_SALAH = '#ff8a80'
HIJAU_OK = '#8be28b'


def panah(x1, y1, x2, y2, col=KUNING_ARUS, w=2.6):
    a = math.atan2(y2 - y1, x2 - x1)
    L = 10
    p1 = (x2 - L * math.cos(a - .45), y2 - L * math.sin(a - .45))
    p2 = (x2 - L * math.cos(a + .45), y2 - L * math.sin(a + .45))
    return (f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="{col}" stroke-width="{w}" stroke-linecap="round"/>'
            f'<polygon points="{x2},{y2} {p1[0]:.1f},{p1[1]:.1f} {p2[0]:.1f},{p2[1]:.1f}" fill="{col}"/>')


def res_h(x1, x2, y, label='', lab_y=None):
    """Resistor mendatar dengan kaki kawat, gaya sama dengan diagram robot."""
    m = (x1 + x2) / 2
    s = f'<line x1="{x1}" y1="{y}" x2="{x2}" y2="{y}" stroke="{KAWAT}" stroke-width="2.4"/>'
    s += f'<rect x="{m - 26}" y="{y - 9}" width="52" height="18" rx="8" fill="#7fb0dd" stroke="#3f6f9c"/>'
    if label:
        s += text(m, lab_y if lab_y is not None else y - 16, label, 12, anchor='middle', font=MONO)
    return s


def res_v(x, y1, y2, label='', kanan=True):
    m = (y1 + y2) / 2
    s = f'<line x1="{x}" y1="{y1}" x2="{x}" y2="{y2}" stroke="{KAWAT}" stroke-width="2.4"/>'
    s += f'<rect x="{x - 9}" y="{m - 26}" width="18" height="52" rx="8" fill="#7fb0dd" stroke="#3f6f9c"/>'
    if label:
        s += text(x + 18 if kanan else x - 18, m + 4, label, 12, anchor='start' if kanan else 'end', font=MONO)
    return s


def baterai_v(x, y, sel=2):
    """Simbol baterai tegak. Kutub + di atas (y), − di bawah. Mengembalikan (svg, y_bawah)."""
    s = ''
    yy = y
    for i in range(sel):
        s += f'<line x1="{x - 22}" y1="{yy}" x2="{x + 22}" y2="{yy}" stroke="#f4f6f5" stroke-width="3"/>'
        s += f'<line x1="{x - 11}" y1="{yy + 11}" x2="{x + 11}" y2="{yy + 11}" stroke="#f4f6f5" stroke-width="5"/>'
        yy += 22
    s += text(x + 30, y + 6, '+', 15, weight=700, fill=WIRE['5v']) + text(x + 30, yy - 6, '−', 15, weight=700)
    return s, yy - 11


def led_v(x, y, h=56, warna='#ff6b5e'):
    """LED tegak, arus mengalir dari atas (anoda) ke bawah (katoda)."""
    t = y + 14
    b = y + h - 18
    s = f'<line x1="{x}" y1="{y}" x2="{x}" y2="{y + h}" stroke="{KAWAT}" stroke-width="2.4"/>'
    s += f'<polygon points="{x - 13},{t} {x + 13},{t} {x},{b}" fill="{warna}" stroke="#111" stroke-width="1.2"/>'
    s += f'<line x1="{x - 14}" y1="{b}" x2="{x + 14}" y2="{b}" stroke="#f4f6f5" stroke-width="3"/>'
    for d in (0, 12):
        s += panah(x + 16, t + 6 + d, x + 30, t - 4 + d, '#ffd8a8', 1.6)
    return s


def gnd_sym(x, y):
    s = f'<line x1="{x}" y1="{y - 14}" x2="{x}" y2="{y}" stroke="#f4f6f5" stroke-width="2.4"/>'
    for i, w in enumerate((18, 12, 6)):
        s += f'<line x1="{x - w}" y1="{y + i * 6}" x2="{x + w}" y2="{y + i * 6}" stroke="#f4f6f5" stroke-width="2.4"/>'
    return s


def meter(cx, cy, huruf, warna='#f2c230'):
    return (f'<circle cx="{cx}" cy="{cy}" r="22" fill="{warna}" stroke="#111" stroke-width="1.5"/>'
            + text(cx, cy + 7, huruf, 20, anchor='middle', fill='#1b1b1b', weight=700))


# ---------------- diagram ----------------

def rangkaian():
    W, H = 760, 380
    s = text(30, 34, 'Rangkaian tertutup: baterai, resistor, LED', 16, weight=600)
    # kawat + (merah) dari baterai ke LED, kawat − (hitam) kembali
    s += line([(110, 168), (110, 110), (250, 110)], '5v', 4) + line([(370, 110), (650, 110), (650, 170)], '5v', 4)
    s += line([(650, 226), (650, 290), (110, 290), (110, 222)], 'gnd', 4)
    b, yb = baterai_v(110, 180)
    s += b
    s += text(30, 196, 'baterai', 12.5) + text(30, 213, '6 V', 12.5, font=MONO)
    s += res_h(250, 370, 110, 'resistor 220 Ω', lab_y=96)
    s += led_v(650, 170) + text(690, 204, 'LED merah', 12.5) + text(690, 221, '(turun 2 V)', 11.5, fill='#cfd8d4')
    # arah arus
    s += panah(450, 128, 520, 128) + panah(668, 240, 668, 280) + panah(420, 272, 350, 272) + panah(92, 262, 92, 236)
    s += text(470, 150, 'arus', 12, fill=KUNING_ARUS, anchor='middle')
    # voltmeter di dua ujung resistor
    s += '<rect x="250" y="160" width="120" height="58" rx="10" fill="#f2c230" stroke="#111" stroke-width="1.5"/>'
    s += '<rect x="262" y="168" width="96" height="26" rx="4" fill="#1d2b22"/>' + text(310, 187, '4,0 V', 15, anchor='middle', fill='#b6f5c1', font=MONO, weight=600)
    s += text(310, 210, 'V⎓', 12, anchor='middle', fill='#1b1b1b', weight=700)
    s += f'<path d="M285 160 C 280 140 262 130 256 114" fill="none" stroke="{WIRE["5v"]}" stroke-width="2.2" stroke-dasharray="5 3"/>'
    s += '<path d="M335 160 C 340 140 358 130 364 114" fill="none" stroke="#111" stroke-width="2.2" stroke-dasharray="5 3"/>'
    s += '<circle cx="256" cy="110" r="4.5" fill="#fff" stroke="#111"/><circle cx="364" cy="110" r="4.5" fill="#fff" stroke="#111"/>'
    s += text(390, 196, 'tegangan diukur di antara dua titik', 12, fill='#e8efe9')
    s += text(30, 330, 'Arus keluar dari +, lewat resistor dan LED, lalu kembali ke −. Besarnya sama di semua titik: 4 V / 220 Ω = 18 mA.', 12.5, fill='#e8efe9')
    s += text(30, 352, 'Putus di satu titik mana pun (kabel lepas, saklar OFF), arus berhenti di seluruh rangkaian.', 12.5, fill='#ffd8a8')
    return svg(W, H, s, 'Rangkaian tertutup baterai 6 V, resistor 220 ohm dan LED merah, dengan voltmeter di dua ujung resistor')


def gnd_bersama():
    W, H = 760, 430
    s = ''
    for k, ox in enumerate((0, 390)):
        ada = k == 1
        s += text(ox + 20, 32, 'DENGAN GND bersama' if ada else 'TANPA GND bersama', 14.5, weight=700, fill=HIJAU_OK if ada else MERAH_SALAH)
        s += box(ox + 20, 56, 120, 94, 'ESP32', ['daya dari USB'], fill='#151a18')
        s += box(ox + 230, 56, 120, 94, 'Servo', ['sinyal · + · −'], fill='#1f4e79', stroke='#8ec5ff')
        s += box(ox + 230, 262, 120, 64, 'XL4015', ['6 V'], fill='#241a3a', stroke='#b08cff')
        s += line([(ox + 140, 86), (ox + 230, 86)], 'oranye', 4)
        s += text(ox + 185, 76, 'sinyal', 11.5, anchor='middle', fill='#ffd8a8')
        s += line([(ox + 268, 150), (ox + 268, 262)], '5v', 4) + line([(ox + 312, 150), (ox + 312, 262)], 'gnd', 4)
        s += text(ox + 262, 210, '+', 13, anchor='end', weight=700, fill=WIRE['5v']) + text(ox + 320, 210, '−', 13, weight=700)
        s += text(ox + 66, 170, 'GND', 11.5, anchor='end', font=MONO)
        if ada:
            s += line([(ox + 80, 150), (ox + 80, 360), (ox + 312, 360), (ox + 312, 326)], 'gnd', 4)
            s += f'<path d="M{ox + 146} 96 H{ox + 222}" stroke="{KUNING_ARUS}" stroke-width="2" stroke-dasharray="5 4" fill="none"/>'
            s += f'<path d="M{ox + 300} 156 V{ox * 0 + 254}" stroke="{KUNING_ARUS}" stroke-width="2" stroke-dasharray="5 4" fill="none"/>'
            s += f'<path d="M{ox + 300} 340 V{348} H{ox + 96} V{160}" stroke="{KUNING_ARUS}" stroke-width="2" stroke-dasharray="5 4" fill="none"/>'
            s += text(ox + 106, 336, 'jalur pulang sinyal', 11.5, fill=KUNING_ARUS)
            s += text(ox + 20, 392, 'Sinyal 3,3 V diukur terhadap GND yang sama,', 12, fill='#e8efe9')
            s += text(ox + 20, 410, 'jadi servo membaca perintah dengan benar.', 12, fill='#e8efe9')
        else:
            s += line([(ox + 80, 150), (ox + 80, 196)], 'gnd', 4)
            s += f'<circle cx="{ox + 80}" cy="210" r="11" fill="none" stroke="{MERAH_SALAH}" stroke-width="2.4"/>'
            s += text(ox + 80, 215, '?', 14, anchor='middle', weight=700, fill=MERAH_SALAH)
            s += text(ox + 98, 214, 'tidak tersambung', 11.5, fill=MERAH_SALAH)
            s += text(ox + 20, 392, 'Sinyal tidak punya jalur pulang. Servo membaca', 12, fill='#e8efe9')
            s += text(ox + 20, 410, 'tegangan yang melayang: gerak acak atau diam.', 12, fill='#e8efe9')
    s += '<line x1="380" y1="20" x2="380" y2="410" stroke="#fff" stroke-opacity=".25" stroke-dasharray="4 5"/>'
    return svg(W, H, s, 'Perbandingan ESP32 dan servo tanpa GND bersama dan dengan GND bersama')


def seri_paralel():
    W, H = 760, 350
    s = text(30, 34, 'SERI: tegangan dijumlah', 15, weight=700)
    for i, x in enumerate((60, 226)):
        s += f'<rect x="{x}" y="96" width="150" height="56" rx="10" fill="#3a7bd5" stroke="#0b0d0c" stroke-width="1.4"/>'
        s += f'<rect x="{x + 150}" y="112" width="8" height="24" rx="2" fill="#cfd8d4"/>'
        s += text(x + 75, 130, '18650 · 3,7 V', 13, anchor='middle', weight=600)
        s += text(x + 14, 90, '−', 15, weight=700) + text(x + 136, 90, '+', 15, weight=700, fill=WIRE['5v'])
    s += f'<path d="M60 172 v10 H384 v-10" fill="none" stroke="#f4f6f5" stroke-width="1.6"/>'
    s += text(214, 204, '3,7 + 3,7 = 7,4 V (penuh 8,4 V)', 14, anchor='middle', weight=600, fill='#ffd8a8')
    s += text(214, 228, 'arusnya sama di kedua sel', 12.5, anchor='middle', fill='#e8efe9')
    s += f'<line x1="222" y1="112" x2="222" y2="60" stroke="{WIRE["oranye"]}" stroke-width="3"/>' + text(222, 54, 'titik tengah (kabel balance BMS)', 11.5, anchor='middle', fill='#ffd8a8')
    s += '<line x1="400" y1="20" x2="400" y2="330" stroke="#fff" stroke-opacity=".25" stroke-dasharray="4 5"/>'
    s += text(430, 34, 'PARALEL: arus dijumlah', 15, weight=700)
    s += line([(440, 90), (730, 90)], '5v', 5) + line([(440, 270), (730, 270)], 'gnd', 5)
    s += text(440, 80, 'WAGO 6V', 12, font=MONO, fill=WIRE['5v']) + text(440, 294, 'WAGO −6', 12, font=MONO)
    for x in (490, 620):
        s += line([(x + 18, 90), (x + 18, 146)], '5v', 4) + line([(x + 62, 214), (x + 62, 270)], 'gnd', 4)
        s += box(x, 146, 84, 68, 'servo', ['6 V'], fill='#1f4e79', stroke='#8ec5ff')
    s += text(430, 318, '0,5 A + 0,5 A = 1 A dari sumber,', 12.5, fill='#e8efe9') + text(430, 338, 'tiap servo tetap mendapat 6 V', 12.5, fill='#e8efe9')
    return svg(W, H, s, 'Dua sel baterai disusun seri dan dua servo disusun paralel')


def pembagi():
    W, H = 760, 360
    s = text(30, 30, 'Pembagi tegangan: 5 V dari echo HC-SR04 menjadi 3,3 V', 16, weight=600)
    x = 200
    s += line([(x, 76), (x, 84)], 'kuning', 4)
    s += box(120, 48, 160, 30, 'Echo · 5 V', '', fill='#3a2a13', stroke='#ffc27a')
    s += res_v(x, 84, 176, 'R1 = 1 kΩ')
    s += f'<circle cx="{x}" cy="196" r="6" fill="#fff" stroke="#111"/>'
    s += f'<line x1="{x}" y1="176" x2="{x}" y2="216" stroke="{KAWAT}" stroke-width="2.4"/>'
    s += line([(x, 196), (430, 196)], 'kuning', 4)
    s += box(430, 166, 170, 60, 'Pin ESP32', ['maks sekitar 3,6 V'], fill='#151a18')
    s += text(300, 186, '3,33 V', 14, anchor='middle', weight=700, fill='#ffd8a8', font=MONO)
    s += res_v(x, 216, 300, 'R2 = 2 kΩ')
    s += gnd_sym(x, 314) + text(x + 28, 322, 'GND', 12, font=MONO)
    s += text(430, 272, 'Vout = Vin × R2 / (R1 + R2)', 14, weight=600, font=MONO)
    s += text(430, 298, '= 5 × 2 / (1 + 2) = 3,33 V', 14, font=MONO, fill='#ffd8a8')
    s += text(430, 324, 'Tegangan terbagi sesuai perbandingan R1 dan R2.', 12, fill='#e8efe9')
    return svg(W, H, s, 'Pembagi tegangan 1 kilo ohm dan 2 kilo ohm menurunkan 5 volt menjadi 3,33 volt')


def pwm():
    W, H = 760, 390
    s = text(30, 34, 'PWM: menyala-mati sangat cepat, motor merasakan rata-ratanya', 16, weight=600)
    x0, x1, per = 190, 580, 97.5
    for i, (duty, v) in enumerate(((.25, '1,5 V'), (.5, '3,0 V'), (.75, '4,5 V'))):
        top = 70 + i * 100
        bot = top + 60
        d = f'M{x0} {bot}'
        x = x0
        while x < x1 - 1:
            on = per * duty
            d += f' V{top} H{x + on} V{bot} H{x + per}'
            x += per
        s += f'<path d="{d}" fill="none" stroke="#8ec5ff" stroke-width="2.6"/>'
        avg = bot - (bot - top) * duty
        s += f'<line x1="{x0}" y1="{avg}" x2="{x1}" y2="{avg}" stroke="{KUNING_ARUS}" stroke-width="2" stroke-dasharray="6 4"/>'
        s += text(30, top + 26, f'duty {int(duty * 100)}%', 13.5, weight=600)
        s += text(30, top + 46, f'{int(duty * 100)}% waktu menyala', 11.5, fill='#cfd8d4')
        s += text(x1 + 18, avg + 5, f'rata-rata ≈ {v}', 13, fill=KUNING_ARUS, font=MONO)
        s += text(x0 - 8, top + 5, '6 V', 10.5, anchor='end', fill='#cfd8d4', font=MONO) + text(x0 - 8, bot + 4, '0', 10.5, anchor='end', fill='#cfd8d4', font=MONO)
    s += text(30, 372, 'Contoh untuk rel motor 6 V. ESP32 mengatur duty, TB6612 menyalakan-mematikan arus motor sesuai duty itu.', 12.5, fill='#e8efe9')
    return svg(W, H, s, 'Tiga gelombang PWM dengan duty 25, 50 dan 75 persen beserta tegangan rata-ratanya')


def servo_pulsa():
    W, H = 760, 360
    s = text(30, 34, 'Servo: yang dibaca adalah lebar pulsa, diulang tiap 20 ms', 16, weight=600)
    x0 = 150
    k = 20  # piksel per milidetik
    for i, (ms, deg) in enumerate(((0.5, 0), (1.5, 90), (2.5, 180))):
        top = 70 + i * 88
        bot = top + 44
        w = ms * k
        s += f'<path d="M{x0 - 30} {bot} H{x0} V{top} H{x0 + w} V{bot} H{x0 + 20 * k} V{top} H{x0 + 20 * k + w} V{bot} H{x0 + 20 * k + 40}" fill="none" stroke="#8ec5ff" stroke-width="2.6"/>'
        s += text(30, top + 28, f'{ms:.1f} ms'.replace('.', ','), 14, weight=600, font=MONO)
        s += text(x0 + w + 8, top + 14, f'pulsa {ms:.1f} ms'.replace('.', ','), 11.5, fill=KUNING_ARUS)
        # lengan servo
        cx, cy = 680, bot - 4
        s += f'<path d="M{cx - 34} {cy} A34 34 0 0 1 {cx + 34} {cy}" fill="none" stroke="#fff" stroke-opacity=".3" stroke-dasharray="3 4"/>'
        a = math.radians(180 - deg)
        ex, ey = cx + 32 * math.cos(a), cy - 32 * math.sin(a)
        s += f'<circle cx="{cx}" cy="{cy}" r="8" fill="#1f4e79" stroke="#8ec5ff" stroke-width="1.5"/>'
        s += f'<line x1="{cx}" y1="{cy}" x2="{ex:.1f}" y2="{ey:.1f}" stroke="#f4f6f5" stroke-width="6" stroke-linecap="round"/>'
        s += text(cx + 42, cy - 8, f'{deg}°', 14, weight=700, font=MONO)
    s += f'<path d="M{x0} 336 h{20 * k}" stroke="#f4f6f5" stroke-width="1.4"/>' + text(x0 + 10 * k, 330, '20 ms = 50 kali per detik', 11.5, anchor='middle', fill='#cfd8d4')
    s += text(30, 354, 'Angka 0,5-2,5 ms itu perkiraan; tiap servo sedikit berbeda. Kode panduan memakai 0,6-2,4 ms.', 12, fill='#e8efe9')
    return svg(W, H, s, 'Lebar pulsa servo 0,5, 1,5 dan 2,5 milidetik dan sudut lengan 0, 90 dan 180 derajat')


def multimeter():
    W, H = 760, 460
    s = ''
    # badan multimeter
    s += '<rect x="34" y="40" width="210" height="350" rx="22" fill="#f2c230" stroke="#111" stroke-width="2"/>'
    s += '<rect x="56" y="62" width="166" height="62" rx="6" fill="#1d2b22"/>' + text(139, 106, '7,80', 32, anchor='middle', fill='#b6f5c1', font=MONO, weight=600)
    s += '<circle cx="139" cy="216" r="50" fill="#2a2a2a" stroke="#111" stroke-width="2"/>'
    s += '<line x1="139" y1="216" x2="104" y2="181" stroke="#f4f6f5" stroke-width="7" stroke-linecap="round"/>'
    for t, (x, y) in (('V⎓', (92, 160)), ('V~', (186, 160)), ('Ω', (204, 222)), ('•)))', (178, 282)), ('A', (82, 282)), ('10A', (64, 222))):
        s += text(x, y, t, 13, anchor='middle', fill='#1b1b1b', weight=700)
    for x, t in ((78, '10A'), (139, 'COM'), (200, 'VΩmA')):
        s += f'<circle cx="{x}" cy="340" r="13" fill="#111" stroke="#555" stroke-width="2"/>' + text(x, 318, t, 11.5, anchor='middle', fill='#1b1b1b', weight=700, font=MONO)
    s += f'<path d="M200 353 V420" fill="none" stroke="{WIRE["5v"]}" stroke-width="4"/>'
    s += '<path d="M139 353 V420" fill="none" stroke="#111" stroke-width="4"/>'
    s += text(34, 438, 'probe hitam: COM (selalu)', 11.5, weight=600) + text(34, 454, 'probe merah: VΩmA (hampir semua pengukuran)', 11.5, fill='#ffb4ab', weight=600)
    # skenario tegangan
    s += text(300, 34, 'Tegangan: rangkaian menyala, probe di DUA titik', 13.5, weight=700, fill=HIJAU_OK)
    s += line([(340, 70), (700, 70), (700, 92)], '5v', 3) + line([(700, 148), (700, 170), (340, 170)], 'gnd', 3)
    b, _ = baterai_v(340, 104)
    s += line([(340, 70), (340, 104)], '5v', 3) + line([(340, 137), (340, 170)], 'gnd', 3) + b
    s += res_v(700, 92, 148)
    s += meter(520, 120, 'V')
    s += f'<path d="M498 120 C 450 120 380 90 344 72" fill="none" stroke="{WIRE["5v"]}" stroke-width="2" stroke-dasharray="5 3"/>'
    s += '<path d="M520 142 C 520 160 400 168 344 168" fill="none" stroke="#111" stroke-width="2" stroke-dasharray="5 3"/>'
    # skenario arus
    s += text(300, 214, 'Arus: rangkaian DIBUKA, multimeter disisipkan', 13.5, weight=700, fill=HIJAU_OK)
    s += line([(340, 250), (498, 250)], '5v', 3) + line([(542, 250), (700, 250), (700, 272)], '5v', 3) + line([(700, 328), (700, 350), (340, 350)], 'gnd', 3)
    b, _ = baterai_v(340, 284)
    s += line([(340, 250), (340, 284)], '5v', 3) + line([(340, 317), (340, 350)], 'gnd', 3) + b
    s += res_v(700, 272, 328) + meter(520, 250, 'A')
    s += text(520, 298, 'arus lewat multimeter', 11.5, anchor='middle', fill='#e8efe9')
    s += text(330, 396, 'Probe di 10A atau kenop di A, lalu ditempel ke dua kutub', 12, fill=MERAH_SALAH, weight=600)
    s += text(330, 414, 'baterai = korsleting lewat multimeter, karena mode arus', 12, fill=MERAH_SALAH)
    s += text(330, 432, 'hampir tanpa hambatan.', 12, fill=MERAH_SALAH)
    return svg(W, H, s, 'Multimeter dengan kenop dan colokan, cara mengukur tegangan secara paralel dan arus secara seri')


def kapasitor():
    W, H = 760, 280
    s = text(30, 34, 'Dua jenis kapasitor di robotmu', 16, weight=600)
    # elektrolit
    s += '<rect x="90" y="64" width="80" height="120" rx="10" fill="#1d2f4a" stroke="#0b0d0c" stroke-width="1.5"/>'
    s += '<rect x="90" y="64" width="20" height="120" rx="8" fill="#c9d1d6"/>'
    for y in (90, 120, 150):
        s += text(100, y + 5, '−', 15, anchor='middle', fill='#1b1b1b', weight=700)
    s += text(140, 118, '470', 13, anchor='middle', weight=700, font=MONO) + text(140, 136, 'µF', 12, anchor='middle', font=MONO) + text(140, 160, '50V', 11, anchor='middle', font=MONO, fill='#cfd8d4')
    s += f'<line x1="104" y1="184" x2="104" y2="226" stroke="{KAWAT}" stroke-width="3"/><line x1="156" y1="184" x2="156" y2="250" stroke="{KAWAT}" stroke-width="3"/>'
    s += text(100, 244, '−', 15, anchor='end', weight=700) + text(162, 248, '+ (kaki panjang)', 12, weight=600, fill='#ffd8a8')
    s += text(200, 84, 'Elektrolit', 15, weight=700)
    for i, t in enumerate(['berkutub: garis − dan kaki panjang +', 'nilai besar (µF): penyangga lonjakan', 'tegangan tertulis = batas maksimum', 'terbalik: bisa panas, bocor, meletus']):
        s += text(200, 108 + i * 22, t, 12.5, fill='#e8efe9')
    # keramik
    s += '<ellipse cx="470" cy="118" rx="40" ry="34" fill="#e08a2e" stroke="#111" stroke-width="1.5"/>' + text(470, 125, '104', 18, anchor='middle', fill='#1b1b1b', weight=700, font=MONO)
    s += f'<line x1="456" y1="150" x2="456" y2="236" stroke="{KAWAT}" stroke-width="3"/><line x1="484" y1="150" x2="484" y2="236" stroke="{KAWAT}" stroke-width="3"/>'
    s += text(530, 84, 'Keramik', 15, weight=700)
    for i, t in enumerate(['tidak berkutub', 'nilai kecil (nF): redam noise cepat', '104 = 10 × 10⁴ pF = 100 nF', 'di kaki motor TT']):
        s += text(530, 108 + i * 22, t, 12.5, fill='#e8efe9')
    return svg(W, H, s, 'Kapasitor elektrolit 470 mikrofarad yang berkutub dan kapasitor keramik 104 yang tidak berkutub')


def solder():
    W, H = 760, 270
    s = text(30, 34, 'Bentuk solderan', 16, weight=600)
    contoh = [('Baik', HIJAU_OK, 'mengkilap, seperti gunung kecil,', 'membungkus kaki dan pad'),
              ('Dingin', MERAH_SALAH, 'buram, berbentuk bola,', 'tidak menempel ke pad'),
              ('Kurang timah', '#ffd8a8', 'pad belum tertutup,', 'mudah retak saat ditarik')]
    for i, (nama, col, a, b) in enumerate(contoh):
        cx = 130 + i * 250
        s += f'<rect x="{cx - 70}" y="160" width="140" height="16" fill="#2b6b3f" stroke="#0b0d0c"/>'
        s += f'<rect x="{cx - 30}" y="152" width="60" height="10" rx="2" fill="#c88a3a"/>'
        s += f'<rect x="{cx - 4}" y="70" width="8" height="120" fill="{KAWAT}"/>'
        if i == 0:
            s += f'<path d="M{cx - 30} 153 Q{cx - 8} 146 {cx - 5} 104 L{cx + 5} 104 Q{cx + 8} 146 {cx + 30} 153 Z" fill="#dfe3e6" stroke="#111" stroke-width="1"/>'
        elif i == 1:
            s += f'<circle cx="{cx}" cy="126" r="20" fill="#a9aeb1" stroke="#111" stroke-width="1"/>'
            s += f'<line x1="{cx - 30}" y1="150" x2="{cx + 30}" y2="150" stroke="{BG_GAP}" stroke-width="3"/>'
        else:
            s += f'<path d="M{cx - 12} 153 Q{cx - 6} 140 {cx - 5} 128 L{cx + 5} 128 Q{cx + 6} 140 {cx + 12} 153 Z" fill="#dfe3e6" stroke="#111" stroke-width="1"/>'
        s += text(cx, 214, nama, 14, anchor='middle', weight=700, fill=col)
        s += text(cx, 236, a, 11.5, anchor='middle', fill='#e8efe9') + text(cx, 252, b, 11.5, anchor='middle', fill='#e8efe9')
    return svg(W, H, s, 'Perbandingan solderan yang baik, solderan dingin dan solderan yang kurang timah')


BG_GAP = '#2f5a4e'


# ---------------- sampul ----------------

def sampul():
    g = ''
    # multimeter
    g += '<g transform="translate(70 40)">'
    g += '<rect width="210" height="330" rx="24" fill="#f2c230" stroke="#0b0d0c" stroke-width="2"/>'
    g += '<rect x="22" y="22" width="166" height="64" rx="8" fill="#1d2b22"/>'
    g += f'<text x="105" y="68" font-family="{MONO}" font-size="34" font-weight="600" fill="#b6f5c1" text-anchor="middle">6,02</text>'
    g += '<circle cx="105" cy="176" r="52" fill="#2a2a2a" stroke="#0b0d0c" stroke-width="2"/><line x1="105" y1="176" x2="70" y2="141" stroke="#f4f6f5" stroke-width="8" stroke-linecap="round"/>'
    g += '<circle cx="50" cy="292" r="13" fill="#111"/><circle cx="105" cy="292" r="13" fill="#111"/><circle cx="160" cy="292" r="13" fill="#111"/>'
    g += '</g>'
    # kabel probe
    g += f'<path d="M230 332 C 300 420 360 300 420 250" fill="none" stroke="{WIRE["5v"]}" stroke-width="7" stroke-linecap="round"/>'
    g += '<path d="M175 332 C 230 470 470 380 560 250" fill="none" stroke="#111" stroke-width="7" stroke-linecap="round"/>'
    # rangkaian kecil: resistor, LED, kapasitor di papan
    g += '<rect x="390" y="150" width="340" height="170" rx="14" fill="#f4f4ef" stroke="#c9c9c1" stroke-width="2"/>'
    for r in range(6):
        for c in range(15):
            g += f'<rect x="{408 + c * 21}" y="{166 + r * 24}" width="7" height="7" rx="1.5" fill="#c7c7bd"/>'
    g += f'<line x1="420" y1="250" x2="540" y2="250" stroke="{KAWAT}" stroke-width="4"/><rect x="450" y="238" width="62" height="24" rx="11" fill="#7fb0dd" stroke="#3f6f9c" stroke-width="2"/>'
    for i, cl in enumerate(('#7a4a1d', '#111', '#111', '#7a4a1d')):
        g += f'<rect x="{462 + i * 10}" y="238" width="5" height="24" fill="{cl}"/>'
    g += f'<line x1="560" y1="250" x2="560" y2="200" stroke="{KAWAT}" stroke-width="4"/><line x1="584" y1="250" x2="584" y2="200" stroke="{KAWAT}" stroke-width="4"/>'
    g += '<path d="M552 200 v-24 a20 20 0 0 1 40 0 v24 z" fill="#ff5a4f" stroke="#a3221a" stroke-width="2"/>'
    g += '<circle cx="572" cy="178" r="34" fill="#ff5a4f" opacity=".25"/>'
    g += '<rect x="640" y="176" width="46" height="72" rx="8" fill="#1d2f4a" stroke="#0b0d0c" stroke-width="2"/><rect x="640" y="176" width="12" height="72" rx="5" fill="#c9d1d6"/>'
    g += f'<line x1="652" y1="248" x2="652" y2="270" stroke="{KAWAT}" stroke-width="4"/><line x1="676" y1="248" x2="676" y2="282" stroke="{KAWAT}" stroke-width="4"/>'
    # simbol ohm dan rumus
    g += f'<text x="560" y="110" font-family="{MONO}" font-size="40" font-weight="600" fill="#f4f6f5" text-anchor="middle" opacity=".92">V = I × R</text>'
    return cv.bingkai(g, 'Ilustrasi multimeter, resistor, LED dan kapasitor untuk panduan elektronika dasar', 'e', W=800, H=500)
