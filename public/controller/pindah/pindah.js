// Menerima progres panduan dari alamat lama (rizkyadisatria.github.io) lewat URL hash.
// Hash tidak dikirim ke server. Hanya kunci progres yang dikenal, hanya jika belum ada di sini.
(function () {
  var OK = /^(robot-ai\/[a-z0-9-]+|elektronika-dasar|rakit-cuaca)(:ringkas)?$/
  var m = location.hash.match(/^#pindah=([A-Za-z0-9+/=]+)$/)
  try {
    if (m) {
      var data = JSON.parse(decodeURIComponent(escape(atob(m[1]))))
      Object.keys(data).forEach(function (k) {
        var v = data[k]
        if (!OK.test(k) || typeof v !== 'string' || v.length > 20000) return
        try { JSON.parse(v) } catch (e) { return }
        if (localStorage.getItem(k) === null) localStorage.setItem(k, v)
      })
    }
  } catch (e) { /* abaikan: tetap lanjut ke panduan */ }
  history.replaceState(null, '', location.pathname)
  location.replace('/controller/')
})()
