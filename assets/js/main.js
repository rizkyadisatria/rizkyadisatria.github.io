/*==================== TRANSLATIONS ====================*/
const i18n = {
  en: {
    'nav.home':        'Home',
    'nav.about':       'About',
    'nav.services':    'Services',
    'nav.experience':  'Experience',
    'nav.skills':      'Skills',
    'nav.projects':    'Projects',
    'nav.contact':     'Contact',

    'hero.badge':      'Available for work',
    'hero.greeting':   "Hi there, I'm",
    'hero.profession': 'Full Stack Developer',
    'hero.bio':        'Full Stack Developer with 4+ years of experience building scalable web applications and e-commerce platforms. I enjoy turning complex problems into clean, elegant solutions.',

    'stat.years':      'Years Exp.',
    'stat.projects':   'Projects',
    'stat.companies':  'Companies',

    'btn.download':    'Download CV',
    'btn.projects':    'See Projects',
    'proj.view':       'View Project',

    'section.about':      'About Me',
    'section.services':   'Services',
    'section.experience': 'Experience',
    'section.education':  'Education',
    'section.skills':     'Skills',
    'section.projects':   'Projects',
    'section.contact':    'Get in Touch',
    'section.social':     'Social',
    'section.languages':  'Languages',

    'about.text': "I'm a Full Stack Developer with over 4 years of professional experience building end-to-end web solutions. I specialize in e-commerce platforms and have a strong command of both frontend and backend technologies. I enjoy tackling complex challenges and delivering clean, performant code. Outside of my main role, I take on freelance projects ranging from hotel booking systems and company profiles to VPS deployment and network management.",

    'exp.badge1':  'Aug 2021 – Feb 2022',
    'exp.badge2':  'Mar 2022 – Present',
    'exp.badge3':  'Aug 2023 – Present',
    'exp1.title':  'Technical Support',
    'exp1.desc':   'Resolved client issues related to domain management, hosting configuration, and WordPress websites. Provided step-by-step technical guidance and troubleshooting support.',
    'exp2.title':  'Full Stack Developer',
    'exp2.desc':   'Built and maintained the Bumigorashop e-commerce platform from the ground up, collaborating with a designer to create a marketplace that empowers local sellers to reach wider markets.',
    'exp3.title':  'Freelance Developer',
    'exp3.company':'Self-employed',
    'exp3.desc':   'Deliver end-to-end web and mobile solutions for various clients — hotel booking systems, company profiles, hosting order platforms, Mikrotik management with Mikhmon, remote VPN setups, and VPS deployment.',

    'edu1.title':  'Network Engineering',
    'edu2.title':  'Bachelor of Computer Science',

    'proj1.desc':  'A functional clone of Google Search built with React.js, Tailwind CSS, and the Google Search API — replicating the UI and delivering real-time search results.',
    'proj2.desc':  'A furniture e-commerce landing page built from a Figma design using HTML, CSS, and vanilla JavaScript. Course final project from BuildWithAngga.',
    'proj3.desc':  'A coffee shop landing page built from a Figma design using HTML, CSS, and JavaScript. Course final project from Progate.',
    'proj4.desc':  'A Pokédex web app integrating the public PokéAPI, allowing users to browse and "catch" Pokémon. Built with HTML, CSS, and JavaScript.',
    'proj5.desc':  'A hotel booking website for a property in Kuta, Lombok. Built with WordPress and integrated with a third-party payment gateway.',
    'proj6.desc':  'A villa booking website built with WordPress, developed through a referral partnership with Lina Boutique Villas & Spa for properties in Lombok.',
    'proj7.desc':  'A full-featured B2B platform built with the MERN stack. Features product catalog, user authentication, admin dashboard, purchasing system, and payment gateway.',
    'proj8.desc':  'A school fee management platform (SPP) with invoice generation, multi-gateway payments, WhatsApp notifications, and subscription billing. Built with Laravel 12, React, Inertia.js, and Firebase.',

    'lang.id': 'Indonesian — Native',
    'lang.en': 'English — Conversational',

    // Services
    'svc1.title': 'E-Commerce',
    'svc1.desc': 'Custom online stores with product management, payment gateways, and admin dashboards.',
    'svc2.title': 'Company Profile',
    'svc2.desc': 'Professional landing pages and company websites that showcase your brand and services.',
    'svc3.title': 'Booking Systems',
    'svc3.desc': 'Hotel, villa, and property booking platforms with calendar management and payment integration.',
    'svc4.title': 'VPS & DevOps',
    'svc4.desc': 'Linux VPS setup, deployment automation, VPN configuration, Mikrotik management, and network infrastructure.',
    'svc5.title': 'Custom Web App',
    'svc5.desc': 'Tailor-made web applications — SaaS platforms, dashboards, internal tools — built to your specifications.',
    'svc6.title': 'WordPress',
    'svc6.desc': 'WordPress site development, theme customization, plugin setup, and hosting configuration.',

    // Contact
    'contact.intro': 'Have a project in mind or want to discuss an opportunity? I\'d love to hear from you.',
    'contact.name': 'Name',
    'contact.email': 'Email',
    'contact.message': 'Message',
    'contact.send': 'Send Message',
    'contact.note': 'Or reach me directly via <a href="mailto:rizkyadsatria@gmail.com">email</a> or <a href="https://wa.me/6285933005036" target="_blank" rel="noopener noreferrer">WhatsApp</a>.',
    'contact.namePlaceholder': 'Your name',
    'contact.emailPlaceholder': 'your@email.com',
    'contact.messagePlaceholder': 'Tell me about your project...',
  },
  id: {
    'nav.home':        'Beranda',
    'nav.about':       'Tentang',
    'nav.services':    'Layanan',
    'nav.experience':  'Pengalaman',
    'nav.skills':      'Keahlian',
    'nav.projects':    'Proyek',
    'nav.contact':     'Kontak',

    'hero.badge':      'Tersedia untuk bekerja',
    'hero.greeting':   'Halo, nama saya',
    'hero.profession': 'Full Stack Developer',
    'hero.bio':        'Full Stack Developer dengan 4+ tahun pengalaman membangun aplikasi web dan platform e-commerce yang skalabel. Saya senang mengubah masalah kompleks menjadi solusi yang bersih dan elegan.',

    'stat.years':      'Pengalaman',
    'stat.projects':   'Proyek',
    'stat.companies':  'Perusahaan',

    'btn.download':    'Unduh CV',
    'btn.projects':    'Lihat Proyek',
    'proj.view':       'Lihat Proyek',

    'section.about':      'Tentang Saya',
    'section.services':   'Layanan',
    'section.experience': 'Pengalaman',
    'section.education':  'Pendidikan',
    'section.skills':     'Keahlian',
    'section.projects':   'Proyek',
    'section.contact':    'Hubungi Saya',
    'section.social':     'Media Sosial',
    'section.languages':  'Bahasa',

    'about.text': 'Saya adalah Full Stack Developer dengan lebih dari 4 tahun pengalaman profesional dalam membangun solusi web end-to-end. Saya spesialisasi dalam platform e-commerce dan memiliki kemampuan kuat di bidang teknologi frontend maupun backend. Saya senang menghadapi tantangan kompleks dan menghasilkan kode yang bersih dan berperforma tinggi. Di luar pekerjaan utama, saya juga mengerjakan proyek freelance mulai dari sistem pemesanan hotel, profil perusahaan, hingga deployment VPS dan manajemen jaringan.',

    'exp.badge1':  'Agu 2021 – Feb 2022',
    'exp.badge2':  'Mar 2022 – Sekarang',
    'exp.badge3':  'Agu 2023 – Sekarang',
    'exp1.title':  'Technical Support',
    'exp1.desc':   'Menangani masalah klien terkait pengelolaan domain, konfigurasi hosting, dan website WordPress. Memberikan panduan teknis dan dukungan troubleshooting secara bertahap.',
    'exp2.title':  'Full Stack Developer',
    'exp2.desc':   'Membangun dan memelihara platform e-commerce Bumigorashop dari awal, berkolaborasi dengan desainer untuk menciptakan marketplace yang membantu penjual lokal menjangkau pasar lebih luas.',
    'exp3.title':  'Freelance Developer',
    'exp3.company':'Wiraswasta',
    'exp3.desc':   'Mengerjakan solusi web dan mobile end-to-end untuk berbagai klien — sistem pemesanan hotel, profil perusahaan, platform pemesanan hosting, manajemen Mikrotik dengan Mikhmon, setup VPN jarak jauh, dan deployment VPS.',

    'edu1.title':  'Teknik Jaringan Komputer',
    'edu2.title':  'Sarjana Ilmu Komputer',

    'proj1.desc':  'Klon fungsional Google Search yang dibangun dengan React.js, Tailwind CSS, dan Google Search API — meniru tampilan UI dan menampilkan hasil pencarian secara real-time.',
    'proj2.desc':  'Landing page e-commerce furnitur yang dibangun dari desain Figma menggunakan HTML, CSS, dan JavaScript vanilla. Proyek akhir kursus dari BuildWithAngga.',
    'proj3.desc':  'Landing page kedai kopi yang dibangun dari desain Figma menggunakan HTML, CSS, dan JavaScript. Proyek akhir kursus dari Progate.',
    'proj4.desc':  'Aplikasi web Pokédex yang mengintegrasikan PokéAPI publik, memungkinkan pengguna menjelajahi dan "menangkap" Pokémon. Dibangun dengan HTML, CSS, dan JavaScript.',
    'proj5.desc':  'Website pemesanan hotel untuk properti di Kuta, Lombok. Dibangun dengan WordPress dan diintegrasikan dengan payment gateway pihak ketiga.',
    'proj6.desc':  'Website pemesanan villa yang dibangun dengan WordPress, dikembangkan melalui kemitraan referral dengan Lina Boutique Villas & Spa untuk properti di Lombok.',
    'proj7.desc':  'Platform B2B berfitur lengkap yang dibangun dengan MERN stack. Fitur termasuk katalog produk, autentikasi pengguna, dashboard admin, sistem pembelian, dan payment gateway.',
    'proj8.desc':  'Platform manajemen pembayaran sekolah (SPP) dengan pembuatan invoice, pembayaran multi-gateway, notifikasi WhatsApp, dan penagihan berlangganan. Dibangun dengan Laravel 12, React, Inertia.js, dan Firebase.',

    'lang.id': 'Indonesia — Bahasa Ibu',
    'lang.en': 'Inggris — Percakapan',

    // Services
    'svc1.title': 'E-Commerce',
    'svc1.desc': 'Toko online kustom dengan manajemen produk, payment gateway, dan dashboard admin.',
    'svc2.title': 'Profil Perusahaan',
    'svc2.desc': 'Landing page profesional dan website perusahaan yang menampilkan brand dan layanan Anda.',
    'svc3.title': 'Sistem Booking',
    'svc3.desc': 'Platform pemesanan hotel, villa, dan properti dengan manajemen kalender dan integrasi pembayaran.',
    'svc4.title': 'VPS & DevOps',
    'svc4.desc': 'Setup Linux VPS, otomatisasi deployment, konfigurasi VPN, manajemen Mikrotik, dan infrastruktur jaringan.',
    'svc5.title': 'Aplikasi Web Kustom',
    'svc5.desc': 'Aplikasi web sesuai kebutuhan — platform SaaS, dashboard, tools internal — dibangun sesuai spesifikasi.',
    'svc6.title': 'WordPress',
    'svc6.desc': 'Pembuatan website WordPress, kustomisasi tema, setup plugin, dan konfigurasi hosting.',

    // Contact
    'contact.intro': 'Punya proyek atau ingin berdiskusi? Saya siap membantu.',
    'contact.name': 'Nama',
    'contact.email': 'Email',
    'contact.message': 'Pesan',
    'contact.send': 'Kirim Pesan',
    'contact.note': 'Atau hubungi langsung via <a href="mailto:rizkyadsatria@gmail.com">email</a> atau <a href="https://wa.me/6285933005036" target="_blank" rel="noopener noreferrer">WhatsApp</a>.',
    'contact.namePlaceholder': 'Nama Anda',
    'contact.emailPlaceholder': 'email@anda.com',
    'contact.messagePlaceholder': 'Ceritakan tentang proyek Anda...',
  }
}

