# Membangun halaman panduan Robot AI 4WD dari isi di p0..p6.
# Jalankan: python3 build.py   (folder _build tidak ikut terbit di GitHub Pages)
import html, os, re, shutil, sys, zipfile
sys.path.insert(0, os.path.dirname(__file__))
import diag
import cover

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..')) + '/'
KODE = ROOT + 'kode/'
TES_SRC = os.environ.get('TES_SRC', '')


def esc(s):
    return html.escape(s, quote=True)


from paten import PATEN  # noqa: E402

PARTS = [
    ('1-daya', 'Daya', 'Baterai, BMS, sekring, saklar, dan tiga konverter tegangan. Diukur dengan multimeter sebelum ada board yang disambung.'),
    ('2-badan', 'Badan', 'Chassis, 4 motor lewat TB6612, kepala pan-tilt, sensor jarak, kemiringan, cuaca, tombol STOP, dan kode badan.'),
    ('3-otak', 'Otak', 'ESP32-S3 dengan layar sentuh, SD card, speaker, mikrofon, lalu disambung ke badan lewat UART.'),
    ('4-mata', 'Mata', 'ESP32-CAM: upload lewat papan MB, lihat foto di browser, lalu pindah ke daya robot dan kepala pan-tilt.'),
    ('5-ai', 'Kecerdasan', 'Bot Telegram, AI Claude, suara ke teks, lalu kode otak lengkap: wajah, ngobrol, melihat, mode ikuti dan jaga.'),
    ('6-rakit', 'Rakit akhir', 'Tata letak di chassis tiga lantai, pindah ke daya baterai, uji di lantai, kalibrasi, dan daftar masalah umum.'),
]

# ---------------- komponen kecil ----------------

def pin(t): return f'<span class="pin">{esc(t)}</span>'


def photo(src, alt, cap='', w=None):
    st = f' style="max-width:{w}px"' if w else ''
    c = f'<figcaption class="cap">{cap}</figcaption>' if cap else ''
    return f'<figure class="photo"><img src="{src}" alt="{esc(alt)}" loading="lazy"{st}>{c}</figure>'


def fig(svg, cap=''):
    c = f'<p class="cap">{cap}</p>' if cap else ''
    return f'<div class="figure">{svg}</div><p class="cap geser">Geser gambar ke samping untuk melihat semuanya.</p>{c}'


def table(head, rows, cap=''):
    h = ''.join(f'<th>{x}</th>' for x in head)
    b = ''.join('<tr>' + ''.join(f'<td>{c}</td>' for c in r) + '</tr>' for r in rows)
    c = f'<p class="cap">{cap}</p>' if cap else ''
    return f'<div class="tbl"><table><thead><tr>{h}</tr></thead><tbody>{b}</tbody></table></div>{c}'


def kode_tes(nama):
    """blok kode dari kode/tes/<nama>.ino"""
    path = KODE + 'tes/' + nama + '.ino'
    src = open(path, encoding='utf-8').read()
    return (f'<div class="codehead"><span>{nama}.ino</span><a href="../kode/tes/{nama}.ino" download>Unduh</a></div>'
            f'<div class="code"><button type="button" class="copy">Salin</button><pre><code>{esc(src)}</code></pre></div>')


def kode_teks(src, judul=''):
    h = f'<div class="codehead"><span>{esc(judul)}</span></div>' if judul else ''
    return f'{h}<div class="code"><button type="button" class="copy">Salin</button><pre><code>{esc(src)}</code></pre></div>'


def warn(judul, isi): return f'<div class="warn"><b>{judul}</b><p>{isi}</p></div>'
def ok(judul, isi): return f'<div class="ok"><b>{judul}</b><p>{isi}</p></div>'
def info(judul, isi): return f'<div class="info"><b>{judul}</b><p>{isi}</p></div>'


def ul(items): return '<ul class="plain">' + ''.join(f'<li>{x}</li>' for x in items) + '</ul>'


def paten_box(boleh, jangan):
    return ('<div class="paten"><b>Setelah lulus: boleh dipatenkan atau belum?</b>'
            f'<p><span class="py">Boleh dipatenkan</span> {boleh}</p><p><span class="pn">Jangan dulu</span> {jangan}</p></div>')


def stage(slug, n, judul, need, steps, figs='', pre='', code='', after='', ok_=None, learn=None, gagal=None):
    lis = ''.join(f'<li><input type="checkbox" id="{slug}-{n}-{i}"><label for="{slug}-{n}-{i}">{s}</label></li>' for i, s in enumerate(steps))
    needs = ''.join(f'<span>{esc(x)}</span>' for x in need)
    out = f'<article class="stage" id="tahap{n}"><div class="stagehead"><div><span class="eyebrow">Tahap {n}</span><h3>{judul}</h3></div><span class="cap" data-count></span></div>'
    out += f'<div class="need">{needs}</div>' if need else ''
    out += pre + figs + f'<ol class="steps">{lis}</ol>' + code + after
    if learn: out += f'<p class="cap"><b>Kenapa begitu?</b> {learn}</p>'
    if ok_: out += ok('Berhasil kalau', ok_)
    if (slug, n) in PATEN: out += paten_box(*PATEN[(slug, n)])
    if gagal: out += '<details><summary>Kalau tidak berhasil</summary><div>' + ul(gagal) + '</div></details>'
    return out + '</article>'


def section(id_, eyebrow, judul, isi, lead=''):
    l = f'<p class="lead">{lead}</p>' if lead else ''
    return f'<section id="{id_}" aria-labelledby="h-{id_}"><span class="eyebrow">{eyebrow}</span><h2 id="h-{id_}">{judul}</h2>{l}{isi}</section>'


