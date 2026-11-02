<script lang="ts">
  import { reveal } from '../lib/actions.ts'
  import { t } from '../lib/state.svelte.ts'
</script>

<section class="profile section" id="profile">
  <div class="bd-container">
    <h2 class="section-title" use:reveal={{ cls: 'reveal' }}>{t('section.about')}</h2>
    <p class="profile__description" use:reveal={{ cls: 'reveal' }}>{t('about.text')}</p>
  </div>
</section>
