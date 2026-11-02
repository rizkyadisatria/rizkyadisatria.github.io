<script lang="ts">
  import { FORM_ENDPOINT } from '../data/portfolio.ts'
  import { reveal } from '../lib/actions.ts'
  import { app, t } from '../lib/state.svelte.ts'

  const LABELS = {
    en: {
      sending: 'Sending...',
      sent: "Message sent! I'll get back to you soon.",
      error: 'Failed to send. Please try email or WhatsApp instead.',
    },
    id: {
      sending: 'Mengirim...',
      sent: 'Pesan terkirim! Saya akan segera merespons.',
      error: 'Gagal mengirim. Silakan coba via email atau WhatsApp.',
    },
  } as const

  let status = $state<'idle' | 'sending' | 'success' | 'error'>('idle')

  const statusText = $derived(status === 'idle' ? '' : LABELS[app.lang][status === 'sending' ? 'sending' : status === 'success' ? 'sent' : 'error'])

  async function submit(event: SubmitEvent): Promise<void> {
    event.preventDefault()
    const form = event.currentTarget as HTMLFormElement
    status = 'sending'
    try {
      const res = await fetch(FORM_ENDPOINT, {
        method: 'POST',
        body: new FormData(form),
        headers: { Accept: 'application/json' },
      })
      if (!res.ok) throw new Error('Formspree error')
      status = 'success'
      form.reset()
    } catch {
      status = 'error'
    }
  }
</script>

<section class="contact section" id="contact">
  <div class="bd-container">
    <h2 class="section-title" use:reveal={{ cls: 'reveal' }}>{t('section.contact')}</h2>
    <p class="contact__intro">{t('contact.intro')}</p>
    <form
      class="contact__form"
      id="contact-form"
      action={FORM_ENDPOINT}
      method="POST"
      onsubmit={submit}
      use:reveal={{ cls: 'reveal' }}
    >
      <div class="contact__row">
        <div class="contact__field">
          <label for="name" class="contact__label">{t('contact.name')}</label>
          <input
            type="text"
            name="name"
            id="name"
            class="contact__input"
            required
            maxlength="100"
            placeholder={t('contact.namePlaceholder')}
          />
        </div>
        <div class="contact__field">
          <label for="email" class="contact__label">{t('contact.email')}</label>
          <input
            type="email"
            name="email"
            id="email"
            class="contact__input"
            required
            maxlength="254"
            placeholder={t('contact.emailPlaceholder')}
          />
        </div>
      </div>
      <div class="contact__field">
        <label for="message" class="contact__label">{t('contact.message')}</label>
        <textarea
          name="message"
          id="message"
          class="contact__input contact__textarea"
          rows="5"
          required
          maxlength="5000"
          placeholder={t('contact.messagePlaceholder')}
        ></textarea>
      </div>
      <!-- Honeypot anti-spam (Formspree menolak kiriman yang field ini terisi) -->
      <input class="contact__hp" type="text" name="_gotcha" tabindex="-1" autocomplete="off" aria-hidden="true" />
      <button type="submit" class="btn btn--primary contact__submit" id="contact-submit-btn" disabled={status === 'sending'}>
        <i class="bx bx-send"></i>
        <span>{t('contact.send')}</span>
      </button>
      <p
        class="contact__status"
        class:contact__status--success={status === 'success'}
        class:contact__status--error={status === 'error'}
        id="contact-status"
        aria-live="polite"
      >
        {statusText}
      </p>
      <p class="contact__note">{@html t('contact.note')}</p>
    </form>
  </div>
</section>
