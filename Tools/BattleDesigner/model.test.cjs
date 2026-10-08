'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const M = require('./model.js');

function example() { return M.createExample(); }
function find(p, id) { return p.objects.find(o => o.moduleId === id); }
function fails(p, message) {
  assert.ok(M.validate(p).errors.length, message);
  assert.throws(() => M.serialize(p), undefined, message);
}
function projectWith(object) { const p = M.createProject(); p.objects = [object]; return p; }

test('CommonJS and browser UMD expose the same contract without DOM or network', () => {
  const context = vm.createContext({ TextEncoder });
  vm.runInContext(fs.readFileSync(path.join(__dirname, 'catalog.js'), 'utf8'), context);
  vm.runInContext(fs.readFileSync(path.join(__dirname, 'units.js'), 'utf8'), context);
  vm.runInContext(fs.readFileSync(path.join(__dirname, 'model.js'), 'utf8'), context);
  assert.ok(context.LKLevel);
  assert.equal(context.LKLevel.FORMAT, M.FORMAT);
  assert.equal(context.LKLevel.MODULES.length, M.MODULES.length);
  assert.equal(context.LKLevel.validate(context.LKLevel.createExample()).errors.length, 0);
});

test('default project is a valid 40×24 m document with ISO dates and unique IDs', () => {
  const p = M.createProject();
  assert.deepEqual(p.field, { width: 40, height: 24, grid: 1 });
  assert.deepEqual(p.settings, { encounterId: 'Patrol', seed: 12345, battleTimeLimit: 300 });
  assert.equal(p.meta.createdAt, new Date(p.meta.createdAt).toISOString());
  assert.notEqual(p.meta.id, M.createProject().meta.id);
  assert.deepEqual(M.validate(p).errors, []);
  assert.ok(M.validate(p).warnings.some(s => s.includes('敌方角色')));
});

test('all modules have stable unique IDs, legal roles, default dimensions and explicit source IDs', () => {
  const ids = new Set();
  for (const m of M.MODULES) {
    assert.ok(!ids.has(m.id)); ids.add(m.id);
    assert.equal(M.getModule(m.id), m);
    assert.ok(['terrain', 'obstacle', 'decoration', 'zone', 'unit', 'building', 'note'].includes(m.role));
    assert.ok(['existing', 'design', 'runtime'].includes(m.implementation));
    assert.ok(M.TEAMS.includes(m.defaultTeam));
    assert.ok(m.defaultWidth > 0 && m.defaultHeight > 0);
    if (m.implementation === 'existing') assert.match(m.unitId, /^(Unit|Building|Hero|Boss)_/);
  }
  for (const id of ['grass', 'road', 'water', 'rock', 'tree', 'player-zone', 'enemy-zone', 'wall-common', 'wall-rare', 'wall-epic', 'wall-imperial', 'mage-tower', 'healing-station', 'siege-cart', 'skeleton', 'skeleton-archer', 'necromancer', 'skeleton-giant', 'skeleton-king', 'note']) assert.ok(ids.has(id));
  assert.equal(M.getModule('necromancer').unitId, 'Hero_Necromancer');
  assert.equal(M.getModule('skeleton-king').unitId, 'Boss_SkeletonKing');
  assert.equal(M.getModule('wall-imperial').unitId, 'Unit_WallImperial');
  assert.equal(M.getModule('__proto__'), undefined);
});

test('factories reject unknown IDs and nonfinite coordinates and do not share mutable state', () => {
  assert.throws(() => M.createObject('no-such-module', 2, 2));
  assert.throws(() => M.createObject('rock', NaN, 2));
  assert.throws(() => M.createObject('rock', 2, Infinity));
  const a = M.createObject('rock', 3, 3), b = M.createObject('rock', 3, 3);
  assert.notEqual(a.id, b.id);
  a.width = 9; assert.equal(b.width, 2);
  assert.equal(M.getModule('rock').defaultWidth, 2);
});

