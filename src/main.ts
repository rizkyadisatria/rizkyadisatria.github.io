// Font & ikon di-bundle sendiri (tanpa CDN pihak ketiga) dan versinya terkunci lewat bun.lock.
import '@fontsource/poppins/latin-400.css'
import '@fontsource/poppins/latin-500.css'
import '@fontsource/poppins/latin-600.css'
import '@fontsource/poppins/latin-700.css'
import 'boxicons/css/boxicons.min.css'
import './app.css'

import { mount } from 'svelte'
import App from './App.svelte'
import { app } from './lib/state.svelte.ts'

// Terapkan tema tersimpan sebelum render pertama agar tidak berkedip terang → gelap.
document.body.classList.toggle('dark-theme', app.dark)

mount(App, { target: document.getElementById('app')! })
