<script lang="ts">
  // Panel kontrol Zond. Disajikan oleh ESP32-S3 (otak) di jaringan WiFi yang sama.
  // Semua perintah berupa GET ke /api/... di S3; gambar kamera diambil langsung dari ESP32-CAM.
  import { onMount } from 'svelte'

  type Status = {
    nama: string
    hidup: boolean
    baterai: number
    depan: number
    belakang: number
    flag: number
    vol: number
    status: string
    kamera: string
    mode: string
  }

  let s = $state<Status | null>(null)
  let tersambung = $state(false)
  let kecepatan = $state(160)
  let arahAktif = $state<string | null>(null)
  let toleh = $state(0) // -60 kiri .. 60 kanan
  let angguk = $state(90)
  let volume = $state(12)
  let ubahVolume = false
  let kalimat = $state('')
  let kameraNyala = $state(true)
  let kameraGagal = $state(false)
  let srcKamera = $state('')

  async function api(jalur: string, batasMs = 1500): Promise<Response | null> {
    const ac = new AbortController()
    const t = setTimeout(() => ac.abort(), batasMs)
    try {
      return await fetch(jalur, { signal: ac.signal, cache: 'no-store' })
    } catch {
      return null
    } finally {
      clearTimeout(t)
    }
  }

  async function ambilStatus() {
    const r = await api('/api/status')
    if (!r || !r.ok) {
      tersambung = false
      return
    }
    tersambung = true
    s = (await r.json()) as Status
    if (!ubahVolume) volume = s.vol
  }

  // ---------- gerak: tahan = jalan, lepas = berhenti ----------
  // Selama tombol ditekan, perintah gerak 350 ms dikirim ulang tiap 150 ms. Kalau jari diangkat,
  // layar HP mati, atau WiFi putus, kiriman berhenti dan badan berhenti sendiri paling lambat 0,35 detik.
  const ARAH: Record<string, [number, number]> = {
    maju: [1, 1],
    mundur: [-1, -1],
    kiri: [-1, 1],
    kanan: [1, -1],
    majukiri: [0.35, 1],
    majukanan: [1, 0.35],
    mundurkiri: [-0.35, -1],
    mundurkanan: [-1, -0.35],
  }
  let pengulang: ReturnType<typeof setInterval> | null = null
  let sedangKirim = false

  function kirimGerak() {
    if (!arahAktif || sedangKirim) return
    const [a, b] = ARAH[arahAktif]
    sedangKirim = true
    api(`/api/gerak?l=${Math.round(a * kecepatan)}&r=${Math.round(b * kecepatan)}`, 600).finally(() => (sedangKirim = false))
  }

  function mulai(arah: string) {
    arahAktif = arah
    kirimGerak()
    if (pengulang) clearInterval(pengulang)
    pengulang = setInterval(kirimGerak, 150)
  }

  function berhenti() {
    if (pengulang) clearInterval(pengulang)
    pengulang = null
    if (arahAktif) api('/api/stop')
    arahAktif = null
  }

  function tekan(e: PointerEvent, arah: string) {
    ;(e.currentTarget as HTMLElement).setPointerCapture(e.pointerId)
    mulai(arah)
  }

  const TOMBOL_KEY: Record<string, string> = {
    ArrowUp: 'maju', w: 'maju', ArrowDown: 'mundur', s: 'mundur', ArrowLeft: 'kiri', a: 'kiri', ArrowRight: 'kanan', d: 'kanan',
  }
  function tombolTurun(e: KeyboardEvent) {
    if ((e.target as HTMLElement).tagName === 'INPUT') return
    if (e.key === ' ') {
      e.preventDefault()
      berhenti()
      api('/api/stop')
      return
    }
    const arah = TOMBOL_KEY[e.key]
    if (arah && !e.repeat) {
      e.preventDefault()
      mulai(arah)
    }
  }
  function tombolNaik(e: KeyboardEvent) {
    if (TOMBOL_KEY[e.key]) berhenti()
  }

  // ---------- kepala ----------
  let tKepala = 0
  let kepalaTertunda: ReturnType<typeof setTimeout> | null = null
  function kirimKepala() {
    const now = Date.now()
    if (kepalaTertunda) clearTimeout(kepalaTertunda)
    const kirim = () => {
      tKepala = Date.now()
      api(`/api/kepala?p=${90 - toleh}&t=${angguk}`)
    }
    if (now - tKepala > 150) kirim()
    else kepalaTertunda = setTimeout(kirim, 150)
  }
  function kepalaLurus() {
    toleh = 0
    angguk = 90
    kirimKepala()
  }

  // ---------- suara ----------
  async function aturVolume() {
    ubahVolume = true
    await api(`/api/volume?v=${volume}`)
    setTimeout(() => (ubahVolume = false), 1500)
  }
  async function bilang(e: SubmitEvent) {
    e.preventDefault()
    const t = kalimat.trim()
    if (!t) return
    kalimat = ''
    await api(`/api/bilang?t=${encodeURIComponent(t)}`, 3000)
  }

  // ---------- kamera: ambil foto berulang-ulang (lebih ringan dari video untuk ESP32-CAM) ----------
  function fotoBerikut(jeda: number) {
    setTimeout(() => {
      if (!kameraNyala || !s?.kamera) {
        if (kameraNyala) fotoBerikut(1000)
        return
      }
      srcKamera = `http://${s.kamera}/capture?res=vga&t=${Date.now()}`
    }, jeda)
  }

  function teksFlag(f: number): string[] {
    const t: string[] = []
    if (f & 1) t.push('Tombol STOP terkunci')
    if (f & 2) t.push('Baterai lemah')
    if (f & 4) t.push('Halangan depan')
    if (f & 8) t.push('Halangan belakang')
    if (f & 16) t.push('Robot miring')
    return t
  }

  onMount(() => {
    ambilStatus()
    const id = setInterval(ambilStatus, 1000)
    const henti = () => berhenti()
    const sembunyi = () => document.hidden && berhenti()
    window.addEventListener('blur', henti)
    document.addEventListener('visibilitychange', sembunyi)
    return () => {
      clearInterval(id)
      window.removeEventListener('blur', henti)
      document.removeEventListener('visibilitychange', sembunyi)
      berhenti()
    }
  })

  $effect(() => {
    if (kameraNyala) fotoBerikut(0)
  })
