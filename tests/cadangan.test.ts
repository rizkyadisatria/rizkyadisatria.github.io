// Tes logika cadangan progres (public/controller/assets/cadangan.js), tanpa browser.
import { describe, expect, test } from 'bun:test'
import { createRequire } from 'node:module'

const require = createRequire(import.meta.url)
const C = require('../public/controller/assets/cadangan.js')

class Penyimpanan {
  private m = new Map<string, string>()
  get length() { return this.m.size }
  key(i: number) { return [...this.m.keys()][i] ?? null }
  getItem(k: string) { return this.m.has(k) ? this.m.get(k)! : null }
  setItem(k: string, v: string) { this.m.set(k, String(v)) }
  removeItem(k: string) { this.m.delete(k) }
}
const buat = (isi: Record<string, unknown> = {}) => {
  const s = new Penyimpanan()
  for (const [k, v] of Object.entries(isi)) s.setItem(k, typeof v === 'string' ? v : JSON.stringify(v))
  return s
}
const berkas = (data: unknown, ekstra: object = {}) => JSON.stringify({ jenis: C.JENIS, versi: C.VERSI, data, ...ekstra })

describe('kunciProgres', () => {
  test('hanya kunci panduan yang dikenal', () => {
    for (const k of ['robot-ai/1-daya', 'robot-ai/utama', 'robot-ai/2-badan:ringkas', 'elektronika-dasar', 'elektronika-dasar:ringkas', 'rakit-cuaca'])
      expect(C.kunciProgres(k)).toBe(true)
    for (const k of ['lang', 'selected-theme', 'panduan-tema', 'sudah-pindah', 'mode', 'robot-ai/', 'robot-ai/A B', '__proto__', '', 'x'.repeat(200)])
      expect(C.kunciProgres(k)).toBe(false)
  })
})

describe('buatBerkas', () => {
  test('hanya memuat progres, bukan tema/bahasa, dan membuang data rusak', () => {
    const s = buat({ lang: 'en', 'panduan-tema': 'dark', 'robot-ai/1-daya': { a: true, b: false }, 'robot-ai/1-daya:ringkas': { d: 1, n: 2 }, 'rakit-cuaca': 'bukan json', 'elektronika-dasar': { x: 'ya' } })
    const out = C.buatBerkas(s, new Date('2026-11-02T00:00:00Z'))
    expect(out.jenis).toBe(C.JENIS)
    expect(out.dibuat).toBe('2026-11-02T00:00:00.000Z')
    expect(Object.keys(out.data).sort()).toEqual(['robot-ai/1-daya', 'robot-ai/1-daya:ringkas'])
  })
})

describe('periksa', () => {
  test('menolak berkas yang bukan cadangan', () => {
    expect(C.periksa('').galat).toBe('kosong')
    expect(C.periksa('{bukan json').galat).toBe('bukan-json')
    expect(C.periksa('[]').galat).toBe('bukan-cadangan')
    expect(C.periksa(JSON.stringify({ jenis: 'lain', versi: 1, data: {} })).galat).toBe('bukan-cadangan')
    expect(C.periksa(berkas({}, { versi: 9 })).galat).toBe('versi')
    expect(C.periksa(berkas({})).galat).toBe('tidak-ada-data')
    expect(C.periksa('x'.repeat(1_000_001)).galat).toBe('terlalu-besar')
  })
  test('membuang kunci asing dan nilai rusak, menerima yang sah', () => {
    const h = C.periksa(berkas({
      'robot-ai/1-daya': { 'a-1': true }, lang: { x: true }, 'robot-ai/2-badan': { a: 'true' },
      'robot-ai/3-otak': { 'a b': true }, 'robot-ai/4-mata:ringkas': { d: 5, n: 2 }, 'robot-ai/5-ai:ringkas': { d: 1, n: 3 }, __proto__: { x: 1 },
    }))
    expect(h.ok).toBe(true)
    expect(Object.keys(h.data).sort()).toEqual(['robot-ai/1-daya', 'robot-ai/5-ai:ringkas'])
    expect(h.ditolak).toBe(4)
  })
  test('tidak mencemari prototipe lewat kunci berbahaya', () => {
    const h = C.periksa('{"jenis":"' + C.JENIS + '","versi":1,"data":{"robot-ai/1-daya":{"__proto__":true,"a":true}}}')
    expect(h.ok).toBe(false)
    expect(({} as any).a).toBeUndefined()
  })
})

