# 美术与音频来源台账

更新：2026-09-23。A/B/C/D 素材与后续步行修复已整合，各批历史来源分别记录；Sprint 6 仍不恢复，打包与硬件验收不在本轮范围。需求与缺口见 [12](12-AssetRequest.md)，风格规范见 [07](07-ArtStyleGuide.md)。

## v0.8 后续：步行显示修复

2026-09-23 追加的战斗显示修复新增 24 张 RGBA 步行图，全部使用内置 `image_gen`，分别参考本项目对应的 D 批旧角色图集；没有外部图片、采样、音乐或音效新增。原输出 PNG 按字节复制留档，UE 只按元数据裁取四姿势和设置枢轴/显示比例。生成图按适用工具条款管理，不标为 CC0，不编造种子或工具未提供的模型版本。实际完整提示词见 [46](46-MovementFixPrompts.md)，逐项输入参考、原始输出路径、日期、哈希与裁取参数见 [MovementFix manifest](../ArtSource/StorybookV1/MovementFix/manifest.json)，144 个新增引擎资源及修复记录见 [45](45-BattlePresentationFixes.md)。以下 A～D 历史来源保留。

## 制作批次 A：家园与声音基础

本批先完成可独立验收的一条链：开始界面插画、七座家园建筑、统一 UI 色板与边框、八个既有战斗音效键和界面点击音。角色单帧、逐帧动画、五区域地面、技能特效和循环音乐另列制作进度，未完成项不标作已接入。

生成图片使用内置 imagegen。每张资产分别保存完整提示词到 `ArtSource/StorybookV1/Prompts`，原始图片保存在 `Images`；新引擎资产放入 `/Game/Art/StorybookV1`，不覆盖旧图。AI 生成图不标成 CC0 或第三方开源素材。

音效优先采用 Kenney 的 CC0 包，保留原包的许可证与下载 URL、源文件名、处理步骤和 SHA-256。首次来源核对日期为 2026-09-19。

