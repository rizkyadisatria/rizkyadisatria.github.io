# Potongan HTML untuk halaman Elektronika Dasar. Komponen umum (section, tabel, kotak) diambil dari pembangun panduan robot.
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', '..', 'robot-ai', '_build'))
import build as rb  # noqa: E402  (pembangun panduan robot; hanya fungsinya yang dipakai)

section, table, warn, ok, info, ul, fig, esc = rb.section, rb.table, rb.warn, rb.ok, rb.info, rb.ul, rb.fig, rb.esc
HEAD = rb.HEAD
ROBOT = '../robot-ai/'


def latihan(n, judul, steps):
    """Kotak centang latihan. Pola <li><input id> dibaca _alat/langkah.py untuk label status di halaman utama."""
    lis = ''.join(f'<li><input type="checkbox" id="ed-{n}-{i}"><label for="ed-{n}-{i}">{s}</label></li>' for i, s in enumerate(steps))
    return (f'<article class="stage" id="latihan{n}"><div class="stagehead"><div><span class="eyebrow">Latihan bab {n}</span>'
            f'<h3>{judul}</h3></div><span class="cap" data-count></span></div><ol class="steps">{lis}</ol></article>')


def tanya(pasangan):
    d = ''.join(f'<details><summary>{q}</summary><div><p>{a}</p></div></details>' for q, a in pasangan)
    return f'<div class="tanya"><h3>Cek pemahaman</h3><p class="cap">Jawab dulu di kepala, baru buka jawabannya.</p>{d}</div>'


def kejadian(isi):
    return info('Kejadian di robotmu', isi)


def rumus(r, ket=''):
    k = f'<span>{ket}</span>' if ket else ''
    return f'<div class="rumus"><code>{r}</code>{k}</div>'


def _in(nama, label, ph='', nilai=''):
    v = f' value="{esc(nilai)}"' if nilai else ''
    return f'<label>{label}<input type="text" inputmode="decimal" autocomplete="off" name="{nama}" placeholder="{esc(ph)}"{v}></label>'


def kalk_ohm():
    return ('<div class="kalk" data-kalk="ohm"><h3>Kalkulator hukum Ohm</h3>'
            '<p class="cap">Isi dua kotak, kosongkan satu, lalu tekan Hitung. Hambatan boleh ditulis 1k atau 2,2k.</p>'
            '<div class="baris">' + _in('v', 'Tegangan (V)', 'misal 4') + _in('i', 'Arus (mA)', 'misal 18') + _in('r', 'Hambatan (Ω)', 'misal 220') + '</div>'
            '<div class="aksi"><button type="button" data-hitung>Hitung</button><button type="button" class="sek" data-reset>Kosongkan</button></div>'
            '<output class="hasil" aria-live="polite"></output></div>')


def kalk_led():
    pilih = ''.join(f'<option value="{v}"{" selected" if v == "2.0" else ""}>{t}</option>' for v, t in (
        ('2.0', 'Merah (2,0 V)'), ('2.1', 'Kuning (2,1 V)'), ('2.2', 'Hijau biasa (2,2 V)'), ('3.0', 'Biru / hijau terang (3,0 V)'), ('3.2', 'Putih (3,2 V)')))
    return ('<div class="kalk" data-kalk="led"><h3>Kalkulator resistor LED</h3>'
            '<div class="baris">' + _in('vs', 'Tegangan sumber (V)', '', '5')
            + f'<label>Warna LED<select name="vf">{pilih}</select></label>' + _in('i', 'Arus LED (mA)', '', '10') + '</div>'
            '<output class="hasil" aria-live="polite"></output></div>')


def kalk_pembagi():
    return ('<div class="kalk" data-kalk="pembagi"><h3>Kalkulator pembagi tegangan</h3>'
            '<div class="baris">' + _in('vin', 'Tegangan masuk (V)', '', '5') + _in('r1', 'R1, ke sumber (Ω)', '', '1k') + _in('r2', 'R2, ke GND (Ω)', '', '2k') + '</div>'
            '<output class="hasil" aria-live="polite"></output></div>')


