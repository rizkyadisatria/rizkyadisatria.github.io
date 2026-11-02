"""Terjemahan isi panduan (ID -> EN) lewat katalog per panduan.

Cara kerja
  1. `tandai`: tiap blok teks (p, li, label, td, th, h2-h4, summary, figcaption, dst.) diberi atribut
     data-t="<hash isi>". Kunci dihitung dari HTML dalam blok, jadi terjemahan ikut blok yang sama
     walau halamannya digenerate ulang, dan teks yang sama di banyak tempat cukup diterjemahkan sekali.
  2. Terjemahan ditulis di _alat/terjemahan/<panduan>-*.tsv, satu baris per blok: kunci<TAB>HTML Inggris.
  3. `bangun` memeriksa tiap terjemahan (tag HTML harus sama dengan aslinya: tautan, <code>, <b> tidak boleh hilang)
     lalu menulis <panduan>/en.json yang dimuat bahasa.js saat EN aktif. Blok tanpa terjemahan tetap Indonesia.

Pakai:
  python3 _alat/terjemah.py tandai            # beri data-t di semua halaman panduan (aman diulang)
  python3 _alat/terjemah.py belum robot-ai    # tulis _alat/belum/robot-ai.tsv (kunci<TAB>HTML Indonesia) yang belum diterjemahkan
  python3 _alat/terjemah.py bangun            # periksa semua .tsv dan tulis en.json
  python3 _alat/terjemah.py status            # persentase terjemahan per halaman
Folder _alat tidak ikut terbit.
"""
import glob, hashlib, json, os, re, subprocess, sys

CTRL = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
DIR_TSV = os.path.join(CTRL, '_alat', 'terjemahan')
DIR_BELUM = os.path.join(CTRL, '_alat', 'belum')
PANDUAN = {  # panduan -> berkas halaman
    'robot-ai': ['robot-ai/index.html'] + [f'robot-ai/{p}/index.html' for p in ('1-daya', '2-badan', '3-otak', '4-mata', '5-ai', '6-rakit')],
    'elektronika-dasar': ['elektronika-dasar/index.html'],
    'stasiun-cuaca': ['stasiun-cuaca/index.html'],
}

JS_HALAMAN = {'stasiun-cuaca': 'stasiun-cuaca/index.html'}  # tahap yang dibuat JavaScript, kunci "js:..." (lihat _alat/jsblok.cjs)

BLOK = r'(?:p|li|ul|ol|table|div|h[1-4]|details|figure|pre|section|article|tr|thead|tbody|svg|nav|header|footer|form|button|textarea|select|script|style|canvas)'
DAUN = re.compile(r'<(p|li|label|h[1-4]|summary|td|th|figcaption|dt|dd|caption|legend|b|span|strong)\b([^>]*)>(.*?)</\1>', re.S)
LABEL_INPUT = re.compile(r'(<label\b[^>]*>)([^<]*[A-Za-z\u00c0-\u00ff][^<]*?)(\s*<input\b)')
NEED = re.compile(r'(<div class="need">)(.*?)(</div>)', re.S)
SPAN = re.compile(r'<span\b([^>]*)>(.*?)</span>', re.S)
RULE_B = re.compile(r'(<div class="rule">)<b\b([^>]*)>(.*?)</b>', re.S)
ABAIKAN = re.compile(r'<(pre|script|style|svg)\b.*?</\1>', re.S)


def kunci(inner):
    return hashlib.sha1(re.sub(r'\s+', ' ', inner).strip().encode('utf-8')).hexdigest()[:12]


def layak(inner):
    """Blok daun yang memuat teks untuk dibaca (bukan angka/kode murni)."""
    if re.search(r'<' + BLOK + r'\b', inner) or '<input' in inner:
        return False
    teks = re.sub(r'<[^>]+>', ' ', inner)
    teks = re.sub(r'&[a-z#0-9]+;', ' ', teks)
    kata = re.findall(r'[A-Za-z\u00c0-\u00ff]{3,}', teks)
    return len(kata) >= 1


