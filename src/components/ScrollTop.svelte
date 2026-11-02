<script lang="ts">
  let visible = $state(false)

  $effect(() => {
    const update = () => (visible = window.scrollY >= 300)
    update()
    window.addEventListener('scroll', update, { passive: true })
    return () => window.removeEventListener('scroll', update)
  })
</script>

<!-- svelte-ignore a11y_invalid_attribute -->
<a href="#" class="scrolltop" class:show-scroll={visible} id="scroll-top" aria-label="Scroll to top">
  <i class="bx bx-chevron-up"></i>
</a>
