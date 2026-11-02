<script lang="ts">
  import { SKILLS } from '../data/portfolio.ts'
  import { reveal } from '../lib/actions.ts'
  import { t } from '../lib/state.svelte.ts'

  // Indeks awal tiap grup supaya jeda animasi antar tag berlanjut lintas grup.
  const tagOffsets = SKILLS.map((_, gi) => SKILLS.slice(0, gi).reduce((n, g) => n + g.tags.length, 0))
</script>

<section class="skills section" id="skills">
  <div class="bd-container">
    <h2 class="section-title" use:reveal={{ cls: 'reveal' }}>{t('section.skills')}</h2>
    <div class="skills__groups">
      {#each SKILLS as group, gi (group.group)}
        <div class="skills__group">
          <h3 class="skills__group-title" use:reveal={{ cls: 'reveal', index: gi }}>
            <i class="bx {group.icon}"></i>
            {group.group}
          </h3>
          <div class="skills__wrap">
            {#each group.tags as tag, ti (tag)}
              <span class="skill-tag" use:reveal={{ index: tagOffsets[gi]! + ti, step: 0.06 }}>{tag}</span>
            {/each}
          </div>
        </div>
      {/each}
    </div>
  </div>
</section>
