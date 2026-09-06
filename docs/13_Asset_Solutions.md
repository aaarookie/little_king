# 《小小国王》免费可商用资产方案与制作教程

> **当前排期覆盖**：音效与表现素材后置，本文仅作后续参考，不是 Sprint 5 或地牢启动前置任务。Sprint 6 已删除，当前下一阶段见 [17 地牢开发规划](17-DungeonDevelopmentPlan.md)。

> **Sprint 5 修订提示（2026-09-06）**：本文保留历史教程/素材参考。当前规则以 [01-GDD](01-GDD.md) 为准，先执行 [16 迁移教程](16-Sprint5MigrationTutorial.md)。旧倒计时开战、法术门/灰卡、团队血条、死亡震动、集火高于嘲讽的说明已失效；当前相机仍按正俯视验证，斜视为未验收方案。资产清单中的“完成”不代表本轮资产接通或运行验收完成。

核验日期：2026-09-05。依据：你提供的 `12-AssetRequest.md`。本次没有拿到 `07-ArtStyleGuide.md`、工程文件或原始美术，所以画风判断以需求文档中的描述为准；尚未逐包下载、试听或在你的工程中测试。下文的资源许可来自作者发布页或官方条款；制作参数是针对本项目的建议起点。

**推荐组合：已有单位与卡图继续使用；CC0 素材补特效、UI 和音频；AI 辅助角色、卡面及环境；Krita 做整理；UE 做动效。** 先补齐 P0，再投入逐帧动画。

“免费”“开放许可”“软件开源”和“AI 输出可商用”是不同条件。表中的 CC0、CC BY 和 OFL 资源都允许按相应条件用于商业游戏；AI 路线有独立的平台或模型条件，不能把所有 AI 工具都当成免费商用工具。

## 1. 按你的资产清单逐项安排

| 清单中的需求 | 建议解决方案 | 是否值得用 AI | 现在如何推进 |
|---|---|---|---|
| 已有 8 个常用单位、7 张卡图 | 保留初版，先统一脚底锚点和显示大小 | P1 再按同一参考重绘 | 不占用 P0 制作时间 |
| 敌兵试点 1 个 | 用已有单位作画风参考，AI 出单帧，再人工清边 | 很适合 | 骷髅剑士只是文档建议；若首章对抗人类，可先做叛军剑士 |
| 待定佣兵 3 个 | 按兵种功能做差异化单帧 | 很适合 | 先确定兵种，避免提前量产；候选见第 8 节 |
| 5 组技能／法术特效 | Kenney Particle Pack + 自绘简单圆环 + UE 动态材质／Niagara | AI 适合花瓣、火焰核心等零件 | 第一优先级；不必先做序列帧 |
| 战场地面 1 张 | AI 出草地、石板和边缘装饰；Krita 拼合 | 很适合 | 地面先测试透视，再制作大图 |
| 卡框、按钮、面板等 UI | Kenney 边框为基础，Krita 整理九宫格，AI 补固定装饰 | 适合装饰，不适合精确排版 | 全套先用同一边角语言 |
| 费用、银币、状态图标 | 自绘几何形；或 Game-icons.net 的明确授权图标 | 简单图形没必要依赖 AI | 数字与文字在 UMG 中叠加 |
| 8 个音效键 | CC0 音效包筛选、裁剪、统一导出 | P0 无需 AI | 按第 6 节逐键选音 |
| 战斗 BGM、结算短曲 | CC0 JRPG 音乐 + Kenney Music Jingles | AI 可做，免费版授权需单独判断 | 零素材费路线直接用 CC0 |
| 中文字体 | 思源黑体；标题可配思源宋体 | 不需要 | 保留 OFL 与版权声明 |
| P1 逐帧动画 | 已有单帧拆层／Krita 补关键帧 | AI 辅助姿态，人工统一外观 | 先试 1 个单位的 Attack，再做 Walk |
| P1 英雄立绘、新卡 | AI 参考已有角色，分开生成插画与边框 | 很适合 | 角色身份固定，装饰细节按显示尺寸增加 |
| P2 地牢、家园、事件插画 | AI 背景和单体物件，必要时用 CC0 模型辅助定透视 | 很适合 | 按一个章节成套制作，复用相机、色板与边缘装饰 |
| P2 怪物、Boss | AI 出单帧；需要大量动作时考虑 CC0 模型渲染成精灵 | 单帧适合，动画需人工 | 按剧情选种族，不默认全部亡灵 |

风格策略：**战斗单位保持简洁、轮廓清楚；立绘、卡面和固定 UI 角饰承担华丽细节；地面中心保持安静。** 这样可以保留童话气质，也能让自动战斗中密集出现的单位与技能容易辨认。

## 2. 已核验的免费资源清单

以下链接指向具体资源包或作者发布页。适配评价是本方案的判断，不代表作者承诺适合你的游戏。资源包可能还要改色、裁剪、转换格式；“可商用”不等于下载后已经符合交付规格。