/*==================== LANGUAGE SWITCH ====================*/
let currentLang = localStorage.getItem('lang') || 'en'

function applyLanguage(lang) {
  document.querySelectorAll('[data-i18n]').forEach(el => {
    const key = el.getAttribute('data-i18n')
    if (i18n[lang][key] !== undefined) {
      if (i18n[lang][key].includes('<')) {
        el.innerHTML = i18n[lang][key]
      } else {
        el.textContent = i18n[lang][key]
      }
    }
  })
  document.querySelectorAll('[data-i18n-placeholder]').forEach(el => {
    const key = el.getAttribute('data-i18n-placeholder')
    if (i18n[lang][key] !== undefined) {
      el.setAttribute('placeholder', i18n[lang][key])
    }
  })
  document.documentElement.setAttribute('data-lang', lang)
  document.documentElement.setAttribute('lang', lang === 'id' ? 'id' : 'en')
  localStorage.setItem('lang', lang)
  currentLang = lang

  const btn = document.getElementById('lang-toggle')
  if (btn) {
    btn.textContent = lang === 'en' ? 'ID' : 'EN'
    btn.setAttribute('aria-label', lang === 'en' ? 'Switch language to Indonesian' : 'Ganti bahasa ke Inggris')
  }

  // Update meta tags
  const metaDesc = document.querySelector('meta[name="description"]')
  if (metaDesc) {
    metaDesc.setAttribute('content', lang === 'id'
      ? 'Rizky Adi Satria — Full Stack Developer dengan 4+ tahun pengalaman membangun aplikasi web, platform e-commerce, dan proyek freelance.'
      : 'Rizky Adi Satria — Full Stack Developer with 4+ years of experience building web applications, e-commerce platforms, and freelance projects.')
  }
  document.title = lang === 'id'
    ? 'Rizky Adi Satria — Full Stack Developer'
    : 'Rizky Adi Satria — Full Stack Developer'
}

