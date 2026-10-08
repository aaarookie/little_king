/* Offline Battle Designer UI. Data and validation live in model.js. */
(function () {
  'use strict';
  const $ = id => document.getElementById(id);
  const M = window.LKLevel;
  if (!M) { $('save-status').textContent = '模块未加载：请保留同目录的 model.js，再刷新。'; return; }
  const NS = 'http://www.w3.org/2000/svg';
  const STORAGE_KEY = 'little-king.battle-designer.draft.v1';
  const TEAM_NAMES = { player: '我方', enemy: '敌方', neutral: '中立' };
  const TEAM_COLORS = { player: '#3c8068', enemy: '#b06452', neutral: '#777a64' };
  const ENEMY_ONLY = new Set(['skeleton', 'skeleton-archer', 'necromancer', 'skeleton-giant', 'skeleton-king']);
  const clone = value => JSON.parse(JSON.stringify(value));
  const escapeXML = value => String(value).replace(/[&<>"']/g, char => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&apos;' })[char]);
  const short = (value, length) => Array.from(String(value)).length > length ? Array.from(String(value)).slice(0, length).join('') + '…' : String(value);
  const round = number => Math.round(number * 1000) / 1000;
  const state = { project: M.createTerrainExample(), selectedId: null, placement: null, category: '全部', search: '', snap: true, panel: 'layout', selectedWaveId: null, pickWaveId: null,
    history: [], future: [], dirty: false, draftBlocked: false, brokenDraft: null, zoom: 1, panX: 0, panY: 0,
    space: false, gesture: null, toastTimer: null, restoring: false };
  const svg = $('battle-canvas');

  function toast(message, error) {
    $('toast').textContent = message; $('toast').hidden = false; $('toast').classList.toggle('error', Boolean(error));
    clearTimeout(state.toastTimer); state.toastTimer = setTimeout(() => { $('toast').hidden = true; }, error ? 7000 : 3800);
  }
  function status(message, warning) { $('save-status').textContent = message; $('save-status').classList.toggle('status-warning', Boolean(warning)); }
  function selected() { return state.project.objects.find(o => o.id === state.selectedId); }
  function projectKey(p) { return JSON.stringify(p); }
  function persist() {
    if (state.draftBlocked) { status('旧草稿已保护 · 当前设计请先下载 JSON', true); return; }
    try {
      localStorage.setItem(STORAGE_KEY, JSON.stringify({ project: state.project, dirty: state.dirty, savedAt: new Date().toISOString() }));
      status((state.dirty ? '草稿已保存在此浏览器 · 尚有未下载的修改' : '草稿已保存在此浏览器') + ' · ' + new Date().toLocaleTimeString('zh-CN', { hour: '2-digit', minute: '2-digit' }));
    } catch (_) { status('浏览器未允许保存草稿 · 请下载 JSON 保留设计', true); }
  }
  function restore() {
    try {
      const raw = localStorage.getItem(STORAGE_KEY);
      if (!raw) { status('已打开示例 · 修改后会自动保存本地草稿'); return; }
      try {
        const envelope = JSON.parse(raw);
        if (!envelope || typeof envelope !== 'object' || typeof envelope.dirty !== 'boolean' || !envelope.project) throw new Error('无法识别草稿结构');
        state.project = M.parseProject(JSON.stringify(envelope.project));
        state.dirty = envelope.dirty;
        status('已恢复本地草稿' + (state.dirty ? ' · 尚有未下载的修改' : ''));
      } catch (_) {
        state.brokenDraft = raw; state.draftBlocked = true; $('draft-alert').hidden = false;
        status('旧草稿无法读取，已保留原文 · 当前展示示例', true);
      }
    } catch (_) { status('浏览器未允许读取草稿 · 下载 JSON 可保留设计', true); }
  }
  function pushHistory(before) {
    state.history.push(before); if (state.history.length > 100) state.history.shift();
    state.future = []; state.dirty = true; state.project.meta.updatedAt = new Date().toISOString(); persist();
  }
  function mutate(action, options) {
    const before = clone(state.project), previousSelection = state.selectedId;
    try {
      action();
      const result = M.validate(state.project);
      if (result.errors.length) throw new Error(result.errors[0]);
      // Match import/export limits before accepting any edit, including long
      // notes spread across hundreds of objects, so every saved draft reopens.
      M.serialize(state.project);
    } catch (error) {
      state.project = before; state.selectedId = previousSelection;
      toast('未应用这次修改：' + short(error.message, 180), true); render(options); return;
    }
    if (projectKey(before) !== projectKey(state.project)) pushHistory(before);
    render(options);
  }
  function undo() {
    if (!state.history.length) return;
    state.future.push(clone(state.project)); state.project = state.history.pop();
    if (!selected()) state.selectedId = null;
    state.dirty = true; state.project.meta.updatedAt = new Date().toISOString(); persist(); render();
  }
  function redo() {
    if (!state.future.length) return;
    state.history.push(clone(state.project)); state.project = state.future.pop();
    if (!selected()) state.selectedId = null;
    state.dirty = true; state.project.meta.updatedAt = new Date().toISOString(); persist(); render();
  }
  function element(tag, attrs, text) {
    const node = document.createElementNS(NS, tag);
    Object.entries(attrs || {}).forEach(([key, value]) => node.setAttribute(key, String(value)));
    if (text !== undefined) node.textContent = text;
    return node;
  }
  function symbolMarkup(module, width, height) {
    const c = escapeXML(module.color), w = width, h = height, hw = w / 2, hh = h / 2;
    const base = `fill="${c}" stroke="#364832" stroke-width="0.07" stroke-linejoin="round"`;
    if (module.role === 'terrain') {
      let extra = '';
      if (module.id === 'water') extra = `<path d="M ${-hw * .65} ${-hh * .2} q ${w * .12} ${-h * .15} ${w * .24} 0 t ${w * .24} 0 M ${-hw * .25} ${hh * .35} q ${w * .12} ${-h * .15} ${w * .24} 0" stroke="#d4e9df" stroke-width=".07" fill="none" opacity=".75"/>`;
      if (module.id === 'road') extra = `<path d="M ${-hw * .85} 0 H ${hw * .85}" stroke="#e4d6a9" stroke-width=".06" stroke-dasharray=".24 .4" fill="none"/>`;
      if (module.id === 'grass') extra = `<path d="M ${-w * .24} ${h * .13} l -.12 -.2 m .12 .2 l .12 -.26 M ${w * .21} ${-h * .12} l -.1 -.19 m .1 .19 l .13 -.24" stroke="#46673f" stroke-width=".06" fill="none" opacity=".6"/>`;
      if (module.id === 'brush') extra = [-.3, 0, .3].map(x => `<path d="M ${w*x} ${h*.2} l -.3 ${-h*.5} m .3 ${h*.5} l .3 ${-h*.4}" stroke="#c1d29b" stroke-width=".12" fill="none"/>`).join('');
      if (module.id === 'swamp') extra = `<ellipse rx="${w*.3}" ry="${h*.2}" fill="#344f492e"/><path d="M ${-w*.3} 0 h ${w*.25} m ${w*.08} ${h*.12} h ${w*.2}" stroke="#bfd2a7" stroke-width=".1"/>`;
      return `<rect x="${-hw}" y="${-hh}" width="${w}" height="${h}" rx=".12" fill="${c}" fill-opacity=".75" stroke="${c}" stroke-width=".035"/>${extra}`;
    }
    if (module.role === 'zone') return `<rect x="${-hw}" y="${-hh}" width="${w}" height="${h}" rx=".2" fill="${c}" fill-opacity=".14" stroke="${c}" stroke-width=".075" stroke-dasharray=".35 .2"/><path d="M ${-hw + .35} ${-hh + .7} v -.35 h .35 M ${hw - .35} ${hh - .7} v .35 h -.35" fill="none" stroke="${c}" stroke-width=".09"/>`;
    if (module.id === 'tree') return `<ellipse cx="0" cy="${h * .32}" rx="${w * .39}" ry="${h * .13}" fill="#253c2623"/><path d="M -.13 ${h * .38} L -.1 ${-h * .08} H .13 L .16 ${h * .38}" fill="#826d43" stroke="#435239" stroke-width=".05"/><path d="M ${-hw * .9} ${h * .08} Q ${-hw * 1.05} ${-h * .15} ${-w * .23} ${-h * .24} Q ${-w * .15} ${-hh * 1.03} ${w * .06} ${-hh * .84} Q ${w * .38} ${-hh * .97} ${w * .36} ${-h * .18} Q ${hw * 1.02} ${-h * .13} ${hw * .92} ${h * .09} Q ${w * .2} ${h * .32} 0 ${h * .15} Q ${-w * .23} ${h * .3} ${-hw * .9} ${h * .08}Z" ${base}/><path d="M ${-w * .23} ${-h * .12} Q ${-w * .1} ${-h * .3} ${w * .07} ${-h * .24}" fill="none" stroke="#88a069" stroke-width=".08"/>`;
    if (module.id === 'rock') return `<ellipse cy="${h * .26}" rx="${hw}" ry="${h * .21}" fill="#313b2728"/><path d="M ${-hw * .93} ${h * .22} L ${-w * .35} ${-h * .28} L ${w * .06} ${-hh * .93} L ${w * .37} ${-h * .24} L ${hw * .95} ${h * .26} L ${w * .16} ${hh * .8} L ${-w * .26} ${h * .37} Z" ${base}/><path d="M ${-w * .35} ${-h * .28} L ${-w * .1} ${h * .09} L ${w * .37} ${-h * .24} M ${-w * .1} ${h * .09} L ${w * .16} ${hh * .8}" fill="none" stroke="#cfceaf" stroke-width=".06"/>`;
    if (module.id === 'flowers') return `<ellipse cy="${h * .13}" rx="${w * .45}" ry="${h * .27}" fill="#718c55"/>` + [[-.27, -.08], [0, -.22], [.25, .04]].map(([x,y]) => `<path d="M ${x * w} ${h * .18} V ${y * h}" stroke="#3f6136" stroke-width=".07"/><circle cx="${x * w}" cy="${y * h}" r="${Math.min(w, h) * .18}" fill="${c}" stroke="#f0dcc7" stroke-width=".08"/><circle cx="${x * w}" cy="${y * h}" r=".06" fill="#aa8449"/>`).join('');
    if (module.role === 'note') return `<path d="M ${-hw} ${-hh} H ${hw - .32} L ${hw} ${-hh + .32} V ${hh} H ${-hw} Z" fill="#fcf1bf" stroke="#bc9d4e" stroke-width=".06"/><path d="M ${hw - .32} ${-hh} V ${-hh + .32} H ${hw}" fill="#e2c878" stroke="#bc9d4e" stroke-width=".04"/>`;
    if (module.id.startsWith('wall-')) {
      const n = Math.max(2, Math.min(10, Math.round(w / .65))); let lines = '';
      for (let i = 1; i < n; i++) lines += `<path d="M ${-hw + w * i / n} ${-hh} V ${hh}" stroke="#47503c" stroke-width=".035"/>`;
      return `<rect x="${-hw}" y="${-hh}" width="${w}" height="${h}" rx=".06" ${base}/><path d="M ${-hw + .05} ${-hh + .055} H ${hw - .05}" stroke="#f2e8cc" stroke-width=".06"/>${lines}<path d="M 0 ${-hh - .15} l -.18 -.15 h .36 Z" fill="#6a6c4a"/>`;
    }
    if (module.role === 'building') {
      if (module.id === 'siege-cart') return `<circle cx="${-w * .27}" cy="${h * .33}" r="${h * .13}" fill="#4c4b38"/><circle cx="${w * .27}" cy="${h * .33}" r="${h * .13}" fill="#4c4b38"/><rect x="${-w * .4}" y="${-h * .12}" width="${w * .8}" height="${h * .4}" rx=".09" ${base}/><path d="M ${-w * .42} ${-h * .17} L ${-w * .26} ${-h * .4} H ${w * .27} L ${w * .42} ${-h * .17}Z" fill="#e0c79c" stroke="#564c35" stroke-width=".07"/><path d="M ${-w * .35} ${h * .09} H ${w * .5}" stroke="#534638" stroke-width=".13"/>`;
      const tower = module.id.includes('tower');
      return `<ellipse cy="${h * .36}" rx="${w * .43}" ry="${h * .12}" fill="#253c2625"/><rect x="${-w * .29}" y="${-h * .18}" width="${w * .58}" height="${h * .56}" rx=".04" ${base}/><path d="M ${-w * .43} ${-h * .13} L 0 ${-h * .47} L ${w * .43} ${-h * .13} Z" fill="${tower ? '#665981' : '#657b52'}" stroke="#3e4b3a" stroke-width=".07"/><path d="M -.12 ${h * .38} V ${h * .1} Q 0 ${-h * .01} .12 ${h * .1} V ${h * .38}" fill="#485444"/>${module.id === 'healing-station' ? `<path d="M 0 ${-h * .05} V ${h * .15} M ${-w * .1} ${h * .05} H ${w * .1}" stroke="#f3f0d7" stroke-width=".1"/>` : `<rect x="${-w * .09}" y="${-h * .1}" width="${w * .18}" height="${h * .13}" fill="#ece0ac"/>`}`;
    }
    if (module.id === 'mountain') return `<rect x="${-hw}" y="${-hh}" width="${w}" height="${h}" rx=".6" ${base}/><path d="M ${-hw*.9} ${hh*.8} L ${-hw*.4} ${-hh*.7} H ${hw*.5} L ${hw*.9} ${hh*.8}Z" fill="#cbbb94" stroke="#70664f" stroke-width=".12"/><path d="M ${-hw*.4} ${-hh*.7} L ${-hw*.2} ${-hh*.9} H ${hw*.35} L ${hw*.5} ${-hh*.7}Z" fill="#e7dab9"/>`;
    if (module.id === 'campfire') return `<ellipse rx="${w*.45}" ry="${h*.4}" fill="#858073" stroke="#565646" stroke-width=".14"/><path d="M ${-w*.3} ${h*.25} L ${w*.3} ${-h*.05} M ${-w*.3} ${-h*.05} L ${w*.3} ${h*.25}" stroke="#705238" stroke-width=".23"/><path d="M ${-w*.21} ${h*.16} Q ${-w*.3} ${-h*.05} ${-w*.05} ${-h*.3} L 0 ${-h*.47} Q ${w*.3} ${-h*.06} ${w*.2} ${h*.17}Z" fill="#e49a45" stroke="#a96535" stroke-width=".08"/><path d="M ${-w*.08} ${h*.15} Q 0 ${-h*.3} ${w*.09} ${h*.15}Z" fill="#f7d483"/>`;
    if (module.role === 'unit') {
      const r = Math.min(w, h) * .25;
      const skull = module.id.includes('skeleton') || module.id === 'necromancer';
      const crown = module.id === 'skeleton-king' || module.id === 'knight';
      return `<ellipse cy="${h * .33}" rx="${w * .43}" ry="${h * .13}" fill="#253c2625"/><path d="M ${-w * .37} ${h * .33} Q ${-w * .31} ${-h * .08} 0 ${-h * .04} Q ${w * .31} ${-h * .08} ${w * .37} ${h * .33}Z" ${base}/><circle cy="${-h * .19}" r="${r}" fill="${skull ? '#eee1be' : '#dfcba3'}" stroke="#465040" stroke-width=".06"/>${skull ? `<circle cx="${-r * .36}" cy="${-h * .2}" r="${r * .16}" fill="#585c47"/><circle cx="${r * .36}" cy="${-h * .2}" r="${r * .16}" fill="#585c47"/>` : ''}${crown ? `<path d="M ${-r} ${-h * .38} l 0 ${-h * .13} ${r * .6} ${h * .08} ${r * .4} ${-h * .13} ${r * .4} ${h * .13} ${r * .6} ${-h * .08} v ${h * .13} Z" fill="#cda85f" stroke="#765f33" stroke-width=".035"/>` : ''}${module.id.includes('archer') || module.id === 'ranger' ? `<path d="M ${w * .36} ${-h * .24} Q ${w * .76} 0 ${w * .36} ${h * .25} Z" fill="none" stroke="#746642" stroke-width=".06"/>` : `<path d="M ${w * .4} ${h * .25} V ${-h * .21}" stroke="#66634b" stroke-width=".07"/>`}`;
    }
    return `<rect x="${-hw}" y="${-hh}" width="${w}" height="${h}" rx=".15" ${base}/>`;
  }
  function icon(module) {
    const w = module.id.startsWith('wall-') ? 3 : 2, h = module.id.startsWith('wall-') ? .5 : 1.9;
    return `<svg viewBox="-1.9 -1.4 3.8 2.8" aria-hidden="true">${symbolMarkup(module, w, h)}</svg>`;
  }
  function renderPalette() {
    const categories = ['全部', ...new Set(M.MODULES.map(m => m.category))];
    const tabs = $('category-tabs'); tabs.replaceChildren();
    categories.forEach(category => {
      const button = document.createElement('button'); button.className = 'category-tab' + (category === state.category ? ' active' : '');
      button.textContent = category; button.setAttribute('aria-pressed', String(category === state.category));
      button.addEventListener('click', () => { state.category = category; renderPalette(); }); tabs.append(button);
    });
    const visible = M.MODULES.filter(m => (state.category === '全部' || m.category === state.category) && (m.label + ' ' + m.id + ' ' + (m.unitId || '')).toLowerCase().includes(state.search));
    $('module-count').textContent = visible.length;
    const library = $('module-library'), previousScroll = library.scrollTop; library.replaceChildren();
    [...new Set(visible.map(m => m.category))].forEach(category => {
      const group = document.createElement('section'); group.className = 'module-group';
      const heading = document.createElement('h3'); heading.textContent = category;
      const total = document.createElement('span'); total.textContent = visible.filter(m => m.category === category).length + ' 种积木'; heading.append(total); group.append(heading);
      const grid = document.createElement('div'); grid.className = 'module-grid';
      visible.filter(m => m.category === category).forEach(m => {
        const button = document.createElement('button'); button.type = 'button'; button.className = 'module-card' + (state.placement === m.id ? ' active' : '');
        button.draggable = true; button.dataset.moduleId = m.id; button.setAttribute('aria-pressed', String(state.placement === m.id));
        button.title = `${m.label} · ${m.defaultWidth} × ${m.defaultHeight} 米\n${m.implementation === 'runtime' ? '已接入游戏；导入关卡后生效。' : m.implementation === 'existing' ? '已有游戏单位。' : '仅作设计标注，不改变游戏规则。'}\n拖到画布，或点击后在地图放置。`;
        button.innerHTML = `<span class="module-symbol">${icon(m)}</span><span class="module-name">${escapeXML(m.label)}</span><span class="module-sub">${m.defaultWidth} × ${m.defaultHeight} 米</span>${m.implementation === 'design' ? '<span class="module-design-mark" title="设计意图">◇</span>' : ''}`;
        button.addEventListener('click', () => { state.panel = 'layout'; state.pickWaveId = null; state.placement = m.id; state.selectedId = null; renderPalette(); render(); });
        button.addEventListener('dragstart', event => { event.dataTransfer.setData('application/x-lk-module', m.id); event.dataTransfer.setData('text/plain', m.id); event.dataTransfer.effectAllowed = 'copy'; });
        grid.append(button);
      });
      group.append(grid); library.append(group);
    });
    if (!visible.length) { const empty = document.createElement('p'); empty.className = 'empty-library'; empty.textContent = '没有找到这个积木，试试其他名称。'; library.append(empty); }
    library.scrollTop = previousScroll;
  }
  function viewBox() {
    const p = state.project, margin = 1.4, fullW = p.field.width + margin * 2, fullH = p.field.height + margin * 2;
    const w = fullW / state.zoom, h = fullH / state.zoom;
    return [p.field.width / 2 + state.panX - w / 2, p.field.height / 2 + state.panY - h / 2, w, h];
  }
  function renderCanvas(labelScaleOverride) {
    const p = state.project, W = p.field.width, H = p.field.height, grid = p.field.grid;
    svg.setAttribute('viewBox', viewBox().join(' ')); svg.replaceChildren();
    svg.classList.toggle('mode-place', Boolean(state.placement)); svg.classList.toggle('mode-pan', state.space);
    const defs = element('defs');
    const pattern = element('pattern', { id: 'battle-grid', width: grid, height: grid, patternUnits: 'userSpaceOnUse' });
    pattern.append(element('path', { d: `M ${grid} 0 H 0 V ${grid}`, fill: 'none', stroke: '#4d684e', 'stroke-opacity': '.14', 'stroke-width': '.025' })); defs.append(pattern); svg.append(defs);
    svg.append(element('rect', { x: '.12', y: '.16', width: W, height: H, rx: '.13', fill: '#6b7c5530' }));
    svg.append(element('rect', { x: 0, y: 0, width: W, height: H, rx: '.12', fill: '#d3dec0', stroke: '#7a9365', 'stroke-width': '.07', 'data-background': 'true' }));
    const rank = { terrain: 0, zone: 1, decoration: 2, obstacle: 3, building: 4, unit: 5, note: 6 };
    const objects = p.objects.slice().sort((a,b) => (rank[M.getModule(a.moduleId).role] - rank[M.getModule(b.moduleId).role]));
    objects.filter(o => rank[M.getModule(o.moduleId).role] < 2).forEach(o => svg.append(renderObject(o)));
    svg.append(element('rect', { x: 0, y: 0, width: W / 2, height: H, fill: '#7a9f6538', 'pointer-events': 'none' }));
    svg.append(element('rect', { x: W / 2, y: 0, width: W / 2, height: H, fill: '#bfa06b14', 'pointer-events': 'none' }));
    svg.append(element('rect', { x: 0, y: 0, width: W, height: H, fill: 'url(#battle-grid)', 'pointer-events': 'none' }));
    svg.append(element('path', { d: `M ${W / 2} 0 V ${H}`, stroke: '#c5a037', 'stroke-width': '.09', 'stroke-dasharray': '.5 .28', 'pointer-events': 'none' }));
    objects.filter(o => rank[M.getModule(o.moduleId).role] >= 2).forEach(o => svg.append(renderObject(o)));
    renderLabels(objects, labelScaleOverride);
    if (state.panel === 'waves') p.waves.forEach((wave, index) => {
      const selected = wave.id === state.selectedWaveId;
      const group = element('g', { 'data-wave-id': wave.id, cursor: 'pointer' });
      group.append(element('circle', { cx: wave.x, cy: wave.y, r: Math.max(.4, wave.radius), fill: '#b15f5320', stroke: selected ? '#8e432d' : '#b07759', 'stroke-width': selected ? .12 : .07, 'stroke-dasharray': '.3 .2' }));
      group.append(element('circle', { cx: wave.x, cy: wave.y, r: .48, fill: '#f4e5be', stroke: '#87573c', 'stroke-width': .08 }));
      group.append(element('text', { x: wave.x, y: wave.y + .15, 'text-anchor': 'middle', 'font-size': '.43', fill: '#71452c', 'font-weight': 700 }, index + 1)); svg.append(group);
    });
    // Coordinate ticks are kept outside the playable rectangle.
    for (let x = 0; x <= W; x += W <= 50 ? 5 : 10) svg.append(element('text', { x, y: H + .65, 'text-anchor': 'middle', fill: '#718262', 'font-size': '.35', 'font-family': 'sans-serif', 'pointer-events': 'none' }, x));
    for (let y = 0; y <= H; y += H <= 50 ? 4 : 10) svg.append(element('text', { x: -.38, y: y + .12, 'text-anchor': 'end', fill: '#718262', 'font-size': '.35', 'font-family': 'sans-serif', 'pointer-events': 'none' }, y));
    $('empty-hint').hidden = p.objects.length > 0;
    $('zoom-reset').textContent = Math.round(state.zoom * 100) + '%';
    $('grid-label').textContent = grid; $('object-count').textContent = p.objects.length + ' 个积木';
  }
  function renderObject(o) {
    const module = M.getModule(o.moduleId), isSelected = o.id === state.selectedId;
    const group = element('g', { 'data-object-id': o.id, class: 'map-object' + (isSelected ? ' selected' : ''), transform: `translate(${o.x} ${o.y})`, role: 'graphics-symbol', 'aria-label': `${o.label}，${TEAM_NAMES[o.team]}，位置 ${o.x}, ${o.y} 米` });
    group.append(element('title', {}, `${o.label} · ${TEAM_NAMES[o.team]}${o.count > 1 ? ' ×' + o.count : ''}${o.spawnAtSeconds ? ' · ' + o.spawnAtSeconds + ' 秒出现' : ''}${o.note ? '\n' + o.note : ''}`));
    const rotated = element('g', { transform: `rotate(${o.rotation})`, class: 'object-main' });
    rotated.innerHTML = symbolMarkup(module, o.width, o.height);
    // A minimum transparent hit area keeps thin walls and small soldiers selectable.
    rotated.append(element('rect', { x: -Math.max(o.width, .8) / 2, y: -Math.max(o.height, .8) / 2, width: Math.max(o.width, .8), height: Math.max(o.height, .8), fill: 'transparent', stroke: 'none' }));
    if (module.implementation === 'design' && !['zone', 'note', 'terrain'].includes(module.role)) rotated.append(element('rect', { x: -o.width / 2, y: -o.height / 2, width: o.width, height: o.height, fill: 'none', stroke: '#7d8266', 'stroke-width': '.035', 'stroke-dasharray': '.13 .13', 'pointer-events': 'none' }));
    if (isSelected) {
      rotated.append(element('rect', { x: -o.width / 2 - .16, y: -o.height / 2 - .16, width: o.width + .32, height: o.height + .32, rx: '.06', fill: 'none', stroke: '#fef9d9', 'stroke-width': '.15', 'pointer-events': 'none' }));
      rotated.append(element('rect', { x: -o.width / 2 - .16, y: -o.height / 2 - .16, width: o.width + .32, height: o.height + .32, rx: '.06', fill: 'none', stroke: '#a68537', 'stroke-width': '.055', 'pointer-events': 'none' }));
      [[-1,-1],[1,-1],[-1,1],[1,1]].forEach(([sx,sy]) => rotated.append(element('rect', { x: sx * (o.width / 2 + .16) - .085, y: sy * (o.height / 2 + .16) - .085, width: '.17', height: '.17', fill: '#fbf7d8', stroke: '#997b36', 'stroke-width': '.04', 'pointer-events': 'none' })));
    }
    group.append(rotated);
    if (['unit', 'building'].includes(module.role)) {
      group.append(element('circle', { cx: -o.width / 2 - .1, cy: -o.height / 2 + .12, r: '.12', fill: TEAM_COLORS[o.team], stroke: '#f3f4dd', 'stroke-width': '.045', 'pointer-events': 'none' }));
    }
    return group;
  }
  function renderLabels(objects, scaleOverride) {
    // Labels are a separate annotation layer: screen-readable typography never
    // enlarges an object's physical footprint or the coordinates in its file.
    const scale = Math.max(.1, scaleOverride || (svg.getScreenCTM() && svg.getScreenCTM().a) || 12);
    const font = 11 / scale, secondaryFont = 10 / scale, pad = 3 / scale, gap = 3 / scale;
    const occupied = [], overlay = element('g', { class: 'label-overlay' });
    const textWidth = (text, size) => Array.from(text).reduce((sum, char) => sum + (char.charCodeAt(0) > 255 ? 1 : .62), 0) * size;
    const overlaps = (a,b) => a.x < b.x + b.width + gap && a.x + a.width + gap > b.x && a.y < b.y + b.height + gap && a.y + a.height + gap > b.y;
    const ordered = objects.slice().sort((a,b) => Number(b.id === state.selectedId) - Number(a.id === state.selectedId));
    ordered.forEach(o => {
      const m = M.getModule(o.moduleId), isSelected = o.id === state.selectedId;
      if (m.role === 'terrain' && !isSelected) return;
      const name = short(o.label, isSelected ? 18 : 10), info = [];
      if (['unit','building'].includes(m.role)) {
        if (o.count > 1) info.push('×' + o.count);
        if (o.spawnAtSeconds > 0) info.push(o.spawnAtSeconds + ' 秒');
      }
      if (m.role === 'note' && isSelected && o.note) info.push(short(o.note.replace(/\n/g, ' '), 18));
      const detail = info.join(' · '), lineH = font * 1.3;
      const width = Math.max(textWidth(name, font), textWidth(detail, secondaryFont)) + pad * 2;
      const height = lineH + pad * 2 + (detail ? secondaryFont * 1.35 : 0);
      const angle = o.rotation * Math.PI / 180;
      const halfH = Math.abs(Math.sin(angle)) * o.width / 2 + Math.abs(Math.cos(angle)) * o.height / 2;
      let top = o.y + halfH + gap;
      if (m.role === 'zone') top = o.y - o.height / 2 + gap;
      if (m.role === 'note' || m.role === 'terrain') top = o.y - height / 2;
      const origin = { x: o.x - width / 2, y: top, width, height };
      const candidates = [origin];
      for (let i = 1; i <= 5; i++) {
        candidates.push({ ...origin, y: top + i * (height + gap) });
        candidates.push({ ...origin, y: o.y - halfH - i * (height + gap) });
      }
      candidates.push({ ...origin, x: o.x + o.width / 2 + gap, y: o.y - height / 2 });
      candidates.push({ ...origin, x: o.x - o.width / 2 - width - gap, y: o.y - height / 2 });
      const inside = r => r.x >= .1 && r.y >= .1 && r.x + r.width <= state.project.field.width - .1 && r.y + r.height <= state.project.field.height - .1;
      let rect = candidates.find(r => inside(r) && !occupied.some(other => overlaps(r, other)));
      // Dense formations retain their markers; selecting one always reveals
      // its name at readable size, while a hover title contains full details.
      if (!rect && !isSelected) return;
      if (!rect) rect = { ...origin, x: Math.max(.1, Math.min(state.project.field.width - width - .1, origin.x)), y: Math.max(.1, Math.min(state.project.field.height - height - .1, origin.y)) };
      occupied.push(rect);
      const group = element('g', { 'data-object-id': o.id, class: 'map-object map-label' + (isSelected ? ' selected' : '') });
      group.append(element('title', {}, `${o.label}${o.note ? '\n' + o.note : ''}`));
      if (Math.abs(rect.y - top) > gap || Math.abs(rect.x - origin.x) > gap) group.append(element('path', { d: `M ${o.x} ${o.y} L ${rect.x + rect.width / 2} ${rect.y + rect.height / 2}`, fill: 'none', stroke: isSelected ? '#ad8742' : '#768569', 'stroke-width': 1 / scale, 'stroke-dasharray': `${2 / scale} ${2 / scale}`, 'pointer-events': 'none' }));
      group.append(element('rect', { x: rect.x, y: rect.y, width: rect.width, height: rect.height, rx: 3 / scale, fill: isSelected ? '#fff3c9' : m.role === 'note' ? '#fff2bf' : '#f8f8e9', 'fill-opacity': '.95', stroke: isSelected ? '#b38d3e' : '#c4d1b4', 'stroke-width': .7 / scale }));
      const center = rect.x + rect.width / 2;
      group.append(element('text', { x: center, y: rect.y + pad + font, 'text-anchor': 'middle', fill: '#344e38', 'font-family': 'Microsoft YaHei, sans-serif', 'font-size': font, 'font-weight': isSelected || m.role === 'note' ? '600' : '500', class: 'object-label' }, name));
      if (detail) group.append(element('text', { x: center, y: rect.y + pad + lineH + secondaryFont, 'text-anchor': 'middle', fill: TEAM_COLORS[o.team], 'font-family': 'Microsoft YaHei, sans-serif', 'font-size': secondaryFont, class: 'object-label' }, detail));
      overlay.append(group);
    });
    // Draw the selected label last, above neighboring annotations.
    const current = overlay.querySelector('.selected'); if (current) overlay.append(current);
    svg.append(overlay);
  }
  function inputField(label, key, value, attributes, hint) {
    const wrap = document.createElement('label'); wrap.className = 'form-field';
    const name = document.createElement('span'); name.textContent = label; wrap.append(name);
    const input = document.createElement(attributes && attributes.multiline ? 'textarea' : 'input');
    input.dataset.property = key; input.value = value;
    Object.entries(attributes || {}).forEach(([key, v]) => { if (key !== 'multiline') input.setAttribute(key, String(v)); });
    input.addEventListener('change', () => changeProperty(key, input)); wrap.append(input);
    if (hint) { const small = document.createElement('small'); small.textContent = hint; wrap.append(small); }
    return wrap;
  }
  function selectField(label, key, value, choices, disabled) {
    const wrap = document.createElement('label'); wrap.className = 'form-field';
    const name = document.createElement('span'); name.textContent = label; wrap.append(name);
    const input = document.createElement('select'); input.dataset.property = key; input.disabled = Boolean(disabled);
    choices.forEach(([val, name]) => { const option = document.createElement('option'); option.value = val; option.textContent = name; input.append(option); });
    input.value = String(value); input.addEventListener('change', () => changeProperty(key, input)); wrap.append(input); return wrap;
  }
  function row(...fields) { const node = document.createElement('div'); node.className = 'form-row'; fields.forEach(f => node.append(f)); return node; }
  function section(label) { const node = document.createElement('p'); node.className = 'section-label'; node.textContent = label; return node; }
  function hint(text) { const node = document.createElement('p'); node.className = 'selection-hint'; node.textContent = text; return node; }
  function actionButton(text, action, className) { const b = document.createElement('button'); b.type = 'button'; b.className = className || 'secondary'; b.textContent = text; b.addEventListener('click', action); return b; }
  function selectedWave() { return state.project.waves.find(w => w.id === state.selectedWaveId); }
  function renderWaves(inspector) {
    $('inspector-heading').textContent = '敌军增援波次'; $('selection-badge').textContent = state.project.waves.length + ' 波';
    inspector.append(hint('事件增援独立于银币出牌，每条波次只执行一次。时间从正式开战起计算；条件满足后仅登记一次延迟。'));
    const add = actionButton('＋ 添加波次', () => mutate(() => { const w = M.createWave(state.project.field.width * .8, state.project.field.height / 2); state.project.waves.push(w); state.selectedWaveId = w.id; }));
    add.disabled = state.project.waves.length >= M.MAX_WAVES; inspector.append(add);
    const list = document.createElement('div'); list.className = 'wave-list';
    state.project.waves.forEach((w, index) => {
      const b = actionButton((index + 1) + '. ' + w.label, () => { state.selectedWaveId = w.id; state.pickWaveId = null; render(); }, 'wave-list-item' + (w.id === state.selectedWaveId ? ' active' : ''));
      b.title = w.label; list.append(b);
    }); inspector.append(list);
    const w = selectedWave();
    if (!w) { inspector.append(hint('选择一条波次编辑，或添加新的增援。地图上的圆圈为分散范围，圆心可通过“在地图选落点”修改。')); return; }
    inspector.append(inputField('波次名称', 'wave.label', w.label, { maxlength: 100 }));
    inspector.append(selectField('触发方式', 'wave.trigger', w.trigger, [['time','开战后指定时间'], ['hero-health-below','英雄首次低于指定血量'], ['hero-first-death','英雄首次死亡']]));
    if (w.trigger !== 'time') {
      inspector.append(selectField('条件英雄', 'wave.targetId', w.targetId, M.triggerHeroes(state.project).map(o => [o.id, o.label + ' · ' + TEAM_NAMES[o.team] + ' (' + o.x + ', ' + o.y + ')'])));
      inspector.append(hint('仅能引用单独放置、开局出现的英雄。删除或改变该英雄数量前，须先修改或删除引用它的波次。'));
      if (w.trigger === 'hero-health-below') inspector.append(inputField('生命阈值 / %', 'wave.healthPercent', round(w.healthThreshold * 100), { type: 'number', min: 1, max: 99, step: 1 }));
    }
    inspector.append(inputField(w.trigger === 'time' ? '开战后 / 秒' : '条件触发后延迟 / 秒', 'wave.time', w.time, { type: 'number', min: 0, max: 3600, step: .5 }));
    inspector.append(selectField('增援兵种', 'wave.unitId', w.unitId, M.WAVE_UNITS.map(u => [u.id, u.label])));
    inspector.append(row(inputField('生成数量', 'wave.count', w.count, { type: 'number', min: 1, max: 50, step: 1 }), inputField('分散半径 / 米', 'wave.radius', w.radius, { type: 'number', min: 0, max: 10, step: .25 })));
    inspector.append(row(inputField('落点 X / 米', 'wave.x', w.x, { type: 'number', min: 0, max: state.project.field.width, step: .25 }), inputField('落点 Y / 米', 'wave.y', w.y, { type: 'number', min: 0, max: state.project.field.height, step: .25 })));
    inspector.append(actionButton(state.pickWaveId === w.id ? '取消选点（或 Esc）' : '在地图选落点', () => { state.pickWaveId = state.pickWaveId === w.id ? null : w.id; state.placement = null; render(); }));
    inspector.append(hint('波次全部属于敌方；空间不足时按运行时可用位置生成。条件波次引用的是当前地图中的这一个英雄实例。'));
    inspector.append(actionButton('删除本波次', () => mutate(() => { state.project.waves = state.project.waves.filter(v => v.id !== w.id); state.selectedWaveId = null; state.pickWaveId = null; }), 'danger'));
  }
  function renderEnemy(inspector) {
    const e = state.project.enemy;
    $('inspector-heading').textContent = '敌方牌组与银币'; $('selection-badge').textContent = e.cards.length + ' 张';
    inspector.append(hint('敌方持续回复银币并部署所选卡牌。时间 / 条件波次不消耗银币。暂沿用现有 AI 出牌策略。'));
    inspector.append(selectField('敌方主动出牌', 'enemy.usesCards', String(e.usesCards), [['true','开启'], ['false','关闭（仅地图与波次敌军）']]));
    inspector.append(row(inputField('每秒银币', 'enemy.silverPerSecond', e.silverPerSecond, { type: 'number', min: 0, max: 100, step: .1 }), inputField('银币上限', 'enemy.silverCap', e.silverCap, { type: 'number', min: 1, max: 1000, step: 1 })));
    inspector.append(inputField('初始银币', 'enemy.startingSilver', e.startingSilver, { type: 'number', min: 0, max: e.silverCap, step: 1 }));
    inspector.append(hint('同类卡最多选择一次。关闭出牌会保留已选卡牌；开关、经济和牌组均可撤销并随 JSON 保存。'));
    ['佣兵', '法术', '建筑'].forEach(kind => {
      const details = document.createElement('details'); details.className = 'card-catalog'; details.open = true;
      const summary = document.createElement('summary'); summary.textContent = kind + ' · ' + M.CARDS.filter(c => c.kind === kind && e.cards.includes(c.id)).length + ' 已选'; details.append(summary);
      M.CARDS.filter(c => c.kind === kind).forEach(card => {
        const label = document.createElement('label'); label.className = 'card-choice';
        const box = document.createElement('input'); box.type = 'checkbox'; box.checked = e.cards.includes(card.id);
        box.addEventListener('change', () => mutate(() => { if (box.checked) state.project.enemy.cards.push(card.id); else state.project.enemy.cards = state.project.enemy.cards.filter(id => id !== card.id); }));
        const text = document.createElement('span'); text.textContent = card.label; text.title = card.id;
        label.append(box, text); details.append(label);
      }); inspector.append(details);
    });
  }
  function renderInspector() {
    const inspector = $('inspector'), scroll = inspector.scrollTop; inspector.replaceChildren();
    if (state.panel === 'waves') { renderWaves(inspector); inspector.scrollTop = scroll; return; }
    if (state.panel === 'enemy') { renderEnemy(inspector); inspector.scrollTop = scroll; return; }
    const o = selected();
    $('inspector-heading').textContent = o ? '积木属性' : '战场设置'; $('selection-badge').textContent = o ? M.getModule(o.moduleId).category : '全局';
    if (o) {
      const m = M.getModule(o.moduleId), type = document.createElement('div'); type.className = 'object-type-row';
      type.innerHTML = `<span class="object-swatch" style="background:${escapeXML(m.color)}44">${m.role === 'unit' ? '♟' : m.role === 'building' ? '♜' : '◇'}</span><div><strong>${escapeXML(m.label)}</strong><p>${m.implementation === 'runtime' ? '游戏模块 · 导入后生效' : m.implementation === 'existing' ? '已有游戏单位' : '仅作设计标注'}</p></div>`; inspector.append(type);
      const terrainHints = { brush: '同一连通草丛内互相可见；向草丛外攻击时，自身与 2 米内友方草丛单位暴露 3 秒。', swamp: '区域内单位移速降低 30%；单位免疫或反向加速由游戏单位能力配置。', campfire: '有实体体积的战场装饰，不可攻击；单位须绕行。', mountain: '不可通行高台，无上下山路径。放在其上的单位固定驻守，地面近战无法攻击，远程和法术正常命中。', grass: '草地为静态地面装饰，不提供草丛隐身。', road: '道路为静态地面装饰，不加速。', water: '水域为静态地面装饰，不阻挡、不减速；需要减速请使用沼泽。', flowers: '花丛为可穿行的静态装饰，不提供隐身。', rock: '岩石为不可破坏的实体障碍，单位需绕行。', tree: '树木为不可破坏的实体障碍，单位需绕行。', 'player-zone': '实际限制开局我方英雄的部署位置；仍遵守己方半场和碰撞规则。', 'enemy-zone': '仅标注敌方出现区域，不自动生成敌军；请用地图单位或增援波次指定实际落点。', note: '仅用于设计沟通，不生成战斗对象。' };
      if (terrainHints[m.id]) inspector.append(hint(terrainHints[m.id]));
      if (m.implementation === 'design') { const note = document.createElement('p'); note.className = 'design-notice'; note.textContent = '这块积木仅作设计标注，不自动改变碰撞、出兵或战斗行为。'; inspector.append(note); }
      inspector.append(inputField('显示名称', 'label', o.label, { maxlength: 100 }));
      inspector.append(row(inputField('X 位置 / 米', 'x', o.x, { type: 'number', min: 0, max: 100, step: 'any' }), inputField('Y 位置 / 米', 'y', o.y, { type: 'number', min: 0, max: 100, step: 'any' })));
      inspector.append(row(inputField('宽度 / 米', 'width', o.width, { type: 'number', min: .1, max: 100, step: 'any' }), inputField('高度 / 米', 'height', o.height, { type: 'number', min: .1, max: 100, step: 'any' })));
      const angle = inputField('顺时针旋转 / 度', 'rotation', o.rotation, { type: 'number', min: -3600, max: 3600, step: 'any' });
      const controls = document.createElement('div'); controls.className = 'rotation-actions'; const input = angle.querySelector('input'); input.remove();
      const left = document.createElement('button'); left.type = 'button'; left.textContent = '↶ Q'; left.title = '逆时针 15°'; left.addEventListener('click', () => rotate(-15));
      const right = document.createElement('button'); right.type = 'button'; right.textContent = 'E ↷'; right.title = '顺时针 15°'; right.addEventListener('click', () => rotate(15)); controls.append(left, input, right); angle.append(controls); inspector.append(angle);
      let teams = Object.entries(TEAM_NAMES);
      if (['unit','building'].includes(m.role)) teams = teams.filter(([key]) => key !== 'neutral');
      if (ENEMY_ONLY.has(m.id) || m.role === 'zone') teams = teams.filter(([key]) => key === m.defaultTeam);
      inspector.append(selectField('所属阵营', 'team', o.team, teams, teams.length === 1));
      if (['unit','building'].includes(m.role)) inspector.append(row(inputField('数量', 'count', o.count, { type: 'number', min: 1, max: 20, step: 1 }), inputField('出现时间 / 秒', 'spawnAtSeconds', o.spawnAtSeconds, { type: 'number', min: 0, max: 300, step: .5 })));
      inspector.append(inputField('设计备注', 'note', o.note, { multiline: true, maxlength: 4000, placeholder: '这里如何行动？为什么这样布置？' }));
      const actions = document.createElement('div'); actions.className = 'inspector-actions';
      const duplicate = document.createElement('button'); duplicate.textContent = '复制 Ctrl+D'; duplicate.addEventListener('click', duplicateSelected);
      const remove = document.createElement('button'); remove.textContent = '删除 Delete'; remove.className = 'danger'; remove.addEventListener('click', deleteSelected); actions.append(duplicate, remove); inspector.append(actions);
      const id = document.createElement('p'); id.className = 'selected-id'; id.textContent = m.unitId ? '游戏单位：' + m.unitId : '模块：' + m.id; inspector.append(id);
    } else {
      const hint = document.createElement('p'); hint.className = 'selection-hint'; hint.textContent = state.placement ? '正在放置「' + M.getModule(state.placement).label + '」。点击地图放置，按 Esc 返回选择。' : '点击地图中的积木，编辑它的位置、阵营和行为说明。'; inspector.append(hint);
      inspector.append(inputField('地图名称', 'meta.title', state.project.meta.title, { maxlength: 120 }));
      inspector.append(inputField('关卡设计说明', 'meta.description', state.project.meta.description, { multiline: true, maxlength: 4000, placeholder: '期望的战斗体验、胜利条件、路线与节奏…' }));
      inspector.append(section('场地尺寸'));
      inspector.append(row(inputField('宽度 / 米', 'field.width', state.project.field.width, { type: 'number', min: 8, max: 100, step: 1 }), inputField('高度 / 米', 'field.height', state.project.field.height, { type: 'number', min: 8, max: 100, step: 1 })));
      inspector.append(selectField('网格大小', 'field.grid', state.project.field.grid, M.GRIDS.map(v => [String(v), v + ' 米'])));
      inspector.append(section('战斗意图'));
      inspector.append(selectField('遭遇模板', 'settings.encounterId', state.project.settings.encounterId, [...[['Patrol','普通遭遇'], ['Elite','精英战'], ['Boss','首领战']], ...(!['Patrol','Elite','Boss'].includes(state.project.settings.encounterId) ? [[state.project.settings.encounterId, state.project.settings.encounterId]] : [])]));
      inspector.append(row(inputField('随机种子', 'settings.seed', state.project.settings.seed, { type: 'number', min: 0, max: 2147483647, step: 1 }), inputField('战斗时限 / 秒', 'settings.battleTimeLimit', state.project.settings.battleTimeLimit, { type: 'number', min: 1, max: 3600, step: 1 })));
      const note = document.createElement('p'); note.className = 'selection-hint'; note.textContent = '默认战场 40 × 24 米。试玩脚本会读取尺寸并同步边界与镜头；敌方出现时间直接生成波次。'; inspector.append(note);
    }
    inspector.scrollTop = scroll;
  }
  function changeProperty(key, input) {
    const numeric = input.type === 'number' || key === 'field.grid';
    const value = numeric ? Number(input.value) : input.value;
    if ((numeric && (input.value.trim() === '' || !Number.isFinite(value))) || !input.checkValidity()) { toast('请输入此项允许的数值或文本。', true); renderInspector(); return; }
    if ((key === 'label' || key === 'meta.title') && !String(value).trim()) { toast('名称不能为空。', true); renderInspector(); return; }
    mutate(() => {
      if (key.startsWith('wave.')) {
        const w = selectedWave(); if (!w) return;
        const property = key.slice(5);
        if (property === 'healthPercent') w.healthThreshold = round(value / 100);
        else if (property === 'trigger') {
          w.trigger = value;
          if (value === 'time') w.targetId = '';
          else if (!M.triggerHeroes(state.project).some(o => o.id === w.targetId)) {
            const hero = M.triggerHeroes(state.project)[0];
            if (!hero) throw new Error('请先在地图放置一名单独出现（数量 1、出现时间 0）的英雄。');
            w.targetId = hero.id;
          }
        } else w[property] = value;
      }
      else if (key.includes('.')) { const [group, property] = key.split('.'); state.project[group][property] = key === 'enemy.usesCards' ? value === 'true' : value; if (group === 'field') resetView(false); }
      else if (selected()) selected()[key] = numeric ? round(value) : value;
    });
  }
  function renderValidation() {
    const result = M.validate(state.project), list = $('validation-list'); list.replaceChildren();
    const count = $('validation-count'); count.className = 'check-badge' + (result.errors.length ? ' error' : result.warnings.length ? ' warning' : '');
    count.textContent = result.errors.length ? result.errors.length + ' 项待修正' : result.warnings.length ? result.warnings.length + ' 条提醒' : '可交付';
    const items = [...result.errors.map(message => ({ type: 'error', message })), ...result.warnings.map(message => ({ type: 'warning', message }))];
    if (!items.length) items.push({ type: 'success', message: '布局结构完整，可以下载并交付设计。战斗数值与通行性仍需在游戏中验收。' });
    items.forEach(item => {
      const message = typeof item.message === 'string' ? item.message : item.message.message || String(item.message);
      const match = message.match(/^对象 (\d+)：/), object = match ? state.project.objects[Number(match[1]) - 1] : null;
      const node = document.createElement(object ? 'button' : 'div'); node.className = 'validation-item ' + item.type;
      node.innerHTML = `<span aria-hidden="true">${item.type === 'error' ? '!' : item.type === 'warning' ? '◇' : '✓'}</span><span>${escapeXML(message)}</span>`;
      if (object) { node.dataset.objectId = object.id; node.title = '选中对应积木'; node.addEventListener('click', () => { state.selectedId = object.id; state.placement = null; render(); renderPalette(); }); }
      list.append(node);
    });
  }
  function render(options) {
    const p = state.project;
    $('canvas-title').textContent = p.meta.title; document.title = p.meta.title + ' · 小小国王战场工坊';
    $('placement-status').textContent = state.pickWaveId ? '点击地图设置波次落点 · Esc 取消' : state.placement ? '放置：' + M.getModule(state.placement).label + ' · Esc 取消' : '拖动积木调整位置';
    ['layout', 'waves', 'enemy'].forEach(panel => { $('tab-' + panel).setAttribute('aria-pressed', String(state.panel === panel)); });
    $('tool-select').classList.toggle('active', !state.placement); $('tool-select').setAttribute('aria-pressed', String(!state.placement));
    $('undo').disabled = !state.history.length; $('redo').disabled = !state.future.length;
    renderCanvas(); if (!options || options.inspector !== false) renderInspector(); renderValidation();
    $('cursor-position').textContent = p.field.width + ' × ' + p.field.height + ' 米';
  }
  function canvasPoint(event) {
    const matrix = svg.getScreenCTM(); if (!matrix) return null;
    const point = svg.createSVGPoint(); point.x = event.clientX; point.y = event.clientY;
    const pos = point.matrixTransform(matrix.inverse()); return { x: pos.x, y: pos.y };
  }
  function snap(value) { return state.snap ? round(Math.round(value / state.project.field.grid) * state.project.field.grid) : round(value); }
  function fitObject(o, x, y) {
    const angle = o.rotation * Math.PI / 180, halfW = Math.abs(Math.cos(angle)) * o.width / 2 + Math.abs(Math.sin(angle)) * o.height / 2,
      halfH = Math.abs(Math.sin(angle)) * o.width / 2 + Math.abs(Math.cos(angle)) * o.height / 2;
    const W = state.project.field.width, H = state.project.field.height;
    // Round the legal interval inward; rounding an exact rotated edge outward
    // by half a millimetre would otherwise create a draft the model rejects.
    const minX = Math.ceil((halfW - 1e-9) * 1000) / 1000, maxX = Math.floor((W - halfW + 1e-9) * 1000) / 1000;
    const minY = Math.ceil((halfH - 1e-9) * 1000) / 1000, maxY = Math.floor((H - halfH + 1e-9) * 1000) / 1000;
    o.x = Math.max(Math.min(minX, W / 2), Math.min(maxX, snap(x)));
    o.y = Math.max(Math.min(minY, H / 2), Math.min(maxY, snap(y)));
  }
  function addObject(moduleId, point) {
    if (state.project.objects.length >= 500) { toast('最多可以放置 500 个积木。', true); return; }
    if (!point || point.x < 0 || point.y < 0 || point.x > state.project.field.width || point.y > state.project.field.height) { toast('请放在战场边界内。', true); return; }
    mutate(() => { const o = M.createObject(moduleId, point.x, point.y); fitObject(o, point.x, point.y); state.project.objects.push(o); state.selectedId = o.id; });
  }
  function duplicateSelected() {
    const o = selected(); if (!o) return;
    if (state.project.objects.length >= 500) { toast('最多可以放置 500 个积木。', true); return; }
    mutate(() => { const copy = Object.assign({}, clone(o), { id: M.createObject(o.moduleId, o.x, o.y).id }); fitObject(copy, o.x + state.project.field.grid, o.y + state.project.field.grid); state.project.objects.push(copy); state.selectedId = copy.id; });
  }
  function deleteSelected() { if (!selected()) return; mutate(() => { state.project.objects = state.project.objects.filter(o => o.id !== state.selectedId); state.selectedId = null; }); }
  function rotate(delta) { if (!selected()) return; mutate(() => { const o = selected(); o.rotation = ((o.rotation + delta) % 360 + 360) % 360; }); }
  function chooseSelect() { state.placement = null; state.pickWaveId = null; state.selectedId = null; render(); renderPalette(); }
  function resetView(renderNow) { state.zoom = 1; state.panX = 0; state.panY = 0; if (renderNow !== false) renderCanvas(); }
  function zoom(delta) { state.zoom = Math.max(.6, Math.min(3, Math.round((state.zoom + delta) * 10) / 10)); renderCanvas(); }
  function finishGesture(cancelled) {
    const gesture = state.gesture; if (!gesture) return;
    state.gesture = null; svg.classList.remove('dragging');
    if (gesture.type === 'object') {
      const errors = M.validate(state.project).errors;
      if (cancelled || errors.length) {
        state.project = gesture.before;
        if (!cancelled && errors.length) toast('未应用这次拖动：' + short(errors[0], 180), true);
      }
      else if (projectKey(gesture.before) !== projectKey(state.project)) pushHistory(gesture.before);
      render();
    }
  }
  svg.addEventListener('pointerdown', event => {
    if (event.button !== 0 && event.button !== 1) return;
    const point = canvasPoint(event); if (!point) return;
    event.preventDefault(); svg.focus({ preventScroll: true });
    if (state.space || event.button === 1) {
      state.gesture = { type: 'pan', startX: event.clientX, startY: event.clientY, panX: state.panX, panY: state.panY, scale: svg.getScreenCTM().a };
      svg.classList.add('dragging'); svg.setPointerCapture(event.pointerId); return;
    }
    if (state.pickWaveId) {
      if (point.x < 0 || point.y < 0 || point.x > state.project.field.width || point.y > state.project.field.height) { toast('请选择战场边界内的落点。', true); return; }
      mutate(() => { const w = state.project.waves.find(w => w.id === state.pickWaveId); if (w) { w.x = snap(point.x); w.y = snap(point.y); } state.pickWaveId = null; }); return;
    }
    const waveGroup = event.target.closest('[data-wave-id]');
    if (waveGroup) { state.selectedWaveId = waveGroup.dataset.waveId; state.panel = 'waves'; render(); return; }
    if (state.placement) { state.panel = 'layout'; addObject(state.placement, point); return; }
    const group = event.target.closest('[data-object-id]');
    if (group) {
      state.panel = 'layout'; state.selectedId = group.dataset.objectId; const o = selected();
      state.gesture = { type: 'object', before: clone(state.project), id: o.id, offsetX: point.x - o.x, offsetY: point.y - o.y, moved: false };
      svg.setPointerCapture(event.pointerId); render();
    } else { state.selectedId = null; render(); }
  });
  svg.addEventListener('pointermove', event => {
    const point = canvasPoint(event); if (!point) return;
    $('cursor-position').textContent = 'X ' + round(point.x).toFixed(1) + ' · Y ' + round(point.y).toFixed(1) + ' 米';
    const gesture = state.gesture; if (!gesture) return;
    if (gesture.type === 'pan') {
      state.panX = gesture.panX - (event.clientX - gesture.startX) / gesture.scale;
      state.panY = gesture.panY - (event.clientY - gesture.startY) / gesture.scale;
      state.panX = Math.max(-state.project.field.width, Math.min(state.project.field.width, state.panX));
      state.panY = Math.max(-state.project.field.height, Math.min(state.project.field.height, state.panY)); renderCanvas();
    } else {
      const o = selected(); if (!o) return;
      fitObject(o, point.x - gesture.offsetX, point.y - gesture.offsetY); gesture.moved = true;
      svg.classList.add('dragging'); renderCanvas();
      // Keep typing focus stable; the property panel refreshes when the drag ends.
    }
  });
  svg.addEventListener('pointerup', () => finishGesture(false));
  svg.addEventListener('pointercancel', () => finishGesture(true));
  svg.addEventListener('lostpointercapture', () => finishGesture(false));
  svg.addEventListener('pointerleave', () => { if (!state.gesture) $('cursor-position').textContent = state.project.field.width + ' × ' + state.project.field.height + ' 米'; });
  $('canvas-stage').addEventListener('dragover', event => { if (event.dataTransfer.types.includes('application/x-lk-module')) { event.preventDefault(); event.dataTransfer.dropEffect = 'copy'; $('canvas-stage').classList.add('drop-active'); } });
  $('canvas-stage').addEventListener('dragleave', event => { if (!$('canvas-stage').contains(event.relatedTarget)) $('canvas-stage').classList.remove('drop-active'); });
  $('canvas-stage').addEventListener('drop', event => {
    event.preventDefault(); $('canvas-stage').classList.remove('drop-active');
    const id = event.dataTransfer.getData('application/x-lk-module'); if (!M.getModule(id)) return;
    state.placement = null; addObject(id, canvasPoint(event)); renderPalette();
  });

  function download(contents, filename, type) {
    const blob = contents instanceof Blob ? contents : new Blob([contents], { type: type || 'application/json;charset=utf-8' });
    const url = URL.createObjectURL(blob), link = document.createElement('a'); link.href = url; link.download = filename; document.body.append(link); link.click(); link.remove();
    setTimeout(() => URL.revokeObjectURL(url), 30000);
  }
  function filename(extension, suffix) { return (state.project.meta.title.replace(/[<>:"/\\|?*\u0000-\u001F]/g, '_').trim().slice(0, 70) || '战场设计') + (suffix || '') + extension; }
  function exportProject() {
    try { download(M.serialize(state.project), filename('.json')); state.dirty = false; persist(); toast('设计 JSON 已下载。把这个文件发给我，就能继续制作关卡。'); return true; }
    catch (error) { toast('设计尚有需要修正的问题：\n' + short(error.message, 200), true); renderValidation(); return false; }
  }
  function confirmReplace(action, label) {
    const hasWork = state.dirty || state.project.objects.length > 0;
    if (!hasWork) { action(); return; }
    const dialog = $('confirm-dialog'); $('confirm-title').textContent = label || '替换当前设计？';
    $('confirm-message').textContent = state.dirty ? '当前有尚未下载的修改。此操作会替换画布和本地草稿，请先下载 JSON 保留设计。' : '此操作会替换当前画布及本地草稿。已下载的 JSON 不受影响，也可以先再下载一份。';
    const close = () => { dialog.close(); $('confirm-proceed').onclick = null; $('confirm-download').onclick = null; };
    $('confirm-cancel').onclick = close;
    $('confirm-proceed').onclick = () => { close(); action(); };
    $('confirm-download').onclick = () => { if (exportProject()) { close(); action(); } };
    dialog.showModal();
  }
  function replaceProject(project, dirty, message) {
    state.project = project; state.selectedId = null; state.placement = null; state.history = []; state.future = []; state.dirty = dirty;
    state.panel = 'layout'; state.selectedWaveId = null; state.pickWaveId = null;
    resetView(false); persist(); renderPalette(); render(); if (message) toast(message);
  }
  $('import-file').addEventListener('change', async event => {
    const file = event.target.files[0]; event.target.value = ''; if (!file) return;
    if (file.size > 2 * 1024 * 1024) { toast('文件超过 2 MiB 上限，当前设计保持不变。', true); return; }
    let project;
    try { project = M.parseProject(await file.text()); }
    catch (error) { toast('导入失败，当前设计保持不变。\n' + short(error.message, 220), true); return; }
    confirmReplace(() => replaceProject(project, false, '已导入「' + project.meta.title + '」。'), '导入并替换当前设计？');
  });
  async function exportPNG() {
    const button = $('export-png'); button.disabled = true;
    let url;
    try {
      const p = state.project, W = p.field.width, H = p.field.height;
      // Export labels are composed for the full field even if the live view is
      // zoomed in, with a minimum 22 px at the 2000 px export width.
      renderCanvas(Math.min((svg.getScreenCTM() && svg.getScreenCTM().a || 12) / state.zoom, 1000 / (W + 3)));
      const source = svg.cloneNode(true); renderCanvas();
      const width = 2000, height = Math.round(width * (H + 4) / (W + 3));
      source.setAttribute('viewBox', `-1.5 -2 ${W + 3} ${H + 4}`); source.setAttribute('width', width); source.setAttribute('height', height); source.removeAttribute('tabindex');
      const background = element('rect', { x: -1.5, y: -2, width: W + 3, height: H + 4, fill: '#f5f2e9' }); source.prepend(background);
      source.querySelectorAll('.selected').forEach(node => { node.classList.remove('selected'); const main = node.querySelector('.object-main'); if (main) Array.from(main.children).filter(child => child.getAttribute('stroke') === '#a68537' || child.getAttribute('stroke') === '#fef9d9' || child.getAttribute('fill') === '#fbf7d8').forEach(child => child.remove()); });
      // SVG exported through an image has no access to page CSS; inline text styling explicitly.
      source.querySelectorAll('.object-label').forEach(node => { node.setAttribute('font-family', 'Microsoft YaHei, sans-serif'); node.setAttribute('paint-order', 'stroke'); node.setAttribute('stroke', '#f5f7e8'); node.setAttribute('stroke-width', '.085'); });
      source.append(element('text', { x: 0, y: -1.05, fill: '#2d5948', 'font-size': '.63', 'font-weight': '600', 'font-family': 'Microsoft YaHei, sans-serif' }, short(p.meta.title, 45)));
      source.append(element('text', { x: W, y: -1.1, 'text-anchor': 'end', fill: '#7a826c', 'font-size': '.3', 'font-family': 'Microsoft YaHei, sans-serif' }, '设计预览 · 非游戏运行'));
      source.append(element('text', { x: 0, y: H + 1.45, fill: '#7a826c', 'font-size': '.32', 'font-family': 'Microsoft YaHei, sans-serif' }, `小小国王 · 战场工坊  |  ${W} × ${H} 米  |  左：玩家 · 右：敌方  |  请同时交付设计 JSON`));
      const xml = new XMLSerializer().serializeToString(source);
      url = URL.createObjectURL(new Blob([xml], { type: 'image/svg+xml;charset=utf-8' }));
      const image = new Image(); await new Promise((resolve, reject) => { image.onload = resolve; image.onerror = () => reject(new Error('浏览器无法读取预览图。')); image.src = url; });
      const canvas = document.createElement('canvas'); canvas.width = width; canvas.height = height; const context = canvas.getContext('2d'); if (!context) throw new Error('浏览器无法创建图片画布。'); context.drawImage(image, 0, 0);
      const blob = await new Promise(resolve => canvas.toBlob(resolve, 'image/png')); if (!blob) throw new Error('浏览器无法导出 PNG。');
      download(blob, filename('.png')); toast('预览图片已下载。可编辑的布局仍请使用设计 JSON。');
    } catch (error) { toast('图片导出失败：' + error.message, true); }
    finally { if (url) URL.revokeObjectURL(url); button.disabled = false; }
  }

  $('new-project').addEventListener('click', () => confirmReplace(() => replaceProject(M.createProject(), true, '新战场已准备好，从左侧放置积木吧。'), '新建空白战场？'));
  $('load-example').addEventListener('click', () => confirmReplace(() => replaceProject(M.createTerrainExample(), true, '已打开高地伏击示例，包含四种地形与三种波次。'), '打开示例并替换当前设计？'));
  ['layout', 'waves', 'enemy'].forEach(panel => $('tab-' + panel).addEventListener('click', () => { state.panel = panel; state.pickWaveId = null; state.placement = null; state.selectedId = null; render(); renderPalette(); }));
  $('import-project').addEventListener('click', () => $('import-file').click());
  $('export-project').addEventListener('click', exportProject);
  $('export-png').addEventListener('click', exportPNG);
  $('export-unreal').addEventListener('click', () => { try { download(JSON.stringify(M.toUnreal(state.project), null, 2), filename('.json', '-UE坐标对照')); toast('已下载开发坐标对照。交付设计时请同时提供原始设计 JSON。'); } catch (error) { toast('无法导出坐标：' + short(error.message, 200), true); } });
  $('download-broken-draft').addEventListener('click', () => download(state.brokenDraft || '', '战场工坊-原始草稿备份.txt', 'text/plain;charset=utf-8'));
  $('discard-broken-draft').addEventListener('click', () => {
    if (!window.confirm('确认清除无法读取的旧草稿？建议先下载原文备份。当前画布将成为新的草稿。')) return;
    state.draftBlocked = false; state.brokenDraft = null; $('draft-alert').hidden = true; persist();
  });
  $('undo').addEventListener('click', undo); $('redo').addEventListener('click', redo);
  $('tool-select').addEventListener('click', chooseSelect);
  $('zoom-in').addEventListener('click', () => zoom(.2)); $('zoom-out').addEventListener('click', () => zoom(-.2)); $('zoom-reset').addEventListener('click', () => resetView());
  $('snap-grid').addEventListener('change', event => { state.snap = event.target.checked; });
  $('module-search').addEventListener('input', event => { state.search = event.target.value.trim().toLowerCase(); renderPalette(); });
  const openHelp = () => $('help-dialog').showModal();
  $('show-help').addEventListener('click', openHelp); $('show-help-bottom').addEventListener('click', openHelp); $('close-help').addEventListener('click', () => $('help-dialog').close());
  $('help-dialog').addEventListener('click', event => { if (event.target === $('help-dialog')) { const r = $('help-dialog').getBoundingClientRect(); if (event.clientX < r.left || event.clientX > r.right || event.clientY < r.top || event.clientY > r.bottom) $('help-dialog').close(); } });
  function isEditing(target) { return Boolean(target.closest('input,textarea,select,[contenteditable="true"]')); }
  window.addEventListener('keydown', event => {
    if (document.querySelector('dialog[open]') || isEditing(event.target)) return;
    const key = event.key.toLowerCase(), ctrl = event.ctrlKey || event.metaKey;
    if (ctrl && key === 'z') { event.preventDefault(); event.shiftKey ? redo() : undo(); }
    else if (ctrl && key === 'y') { event.preventDefault(); redo(); }
    else if (ctrl && key === 'd') { event.preventDefault(); duplicateSelected(); }
    else if (ctrl && key === 's') { event.preventDefault(); exportProject(); }
    else if (!ctrl && !event.altKey && (key === 'delete' || key === 'backspace')) { event.preventDefault(); deleteSelected(); }
    else if (!ctrl && !event.altKey && key === 'q') { event.preventDefault(); rotate(-15); }
    else if (!ctrl && !event.altKey && key === 'e') { event.preventDefault(); rotate(15); }
    else if (!ctrl && !event.altKey && key === 'v') { event.preventDefault(); chooseSelect(); }
    else if (key === 'escape') { finishGesture(true); chooseSelect(); }
    else if (key === ' ') { event.preventDefault(); state.space = true; svg.classList.add('mode-pan'); }
    else if (!ctrl && !event.altKey && ['arrowleft','arrowright','arrowup','arrowdown'].includes(key) && selected()) {
      event.preventDefault(); const step = (event.shiftKey ? .1 : state.project.field.grid); mutate(() => { const o = selected(); o.x = round(o.x + (key === 'arrowleft' ? -step : key === 'arrowright' ? step : 0)); o.y = round(o.y + (key === 'arrowup' ? -step : key === 'arrowdown' ? step : 0)); });
    }
  });
  window.addEventListener('keyup', event => { if (event.key === ' ') { state.space = false; svg.classList.remove('mode-pan'); } });
  window.addEventListener('blur', () => { state.space = false; svg.classList.remove('mode-pan'); finishGesture(false); });
  window.addEventListener('beforeunload', event => { if (state.dirty) { event.preventDefault(); event.returnValue = ''; } });
  window.addEventListener('resize', () => renderCanvas());
  restore(); renderPalette(); render();
})();