HEAD = '''<!doctype html>
<html lang="id">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>{title}</title>
<meta name="description" content="{desc}">
<meta name="theme-color" content="#fafaf9">
<link rel="icon" type="image/svg+xml" href="{a}favicon.svg">
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Inter:wght@400..700&family=JetBrains+Mono:wght@400;500&display=swap">
<link rel="stylesheet" href="{a}tema.css">
<script src="{a}tema.js"></script>
<link rel="stylesheet" href="{a}panduan.css">
</head>
'''


def toc(aktif, rel):
    t = f'<a href="{rel}"{" aria-current=\"page\"" if aktif == "" else ""}>Mulai</a>'
    for i, (slug, nama, _) in enumerate(PARTS, 1):
        cur = ' aria-current="page"' if slug == aktif else ''
        t += f'<a href="{rel}{slug}/"{cur}>{i}. {nama}</a>'
    return f'<nav class="toc" aria-label="Bagian panduan robot">{t}</nav>'


def halaman_bagian(slug, judul_h1, lead, isi, desc):
    idx = [p[0] for p in PARTS].index(slug)
    nomor = idx + 1
    prev = ('../', 'Mulai: rencana dan barang') if idx == 0 else (f'../{PARTS[idx - 1][0]}/', f'Bagian {idx}: {PARTS[idx - 1][1]}')
    nxt = (f'../{PARTS[idx + 1][0]}/', f'Bagian {idx + 2}: {PARTS[idx + 1][1]}') if idx + 1 < len(PARTS) else ('../', 'Kembali ke halaman utama robot')
    out = HEAD.format(title=f'Bagian {nomor}: {PARTS[idx][1]} · Robot AI 4WD', desc=esc(desc), a='../../assets/')
    out += f'<body data-key="robot-ai/{slug}"><div class="wrap">'
    out += (f'<header class="top"><a class="back" href="../">← Robot AI 4WD</a>{cover.SAMPUL[slug]()}<span class="eyebrow">Robot AI 4WD · Bagian {nomor} dari {len(PARTS)}</span>'
            f'<h1>{judul_h1}</h1><p class="lead">{lead}</p>'
            f'<div class="progress"><span id="prog">0 / 0 langkah</span><div class="bar"><i id="progbar"></i></div></div>{toc(slug, "../")}</header>')
    out += isi
    out += (f'<div class="pager"><a href="{prev[0]}"><span>← Sebelumnya</span>{prev[1]}</a>'
            f'<a class="next" href="{nxt[0]}"><span>Berikutnya →</span>{nxt[1]}</a></div>')
    out += ('<footer><span>Kode sudah dikompilasi untuk ESP32 core 3.3.12, tapi robot lengkapnya belum diuji di perangkat nyata. '
            'Cabut daya sebelum mengubah kabel.</span><a href="../../">Semua panduan</a></footer>')
    out += '</div><script src="../../assets/panduan.js"></script></body></html>\n'
    os.makedirs(ROOT + slug, exist_ok=True)
    open(ROOT + slug + '/index.html', 'w', encoding='utf-8').write(out)


def halaman_utama(isi):
    out = HEAD.format(title='Robot AI 4WD · Panduan Elektronika',
                      desc='Panduan lengkap merakit robot AI 4WD dengan ESP32-S3, ESP32 DevKit dan ESP32-CAM: daya baterai, motor, sensor, layar sentuh, suara, Telegram dan AI Claude. Bertahap untuk pemula.',
                      a='../assets/')
    out += '<body data-key="robot-ai/utama"><div class="wrap">' + isi
    out += ('<footer><span>Dibuat untuk belajar. Kode sudah dikompilasi, robot lengkapnya belum diuji di perangkat nyata.</span>'
            '<a href="../">Semua panduan</a></footer></div><script src="../assets/panduan.js"></script></body></html>\n')
    open(ROOT + 'index.html', 'w', encoding='utf-8').write(out)


def salin_kode():
    os.makedirs(KODE + 'tes', exist_ok=True)
    if TES_SRC:
        for d in sorted(os.listdir(TES_SRC)):
            f = os.path.join(TES_SRC, d, d + '.ino')
            if os.path.exists(f): shutil.copy(f, KODE + 'tes/' + d + '.ino')
    with zipfile.ZipFile(KODE + 'robot-ai-kode.zip', 'w', zipfile.ZIP_DEFLATED) as z:
        for d in ('robot_badan', 'robot_otak', 'robot_mata'):
            z.write(KODE + d + '/' + d + '.ino', d + '/' + d + '.ino')
        for f in sorted(os.listdir(KODE + 'tes')):
            n = f[:-4]
            z.write(KODE + 'tes/' + f, 'tes/' + n + '/' + f)


if __name__ == '__main__':
    salin_kode()
    cover.tulis_kartu(ROOT + '../')
    import p0, p1, p2, p3, p4, p5, p6
    halaman_utama(p0.isi())
    for m in (p1, p2, p3, p4, p5, p6):
        halaman_bagian(*m.halaman())
    # daftar langkah untuk label progres otomatis di halaman utama (controller/index.html)
    sys.path.insert(0, ROOT + '../_alat')
    import langkah
    n = langkah.tulis_langkah('robot-ai', [ROOT + 'index.html'] + [ROOT + p[0] + '/index.html' for p in PARTS])
    print('selesai,', n, 'langkah')