// Apply on load
applyLanguage(currentLang)

const langToggle = document.getElementById('lang-toggle')
if (langToggle) {
  langToggle.addEventListener('click', () => {
    applyLanguage(currentLang === 'en' ? 'id' : 'en')
  })
}

/*==================== SHOW MENU (with aria-expanded) ====================*/
const navToggle = document.getElementById('nav-toggle')
const navMenu   = document.getElementById('nav-menu')

if (navToggle && navMenu) {
  navToggle.addEventListener('click', () => {
    navMenu.classList.toggle('show-menu')
    const expanded = navMenu.classList.contains('show-menu')
    navToggle.setAttribute('aria-expanded', expanded)
    navToggle.setAttribute('aria-label', expanded ? 'Close menu' : 'Open menu')
  })
}

/*==================== CLOSE MENU ON LINK CLICK ====================*/
document.querySelectorAll('.nav__link').forEach(link => {
  link.addEventListener('click', () => {
    document.getElementById('nav-menu').classList.remove('show-menu')
    const tgl = document.getElementById('nav-toggle')
    if (tgl) {
      tgl.setAttribute('aria-expanded', 'false')
      tgl.setAttribute('aria-label', 'Open menu')
    }
  })
})

/*==================== ACTIVE LINK ON SCROLL ====================*/
const sections = document.querySelectorAll('section[id]')

