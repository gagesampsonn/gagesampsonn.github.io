// Gage Sampson — Minimal JS
document.addEventListener('DOMContentLoaded', () => {

  // Mobile nav toggle
  const toggle = document.querySelector('.nav-toggle');
  const nav = document.querySelector('.site-nav');
  if (toggle && nav) {
    toggle.addEventListener('click', () => {
      toggle.classList.toggle('active');
      nav.classList.toggle('active');
      document.body.style.overflow = nav.classList.contains('active') ? 'hidden' : '';
    });
    nav.querySelectorAll('a').forEach(a => a.addEventListener('click', () => {
      toggle.classList.remove('active');
      nav.classList.remove('active');
      document.body.style.overflow = '';
    }));
    document.addEventListener('click', e => {
      if (!nav.contains(e.target) && !toggle.contains(e.target)) {
        toggle.classList.remove('active');
        nav.classList.remove('active');
        document.body.style.overflow = '';
      }
    });
  }

  // Smooth scroll for anchor links
  document.querySelectorAll('a[href^="#"]').forEach(a => {
    a.addEventListener('click', e => {
      const href = a.getAttribute('href');
      if (href === '#' || !href) return;
      const el = document.querySelector(href);
      if (el) {
        e.preventDefault();
        el.scrollIntoView({ behavior: 'smooth', block: 'start' });
        // offset for sticky header
        setTimeout(() => window.scrollBy(0, -64), 300);
      }
    });
  });

  // Header shadow on scroll
  const header = document.querySelector('.site-header');
  if (header) {
    window.addEventListener('scroll', () => {
      header.style.boxShadow = window.scrollY > 20 ? '0 1px 4px rgba(0,0,0,.06)' : 'none';
    }, { passive: true });
  }

  // Story card click → open matching detail
  document.querySelectorAll('.story-card[data-detail]').forEach(card => {
    card.addEventListener('click', () => {
      const id = card.getAttribute('data-detail');
      const detail = document.getElementById(id);
      if (detail) {
        detail.open = true;
        detail.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
      }
    });
  });

  // Keyboard: ESC closes mobile nav
  document.addEventListener('keydown', e => {
    if (e.key === 'Escape' && nav && nav.classList.contains('active')) {
      toggle.classList.remove('active');
      nav.classList.remove('active');
      document.body.style.overflow = '';
    }
  });

  // Simple form validation
  const form = document.querySelector('.contact-form');
  if (form) {
    form.addEventListener('submit', e => {
      let valid = true;
      form.querySelectorAll('[required]').forEach(input => {
        if (!input.value.trim()) { input.style.borderColor = '#c00'; valid = false; }
        else { input.style.borderColor = ''; }
      });
      if (!valid) { e.preventDefault(); }
    });
  }
});
