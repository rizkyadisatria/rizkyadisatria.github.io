// Bangun panel web Zond (Svelte 5 + TypeScript) menjadi SATU halaman HTML, lalu tanam ke robot_otak.ino
// sebagai string PANEL_HTML. Pembaca panduan tetap cukup menyalin satu file .ino ke Arduino IDE.
// Jalankan dari akar repo: bun run public/controller/robot-ai/_build/panel/bangun.ts
import { SveltePlugin } from 'bun-plugin-svelte'
import { join } from 'node:path'

const DIR = import.meta.dir
const INO = join(DIR, '../../kode/robot_otak/robot_otak.ino')
const AWAL = '// <PANEL_WEB> dibuat otomatis oleh _build/panel/bangun.ts, jangan diedit di sini'
const AKHIR = '// </PANEL_WEB>'
const BATAS = 'PANEL' // pembatas raw string C++: R"PANEL( ... )PANEL"

const hasil = await Bun.build({
  entrypoints: [join(DIR, 'main.ts')],
  target: 'browser',
  minify: true,
  plugins: [
    // Bun memilih jalur "development" milik esm-env, jadi Svelte ikut membawa pemeriksaan mode pengembangan
    // (lebih besar dan lebih lambat). Paksa DEV = false.
    {
      name: 'svelte-produksi',
      setup(b) {
        b.onResolve({ filter: /^esm-env$/ }, () => ({ path: 'esm-env', namespace: 'svelte-produksi' }))
        b.onLoad({ filter: /.*/, namespace: 'svelte-produksi' }, () => ({
          contents: 'export const BROWSER = true, DEV = false, NODE = false;',
          loader: 'js',
        }))
      },
    },
    SveltePlugin({ development: false }),
  ],
})
if (!hasil.success) {
  for (const log of hasil.logs) console.error(log)
  process.exit(1)
}

let js = ''
let css = ''
for (const out of hasil.outputs) {
  if (out.path.endsWith('.js')) js += await out.text()
  else if (out.path.endsWith('.css')) css += await out.text()
}
// </script> di dalam JS akan menutup tag lebih awal; pecah supaya aman.
js = js.replaceAll('</script', '<\\/script')

const html =
  '<!doctype html><html lang="id"><head><meta charset="utf-8">' +
  '<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">' +
  '<title>Panel Zond</title><style>' + css + '</style></head><body>' +
  '<script type="module">' + js + '</script></body></html>'

if (html.includes(`)${BATAS}"`)) throw new Error('isi panel memuat pembatas raw string')

const ino = await Bun.file(INO).text()
const a = ino.indexOf(AWAL)
const b = ino.indexOf(AKHIR)
if (a < 0 || b < a) throw new Error(`penanda ${AWAL} / ${AKHIR} tidak ditemukan di robot_otak.ino`)

const blok = `${AWAL}\nconst char PANEL_HTML[] = R"${BATAS}(${html})${BATAS}";\n`
await Bun.write(INO, ino.slice(0, a) + blok + ino.slice(b))
console.log(`panel: ${html.length} byte ditanam ke robot_otak.ino`)