describe('gabung', () => {
  test('centang yang sudah ada tidak dicabut, yang baru ditambah, ringkasan dihitung ulang', () => {
    const s = buat({ 'robot-ai/1-daya': { a: true, b: true }, 'robot-ai/1-daya:ringkas': { d: 2, n: 4 } })
    const h = C.periksa(berkas({ 'robot-ai/1-daya': { a: false, c: true, d: false }, 'robot-ai/1-daya:ringkas': { d: 1, n: 4 }, 'robot-ai/2-badan': { z: true }, 'robot-ai/2-badan:ringkas': { d: 1, n: 9 } }))
    expect(C.gabung(s, h.data)).toBe(2)
    expect(JSON.parse(s.getItem('robot-ai/1-daya')!)).toEqual({ a: true, b: true, c: true, d: false })
    expect(JSON.parse(s.getItem('robot-ai/1-daya:ringkas')!)).toEqual({ d: 3, n: 4 })
    expect(JSON.parse(s.getItem('robot-ai/2-badan:ringkas')!)).toEqual({ d: 1, n: 9 })
  })
  test('centang barang (have-) tidak dihitung sebagai langkah', () => {
    const s = buat({})
    const h = C.periksa(berkas({ 'robot-ai/utama': { 'have-bms': true, 'have-wago': true, s1: true }, 'robot-ai/utama:ringkas': { d: 0, n: 3 } }))
    C.gabung(s, h.data)
    expect(JSON.parse(s.getItem('robot-ai/utama:ringkas')!)).toEqual({ d: 1, n: 3 })
  })
  test('tidak menyentuh kunci non-progres', () => {
    const s = buat({ lang: 'en', 'panduan-tema': 'dark' })
    C.gabung(s, C.periksa(berkas({ 'rakit-cuaca': { a: true } })).data)
    expect(s.getItem('lang')).toBe('en')
    expect(s.getItem('panduan-tema')).toBe('dark')
  })
})

describe('ganti', () => {
  test('menghapus semua progres lama, tapi tidak tema/bahasa', () => {
    const s = buat({ lang: 'en', 'panduan-tema': 'dark', 'robot-ai/1-daya': { a: true }, 'robot-ai/1-daya:ringkas': { d: 1, n: 1 }, 'rakit-cuaca': { q: true } })
    C.ganti(s, C.periksa(berkas({ 'elektronika-dasar': { x: true }, 'elektronika-dasar:ringkas': { d: 1, n: 5 } })).data)
    expect(s.getItem('robot-ai/1-daya')).toBeNull()
    expect(s.getItem('rakit-cuaca')).toBeNull()
    expect(JSON.parse(s.getItem('elektronika-dasar')!)).toEqual({ x: true })
    expect(s.getItem('lang')).toBe('en')
    expect(s.getItem('panduan-tema')).toBe('dark')
  })
  test('ekspor lalu impor di penyimpanan kosong memulihkan persis', () => {
    const asal = buat({ 'robot-ai/3-otak': { a: true, b: true }, 'robot-ai/3-otak:ringkas': { d: 2, n: 6 }, 'robot-ai/utama': { 'have-bms': false, s: true }, 'robot-ai/utama:ringkas': { d: 1, n: 3 } })
    const file = JSON.stringify(C.buatBerkas(asal))
    const tujuan = buat({})
    const h = C.periksa(file)
    C.ganti(tujuan, h.data)
    for (const k of ['robot-ai/3-otak', 'robot-ai/3-otak:ringkas', 'robot-ai/utama', 'robot-ai/utama:ringkas'])
      expect(JSON.parse(tujuan.getItem(k)!)).toEqual(JSON.parse(asal.getItem(k)!))
  })
})
