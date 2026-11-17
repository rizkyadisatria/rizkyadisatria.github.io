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
    ai: string
    aiTerakhir: string
    aiTersedia: Record<string, boolean>
    dengarTerus: boolean
    mendengar: boolean
    fiturKamera: boolean
    skorKamu: number
    skorZond: number
    cuaca: string
    jam: string
    tanggal: string
    jelajahSisa: number
    bateraiPersen: number
    sisaMenit: number
    suara: string
    suaraSiap: boolean
    suaraNama: string
    suaraHariIni: number
    suaraGalat: number
  }

  // ---------- jelajah sendiri ----------
  let jelajahMenit = $state(5)
  async function aturJelajah(menit: number) {
    await api(`/api/jelajah?menit=${menit}`)
    await ambilStatus()
  }
  function mmss(detik: number): string {
    const d = Math.max(0, detik)
    return `${Math.floor(d / 60)}:${String(d % 60).padStart(2, '0')}`
  }

  // ---------- baterai: riwayat tegangan per menit (6 jam terakhir) ----------
  let riwayat = $state<number[]>([]) // volt x 100, paling lama dulu
  async function ambilBaterai() {
    const r = await api('/api/baterai')
    if (r && r.ok) riwayat = ((await r.json()) as { v: number[] }).v
  }
  const V_BAWAH = 6.4
  const V_ATAS = 8.4
  const LEBAR = 320
  const TINGGI = 120
  function yVolt(v: number): number {
    const p = (Math.min(V_ATAS, Math.max(V_BAWAH, v)) - V_BAWAH) / (V_ATAS - V_BAWAH)
    return TINGGI - 4 - p * (TINGGI - 8)
  }
  const garisBaterai = $derived.by(() => {
    const n = riwayat.length
    if (n < 2) return ''
    return riwayat.map((v, i) => `${i ? 'L' : 'M'}${((i / (n - 1)) * LEBAR).toFixed(1)},${yVolt(v / 100).toFixed(1)}`).join('')
  })
  function durasi(menit: number): string {
    if (menit < 60) return `${menit} menit`
    return `${Math.floor(menit / 60)} jam${menit % 60 ? ` ${menit % 60} menit` : ''}`
  }

  // ---------- suara bicara ----------
  async function pilihSuara(e: Event) {
    await api(`/api/suara?pakai=${(e.currentTarget as HTMLSelectElement).value}`)
    await ambilStatus()
  }

  // ---------- belok presisi (giroskop di badan) ----------
  function belok(derajat: number) {
    api(`/api/belok?d=${derajat}`)
  }

  // ---------- alarm dan pengingat ----------
  type Alarm = { jam: string; hari: number; bunyi: number; pesan: string; kapan: string }
  const NAMA_HARI = ['Min', 'Sen', 'Sel', 'Rab', 'Kam', 'Jum', 'Sab']
  const BUNYI = ['suara', 'radio', 'lagu SD']
  let alarm = $state<Alarm[]>([])
  let alarmJam = $state('06:00')
  let alarmUlang = $state('0') // '0' sekali, '127' tiap hari, '62' Sen-Jum, '65' Sab-Min, 'pilih'
  let alarmHari = $state<boolean[]>([false, true, true, true, true, true, false])
  let alarmPesan = $state('')
  let alarmBunyi = $state(0)
  let alarmPesanGagal = $state('')

  async function ambilAlarm() {
    const r = await api('/api/alarm')
    if (r && r.ok) alarm = ((await r.json()) as { alarm: Alarm[] }).alarm
  }
  async function tambahAlarm(e: SubmitEvent) {
    e.preventDefault()
    const hari =
      alarmUlang === 'pilih' ? alarmHari.reduce((m, on, i) => (on ? m | (1 << i) : m), 0) : Number(alarmUlang)
    const q = new URLSearchParams({ jam: alarmJam, hari: String(hari), pesan: alarmPesan.trim(), bunyi: String(alarmBunyi) })
    const r = await api(`/api/alarm/tambah?${q}`)
    alarmPesanGagal = !r ? 'Robot tidak menjawab' : r.ok ? '' : await r.text()
    if (r?.ok) alarmPesan = ''
    await ambilAlarm()
  }
  async function hapusAlarm(i: number) {
    await api(`/api/alarm/hapus?i=${i}`)
    await ambilAlarm()
  }

  // ---------- fitur kamera AI: lihat sekeliling dan suit ----------
  let kameraAIPesan = $state('')
  async function aturFiturKamera(e: Event) {
    const nyala = (e.currentTarget as HTMLInputElement).checked
    await api(`/api/fiturkamera?on=${nyala ? 1 : 0}`)
    await ambilStatus()
  }
  async function jalankanKameraAI(jalur: string, pesan: string) {
    const r = await api(jalur)
    kameraAIPesan = !r ? 'Robot tidak menjawab' : r.ok ? pesan : await r.text()
    setTimeout(() => (kameraAIPesan = ''), 6000)
  }

  // ---------- bicara: robot merekam satu kalimat lewat mikrofonnya sendiri ----------
  let mintaBicara = $state(false)
  async function bicara() {
    mintaBicara = true
    await api('/api/bicara')
    setTimeout(() => (mintaBicara = false), 1500)
  }
  async function aturDengar(e: Event) {
    const nyala = (e.currentTarget as HTMLInputElement).checked
    await api(`/api/dengar?on=${nyala ? 1 : 0}`)
    await ambilStatus()
  }

  const PILIHAN_AI: { id: string; nama: string; ket: string }[] = [
    { id: 'otomatis', nama: 'Otomatis', ket: 'gratis: Gemini, lalu Gemma, lalu Groq' },
    { id: 'gemini', nama: 'Gemini', ket: 'gratis, jatah harian kecil' },
    { id: 'gemma', nama: 'Gemma', ket: 'gratis, jatah besar' },
    { id: 'groq', nama: 'Groq', ket: 'gratis, cepat' },
    { id: 'haiku', nama: 'Claude Haiku', ket: 'berbayar per pemakaian' },
  ]
  let aiMenunggu = $state('')
  let aiPesan = $state('')

  async function pilihAI(id: string) {
    aiMenunggu = id
    aiPesan = ''
    const r = await api(`/api/ai?m=${id}`)
    if (!r) aiPesan = 'Robot tidak menjawab'
    else if (!r.ok) aiPesan = await r.text()
    await ambilStatus()
    aiMenunggu = ''
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
    ambilAlarm()
    ambilBaterai()
    const id = setInterval(ambilStatus, 1000)
    const idAlarm = setInterval(ambilAlarm, 30000) // alarm sekali hilang sendiri setelah berbunyi
    const idBaterai = setInterval(ambilBaterai, 60000)
    const henti = () => berhenti()
    const sembunyi = () => document.hidden && berhenti()
    window.addEventListener('blur', henti)
    document.addEventListener('visibilitychange', sembunyi)
    return () => {
      clearInterval(id)
      clearInterval(idAlarm)
      clearInterval(idBaterai)
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
      {#if !tersambung}otak tidak terhubung{:else if !s?.hidup}badan tidak menjawab{:else}{s.bateraiPersen >= 0 ? `${s.bateraiPersen}% · ` : ''}{s.baterai.toFixed(1)} V · depan {s.depan} cm · belakang {s.belakang} cm{/if}
    </span>
  </header>
  {#if s?.jam}<p class="waktu">{s.jam} · {s.tanggal}{#if s.cuaca}<br />{s.cuaca}{/if}</p>{/if}

  {#if s && s.flag}
    <div class="peringatan">
      {#each teksFlag(s.flag) as t (t)}<span>{t}</span>{/each}
    </div>
  {/if}

  <section class="bicara">
    <button class="mik" class:dengar={s?.mendengar || mintaBicara} onclick={bicara} disabled={!tersambung}>
      {s?.mendengar ? 'Mendengarkan... bicara sekarang' : 'Bicara ke ' + (s?.nama ?? 'robot')}
    </button>
    <p class="ket kecil-ket">Ketuk, lalu bicara ke mikrofon robot. Robot berhenti merekam saat kamu diam (paling lama 6 detik).</p>
    <label class="saklar">
      <input type="checkbox" checked={s?.dengarTerus ?? false} onchange={aturDengar} disabled={!tersambung} />
      Dengar terus (cukup sebut "{s?.nama ?? 'nama robot'}"). Boros: setiap suara di ruangan memakai jatah suara-ke-teks.
    </label>
  </section>

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
    <div class="baris belok">
      <button onclick={() => belok(-90)}>⟲ 90° kiri</button>
      <button onclick={() => belok(180)}>↻ balik</button>
      <button onclick={() => belok(90)}>90° kanan ⟳</button>
    </div>
    <p class="ket kecil-ket">Belok tepat memakai giroskop di badan; sudutnya tidak berubah walau baterai melemah.</p>
    <div class="baris jelajah">
      {#if s?.mode === 'jelajah'}
        <button class="aktif-jelajah" onclick={() => aturJelajah(0)}>Berhenti menjelajah (sisa {mmss(s.jelajahSisa)})</button>
      {:else}
        <select bind:value={jelajahMenit} aria-label="Lama jelajah">
          <option value={5}>5 menit</option>
          <option value={10}>10 menit</option>
          <option value={15}>15 menit</option>
          <option value={30}>30 menit</option>
        </select>
        <button disabled={!tersambung || !s?.hidup} onclick={() => aturJelajah(jelajahMenit)}>Jelajah sendiri</button>
      {/if}
    </div>
    <p class="ket kecil-ket">Maju sampai ada halangan, lalu belok ke arah acak. Berhenti sendiri kalau waktunya habis, baterai tinggal 10%, atau tombol STOP ditekan. Tombol arah juga menghentikannya.</p>
  </section>

  <section>
    <h2>Baterai {#if s && s.bateraiPersen >= 0}<small>{s.bateraiPersen}% · {s.baterai.toFixed(2)} V</small>{/if}</h2>
    {#if riwayat.length >= 2}
      <svg class="grafik" viewBox="0 0 {LEBAR} {TINGGI}" role="img" aria-label="Tegangan baterai {durasi(riwayat.length)} terakhir">
        {#each [8.4, 7.8, 7.2, 6.6] as v (v)}
          <line x1="0" x2={LEBAR} y1={yVolt(v)} y2={yVolt(v)} />
          <text x="2" y={v === V_ATAS ? yVolt(v) + 11 : yVolt(v) - 3}>{v.toFixed(1)} V</text>
        {/each}
        <path d={garisBaterai} />
      </svg>
      <div class="sumbu"><span>{durasi(riwayat.length)} lalu</span><span>sekarang</span></div>
    {:else}
      <p class="ket">Grafik muncul setelah robot menyala 2 menit (satu titik per menit, paling lama 6 jam).</p>
    {/if}
    <p class="ket kecil-ket">
      {#if s && s.sisaMenit >= 0}Perkiraan sisa {durasi(s.sisaMenit)}, kalau pemakaiannya seperti setengah jam terakhir.{:else}Perkiraan sisa waktu muncul setelah sekitar 10 menit menyala.{/if}
      Peringatan dikirim ke Telegram saat tinggal 20% dan 10%.
    </p>
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
    <label class="suara">Suara bicara
      <select value={s?.suara ?? 'gemini'} onchange={pilihSuara} disabled={!tersambung}>
        <option value="gemini" disabled={!s?.suaraSiap}>Cowok ({s?.suaraNama || 'Gemini'})</option>
        <option value="google">Google</option>
      </select>
    </label>
    <p class="ket kecil-ket">
      {#if !s?.suaraSiap}Suara cowok belum aktif: isi SUARA_GEMINI dan GEMINI_API_KEY di kode.{:else}Suara Gemini hari ini {s.suaraHariIni} kalimat (jatah gratis harian).{#if s.suaraGalat} Galat terakhir: kode {s.suaraGalat}, sementara memakai suara Google.{/if}{/if}
    </p>
    <form onsubmit={bilang}>
      <input type="text" placeholder="Suruh Zond bilang..." bind:value={kalimat} maxlength="140" />
      <button type="submit">Bilang</button>
    </form>
  </section>

  <section>
    <h2>Alarm dan pengingat</h2>
    {#if alarm.length}
      <ul class="alarm">
        {#each alarm as a, i (i)}
          <li>
            <b>{a.jam}</b>
            <span>{a.pesan}<small>{a.kapan} · {BUNYI[a.bunyi] ?? 'suara'}</small></span>
            <button class="hapus" aria-label="Hapus alarm {a.jam}" onclick={() => hapusAlarm(i)}>✕</button>
          </li>
        {/each}
      </ul>
    {:else}
      <p class="ket">Belum ada alarm. Bisa juga lewat suara: "{s?.nama ?? 'Zond'}, ingatkan aku jam 5 sore minum obat".</p>
    {/if}
    <form class="form-alarm" onsubmit={tambahAlarm}>
      <div class="baris">
        <input type="time" bind:value={alarmJam} required />
        <select bind:value={alarmUlang}>
          <option value="0">Sekali</option>
          <option value="127">Setiap hari</option>
          <option value="62">Senin-Jumat</option>
          <option value="65">Sabtu-Minggu</option>
          <option value="pilih">Pilih hari</option>
        </select>
      </div>
      {#if alarmUlang === 'pilih'}
        <div class="hari">
          {#each NAMA_HARI as h, i (h)}
            <label><input type="checkbox" bind:checked={alarmHari[i]} />{h}</label>
          {/each}
        </div>
      {/if}
      <input type="text" placeholder="Pesan, misalnya: bangun, minum obat" maxlength="79" bind:value={alarmPesan} />
      <div class="baris">
        <select bind:value={alarmBunyi}>
          <option value={0}>Suara saja</option>
          <option value={1}>Suara + radio</option>
          <option value={2}>Suara + lagu SD</option>
        </select>
        <button type="submit">Pasang</button>
      </div>
      {#if alarmPesanGagal}<p class="ket">{alarmPesanGagal}</p>{/if}
    </form>
    <p class="ket kecil-ket">Alarm sebelum jam 11 siang sekalian menyebut cuaca hari ini. Pengingat juga dikirim ke Telegram.</p>
  </section>

  <section>
    <h2>Fitur kamera AI</h2>
    <label class="saklar">
      <input type="checkbox" checked={s?.fiturKamera ?? false} onchange={aturFiturKamera} disabled={!tersambung} />
      Aktifkan "lihat sekeliling" dan "suit". Tiap kali dipakai, foto dikirim ke AI dan memakai jatah AI lebih cepat.
    </label>
    <div class="baris">
      <button disabled={!s?.fiturKamera} onclick={() => jalankanKameraAI('/api/sekeliling', 'Zond menoleh dan memotret. Ceritanya dikirim ke Telegram.')}>Lihat sekeliling</button>
      <button disabled={!s?.fiturKamera} onclick={() => jalankanKameraAI('/api/suit', 'Siapkan tanganmu di depan kamera!')}>Main suit</button>
    </div>
    {#if s?.fiturKamera && (s.skorKamu || s.skorZond)}<p class="ket">Skor suit: kamu {s.skorKamu}, {s.nama} {s.skorZond}</p>{/if}
    {#if kameraAIPesan}<p class="ket">{kameraAIPesan}</p>{/if}
  </section>

  <section>
    <h2>Otak AI {#if s?.aiTerakhir}<small>terakhir menjawab: {s.aiTerakhir}</small>{/if}</h2>
    <div class="ai">
      {#each PILIHAN_AI as p (p.id)}
        <button
          class:pilih={s?.ai === p.id}
          disabled={!s?.aiTersedia?.[p.id] || aiMenunggu !== ''}
          onclick={() => pilihAI(p.id)}
        >
          <b>{p.nama}</b>
          <span>{s?.aiTersedia?.[p.id] === false ? 'API key belum diisi' : p.ket}</span>
        </button>
      {/each}
    </div>
    {#if aiPesan}<p class="ket">{aiPesan}</p>{/if}
    <p class="ket kecil-ket">Kalau pilihanmu gagal (jatah habis atau saldo kosong), robot otomatis memakai yang gratis.</p>
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
  .mik {
    width: 100%;
    padding: 16px;
    font-size: 1.1rem;
    font-weight: 600;
    background: #1f6f3a;
    border-radius: 14px;
  }
  .mik.dengar {
    background: #c2410c;
    animation: denyut 1s ease-in-out infinite;
  }
  @keyframes denyut {
    50% {
      filter: brightness(1.3);
    }
  }
  .mik:disabled {
    opacity: 0.5;
  }
  .saklar {
    display: flex;
    gap: 8px;
    align-items: flex-start;
    color: #8a94a3;
    font-size: 0.8rem;
  }
  .saklar input {
    margin-top: 2px;
  }
  .ai {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 8px;
  }
  .ai button {
    text-align: left;
    display: flex;
    flex-direction: column;
    gap: 2px;
  }
  .ai button:first-child {
    grid-column: 1 / -1;
  }
  .ai button span {
    font-size: 0.75rem;
    color: #8a94a3;
  }
  .ai button.pilih {
    background: #2f6a3a;
    outline: 2px solid #5fcf72;
  }
  .ai button.pilih span {
    color: #cfe9d4;
  }
  .ai button:disabled {
    opacity: 0.45;
    cursor: default;
  }
  .kecil-ket {
    font-size: 0.8rem;
    margin-bottom: 0;
  }
  .waktu {
    margin: 6px 0 0;
    color: #8a94a3;
    font-size: 0.85rem;
  }
  .belok {
    margin-top: 10px;
  }
  .alarm {
    list-style: none;
    padding: 0;
    margin: 0 0 8px;
  }
  .alarm li {
    display: flex;
    align-items: center;
    gap: 10px;
    padding: 8px 0;
    border-bottom: 1px solid #243041;
  }
  .alarm li b {
    font-size: 1.1rem;
  }
  .alarm li span {
    flex: 1;
    display: flex;
    flex-direction: column;
  }
  .alarm li small {
    color: #8a94a3;
    font-size: 0.75rem;
  }
  .hapus {
    padding: 4px 10px;
    background: #3a2430;
  }
  .form-alarm {
    flex-direction: column;
    gap: 8px;
  }
  .form-alarm .baris > * {
    flex: 1;
  }
  input[type='time'],
  select {
    padding: 9px;
    border-radius: 10px;
    border: 1px solid #2a3a4d;
    background: #10141a;
    color: inherit;
    font-size: 0.95rem;
    min-width: 0;
  }
  .hari {
    display: flex;
    flex-wrap: wrap;
    gap: 6px 12px;
    font-size: 0.85rem;
  }
  .hari label {
    margin: 0;
    display: flex;
    gap: 4px;
    align-items: center;
  }
  .jelajah {
    margin-top: 12px;
  }
  .jelajah select {
    flex: 0 0 auto;
  }
  .aktif-jelajah {
    background: #3d7bd9;
  }
  .grafik {
    width: 100%;
    height: auto;
    display: block;
    background: #10141a;
    border-radius: 8px;
  }
  .grafik line {
    stroke: #243041;
    stroke-width: 1;
  }
  .grafik text {
    fill: #8a94a3;
    font-size: 9px;
  }
  .grafik path {
    fill: none;
    stroke: #5fcf72;
    stroke-width: 2;
    stroke-linejoin: round;
  }
  .sumbu {
    display: flex;
    justify-content: space-between;
    color: #8a94a3;
    font-size: 0.75rem;
    margin-top: 4px;
  }
  .suara {
    display: flex;
    align-items: center;
    gap: 8px;
  }
  .suara select {
    flex: 1;
  }
  .status {
    text-align: center;
    color: #8a94a3;
    margin-top: 12px;
  }
</style>