def kalk_warna():
    gelang = ''.join(f'<rect class="gl" data-g="{i}" x="{58 + i * 20}" y="6" width="10" height="36" rx="1"/>' for i in range(5))
    prev = (f'<svg class="res" viewBox="0 0 240 48" aria-hidden="true"><line x1="0" y1="24" x2="240" y2="24" stroke="#9ea4a7" stroke-width="4"/>'
            f'<rect x="40" y="4" width="160" height="40" rx="16" fill="#cbb58c" stroke="#8b7a55" stroke-width="2"/>{gelang}</svg>')
    sel = ''.join(f'<label data-g="{i}">Gelang {i + 1}<select name="g{i}"></select></label>' for i in range(5))
    return ('<div class="kalk" data-kalk="warna"><h3>Pembaca gelang warna resistor</h3>'
            '<p class="cap">Pegang resistor dengan gelang toleransi di kanan (emas atau perak, atau di resistor 5 gelang biasanya coklat yang agak terpisah), lalu baca dari kiri.</p>'
            '<div class="baris sempit"><label>Jumlah gelang<select name="n"><option value="4">4 gelang</option><option value="5" selected>5 gelang</option></select></label></div>'
            + prev + f'<div class="baris">{sel}</div><output class="hasil" aria-live="polite"></output></div>')


GAYA = '''<style>
.rumus{display:grid;gap:4px;padding:14px 18px;border-radius:var(--radius);background:var(--surface-2);border:1px solid var(--line)}
.rumus code{font:600 1.25rem/1.3 var(--mono);color:var(--ink);background:none;border:0;padding:0}
.rumus span{font-size:.88rem;color:var(--muted)}
.tanya{display:grid;gap:10px;padding:16px 18px;border:1px solid var(--line);border-radius:var(--radius);background:var(--surface)}
.tanya details p{margin:0}
.kalk{display:grid;gap:12px;padding:16px 18px;border:1px solid var(--line);border-radius:var(--radius);background:var(--surface);box-shadow:var(--shadow)}
.kalk .baris{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:10px}
.kalk label{display:grid;gap:6px;font-size:.84rem;font-weight:500;color:var(--muted)}
.kalk label[hidden]{display:none}
.kalk .baris.sempit{grid-template-columns:minmax(150px,240px)}
.kalk input{appearance:none;-webkit-appearance:none;width:100%;height:42px;margin:0;display:block;padding:0 12px;border:1px solid var(--line-2);border-radius:10px;background:var(--surface-2);color:var(--ink);font:500 1rem var(--mono);cursor:text}
.kalk input::after{content:none;display:none}
.kalk select{width:100%;height:42px;padding:0 10px;border:1px solid var(--line-2);border-radius:10px;background:var(--surface-2);color:var(--ink);font:500 .95rem var(--body)}
.kalk input:focus-visible,.kalk select:focus-visible{outline:2px solid var(--accent);outline-offset:1px}
.kalk .aksi{display:flex;flex-wrap:wrap;gap:8px}
.kalk button{height:40px;padding:0 18px;border-radius:999px;border:1px solid var(--ink);background:var(--ink);color:var(--bg);font:600 .9rem var(--body);cursor:pointer}
.kalk button.sek{background:transparent;color:var(--ink);border-color:var(--line-2)}
.kalk .hasil{display:block;min-height:1.4em;padding:12px 14px;border-radius:10px;background:var(--ok-soft);color:var(--ink);font-size:.95rem;line-height:1.55}
.kalk .hasil:empty{display:none}
.kalk .hasil b{font-family:var(--mono)}
.kalk .hasil.salah{background:var(--accent-soft)}
.kalk svg.res{width:100%;max-width:320px;height:auto}
</style>
'''
