// Server lokal: build → sajikan dist/, rebuild otomatis saat src/ atau public/ berubah.
import { watch } from 'node:fs'
import { join, normalize, sep } from 'node:path'
import { build, DIST } from './build.ts'

const port = Number(process.env.PORT ?? 3000)
const watching = !process.argv.includes('--no-watch')

async function rebuild(): Promise<void> {
  try {
    const { ms } = await build()
    console.log(`✓ build ${ms} ms`)
  } catch (e) {
    console.error('✗ build gagal:', (e as Error).message)
  }
}

await rebuild()

Bun.serve({
  port,
  async fetch(req) {
    const url = new URL(req.url)
    let pathname: string
    try {
      pathname = decodeURIComponent(url.pathname)
    } catch {
      return new Response('Bad request', { status: 400 })
    }

    const target = normalize(join(DIST, pathname))
    if (target !== DIST && !target.startsWith(DIST + sep)) return new Response('Forbidden', { status: 403 })

    let file = Bun.file(target)
    if (!pathname.endsWith('/') && !(await file.exists())) return new Response('Not found', { status: 404 })

    // direktori → index.html (seperti GitHub Pages / Vercel dengan trailingSlash)
    const index = Bun.file(join(target, 'index.html'))
    if (await index.exists()) {
      if (!pathname.endsWith('/')) return Response.redirect(`${url.pathname}/${url.search}`, 301)
      file = index
    }
    return (await file.exists()) ? new Response(file) : new Response('Not found', { status: 404 })
  },
})

console.log(`Dev server: http://localhost:${port}`)

if (watching) {
  let timer: ReturnType<typeof setTimeout> | undefined
  for (const dir of ['src', 'public']) {
    watch(join(import.meta.dir, dir), { recursive: true }, () => {
      clearTimeout(timer)
      timer = setTimeout(rebuild, 150)
    })
  }
}
