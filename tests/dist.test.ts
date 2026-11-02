// Tes hasil build (jalankan via `bun run test`, yang membangun dist/ lebih dulu).
import { describe, expect, test } from 'bun:test'
import { existsSync, readFileSync, readdirSync, statSync } from 'node:fs'
import { join } from 'node:path'
import { resolveSiteUrl } from '../build.ts'
import { i18n } from '../src/lib/i18n.data.ts'

const ROOT = join(import.meta.dir, '..')
const DIST = join(ROOT, 'dist')
const read = (path: string) => readFileSync(join(DIST, path), 'utf8')
const vercel = JSON.parse(readFileSync(join(ROOT, 'vercel.json'), 'utf8'))

function walk(dir: string): string[] {
  return readdirSync(dir).flatMap((name) => {
    const full = join(dir, name)
    return statSync(full).isDirectory() ? [full, ...walk(full)] : [full]
  })
}

describe('i18n', () => {
  test('kunci bahasa en dan id sama persis', () => {
    expect(Object.keys(i18n.id).sort()).toEqual(Object.keys(i18n.en).sort())
  })
})

describe('resolveSiteUrl', () => {
  test('SITE_URL menang atas domain Vercel dan default', () => {
    expect(resolveSiteUrl({ SITE_URL: 'https://contoh.dev/', VERCEL_PROJECT_PRODUCTION_URL: 'x.vercel.app' })).toBe('https://contoh.dev')
  })
  test('memakai domain produksi Vercel bila SITE_URL kosong', () => {
    expect(resolveSiteUrl({ VERCEL_PROJECT_PRODUCTION_URL: 'rizky.vercel.app' })).toBe('https://rizky.vercel.app')
  })
  test('default ke domain GitHub Pages lama', () => {
    expect(resolveSiteUrl({})).toBe('https://rizkyadisatria.github.io')
  })
})

describe('dist/', () => {
  test('index.html: placeholder terisi, tidak ada script/stylesheet eksternal', () => {
    const html = read('index.html')
    expect(html).not.toContain('%SITE_URL%')
    expect(html).not.toContain('%ICONS%')
    expect(html).toContain('rel="canonical"')
    const external = [...html.matchAll(/<(?:script|link)[^>]+(?:src|href)="(https?:\/\/[^"]+)"/g)]
      .map((m) => m[1]!)
      .filter((url) => !url.includes('rel="canonical"'))
    // satu-satunya tautan https di <link> adalah canonical milik situs sendiri
    const canonical = html.match(/<link rel="canonical" href="([^"]+)"/)?.[1]
    expect(external.filter((u) => u !== canonical)).toEqual([])
  })

  test('bundle ada, tanpa source map', () => {
    const files = readdirSync(join(DIST, '_app'))
    expect(files.some((f) => f.endsWith('.js'))).toBe(true)
    expect(files.some((f) => f.endsWith('.css'))).toBe(true)
    expect(walk(DIST).some((f) => f.endsWith('.map'))).toBe(false)
  })

  test('font disajikan sebagai file (bukan data: URI) dan semua file-nya ada', () => {
    const cssFiles = readdirSync(join(DIST, '_app')).filter((f) => f.endsWith('.css'))
    for (const file of cssFiles) {
      const css = read(`_app/${file}`)
      expect(css).not.toContain('data:font')
      expect(css).not.toContain('fonts.invalid')
      for (const [, url] of css.matchAll(/url\((\/_app\/fonts\/[^)]+)\)/g)) {
        expect(existsSync(join(DIST, url!))).toBe(true)
      }
    }
  })

  test('robots.txt dan sitemap.xml memuat URL situs', () => {
    const site = resolveSiteUrl()
    expect(read('robots.txt')).toContain(`Sitemap: ${site}/sitemap.xml`)
    expect(read('sitemap.xml')).toContain(`<loc>${site}/</loc>`)
  })

  test('panduan /controller ikut terbit, alat Python dan berkas rahasia tidak', () => {
    expect(existsSync(join(DIST, 'controller/index.html'))).toBe(true)
    expect(existsSync(join(DIST, 'controller/robot-ai/index.html'))).toBe(true)
    const leaked = walk(DIST).filter((f) => /(\/_build|\/_alat|\/__pycache__|\.py$|\.pyc$|\.gitignore$|secrets\.yaml$)/.test(f))
    expect(leaked).toEqual([])
  })
})

