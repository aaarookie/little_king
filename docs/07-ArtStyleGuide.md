# 美术风格指南（AI 出图统一画风）

**风格定位**：宫崎骏式唯美 × 极繁装饰（Ghibli-inspired aesthetic × maximalist ornate）
**参考氛围**：千与千寻 / 哈尔的移动城堡 / 借东西的小人阿莉埃蒂 的温暖手绘感 + 华丽繁复的宫廷装饰细节
**适合本作**：温暖童话的王国（家园/战场）× 华丽残垣的地牢（不靠黑暗血腥，靠繁复与幽光营造氛围）

---

## ① 风格锚点（最重要！每个提示词原样粘贴这一段）

英文提示词（AI 出图英文效果最好，**每次生成原样复制，不要改动措辞顺序**）：

```
Hand-painted 2D game art in a Studio Ghibli-inspired storybook style;
soft gouache and watercolor textures; dreamy warm lighting with gentle god rays;
lush painterly detail; whimsical fairy-tale mood blended with richly ornate
maximalist decoration, intricate filigree patterns and ornamental flourishes;
gentle muted earth-tone palette with emerald green, amber gold and dusty rose accents;
clean confident outlines; flat painterly shading; charming, elegant and beautiful;
no photorealism, no 3D rendering, no dark horror
```

**中文释义**（理解用，不要贴中文进提示词）：
手绘 2D · 吉卜力故事书风 · 水彩/水粉质感 · 梦幻暖光与光束 · 丰富笔触细节 · 童话氛围 × 华丽极繁装饰（繁复花纹/装饰卷草）· 柔和大地色调 + 翡翠绿/琥珀金/灰玫瑰点缀 · 干净轮廓线 · 平涂质感 · 不写实/不 3D/不黑暗恐怖

**为什么"极繁"这样写**：`richly ornate maximalist decoration, intricate filigree patterns and ornamental flourishes` 这组词管"繁复华丽"，`soft gouache and watercolor` + `dreamy warm lighting` 管"唯美宫崎骏"，两者缺一不可。

---

## ② 通用公式

```
[风格锚点] + [主体描述] + [构图/格式约束]
```

三段固定顺序，**锚点永远在最前**。下面每个分类模板都已拼好，只需替换【】里的变量。

---

## ③ 分类模板（可直接复制）

### A. 角色设定图（每个角色先做这张，作为一致性参考图！）

```
[风格锚点],
character reference sheet of a [角色描述], three views: front, side and back,
standing upright, full body, clean simple pose, showing complete outfit design,
[特征清单], [标志物], consistent colors across all views,
character concept art, centered, single character, no text
```

> 出图后用这张做**参考图**（把图拖进 ChatGPT image 对话 → "based on this reference image, same character, ..."），所有衍生图都基于它，保证同一个角色长得一样。

### B. 角色精灵（用于 DT_Units 的单位外观）

```
[风格锚点],
full-body game sprite of a [角色描述], standing facing forward in three-quarter view,
[特征清单], [标志物], wearing [服装],
confident readable silhouette, visible at small size on a battlefield,
single character, full body with margin around, flat solid pure white background, no text
```

> **透明底说明**：ChatGPT image 不一定输出真透明 PNG。需要抠图时把最后一句换成 `flat solid pure green background (#00FF00)`，导入后用编辑器/PS 一键去绿底；纯白底则适合"颜色减淡"混合去底。

### C. 卡牌图标（方形 1:1，教程 A 用）

```
[风格锚点],
square game card icon of [主体], centered composition,
ornate gilded decorative frame with filigree corner ornaments,
miniature detailed scene inside the frame, magical glow accents,
bold readable shapes at small size, rich background color in [主色],
square format, no text
```

### D. 建筑（箭塔/兵营——2.5D 战场视角）

```
[风格锚点],
2D game building sprite of [建筑描述], front-facing facade with slightly visible roof top,
designed for a top-down battle arena viewed from a tilted camera,
ornate medieval fantasy architecture, [细节],
warm stone and wood textures with golden trims, cozy lived-in details,
isolated on flat solid pure white background, centered, no ground shadow, no text
```

### E. 地牢房间背景（俯视战斗场地，可平铺可整张）

```
[风格锚点],
2D game environment background of a dungeon arena room, top-down view,
ornate carved stone floor with moss and glowing runes,
ancient pillars with filigree carvings, hanging lanterns and climbing vines,
mist and floating dust in god rays, playable open area in the center,
soft magical light from above, painted background, no characters, no text
```

