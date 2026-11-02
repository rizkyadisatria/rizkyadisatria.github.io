<script lang="ts">
  import { LANGUAGES, SOCIAL } from '../data/portfolio.ts'
  import { reveal } from '../lib/actions.ts'
  import { t } from '../lib/state.svelte.ts'
</script>

<section class="bottom-row section">
  <div class="bd-container bottom-row__grid">
    <div>
      <h2 class="section-title" use:reveal={{ cls: 'reveal' }}>{t('section.social')}</h2>
      <div class="social-links">
        {#each SOCIAL as link, i (link.href)}
          <a href={link.href} target="_blank" rel="noopener noreferrer" class="social-link" use:reveal={{ index: i }}>
            <i class="bx {link.icon}"></i><span>{link.label}</span>
          </a>
        {/each}
      </div>
    </div>
    <div>
      <h2 class="section-title" use:reveal={{ cls: 'reveal' }}>{t('section.languages')}</h2>
      <div class="lang-list">
        {#each LANGUAGES as lang, i (lang.key)}
          <div class="lang-item" use:reveal={{ index: SOCIAL.length + i }}>
            <i class="bx bx-check-circle"></i>
            <span>{t(lang.key)}</span>
          </div>
        {/each}
      </div>
    </div>
  </div>
</section>