function scrollActive() {
  const scrollY = window.pageYOffset
  const header  = document.getElementById('header')
  const offset  = header ? header.offsetHeight + 16 : 70

  sections.forEach(current => {
    const sectionTop = current.offsetTop - offset
    const sectionEnd = sectionTop + current.offsetHeight
    const id   = current.getAttribute('id')
    const link = document.querySelector(`.nav__menu a[href*="${id}"]`)
    if (!link) return
    link.classList.toggle('active-link', scrollY >= sectionTop && scrollY < sectionEnd)
  })
}
window.addEventListener('scroll', scrollActive)

/*==================== SCROLL TO TOP ====================*/
function handleScrollTop() {
  const btn = document.getElementById('scroll-top')
  if (!btn) return
  btn.classList.toggle('show-scroll', window.scrollY >= 300)
}
window.addEventListener('scroll', handleScrollTop)

/*==================== DARK / LIGHT THEME ====================*/
const themeButton = document.getElementById('theme-button')
const DARK_CLASS  = 'dark-theme'
const SUN_ICON    = 'bx-sun'

const savedTheme = localStorage.getItem('selected-theme')
const savedIcon  = localStorage.getItem('selected-icon')

if (savedTheme) {
  document.body.classList[savedTheme === 'dark' ? 'add' : 'remove'](DARK_CLASS)
  themeButton.classList[savedIcon === 'bx-moon' ? 'add' : 'remove'](SUN_ICON)
}

