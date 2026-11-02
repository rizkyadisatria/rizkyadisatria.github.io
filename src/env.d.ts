// Impor sisi-efek CSS dari paket npm (ditangani oleh Bun.build).
declare module '*.css'
declare module '*.svelte' {
  import type { Component } from 'svelte'
  const component: Component<any>
  export default component
}
