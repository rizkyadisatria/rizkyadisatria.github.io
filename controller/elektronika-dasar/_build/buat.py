# Membangun halaman Elektronika Dasar (controller/elektronika-dasar/index.html) dan sampulnya.
# Jalankan: python3 buat.py   (folder _build tidak dipakai oleh halaman yang terbit)
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gambar as g  # noqa: E402
import isi  # noqa: E402
from buat_alat import HEAD, GAYA, esc  # noqa: E402

ROOT = os.path.abspath(os.path.join(HERE, '..')) + '/'
CTRL = os.path.abspath(os.path.join(HERE, '..', '..')) + '/'
JUDUL = 'Elektronika Dasar untuk IoT dan Mikrokontroler'
DESK = ('Elektronika yang dipakai di proyek ESP32, Arduino, dan IoT: tegangan dan arus, GND bersama, regulator, umur baterai, resistor, kapasitor, '
        'LED, transistor dan relay, pin mikrokontroler, PWM dan servo, I2C/SPI/UART, multimeter, breadboard, dan cara mencari kesalahan. Dengan kalkulator dan latihan.')


def halaman():
    toc = '<a href="#mulai">Mulai</a>' + ''.join(f'<a href="#{i}">{n + 1}. {esc(j)}</a>' for n, (i, j) in enumerate(isi.BAB))
    out = HEAD.format(title=f'{JUDUL} · Panduan Elektronika', desc=esc(DESK), a='../assets/').replace('</head>', GAYA + '</head>')
    out += '<body data-key="elektronika-dasar"><div class="wrap">'
    out += (f'<header class="top"><a class="back" href="../">← Semua panduan</a>{g.sampul()}'
            '<span class="eyebrow">Panduan · Pemula · 16 bab</span>'
            f'<h1>{JUDUL}</h1>'
            '<p class="lead">Dasar yang berlaku untuk proyek mikrokontroler apa pun, dari stasiun cuaca sampai robot: kenapa modul membaca angka acak tanpa GND bersama, '
            'berapa lama baterai bertahan, cara menyalakan relay dari pin 3,3 V, dan cara memeriksa rangkaian dengan multimeter.</p>'
            '<div class="progress"><span id="prog">0 / 0 langkah</span><div class="bar"><i id="progbar"></i></div></div>'
            f'<nav class="toc" aria-label="Daftar bab">{toc}</nav></header>')
    out += isi.pendahuluan()
    for nama, _ in isi.BAB:
        out += getattr(isi, nama)()
    out += isi.istilah() + isi.sumber()
    out += ('<div class="pager"><a href="../"><span>← Kembali</span>Semua panduan</a>'
            '<a class="next" href="../stasiun-cuaca/"><span>Coba proyek pertama →</span>Stasiun cuaca kamar</a></div>')
    out += ('<footer><span>Dibuat untuk belajar merakit dengan tegangan rendah. Angka komponen adalah nilai umum; lembar data komponenmu lebih tepat.</span>'
            '<a href="../">Semua panduan</a></footer>')
    out += '</div><script src="../assets/panduan.js"></script><script src="kalkulator.js"></script></body></html>\n'
    open(ROOT + 'index.html', 'w', encoding='utf-8').write(out)


if __name__ == '__main__':
    halaman()
    os.makedirs(ROOT + 'img', exist_ok=True)
    open(ROOT + 'img/sampul.svg', 'w', encoding='utf-8').write(g.sampul().replace('class="cover" ', ''))
    sys.path.insert(0, CTRL + '_alat')
    import langkah
    print('selesai,', langkah.tulis_langkah('elektronika-dasar', [ROOT + 'index.html']), 'langkah')
