/* Lightweight, dependency-free Sphinx UX helpers for MiniZIP docs. */
(function() {
  const THEME_KEY = "minizip-doc-theme";
  const DARK = "dark";
  const LIGHT = "light";

  function preferredTheme() {
    const saved = window.localStorage.getItem(THEME_KEY);
    if (saved === DARK || saved === LIGHT) return saved;
    return DARK;
  }

  function applyTheme(theme) {
    const isDark = theme !== LIGHT;
    document.documentElement.classList.toggle("stkman-theme-light", !isDark);
    window.localStorage.setItem(THEME_KEY, isDark ? DARK : LIGHT);
    const toggle = document.getElementById("stkman-theme-toggle");
    if (toggle) {
      toggle.textContent = isDark ? "☀ Light mode" : "🌙 Dark mode";
      toggle.setAttribute("aria-label", isDark ? "Switch to light theme" : "Switch to dark theme");
      toggle.setAttribute("title", toggle.textContent);
    }
  }

  function ensureToolbar() {
    if (document.getElementById("stkman-toolbar")) return;

    const bar = document.createElement("div");
    bar.id = "stkman-toolbar";
    bar.className = "stkman-toolbar";

    const btn = document.createElement("button");
    btn.type = "button";
    btn.id = "stkman-theme-toggle";
    btn.className = "stkman-btn";
    btn.addEventListener("click", () => {
      const nowDark = !document.documentElement.classList.contains("stkman-theme-light");
      applyTheme(nowDark ? LIGHT : DARK);
    });

    const copy = document.createElement("button");
    copy.type = "button";
    copy.className = "stkman-btn";
    copy.textContent = "Copy heading link";
    copy.addEventListener("click", () => {
      const hash = window.location.hash || "";
      const link = (window.location.origin + window.location.pathname + hash);
      if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(link).catch(() => {});
      }
      showToast("Heading link copied");
    });

    bar.appendChild(btn);
    bar.appendChild(copy);
    document.body.appendChild(bar);

    const progress = document.createElement("div");
    progress.id = "stkman-progress";
    progress.className = "stkman-progress";
    progress.style.display = "none";
    document.body.appendChild(progress);

    applyTheme(preferredTheme());
  }

  let toastTimer = null;
  function showToast(text) {
    const toast = document.getElementById("stkman-progress");
    toast.textContent = text;
    toast.style.display = "block";
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => {
      toast.style.display = "none";
      toast.textContent = "";
    }, 1500);
  }

  function setupProgressBars() {
    const nodes = document.querySelectorAll(".wy-side-scroll a.internal, .rst-content a.internal");
    nodes.forEach((node) => {
      node.addEventListener("click", () => {
        const progress = document.getElementById("stkman-progress");
        if (!progress) return;
        progress.textContent = "Loading...";
        progress.style.display = "block";
        setTimeout(() => {
          if (progress.style.display === "block") {
            progress.style.display = "none";
            progress.textContent = "";
          }
        }, 500);
      });
    });
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", () => {
      ensureToolbar();
      setupProgressBars();
    });
  } else {
    ensureToolbar();
    setupProgressBars();
  }
})();
