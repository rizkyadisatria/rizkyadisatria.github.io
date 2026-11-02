<script lang="ts">
  import { EXPERIENCE } from '../data/portfolio.ts'
  import { reveal } from '../lib/actions.ts'
  import { t } from '../lib/state.svelte.ts'
</script>

<section class="experience section" id="experience">
  <div class="bd-container">
    <h2 class="section-title" use:reveal={{ cls: 'reveal' }}>{t('section.experience')}</h2>
    <div class="timeline">
      {#each EXPERIENCE as item, i (item.title)}
        <div
          class="timeline__item"
          class:timeline__item--last={i === EXPERIENCE.length - 1}
          use:reveal={{ cls: 'reveal-left', index: i }}
        >
          <div class="timeline__dot"></div>
          <div class="timeline__body">
            <div class="timeline__row">
              <h3 class="timeline__title">{t(item.title)}</h3>
              <span class="timeline__badge">{t(item.badge)}</span>
            </div>
            {#if item.company}
              <p class="timeline__company">
                {item.company.name} &mdash;
                <a href={item.company.href} target="_blank" rel="noopener noreferrer">{item.company.label}</a>
              </p>
            {:else if item.companyKey}
              <p class="timeline__company">{t(item.companyKey)}</p>
            {/if}
            <p class="timeline__desc">{t(item.desc)}</p>
          </div>
        </div>
      {/each}
    </div>
  </div>
</section>
