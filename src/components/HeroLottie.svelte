<script lang="ts">
  import type { AnimationItem } from 'lottie-web'
  import { app } from '../lib/state.svelte.ts'

  let container = $state<HTMLDivElement>()

  // Muat ulang animasi saat tema berganti (ada versi terang & gelap).
  $effect(() => {
    const el = container
    if (!el) return
    const path = app.dark ? '/assets/lottie/background01-dark.json' : '/assets/lottie/background01.json'

    let anim: AnimationItem | undefined
    let cancelled = false

    // Versi "light" tanpa expression engine (tanpa eval) — animasi ini tidak memakai expression.
    import('lottie-web/build/player/lottie_light').then(({ default: lottie }) => {
      if (cancelled) return
      anim = lottie.loadAnimation({
        container: el,
        renderer: 'svg',
        loop: true,
        autoplay: true,
        path,
        rendererSettings: { preserveAspectRatio: 'xMidYMid slice' },
      })
    })

    return () => {
      cancelled = true
      anim?.destroy()
    }
  })
</script>

<div id="hero-lottie" class="hero-lottie" bind:this={container}></div>