themeButton.addEventListener('click', () => {
  document.body.classList.toggle(DARK_CLASS)
  themeButton.classList.toggle(SUN_ICON)
  localStorage.setItem('selected-theme', document.body.classList.contains(DARK_CLASS) ? 'dark' : 'light')
  localStorage.setItem('selected-icon',  themeButton.classList.contains(SUN_ICON) ? 'bx-moon' : 'bx-sun')
})

/*==================== GENERATE PDF ====================*/
const pdfOptions = {
  margin:      0,
  filename:    'RizkyAdiSatria_CV.pdf',
  image:       { type: 'jpeg', quality: 0.98 },
  html2canvas: { scale: 2, useCORS: true, logging: false },
  jsPDF:       { unit: 'mm', format: 'a4', orientation: 'portrait' },
  enableLinks: true,
}

function generateCV() {
  const cvPrint = document.getElementById('cv-print')
  cvPrint.style.display = 'block'
  setTimeout(() => {
    html2pdf().set(pdfOptions).from(cvPrint).save().then(() => {
      cvPrint.style.display = 'none'
    })
  }, 300)
}

// Nav PDF button
const resumeButton = document.getElementById('resume-button')
if (resumeButton) resumeButton.addEventListener('click', generateCV)

// Hero Download CV button
const downloadCvBtn = document.getElementById('download-cv-btn')
if (downloadCvBtn) downloadCvBtn.addEventListener('click', generateCV)

/*==================== CONTACT FORM AJAX ====================*/
function initContactForm() {
  const form   = document.getElementById('contact-form')
  const status = document.getElementById('contact-status')
  const btn    = document.getElementById('contact-submit-btn')
  if (!form || !status || !btn) return

  const labels = {
    en: {
      sending:  'Sending...',
      sent:     'Message sent! I\'ll get back to you soon.',
      error:    'Failed to send. Please try email or WhatsApp instead.',
    },
    id: {
      sending:  'Mengirim...',
      sent:     'Pesan terkirim! Saya akan segera merespons.',
      error:    'Gagal mengirim. Silakan coba via email atau WhatsApp.',
    }
  }

  form.addEventListener('submit', async e => {
    e.preventDefault()
    const lang = (document.documentElement.getAttribute('data-lang') === 'id') ? 'id' : 'en'

    status.textContent = labels[lang].sending
    status.className   = 'contact__status'

    try {
      const res = await fetch(form.action, {
        method: 'POST',
        body: new FormData(form),
        headers: { 'Accept': 'application/json' },
      })
      if (res.ok) {
        status.textContent = labels[lang].sent
        status.className   = 'contact__status contact__status--success'
        form.reset()
      } else {
        throw new Error('Formspree error')
      }
    } catch {
      status.textContent = labels[lang].error
      status.className   = 'contact__status contact__status--error'
    }
  })
}

initContactForm()

/*==================== SCROLL REVEAL ====================*/
function initScrollReveal() {
  const observer = new IntersectionObserver(entries => {
    entries.forEach(entry => {
      if (!entry.isIntersecting) return
      entry.target.classList.add('visible')
      observer.unobserve(entry.target)
    })
  }, { threshold: 0.12, rootMargin: '0px 0px -30px 0px' })

  // Section titles
  document.querySelectorAll('.section-title').forEach(el => {
    el.classList.add('reveal')
    observer.observe(el)
  })

  // About description
  document.querySelectorAll('.profile__description').forEach(el => {
    el.classList.add('reveal')
    observer.observe(el)
  })

  // Timeline items: staggered slide from left
  document.querySelectorAll('.timeline__item').forEach((el, i) => {
    el.classList.add('reveal-left')
    el.style.transitionDelay = `${i * 0.1}s`
    observer.observe(el)
  })

  // Service cards: staggered fade-in
  document.querySelectorAll('.service-card').forEach((el, i) => {
    el.classList.add('reveal')
    el.style.transitionDelay = `${i * 0.1}s`
    observer.observe(el)
  })

  // Contact form
  document.querySelectorAll('.contact__form').forEach(el => {
    el.classList.add('reveal')
    observer.observe(el)
  })

  // Skill group titles
  document.querySelectorAll('.skills__group-title').forEach((el, i) => {
    el.classList.add('reveal')
    el.style.transitionDelay = `${i * 0.1}s`
    observer.observe(el)
  })

  // Skill tags: staggered fade-in
  document.querySelectorAll('.skill-tag').forEach((el, i) => {
    el.style.transitionDelay = `${i * 0.06}s`
    observer.observe(el)
  })

  // Project cards: staggered slide up
  document.querySelectorAll('.project-card').forEach((el, i) => {
    el.style.transitionDelay = `${i * 0.08}s`
    observer.observe(el)
  })

  // Social links & language items: staggered
  document.querySelectorAll('.social-link, .lang-item').forEach((el, i) => {
    el.style.transitionDelay = `${i * 0.1}s`
    observer.observe(el)
  })
}

