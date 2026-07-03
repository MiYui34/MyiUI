(function () {
  'use strict';

  var TRANSITION_MS = 180;

  function scaleCanvas() {
    var canvas = document.getElementById('canvas');
    if (!canvas) return;
    var scale = Math.min(window.innerWidth / 1920, window.innerHeight / 1080);
    canvas.style.transform = 'scale(' + scale + ')';
  }

  function navigate(href) {
    var screen = document.querySelector('.screen');
    if (!screen) {
      window.location.href = href;
      return;
    }
    screen.classList.add('is-exiting');
    setTimeout(function () {
      window.location.href = href;
    }, TRANSITION_MS);
  }

  function initNavigation() {
    document.querySelectorAll('[data-nav]').forEach(function (el) {
      el.addEventListener('click', function (e) {
        var href = el.getAttribute('data-nav') || el.getAttribute('href');
        if (!href || href === '#') return;
        e.preventDefault();
        navigate(href);
      });
    });
  }

  function initToggles() {
    document.querySelectorAll('.toggle input').forEach(function (input) {
      input.addEventListener('change', function () {
        input.closest('.setting-row')?.setAttribute('data-value', input.checked ? 'true' : 'false');
      });
    });
  }

  function initSliders() {
    document.querySelectorAll('.slider-wrap input[type="range"]').forEach(function (input) {
      var valueEl = input.closest('.slider-wrap')?.querySelector('.slider-value');
      var unit = input.dataset.unit || '';
      function update() {
        if (valueEl) valueEl.textContent = input.value + unit;
      }
      input.addEventListener('input', update);
      update();
    });
  }

  function initEnumChips() {
    document.querySelectorAll('.enum-row').forEach(function (row) {
      row.querySelectorAll('.enum-chip').forEach(function (chip) {
        chip.addEventListener('click', function () {
          row.querySelectorAll('.enum-chip').forEach(function (c) { c.classList.remove('selected'); });
          chip.classList.add('selected');
        });
      });
    });
  }

  function initKeybinds() {
    document.querySelectorAll('.keybind-btn').forEach(function (btn) {
      btn.addEventListener('click', function () {
        if (btn.classList.contains('is-listening')) return;
        btn.classList.add('is-listening');
        btn.textContent = '按下按键…';
        function onKey(e) {
          e.preventDefault();
          btn.textContent = e.key === ' ' ? '空格' : e.key.length === 1 ? e.key.toUpperCase() : e.key;
          btn.classList.remove('is-listening');
          document.removeEventListener('keydown', onKey);
        }
        document.addEventListener('keydown', onKey);
      });
    });
  }

  function initSelectableCards() {
    document.querySelectorAll('.world-list, .server-list').forEach(function (list) {
      list.querySelectorAll('.world-card, .server-card').forEach(function (card) {
        card.addEventListener('click', function () {
          list.querySelectorAll('.selected').forEach(function (c) { c.classList.remove('selected'); });
          card.classList.add('selected');
        });
      });
    });
  }

  function initPackToggles() {
    document.querySelectorAll('.pack-row .toggle input').forEach(function (input) {
      input.addEventListener('change', function () {
        var row = input.closest('.pack-row');
        if (row) row.classList.toggle('pack-row--enabled', input.checked);
      });
    });
  }

  window.MyUIMenu = {
    navigate: navigate,
    scaleCanvas: scaleCanvas
  };

  document.addEventListener('DOMContentLoaded', function () {
    scaleCanvas();
    window.addEventListener('resize', scaleCanvas);
    initNavigation();
    initToggles();
    initSliders();
    initEnumChips();
    initKeybinds();
    initSelectableCards();
    initPackToggles();
  });
})();