</script>

<svelte:window onkeydown={tombolTurun} onkeyup={tombolNaik} />

<main>
  <header>
    <h1>{s?.nama ?? 'Robot'}</h1>
    <span class="titik" class:on={tersambung && s?.hidup}></span>
    <span class="kecil">
      {#if !tersambung}otak tidak terhubung{:else if !s?.hidup}badan tidak menjawab{:else}{s.baterai.toFixed(1)} V · depan {s.depan} cm · belakang {s.belakang} cm{/if}
    </span>
  </header>

  {#if s && s.flag}
    <div class="peringatan">
      {#each teksFlag(s.flag) as t (t)}<span>{t}</span>{/each}
    </div>
  {/if}

  <section class="kamera">
    {#if kameraNyala && s?.kamera}
      <img
        src={srcKamera}
        alt=""
        class:redup={kameraGagal}
        onload={() => {
          kameraGagal = false
          fotoBerikut(100)
        }}
        onerror={() => {
          kameraGagal = true
          fotoBerikut(1500)
        }}
      />
      {#if kameraGagal}<p class="ket">Kamera tidak terhubung ({s.kamera})</p>{/if}
    {:else}
      <p class="ket">{s?.kamera ? 'Kamera dimatikan' : 'Alamat kamera belum diketahui'}</p>
    {/if}
    <button class="kecil-btn" onclick={() => (kameraNyala = !kameraNyala)}>{kameraNyala ? 'Matikan kamera' : 'Nyalakan kamera'}</button>
  </section>

  <section>
    <h2>Gerak <small>tahan untuk jalan, lepas untuk berhenti</small></h2>
    <div class="dpad" oncontextmenu={(e) => e.preventDefault()}>
      {#each [['majukiri', '↖'], ['maju', '▲'], ['majukanan', '↗'], ['kiri', '◀'], ['stop', '■'], ['kanan', '▶'], ['mundurkiri', '↙'], ['mundur', '▼'], ['mundurkanan', '↘']] as [arah, ikon] (arah)}
        {#if arah === 'stop'}
          <button class="stop" onpointerdown={() => { berhenti(); api('/api/stop') }}>{ikon}</button>
        {:else}
          <button
            class:aktif={arahAktif === arah}
            onpointerdown={(e) => tekan(e, arah)}
            onpointerup={berhenti}
            onpointercancel={berhenti}
            onlostpointercapture={berhenti}
          >{ikon}</button>
        {/if}
      {/each}
    </div>
    <label>Kecepatan {kecepatan}<input type="range" min="100" max="255" bind:value={kecepatan} /></label>
  </section>

  <section>
    <h2>Kepala</h2>
    <label>Toleh {toleh < 0 ? 'kiri' : toleh > 0 ? 'kanan' : 'lurus'}<input type="range" min="-60" max="60" bind:value={toleh} oninput={kirimKepala} /></label>
    <label>Angguk {angguk}°<input type="range" min="40" max="150" bind:value={angguk} oninput={kirimKepala} /></label>
    <button class="kecil-btn" onclick={kepalaLurus}>Lurus ke depan</button>
  </section>

  <section>
    <h2>Musik dan suara</h2>
    <div class="baris">
      <button onclick={() => api('/api/lagu', 4000)}>Lagu berikutnya</button>
      <button onclick={() => api('/api/radio', 4000)}>Radio berikutnya</button>
      <button onclick={() => api('/api/diam')}>Diam</button>
    </div>
    <label>Volume {volume}<input type="range" min="0" max="21" bind:value={volume} onchange={aturVolume} /></label>
    <form onsubmit={bilang}>
      <input type="text" placeholder="Suruh Zond bilang..." bind:value={kalimat} maxlength="140" />
      <button type="submit">Bilang</button>
    </form>
  </section>

  {#if s?.status}<p class="status">{s.status}</p>{/if}
</main>

<style>
  :global(html) {
    background: #10141a;
    color: #e6e9ee;
    font-family: system-ui, sans-serif;
  }
  :global(body) {
    margin: 0;
  }
  main {
    max-width: 480px;
    margin: 0 auto;
    padding: 12px 16px 32px;
  }
  header {
    display: flex;
    align-items: center;
    gap: 8px;
    flex-wrap: wrap;
  }
  h1 {
    font-size: 1.4rem;
    margin: 0;
  }
  h2 {
    font-size: 1rem;
    margin: 0 0 8px;
  }
  h2 small {
    font-weight: normal;
    color: #8a94a3;
  }
  .kecil {
    color: #8a94a3;
    font-size: 0.85rem;
  }
  .titik {
    width: 10px;
    height: 10px;
    border-radius: 50%;
    background: #c0392b;
  }
  .titik.on {
    background: #2ecc71;
  }
  .peringatan {
    display: flex;
    flex-wrap: wrap;
    gap: 6px;
    margin-top: 8px;
  }
  .peringatan span {
    background: #7a1d1d;
    padding: 3px 8px;
    border-radius: 6px;
    font-size: 0.85rem;
  }
  section {
    background: #18202a;
    border-radius: 12px;
    padding: 12px;
    margin-top: 12px;
  }
  .kamera {
    text-align: center;
  }
  .kamera img {
    width: 100%;
    border-radius: 8px;
    display: block;
    background: #000;
    aspect-ratio: 4 / 3;
    object-fit: cover;
  }
  .kamera img.redup {
    opacity: 0.15;
  }
  .ket {
    color: #8a94a3;
    margin: 8px 0;
  }
  .dpad {
    display: grid;
    grid-template-columns: repeat(3, 1fr);
    gap: 8px;
    max-width: 300px;
    margin: 0 auto 12px;
    touch-action: none;
    user-select: none;
    -webkit-user-select: none;
  }
  .dpad button {
    aspect-ratio: 1;
    font-size: 1.6rem;
    border-radius: 14px;
    background: #2a3a4d;
    touch-action: none;
  }
  .dpad button.aktif {
    background: #3d7bd9;
  }
  .dpad button.stop {
    background: #a0302a;
  }
  button {
    border: 0;
    color: inherit;
    background: #2a3a4d;
    padding: 10px 12px;
    border-radius: 10px;
    font-size: 0.95rem;
    cursor: pointer;
  }
  button:active {
    filter: brightness(1.3);
  }
  .kecil-btn {
    margin-top: 8px;
    font-size: 0.85rem;
    padding: 6px 10px;
  }
  .baris {
    display: flex;
    gap: 8px;
    flex-wrap: wrap;
  }
  .baris button {
    flex: 1;
  }
  label {
    display: block;
    margin-top: 10px;
    font-size: 0.9rem;
  }
  input[type='range'] {
    width: 100%;
  }
  form {
    display: flex;
    gap: 8px;
    margin-top: 12px;
  }
  input[type='text'] {
    flex: 1;
    min-width: 0;
    padding: 10px;
    border-radius: 10px;
    border: 1px solid #2a3a4d;
    background: #10141a;
    color: inherit;
    font-size: 1rem;
  }
  .status {
    text-align: center;
    color: #8a94a3;
    margin-top: 12px;
  }
</style>
