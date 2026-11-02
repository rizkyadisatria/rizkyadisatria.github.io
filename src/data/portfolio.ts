import type { I18nKey } from '../lib/i18n.data.ts'

export const CONTACT = {
  email: 'rizkyadsatria@gmail.com',
  phone: '+62 859-3300-5036',
  whatsapp: 'https://wa.me/6285933005036',
  location: 'Mataram, Nusa Tenggara Barat',
} as const

// Endpoint publik Formspree (memang dirancang terlihat di sisi klien).
export const FORM_ENDPOINT = 'https://formspree.io/f/mzdynjbp'

export const NAV: ReadonlyArray<{ id: string; icon: string; key: I18nKey }> = [
  { id: 'home', icon: 'bx-home', key: 'nav.home' },
  { id: 'profile', icon: 'bx-user', key: 'nav.about' },
  { id: 'services', icon: 'bx-bulb', key: 'nav.services' },
  { id: 'experience', icon: 'bx-briefcase', key: 'nav.experience' },
  { id: 'skills', icon: 'bx-code-alt', key: 'nav.skills' },
  { id: 'projects', icon: 'bx-folder', key: 'nav.projects' },
  { id: 'contact', icon: 'bx-envelope', key: 'nav.contact' },
]

export const STATS: ReadonlyArray<{ num: string; key: I18nKey }> = [
  { num: '4+', key: 'stat.years' },
  { num: '8+', key: 'stat.projects' },
  { num: '7', key: 'stat.companies' },
]

export const ROLES = [
  'Full Stack Developer',
  'Laravel Developer',
  'React Developer',
  'Wordpress Developer',
  'MERN Stack Developer',
] as const

export const SERVICES: ReadonlyArray<{ icon: string; title: I18nKey; desc: I18nKey }> = [
  { icon: 'bx-shopping-bag', title: 'svc1.title', desc: 'svc1.desc' },
  { icon: 'bx-buildings', title: 'svc2.title', desc: 'svc2.desc' },
  { icon: 'bx-hotel', title: 'svc3.title', desc: 'svc3.desc' },
  { icon: 'bx-cloud', title: 'svc4.title', desc: 'svc4.desc' },
  { icon: 'bx-wrench', title: 'svc5.title', desc: 'svc5.desc' },
  { icon: 'bxl-wordpress', title: 'svc6.title', desc: 'svc6.desc' },
]

type ExperienceItem = {
  title: I18nKey
  badge: I18nKey
  desc: I18nKey
  company?: { name: string; href: string; label: string }
  companyKey?: I18nKey
}

export const EXPERIENCE: readonly ExperienceItem[] = [
  {
    title: 'exp1.title',
    badge: 'exp.badge1',
    desc: 'exp1.desc',
    company: { name: 'CV. Data Digital Indonesia', href: 'https://dewahoster.co.id', label: 'dewahoster.co.id' },
  },
  {
    title: 'exp2.title',
    badge: 'exp.badge2',
    desc: 'exp2.desc',
    company: { name: 'PT. Bumigora Karya Mandiri', href: 'https://bumigorashop.com', label: 'bumigorashop.com' },
  },
  { title: 'exp3.title', badge: 'exp.badge3', desc: 'exp3.desc', companyKey: 'exp3.company' },
]

export const EDUCATION: ReadonlyArray<{ title: I18nKey; period: string; school: string }> = [
  { title: 'edu1.title', period: '2015 – 2018', school: 'SMK Negeri 6 Mataram' },
  { title: 'edu2.title', period: '2018 – 2022', school: 'Universitas Bumigora' },
]

export const SKILLS: ReadonlyArray<{ icon: string; group: string; tags: readonly string[] }> = [
  { icon: 'bx-laptop', group: 'Frontend', tags: ['HTML & CSS', 'JavaScript', 'React.js', 'Tailwind CSS', 'Inertia.js'] },
  { icon: 'bx-server', group: 'Backend', tags: ['Laravel', 'Node.js', 'Express.js', 'MongoDB', 'MySQL', 'WordPress'] },
  { icon: 'bx-cog', group: 'DevOps & Tools', tags: ['Git & GitHub', 'Linux / VPS', 'Firebase', 'Mikrotik'] },
]

export const PROJECTS: ReadonlyArray<{
  icon: string
  year: string
  title: string
  desc: I18nKey
  tags: readonly string[]
  href: string
}> = [
  { icon: 'bx-code-alt', year: '2021', title: 'Google Clone', desc: 'proj1.desc', tags: ['React.js', 'Tailwind CSS', 'API'], href: 'https://googlesearch-rizkyadisatria.vercel.app/' },
  { icon: 'bx-shopping-bag', year: '2022', title: 'LuxSpace', desc: 'proj2.desc', tags: ['HTML', 'CSS', 'JavaScript'], href: 'https://luxspace-rizkyadisatria.vercel.app/' },
  { icon: 'bx-coffee', year: '2022', title: 'Aroma Bumi', desc: 'proj3.desc', tags: ['HTML', 'CSS', 'JavaScript'], href: 'https://coffee-rizkyadisatria.vercel.app/' },
  { icon: 'bxs-game', year: '2023', title: 'Pokémon Catch', desc: 'proj4.desc', tags: ['HTML', 'CSS', 'JavaScript', 'API'], href: 'https://pokemon-rizkyadisatria.vercel.app/' },
  { icon: 'bx-hotel', year: '2023', title: 'Lina Boutique Villas & Spa', desc: 'proj5.desc', tags: ['WordPress', 'Payment Gateway'], href: 'https://linalombok.com' },
  { icon: 'bx-buildings', year: '2025', title: 'The Lombok Villas', desc: 'proj6.desc', tags: ['WordPress', 'Booking'], href: 'https://thelombokhouses.com' },
  { icon: 'bx-store', year: '2025', title: 'Tunnels', desc: 'proj7.desc', tags: ['React', 'Node.js', 'Express', 'MongoDB'], href: 'https://tunnels-phi.vercel.app/' },
  { icon: 'bx-book-open', year: '2026', title: 'Seqolah', desc: 'proj8.desc', tags: ['Laravel 12', 'React', 'Inertia.js', 'Firebase'], href: 'https://seqolah.com/' },
]

export const SOCIAL = [
  { icon: 'bxl-github', label: 'rizkyadisatria', href: 'https://github.com/rizkyadisatria' },
  { icon: 'bxl-instagram', label: '@rzkydst', href: 'https://instagram.com/rzkydst' },
] as const

export const LANGUAGES: ReadonlyArray<{ key: I18nKey }> = [{ key: 'lang.id' }, { key: 'lang.en' }]