test('meaningful example roundtrips without data loss and export does not mutate source', () => {
  const p = example();
  assert.deepEqual(M.validate(p).errors, []);
  assert.ok(p.objects.some(o => o.spawnAtSeconds > 0));
  assert.ok(p.objects.some(o => o.team === 'enemy'));
  const snapshot = JSON.stringify(p);
  const encoded = M.serialize(p);
  assert.ok(encoded.includes('\n  "format"'));
  assert.deepEqual(M.parseProject(encoded), p);
  M.toUnreal(p);
  assert.equal(JSON.stringify(p), snapshot);
});

test('reject invalid JSON, root structure, format and version', () => {
  for (const text of ['', 'not json', 'null', '[]', '42']) assert.throws(() => M.parseProject(text));
  for (const value of [null, [], 'map', 4]) assert.ok(M.validate(value).errors.length);
  const p = example(); p.format = 'other-game'; fails(p);
  p.format = M.FORMAT; p.version = 99; fails(p);
  assert.throws(() => M.parseProject(JSON.stringify(p)), /version/);
});

test('strict required fields, unexpected fields and nested data types are rejected', () => {
  for (const mutate of [
    p => { delete p.meta.title; },
    p => { p.field.unknown = 1; },
    p => { p.settings = []; },
    p => { p.objects = {}; },
    p => { p.objects[0] = null; },
    p => { delete p.objects[0].count; },
    p => { p.objects[0].note = { text: 'nested' }; },
    p => { p.meta.createdAt = 'yesterday'; },
    p => { p.meta = new Date(); }
  ]) { const p = example(); mutate(p); fails(p); }
});

test('prototype pollution fields are rejected at every depth before accepting a file', () => {
  const encoded = M.serialize(example());
  for (const key of ['__proto__', 'constructor', 'prototype']) {
    const malicious = encoded.replace('"format":', '"' + key + '": {"polluted": true}, "format":');
    assert.throws(() => M.parseProject(malicious), /原型字段/);
    const nested = encoded.replace('"meta": {', '"meta": {"' + key + '": {"polluted": true},');
    assert.throws(() => M.parseProject(nested), /原型字段/);
  }
  assert.equal({}.polluted, undefined);
  const p = example(); p.objects[0] = Object.assign(Object.create({ inherited: true }), p.objects[0]);
  fails(p);
});

test('sparse arrays, accessors and hidden serialization hooks are rejected', () => {
  const sparse = example(); delete sparse.objects[2]; fails(sparse);
  const hooked = example(); Object.defineProperty(hooked.meta, 'toJSON', { value: () => ({}) }); fails(hooked);
  const hookedArray = example(); hookedArray.objects.toJSON = () => []; fails(hookedArray);
  const accessor = example();
  Object.defineProperty(accessor.objects[0], 'label', { get() { throw new Error('getter must not run'); } });
  fails(accessor);
});

test('2 MiB input limit is UTF-8 bytes rather than JavaScript string length', () => {
  assert.throws(() => M.parseProject(' '.repeat(M.MAX_BYTES + 1)), /2 MiB/);
  const unicode = '中'.repeat(Math.ceil(M.MAX_BYTES / 3));
  assert.ok(unicode.length < M.MAX_BYTES);
  assert.throws(() => M.parseProject(unicode), /2 MiB/);
  assert.throws(() => M.parseProject({}), /JSON 文本/);
});

test('object count limit is inclusive and duplicate object IDs fail', () => {
  const p = M.createProject();
  p.objects = Array.from({ length: 500 }, () => M.createObject('skeleton', 20, 12));
  assert.deepEqual(M.validate(p).errors, []);
  p.objects.push(M.createObject('skeleton', 20, 12)); fails(p, '501 rejected');
  p.objects.pop(); p.objects[1].id = p.objects[0].id; fails(p, 'duplicate rejected');
});