def _beri(attrs, inner):
    if 'data-t=' in attrs:
        return attrs, False
    return f'{attrs} data-t="{kunci(inner)}"', True


def tandai_teks(html):
    """-> (html baru, {kunci: HTML Indonesia})"""
    blok = {}
    # lindungi pre/script/style/svg agar isinya tak tersentuh
    simpan = []
    def lindungi(m):
        simpan.append(m.group(0)); return f'\x00{len(simpan) - 1}\x00'
    h = ABAIKAN.sub(lindungi, html)

    def label_input(m):
        buka, teks, tutup = m.groups()
        if not layak(teks): return m.group(0)
        blok[kunci(teks)] = teks.strip()
        return f'{buka}<span data-t="{kunci(teks)}">{teks}</span>{tutup}'
    h = LABEL_INPUT.sub(label_input, h)

    def daun(m):
        tag, attrs, inner = m.groups()
        if 'data-en' in attrs: return m.group(0)   # data-en = terjemahan tulis tangan di generator
        if not layak(inner):   # pembungkus (mis. <li> berisi <input> + <label>): tandai blok daun di dalamnya
            return f'<{tag}{attrs}>{DAUN.sub(daun, inner)}</{tag}>'
        attrs, _ = _beri(attrs, inner)
        blok[kunci(inner)] = inner.strip()
        return f'<{tag}{attrs}>{inner}</{tag}>'
    h = DAUN.sub(daun, h)

    def need(m):
        def sp(ms):
            attrs, inner = ms.groups()
            if not layak(inner) or 'data-en' in attrs: return ms.group(0)
            attrs, _ = _beri(attrs, inner); blok[kunci(inner)] = inner.strip()
            return f'<span{attrs}>{inner}</span>'
        return m.group(1) + SPAN.sub(sp, m.group(2)) + m.group(3)
    h = NEED.sub(need, h)

    def rule(m):
        pre, attrs, inner = m.groups()
        if not layak(inner) or 'data-en' in attrs: return m.group(0)
        attrs, _ = _beri(attrs, inner); blok[kunci(inner)] = inner.strip()
        return f'{pre}<b{attrs}>{inner}</b>'
    h = RULE_B.sub(rule, h)

    h = re.sub(r'\x00(\d+)\x00', lambda m: simpan[int(m.group(1))], h)
    return h, blok


def katalog_rel(berkas):
    """Jalur relatif dari halaman ke <panduan>/en.json."""
    bagian = berkas.split('/')
    return '../' * (len(bagian) - 2) + 'en.json'


def tandai_semua(slugs=None):
    for slug, berkas in PANDUAN.items():
        if slugs and slug not in slugs: continue
        for b in berkas:
            p = os.path.join(CTRL, b)
            if not os.path.exists(p): continue
            s = open(p, encoding='utf-8').read()
            baru, blok = tandai_teks(s)
            kat = katalog_rel(b)
            if 'data-katalog=' not in baru:
                baru = re.sub(r'<body\b', f'<body data-katalog="{kat}"', baru, count=1)
            if baru != s: open(p, 'w', encoding='utf-8').write(baru)
            print(f'{b}: {len(blok)} blok')


def blok_panduan(slug):
    """{kunci: HTML Indonesia} gabungan semua halaman panduan."""
    out = {}
    for b in PANDUAN[slug]:
        p = os.path.join(CTRL, b)
        if not os.path.exists(p): continue
        out.update(tandai_teks(open(p, encoding='utf-8').read())[1])
    if slug in JS_HALAMAN:
        r = subprocess.run(['node', os.path.join(CTRL, '_alat', 'jsblok.cjs'), os.path.join(CTRL, JS_HALAMAN[slug])],
                           capture_output=True, text=True, check=True)
        out.update(json.loads(r.stdout))
    return out