> 变体词：`crystal cavern`（水晶洞）/ `royal library ruins`（皇家图书馆废墟）/ `overgrown garden court`（荒废花园庭院）/ `boss chamber`（Boss 室：更大空间 + 中央王座石台）。

### F. 家园背景（国王视角的温暖庭院）

```
[风格锚点],
2D game environment background of a cozy kingdom home base, top-down view,
castle courtyard with blooming flower garden, golden banners and wind chimes,
stone paths with ornate mosaics, afternoon sunlight through tree leaves,
playable open area in the center, painted background, no characters, no text
```

### G. 法术/特效（火球、治疗波）

```
[风格锚点],
spell effect illustration of [火球术/治疗波], swirling ornate magic circle,
glowing particles and ribbons of light, ethereal and beautiful,
strong glow on deep [背景色] background, high contrast, no text
```

### H. UI 装饰（边框/面板/按钮，极繁主战场）

```
[风格锚点],
ornate 2D game UI panel frame asset, gilded baroque-style border with floral
ornaments and corner flourishes, [主色] with gold accents,
four separate corner pieces and straight edge pieces, empty transparent center,
UI asset, no text
```

---

## ④ 本作角色设定对照表（A/B 模板的【】填这里）

| 项目 ID | 角色 | 提示词描述（英文直接可用） | 主色 |
|---|---|---|---|
| Hero_Knight | 骑士 | `young golden-haired knight in ivory-and-gold plate armor with lion motifs, holding an ornate lion-crest helm under one arm, noble and gentle expression` | 象牙白+暖金 |
| Hero_Mage | 法师 | `elderly silver-haired mage in a starry violet robe with a hood draped back, holding a staff topped with a glowing star crystal, wise kind eyes` | 星紫+金 |
| Hero_Ranger | 游侠 | `nimble forest ranger with short auburn hair, deep emerald cloak with leaf embroidery, longbow on back and quiver at hip, alert warm eyes` | 森林绿+琥珀 |
| Unit_Swordsman | 剑士 | `young swordsman in light blue travel armor, crimson headband, simple steel sword, determined but cheerful look` | 蓝灰+绯红 |
| Unit_Archer | 弓箭手 | `swift archer girl with twin braids, tan leather tunic and green hood, recurve bow drawn at ease` | 土棕+草绿 |
| Unit_Shieldbearer | 盾卫 | `stocky shield warrior with ginger beard, copper armor, huge round tower shield with engraved sun emblem` | 铜+土棕 |
| 敌兵（骷髅兵） | | `cute-but-creepy animated skeleton soldier in rusty ornate armor, hollow eyes with faint blue soul flame, chipped sword` | 暗灰+幽蓝 |
| Building_ArrowTower | 箭塔 | `tall ornate arrow tower with conical emerald roof, golden weathervane, **a guard archer in green hood and tan leather standing at the open top battlement, drawing his bow with an arrow nocked**, warm light spilling from arrow slits, ivy creeping up` | 石色+翡翠顶 |
| Building_Barracks | 兵营 | `cozy stone barracks with wooden training dummies outside, hanging banners and a small forge chimney, warm lantern light in windows` | 石色+暖棕 |
| Spell_Fireball | 火球术 | `fireball with a heart-shaped core, swirling golden flames forming ornate spirals` | 琥珀金 |
| Spell_HealWave | 治疗波 | `gentle healing wave of emerald light with floating petals and sparkles, soft circular ripple` | 翡翠绿 |

**敌方通用**：`rusty ornate armor, faint blue soul-flame eyes`（生锈华丽盔甲+幽蓝魂火）——保持"华丽但破败"，不吓人。

---

## ④.5 示例：箭塔专属提示词（含塔内弓箭手）

**设计要点**（先读再出图）：
- **弓箭手必须站在顶层开放式箭垛平台上**（不是在窄箭窗里）——战场上塔是很小的单位，弓手要有清晰剪影
- 姿态用"**挽弓待发**"（arrow nocked, bow drawn）：这是塔的默认"攻击准备"状态；将来程序化攻击动画（前摇/闪光/飘箭）会配合这张图，不需要另出攻击帧
- 弓箭手与玩家弓手单位（Unit_Archer）同属"绿兜帽+皮革"卫兵家族，但**不必是同一个角色**（塔卫可以更壮实）
- 极繁细节放塔身（雕花/藤蔓/金饰/暖光箭窗），弓手本身保持简洁