test('all numeric object fields reject NaN, Infinity, strings and extreme ranges', () => {
  for (const key of ['x', 'y', 'width', 'height', 'rotation', 'count', 'spawnAtSeconds']) {
    for (const invalid of [NaN, Infinity, -Infinity, '2', null]) {
      const p = example(); p.objects[0][key] = invalid; fails(p, key + '=' + invalid);
    }
  }
  for (const [key, invalid] of [['width', 0], ['height', -1], ['count', 0], ['count', 21], ['count', 1.5], ['spawnAtSeconds', -1], ['spawnAtSeconds', 301], ['rotation', 3601], ['x', -1], ['y', 101]]) {
    const p = example(); p.objects[0][key] = invalid; fails(p, key + '=' + invalid);
  }
});

test('field, grid, settings and text limits are checked without coercion', () => {
  for (const mutate of [
    p => { p.field.width = 7; }, p => { p.field.height = 101; }, p => { p.field.grid = 0.75; },
    p => { p.field.width = '40'; }, p => { p.settings.seed = -1; },
    p => { p.settings.seed = 2147483648; }, p => { p.settings.battleTimeLimit = 0; },
    p => { p.settings.battleTimeLimit = 1.1; }, p => { p.meta.title = ''; },
    p => { p.meta.description = 'x'.repeat(4001); }, p => { p.objects[0].note = 'x'.repeat(4001); },
    p => { p.objects[0].label = 'x'.repeat(101); }, p => { p.meta.id = '../unsafe'; }
  ]) { const p = example(); mutate(p); fails(p); }
  for (const grid of M.GRIDS) { const p = example(); p.field.grid = grid; assert.deepEqual(M.validate(p).errors, []); }
});

test('enemy-only units cannot be placed as player or neutral, and zones preserve their meaning', () => {
  for (const id of ['skeleton', 'skeleton-archer', 'necromancer', 'skeleton-giant', 'skeleton-king']) {
    for (const team of ['player', 'neutral']) { const o = M.createObject(id, 20, 12); o.team = team; fails(projectWith(o)); }
  }
  for (const [id, team] of [['wall-common', 'neutral'], ['knight', 'neutral'], ['player-zone', 'enemy'], ['enemy-zone', 'player']]) {
    const o = M.createObject(id, 20, 12); o.team = team; fails(projectWith(o));
  }
});

test('complete rotated rectangle is checked at all four boundaries, not only its center', () => {
  for (const [x, y, rotation] of [[1, 12, 0], [39, 12, 0], [20, 0.3, 90], [20, 23.7, 90], [1.1, 12, 45]]) {
    const o = M.createObject('wall-common', x, y); o.rotation = rotation;
    fails(projectWith(o), 'full wall footprint outside');
  }
  const o = M.createObject('wall-common', 1.5, 0.18);
  assert.deepEqual(M.validate(projectWith(o)).errors, [], 'exact boundary allowed');
  o.rotation = 180;
  assert.deepEqual(M.validate(projectWith(o)).errors, [], 'floating point tolerance on boundary');
  o.rotation = 90; o.x = 0.18; o.y = 1.5;
  assert.deepEqual(M.validate(projectWith(o)).errors, []);
});

test('footprint uses clockwise screen rotation and a center anchor', () => {
  const o = M.createObject('wall-common', 10, 8); o.width = 4; o.height = 2; o.rotation = 90;
  const corners = M.footprint(o);
  assert.deepEqual(corners.map(p => [Math.round(p.x), Math.round(p.y)]), [[11, 6], [11, 10], [9, 10], [9, 6]]);
});

test('Unreal export transforms meters and clockwise orientation consistently', () => {
  const p = M.createProject();
  const o = M.createObject('wall-common', 4, 7); o.rotation = 30; o.team = 'enemy'; o.count = 2; o.spawnAtSeconds = 12.5;
  p.objects = [o];
  const ue = M.toUnreal(p), result = ue.objects[0];
  assert.equal(ue.format, 'little-king.battle-layout.ue-manifest');
  assert.deepEqual(ue.field, { widthCm: 4000, heightCm: 2400, gridCm: 100 });
  assert.deepEqual(result.location, { X: 500, Y: -1600, Z: 0 });
  assert.deepEqual(result.rotation, { Pitch: 0, Yaw: 120, Roll: 0 });
  assert.deepEqual(result.sizeCm, { width: 300, height: 36 });
  assert.equal(result.unitId, 'Unit_WallCommon');
  assert.equal(result.count, 2); assert.equal(result.spawnAtSeconds, 12.5);
  assert.equal(result.team, 'enemy');
  ue.settings.seed = 99; assert.equal(p.settings.seed, 12345);
  o.x = 20; o.y = 12; o.rotation = -90;
  assert.deepEqual(M.toUnreal(p).objects[0].location, { X: 0, Y: 0, Z: 0 });
  assert.equal(M.toUnreal(p).objects[0].rotation.Yaw, 0);
});

