// html2pdf (+ jsPDF + html2canvas) cukup berat, jadi baru dimuat saat tombol CV diklik.
const PDF_OPTIONS = {
  margin: 0,
  filename: 'RizkyAdiSatria_CV.pdf',
  image: { type: 'jpeg' as const, quality: 0.98 },
  html2canvas: { scale: 2, useCORS: true, logging: false },
  jsPDF: { unit: 'mm', format: 'a4', orientation: 'portrait' as const },
  enableLinks: true,
}

/** Render template CV tersembunyi (#cv-print) menjadi PDF lalu unduh. */
export async function generateCV(el: HTMLElement): Promise<void> {
  const { default: html2pdf } = await import('html2pdf.js')
  el.style.display = 'block'
  try {
    // jeda singkat agar layout template selesai dihitung sebelum di-capture
    await new Promise((resolve) => setTimeout(resolve, 300))
    await html2pdf().set(PDF_OPTIONS).from(el).save()
  } finally {
    el.style.display = 'none'
  }
}
