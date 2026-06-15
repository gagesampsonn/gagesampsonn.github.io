// Gage Sampson — site interactions
document.addEventListener('DOMContentLoaded', () => {
  const toggle = document.getElementById('menuToggle');
  const nav = document.getElementById('nav');

  // Mobile nav
  const closeNav = () => {
    if (!nav) return;
    toggle.classList.remove('active');
    nav.classList.remove('active');
    toggle.setAttribute('aria-expanded', 'false');
    document.body.style.overflow = '';
  };
  if (toggle && nav) {
    toggle.addEventListener('click', () => {
      const open = nav.classList.toggle('active');
      toggle.classList.toggle('active', open);
      toggle.setAttribute('aria-expanded', open ? 'true' : 'false');
      document.body.style.overflow = open ? 'hidden' : '';
    });
    nav.querySelectorAll('a').forEach(a => a.addEventListener('click', closeNav));
    document.addEventListener('click', e => {
      if (nav.classList.contains('active') && !nav.contains(e.target) && !toggle.contains(e.target)) closeNav();
    });
    document.addEventListener('keydown', e => { if (e.key === 'Escape') closeNav(); });
  }

  // Smooth scroll with sticky-header offset
  document.querySelectorAll('a[href^="#"]').forEach(a => {
    a.addEventListener('click', e => {
      const href = a.getAttribute('href');
      if (!href || href === '#') return;
      const el = document.querySelector(href);
      if (!el) return;
      e.preventDefault();
      const y = el.getBoundingClientRect().top + window.scrollY - 78;
      window.scrollTo({ top: y, behavior: 'smooth' });
    });
  });

  // Header shadow on scroll
  const header = document.querySelector('.site-header');
  if (header) {
    const onScroll = () => { header.style.boxShadow = window.scrollY > 16 ? '0 6px 24px -12px rgba(22,20,14,.25)' : 'none'; };
    window.addEventListener('scroll', onScroll, { passive: true });
    onScroll();
  }

  // Scroll-reveal (JS adds the hidden state so no-JS users still see everything)
  const reduce = window.matchMedia('(prefers-reduced-motion: reduce)').matches;
  if (!reduce && 'IntersectionObserver' in window) {
    const targets = document.querySelectorAll(
      '.section .section-label, .section h2, .section > .wrap-wide > p, .svc, .t-item, .testimonial, .feature, .stats, .facts-list, .about-img'
    );
    const io = new IntersectionObserver((entries) => {
      entries.forEach(en => { if (en.isIntersecting) { en.target.classList.add('in'); io.unobserve(en.target); } });
    }, { threshold: 0.12, rootMargin: '0px 0px -6% 0px' });
    targets.forEach((t, i) => { t.classList.add('io'); t.style.transitionDelay = (Math.min(i % 4, 3) * 60) + 'ms'; io.observe(t); });
  }
});