test('terrain and player zones are runtime modules while enemy zone remains annotation', () => {
  const ue = M.toUnreal(example());
  assert.ok(ue.notes.some(t => t.includes('不是 Unreal 自动导入')));
  const rock = ue.objects.find(o => o.moduleId === 'rock');
  assert.equal(rock.implementation, 'runtime');
  assert.equal(Object.prototype.hasOwnProperty.call(rock, 'unitId'), false);
  assert.equal(ue.objects.find(o => o.moduleId === 'enemy-zone').implementation, 'design');
  assert.equal(ue.objects.find(o => o.moduleId === 'player-zone').implementation, 'runtime');
});

test('narrow passages are advisory, overlapping wall segments remain legal', () => {
  const p = M.createProject();
  p.objects = [M.createObject('rock', 10, 10), M.createObject('rock', 12.5, 10)];
  assert.ok(M.validate(p).warnings.some(w => w.includes('窄通道')));
  assert.deepEqual(M.validate(p).errors, []);
  p.objects = [M.createObject('wall-common', 10, 10), M.createObject('wall-common', 11, 10)];
  assert.deepEqual(M.validate(p).errors, []);
});

test('large valid notes cannot bypass export size cap', () => {
  const p = M.createProject();
  p.objects = Array.from({ length: 500 }, () => { const o = M.createObject('skeleton', 20, 12); o.note = '中'.repeat(1600); return o; });
  assert.deepEqual(M.validate(p).errors, []);
  assert.throws(() => M.serialize(p), /2 MiB/);
});

test('v1 import migrates once without discarding objects or silently accepting v2 fields', () => {
  const old = example(); old.version = 1; delete old.enemy; delete old.waves;
  const migrated = M.parseProject(JSON.stringify(old));
  assert.equal(migrated.version, 2); assert.deepEqual(migrated.objects, old.objects);
  assert.equal(migrated.enemy.usesCards, false); assert.deepEqual(migrated.waves, []);
  assert.deepEqual(M.parseProject(M.serialize(migrated)), migrated);
  old.enemy = migrated.enemy; assert.throws(() => M.parseProject(JSON.stringify(old)), /version: 1/);
});

test('terrain ambush has four actual terrain modules, three trigger types and enemy economy', () => {
  const p = M.createTerrainExample(); assert.deepEqual(M.validate(p).errors, []);
  assert.equal(p.enemy.usesCards, true); assert.equal(p.enemy.silverPerSecond, 1.5);
  for (const id of ['brush','swamp','campfire','mountain']) { assert.ok(find(p,id)); assert.equal(M.getModule(id).implementation,'runtime'); }
  assert.deepEqual(p.waves.map(w => w.trigger), ['time','hero-health-below','hero-first-death']);
  assert.deepEqual(M.parseProject(M.serialize(p)), p);
  const disk = M.parseProject(fs.readFileSync(path.join(__dirname,'examples/terrain-ambush.json'),'utf8'));
  assert.deepEqual(disk.waves.map(w => w.trigger), p.waves.map(w => w.trigger));
});

test('wave trigger heroes are single opening hero instances, including bosses and either team', () => {
  const p = M.createTerrainExample(); const target = find(p,'necromancer'); const w = p.waves[1];
  assert.equal(M.triggerHeroes(p)[0].id, target.id);
  for (const mutate of [o => { o.count=2; },o => { o.spawnAtSeconds=1; },o => { o.moduleId='skeleton'; }]) {
    const q=JSON.parse(JSON.stringify(p)); mutate(q.objects.find(o=>o.id===target.id)); fails(q);
  }
  const q=JSON.parse(JSON.stringify(p)); q.objects=q.objects.filter(o=>o.id!==target.id); fails(q,'deleting referenced hero rejected');
  q.waves=q.waves.filter(v=>v.targetId!==target.id); assert.deepEqual(M.validate(q).errors,[]);
  w.targetId='not-present'; fails(p);
});

