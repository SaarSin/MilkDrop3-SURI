// MilkDrop 3.0.S guides - small progressive enhancements. Everything works without JS.
(function () {
  'use strict';

  // Mobile table-of-contents toggle.
  var sidebar = document.querySelector('.sidebar');
  var toggle = document.querySelector('.menu-toggle');
  if (sidebar && toggle) {
    toggle.addEventListener('click', function () {
      var open = sidebar.classList.toggle('open');
      toggle.setAttribute('aria-expanded', String(open));
    });
  }

  // Highlight the section currently on screen in the table of contents.
  var links = Array.prototype.slice.call(document.querySelectorAll('.toc a[href^="#"]'));
  var byId = {};
  links.forEach(function (a) { byId[a.getAttribute('href').slice(1)] = a; });
  if ('IntersectionObserver' in window && links.length) {
    var observer = new IntersectionObserver(function (entries) {
      entries.forEach(function (e) {
        if (!e.isIntersecting) return;
        links.forEach(function (a) { a.classList.remove('active'); });
        var link = byId[e.target.id];
        if (link) link.classList.add('active');
      });
    }, { rootMargin: '0px 0px -70% 0px' });
    Object.keys(byId).forEach(function (id) {
      var el = document.getElementById(id);
      if (el) observer.observe(el);
    });
  }

  // Copy buttons on code blocks.
  document.querySelectorAll('pre').forEach(function (pre) {
    var btn = document.createElement('button');
    btn.type = 'button';
    btn.className = 'copy-btn';
    btn.textContent = 'Copy';
    btn.setAttribute('aria-label', 'Copy code to clipboard');
    btn.addEventListener('click', function () {
      var text = pre.querySelector('code') ? pre.querySelector('code').innerText : pre.innerText;
      var done = function () { btn.textContent = 'Copied'; setTimeout(function () { btn.textContent = 'Copy'; }, 1400); };
      if (navigator.clipboard && window.isSecureContext) {
        navigator.clipboard.writeText(text).then(done, function () { fallbackCopy(text); done(); });
      } else { fallbackCopy(text); done(); }
    });
    pre.appendChild(btn);
  });
  function fallbackCopy(text) {
    var ta = document.createElement('textarea');
    ta.value = text; ta.style.position = 'fixed'; ta.style.opacity = '0';
    document.body.appendChild(ta); ta.select();
    try { document.execCommand('copy'); } catch (e) { /* ignore */ }
    document.body.removeChild(ta);
  }

  // Live filter: <input data-filter="tableGroupId"> hides non-matching rows in the tables inside that element.
  document.querySelectorAll('input[data-filter]').forEach(function (input) {
    var scope = document.getElementById(input.getAttribute('data-filter'));
    if (!scope) return;
    var empty = scope.querySelector('.empty');
    input.addEventListener('input', function () {
      var q = input.value.trim().toLowerCase();
      var shown = 0;
      scope.querySelectorAll('tbody tr').forEach(function (tr) {
        if (tr.classList.contains('group-row')) return;
        var hit = !q || tr.textContent.toLowerCase().indexOf(q) !== -1;
        tr.hidden = !hit;
        if (hit) shown++;
      });
      // hide group headers whose rows are all hidden
      scope.querySelectorAll('tr.group-row').forEach(function (g) {
        var next = g.nextElementSibling, any = false;
        while (next && !next.classList.contains('group-row')) { if (!next.hidden) any = true; next = next.nextElementSibling; }
        g.hidden = !any;
      });
      scope.querySelectorAll('.table-wrap').forEach(function (w) {
        w.hidden = q && !w.querySelector('tbody tr:not([hidden]):not(.group-row)');
      });
      if (empty) empty.style.display = shown ? 'none' : 'block';
    });
  });
})();