**主提示词（可直接复制）**：

```
Hand-painted 2D game art in a Studio Ghibli-inspired storybook style;
soft gouache and watercolor textures; dreamy warm lighting with gentle god rays;
lush painterly detail; whimsical fairy-tale mood blended with richly ornate
maximalist decoration, intricate filigree patterns and ornamental flourishes;
gentle muted earth-tone palette with emerald green, amber gold and dusty rose accents;
clean confident outlines; flat painterly shading; charming, elegant and beautiful;
no photorealism, no 3D rendering, no dark horror,
2D game building sprite of an ornate medieval arrow tower with a defending archer inside,
front-facing facade with slightly visible roof top, designed for a top-down battle arena
viewed from a tilted camera: a stocky guard archer in green hood and tan leather tunic
stands at the open top battlement platform behind the crenellations,
drawing his bow with an arrow nocked and a faint golden glow at the arrow tip,
ready to shoot; warm lantern light spills from the arrow slits below,
conical emerald roof with golden weathervane, ornate carved stone walls,
ivy creeping up, cozy lived-in details,
single tower and single archer, isolated on flat solid pure white background,
centered, no ground shadow, no text
```

**两个变体**（微调描述句即可）：

| 用途 | 修改 |
|---|---|
| 强调"正在射击" | 把 `ready to shoot` 换成 `releasing the arrow, arrow mid-flight leaving a golden light trail` |
| 敌方据点版（若以后分阵营塔） | 弓手描述改为 `skeleton archer in rusty ornate armor with faint blue soul-flame eyes` |

**一致性提示**：出图前可先贴一张已满意的场景图（"match the color palette and style of this reference image"）；塔身主色暖石灰+翡翠顶，弓手兜帽草绿，与全游戏色板（翡翠绿/琥珀金/灰玫瑰）一致。

---

## ⑤ 负面清单（写进提示词或作为要求）

```
no text, no letters, no watermark, no signature, no border frame (except card/UI prompts),
no photorealism, no 3D render, no CGI gloss, no dark gritty horror gore,
no extra characters, no weapons pointing at viewer, no distorted anatomy, no extra limbs
```

---

## ⑥ 一致性工作流（防止角色漂移）

1. **每张图都带风格锚点**，一个字不改
2. **先出设定图**（模板 A，三视图）→ 之后所有该角色图以它做参考图
3. 固定角色三要素：发型发色 / 服装主色 / 标志物（骑士=狮盔、法师=星光法杖、游侠=绿斗篷）——生成时在描述里**每次都写全**
4. 固定构图：角色=全身 3/4 正面；图标=方形居中；建筑=正面+微露顶
5. 同一批图一次性生成（同会话同模型），跨天生成时先贴一张旧图找感觉
6. 不满意就改**主体描述**，**不要改锚点**——出问题 90% 出在锚点被改

---

## ⑦ 与项目资产的对齐

- 卡牌图标 → 对应教程 [06-Sprint2Guide.md](06-Sprint2Guide.md) 的 A 步骤（Icon 字段）
- 角色精灵 → 教程 B（DT_Units.Sprite）
- 生成文件的命名直接用项目 ID（`Hero_Knight.png`、`Spell_Fireball.png` 等），导入后好对应

---

## ⚠️ ⑧ 显示技术提醒（重要，做角色精灵前先看）

当前战场相机是**正俯视（Pitch -90）**，色块调试期没问题；但换成真实角色精灵后，正俯视会把角色看成"贴在地上的纸片"，观感差。**推荐改为 2.5D 战场视角**（皇室战争/元气骑士式）：
- 相机改为俯视 55°~65°（Pitch -55 ~ -65，保持正交）
- 角色精灵"立起来"（朝向相机），建筑/地面保持平铺
- 战场逻辑（XY 平面坐标/索敌/移动）**完全不用改**，只改表现层

所以角色出图请用**正面/3/4 视角的站姿立绘**（上面模板已如此设计），不要出"俯视头顶"的图。需要我改相机方案时告诉我——会顺带把精灵朝向、调试框绘制一起调整好，再让你测试。

---

## 🎯 ⑨ 从三视图到战斗精灵（转化流程）

