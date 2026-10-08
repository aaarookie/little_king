/* Little King battle layout model. No network, engine, DOM or package dependencies. */
(function (root, factory) {
  'use strict';
  const api = factory();
  if (typeof module === 'object' && module.exports) module.exports = api;
  if (root && typeof root === 'object') root.LKLevel = api;
})(typeof globalThis !== 'undefined' ? globalThis : this, function () {
  'use strict';
  const FORMAT = 'little-king.battle-layout';
  const VERSION = 2;
  const MAX_BYTES = 2 * 1024 * 1024;
  const MAX_OBJECTS = 500;
  const MAX_WAVES = 64;
  const GRIDS = Object.freeze([0.25, 0.5, 1, 2]);
  const TEAMS = Object.freeze(['player', 'enemy', 'neutral']);
  const ENEMY_ONLY = new Set(['skeleton', 'skeleton-archer', 'necromancer', 'skeleton-giant', 'skeleton-king']);
  const MODULES = Object.freeze([
    ['grass', '草地', '地形', '#77976b', 8, 6, 'neutral', 'terrain', 'runtime'],
    ['brush', '草丛', '地形', '#3f7543', 6, 4, 'neutral', 'terrain', 'runtime'],
    ['swamp', '沼泽', '地形', '#648d86', 6, 4, 'neutral', 'terrain', 'runtime'],
    ['campfire', '军营篝火', '障碍', '#b78657', 2, 2, 'neutral', 'obstacle', 'runtime'],
    ['mountain', '高山', '障碍', '#a39179', 6, 5, 'neutral', 'obstacle', 'runtime'],
    ['road', '道路', '地形', '#baaa83', 10, 3, 'neutral', 'terrain', 'runtime'],
    ['water', '水域', '地形', '#70a5b4', 6, 4, 'neutral', 'terrain', 'runtime'],
    ['rock', '岩石', '障碍', '#858477', 2, 2, 'neutral', 'obstacle', 'runtime'],
    ['tree', '树木', '障碍', '#426b48', 2, 2, 'neutral', 'obstacle', 'runtime'],
    ['flowers', '花丛', '装饰', '#b89b80', 2, 1, 'neutral', 'decoration', 'runtime'],
    ['player-zone', '我方部署区', '区域', '#5e9caa', 8, 12, 'player', 'zone', 'runtime'],
    ['enemy-zone', '敌方出现区', '区域', '#bb7567', 8, 12, 'enemy', 'zone', 'design'],
    ['knight', '骑士', '英雄', '#b6a36d', 1.2, 1.2, 'player', 'unit', 'existing', 'Hero_Knight'],
    ['mage', '法师', '英雄', '#a591c3', 1, 1, 'player', 'unit', 'existing', 'Hero_Mage'],
    ['ranger', '游侠', '英雄', '#8ba570', 1, 1, 'player', 'unit', 'existing', 'Hero_Ranger'],
    ['swordsman', '剑士', '佣兵', '#a2b5b1', 0.8, 0.8, 'player', 'unit', 'existing', 'Unit_Swordsman'],
    ['archer', '弓箭手', '佣兵', '#aaa578', 0.8, 0.8, 'player', 'unit', 'existing', 'Unit_Archer'],
    ['skeleton', '骷髅兵', '敌军', '#ccbe99', 0.8, 0.8, 'enemy', 'unit', 'existing', 'Unit_Skeleton'],
    ['skeleton-archer', '骷髅射手', '敌军', '#b6b7a1', 0.8, 0.8, 'enemy', 'unit', 'existing', 'Unit_SkeletonArcher'],
    ['necromancer', '死灵法师', '敌军', '#a586b1', 1.2, 1.2, 'enemy', 'unit', 'existing', 'Hero_Necromancer'],
    ['skeleton-giant', '骷髅巨人', '敌军', '#b5ab81', 1.8, 1.8, 'enemy', 'unit', 'existing', 'Hero_SkeletonGiant'],
    ['skeleton-king', '骷髅王', '敌军', '#bf9a5f', 1.8, 1.8, 'enemy', 'unit', 'existing', 'Boss_SkeletonKing'],
    ['wall-common', '普通防御墙', '建筑', '#aaa18f', 3, 0.36, 'player', 'building', 'existing', 'Unit_WallCommon'],
    ['wall-rare', '稀有防御墙', '建筑', '#8dacc2', 3, 0.36, 'player', 'building', 'existing', 'Unit_WallRare'],
    ['wall-epic', '史诗防御墙', '建筑', '#ac8ebf', 3, 0.36, 'player', 'building', 'existing', 'Unit_WallEpic'],
    ['wall-imperial', '禁军之墙', '建筑', '#caa561', 3, 0.36, 'player', 'building', 'existing', 'Unit_WallImperial'],
    ['mage-tower', '法师塔', '建筑', '#9387ae', 2, 2, 'player', 'building', 'existing', 'Unit_MageTower'],
    ['healing-station', '牧师治疗站', '建筑', '#c7b58b', 2, 2, 'player', 'building', 'existing', 'Unit_HealingStation'],
    ['siege-cart', '攻城车', '建筑', '#ab855e', 2, 1.5, 'player', 'building', 'existing', 'Unit_SiegeCart'],
    ['arrow-tower', '箭塔', '建筑', '#9e936f', 1.8, 1.8, 'player', 'building', 'existing', 'Building_ArrowTower'],
    ['barracks', '兵营', '建筑', '#b09578', 2.5, 2, 'player', 'building', 'existing', 'Building_Barracks'],
    ['note', '文字标注', '标注', '#d8bd70', 4, 2, 'neutral', 'note', 'design']
  ].map(function (row) {
    const m = { id: row[0], label: row[1], category: row[2], color: row[3], defaultWidth: row[4],
      defaultHeight: row[5], defaultTeam: row[6], role: row[7], implementation: row[8] };
    if (row[9]) m.unitId = row[9];
    return Object.freeze(m);
  }));
  const byId = new Map(MODULES.map(function (m) { return [m.id, m]; }));
  const WAVE_UNITS = Object.freeze((typeof module === 'object' && module.exports ? require('./units.js') : globalThis.LKUnitCatalog).map(u => Object.freeze(u)));
  const HERO_MODULES = new Set(['knight', 'mage', 'ranger', 'necromancer', 'skeleton-giant', 'skeleton-king']);
  const TRIGGERS = Object.freeze(['time', 'hero-health-below', 'hero-first-death']);
  const CARDS = Object.freeze((typeof module === 'object' && module.exports ? require('./catalog.js') : globalThis.LKCardCatalog).map(c => Object.freeze(c)));
  let sequence = 0;
  function uniqueId(prefix) {
    const crypto = typeof globalThis !== 'undefined' && globalThis.crypto;
    if (crypto && typeof crypto.randomUUID === 'function') return prefix + crypto.randomUUID();
    sequence += 1;
    return prefix + Date.now().toString(36) + '-' + sequence.toString(36) + '-' + Math.random().toString(36).slice(2, 10);
  }
  function getModule(id) { return byId.get(id); }
  function createProject() {
    const now = new Date().toISOString();
    return { format: FORMAT, version: VERSION,
      meta: { id: uniqueId('map-'), title: '未命名战斗场地', description: '', createdAt: now, updatedAt: now },
      field: { width: 40, height: 24, grid: 1 },
      settings: { encounterId: 'Patrol', seed: 12345, battleTimeLimit: 300 }, objects: [], enemy: defaultEnemy(), waves: [] };
  }
  function defaultEnemy() { return { usesCards: false, cards: ['Unit_Skeleton', 'Unit_SkeletonArcher', 'Spell_SkeletonCircle'], silverPerSecond: 1, silverCap: 20, startingSilver: 5 }; }
  function createWave(x, y) { return { id: uniqueId('wave-'), label: '新波次', trigger: 'time', time: 30, targetId: '', healthThreshold: .5, unitId: 'Unit_Skeleton', count: 3, x: x, y: y, radius: 2 }; }
  function triggerHeroes(project) { return project.objects.filter(o => HERO_MODULES.has(o.moduleId) && o.count === 1 && o.spawnAtSeconds === 0); }
  function createObject(moduleId, x, y) {
    const m = getModule(moduleId);
    if (!m) throw new Error('未知模块：' + String(moduleId));
    if (!Number.isFinite(x) || !Number.isFinite(y)) throw new Error('对象坐标必须是有限数字。');
    return { id: uniqueId('obj-'), moduleId: m.id, label: m.label, x: x, y: y,
      width: m.defaultWidth, height: m.defaultHeight, rotation: 0, team: m.defaultTeam,
      count: 1, spawnAtSeconds: 0, note: '' };
  }
  function createExample() {
    const p = createProject();
    p.meta.title = '林间关隘 · 巡逻战';
    p.meta.description = '左侧部署，沿中路推进。敌方墙阵留下上下绕行空间；后排法师受两组骷髅掩护。草地和道路为视觉装饰，树木与岩石为不可破坏障碍。';
    const add = function (id, x, y, changes) {
      const o = Object.assign(createObject(id, x, y), changes || {}); p.objects.push(o); return o;
    };
    add('grass', 20, 12, { width: 40, height: 24 });
    add('road', 20, 12, { width: 40, height: 4 });
    add('player-zone', 5.5, 12, { width: 9, height: 16 });
    add('enemy-zone', 34, 12, { width: 10, height: 16 });
    add('tree', 17, 5, { width: 3, height: 3 });
    add('tree', 17, 19, { width: 3, height: 3 });
    add('rock', 21, 4, { width: 3, height: 2 });
    add('rock', 21, 20, { width: 3, height: 2 });
    add('wall-common', 27, 10.5, { team: 'enemy', rotation: 90 });
    add('wall-common', 27, 13.5, { team: 'enemy', rotation: 90 });
    add('skeleton', 29, 9, { count: 3, note: '第一组近战护卫，组内位置在落地时散开。' });
    add('skeleton', 29, 15, { count: 3 });
    add('skeleton-archer', 32, 12, { count: 3 });
    add('necromancer', 35, 12);
    add('skeleton', 36, 7, { count: 4, spawnAtSeconds: 30, note: '30 秒后从右上角增援。' });
    add('note', 18, 8, { label: '上下绕行', width: 5, note: '墙后法师是主要目标；预留两侧可通行路线。' });
    return p;
  }
  function createTerrainExample() {
    const p = createProject();
    p.meta.title = '高地伏击 · 双路增援';
    p.meta.description = '草丛中的骷髅射手伏击中路，沼泽拖慢推进；远程单位和法术处理高山守军。篝火形成绕行点。三种一次性波次与银币出牌同时运作。';
    const add = (id, x, y, changes) => { const o = Object.assign(createObject(id, x, y), changes || {}); p.objects.push(o); return o; };
    add('player-zone', 5, 12, { width: 8, height: 16 });
    add('brush', 24, 7, { width: 8, height: 4 });
    add('brush', 30, 7, { width: 5, height: 4, note: '与左侧草丛重叠，构成同一连通片。' });
    add('swamp', 21, 16, { width: 8, height: 5 });
    add('campfire', 16, 12);
    add('mountain', 33, 17, { width: 7, height: 5, note: '山上驻守固定；地面近战无法攻击。' });
    add('skeleton-archer', 33, 17, { count: 3, note: '高山远程守军。' });
    add('skeleton-archer', 25, 7, { count: 3, note: '草丛伏兵。' });
    const hero = add('necromancer', 36, 10, { label: '伏击指挥官' });
    p.enemy = { usesCards: true, cards: ['Unit_Skeleton', 'Unit_SkeletonArcher', 'Spell_SkeletonCircle'], silverPerSecond: 1.5, silverCap: 20, startingSilver: 5 };
    p.waves.push(Object.assign(createWave(29, 12), { label: '开战 15 秒近战增援', time: 15, count: 5 }));
    p.waves.push(Object.assign(createWave(34, 5), { label: '指挥官首次半血呼援', trigger: 'hero-health-below', targetId: hero.id, time: 2, count: 4, unitId: 'Unit_SkeletonArcher', radius: 1.5 }));
    p.waves.push(Object.assign(createWave(36, 12), { label: '指挥官首次倒下后增援', trigger: 'hero-first-death', targetId: hero.id, time: 1, count: 1, unitId: 'Hero_SkeletonGiant', radius: 1 }));
    return p;
  }
  function isRecord(value) {
    if (!value || typeof value !== 'object' || Array.isArray(value)) return false;
    const proto = Object.getPrototypeOf(value);
    return proto === Object.prototype || proto === null;
  }
  function footprint(object) {
    const angle = object.rotation * Math.PI / 180;
    const c = Math.cos(angle), s = Math.sin(angle);
    const w = object.width / 2, h = object.height / 2;
    return [[-w, -h], [w, -h], [w, h], [-w, h]].map(function (xy) {
      return { x: object.x + xy[0] * c - xy[1] * s, y: object.y + xy[0] * s + xy[1] * c };
    });
  }
  function bounds(object) {
    const points = footprint(object);
    return { minX: Math.min.apply(null, points.map(p => p.x)), maxX: Math.max.apply(null, points.map(p => p.x)),
      minY: Math.min.apply(null, points.map(p => p.y)), maxY: Math.max.apply(null, points.map(p => p.y)) };
  }
  function validate(project) {
    const errors = [], warnings = [];
    function record(value, label, keys) {
      if (!isRecord(value)) { errors.push(label + '必须是普通对象。'); return false; }
      const own = Reflect.ownKeys(value);
      if (own.some(k => { const d = Object.getOwnPropertyDescriptor(value, k); return d.get || d.set; })) {
        errors.push(label + '不能包含动态属性。'); return false;
      }
      const extra = own.filter(k => !keys.includes(k));
      if (extra.length) errors.push(label + '含不支持的字段：' + extra.slice(0, 4).map(String).join('、'));
      const missing = keys.filter(k => !Object.prototype.hasOwnProperty.call(value, k));
      if (missing.length) errors.push(label + '缺少字段：' + missing.join('、'));
      return true;
    }
    function string(value, label, min, max) {
      if (typeof value !== 'string' || value.length < min || value.length > max) {
        errors.push(label + '必须为 ' + min + '–' + max + ' 字符的文本。'); return false;
      }
      return true;
    }
    function identifier(value, label) {
      if (string(value, label, 1, 80) && !/^[A-Za-z0-9][A-Za-z0-9_.-]*$/.test(value)) errors.push(label + '只能包含英文字母、数字、下划线、点和连字符。');
    }
    function number(value, label, min, max, integer) {
      const valid = typeof value === 'number' && Number.isFinite(value) && value >= min && value <= max && (!integer || Number.isInteger(value));
      if (!valid) errors.push(label + '必须是 ' + min + '–' + max + ' 的有限' + (integer ? '整数。' : '数字。'));
      return valid;
    }
    if (!record(project, '地图', ['format', 'version', 'meta', 'field', 'settings', 'objects', 'enemy', 'waves'])) return { errors, warnings };
    if (project.format !== FORMAT) errors.push('文件格式必须是 ' + FORMAT + '。');
    if (project.version !== VERSION) errors.push('不支持该版本；当前仅支持 version: 2，version: 1 请通过导入迁移。');
    if (record(project.meta, '地图信息', ['id', 'title', 'description', 'createdAt', 'updatedAt'])) {
      identifier(project.meta.id, '地图 ID');
      string(project.meta.title, '地图名称', 1, 120);
      string(project.meta.description, '地图说明', 0, 4000);
      ['createdAt', 'updatedAt'].forEach(function (key) {
        const value = project.meta[key];
        if (typeof value !== 'string' || value.length > 40 || !/^\d{4}-\d{2}-\d{2}T/.test(value) || !Number.isFinite(Date.parse(value))) errors.push(key + '必须是 ISO 日期时间。');
      });
    }
    let fieldValid = false;
    if (record(project.field, '战场', ['width', 'height', 'grid'])) {
      const w = number(project.field.width, '战场宽度', 8, 100);
      const h = number(project.field.height, '战场高度', 8, 100);
      fieldValid = w && h;
      if (!GRIDS.includes(project.field.grid)) errors.push('网格仅支持 0.25、0.5、1 或 2 米。');
      if (fieldValid && (project.field.width !== 40 || project.field.height !== 24)) warnings.push('地图尺寸与默认 40 × 24 米不同；试玩会同步边界与镜头，战斗节奏需重新检查。');
    }
    if (record(project.settings, '战斗设置', ['encounterId', 'seed', 'battleTimeLimit'])) {
      identifier(project.settings.encounterId, '遭遇模板 ID');
      number(project.settings.seed, '随机种子', 0, 2147483647, true);
      number(project.settings.battleTimeLimit, '战斗时限', 1, 3600, true);
      if (typeof project.settings.encounterId === 'string' && !['Patrol', 'Elite', 'Boss', 'Encounter_UndeadPatrol', 'Encounter_UndeadElite', 'Encounter_SkeletonKing'].includes(project.settings.encounterId)) warnings.push('遭遇模板 ID 不在内置清单中；落地前需确认项目中存在此模板。');
    }
    function array(value, label, max) {
      if (!Array.isArray(value) || value.length > max) { errors.push(label + '必须是最多 ' + max + ' 项的数组。'); return false; }
      if (Reflect.ownKeys(value).some(k => k !== 'length' && (typeof k !== 'string' || !/^(0|[1-9][0-9]*)$/.test(k)))) { errors.push(label + '不能含额外字段。'); return false; }
      for (let i = 0; i < value.length; i++) { const d = Object.getOwnPropertyDescriptor(value, i); if (!d || d.get || d.set) { errors.push(label + '不能含空缺或动态属性。'); return false; } }
      return true;
    }
    if (record(project.enemy, '敌方经济', ['usesCards', 'cards', 'silverPerSecond', 'silverCap', 'startingSilver'])) {
      const e = project.enemy;
      if (typeof e.usesCards !== 'boolean') errors.push('敌方是否出牌必须是布尔值。');
      number(e.silverPerSecond, '每秒银币', 0, 100);
      number(e.silverCap, '银币上限', 1, 1000);
      number(e.startingSilver, '初始银币', 0, 1000);
      if (e.startingSilver > e.silverCap) errors.push('初始银币不能大于银币上限。');
      if (array(e.cards, '敌方牌组', CARDS.length)) {
        const seen = new Set();
        e.cards.forEach(id => { if (!CARDS.some(c => c.id === id)) errors.push('未知敌方卡牌 ID：' + String(id)); if (seen.has(id)) errors.push('敌方牌组不允许重复卡牌。'); seen.add(id); });
        if (e.usesCards && !e.cards.length) errors.push('启用敌方出牌时，至少选择一张卡牌。');
      }
    }
    if (!Array.isArray(project.objects)) { errors.push('objects 必须是数组。'); return { errors, warnings }; }
    if (project.objects.length > MAX_OBJECTS) { errors.push('最多允许 ' + MAX_OBJECTS + ' 个对象。'); return { errors, warnings }; }
    if (Reflect.ownKeys(project.objects).some(k => k !== 'length' && (typeof k !== 'string' || !/^(0|[1-9][0-9]*)$/.test(k)))) {
      errors.push('对象数组不能含额外字段。'); return { errors, warnings };
    }
    for (let i = 0; i < project.objects.length; i++) {
      const descriptor = Object.getOwnPropertyDescriptor(project.objects, i);
      if (!descriptor || descriptor.get || descriptor.set) {
        errors.push('对象数组不能含空缺或动态属性。'); return { errors, warnings };
      }
    }
    const ids = new Set(), solids = [], enemyHeroTypes = new Set();
    let enemyCount = 0, unitCount = 0, designCount = 0, deployment = false, resizedExisting = 0;
    project.objects.forEach(function (o, index) {
      const label = '对象 ' + (index + 1) + '：';
      if (!record(o, label, ['id', 'moduleId', 'label', 'x', 'y', 'width', 'height', 'rotation', 'team', 'count', 'spawnAtSeconds', 'note'])) return;
      identifier(o.id, label + 'ID');
      const idKey = typeof o.id === 'string' ? o.id.toLowerCase() : o.id;
      if (ids.has(idKey)) errors.push(label + 'ID 重复（游戏 ID 不区分大小写）。');
      ids.add(idKey);
      const m = typeof o.moduleId === 'string' ? getModule(o.moduleId) : undefined;
      if (!m) errors.push(label + '未知模块。');
      string(o.label, label + '名称', 1, 100);
      string(o.note, label + '备注', 0, 4000);
      const x = number(o.x, label + 'X 坐标', 0, 100), y = number(o.y, label + 'Y 坐标', 0, 100);
      const w = number(o.width, label + '宽度', 0.1, 100), h = number(o.height, label + '高度', 0.1, 100);
      const r = number(o.rotation, label + '旋转角度', -3600, 3600);
      const c = number(o.count, label + '数量', 1, 20, true);
      const t = number(o.spawnAtSeconds, label + '出现时间', 0, 300);
      if (!TEAMS.includes(o.team)) errors.push(label + '阵营必须是 player、enemy 或 neutral。');
      if (m && ENEMY_ONLY.has(m.id) && o.team !== 'enemy') errors.push(label + m.label + '仅允许敌方阵营。');
      if (m && ['unit', 'building'].includes(m.role) && o.team === 'neutral') errors.push(label + '角色与建筑必须指定我方或敌方阵营。');
      if (m && m.role === 'zone' && o.team !== m.defaultTeam) errors.push(label + m.label + '的阵营与模块不符。');
      if (m && HERO_MODULES.has(m.id)) {
        if (o.count !== 1 || o.spawnAtSeconds !== 0) errors.push(label + '地图英雄必须数量为 1 且开局出现，延后增援英雄请放入波次。');
        if (o.team === 'enemy') { if (enemyHeroTypes.has(m.id)) errors.push(label + '开局敌方英雄类型不能重复，同类后续英雄请放入波次。'); enemyHeroTypes.add(m.id); }
      }
      if (m && ['unit','building'].includes(m.role) && o.team !== 'enemy' && o.spawnAtSeconds > 0) errors.push(label + '延时地图对象只支持敌方；我方单位请在开局放置。');
      if (fieldValid && x && y && w && h && r) {
        const b = bounds(o);
        if (b.minX < -1e-7 || b.minY < -1e-7 || b.maxX > project.field.width + 1e-7 || b.maxY > project.field.height + 1e-7) errors.push(label + '完整旋转轮廓超出战场；请移动或缩小对象。');
        if (m && (m.role === 'obstacle' || (m.role === 'building' && m.id !== 'siege-cart'))) solids.push({ object: o, bounds: b });
      }
      if (m) {
        if (m.role === 'unit' && o.team === 'enemy' && c) enemyCount += o.count;
        if (['unit', 'building'].includes(m.role) && c) unitCount += o.count;
        if (m.implementation === 'design' && !['note', 'zone'].includes(m.role)) designCount++;
        if (m.id === 'player-zone') deployment = true;
        if (m.implementation === 'existing' && (o.width !== m.defaultWidth || o.height !== m.defaultHeight)) resizedExisting++;
        if (!['unit', 'building'].includes(m.role) && ((c && o.count !== 1) || (t && o.spawnAtSeconds !== 0))) warnings.push(label + '数量和出现时间仅作为设计说明；该类模块不会自动生成单位波次。');
      }
    });
    // Cross references are only inspected after all object shapes are trusted.
    if (errors.length) return { errors, warnings };
    if (array(project.waves, '波次', MAX_WAVES)) {
      const waveIds = new Set();
      project.waves.forEach((wave, index) => {
        const label = '波次 ' + (index + 1) + '：';
        if (!record(wave, label, ['id', 'label', 'trigger', 'time', 'targetId', 'healthThreshold', 'unitId', 'count', 'x', 'y', 'radius'])) return;
        identifier(wave.id, label + 'ID'); string(wave.label, label + '名称', 1, 100);
        const idKey = typeof wave.id === 'string' ? wave.id.toLowerCase() : wave.id;
        if (waveIds.has(idKey) || ids.has(idKey)) errors.push(label + 'ID 重复（游戏 ID 不区分大小写）。'); waveIds.add(idKey);
        if (!TRIGGERS.includes(wave.trigger)) errors.push(label + '未知触发条件。');
        number(wave.time, label + '时间/延迟', 0, 3600);
        number(wave.healthThreshold, label + '血量阈值', .01, .99);
        string(wave.targetId, label + '目标 ID', 0, 80);
        if (wave.trigger === 'time' && wave.targetId !== '') errors.push(label + '定时波次的目标 ID 必须为空。');
        if (wave.trigger !== 'time') {
          const target = project.objects.find(o => o && o.id === wave.targetId);
          if (!target || !HERO_MODULES.has(target.moduleId) || target.count !== 1 || target.spawnAtSeconds !== 0) errors.push(label + '条件必须引用一名单独放置、开局出现的地图英雄。请先修改引用波次，再删除或改动该英雄。');
        }
        if (!WAVE_UNITS.some(u => u.id === wave.unitId)) errors.push(label + '兵种不在已登记角色清单内。');
        number(wave.count, label + '数量', 1, 50, true);
        number(wave.x, label + 'X', 0, fieldValid ? project.field.width : 100);
        number(wave.y, label + 'Y', 0, fieldValid ? project.field.height : 100);
        number(wave.radius, label + '分散半径', 0, 10);
        if (fieldValid && (wave.x - wave.radius < 0 || wave.y - wave.radius < 0 || wave.x + wave.radius > project.field.width || wave.y + wave.radius > project.field.height)) warnings.push(label + '生成圆部分越界；运行时将在可用位置生成。');
        if (wave.trigger === 'time' && wave.time >= project.settings.battleTimeLimit) warnings.push(label + '出现时间不早于战斗时限，通常无法执行。');
      });
      if (project.waves.reduce((sum, w) => sum + (w && Number.isFinite(w.count) ? w.count : 0), 0) + unitCount > 200) warnings.push('地图与波次累计超过 200 个单位；同时存活数量受运行时容量限制，拥挤区域可能无法放下全部增援。');
      const legacyWaves = project.objects.filter(o => o.spawnAtSeconds > 0 && ['unit','building'].includes(getModule(o.moduleId).role)).length;
      if (legacyWaves + project.waves.length > MAX_WAVES) errors.push('地图延时对象与显式增援合计最多 ' + MAX_WAVES + ' 条波次。');
    }
    ['player', 'enemy'].forEach(team => {
      const teamName = team === 'player' ? '我方' : '敌方';
      const opening = project.objects.filter(o => o.team === team && o.spawnAtSeconds === 0 && ['unit', 'building'].includes(getModule(o.moduleId).role));
      if (opening.reduce((sum, o) => sum + o.count, 0) > 20) warnings.push(teamName + '开局角色与建筑超过默认单阵营 20 个上限；导入或后续生成可能受限。');
      if (opening.filter(o => HERO_MODULES.has(o.moduleId)).reduce((sum, o) => sum + o.count, 0) > 3) warnings.push(teamName + '开局英雄超过默认 3 名；请核对测试关卡的英雄容量与 UI。');
      const buildings = new Map(); opening.filter(o => getModule(o.moduleId).role === 'building').forEach(o => buildings.set(o.moduleId, (buildings.get(o.moduleId) || 0) + o.count));
      if ([...buildings.values()].some(count => count > 2)) warnings.push(teamName + '同类建筑超过默认 2 座上限；请核对关卡导入与出牌限制。');
    });
    if (project.objects.some(o => o && o.moduleId === 'mountain')) warnings.push('高山内开局单位固定驻守，地面近战无法攻击；请配备远程或法术。高台不包含上下山路径。');
    if (!enemyCount) warnings.push('尚未放置敌方角色；建议至少放置 1 名敌人后再交付战斗设计。');
    if (!enemyHeroTypes.size) warnings.push('试玩导入需要至少一名开局敌方英雄或首领；可先完成布局再放置。');
    if (!deployment) warnings.push('建议用“我方部署区”标明开局可放置英雄的位置。');
    if (unitCount > 200) warnings.push('配置总计 ' + unitCount + ' 个角色/建筑，超过当前运行时常用容量；落地前需安排分批生成并检查性能。');
    if (designCount) warnings.push('有 ' + designCount + ' 个地形/障碍/装饰仅为设计标记，需要在引擎内配置美术、碰撞和寻路规则。');
    if (resizedExisting) warnings.push('已修改 ' + resizedExisting + ' 个现有角色/建筑的设计占地；落地时需核对实际碰撞体，修改图中尺寸不会自动改变单位数值。');
    // AABB gap warning is deliberately conservative. It is not a pathfinding guarantee.
    let narrowPairs = 0;
    for (let i = 0; i < solids.length; i++) {
      const a = solids[i].bounds;
      for (let j = i + 1; j < solids.length; j++) {
        const b = solids[j].bounds;
        const gapX = Math.max(0, a.minX - b.maxX, b.minX - a.maxX);
        const gapY = Math.max(0, a.minY - b.maxY, b.minY - a.maxY);
        const overlapX = Math.min(a.maxX, b.maxX) - Math.max(a.minX, b.minX);
        const overlapY = Math.min(a.maxY, b.maxY) - Math.max(a.minY, b.minY);
        if ((gapX > 1e-7 && gapX < 1 && overlapY > 0) || (gapY > 1e-7 && gapY < 1 && overlapX > 0)) narrowPairs++;
      }
    }
    if (narrowPairs) warnings.push('约有 ' + narrowPairs + ' 处障碍间隙小于 1 米，可能形成窄通道；这是外接矩形估计，仍需引擎寻路实测。');
    return { errors, warnings };
  }
  function byteLength(text) {
    if (typeof TextEncoder !== 'undefined') return new TextEncoder().encode(text).length;
    let bytes = 0;
    for (const char of text) { const p = char.codePointAt(0); bytes += p <= 0x7f ? 1 : p <= 0x7ff ? 2 : p <= 0xffff ? 3 : 4; }
    return bytes;
  }
  function requireValid(project) {
    const result = validate(project);
    if (result.errors.length) throw new Error(result.errors.slice(0, 20).join('\n'));
    return project;
  }
  function parseProject(text) {
    if (typeof text !== 'string') throw new Error('请提供 JSON 文本。');
    if (text.length > MAX_BYTES || byteLength(text) > MAX_BYTES) throw new Error('JSON 文件不能超过 2 MiB。');
    let project;
    try {
      project = JSON.parse(text, function (key, value) {
        if (key === '__proto__' || key === 'prototype' || key === 'constructor') throw new Error('禁止原型字段');
        return value;
      });
    } catch (error) { throw new Error('JSON 读取失败：' + error.message); }
    if (isRecord(project) && project.format === FORMAT && project.version === 1) {
      // A v1 file must not smuggle v2 fields past its old contract.
      if ('enemy' in project || 'waves' in project) throw new Error('version: 1 不能包含 version: 2 的 enemy/waves 字段。');
      project.version = VERSION; project.enemy = defaultEnemy(); project.waves = [];
    }
    return requireValid(project);
  }
  function serialize(project) {
    requireValid(project);
    const text = JSON.stringify(project, null, 2) + '\n';
    if (byteLength(text) > MAX_BYTES) throw new Error('导出 JSON 超过 2 MiB；请缩短备注或减少对象。');
    return text;
  }
  function round(n) { return Math.round((n + Number.EPSILON) * 1000000) / 1000000; }
  function toUnreal(project) {
    requireValid(project);
    return { format: FORMAT + '.ue-manifest', version: VERSION, sourceProjectId: project.meta.id,
      field: { widthCm: round(project.field.width * 100), heightCm: round(project.field.height * 100), gridCm: round(project.field.grid * 100) },
      settings: Object.assign({}, project.settings),
      enemy: Object.assign({}, project.enemy, { cards: project.enemy.cards.slice() }),
      waves: project.waves.map(w => Object.assign({}, w, { location: { X: round((project.field.height / 2 - w.y) * 100), Y: round((w.x - project.field.width / 2) * 100), Z: 0 }, radiusCm: round(w.radius * 100) })),
      notes: ['坐标参考清单，不是 Unreal 自动导入文件；试玩脚本应使用原始设计 JSON。', '坐标单位为厘米；左上地图原点转换到战场中心。', '角色群组只导出锚点；运行时检查生成空间与容量。', 'enemy-zone 与 note 仅作设计标注，其他地形按游戏规则加载。'],
      objects: project.objects.map(function (o) {
        const m = getModule(o.moduleId);
        const mapped = { id: o.id, moduleId: o.moduleId, label: o.label, role: m.role, implementation: m.implementation,
          location: { X: round((project.field.height / 2 - o.y) * 100), Y: round((o.x - project.field.width / 2) * 100), Z: 0 },
          rotation: { Pitch: 0, Yaw: round(90 + o.rotation), Roll: 0 },
          sizeCm: { width: round(o.width * 100), height: round(o.height * 100) },
          team: o.team, count: o.count, spawnAtSeconds: o.spawnAtSeconds, note: o.note };
        if (m.unitId) mapped.unitId = m.unitId;
        return mapped;
      }) };
  }
  return Object.freeze({ FORMAT, VERSION, MAX_BYTES, MAX_OBJECTS, MAX_WAVES, MODULES, GRIDS, TEAMS, CARDS, WAVE_UNITS, TRIGGERS,
    getModule, createProject, createObject, createExample, createTerrainExample, createWave, triggerHeroes, validate, parseProject, serialize, toUnreal, footprint });
});
