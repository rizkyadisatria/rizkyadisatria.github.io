<script lang="ts">
  // Template CV tersembunyi: hanya dipakai untuk membuat PDF (lihat lib/cv.ts).
  let { el = $bindable() }: { el?: HTMLElement } = $props()
</script>

<div id="cv-print" bind:this={el}>
  <div class="cv-wrap">
    <div class="cv-left">
      <div class="cv-photo-wrap">
        <img src="/assets/img/profile.jpg" alt="Rizky Adi Satria" class="cv-photo" />
      </div>
      <div class="cv-block">
        <h3 class="cv-block__title">Contact</h3>
        <ul class="cv-list">
          <li><i class="bx bx-map-pin"></i> Mataram, NTB</li>
          <li><i class="bx bx-envelope"></i> rizkyadsatria@gmail.com</li>
          <li><i class="bx bx-phone"></i> +62 859-3300-5036</li>
          <li><i class="bx bxl-github"></i> github.com/rizkyadisatria</li>
        </ul>
      </div>
      <div class="cv-block">
        <h3 class="cv-block__title">Skills</h3>
        <ul class="cv-list">
          <li>HTML & CSS</li>
          <li>JavaScript</li>
          <li>React.js</li>
          <li>Laravel</li>
          <li>Node.js / Express.js</li>
          <li>MongoDB / MySQL</li>
          <li>WordPress</li>
          <li>Tailwind CSS</li>
          <li>Git & GitHub</li>
          <li>Linux / VPS</li>
        </ul>
      </div>
      <div class="cv-block">
        <h3 class="cv-block__title">Languages</h3>
        <ul class="cv-list">
          <li>Indonesian — Native</li>
          <li>English — Conversational</li>
        </ul>
      </div>
    </div>
    <div class="cv-right">
      <div class="cv-header">
        <h1 class="cv-name">Rizky Adi Satria</h1>
        <p class="cv-title">Full Stack Developer</p>
      </div>
      <div class="cv-section">
        <h2 class="cv-section__title">Profile</h2>
        <p class="cv-text">
          Full Stack Developer with 4+ years of professional experience building end-to-end web solutions. Specializes
          in e-commerce platforms with a strong command of both frontend and backend technologies. Passionate about
          clean, performant code and tackling complex technical challenges.
        </p>
      </div>
      <div class="cv-section">
        <h2 class="cv-section__title">Experience</h2>
        <div class="cv-item">
          <div class="cv-item__row">
            <strong class="cv-item__title">Technical Support</strong>
            <span class="cv-item__period">Aug 2021 – Feb 2022</span>
          </div>
          <p class="cv-item__sub">CV. Data Digital Indonesia &bull; dewahoster.co.id</p>
          <p class="cv-text">
            Resolved client issues related to domain management, hosting configuration, and WordPress websites.
            Provided step-by-step technical guidance and troubleshooting support.
          </p>
        </div>
        <div class="cv-item">
          <div class="cv-item__row">
            <strong class="cv-item__title">Full Stack Developer</strong>
            <span class="cv-item__period">Mar 2022 – Present</span>
          </div>
          <p class="cv-item__sub">PT. Bumigora Karya Mandiri &bull; bumigorashop.com</p>
          <p class="cv-text">
            Built and maintained the Bumigorashop e-commerce platform from the ground up, collaborating with a
            designer to create a marketplace empowering local sellers to reach wider markets.
          </p>
        </div>
        <div class="cv-item">
          <div class="cv-item__row">
            <strong class="cv-item__title">Freelance Developer</strong>
            <span class="cv-item__period">Aug 2023 – Present</span>
          </div>
          <p class="cv-item__sub">Self-employed</p>
          <p class="cv-text">
            Deliver end-to-end web and mobile solutions — hotel booking systems, company profiles, hosting order
            platforms, Mikrotik management, VPN setups, and VPS deployment.
          </p>
        </div>
      </div>
      <div class="cv-section">
        <h2 class="cv-section__title">Education</h2>
        <div class="cv-item">
          <div class="cv-item__row">
            <strong class="cv-item__title">Network Engineering</strong>
            <span class="cv-item__period">2015 – 2018</span>
          </div>
          <p class="cv-item__sub">SMK Negeri 6 Mataram</p>
        </div>
        <div class="cv-item">
          <div class="cv-item__row">
            <strong class="cv-item__title">Bachelor of Computer Science</strong>
            <span class="cv-item__period">2018 – 2022</span>
          </div>
          <p class="cv-item__sub">Universitas Bumigora</p>
        </div>
      </div>
    </div>
  </div>
</div>
