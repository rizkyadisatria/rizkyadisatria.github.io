<script lang="ts">
  import { SERVICES } from '../data/portfolio.ts'
  import { reveal } from '../lib/actions.ts'
  import { t } from '../lib/state.svelte.ts'
</script>

<section class="services section" id="services">
  <div class="bd-container">
    <h2 class="section-title" use:reveal={{ cls: 'reveal' }}>{t('section.services')}</h2>
    <div class="services__grid">
      {#each SERVICES as service, i (service.title)}
        <div class="service-card" use:reveal={{ cls: 'reveal', index: i }}>
          <i class="bx {service.icon} service-card__icon"></i>
          <h3 class="service-card__title">{t(service.title)}</h3>
          <p class="service-card__desc">{t(service.desc)}</p>
        </div>
      {/each}
    </div>
  </div>
</section>
