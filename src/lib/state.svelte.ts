import { i18n, type I18nKey, type Lang } from './i18n.data.ts'

// localStorage bisa melempar error (mode privat / storage diblokir), jadi selalu dibungkus.
function safeGet(key: string): string | null {
  try {
    return localStorage.getItem(key)
  } catch {
    return null
  }
}

function safeSet(key: string, value: string): void {
  try {
    localStorage.setItem(key, value)
  } catch {
    /* diabaikan: preferensi tidak tersimpan, aplikasi tetap jalan */
  }
}

class AppState {
  lang = $state<Lang>(safeGet('lang') === 'id' ? 'id' : 'en')
  dark = $state(safeGet('selected-theme') === 'dark')
  menuOpen = $state(false)

  toggleLang(): void {
    this.lang = this.lang === 'en' ? 'id' : 'en'
    safeSet('lang', this.lang)
  }

  toggleTheme(): void {
    this.dark = !this.dark
    safeSet('selected-theme', this.dark ? 'dark' : 'light')
    safeSet('selected-icon', this.dark ? 'bx-moon' : 'bx-sun')
  }
}

export const app = new AppState()

/** Terjemahan reaktif: otomatis ter-render ulang saat bahasa berganti. */
export const t = (key: I18nKey): string => i18n[app.lang][key]

export const META: Record<Lang, { description: string; title: string }> = {
  en: {
    title: 'Rizky Adi Satria — Full Stack Developer',
    description:
      'Rizky Adi Satria — Full Stack Developer with 4+ years of experience building web applications, e-commerce platforms, and freelance projects.',
  },
  id: {
    title: 'Rizky Adi Satria — Full Stack Developer',
    description:
      'Rizky Adi Satria — Full Stack Developer dengan 4+ tahun pengalaman membangun aplikasi web, platform e-commerce, dan proyek freelance.',
  },
}