| 资源与下载入口 | 许可／费用 | 对应需求 | 适配与处理建议 |
|---|---|---|---|
| [Kenney Particle Pack](https://kenney.nl/assets/particle-pack) | CC0；免费；无需强制署名 | 光点、火花、法术粒子 | 包含 80 个 512×512 文件；作为特效零件，不是已接好逻辑的 UE 技能包 |
| [Kenney Fantasy UI Borders](https://kenney.nl/assets/fantasy-ui-borders) | CC0；免费；无需强制署名 | 卡框、头像框、面板装饰 | 140 个文件；优先试用，与奇幻装饰方向较接近，但仍需统一色板与拉伸区 |
| [Kenney UI Pack — RPG Expansion](https://kenney.nl/assets/ui-pack-rpg-expansion) | CC0；免费；无需强制署名 | 按钮、面板、滑条等基础件 | 85 个文件；与装饰边框组合使用，不假设包中恰好覆盖全部 UI 规格 |
| [Kenney RPG Audio](https://kenney.nl/assets/rpg-audio) | CC0；免费；无需强制署名 | 武器、脚步等 RPG 音效候选 | 50 个文件；先选短促、少混响的声音 |
| [Kenney Impact Sounds](https://kenney.nl/assets/impact-sounds) | CC0；免费；无需强制署名 | 近战、受击、倒下声候选 | 130 个文件；按材质感选择，再剪短与混音 |
| [Kenney UI Audio](https://kenney.nl/assets/ui-audio) | CC0；免费；无需强制署名 | 出牌、按钮、提示 | 50 个文件；优先温和的点击、放置感 |
| [Kenney Music Jingles](https://kenney.nl/assets/music-jingles) | CC0；免费；无需强制署名 | 开战、胜利、失败短句候选 | 85 个文件；按音色和情绪试听，裁成规定长度 |
| [RPG Sound Pack — artisticdude](https://opengameart.org/content/rpg-sound-pack) | 该包标注 CC0；免费 | 武器、魔法、怪物与 UI 音效补充 | 作者说明有 95 个 WAV；对照实际下载内容选片段 |
| [80 CC0 RPG SFX](https://opengameart.org/content/80-cc0-rpg-sfx) | 该包标注 CC0；免费 | 刀刃、怪物、物件、法术 | 怪物音要挑不恐怖的版本，适合后续地牢补充 |
| [JRPG Pack 5 Action — SubspaceAudio / Juhani Junkala](https://opengameart.org/content/jrpg-pack-5-action) | 该包标注 CC0；免费 | 战斗 BGM 候选 | 先试听力度较轻的片段，循环接缝要实测 |
| [JRPG Pack 1 Exploration](https://opengameart.org/content/jrpg-pack-1-exploration) | 该包标注 CC0；免费 | 地牢探索、轻战斗候选 | 若 Action 包太激烈，从此包选更轻的节奏 |
| [JRPG Pack 4 Calm](https://opengameart.org/content/jrpg-pack-4-calm) | 该包标注 CC0；免费 | 家园、庭院、非战斗界面 | 可与上述同作者包形成一致的音乐方向 |
| [Game-icons：Fireball](https://game-icons.net/1x1/lorc/fireball.html)、[Crown](https://game-icons.net/1x1/lorc/crown.html) | Lorc；CC BY 3.0；免费；需署名 | 火球、王冠、状态标记 | SVG／PNG；易改色。每个新增图标核对自己的作者，不只署网站名 |
| [思源黑体 Source Han Sans](https://github.com/adobe-fonts/source-han-sans) | SIL OFL 1.1；免费 | 正文、数字、HUD | 使用简体中文地区版本；保留字体版权与许可证 |
| [思源宋体 Source Han Serif](https://github.com/adobe-fonts/source-han-serif) | SIL OFL 1.1；免费 | 标题、卡名 | 建议中等或较粗字重，小字正文仍用黑体 |
| [Quaternius RPG Character Pack](https://quaternius.com/packs/rpgcharacters.html) | 该包 CC0；免费 | P1 动作参考、可渲染的角色基础 | 6 个带绑定和动画的奇幻 **3D** 角色；需 Blender 渲染／改绘，不能直接当 2D 精灵 |
| [Quaternius Medieval Village Pack](https://quaternius.com/packs/medievalvillage.html) | 该包 CC0；免费 | P2 家园建筑、村庄物件 | 3D 中世纪建筑与道具；适合统一角度渲染后改绘 |
| [Kenney Tiny Dungeon](https://kenney.nl/assets/tiny-dungeon) | CC0；免费 | 地牢原型、布局验证 | 16×16 像素素材，和你的手绘画风差别明显；适合占位与测试 |

Kenney 单包页面有免费入口，赞助与 All-in-1 合集是其他选项。OpenGameArt 是混合许可证平台，**这里核验的是表中具体条目，不是整站所有资源**。Quaternius 也有包含付费扩展的其他产品，本表免费结论只针对列出的包。

如果希望减少署名管理，可先使用 CC0、OFL 和自己制作的图形。Game-icons 是补充选择，不是必需依赖。

## 3. 商用授权如何落实

| 类型 | 可用于商业游戏吗 | 实際需要做什么 |
|---|---|---|
| CC0 | 可以复制、修改和用于商业项目 | 不强制署名；仍保存原包、来源链接和许可证，便于追溯。[CC0 官方说明](https://creativecommons.org/publicdomain/zero/1.0/) |
| CC BY 3.0 | 可以，包括改色、改绘 | 保留作品名、作者、来源与许可链接；标明修改。不能给这些素材附加与许可冲突的限制。[CC BY 3.0](https://creativecommons.org/licenses/by/3.0/) |
| SIL OFL 1.1 字体 | 可以嵌入／随商业软件分发 | 携带版权和许可文本；字体不得单独出售；修改字体时处理 Reserved Font Name。游戏本身不因此变成 OFL。[黑体许可](https://github.com/adobe-fonts/source-han-sans/blob/master/LICENSE.txt)、[宋体许可](https://github.com/adobe-fonts/source-han-serif/blob/release/LICENSE.txt) |
| ChatGPT Images 输出 | 按适用协议和使用条件可以商用 | OpenAI 的输出归属条款不保证独占性，也不替输入参考图解决第三方版权。保存自己的输入、生成记录与适用条款。[OpenAI 条款](https://openai.com/policies/row-terms-of-use/) |
| 官方 SDXL 1.0 输出 | 可按其模型许可用于商用 | 官方权重是 CreativeML Open RAIL++-M，带用途限制，不能当成 CC0／无条件许可。另装的 LoRA、模型或参考图要另查。[SDXL 许可](https://huggingface.co/stabilityai/stable-diffusion-xl-base-1.0/blob/main/LICENSE.md) |

AI 工具允许商业使用，与输出是否在某地区获得完整、独占的著作权，是两个问题。对本项目的可执行原则是：**参考图使用你自己创作或确有授权的素材，保留人工修改记录，不把其他游戏／动画中的具体角色、标志或截图当作可商用底稿。** 色板、线条和童话氛围可以用通用视觉描述来传达。

CC BY 图标经过 AI 改绘，不能直接当作没有署名义务的全新素材。需要这种最简授权体验时，优先以自制草图或 CC0 素材为基础。

不要把以下内容当成已经满足本项目的免费商用方案：

- **Suno 免费套餐歌曲**：官方帮助页限定个人、非商业用途。付费选项见第 7 节。[免费套餐权利](https://help.suno.com/en/articles/9601601)
- **MusicGen 官方预训练权重路线**：AudioCraft 的代码是 MIT，但模型权重是 CC BY-NC 4.0；代码开源不能推出模型可免费商用。[官方仓库许可说明](https://github.com/facebookresearch/audiocraft)
- 只写“免费下载”“免版税”而不提供实际许可的素材。原清单提到的其他 AI 音乐／制图平台，不能仅凭平台名称就列为已核验商用来源。

## 4. AI 制图教程：敌兵、佣兵、卡面、背景和物件

### 4.1 路线 A：ChatGPT Images + Krita，适合直接开始

ChatGPT 可以上传参考图编辑，也支持按要求制作透明背景；具体可用额度以账户为准，不能当成无限免费的开源服务。[官方操作说明](https://help.openai.com/en/articles/11084440-images-in-chatgpt)

**先准备一个小参考集。** 选现有资产中比例和线条最满意的一张单位、一张建筑、一张卡面，附上你认可的色板。每类生成任务只上传相关参考，避免把精简战斗单位与高细节立绘混成同一个目标。

**单帧单位操作：**

1. 上传已有单位图，说明它是画风、比例和脚底位置参考。如果生成同一角色的新版本，再明确要求保持身份。
2. 一次只做一个角色、一种姿态。先检查正面 3/4 朝向、手中物品和完整轮廓。
3. 满意后再要求局部修改，如“只简化盔甲”“只修正握剑的手”。每次检查其他区域是否被意外改动。
4. 下载原图，在 Krita 建透明的 512×512 交付画布，等比缩放主体，设统一脚底中心；不要把武器挤出边缘。
5. 把 64px 预览放在战场底色上看：头部、武器、阵营是否仍能看清。存 PNG 成品与 `.kra` 分层源文件。

文档中的“占高 70%~80%”“脚底贴底边”“头顶留白 ≤10%”若都按不含武器的直立主体计算，会互相挤压。实际制作建议以 **固定脚底锚点、统一视觉体型、武器不裁切** 为先，选定一张模板后全队一致；上方空白留给武器或动作。这是建议修订，不代表已修改你的需求文件。

可以复制的敌兵提示词（先确定首章确实需要骷髅再使用）：

```text
Use the uploaded game sprite only as a reference for proportions,
outline thickness, palette restraint and overall visual language.
Create one original skeleton swordsman for a warm fairy-tale 2D auto-battler.
Full body, standing, three-quarter front view, face clearly visible.
Short chibi proportions, a simple readable skull, rusty armor,
a tiny muted blue soul-light in the eye sockets, one simple longsword.
Keep only four flat colors in total: bone ivory, rusty brown,
muted soul blue, and the dark outline color.
Large simple shapes, clean outer contour, minimal internal lines.
Friendly mischievous mood, no gore, no horror.
Keep the entire weapon and both feet visible.
True transparent background with an alpha channel, no ground shadow.
No text, no watermark, no extra characters, no gradients,
no metal textures, no decorative clutter, no photorealism, no 3D render.
```

这段用于战斗精灵。英雄立绘和卡面可以增加水粉笔触、金色纹样和装饰；不要把这些细节全部塞回 64px 战斗单位。

**透明处理：** 优先检查原生透明 PNG。棋盘格如果成为图案的一部分，就不是透明通道。Krita 中在图层下轮流放黑、白、紫色底检查边缘。若必须色键去底，选主体中没有的颜色，并先限制到背景选区，再使用 `滤镜 → 颜色 → 颜色转透明（Color to Alpha）`，最后修边；不要对翡翠绿斗篷、治疗绿光统一使用 `#00FF00` 绿幕。半透明光效尤其适合独立遮罩，不适合粗暴去色。[Krita 官方说明](https://docs.krita.org/en/reference_manual/filters/colors.html)

### 4.2 路线 B：本地 ComfyUI + 官方 SDXL 1.0，免平台出图费

这条路线使用开源软件和带开放许可的模型权重，但 SDXL 的用途限制使它不等同于无限制的开源软件许可。需要自己的硬件、磁盘、电力和安装时间；云端 Comfy 服务不是这条免费本地路线。

**Windows 安装与第一张图：**

1. 按 [ComfyUI 官方便携版教程](https://docs.comfy.org/installation/comfyui_portable_windows) 下载与显卡相匹配的包并解压。NVIDIA 版本按说明运行 `run_nvidia_gpu.bat`；其他显卡使用对应官方包和启动文件。
2. 到 [Stability AI 官方 SDXL 1.0 仓库](https://huggingface.co/stabilityai/stable-diffusion-xl-base-1.0) 的 Files 下载 `sd_xl_base_1.0.safetensors`，放进 `ComfyUI/models/checkpoints/`。同时保存 `LICENSE.md`。本教程先只用 Base，不需要 Refiner、LoRA 或收费节点。
3. 加载 [官方文生图基础流程](https://docs.comfy.org/tutorials/basic/text-to-image)，在 Load Checkpoint 中改选上述 SDXL 文件。基础教程可能使用另一个示例模型；本项目按这里指定的权重替换。
4. 保留基础节点连接：Load Checkpoint 提供 MODEL／CLIP／VAE；两个 CLIP Text Encode 分别输入正、负提示词；Empty Latent Image 接 KSampler；KSampler 经 VAE Decode 接 Save Image。
5. 建议起步参数：1024×1024、batch 1、steps 25、CFG 6、sampler `euler`、scheduler `normal`、denoise 1.0。它们是调试起点，不是官方最优值或质量保证。显存不足时先减小尺寸，不要直接生成 4096 图。
6. 多换 seed 比较轮廓，选中后保存工作流 JSON、模型名和参数。固定 seed 有助复现，不能保证跨提示词的角色身份一致。
7. 需要沿用现有草图时，按 [官方图生图流程](https://docs.comfy.org/tutorials/basic/image-to-image) 用 Load Image + VAE Encode 替换空 latent，仍选 SDXL；denoise 可从 0.3~0.5 试起，越高越容易改动外观。
8. 官方 SDXL Base 的普通出图流程不会因为提示词写“transparent”就生成真正 Alpha。生成干净纯色背景后，用 Krita 做遮罩／去底，再缩到交付尺寸。

SDXL 的角色一致性和清晰指令遵循需要更多修图，适合愿意搭本地流程的情况。若目的是尽快补 P0，直接沿用你正在使用的制图工具通常投入更少。

### 4.3 战场背景：先确定它是地面纹理，还是相机看到的整张背景

你的文档写“图片铺平在战场上”。因此首选 **地面纹理层 + 独立边缘装饰**：地面草地和石板从接近正俯视方向绘制，再由 UE 相机产生斜视效果。若图里先画了强烈的斜透视，又贴到斜看的地面，石板、花坛可能再被压一次。

1. 在 Krita 先画简单布局：中央约 70% 低对比战斗区、装饰集中边缘、左暖右微冷。
2. 上传布局给 AI，让它保持空白区和边界，仅完善材质与色调。
3. 地面本身不要画人物、塔楼、复杂投影或明显消失点。树、旗帜、花丛可以单独生成 PNG，放到边缘。
4. 先用小图导入 UE 验证投影和裁切。通过后再整理成 4096×4096，或按最终取景裁为 4096×2304。普通插值放大满足像素尺寸，不会自动增加真实细节；补绘重点放在玩家能看到的区域。
5. 如果实际工程使用的是朝向相机的背景板，而不是水平地面，可直接画完整斜视场景，但应锁定相机和单位落脚位置。

地面纹理提示词：

```text
Create ground artwork for an original warm fairy-tale kingdom courtyard.
This image will be mapped onto a horizontal game plane.
Use a near top-down ground texture view with no strong perspective convergence.
Soft gouache-like color blocks, muted meadow green and warm stone ivory,
subtle amber accents on the left, slightly cooler tones on the right.
Keep the central 70 percent open, flat and visually quiet for many small units.
Only sparse paving and gentle ground color variation in the center.
Small flowers and low decorative patterns stay near the outer edges.
No people, no buildings, no tall objects, no horizon,
no dramatic cast shadows, no text, no grid, no hard dividing line.
Square composition, opaque background.
```

### 4.4 新卡、建筑与 P2 物件

- **新卡**：上传对应单位成品，要求保留身份；生成 1024×1024 的迷你场景插画。描金边框、费用徽章、卡名在 UMG 中叠加，避免每张卡生成不同宽度的边框或不可修改的文字。
- **建筑**：上传现有箭塔／兵营参考，保持相同 3/4 朝向、屋顶可见程度和底座线。1024×1024，主体以大体块为主，透明底；不要为了填满画布裁掉旗杆。
- **门与宝箱**：先生成关闭状态，再上传它，仅编辑铰链与门板／箱盖；复制到同一尺寸画布并锁定底座。若开闭两帧有整体漂移，优先手动对齐，不重抽两张完全独立图片。
- **英雄立绘**：上传该英雄战斗图和设定图，强调发型发色、服装主色、标志物不变，另外提高笔触和装饰密度。立绘不能反过来改变战斗中的身份辨识。
- **地牢与家园背景**：沿用 4.3 的地面／背景板选择；每个房间只换一个主题，如水晶洞、图书馆、荒废花园。门、宝箱、祭坛和可交互建筑独立成文件。

可替换的物件提示词：

```text
Using the uploaded building as a reference for camera angle and palette,
create one original [closed treasure chest / blacksmith building / ornate altar].
Three-quarter front view with a slightly visible top.
Warm fairy-tale game art, clean silhouette, large readable shapes,
muted emerald, amber gold and warm earth tones.
One iconic functional detail, restrained internal lines.
Full object visible, consistent base line, transparent background.
No ground scenery, no text, no watermark, no extra objects.
```

## 5. 技能特效与 UI：把 AI 用在零件上

### 5.1 先准备一套可复用的特效零件

用 Particle Pack 选光点／火花；简单白色圆环可以在 Krita 用椭圆工具自行画出。建议制作：圆环遮罩、小光点、花瓣、箭影、火球核心、爆开形状。它们是多个技能共用的零件，不要求 AI 一次出一整张 Sprite Sheet。

| 技能 | 组合方案 | 项目半径 → 直径 | 建议表现起点 |
|---|---|---|---|
| 骑士·治疗鼓舞 | 地面绿环 + 少量向上光点 | 300 → 600 世界单位 | 环淡入后保持，光点上升，整体淡出 |
| 法师·火环 | 金色环 + 沿圆周少量火花 | 250 → 500 | 内部装饰从中心扩散至边界；持续时间配合实际伤害时机 |
| 游侠·箭雨 | 琥珀落点圈 + 数支箭影 | 350 → 700 | 落点提示与伤害事件对齐，箭影快速落下后消失 |
| 火球术 | 飞行核心 + 短尾迹 + 命中爆开 | 以工程法术数据为准 | 飞行图跟随现有投射物，命中再生成爆开 |
| 治疗波 | 同一绿环 + 花瓣／光点 | 以工程法术数据为准 | 涟漪扩散并淡出，复用治疗色板 |

**最简 UE 制作步骤：**

1. 导入一张透明圆环 PNG。新建材质 `M_VFX_Unlit`：Surface、Unlit；光晕可用 Additive，需要保留彩色轮廓时用 Translucent。
2. 让贴图 RGB × 颜色参数 × 亮度参数接 Emissive Color；Alpha × 透明度参数接 Opacity。若做 Niagara 粒子，再乘 Particle Color，便于粒子模块控制颜色与淡出。
3. 建 `BP_VFX_Ring`，放一个无碰撞的 Plane，赋予动态材质。圆环平面沿战场地面摆放，并稍微抬高避免重叠闪烁；不要把技能范围圈当成永远正对镜头的广告牌。
4. Plane 的最终世界宽度为 `2 × 技能半径`。如果所用网格原宽 100，治疗圈直径 600 的目标缩放为 6；若网格尺寸不同，按真实宽度换算。
5. 用 Timeline 调透明度，例如 0 → 1 → 0，总长先试 0.8 秒。装饰纹理可以脉冲；承担范围提示的边界要与真实技能范围一致。
6. 在已有技能触发回调中于施法／落点坐标生成该效果；播放结束回收或销毁。伤害和治疗仍由已有逻辑决定，不从视觉动画反推数值。
7. 要加光点时，新建 Niagara System，使用 Simple Sprite Burst 之类的基础模板，替换贴图和材质，设置少量粒子、向上速度及随寿命淡出。可以先试十几个粒子，再结合单位数量看开销。

上述是本项目的实现建议，尚未在你的工程里执行。Niagara 的基础操作可跟做 [Epic 官方入门教程](https://dev.epicgames.com/documentation/unreal-engine/quick-start-for-niagara-effects-in-unreal-engine)。不要把教程中某个模板的默认参数当成你的游戏伤害范围。

AI 火球零件提示词：

```text
One isolated stylized golden fireball core for a 2D fairy-tale game.
A compact round amber core with three large curling flame shapes.
Readable silhouette at 64 pixels, flat warm colors, restrained detail.
True transparent background, no ground, no cast shadow, no text.
Single still effect element, no sprite sheet, no multiple stages,
no photorealistic fire, no scene lighting, no background glow rectangle.
```

### 5.2 UI：一套结构，少量装饰，多处复用

| UI 件 | 建议做法 | 交付建议 |
|---|---|---|
| 卡牌框 | CC0 边框重配色；角饰固定 | `CardFrame_Default.png`，透明中心，附安全拉伸范围 |
| 费用徽章 | 简单圆／宝石形，最多少量高光 | `CostGem.png`，64px；数字实时叠加 |
| 英雄头像框 | 固定圆环与顶端小装饰 | `HeroPortraitFrame.png`，128px |
| 按钮三态 | 同一结构，改亮度／内边与按下偏移 | `Button_Normal.png`、`Button_Hover.png`、`Button_Pressed.png` |
| 血条底与填充 | 圆角几何形；华丽外框单独覆盖 | `HealthBar_BG.png`、`HealthBar_Fill.png`；填充区保持可读 |
| 银币 | 自绘圆币或用许可明确的图标 | `SilverCoin.png`，64px |
| 结算面板 | 九宫格底图 + 固定顶部王冠／花饰 | `Panel_Result.png`；胜负文字单独排 |
| 飘字 | 思源黑体较粗字重 + 描边 | 继续用文本组件，P0 不用生成数字贴图 |

**九宫格操作：**

1. 在 Krita 制作一张透明中心的 256×256 框。例如四边各留 32px 安全区，角饰全部落在角区内，边中段尽量简单。
2. 导入 UE，UI 纹理采用合适的 UI 压缩／纹理组设置并检查透明边缘。把图片放进 UMG Brush。
3. Draw As 选 Box，Margin 四边为 `32 / 256 = 0.125`。这个比例只适用于示例图，实际按安全区像素除以对应图像宽／高计算。
4. 分别拉成正方形、宽按钮和大面板检查四角；需要透明中心且边缘平铺时可用 Border，普通 Image 模式会忽略 Margin。
5. 王冠、花饰、徽章放在独立 Image 控件中，让复杂装饰保持形状；不要靠拉伸把整幅精美插画变成任意宽度的按钮。

Box 拉伸、Border 平铺与 Margin 的机制见 [Epic 官方 UMG Styling](https://dev.epicgames.com/documentation/unreal-engine/umg-styling-in-unreal-engine)。九宫格可以是一张图加安全区说明，无需强制交付九个独立 PNG。

AI 最适合先出一个角饰，再人工镜像、对齐、修线：

```text
One isolated upper-left corner ornament for a square fantasy game UI frame.
Original floral motif in muted amber gold, a tiny emerald accent,
warm storybook appearance, clean contour, restrained internal detail.
The ornament stays entirely inside an L-shaped corner area.
Transparent background, no panel, no lettering, no numbers, no watermark.
```

## 6. 音频：用免费资源覆盖 8 个固定键

下面是**选音方向**，不是已经试听确认的具体文件。按实际包内文件挑选，保留原始文件名到交付登记表中。

| 固定键 | 先找哪个已核验包 | 筛选与加工目标 |
|---|---|---|
| `MeleeHit` | Impact Sounds / RPG Audio | 低沉短击，可叠很轻的金属层，避免过响刺耳 |
| `RangedShoot` | RPG Audio / artisticdude RPG Sound Pack | 弓弦或轻短 swoosh；没有满意候选可自行录制橡皮筋轻弹后加工 |
| `HitTaken` | Impact Sounds | 比 MeleeHit 更轻、更闷，不抢主攻击反馈 |
| `UnitDied` | Impact Sounds / 80 CC0 RPG SFX | 短落地或装备散落，避免恐怖惨叫 |
| `CardPlay` | UI Audio | 柔和放置、点击或轻翻动感 |
| `BattleStart` | Music Jingles | 很短的铜管／钟声感提示；若无合适音色，选中性的正向提示 |
| `Victory` | Music Jingles | 上扬、明亮、2~3 秒以内 |
| `Defeat` | Music Jingles | 下行、柔和、2~3 秒以内，不恐怖 |

**Audacity 加工教程：**

1. 导入选中的原音，截出需要的动作瞬间，清掉过长的开头空白。
2. 普通 SFX 控制在 1.5 秒内；Victory／Defeat 可到 3 秒。头尾加很短淡入淡出避免点击声，注意保留击打瞬态。
3. 如用两层声音，先调整相对音量再混合。确认单声道合并后没有明显抵消。
4. 用 Normalize 将最终峰值设到约 **-12 dBFS**，对应你文档中的峰值要求。这不是 -12 LUFS，也不代表听感响度已完全一致；最后还要按听感微调。[Normalize 官方说明](https://manual.audacityteam.org/man/normalize.html)
5. 导出 WAV，采样率 48000 Hz，SFX 单声道，优先 16-bit PCM；文件名严格使用表中的固定键。导出窗口可设格式、声道、采样率与编码。[导出教程](https://manual.audacityteam.org/man/file_export_dialog.html)
6. 在战斗里同时触发多单位听一次。必要时限制同类声音并发、做小幅音高变化；只把单条声音调小，未必能解决密集命中的叠加问题。

Audacity 是可免费使用的音频编辑工具，其软件许可不替音频来源提供授权。[官方 FAQ](https://www.audacityteam.org/faq/)

**BGM 免费路线：** 从第 2 节的 JRPG Action／Exploration 选一首较轻的战斗候选；Calm 用于家园。试听是最终选曲步骤，不能仅因标题为 Action 就认定适合温暖童话。

循环制作建议：找旋律与和声能接回去的完整乐句，在节拍边界剪切；必要时复制尾部与头部做短交叉淡化，再裁为最终循环片段。循环播放至少数轮，确认没有断拍或突然静音。战斗 BGM 可保留立体声，交付 `BGM_Battle.wav`，并注明是否整个文件可循环或提供循环起止位置。不要对无缝 BGM 的整个头尾都做明显淡出，否则每一轮会听到音量下陷。

## 7. AI 音乐教程：作为付费可商用备选

**P0 仍首选 CC0 音乐。** 如果需要更贴合本作的原创氛围，可以用 Suno 的具备商用权限的付费路线；它不是免费开源方案。

截至核验日，Suno 的付费帮助页把商业权利与**订阅期间的下载**联系起来。其现行条款要求通过允许的渠道获得下载，并对 Remix 单列非商业限制；因此不能只保存一个播放链接、录屏提取声音，或把付费会员理解成所有平台音频都能商用。[付费权利说明](https://help.suno.com/en/articles/9601665)、[现行条款](https://suno.com/terms-of-service)

操作建议：

1. 使用具有商业权限的 Pro／Premier 付费账户，在有效订阅期间新建生成，并在有效期内通过平台允许的下载入口下载。保存生成日期、作品链接／ID、订阅与下载证明、条款版本。
2. 选择纯音乐／Instrumental，使用原创文字描述，不上传他人的旋律，不使用他人歌曲的 Remix。旧免费歌曲不要自动当成补订阅后已获授权；官方旧曲授权说明也提示并非默认追溯。[旧曲说明](https://help.suno.com/en/articles/2425729)
3. 先生成几首候选，优先挑主题简单、少大起大落、能循环的段落。
4. 通过平台正式入口下载允许导出的音频；若需要格式转换，在 Audacity 中做常规剪辑与转 WAV。保持来源记录，不以去除来源标记的方式掩盖生成状态。
5. 按第 6 节剪辑循环。AI 提示词写“seamless loop”不能保证文件已经无缝，必须试听接缝。

可复制的音乐提示词：

```text
Instrumental music for an original warm fairy-tale kingdom auto-battler.
Light chamber orchestra with pizzicato strings, flute, harp,
gentle hand percussion and occasional soft brass.
Playful, noble and adventurous, medium energy, around 100 BPM.
A short memorable original motif, steady pulse, balanced dynamics.
No vocals, no choir, no horror, no trailer impacts,
no imitation of any existing song or film score.
A repeat-friendly arrangement with a clear phrase boundary.
```

这条路线解决的是平台授权条件下的商业使用，不保证输出独占版权。若预算严格为零，直接选用本方案的 CC0 音乐，不必为了 P0 先购买音乐服务。

## 8. P1／P2：一致性比一次生成更多内容更重要

### 8.1 动画：Krita 做 6 帧攻击试点

Krita 可免费用于商业创作，软件 GPL 不要求把你的画作公开或采用 GPL。[Krita 官方许可](https://krita.org/en/about/license/)

1. 打开已有单位图，建 512×512 透明画布，将武器、持武器手臂与躯干尽可能分层。被前景遮住的身体部分需要补画。
2. 切到 Animation 工作区，打开 Timeline 与 Onion Skin。保留同一画布和落脚参考线，不对每帧单独紧裁。
3. 先做 6 个关键姿态：待势、蓄力、蓄力末端、挥击／释放、随动、回位。用变形／旋转打草稿，再补关节与轮廓，避免只有整张纸片摇摆。
4. 示例时间配置：Flipbook 基础 20 fps，第 1~6 张分别保持 `1、1、1、1、2、2` 个节拍；总长 0.4 秒，第 4 张开始约在 0.15 秒。根据现有攻击回调对齐命中帧，不能仅靠改变播放速度凑节奏。
5. 逐张检查武器长度、脸型、衣服轮廓、脚底位置。AI 可辅助提出蓄力／挥击草稿，但最后要人工统一；不要假设一次生成的六宫格已可直接使用。
6. 通过 Render Animation 导出透明 PNG 序列。命名 `Hero_Knight_Attack_00.png` 到 `_05.png`；原文接受 PNG 序列，也可拼成 3072×512 的单行 `Hero_Knight_Attack_6f.png`。
7. UE 中建立 Sprite 和 Paper2D Flipbook，设统一 Pivot／每帧时长；现有状态映射仍需程序接入，不是放入 Sheet 就会自动生效。

教程来源：[Krita 动画入门](https://docs.krita.org/en/user_manual/animation.html)、[序列导出](https://docs.krita.org/en/reference_manual/render_animation.html)、[Epic Flipbook 文档](https://dev.epicgames.com/documentation/unreal-engine/paper-2d-flipbooks-in-unreal-engine)。

先做最常出场的一个剑士或骑士。攻击效果验收后复用画布与节奏规范，再做 Walk。Hurt／Death 可以继续使用已有程序反馈，避免重复投入。

### 8.2 可选：CC0 模型在 Blender 中渲染成 2D 精灵

适合需要很多稳定动作、并愿意接受额外建模／渲染学习成本的情况。第 2 节的 Quaternius 两个包提供可用的 3D 基础，但其外观不会自动变成手绘风。

基本流程：打开 `.blend` 或导入 FBX → 选择相应动画 → 设置正交相机，调到能看清脸和少量头顶的 3/4 方向 → 固定灯光、相机和取景框 → 开启 Film Transparent → 输出 PNG RGBA 帧序列 → 在 Krita 统一色板、描线并按脚底对齐。建筑用相同相机渲染，便于整套透视一致。

可以把渲染图作为自己获授权的 AI 改绘参考；逐帧分别改绘容易闪烁，适合用它确定关键姿态后人工清稿。Blender 软件 GPL 与输出画作的许可是不同层面，底层模型仍按自己的 CC0 来源登记。[Blender 官方许可说明](https://docs.blender.org/manual/en/latest/getting_started/about/license.html)、[正交相机](https://docs.blender.org/manual/en/latest/render/cameras.html)、[透明背景](https://docs.blender.org/manual/en/latest/render/cycles/render_settings/film.html)。上述 Blender 页面本次通过官方搜索摘要核对，正文抓取未成功，具体界面以安装版本为准。

### 8.3 新内容的低成本规划

| 待定内容 | 建议候选 | 制作成本判断 |
|---|---|---|
| 佣兵 A | 弩手：矮而宽的剪影、胸前横弩 | 可以沿用人类体型；武器即可建立区分 |
| 佣兵 B | 狂战士：宽肩、短披风、大斧 | 近战动作可参考剑士，但需避免只换色 |
| 佣兵 C | 骑兵：骑手与坐骑先作为一张整体精灵 | 单帧可行，未来动画和占地处理更贵，优先级可以放后 |
| 人类敌军章节 | 叛军剑士／弓兵 | 同种族双方都可以出现；用旗色、盾形和阵营圈区分 |
| 哥布林章节 | 哥布林矛兵或投掷手 | 先 1 个试点，建立头身比与装备语言 |
| 亡灵地牢 | 骷髅剑士 + 一种小型地牢生物 | 不把整个敌军资产池锁死为亡灵 |
| Boss | 巨魔头目或幼龙，二选一后制作 | 先单帧 + 程序反馈，单独测试体型遮挡 |
| 家园建筑 | 训练营、铁匠铺、金库 | 同一底座、相机和材料色；升级只变局部屋顶／标志物 |

以上是美术与成本方向，未替你确认兵种、章节或数值设计。

## 9. 推荐实施顺序与验收

以下是制作批次，不是对你的空闲时间或 UE 熟练度做的工期承诺。

| 顺序 | 做什么 | 完成标准 |
|---|---|---|
| 1 | 整理来源表与字体；选现有参考图 | 每个来源有许可，视觉模板确定 |
| 2 | 先接 8 个固定键音效的可用版本 | 实战都能触发，音量不突兀 |
| 3 | 做一张共用圆环和光点，接治疗与火环 | 范围尺寸、落地位置、触发时机正确 |
| 4 | 补箭雨、火球、治疗波 | 五组技能都有清楚反馈，无黑底方块 |
| 5 | 制作战场地面与基础 UI | 1920×1080 下能看清单位；框角拉伸正常 |
| 6 | 确定首章敌军后做 1 个敌兵 | 敌我区分明显，64px 轮廓可读 |
| 7 | 选战斗 BGM 与结算短曲 | 循环无明显跳变，结算不抢其他关键提示 |
| 8 | 进入 P1 的单单位 Attack 试点 | 手、武器、脸和脚底稳定，命中时机对齐 |

建议同时保留两种画面检查：实际 1920×1080 战斗截图，以及 64px 单位／128px 卡图预览。不能只在 1024px 大图里看起来漂亮。

### 授权登记表模板

把下面列加到交付清单中即可，不能用“网上下载”或“AI生成”四个字代替来源。

| Asset_ID | 成品文件 | 原始资源名／作品 ID | 作者／工具与模型 | 来源与许可链接 | 许可／套餐 | 修改内容 | 生成／下载日期 | 署名文本或证据位置 |
|---|---|---|---|---|---|---|---|---|
| `MeleeHit` | `Content/Audio/SFX/MeleeHit.wav` | 填实际选用原文件名 | Kenney | 填对应包页 | CC0 | 裁剪、转单声道、峰值调整 | 填实际日期 | 原包许可证 |
| `Enemy_Skeleton_Swordsman` | `Content/Sprites/Enemies/Enemy_Skeleton_Swordsman.png` | 填生成记录 ID | 填实际工具／模型 | 填适用条款 | 填生成时套餐或模型许可 | 清边、缩放、脚底对齐 | 填实际日期 | 输入授权与生成记录 |
| `Icon_Fireball` | `Content/Icons/Misc/Icon_Fireball.png` | Fireball | Lorc | Game-icons 单图页 + CC BY 3.0 | CC BY 3.0 | 若有改色，注明 | 填实际日期 | 下面的示例署名 |

CC BY 的可用署名示例（只有实际采用时才放进游戏）：

> Fireball — Lorc / Game-icons.net. [Original asset](https://game-icons.net/1x1/lorc/fireball.html), licensed under [CC BY 3.0](https://creativecommons.org/licenses/by/3.0/). Modified: recolored and resized.

若未修改，应删掉修改说明；新增图标按其实际作者记录。字体随包保留原 LICENSE 和版权声明。许可记录可以统一放在工程的 `ThirdPartyNotices/` 中，最终发布时通过随附文件或合适的 Credits／Licenses 页面提供需要呈现的信息。

这份方案新增的是资源选择与教程，没有改动原始资产需求，也没有把候选资源标记成已下载、已授权给你的账户或已通过工程验收。