test('wave times are delays for conditional events; health threshold stays strictly within 0..1', () => {
  const p=M.createTerrainExample(); p.waves[1].time=3600; p.waves[1].healthThreshold=.01; assert.deepEqual(M.validate(p).errors,[]);
  p.waves[1].healthThreshold=.99; assert.deepEqual(M.validate(p).errors,[]);
  for (const [key,value] of [['time',-1],['time',3601],['healthThreshold',0],['healthThreshold',1],['count',0],['count',51],['count',1.5],['radius',-1],['radius',10.1],['x',41],['y',25],['trigger','repeat']]) {
    const q=M.createTerrainExample(); q.waves[1][key]=value; fails(q,key);
  }
  p.waves[0].targetId=find(p,'necromancer').id; fails(p,'time waves must not carry misleading condition IDs');
});

test('wave counts and IDs are bounded and cannot collide with map objects', () => {
  const p=M.createProject(); p.waves=Array.from({length:64},()=>M.createWave(20,12)); assert.deepEqual(M.validate(p).errors,[]);
  p.waves.push(M.createWave(20,12)); fails(p); p.waves.pop(); p.waves[1].id=p.waves[0].id; fails(p);
  const q=M.createTerrainExample();q.waves[0].id=q.objects[0].id;fails(q);
});

test('wave and economic numeric fields reject nonfinite values and numeric strings', () => {
  for (const key of ['time','healthThreshold','count','x','y','radius']) for(const value of [NaN,Infinity,'2',null]) { const p=M.createTerrainExample(); p.waves[0][key]=value; fails(p,key); }
  for (const key of ['silverPerSecond','silverCap','startingSilver']) for(const value of [NaN,Infinity,'2',null]) { const p=M.createTerrainExample(); p.enemy[key]=value; fails(p,key); }
});

test('enemy card IDs are canonical and unique, and active decks are nonempty', () => {
  const p=M.createProject(); p.enemy.cards=[]; assert.deepEqual(M.validate(p).errors,[]);
  p.enemy.usesCards=true; fails(p); p.enemy.cards=['Unit_Skeleton'];assert.deepEqual(M.validate(p).errors,[]);
  p.enemy.cards.push('Unit_Skeleton');fails(p);p.enemy.cards=['Unit_WallCommon'];fails(p);
  p.enemy.cards=['Card_WallCommon','Spell_Freeze'];assert.deepEqual(M.validate(p).errors,[]);
  p.enemy.usesCards='true';fails(p);
  assert.equal(new Set(M.CARDS.map(c=>c.id)).size,M.CARDS.length);
});

test('enemy silver has explicit bounds and cannot start above the cap', () => {
  for(const [key,value] of [['silverPerSecond',-1],['silverPerSecond',100.1],['silverCap',0],['silverCap',1001],['startingSilver',-1],['startingSilver',21]]) { const p=M.createProject();p.enemy[key]=value;fails(p,key); }
  const p=M.createProject();p.enemy.silverPerSecond=0;p.enemy.silverCap=1;p.enemy.startingSilver=0;assert.deepEqual(M.validate(p).errors,[]);
});

test('v2 arrays reject holes, accessors and custom serialization hooks', () => {
  for(const key of ['waves','cards']) {
    const p=M.createTerrainExample(), a=key==='waves'?p.waves:p.enemy.cards; delete a[0];fails(p);
    const q=M.createTerrainExample(), b=key==='waves'?q.waves:q.enemy.cards;b.toJSON=()=>[];fails(q);
    const r=M.createTerrainExample(), c=key==='waves'?r.waves:r.enemy.cards;Object.defineProperty(c,0,{get(){throw new Error('must not execute getter');}});fails(r);
  }
});

