// Build produksi: Bun.build (Svelte 5 + TypeScript) → dist/, lalu salin public/ dan buat robots/sitemap.
import type { BunPlugin } from 'bun'
import { SveltePlugin } from 'bun-plugin-svelte'
import { readFileSync } from 'node:fs'
import { cp, mkdir, readFile, rm, writeFile } from 'node:fs/promises'
import { basename, dirname, extname, join } from 'node:path'

const ROOT = import.meta.dir
export const DIST = join(ROOT, 'dist')
const DEFAULT_SITE_URL = 'https://rizkyadisatria.github.io'

/** URL publik situs: SITE_URL > domain produksi Vercel > domain lama. Tanpa slash di akhir. */
export function resolveSiteUrl(env: Record<string, string | undefined> = process.env): string {
  const fromVercel = env.VERCEL_PROJECT_PRODUCTION_URL ? `https://${env.VERCEL_PROJECT_PRODUCTION_URL}` : undefined
  return (env.SITE_URL || fromVercel || DEFAULT_SITE_URL).replace(/\/+$/, '')
}

// Alat penulisan panduan (Python) tidak ikut dipublikasikan, hanya hasil HTML-nya.
const SKIP_DIRS = new Set(['_build', '_alat', '__pycache__', '.esphome'])
const SKIP_EXT = new Set(['.py', '.pyc'])
const SKIP_FILES = new Set(['.gitignore', 'secrets.yaml'])

function shouldCopy(path: string): boolean {
  const name = basename(path)
  if (SKIP_DIRS.has(name) || SKIP_FILES.has(name)) return false
  return !SKIP_EXT.has(extname(name))
}

async function lastModified(): Promise<string> {
  const proc = Bun.spawnSync(['git', 'log', '-1', '--format=%cs'], { cwd: ROOT })
  const out = proc.stdout.toString().trim()
  return proc.exitCode === 0 && /^\d{4}-\d{2}-\d{2}$/.test(out) ? out : new Date().toISOString().slice(0, 10)
}

// Font: hanya woff2 (didukung semua browser modern), disajikan sebagai FILE ber-hash di /_app/fonts/.
// Bun menyisipkan font kecil sebagai data: URI di dalam CSS (CSS membengkak, tidak bisa di-cache terpisah,
// dan diblokir CSP `font-src 'self'`), jadi URL font ditulis ulang di sini dan filenya disalin manual.
const FONT_HOST = 'https://fonts.invalid' // TLD .invalid dicadangkan: tidak mungkin bentrok dengan URL asli
const fontFiles = new Map<string, string>() // path sumber → nama file ber-hash

function fontUrl(sourcePath: string): string {
  let name = fontFiles.get(sourcePath)
  if (!name) {
    const hash = Bun.hash(readFileSync(sourcePath)).toString(36).slice(0, 8)
    name = `${basename(sourcePath, '.woff2')}-${hash}.woff2`
    fontFiles.set(sourcePath, name)
  }
  // Bun membiarkan URL https:// apa adanya (URL absolut berawalan "/" ditolak), jadi pakai host placeholder
  // yang dibuang lagi setelah build (lihat FONT_HOST di build()).
  return `${FONT_HOST}/_app/fonts/${name}`
}

const selfHostedFonts: BunPlugin = {
  name: 'self-hosted-fonts',
  setup(build) {
    build.onLoad({ filter: /(boxicons\.min|@fontsource[\\/]poppins[\\/]latin-\d+)\.css$/ }, async ({ path }) => {
      let css = await Bun.file(path).text()
      // buang format lama: eot/svg/ttf (boxicons) dan woff (poppins)
      css = css
        .replace(/src:url\(\.\.\/fonts\/boxicons\.eot\);src:[^}]*\}/, 'src:url(../fonts/boxicons.woff2) format("woff2")}')
        .replace(/,\s*url\([^)]*\.woff\)\s*format\(['"]woff['"]\)/g, '')
      css = css.replace(/url\(['"]?([^)'"]+\.woff2)['"]?\)/g, (_m, rel: string) => `url(${fontUrl(join(dirname(path), rel))})`)
      return { contents: css, loader: 'css' }
    })
  },
}

export async function build(): Promise<{ ms: number; siteUrl: string }> {
  const t0 = performance.now()
  const siteUrl = resolveSiteUrl()

  await rm(DIST, { recursive: true, force: true })

  const result = await Bun.build({
    entrypoints: [join(ROOT, 'src/index.html')],
    outdir: DIST,
    target: 'browser',
    minify: true,
    splitting: true,
    sourcemap: 'none',
    naming: {
      entry: '[dir]/[name].[ext]',
      chunk: '_app/[name]-[hash].[ext]',
      asset: '_app/[name]-[hash].[ext]',
    },
    plugins: [SveltePlugin(), selfHostedFonts],
  })

  if (!result.success) {
    for (const log of result.logs) console.error(log)
    throw new Error('Bun.build gagal')
  }

  // Buang host placeholder font dari CSS hasil build → URL root-relatif /_app/fonts/....
  for (const file of new Bun.Glob('_app/*.css').scanSync({ cwd: DIST })) {
    const path = join(DIST, file)
    await writeFile(path, (await readFile(path, 'utf8')).replaceAll(FONT_HOST, ''))
  }

  // Font ber-hash (lihat plugin di atas).
  await mkdir(join(DIST, '_app/fonts'), { recursive: true })
  for (const [source, name] of fontFiles) await cp(source, join(DIST, '_app/fonts', name))

  // Aset statis (gambar, lottie, PDF, panduan controller) disalin apa adanya.
  await cp(join(ROOT, 'public'), DIST, { recursive: true, filter: shouldCopy })

  // Isi placeholder URL situs di HTML hasil build.
  const indexPath = join(DIST, 'index.html')
  // Ikon merujuk ke public/ (bukan modul), jadi disisipkan di sini agar tidak dicoba di-bundle oleh Bun.
  const icons =
    '<link rel="icon" type="image/svg+xml" href="/assets/img/favicon.svg">\n  <link rel="apple-touch-icon" href="/assets/img/favicon.svg">'
  const html = (await readFile(indexPath, 'utf8')).replaceAll('%SITE_URL%', siteUrl).replace('<!--%ICONS%-->', icons)
  await writeFile(indexPath, html)

  await writeFile(
    join(DIST, 'robots.txt'),
    `User-agent: *\nAllow: /\nDisallow: /assets/pdf/\n\nSitemap: ${siteUrl}/sitemap.xml\n`,
  )
  await writeFile(
    join(DIST, 'sitemap.xml'),
    `<?xml version="1.0" encoding="UTF-8"?>\n<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">\n  <url>\n    <loc>${siteUrl}/</loc>\n    <lastmod>${await lastModified()}</lastmod>\n    <changefreq>monthly</changefreq>\n    <priority>1.0</priority>\n  </url>\n</urlset>\n`,
  )

  await mkdir(join(DIST, '_app'), { recursive: true })
  return { ms: Math.round(performance.now() - t0), siteUrl }
}

if (import.meta.main) {
  const { ms, siteUrl } = await build()
  const glob = new Bun.Glob('_app/*')
  let total = 0
  for await (const f of glob.scan({ cwd: DIST })) {
    const size = Bun.file(join(DIST, f)).size
    total += size
    console.log(`${f.padEnd(40)} ${(size / 1024).toFixed(1)} KB`)
  }
  console.log(`\nSITE_URL: ${siteUrl}`)
  console.log(`Bundle (_app): ${(total / 1024).toFixed(1)} KB, build ${ms} ms`)
}