### 先理清"三档美术资源"金字塔

| 档次 | 用途 | 精度 | 来源 |
|---|---|---|---|
| **立绘（卡面/部署头像）** | 卡牌图标、英雄选择头像 | ★★★ 华丽精细 | 三视图/单独立绘 |
| **战斗精灵** | DT_Units.Sprite，战场上显示 | ★ 简洁可读（小尺寸 + 轮廓清晰） | **以三视图为参考 AI 重绘** |
| **环境/特效/UI** | 地牢背景、法术粒子、边框 | 按需 | 分类模板 |

**关键认知：战斗精灵不是把立绘缩小，而是让 AI"照着参考图重画一个简化的自己"。** 手工缩图会糊成一团；AI 重绘能在保留角色识别度的同时主动删细节。

### 转化流程（每个角色 4 步）

**第 1 步：裁出正面图**
把三视图里**正面视角单独裁出来**（系统画图/PS 都行），保存为 `Hero_Knight_front.png`。
> ⚠️ 参考图越"干净"越好：整张三视图一起当参考，AI 容易把三个视角的特征混在一起。

**第 2 步：上传正面图做参考 + 贴"转化提示词 v2（强约束版）"**
新对话 → 上传正面图 → 粘贴：

```
Loosely based on this reference image, redraw the character as a MINIMAL 2D
battle sprite, keeping only THREE identity anchors: hairstyle and hair color,
main outfit color, one iconic item.

SIMPLIFICATION RULES (strict):
- flat cel shading only: at most 4 flat colors per character
- no gradients, no soft shading, no light-and-shadow painting
- no fabric folds, no wrinkles, no cloth or metal texture
- no embroidery, no filigree, no ornaments, no gems, no chains, no buckles, no studs
- reduce the outfit to 2-3 large simple shapes (tunic, armor plate, cape...)
- one single clean outline around the whole silhouette; no internal detail lines
  except eyes and simple color separations
- bold readable silhouette, cute sticker-like flat presentation,
  readable when scaled down to 64 pixels

FORMAT:
full body, standing, three-quarter front view, feet centered at the bottom,
single character, flat solid pure green background (#00FF00), no text, no shadow

PALETTE:
soft storybook colors, muted earth tones with emerald green and amber gold accents,
hand-painted warmth but rendered completely flat, charming and friendly;
no photorealism, no 3D rendering, no painterly brushstrokes
```

**v2 和旧版的区别**（为什么这次能压住细节）：
1. **数值化**：`at most 4 flat colors` / `2-3 large shapes`——给模型明确的上限，而不是模糊的"简单点"
2. **删除清单**：把最容易出现的细节（褶皱/渐变/刺绣/宝石/链条/铆钉）**点名禁止**
3. **弱化参考**：开头用 `Loosely based on`（宽松参考）——只保留三个识别锚点，其余允许自由删
4. **只保留三要素**：发型发色 / 服装主色 / 一个标志物——这是"像同一个角色"的最低限度
5. **去掉笔触质感词**：旧版锚点里的 `soft gouache textures` 会诱导模型画纹理，v2 改成 `rendered completely flat`

**可选增强**：在提示词末尾追加这句，让模型自己出一张"小尺寸预览"验证可读性（生成后裁掉预览部分即可）：
```
Also include a small 96-pixel preview of the same sprite in the bottom-left
corner of the image to verify readability
```

**第 2.5 步：还不够简单？四档微调杠杆（从轻到重）**

| 杠杆 | 做法 | 适用 |
|---|---|---|
| ① 加码措辞 | 把 `MINIMAL` 改成 `EXTREME minimal, icon-like` | 还差一点 |
| ② 点名删除 | 针对该角色追加：`remove the cape entirely` / `remove all belt straps and buckles` | 某个部位拖累整体 |
| ③ 丢装饰 | `ignore all armor decorations and trims, keep only the base color scheme` | 装饰是主要噪声源 |
| ④ 二段迭代 | 把生成的图再喂回去：`redraw this sprite again, same character, but much simpler: fewer shapes, larger flat color areas, no shading at all` | 前几档都无效时 |

> 经验：**先跑一张完整 v2，再根据结果用杠杆微调**，不要直接堆最重的措辞——容易把角色简化到失去辨识度。

