<script lang="ts">
  import { CONTACT, ROLES, STATS } from '../data/portfolio.ts'
  import { countUp } from '../lib/actions.ts'
  import { t } from '../lib/state.svelte.ts'
  import HeroLottie from './HeroLottie.svelte'

  let { onDownloadCv }: { onDownloadCv: () => void } = $props()

  // Efek mengetik: teks penuh terlihat dulu, lalu dihapus dan berganti peran.
  let profession = $state<string>(ROLES[0])

  $effect(() => {
    const typeSpeed = 65
    const deleteSpeed = 35
    const pauseAfterType = 1800
    const pauseAfterDelete = 300

    let roleIdx = 0
    let charIdx = ROLES[0].length
    let deleting = true
    let timer: ReturnType<typeof setTimeout>

    const tick = () => {
      const role = ROLES[roleIdx]
      if (!deleting) {
        charIdx++
        profession = role.slice(0, charIdx)
        if (charIdx === role.length) {
          deleting = true
          timer = setTimeout(tick, pauseAfterType)
          return
        }
        timer = setTimeout(tick, typeSpeed)
      } else {
        charIdx--
        profession = role.slice(0, charIdx)
        if (charIdx === 0) {
          deleting = false
          roleIdx = (roleIdx + 1) % ROLES.length
          timer = setTimeout(tick, pauseAfterDelete)
          return
        }
        timer = setTimeout(tick, deleteSpeed)
      }
    }

    // mulai setelah animasi masuk awal selesai
    timer = setTimeout(tick, 1400)
    return () => clearTimeout(timer)
  })
</script>

{#snippet stats(variant: 'desktop' | 'mobile')}
  <div class="home__stats home__stats--{variant}">
    {#each STATS as stat, i (stat.key)}
      {#if i > 0}<div class="home__stat-divider"></div>{/if}
      <div class="home__stat">
        <span class="home__stat-num" use:countUp>{stat.num}</span>
        <span class="home__stat-label">{t(stat.key)}</span>
      </div>
    {/each}
  </div>
{/snippet}

<section class="home section" id="home">
  <HeroLottie />
  <div class="home__container bd-container">
    <div class="home__visual">
      <div class="home__img-wrap">
        <img src="/assets/img/profile.jpg" alt="Rizky Adi Satria" class="home__img" />
      </div>
      <!-- Stats: desktop only (below photo) -->
      {@render stats('desktop')}
    </div>

    <div class="home__data">
      <span class="home__badge">
        <span class="home__badge-dot"></span>
        <span>{t('hero.badge')}</span>
      </span>
      <span class="home__greeting">{t('hero.greeting')}</span>
      <h1 class="home__title">Rizky Adi Satria</h1>
      <p class="home__profession">{profession}</p>
      <p class="home__bio">{t('hero.bio')}</p>
      <ul class="home__meta">
        <li>
          <i class="bx bx-map-pin"></i>
          <span>{CONTACT.location}</span>
        </li>
        <li>
          <a href="mailto:{CONTACT.email}">
            <i class="bx bx-envelope"></i>
            <span>{CONTACT.email}</span>
          </a>
        </li>
        <li>
          <a href={CONTACT.whatsapp} target="_blank" rel="noopener noreferrer">
            <i class="bx bx-phone"></i>
            <span>{CONTACT.phone}</span>
          </a>
        </li>
      </ul>
      <div class="home__btns">
        <button class="btn btn--primary" id="download-cv-btn" onclick={onDownloadCv}>
          <i class="bx bx-file-blank"></i>
          <span>{t('btn.download')}</span>
        </button>
        <a href="#projects" class="btn btn--outline">{t('btn.projects')}</a>
      </div>
      <!-- Stats: mobile only (below buttons) -->
      {@render stats('mobile')}
    </div>
  </div>
</section>
