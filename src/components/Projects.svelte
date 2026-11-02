<script lang="ts">
  import { PROJECTS } from '../data/portfolio.ts'
  import { reveal } from '../lib/actions.ts'
  import { t } from '../lib/state.svelte.ts'
</script>

<section class="certificate section" id="projects">
  <div class="bd-container">
    <h2 class="section-title" use:reveal={{ cls: 'reveal' }}>{t('section.projects')}</h2>
    <div class="projects__grid">
      {#each PROJECTS as project, i (project.title)}
        <article class="project-card" use:reveal={{ index: i, step: 0.08 }}>
          <div class="project-card__top">
            <i class="bx {project.icon} project-card__icon"></i>
            <span class="project-card__year">{project.year}</span>
          </div>
          <h3 class="project-card__title">{project.title}</h3>
          <p class="project-card__desc">{t(project.desc)}</p>
          <div class="project-card__tags">
            {#each project.tags as tag (tag)}
              <span class="project-tag">{tag}</span>
            {/each}
          </div>
          <a
            href={project.href}
            target="_blank"
            rel="noopener noreferrer"
            class="project-card__link"
            aria-label="View {project.title} project"
          >
            <span>{t('proj.view')}</span> <i class="bx bx-link-external"></i>
          </a>
        </article>
      {/each}
    </div>
  </div>
</section>