**第 3 步：一致性校验**
生成的战斗精灵和立绘对比：发型/服装主色/标志物必须一致。不像就反馈差异点（如"头盔改为参考图中的狮纹头盔"），**不要改锚点后半段**。

**第 4 步：去底 + 命名**
纯绿背景用编辑器/PS 一键去底（或用色键）；命名直接用项目 ID：`Hero_Knight.png` → 导入 `Content/Sprites/` → 教程 B 填进 DT_Units。

### 画布规范（保证战场上比例统一）

```
所有角色战斗精灵用同一画布比例：512×512（或 256×256 统一即可）
角色主体占画布高度 70%~80%，脚底贴画布底边居中
头部留白不超过 10% —— 不许一张头顶留 30% 一张顶天立地
建筑单独规范：画布 1024×1024，主体占 85%+
```

> 统一画布比例是**引擎外第一道缩放保险**：即使之后在 DT_Units.SpriteScale 里还能微调，源图一致能省大量对齐时间。建议每出一张就在画图里看一眼比例，跑偏立刻重出。

### 建筑的特殊策略（建筑版 v2 提示词）

建筑在战场上属于"大物件"，**可以比角色精细一档**（角色 1 单位格，建筑 2×2 视觉占位）：
- 同样裁正面图 → 参考重绘
- 简化上限放宽：**5 个平涂色**（角色 4 个）、允许少量功能细节（箭口/门/招牌）
- 但**结构必须清晰**：屋顶/墙体/地基三大形状优先
- 透视要求更严格（2.5D 战场视角统一）

**建筑转化提示词 v2**（上传建筑正面参考图后粘贴）：

```
Loosely based on this reference image, redraw the building as a CLEAN 2D game
building sprite for a battlefield, keeping only THREE identity anchors:
overall silhouette and roof style, main material colors, one iconic element
(weathervane, banner, forge chimney, arrow slits...).

SIMPLIFICATION RULES:
- flat cel shading: at most 5 flat colors per building
- no gradients, no soft light-and-shadow painting
- structure first: three big shapes (roof, walls, base), then only 2-3 small
  details that tell the building's function
- no brick-by-brick texture, no wood grain, no individual stones,
  no moss texture patches, no rubble or clutter around the base,
  no grass tufts, no background scenery
- one clean outline around the whole silhouette, moderate internal lines
  only where big shapes separate
- medium detail level: more detailed than a character sprite, but still
  readable when scaled down on a battlefield

PERSPECTIVE:
front-facing facade with slightly visible roof top, designed for a top-down
battle arena viewed from a tilted camera; base line flat at the bottom,
centered, no ground plane, no drop shadow

FORMAT:
single building, flat solid pure green background (#00FF00),
no text, no people, no creatures, no floating decorations outside the silhouette

PALETTE:
soft storybook colors, muted earth tones (warm stone, timber brown) with
emerald green and amber gold accents, hand-painted warmth but rendered flat,
charming storybook feel; no photorealism, no 3D rendering, no painterly brushstrokes
```

**建筑微调杠杆（双向）**：

| 方向 | 做法 |
|---|---|
| 还是太复杂 | `reduce to 3 flat colors, remove all small window and trim details, keep only roof shape and one door` |
| 太简单/没辨识度 | `add exactly two small story details: [一盏暖灯 / 一面旗帜 / 墙边爬藤]` —— 用"生活感细节"补辨识度，别用花纹 |
| 透视跑偏（看到完整屋顶/纯正面墙） | 强调 `facade facing the viewer with only the roof top edge visible above it` |

> 说明：这个模板对**家园阶段**的新建筑（金库/训练营/铁匠铺等）同样适用——换参考图、换三个识别锚点即可。建筑升级外观（如金顶/旗帜更多）可以在锚点里改。

### 常见翻车点

| 现象 | 原因 | 对策 |
|---|---|---|
| 战斗版和立绘不像 | 参考图太脏（整张三视图） | 裁单张正面图 |
| 细节还是太多，小尺寸看不清 | 简化指令被锚点压过 | 换 **v2 强约束提示词**（数值上限+删除清单），再用四档杠杆微调 |
| 每张比例不统一 | 没按画布规范 | 固定 512×512 + 主体 70%~80% |
| 颜色漂移 | 会话/模型切换 | 同批同会话出；跨天先贴旧图 |
| 生成出多个人/文字 | 忘了负面词 | 补 `single character, no text, no letters` |