def baca_tsv(slug):
    en = {}
    for f in sorted(glob.glob(os.path.join(DIR_TSV, slug + '-*.tsv'))):
        for no, baris in enumerate(open(f, encoding='utf-8').read().split('\n'), 1):
            if not baris.strip() or baris.startswith('#'): continue
            if '\t' not in baris:
                raise SystemExit(f'{os.path.basename(f)}:{no}: tidak ada TAB')
            k, v = baris.split('\t', 1)
            en[k.strip()] = v.strip()
    return en


def kerangka(html):
    """Daftar tag (urut) untuk membandingkan struktur asli dan terjemahan."""
    return sorted(re.findall(r'<[^>]+>', html))


def periksa(slug, en, idb):
    galat = []
    for k, v in en.items():
        if v == '=': continue   # tanda: teksnya sama di kedua bahasa (nama, kode, istilah)
        if k not in idb:
            galat.append(f'{k}: kunci tidak ada di halaman manapun (teks aslinya berubah?)'); continue
        if kerangka(v) != kerangka(idb[k]):
            galat.append(f'{k}: tag tidak sama dengan aslinya\n   ID: {idb[k][:150]}\n   EN: {v[:150]}')
        if '\n' in v or '\t' in v:
            galat.append(f'{k}: terjemahan memuat baris baru/TAB')
    return galat


def bangun():
    for slug in PANDUAN:
        en = baca_tsv(slug)
        if not en: continue
        idb = blok_panduan(slug)
        galat = periksa(slug, en, idb)
        if galat:
            print('\n'.join(galat)); raise SystemExit(f'{slug}: {len(galat)} terjemahan bermasalah')
        out = os.path.join(CTRL, slug, 'en.json')
        with open(out, 'w', encoding='utf-8') as f:
            json.dump(dict(sorted((k, idb[k] if v == '=' else v) for k, v in en.items())), f, ensure_ascii=False, separators=(',', ':'))
        print(f'{slug}/en.json: {len(en)} blok dari {len(idb)} ({len(en) * 100 // max(1, len(idb))}%)')


def belum(slug):
    en = baca_tsv(slug)
    os.makedirs(DIR_BELUM, exist_ok=True)
    idb = blok_panduan(slug)
    sisa = {k: v for k, v in idb.items() if k not in en}
    with open(os.path.join(DIR_BELUM, slug + '.tsv'), 'w', encoding='utf-8') as f:
        for b in PANDUAN[slug]:
            p = os.path.join(CTRL, b)
            if not os.path.exists(p): continue
            f.write(f'# {b}\n')
            for k, v in tandai_teks(open(p, encoding='utf-8').read())[1].items():
                if k in sisa:
                    f.write(f'{k}\t{re.sub(chr(10), " ", v)}\n'); del sisa[k]
        js = {k: v for k, v in sisa.items() if k.startswith('js:')}
        if js:
            f.write('# tahap yang dibuat JavaScript\n')
            for k, v in js.items(): f.write(f'{k}\t{v}\n'); del sisa[k]
    print(f'{slug}: belum diterjemahkan {sum(1 for _ in open(os.path.join(DIR_BELUM, slug + ".tsv"), encoding="utf-8") if "\t" in _)} blok')


def status():
    for slug, berkas in PANDUAN.items():
        en = baca_tsv(slug)
        for b in berkas:
            p = os.path.join(CTRL, b)
            if not os.path.exists(p): continue
            blok = tandai_teks(open(p, encoding='utf-8').read())[1]
            if JS_HALAMAN.get(slug) == b:
                blok.update({k: v for k, v in blok_panduan(slug).items() if k.startswith('js:')})
            ada = sum(1 for k in blok if k in en)
            print(f'{b:34} {ada:4}/{len(blok):4}  {ada * 100 // max(1, len(blok))}%')


if __name__ == '__main__':
    perintah = sys.argv[1] if len(sys.argv) > 1 else 'status'
    if perintah == 'tandai': tandai_semua()
    elif perintah == 'bangun': bangun()
    elif perintah == 'belum': belum(sys.argv[2])
    elif perintah == 'status': status()
    else: raise SystemExit(__doc__)