describe('vercel.json', () => {
  const headersFor = (source: string): Record<string, string> =>
    Object.fromEntries(
      vercel.headers
        .filter((r: { source: string }) => r.source === source)
        .flatMap((r: { headers: { key: string; value: string }[] }) => r.headers.map((h) => [h.key, h.value])),
    )

  test('header keamanan dasar untuk semua path', () => {
    const h = headersFor('/(.*)')
    expect(h['X-Content-Type-Options']).toBe('nosniff')
    expect(h['X-Frame-Options']).toBe('DENY')
    expect(h['Referrer-Policy']).toBeTruthy()
    expect(h['Permissions-Policy']).toBeTruthy()
  })

  test('CSP halaman utama ketat: script hanya dari self, tanpa unsafe-eval, tanpa embed', () => {
    const csp = headersFor('/')['Content-Security-Policy']!
    const directive = (name: string) => csp.split(';').map((d) => d.trim()).find((d) => d.startsWith(name + ' ')) ?? ''
    expect(directive('script-src')).toBe("script-src 'self'")
    expect(csp).not.toContain('unsafe-eval')
    expect(directive('font-src')).toBe("font-src 'self'")
    expect(directive('frame-ancestors')).toBe("frame-ancestors 'none'")
    expect(directive('object-src')).toBe("object-src 'none'")
    expect(directive('connect-src')).toContain('https://formspree.io')
  })

  test('aset ber-hash di-cache immutable', () => {
    expect(headersFor('/_app/(.*)')['Cache-Control']).toContain('immutable')
  })

  test('trailingSlash aktif agar tautan relatif panduan /controller tetap benar', () => {
    expect(vercel.trailingSlash).toBe(true)
  })
})

describe('sumber', () => {
  test('tidak ada referensi CDN pihak ketiga di src/', () => {
    const files = walk(join(ROOT, 'src')).filter((f) => /\.(html|css|ts|svelte)$/.test(f) && !f.endsWith('i18n.data.ts'))
    for (const file of files) {
      const text = readFileSync(file, 'utf8')
      expect(text, file).not.toMatch(/cdn\.jsdelivr|cdnjs\.cloudflare|fonts\.googleapis|unpkg\.com/)
    }
  })
})

describe('pemindahan progres dari alamat lama', () => {
  test('halaman /controller/pindah/ terbit dan memakai skrip eksternal', () => {
    expect(read('controller/pindah/index.html')).toContain('src="./pindah.js"')
    expect(existsSync(join(DIST, 'controller/pindah/pindah.js'))).toBe(true)
  })
})

describe('daftar barang robot (/controller/robot-ai/)', () => {
  const html = () => read('controller/robot-ai/index.html')
  const TIDAK_DIPAKAI = ['ov2640', 'lcd1602', 'relay', 'sg90', 'ldr', 'pot', 'dioda']

  test('gambar barang yang tidak dipakai robot tidak ditampilkan', () => {
    for (const g of TIDAK_DIPAKAI) expect(html(), g).not.toContain(`img/${g}.jpg`)
  })
  test('status "sudah punya" berupa kotak centang dengan id unik, bukan teks yang ditulis manual', () => {
    const ids = [...html().matchAll(/class="have" id="(have-[^"]+)"/g)].map((m) => m[1]!)
    expect(ids.length).toBeGreaterThan(40)
    expect(new Set(ids).size).toBe(ids.length)
    expect(html()).not.toContain('dicoret dari daftar')
    expect(html()).toContain('id="beli-list"')
  })
  test('kotak centang barang tidak ikut terhitung sebagai langkah progres', () => {
    expect(html()).not.toMatch(/<ol class="steps">(?:(?!<\/ol>).)*class="have"/s)
    const langkah = JSON.parse(read('controller/robot-ai/langkah.json'))
    expect(langkah.total).toBe(240)
  })
})

describe('bahasa dan cadangan di panduan', () => {
  test('tema.js memuat bahasa.js dan cadangan.js, yang ikut terbit', () => {
    expect(read('controller/assets/tema.js')).toContain("muat('bahasa.js'")
    expect(existsSync(join(DIST, 'controller/assets/bahasa.js'))).toBe(true)
    expect(existsSync(join(DIST, 'controller/assets/cadangan.js'))).toBe(true)
  })
  test('halaman depan panduan punya terjemahan Inggris untuk tiap panduan', () => {
    const guides = read('controller/guides.js')
    const jumlahSlug = [...guides.matchAll(/^\s*slug:/gm)].length
    expect([...guides.matchAll(/^\s*en: \{/gm)].length).toBe(jumlahSlug)
  })
})

describe('terjemahan isi panduan (/controller)', () => {
  const PANDUAN: Record<string, string[]> = {
    'robot-ai': ['', '1-daya/', '2-badan/', '3-otak/', '4-mata/', '5-ai/', '6-rakit/'],
    'elektronika-dasar': [''],
    'stasiun-cuaca': [''],
  }
  for (const [slug, bagian] of Object.entries(PANDUAN)) {
    test(`${slug}: en.json terbit dan setiap blok bertanda punya terjemahan`, () => {
      const en = JSON.parse(read(`controller/${slug}/en.json`)) as Record<string, string>
      expect(Object.keys(en).length).toBeGreaterThan(100)
      const kunci = new Set<string>()
      for (const b of bagian) {
        const html = read(`controller/${slug}/${b}index.html`)
        expect(html).toContain('data-katalog="')
        for (const m of html.matchAll(/data-t="([0-9a-f]{12})"/g)) kunci.add(m[1]!)
      }
      const hilang = [...kunci].filter((k) => !(k in en))
      expect(hilang).toEqual([])
    })
  }
})