/*==================== COUNTER ANIMATION ====================*/
function animateCount(el) {
  const raw    = el.textContent.trim()
  const target = parseInt(raw, 10)
  const suffix = raw.replace(/[0-9]/g, '')
  if (isNaN(target)) return

  const duration = 1200
  const start    = performance.now()

  ;(function tick(now) {
    const t     = Math.min((now - start) / duration, 1)
    const eased = 1 - Math.pow(1 - t, 3)
    el.textContent = Math.round(eased * target) + suffix
    if (t < 1) requestAnimationFrame(tick)
  })(start)
}

function initCounters() {
  const observer = new IntersectionObserver(entries => {
    entries.forEach(entry => {
      if (!entry.isIntersecting) return
      animateCount(entry.target)
      observer.unobserve(entry.target)
    })
  }, { threshold: 0.2 })

  document.querySelectorAll('.home__stat-num').forEach(el => observer.observe(el))
}

initScrollReveal()
initCounters()

/*==================== HERO LOTTIE ====================*/
;(function () {
  const el = document.getElementById('hero-lottie')
  if (!el || typeof lottie === 'undefined') return

  let anim = null

  function isDark() {
    return document.body.classList.contains('dark-theme')
  }

  function load() {
    if (anim) { anim.destroy(); anim = null }
    anim = lottie.loadAnimation({
      container: el,
      renderer : 'svg',
      loop     : true,
      autoplay : true,
      path     : isDark()
        ? 'assets/lottie/background01-dark.json'
        : 'assets/lottie/background01.json',
      rendererSettings: { preserveAspectRatio: 'xMidYMid slice' },
    })
  }

  load()

  new MutationObserver(() => {
    if (document.body.classList.contains('scale-cv')) {
      anim && anim.pause()
    } else {
      load()
    }
  }).observe(document.body, { attributes: true, attributeFilter: ['class'] })
})()

/*==================== TYPEWRITER LOOP ====================*/
;(function () {
  const el = document.querySelector('.home__profession')
  if (!el) return

  const roles = {
    en: ['Full Stack Developer', 'Laravel Developer', 'React Developer', 'Wordpress Developer', 'MERN Stack Developer'],
    id: ['Full Stack Developer', 'Laravel Developer', 'React Developer', 'Wordpress Developer', 'MERN Stack Developer'],
  }

  const lang = currentLang === 'id' ? 'id' : 'en'
  let roleIdx = 0
  let charIdx = roles[lang][0].length  // start with full text visible, then delete
  let deleting = true
  const typeSpeed = 65
  const deleteSpeed = 35
  const pauseAfterType = 1800
  const pauseAfterDelete = 300

  function tick() {
    const lang = currentLang === 'id' ? 'id' : 'en'
    const role = roles[lang][roleIdx]

    if (!deleting) {
      charIdx++
      el.textContent = role.slice(0, charIdx)
      if (charIdx === role.length) {
        deleting = true
        setTimeout(tick, pauseAfterType)
        return
      }
      setTimeout(tick, typeSpeed)
    } else {
      charIdx--
      el.textContent = role.slice(0, charIdx)
      if (charIdx === 0) {
        deleting = false
        roleIdx = (roleIdx + 1) % roles[lang].length
        setTimeout(tick, pauseAfterDelete)
        return
      }
      setTimeout(tick, deleteSpeed)
    }
  }

  // Start after initial entrance animation
  setTimeout(tick, 1400)
})()

