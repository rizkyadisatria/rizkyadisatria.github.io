<script lang="ts">
  import { EDUCATION, EXPERIENCE } from '../data/portfolio.ts'
  import { reveal } from '../lib/actions.ts'
  import { t } from '../lib/state.svelte.ts'
</script>

<section class="education section" id="education">
  <div class="bd-container">
    <h2 class="section-title" use:reveal={{ cls: 'reveal' }}>{t('section.education')}</h2>
    <div class="timeline">
      {#each EDUCATION as item, i (item.title)}
        <div
          class="timeline__item"
          class:timeline__item--last={i === EDUCATION.length - 1}
          use:reveal={{ cls: 'reveal-left', index: EXPERIENCE.length + i }}
        >
          <div class="timeline__dot"></div>
          <div class="timeline__body">
            <div class="timeline__row">
              <h3 class="timeline__title">{t(item.title)}</h3>
              <span class="timeline__badge">{item.period}</span>
            </div>
            <p class="timeline__company">{item.school}</p>
          </div>
        </div>
      {/each}
    </div>
  </div>
</section>
