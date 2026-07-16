(() => {
  const $ = (sel) => document.querySelector(sel);
  const $$ = (sel) => Array.from(document.querySelectorAll(sel));

  const state = {
    connected: false,
    screenKind: 'UNKNOWN',
    introDone: localStorage.getItem('myiui.introDone') === '1',
    clickGuiOpen: false,
    player: null,
    settings: loadSettings(),
  };

  function loadSettings() {
    try {
      return JSON.parse(localStorage.getItem('myiui.settings') || '{}');
    } catch {
      return {};
    }
  }

  function saveSettings() {
    localStorage.setItem('myiui.settings', JSON.stringify(state.settings));
  }

  function toast(text) {
    const host = $('#toastHost');
    const el = document.createElement('div');
    el.className = 'toast';
    el.textContent = text;
    host.appendChild(el);
    setTimeout(() => el.remove(), 2800);
  }

  async function action(cmd, data) {
    if (!window.myiui) return null;
    try {
      return await window.myiui.action(cmd, data || {});
    } catch (e) {
      console.warn('action failed', cmd, e);
      return null;
    }
  }

  function showLayer(id) {
    ['introLayer', 'menuLayer', 'hudLayer', 'clickGuiLayer'].forEach((lid) => {
      const el = document.getElementById(lid);
      if (!el) return;
      el.hidden = lid !== id;
    });
  }

  function setMenuView(name) {
    $$('.menu-view').forEach((v) => {
      v.hidden = v.dataset.view !== name;
    });
  }

  // ── Intro ──────────────────────────────────────────────
  function playIntro() {
    showLayer('introLayer');
    const emblem = $('#introEmblem');
    const iris = $('#introIris');
    const canvas = $('#introParticles');
    const ctx = canvas.getContext('2d');
    const resize = () => {
      canvas.width = window.innerWidth;
      canvas.height = window.innerHeight;
    };
    resize();
    window.addEventListener('resize', resize);

    const particles = Array.from({ length: 80 }, () => ({
      x: Math.random() * canvas.width,
      y: Math.random() * canvas.height,
      r: Math.random() * 2 + 0.5,
      vx: (Math.random() - 0.5) * 0.6,
      vy: (Math.random() - 0.5) * 0.6,
      a: Math.random(),
    }));

    let raf;
    const draw = () => {
      ctx.clearRect(0, 0, canvas.width, canvas.height);
      for (const p of particles) {
        p.x += p.vx;
        p.y += p.vy;
        p.a *= 0.995;
        ctx.beginPath();
        ctx.arc(p.x, p.y, p.r, 0, Math.PI * 2);
        ctx.fillStyle = `rgba(90,200,250,${Math.max(0, p.a)})`;
        ctx.fill();
      }
      raf = requestAnimationFrame(draw);
    };
    draw();

    requestAnimationFrame(() => emblem.classList.add('is-visible'));
    const t1 = setTimeout(() => iris.classList.add('is-open'), 1600);
    const t2 = setTimeout(finishIntro, 3200);

    function finishIntro() {
      clearTimeout(t1);
      clearTimeout(t2);
      cancelAnimationFrame(raf);
      state.introDone = true;
      localStorage.setItem('myiui.introDone', '1');
      enterMenu();
    }

    $('#introSkip').onclick = finishIntro;
    return finishIntro;
  }

  function enterMenu() {
    showLayer('menuLayer');
    setMenuView('home');
    window.myiui?.setPassthrough(false);
    refreshPlayer();
  }

  async function refreshPlayer() {
    const res = await action('GET_PLAYER');
    const data = res?.data || {};
    state.player = data;
    if (data.name) {
      $('#profileName').textContent = data.name;
      $('#profileAvatar').textContent = data.name.slice(0, 2).toUpperCase();
    }
    if (data.uuid) $('#profileUuid').textContent = data.uuid;
    if (data.minecraft) $('#menuVersion').textContent = `Fabric ${data.minecraft}`;
  }

  // ── Screen routing ─────────────────────────────────────
  function applyScreen(kind) {
    state.screenKind = kind;
    if (kind === 'TITLE' || kind === 'OTHER') {
      if (!state.introDone) playIntro();
      else enterMenu();
    } else if (kind === 'IN_GAME') {
      showLayer('hudLayer');
      window.myiui?.setPassthrough(true);
    } else if (kind === 'PAUSE') {
      showLayer('hudLayer');
      window.myiui?.setPassthrough(false);
    }
  }

  // ── HUD ────────────────────────────────────────────────
  function applyHud(data) {
    const root = document.documentElement;
    root.style.setProperty('--health-pct', String(data.healthPct ?? 1));
    root.style.setProperty('--absorption-pct', String(data.absorptionPct ?? 0));
    root.style.setProperty('--hunger-pct', String(data.hungerPct ?? 1));
    root.style.setProperty('--saturation-pct', String(data.saturationPct ?? 0));

    const health = data.health ?? 20;
    const max = data.healthMax ?? 20;
    $('#healthCaption').textContent = `${health.toFixed(1)} / ${max.toFixed(1)}`;
    const track = $('#healthTrack');
    track.dataset.low = data.lowHealth ? 'true' : 'false';
    track.dataset.damaged = data.damaged ? 'true' : 'false';

    const food = data.food ?? 20;
    $('#hungerCaption').textContent = `${Number(food).toFixed(0)} / 20`;

    const slots = $('#hotbarSlots');
    if (slots && slots.children.length !== 9) {
      slots.innerHTML = '';
      for (let i = 0; i < 9; i++) {
        const el = document.createElement('div');
        el.className = 'myiui-hotbar-slot';
        el.dataset.slot = String(i);
        slots.appendChild(el);
      }
    }
    $$('.myiui-hotbar-slot').forEach((el, i) => {
      el.classList.toggle('is-selected', i === (data.selectedSlot ?? 0));
    });
  }

  function applyIsland(data) {
    const pill = $('#islandPill');
    const expand = $('#islandExpand');
    const list = $('#islandPlayers');
    if (data.mode === 'tablist' && data.players) {
      $('#islandLabel').textContent = `${data.playerCount || data.players.length} 在线`;
      list.innerHTML = '';
      data.players.forEach((p) => {
        const li = document.createElement('li');
        li.textContent = `${p.name}  ${p.latency}ms`;
        list.appendChild(li);
      });
      expand.hidden = false;
      pill.parentElement.dataset.mode = 'expanded';
    } else {
      expand.hidden = true;
      $('#islandLabel').textContent = 'MyiUI';
      pill.parentElement.dataset.mode = 'collapsed';
    }
  }

  function applyMusic(data) {
    const np = $('#nowPlaying');
    if (!data || (!data.songName && !data.playing)) {
      np.hidden = true;
      return;
    }
    np.hidden = false;
    $('#npTitle').textContent = data.songName || '—';
    $('#npArtist').textContent = data.artist || '—';
    if (data.coverUrl) $('#npCover').src = data.coverUrl;
  }

  function applyChat(data) {
    const host = $('#chatMessages');
    if (!host || !data?.text) return;
    const line = document.createElement('div');
    line.className = 'chat-line';
    line.textContent = data.text;
    host.appendChild(line);
    while (host.children.length > 40) host.firstChild.remove();
    host.scrollTop = host.scrollHeight;
  }

  // ── Menu actions ───────────────────────────────────────
  document.body.addEventListener('click', async (e) => {
    const t = e.target.closest('[data-action],[data-nav]');
    if (!t) return;
    if (t.dataset.action) {
      if (t.dataset.action === 'OPEN_SINGLEPLAYER') {
        setMenuView('worlds');
        const res = await action('GET_WORLDS');
        const worlds = res?.data?.worlds || [];
        const list = $('#worldList');
        list.innerHTML = '';
        worlds.forEach((w) => {
          const li = document.createElement('li');
          li.textContent = w.name;
          li.onclick = () => action('JOIN_WORLD', { name: w.name });
          list.appendChild(li);
        });
        if (!worlds.length) {
          list.innerHTML = '<li>暂无世界 — 点击打开原版选择器</li>';
          list.firstChild.onclick = () => action('OPEN_SINGLEPLAYER');
        }
        return;
      }
      if (t.dataset.action === 'OPEN_MULTIPLAYER') {
        setMenuView('servers');
        const res = await action('GET_SERVERS');
        const servers = res?.data?.servers || [];
        const list = $('#serverList');
        list.innerHTML = '';
        servers.forEach((s) => {
          const li = document.createElement('li');
          li.textContent = `${s.name} — ${s.address}`;
          li.onclick = () => action('CONNECT_SERVER', { id: s.id });
          list.appendChild(li);
        });
        if (!servers.length) {
          list.innerHTML = '<li>暂无服务器 — 打开原版列表</li>';
          list.firstChild.onclick = () => action('OPEN_MULTIPLAYER');
        }
        return;
      }
      await action(t.dataset.action);
      return;
    }
    if (t.dataset.nav) setMenuView(t.dataset.nav);
  });

  $('#btnManager')?.addEventListener('click', () => setMenuView('manager'));
  $('#btnReplayIntro')?.addEventListener('click', () => {
    state.introDone = false;
    localStorage.removeItem('myiui.introDone');
    playIntro();
  });

  $('#bgVideoInput')?.addEventListener('change', async (e) => {
    const file = e.target.files?.[0];
    if (!file) return;
    const url = URL.createObjectURL(file);
    const video = $('#bgVideo');
    video.src = url;
    video.play().catch(() => {});
    await action('SET_BG_VIDEO', { path: file.path || file.name });
    toast('背景视频已应用');
  });

  $('#btnSaveOptions')?.addEventListener('click', async () => {
    await action('SET_OPTION', { key: 'fov', value: Number($('#optFov').value) });
    await action('SET_OPTION', { key: 'renderDistance', value: Number($('#optRd').value) });
    await action('SET_OPTION', { key: 'guiScale', value: Number($('#optGui').value) });
    toast('选项已保存');
  });

  // Load options when opening options view
  const optionsObserver = new MutationObserver(async () => {
    if (!$('#viewOptions').hidden) {
      const res = await action('GET_OPTIONS');
      const d = res?.data || {};
      if (d.fov != null) $('#optFov').value = d.fov;
      if (d.renderDistance != null) $('#optRd').value = d.renderDistance;
      if (d.guiScale != null) $('#optGui').value = d.guiScale;
    }
  });
  optionsObserver.observe($('#viewOptions'), { attributes: true, attributeFilter: ['hidden'] });

  // ── ClickGui ───────────────────────────────────────────
  function toggleClickGui(force) {
    state.clickGuiOpen = force ?? !state.clickGuiOpen;
    $('#clickGuiLayer').hidden = !state.clickGuiOpen;
    window.myiui?.setPassthrough(!state.clickGuiOpen && state.screenKind === 'IN_GAME');
  }

  window.addEventListener('keydown', (e) => {
    if (e.code === 'ShiftRight') {
      e.preventDefault();
      toggleClickGui();
    }
    if (e.code === 'Escape' && state.clickGuiOpen) toggleClickGui(false);
  });

  $('#cgClose')?.addEventListener('click', () => toggleClickGui(false));
  $('#cgChat')?.addEventListener('change', (e) => {
    action('UI_FLAGS', { chat: e.target.checked });
  });

  $('#layIslandX')?.addEventListener('input', (e) => {
    state.settings.islandX = e.target.value;
    $('#islandRoot').style.left = `${e.target.value}%`;
    saveSettings();
  });
  $('#layIslandY')?.addEventListener('input', (e) => {
    state.settings.islandY = e.target.value;
    $('#islandRoot').style.top = `${e.target.value}px`;
    saveSettings();
  });

  // Music
  $('#neQr')?.addEventListener('click', async () => {
    const res = await action('NE_QR_START');
    $('#neStatus').textContent = res?.ok ? '请扫码…' : (res?.data?.error || 'API 离线');
  });
  $('#neSearch')?.addEventListener('keydown', async (e) => {
    if (e.key !== 'Enter') return;
    const res = await action('NE_SEARCH', { q: e.target.value });
    const songs = res?.data?.result?.songs || [];
    const list = $('#neResults');
    list.innerHTML = '';
    songs.slice(0, 20).forEach((s) => {
      const li = document.createElement('li');
      li.textContent = `${s.name} — ${(s.artists || []).map((a) => a.name).join(',')}`;
      li.onclick = () => action('NE_PLAY_SONG', {
        id: String(s.id),
        name: s.name,
        artist: (s.artists || []).map((a) => a.name).join(', '),
        coverUrl: s.al?.picUrl || '',
      });
      list.appendChild(li);
    });
  });
  $('#nePlay')?.addEventListener('click', async () => {
    const st = await action('NE_PLAY_STATUS');
    if (st?.data?.playing) await action('NE_PLAY_PAUSE');
    else await action('NE_PLAY_RESUME');
  });
  $('#nePrev')?.addEventListener('click', () => action('NE_PLAY_PREV'));
  $('#neNext')?.addEventListener('click', () => action('NE_PLAY_NEXT'));

  $('#webOpen')?.addEventListener('click', () => {
    const frame = $('#webFrame');
    frame.hidden = false;
    frame.src = $('#webUrl').value;
  });

  // ── WS wiring ──────────────────────────────────────────
  if (window.myiui) {
    window.myiui.onStatus((s) => {
      state.connected = !!s.connected;
      $('#wsStatusText').textContent = s.connected ? '已连接 · 主菜单' : '等待 Mod…';
    });
    window.myiui.onScreen((s) => applyScreen(s.kind));
    window.myiui.onMessage((msg) => {
      switch (msg.type) {
        case 'hello':
          toast(`已连接 MyiUI ${msg.data?.modVersion || ''}`);
          refreshPlayer();
          break;
        case 'screen':
          applyScreen(msg.data?.kind);
          break;
        case 'hud':
          applyHud(msg.data || {});
          break;
        case 'island':
        case 'tablist':
          applyIsland(msg.data || {});
          break;
        case 'music':
          applyMusic(msg.data || {});
          break;
        case 'chat':
          applyChat(msg.data || {});
          break;
        case 'bg_video':
          if (msg.data?.path) {
            $('#bgVideo').src = msg.data.path;
            $('#bgVideo').play().catch(() => {});
          }
          break;
        case 'toast':
          toast(msg.data?.text || '');
          break;
        default:
          break;
      }
    });
  }

  // Default: wait for connection; show menu shell translucent hint
  showLayer('menuLayer');
  setMenuView('home');
  if (state.settings.islandX) $('#islandRoot').style.left = `${state.settings.islandX}%`;
  if (state.settings.islandY) $('#islandRoot').style.top = `${state.settings.islandY}px`;
})();
