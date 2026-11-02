import type { Action } from 'svelte/action'

type RevealOptions = {
  /** Kelas animasi awal. Elemen tanpa kelas (skill-tag, project-card, dst.) sudah punya state awal di CSS. */
  cls?: 'reveal' | 'reveal-left'
  /** Urutan elemen untuk efek bertahap (stagger). */
  index?: number
  /** Jeda antar elemen, dalam detik. */
  step?: number
}

let revealObserver: IntersectionObserver | undefined

function getRevealObserver(): IntersectionObserver | undefined {
  if (typeof IntersectionObserver === 'undefined') return undefined
  revealObserver ??= new IntersectionObserver(
    (entries) => {
      for (const entry of entries) {
        if (!entry.isIntersecting) continue
        entry.target.classList.add('visible')
        revealObserver?.unobserve(entry.target)
      }
    },
    { threshold: 0.12, rootMargin: '0px 0px -30px 0px' },
  )
  return revealObserver
}

/** Fade/slide saat elemen masuk viewport. Tanpa IntersectionObserver, elemen langsung terlihat. */
export const reveal: Action<HTMLElement, RevealOptions | undefined> = (node, options) => {
  const { cls, index, step = 0.1 } = options ?? {}
  if (cls) node.classList.add(cls)
  if (index !== undefined) node.style.transitionDelay = `${index * step}s`

  const observer = getRevealObserver()
  if (!observer) {
    node.classList.add('visible')
    return
  }
  observer.observe(node)
  return { destroy: () => observer.unobserve(node) }
}

function animateCount(el: HTMLElement): void {
  const raw = (el.textContent ?? '').trim()
  const target = parseInt(raw, 10)
  const suffix = raw.replace(/[0-9]/g, '')
  if (Number.isNaN(target)) return

  const duration = 1200
  const start = performance.now()

  const tick = (now: number) => {
    const t = Math.min((now - start) / duration, 1)
    const eased = 1 - Math.pow(1 - t, 3)
    el.textContent = Math.round(eased * target) + suffix
    if (t < 1) requestAnimationFrame(tick)
  }
  tick(start)
}

/** Animasi angka naik (mis. "4+") saat masuk viewport. */
export const countUp: Action<HTMLElement> = (node) => {
  if (typeof IntersectionObserver === 'undefined') return
  const observer = new IntersectionObserver(
    (entries) => {
      for (const entry of entries) {
        if (!entry.isIntersecting) continue
        animateCount(entry.target as HTMLElement)
        observer.unobserve(entry.target)
      }
    },
    { threshold: 0.2 },
  )
  observer.observe(node)
  return { destroy: () => observer.disconnect() }
}
