// Mengeluarkan teks tahap yang dibuat JavaScript di stasiun-cuaca/index.html sebagai JSON {kunci: HTML Indonesia}.
// Kuncinya sama dengan atribut data-t="js:..." di templat halaman. Dipakai terjemah.py.
const fs = require('fs')
const s = fs.readFileSync(process.argv[2], 'utf8')
const a = s.indexOf('const STAGES=['), b = s.indexOf('const LCDROWS'), c = s.indexOf('\n', b)
const { STAGES, LCDROWS } = new Function(s.slice(a, c) + ';return {STAGES,LCDROWS}')()
const o = {}
const k0 = s.indexOf('const COMPONENTS=['), k1 = s.indexOf('\n', s.indexOf('const BADGE='))
const { COMPONENTS, BADGE } = new Function(s.slice(k0, k1) + ';return {COMPONENTS,BADGE}')()
COMPONENTS.forEach((c, i) => { o[`js:cn${i}`] = c[1]; o[`js:cw${i}`] = c[3]; o[`js:ch${i}`] = c[4] })
Object.keys(BADGE).forEach((u) => (o[`js:cb${u}`] = BADGE[u][0]))
for (const st of STAGES) {
  o[`js:t${st.n}`] = st.t
  st.need.forEach((x, i) => (o[`js:n${st.n}-${i}`] = x))
  if (st.pre) o[`js:p${st.n}`] = st.pre
  st.steps.forEach((x, i) => (o[`js:s${st.n}-${i}`] = x))
  o[`js:o${st.n}`] = st.ok
  if (st.learn) o[`js:l${st.n}`] = st.learn
}
o['js:lcdcap'] = 'Pin 1 dan 16 ditandai angka di tepi papan LCD.'
LCDROWS.forEach((r, i) => (o[`js:lcd${i}`] = r[2]))
process.stdout.write(JSON.stringify(o))