test('UE manifest carries independent enemy and wave copies with centimeter coordinates', () => {
  const p=M.createTerrainExample();const original=JSON.stringify(p);const ue=M.toUnreal(p);
  assert.deepEqual(ue.waves[0].location,{X:0,Y:900,Z:0});assert.equal(ue.waves[0].radiusCm,200);
  ue.enemy.cards.length=0;ue.waves[0].time=900;assert.equal(JSON.stringify(p),original);
});

test('schema enumerations match the shipped model and card catalog', () => {
  const s=JSON.parse(fs.readFileSync(path.join(__dirname,'schema.json'),'utf8'));
  assert.equal(s.properties.version.const,M.VERSION);
  assert.deepEqual(s.properties.objects.items.properties.moduleId.enum,M.MODULES.map(m=>m.id));
  assert.deepEqual(s.properties.enemy.properties.cards.items.enum,M.CARDS.map(c=>c.id));
  assert.deepEqual(s.properties.waves.items.properties.unitId.enum,M.WAVE_UNITS.map(u=>u.id));
});

test('canonical card catalog tracks every source-defined playable card ID', () => {
  const source = name => fs.readFileSync(path.join(__dirname,'../../Source/little_king',name),'utf8');
  const ids = new Set(['Unit_Skeleton','Unit_SkeletonArcher','Spell_SkeletonCircle']);
  for(const m of source('ULKGameData.cpp').matchAll(/AddCard\(TEXT\("([^"]+)"\)/g))ids.add(m[1]);
  for(const m of source('LKExpeditionMercenaryContent.cpp').matchAll(/Add\("([^"]+)",\s*TEXT/g))ids.add(m[1]);
  for(const m of source('LKResearchContent.cpp').matchAll(/\{"([^"]+)",FText::FromString/g))ids.add(m[1]);
  assert.deepEqual([...ids].sort(),M.CARDS.map(c=>c.id).sort());
});

test('wave unit catalog covers all five LKUnitContent registries and uses UnitId rather than CardId', () => {
  const ids = new Set();
  for(const file of ['LKUnitContent.cpp','LKUndeadContent.cpp','LKExpeditionMercenaryContent.cpp','LKV083Content.cpp','LKV084Content.cpp']){
    const source=fs.readFileSync(path.join(__dirname,'../../Source/little_king',file),'utf8');
    for(const m of source.matchAll(/Add\("([^"]+)",\s*TEXT/g))ids.add(m[1]);
  }
  assert.deepEqual([...ids].sort(),M.WAVE_UNITS.map(u=>u.id).sort());
  const p=M.createTerrainExample();p.waves[0].unitId='Unit_WallCommon';assert.deepEqual(M.validate(p).errors,[]);
  p.waves[0].unitId='Unit_AngelArcher_Rare';assert.deepEqual(M.validate(p).errors,[]);
  p.waves[0].unitId='Card_WallCommon';fails(p);
});

test('engine import restrictions reject duplicate hero kinds, player delays and grouped map heroes', () => {
  const p=M.createTerrainExample();p.objects.push(M.createObject('necromancer',30,12));fails(p);
  const q=M.createProject();const hero=M.createObject('knight',8,12);hero.count=2;q.objects.push(hero);fails(q);
  hero.count=1;hero.spawnAtSeconds=1;fails(q);hero.moduleId='swordsman';fails(q);
});

test('legacy timed objects share the 64-wave budget and game IDs are case insensitive', () => {
  const p=M.createProject();const o=M.createObject('skeleton',30,12);o.spawnAtSeconds=1;p.objects.push(o);
  p.waves=Array.from({length:63},()=>M.createWave(30,12));assert.deepEqual(M.validate(p).errors,[]);
  p.waves.push(M.createWave(30,12));fails(p);
  const q=M.createProject();q.objects=[M.createObject('skeleton',30,12),M.createObject('skeleton',31,12)];q.objects[0].id='Guard-A';q.objects[1].id='guard-a';fails(q);
});
