<script lang="ts">
  import { NAV } from '../data/portfolio.ts'
  import { app, t } from '../lib/state.svelte.ts'

  let { onDownloadCv }: { onDownloadCv: () => void } = $props()

  // Home aktif di awal (sama seperti HTML lama); selanjutnya dihitung saat scroll.
  let active = $state<string[]>(['home'])
  let headerEl = $state<HTMLElement>()

  function updateActive(): void {
    const offset = headerEl ? headerEl.offsetHeight + 16 : 70
    const y = window.scrollY
    const ids: string[] = []
    for (const { id } of NAV) {
      const section = document.getElementById(id)
      if (!section) continue
      const top = section.offsetTop - offset
      if (y >= top && y < top + section.offsetHeight) ids.push(id)
    }
    active = ids
  }

  $effect(() => {
    window.addEventListener('scroll', updateActive, { passive: true })
    return () => window.removeEventListener('scroll', updateActive)
  })

  function onKey(event: KeyboardEvent, action: () => void): void {
    if (event.key === 'Enter' || event.key === ' ') {
      event.preventDefault()
      action()
    }
  }
</script>

<header class="l-header" id="header" bind:this={headerEl}>
  <nav class="nav bd-container">
    <!-- svelte-ignore a11y_invalid_attribute -->
    <a href="#" class="nav__logo">Rizky<span class="nav__logo-dot">.</span></a>

    <div class="nav__menu" class:show-menu={app.menuOpen} id="nav-menu">
      <ul class="nav__list">
        {#each NAV as item (item.id)}
          <li class="nav__item">
            <a
              href="#{item.id}"
              class="nav__link"
              class:active-link={active.includes(item.id)}
              onclick={() => (app.menuOpen = false)}
            >
              <i class="bx {item.icon} nav__icon"></i>
              <span>{t(item.key)}</span>
            </a>
          </li>
        {/each}
      </ul>
    </div>

    <div class="nav__actions">
      <button
        class="lang-toggle"
        id="lang-toggle"
        title="Switch Language"
        aria-label={app.lang === 'en' ? 'Switch language to Indonesian' : 'Ganti bahasa ke Inggris'}
        onclick={() => app.toggleLang()}
      >
        {app.lang === 'en' ? 'ID' : 'EN'}
      </button>
      <i
        class="bx change-theme"
        class:bx-moon={!app.dark}
        class:bx-sun={app.dark}
        id="theme-button"
        title="Toggle theme"
        role="button"
        tabindex="0"
        aria-label="Toggle theme"
        onclick={() => app.toggleTheme()}
        onkeydown={(e) => onKey(e, () => app.toggleTheme())}
      ></i>
      <i
        class="bx bxs-file-pdf generate-pdf"
        id="resume-button"
        title="Download PDF"
        role="button"
        tabindex="0"
        aria-label="Download PDF"
        onclick={onDownloadCv}
        onkeydown={(e) => onKey(e, onDownloadCv)}
      ></i>
      <button
        class="nav__toggle"
        id="nav-toggle"
        aria-expanded={app.menuOpen}
        aria-label={app.menuOpen ? 'Close menu' : 'Open menu'}
        onclick={() => (app.menuOpen = !app.menuOpen)}
      >
        <i class="bx bx-menu"></i>
      </button>
    </div>
  </nav>
</header>
