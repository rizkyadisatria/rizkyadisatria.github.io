<script lang="ts">
  import { app, META } from './lib/state.svelte.ts'
  import { generateCV } from './lib/cv.ts'
  import Header from './components/Header.svelte'
  import Hero from './components/Hero.svelte'
  import About from './components/About.svelte'
  import Services from './components/Services.svelte'
  import Experience from './components/Experience.svelte'
  import Education from './components/Education.svelte'
  import Skills from './components/Skills.svelte'
  import Projects from './components/Projects.svelte'
  import Contact from './components/Contact.svelte'
  import BottomRow from './components/BottomRow.svelte'
  import CvPrint from './components/CvPrint.svelte'
  import ScrollTop from './components/ScrollTop.svelte'

  let cvEl = $state<HTMLElement>()

  // Tema: kelas `dark-theme` di <body> dipakai seluruh CSS lama.
  $effect(() => {
    document.body.classList.toggle('dark-theme', app.dark)
  })

  // Bahasa: <html lang>, atribut data-lang, <title> dan meta description ikut berganti.
  $effect(() => {
    const root = document.documentElement
    root.setAttribute('lang', app.lang)
    root.setAttribute('data-lang', app.lang)
    document.title = META[app.lang].title
    document
      .querySelector('meta[name="description"]')
      ?.setAttribute('content', META[app.lang].description)
  })

  async function downloadCv(): Promise<void> {
    if (cvEl) await generateCV(cvEl)
  }
</script>

<a class="skip-link" href="#main-content">Skip to content</a>

<Header onDownloadCv={downloadCv} />

<main class="l-main" id="main-content">
  <div id="area-cv">
    <Hero onDownloadCv={downloadCv} />
    <About />
    <Services />
    <Experience />
    <Education />
    <Skills />
    <Projects />
    <Contact />
    <BottomRow />
  </div>
</main>

<CvPrint bind:el={cvEl} />

<ScrollTop />