| 来源 | 作者 | 页面许可 | 本批实际采用 |
| --- | --- | --- | --- |
| [Interface Sounds](https://kenney.nl/assets/interface-sounds) | Kenney | CC0 | 1 条按钮点击 |
| [RPG Audio](https://kenney.nl/assets/rpg-audio) | Kenney | CC0 | 3 条：发射暂用短挥动、倒下、出牌 |
| [Impact Sounds](https://kenney.nl/assets/impact-sounds) | Kenney | CC0 | 2 条：近战与轻受击 |
| [Music Jingles](https://kenney.nl/assets/music-jingles) | Kenney | CC0 | 已核对候选，本批未采用 |

实际采用六个 Kenney 音效；Music Jingles 只下载核对，本轮未采用。开战/胜负三条使用下表的原创程序合成短句。

## 旧资产

`Content/Icons` 和 `Content/Sprites` 的既有图已由项目提供，保留使用；文件名中的 ChatGPT 标记只能说明命名，不能证明生成参数或原始参考图的授权。本轮未获得其原始提示词，因此记录为“项目既有，提示词未归档”，不伪造历史来源。标题插画、卡牌插画与战场小精灵允许不同细节密度，但应共享色板和材质。


## A 批实际图片

全部使用内置 imagegen，未提供第三方输入图；每张单独生成，原图未做后处理改绘。工具没有提供可核验的模型版本/随机种子，本台账不编造这些信息。图片不是开源包，不标为 CC0。菜单 RGB，七建筑均是真 RGBA，实际 alpha 最小值 0、最大值 255。

| 图片 ID | 内容 | 引擎绑定 |
| --- | --- | --- |
| T_MenuKingdom | 暖光城堡庭院、石路与远山 | 开始界面 KingdomIllustration |
| T_Home_Statue | 圣玛丽亚神龛、金环、灯与常春藤 | Home_StatueSaintMaria → SP_Home_Statue |
| T_Home_Library | 绿顶书徽圆塔 | Home_Library |
| T_Home_Gate | 开放的双塔大门 | Home_Gate |
| T_Home_HeroHouse | 盾徽半木英雄之家 | Home_HeroHouse |
| T_Home_Treasury | 钱币徽记金库 | Home_Treasury |
| T_Home_Barracks | 剑盾徽记军营 | Home_Barracks |
| T_Home_WarRoom | 地图桌与行囊帐篷 | Home_WarRoom |

完整原样提示词见 [提示词归档](../ArtSource/StorybookV1/Prompts/prompts.json)，另在 [图片 manifest](../ArtSource/StorybookV1/Images/manifest.json) 与文件尺寸、alpha、SHA-256 一起登记。上面的中文是内容索引，不冒充当时输入给工具的提示词。

### 每张实际提示词

#### T_Home_Barracks

Use case: stylized-concept. Production art for Little King, a warm storybook fantasy strategy game. Hand painted gouache with restrained watercolor grain, confident chestnut outlines, simplified readable silhouettes, matte surfaces, gentle amber light from upper left. Unified palette: deep pine green #193A32, moss #778458, parchment #E7D7AE, antique gold #BE9650, warm limestone #C7BB99, muted terracotta #A76D50. Elegant modest leaf and crown ornaments. No named artist, no existing franchise characters, no text, no letters, no logo, no watermark, no photorealism, no 3D render, no pixel art. A compact medieval barracks: low limestone and timber hall with moss-green tiled roof, crossed sword and shield emblem above the entrance, restrained muted terracotta banner, wooden practice dummy beside the wall. Recognizable complete isolated building prop. Square 1024 by 1024 canvas. Single centered object, full silhouette within 12 percent padding. Three-quarter front view with clearly visible roof, consistent mild raised viewing angle, front door facing viewer. True transparent RGBA background, transparent everywhere outside the isolated object, no painted checkerboard, no colored backdrop, no landscape, no ground plane, no cast shadow, no frame.

#### T_Home_Gate

Use case: stylized-concept. Production art for Little King, a warm storybook fantasy strategy game. Hand painted gouache with restrained watercolor grain, confident chestnut outlines, simplified readable silhouettes, matte surfaces, gentle amber light from upper left. Unified palette: deep pine green #193A32, moss #778458, parchment #E7D7AE, antique gold #BE9650, warm limestone #C7BB99, muted terracotta #A76D50. Elegant modest leaf and crown ornaments. No named artist, no existing franchise characters, no text, no letters, no logo, no watermark, no photorealism, no 3D render, no pixel art. A broad kingdom expedition gate: two short limestone watchtowers with moss-green roofs, large open arched gateway in the center, antique-gold crown emblem and restrained green banners. Gateway clearly OPEN with no opaque wall across the passage. Recognizable complete isolated building prop. Square 1024 by 1024 canvas. Single centered object, full silhouette within 12 percent padding. Three-quarter front view with clearly visible roof, consistent mild raised viewing angle, front door facing viewer. True transparent RGBA background, transparent everywhere outside the isolated object, no painted checkerboard, no colored backdrop, no landscape, no ground plane, no cast shadow, no frame.

#### T_Home_HeroHouse

Use case: stylized-concept. Production art for Little King, a warm storybook fantasy strategy game. Hand painted gouache with restrained watercolor grain, confident chestnut outlines, simplified readable silhouettes, matte surfaces, gentle amber light from upper left. Unified palette: deep pine green #193A32, moss #778458, parchment #E7D7AE, antique gold #BE9650, warm limestone #C7BB99, muted terracotta #A76D50. Elegant modest leaf and crown ornaments. No named artist, no existing franchise characters, no text, no letters, no logo, no watermark, no photorealism, no 3D render, no pixel art. A welcoming heroes' lodge: cream half-timber cottage, moss-green pitched roof, amber windows, a carved gold shield emblem over the doorway, small bench, climbing rose bush. Recognizable complete isolated building prop. Square 1024 by 1024 canvas. Single centered object, full silhouette within 12 percent padding. Three-quarter front view with clearly visible roof, consistent mild raised viewing angle, front door facing viewer. True transparent RGBA background, transparent everywhere outside the isolated object, no painted checkerboard, no colored backdrop, no landscape, no ground plane, no cast shadow, no frame.

#### T_Home_Library

Use case: stylized-concept. Production art for Little King, a warm storybook fantasy strategy game. Hand painted gouache with restrained watercolor grain, confident chestnut outlines, simplified readable silhouettes, matte surfaces, gentle amber light from upper left. Unified palette: deep pine green #193A32, moss #778458, parchment #E7D7AE, antique gold #BE9650, warm limestone #C7BB99, muted terracotta #A76D50. Elegant modest leaf and crown ornaments. No named artist, no existing franchise characters, no text, no letters, no logo, no watermark, no photorealism, no 3D render, no pixel art. A cozy round medieval library: squat round limestone tower with a steep moss-green conical roof, amber arched windows, a large carved open-book emblem above its wooden door, two small book crates. Recognizable complete isolated building prop. Square 1024 by 1024 canvas. Single centered object, full silhouette within 12 percent padding. Three-quarter front view with clearly visible roof, consistent mild raised viewing angle, front door facing viewer. True transparent RGBA background, transparent everywhere outside the isolated object, no painted checkerboard, no colored backdrop, no landscape, no ground plane, no cast shadow, no frame.

#### T_Home_Statue

Use case: stylized-concept. Production art for Little King, a warm storybook fantasy strategy game. Hand painted gouache with restrained watercolor grain, confident chestnut outlines, simplified readable silhouettes, matte surfaces, gentle amber light from upper left. Unified palette: deep pine green #193A32, moss #778458, parchment #E7D7AE, antique gold #BE9650, warm limestone #C7BB99, muted terracotta #A76D50. Elegant modest leaf and crown ornaments. No named artist, no existing franchise characters, no text, no letters, no logo, no watermark, no photorealism, no 3D render, no pixel art. A small shrine to Saint Maria: ivory robed female guardian statue with hands open in a gentle blessing, antique-gold halo, low octagonal limestone pedestal, two tiny amber lanterns and a little ivy. Recognizable complete isolated building prop. Square 1024 by 1024 canvas. Single centered object, full silhouette within 12 percent padding. Three-quarter front view with clearly visible roof, consistent mild raised viewing angle, front door facing viewer. True transparent RGBA background, transparent everywhere outside the isolated object, no painted checkerboard, no colored backdrop, no landscape, no ground plane, no cast shadow, no frame.

#### T_Home_Treasury

Use case: stylized-concept. Production art for Little King, a warm storybook fantasy strategy game. Hand painted gouache with restrained watercolor grain, confident chestnut outlines, simplified readable silhouettes, matte surfaces, gentle amber light from upper left. Unified palette: deep pine green #193A32, moss #778458, parchment #E7D7AE, antique gold #BE9650, warm limestone #C7BB99, muted terracotta #A76D50. Elegant modest leaf and crown ornaments. No named artist, no existing franchise characters, no text, no letters, no logo, no watermark, no photorealism, no 3D render, no pixel art. A sturdy little royal treasury: squat octagonal limestone vault, moss-green roof, heavy reinforced oak door with a gold coin crest, two small sealed coin chests by the entrance. Recognizable complete isolated building prop. Square 1024 by 1024 canvas. Single centered object, full silhouette within 12 percent padding. Three-quarter front view with clearly visible roof, consistent mild raised viewing angle, front door facing viewer. True transparent RGBA background, transparent everywhere outside the isolated object, no painted checkerboard, no colored backdrop, no landscape, no ground plane, no cast shadow, no frame.

#### T_Home_WarRoom

Use case: stylized-concept. Production art for Little King, a warm storybook fantasy strategy game. Hand painted gouache with restrained watercolor grain, confident chestnut outlines, simplified readable silhouettes, matte surfaces, gentle amber light from upper left. Unified palette: deep pine green #193A32, moss #778458, parchment #E7D7AE, antique gold #BE9650, warm limestone #C7BB99, muted terracotta #A76D50. Elegant modest leaf and crown ornaments. No named artist, no existing franchise characters, no text, no letters, no logo, no watermark, no photorealism, no 3D render, no pixel art. A small expedition supply and war-planning pavilion: moss-green canvas tent on a low wooden platform, cream cloth edging with tiny gold crown ornament, central open flap revealing a rolled map on a table, travel packs and two small crates. Recognizable complete isolated building prop. Square 1024 by 1024 canvas. Single centered object, full silhouette within 12 percent padding. Three-quarter front view with clearly visible roof, consistent mild raised viewing angle, front door facing viewer. True transparent RGBA background, transparent everywhere outside the isolated object, no painted checkerboard, no colored backdrop, no landscape, no ground plane, no cast shadow, no frame.

#### T_MenuKingdom

Use case: stylized-concept. Production art for Little King, a warm storybook fantasy strategy game. Hand painted gouache with restrained watercolor grain, confident chestnut outlines, simplified readable silhouettes, matte surfaces, gentle amber light from upper left. Unified palette: deep pine green #193A32, moss #778458, parchment #E7D7AE, antique gold #BE9650, warm limestone #C7BB99, muted terracotta #A76D50. Elegant modest leaf and crown ornaments. No named artist, no existing franchise characters, no text, no letters, no logo, no watermark, no photorealism, no 3D render, no pixel art. Landscape 3:2 illustration filling the canvas, a welcoming tiny medieval kingdom courtyard on a hillside, a cream limestone castle and moss-green roofs on the right, winding stone lane, flowering shrubs, golden pennants, distant mountains and a gentle dawn sky. Calm atmosphere, strong large shapes, readable depth. No people. This goes inside a menu illustration panel with text ABOVE it, so no typography or UI painted in. Detailed edges, quiet central path.

## A 批实际音频

采用的六个原始 OGG 位于 ArtSource/StorybookV1/Audio/Originals。来源作者均为 Kenney，原包随附 CC0 文本允许个人和商业项目使用；保留自愿署名与原文。下载包哈希及精确 URL 见 [downloads.json](../ArtSource/StorybookV1/Licenses/downloads.json)，导出 WAV 与源 OGG 哈希见 [音频清单](../ArtSource/StorybookV1/Audio/manifest.json)。

| 事件 / WAV | 来源文件 / 制作方式 | 峰值目标 | 时长 |
| --- | --- | ---: | ---: |
| MeleeHit / S_MeleeHit.wav | Audio/impactPunch_medium_000.ogg | -17 dBFS | 0.4305 秒 |
| RangedShoot / S_RangedShoot.wav | Audio/knifeSlice.ogg | -19 dBFS | 0.5995 秒 |
| HitTaken / S_HitTaken.wav | Audio/impactSoft_medium_000.ogg | -23 dBFS | 0.118 秒 |
| UnitDied / S_UnitDied.wav | Audio/dropLeather.ogg | -19 dBFS | 0.4151 秒 |
| CardPlay / S_CardPlay.wav | Audio/bookPlace1.ogg | -16 dBFS | 0.2767 秒 |
| UIClick / S_UIClick.wav | Audio/click_003.ogg | -20 dBFS | 0.01 秒 |
| BattleStart / S_BattleStart.wav | 项目原创，五谐波拨弦合成 | -14 dBFS | 1.37 秒 |
| Victory / S_Victory.wav | 项目原创，五谐波拨弦合成 | -14 dBFS | 2.05 秒 |
| Defeat / S_Defeat.wav | 项目原创，五谐波拨弦合成 | -14 dBFS | 1.83 秒 |

六个外部声音只做单声道合并、必要时 48kHz 升采样、去 DC、6ms 边缘淡化和分用途峰值归一化。RangedShoot 当前采用短刀挥动的轻嗖声，作为通用发射反馈；不声称它是专门录制的弓弦。

原创三条并非 AI 音频模型生成，因此没有模型提示词。其制作说明为：“同一温暖、柔和的五谐波拨弦音色；开战短上行，胜利用 C 大调上行，失败用 A 小调下降；无长混响、无刺耳号角。”确切音高、起音时间、长度、包络与谐波参数保存在 [合成脚本](../Scripts/PrepareStorybookAudio.py) 和音频 manifest，可逐样本复现；未用外部旋律、录音或乐器采样。

## 代码绘制资源

UI 6px 圆角、旧金边框、按钮各状态和主题色为项目 C++ 原生绘制，入口 [LKPresentationStyle.h](../Source/little_king/LKPresentationStyle.h)。没有模型提示词；制作说明是“松绿深底、羊皮纸亮字、旧金细线，与建筑色板一致，禁用态可读，保留品质语义色”。家园地面/道路颜色也由代码设置，无额外贴图来源。

## 商用来源管理与剩余项

本批外部音效引用 [Kenney Impact Sounds](https://kenney.nl/assets/impact-sounds)、[RPG Audio](https://kenney.nl/assets/rpg-audio)、[Interface Sounds](https://kenney.nl/assets/interface-sounds)，均已保留各自原包许可。没有把“可免费下载”当作整站商业授权，也没有下载付费素材或使用账号凭证。

生成图按适用工具条款管理，不标成开源/CC0，不声称独占。旧图原始提示词缺失仍待项目补档；本轮新增资产资料齐全，无须用户手动下载。后续实际采用项分别登记在下方 B/C/D 段，未采用候选不能当作已接入。

实际改动、截图与测试见 [37](37-ArtAudioIntegration.md)。

## B 批实际图片（2026-09-19～20）

20 张透明战斗单帧、17 张卡面，均由本项目内置 image_gen 生成，合计 37 张，不新增第三方图片来源。单帧纯文字生成；卡图各以自己的单帧为参考保持身份一致。原图仅复制归档，未进行像素后处理。生成图不标为 CC0，也不编造模型版本或种子。

完整实际提示词逐项列于 [39](39-BattleArtPrompts.md)；机器可读记录为 [精灵目录](../ArtSource/StorybookV1/Battle/catalog.json)、[卡面提示词](../ArtSource/StorybookV1/Battle/Prompts/cards.json)、[来源清单](../ArtSource/StorybookV1/Battle/manifest.json)。清单含 SHA-256、实际 1254×1254 尺寸、alpha、日期、输入参考及其哈希。实际接入 57 个引擎资产和旧精灵复核情况见 [38](38-BattleArtIntegration.md)。

旧 8 个精灵与 7 张卡图仍按上面的“项目既有、历史提示词未归档”登记；兵营只调整引擎显示比例，没有改图。本批没有新增音效或音乐，声音来源仍为 A 批台账。

## C 批实际图片与声音（2026-09-20～21）

新增 16 PNG：五种地面、骑士/法师/游侠三帐篷、五类节点徽记、火球爆点/叶片/尘点三张透明零件。全部为内置 image_gen 的纯文字生成，没有外部参考图；原始 PNG 未做像素后处理。引擎仅缩放显示分辨率、运行时着色与叠加。生成图按适用工具条款管理，不标为 CC0、不声称独占，不编造模型版本或种子。

完整实际提示词见 [41](41-WorldArtPrompts.md)；[catalog](../ArtSource/StorybookV1/World/catalog.json) 保存输入与用途，[manifest](../ArtSource/StorybookV1/World/manifest.json) 保存作者/工具、实际日期、1254×1254 尺寸、alpha、原文件路径与 SHA-256。图像生成时延续 A/B 的水粉、松绿、羊皮纸、旧金、栗棕轮廓约定。

新增 28 WAV 全部为项目原创程序合成，不含第三方采样，不使用 AI 音频模型，因此没有模型提示词。实际制作说明为：“短、柔和的绘本拟音与魔法提示；用可复现的滤波噪声、拨弦谐波及钟声区分动作，不使用长噪声或刺耳高频。”确切音高、扫频、时长、包络、峰值、种子与节流参数见 [合成脚本](../Scripts/PrepareWorldAudio.py) 和 [音频来源清单](../ArtSource/StorybookV1/World/Audio/manifest.json)。每条均归档 WAV 与 SHA-256。

| 音频组 | 稳定键 |
| --- | --- |
| 发射/火球 | BowShot、SpearShot、MagicShot、FireballCast、FireballImpact |
| 恢复/亡灵 | Heal、Summon、Sacrifice、Revive |
| 技能/收益 | Backstab、Coin、Mimic、Dash、Empower、HeavyHit |
| 控制/龙息 | Stun、Freeze、Burn、IceBreath、FireBreath |
| 攻城/指令 | SiegeShot、SiegeImpact、Footstep、CampCommand |
| 服务/家园 | NodeEnter、Rest、Market、Upgrade |

BowShot 为合成拨弦发射声，不是现场录音。A 批通用 RangedShoot 资源仍保留，当前内置远程单位按武器改用 C 批细分类声音；嘲讽/集火共用 Empower/CampCommand，巨骨共用 Heal。未制作新外部下载项或循环 BGM。

C 批原生图形（斩弧、范围环、盾标、目标环、星标、薄冰、层数火点、箭矛石弹拖线、升级叶环的排列/运动）是项目代码创作，没有模型提示词。制作参数与生命周期保存在 [表现子系统](../Source/little_king/ULKPresentationSubsystem.cpp)、[战斗 HUD](../Source/little_king/ALKPresentationHUD.cpp)、[家园 HUD](../Source/little_king/ULKHomeHUDWidget.cpp)。它们使用上述三张生成零件及既有主题色，不引入其他素材来源。

C 批 52 个引擎资产、测试、画面、UE 5.8 教程和协作文件归属见 [40](40-WorldSkillsArtIntegration.md)。


## D 批实际图片、音乐与字体（2026-09-21）

新增 28 个透明 4×4 动画图集及七个建筑等级图，均由内置 image_gen 生成，以既有角色/建筑图作身份参考。35 个 PNG 保留原始字节；没有用 Python 调色、抠图或拼图。原型八图的历史来源缺口仍按上文登记，本次以其为参考并不补造原图历史提示词。

[43](43-PolishArtPrompts.md) 逐项保存实际工具完整输入（含追加约束），对应 `Animations/*.receipt.json` 和 `Buildings/*.receipt.json`。原图、参考哈希、实际尺寸/日期、许可分类见 [manifest](../ArtSource/StorybookV1/Polish/manifest.json)；[frames.json](../ArtSource/StorybookV1/Polish/frames.json) 是 UE 切帧/整图枢轴元数据，不是像素编辑结果。生成图按适用工具条款管理，不标 CC0、不声称独占。

| 来源 | 实际使用 | 许可归档与加工 |
| --- | --- | --- |
| [MiniMax-Music3 模型](https://huggingface.co/MiniMaxAI/MiniMax-Music3) | 本地生成 Home、Expedition、Battle、Boss 四首音乐 | [Community License 原文](../ArtSource/StorybookV1/Polish/Licenses/MiniMax-Music3-LICENSE.txt)，不是 CC0 |
| [Noto CJK 官方仓库](https://github.com/notofonts/noto-cjk) | NotoSansCJKsc-Regular.otf，正文/数字 | [Sans OFL 1.1](../ArtSource/StorybookV1/Polish/Licenses/NotoSans-OFL.txt)，原 OTF 不改动 |
| [Noto CJK 官方仓库](https://github.com/notofonts/noto-cjk) | NotoSerifCJKsc-SemiBold.otf，标题 | [Serif OFL 1.1](../ArtSource/StorybookV1/Polish/Licenses/NotoSerif-OFL.txt)，原 OTF 不改动 |

两个字体的直接下载地址、SHA-256 和许可哈希均在 manifest；作者记为 Noto CJK contributors。没有要求用户手动下载字体。UE 采用内嵌 FontFace/运行时组合字体，无系统字体依赖。

音乐实际安装路径 `E:/minimax_music3/repo`；调用该目录原有 inference 与环境，不修改模型或依赖。完整 caption/lyrics、seed、steps、请求/实际长度和耗时见 [原始回执目录](../ArtSource/StorybookV1/Polish/Music/Originals)，[实际提示词](43-PolishArtPrompts.md) 可直接用于本地模型。原音均保留；去直流、边缘裁剪、1.5 秒余弦重叠循环、电平调整与 16bit PCM 输出见 [处理脚本](../Scripts/PreparePolishAudio.py) 和 [音频 manifest](../ArtSource/StorybookV1/Polish/Music/manifest.json)。最终循环长度依次为 32.300、25.350、20.675、46.554 秒。

MiniMax 原许可含商业软件界面署名和年度收入门槛条款，不能把模型或生成音频写成“无限制 CC0”。本项目仅整合生成音频，不分发模型/推理服务；开始页已显示“音乐：MiniMax-Music3 · AI 生成”。具体条款以归档与[官方许可](https://huggingface.co/MiniMaxAI/MiniMax-Music3/raw/main/LICENSE)为准，不作生成作品独占或绝对无权利风险的承诺。

D 批淡入、遮罩、等级映射和音乐交叉淡化为原生代码表现，没有模型提示词。639 资产、验证及协作教程见 [42](42-PolishArtIntegration.md)。技术验收包含音频格式/峰值/生命周期，未把波形检查冒称为主观试听。
