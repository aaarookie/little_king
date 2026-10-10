# 美术与音频：统一规范、来源与接口

整理日期：2026-10-10；当前基线 v0.8.5。本页合并原美术规范、素材需求、A～D 批整合记录、步行修复和 v0.8.3／v0.8.4 素材说明，保留现行约定与来源入口。完整实际提示词、生成回执、原图、音频、许可和哈希保存在 `ArtSource/StorybookV1`，不在文档中重复展开。阶段发布与验证摘要见 [开发历程](DevelopmentHistory.md)，玩法和内容身份见 [GDD](01-GDD.md)、[内容图鉴](19-ContentCatalog.md)。

## 统一表现规范

温暖的手绘童话王国：水粉质感、克制的水彩颗粒、栗棕轮廓、松绿屋顶、奶油石墙与旧金装饰，上左方柔和暖光。小尺寸单位优先保证职业、武器和轮廓可辨；插画允许更多细节，正文和战场中心保持干净。新提示词以材质、光源、色板和构图统一风格，不以具体画师、工作室或已有作品角色作锚点。

| 用途 | sRGB 色值 |
| --- | --- |
| 松绿／苔绿 | `#193A32`／`#778458` |
| 羊皮纸／旧金 | `#E7D7AE`／`#BE9650` |
| 暖石灰／灰陶红 | `#C7BB99`／`#A76D50` |
| UI 底色／面板 | `#162520`／`#1F362D` |
| 按钮／悬停 | `#2D4336`／`#3F5743` |
| 正文／次级字／金线 | `#F4E6C8`／`#BDC1A4`／`#D1B170` |

UI 入口为 [LKPresentationStyle.h](../Source/little_king/LKPresentationStyle.h)，颜色从 sRGB 转为线性；面板使用 6px 小圆角和细金线。品质白／绿／蓝／紫／橙、阵营、黄色中线、非法落点与血条保留功能色，并辅以文字或形状。名称、费用、品质、容量、冷却与研究状态由 UI 绘制，不烘焙进图片。

- 菜单与卡面等比显示。家园建筑采用略高的正面三分之四插画视角；战斗相机仍正俯视，精灵铺在 XY 平面，插画透视不改变相机与碰撞。
- 默认角色图朝右，运行时镜像为左；上下移动保留最近一次左右朝向。当前只有左右两向，未制作四向／八向背面图。
- 地面明度低于单位，背景不画虚假路线、节点或没有碰撞的阻挡物；地图节点与连接来自实际数据。战场固定曝光、关闭泛光和运动模糊，平面精灵不投影。
- 原图按字节归档，透明素材核对真实 alpha，禁止假棋盘背景。切帧、枢轴和显示比例记入元数据；改变外观使用新版本素材并更新来源记录。
- 音效短、轻、有材质感：木、皮革、柔和击打、拨弦与钟声；高频战斗声音克制，避免长混响和持续叠加。

## 制作阶段与源文件

下表数字是各批新增数量，不代表当前全部内容数量。源档位于 `ArtSource/StorybookV1`，对应 UE 资产位于 `/Game/Art/StorybookV1` 的同名子目录。

| 阶段 | 已完成内容 | 原始来源与完整提示词 | 当期证据 |
| --- | --- | --- | --- |
| A：家园与声音基础，2026-09-19 | 菜单插画、七家园建筑、统一 UI；六条 Kenney 音效与三条原创短句；27 个 UE 资源 | [8 条图片提示词](../ArtSource/StorybookV1/Prompts/prompts.json)、[图片清单](../ArtSource/StorybookV1/Images/manifest.json)、[声音清单](../ArtSource/StorybookV1/Audio/manifest.json) | [StorybookAssets](validation/StorybookAssets-Validation.json) |
| B：战斗识别，2026-09-19～20 | 20 个透明单帧、17 张独立卡面；保留八个旧精灵与七张旧卡图；57 个 UE 资源 | [单帧目录与提示词](../ArtSource/StorybookV1/Battle/catalog.json)、[卡面参考与提示词](../ArtSource/StorybookV1/Battle/Prompts/cards.json)、[清单](../ArtSource/StorybookV1/Battle/manifest.json) | [BattleArt](validation/BattleArt-Validation.json) |
| C：世界与技能，2026-09-20～21 | 五区地面、三英雄营地、五节点徽记、三 FX 零件；28 条原创合成声；52 个 UE 资源 | [16 条图片提示词](../ArtSource/StorybookV1/World/catalog.json)、[图片清单](../ArtSource/StorybookV1/World/manifest.json)、[声音清单](../ArtSource/StorybookV1/World/Audio/manifest.json) | [WorldArt](validation/WorldArt-Validation.json) |
| D：动画与细化，2026-09-21～22 | 28 套 4×4 图集、448 姿势／140 动作、七建筑等级图、四循环音乐、两字体及转场；639 个 UE 资源；A～D 共 775 个 | [图片目录与提示词](../ArtSource/StorybookV1/Polish/catalog.json)、[来源清单](../ArtSource/StorybookV1/Polish/manifest.json)、[动画逐项回执](../ArtSource/StorybookV1/Polish/Animations)、[建筑逐项回执](../ArtSource/StorybookV1/Polish/Buildings) | [PolishArt](validation/PolishArt-Validation.json) |
| v0.8.1 步行修复，2026-09-22～23 | 24 张双足角色四帧图，替换 96 个旧 Move 姿势；144 个 UE 资源，旧图保留；双头龙沿用四足图 | [原图、参考、完整提示词与帧清单](../ArtSource/StorybookV1/MovementFix/manifest.json)；原始回执在同目录 | [MovementFix](validation/MovementFix-Validation.json) |
| v0.8.3 天使与法术，2026-10-01～02 | 十一天使单位共享五套 16 姿态图集、九法术卡图、六原创声；125 个 UE 资源 | [14 次实际生成](../ArtSource/StorybookV1/V083/generation.json)、[素材清单](../ArtSource/StorybookV1/V083/manifest.json)、[声音清单](../ArtSource/StorybookV1/V083/Audio/manifest.json) | [V083Assets](validation/V083Assets-Validation.json)、[阶段记录](53-v0.8.3Changes.md) |
| v0.8.4 城防与驻守，2026-10-08 | 七建筑主图、攻城车独立图集、七原创声；22 Sprite／35 Flipbook；78 个 UE 资源 | [8 次实际生成](../ArtSource/StorybookV1/V084/generation.json)、[素材清单](../ArtSource/StorybookV1/V084/manifest.json)、[声音清单](../ArtSource/StorybookV1/V084/Audio/manifest.json) | [V084Assets](validation/V084Assets-Validation.json)、[阶段记录](57-v0.8.4Changes.md) |

A 批菜单为 1536×1024 RGB，七建筑为实际 1254×1254 RGBA；B／C 批实际图片为 1254×1254，五地面不透明，其余类别按清单记录 alpha。提示词请求尺寸不等于实际输出尺寸。D 与后续图集以各批 manifest、frames 元数据为准，不将请求的 2048 当作实际尺寸。

B 批卡面参考对应的新单帧；D 批和步行修复参考既有角色／建筑；v0.8.4 攻城车动画参考本批主图。v0.8.4 七主图兼作卡图来源，因此审计中的十四个世界／卡图使用条目对应八个独立 PNG，不能解读为十四次生成。六种固定建筑使用静态 Sprite 和五个单帧状态；攻城车有待机 4、移动 4、攻击 4、受击 2、损毁 2 个真实姿态。

v0.8.2 的研究、市场、暂停及骷髅法阵复用既有卡图、地图、字体与 UI：骷髅法阵沿用骷髅兵卡图，其范围和预警由 Canvas 绘制。v0.8.3 黑云复用 [FX_Dust](../ArtSource/StorybookV1/World/Images/T_FX_Dust.png)，运行时染色并叠加雾团。上述复用没有新增图片提示词。

v0.8.5 草丛、沼泽、篝火和高山采用项目代码绘制的初版几何／SVG：浏览器为 `Tools/BattleDesigner/app.js`，UE 为 `ALKBattleTerrain.cpp`，复用基础网格、占位材质和 Noto 字体，无新图片、音效或音乐。功能边界见 [地形与遭遇设计](62-TerrainAndEncounterDesigner.md)。

## 许可与可追溯来源

生成图片登记为“项目生成／内置 imagegen”，按适用工具条款管理，不标为 CC0，不声称独占，也不补造工具未提供的模型版本或随机种子。源文件清单记录实际提示词、工具返回路径、参考图、日期、尺寸与 SHA-256；原始路径用于追溯，项目内归档路径用于协作。

`Content/Icons`、`Content/Sprites` 的原型图片属于项目既有资产，历史提示词未归档。文件名中的 ChatGPT 标记不能证明生成参数或参考图授权；后续以它们作身份参考也不补造原图历史。

| 外部来源 | 实际采用与归档 |
| --- | --- |
| Kenney Interface Sounds | `click_003.ogg` → `UIClick`；[原包 CC0 文本](../ArtSource/StorybookV1/Licenses/Kenney-interface-sounds-License.txt) |
| Kenney RPG Audio | `knifeSlice.ogg` → `RangedShoot`，`dropLeather.ogg` → `UnitDied`，`bookPlace1.ogg` → `CardPlay`；[原包 CC0 文本](../ArtSource/StorybookV1/Licenses/Kenney-rpg-audio-License.txt) |
| Kenney Impact Sounds | `impactPunch_medium_000.ogg` → `MeleeHit`，`impactSoft_medium_000.ogg` → `HitTaken`；[原包 CC0 文本](../ArtSource/StorybookV1/Licenses/Kenney-impact-sounds-License.txt) |
| MiniMax-Music3 | 本地生成 Home／Expedition／Battle／Boss 四首；[Community License 原文](../ArtSource/StorybookV1/Polish/Licenses/MiniMax-Music3-LICENSE.txt)、[实际 Caption／Lyrics](../ArtSource/StorybookV1/Polish/Music/prompts.json)、[原音与调用回执](../ArtSource/StorybookV1/Polish/Music/Originals)、[处理清单](../ArtSource/StorybookV1/Polish/Music/manifest.json) |
| Noto CJK contributors | 正文／数字 `NotoSansCJKsc-Regular.otf`、标题 `NotoSerifCJKsc-SemiBold.otf`；[Sans OFL 1.1](../ArtSource/StorybookV1/Polish/Licenses/NotoSans-OFL.txt)、[Serif OFL 1.1](../ArtSource/StorybookV1/Polish/Licenses/NotoSerif-OFL.txt)；原 OTF 不改动，下载地址与哈希在 Polish manifest |

Kenney 的精确下载 URL、包哈希与 2026-09-19 核对记录见 [downloads.json](../ArtSource/StorybookV1/Licenses/downloads.json)，采用的原 OGG 保存在 [Audio/Originals](../ArtSource/StorybookV1/Audio/Originals)。Music Jingles 只下载核对，未采用；其许可保留在 [Licenses](../ArtSource/StorybookV1/Licenses)。`RangedShoot` 是短挥动声复用，当前内置弓箭等改用 C 批细分类合成声，不描述为实录弓弦。

Music3 归档许可包含界面署名与年度收入门槛条款，具体以原文为准；开始页保留“音乐：MiniMax-Music3 · AI 生成”。仓库只整合生成音频，不分发模型或推理服务。实际调用路径曾为 `E:/minimax_music3/repo`，再次使用前核实；`GeneratePolishMusic.py` 支持 `LK_MUSIC3_INSTALL`。四首请求均为 48 秒、30 steps、low VRAM，seed 为 210901～210904；实际循环长度依次为 32.300、25.350、20.675、46.554 秒。

原创合成声音不含外部录音、人声或乐器采样，制作说明、种子、音高、包络、峰值与哈希保存在各批音频 manifest 和 `Prepare*Audio.py`。它们不是 AI 音乐模型输出，没有模型提示词。UI 边框、范围圈、弹道拖线、状态标记、地面换色、转场遮罩等原生图形由项目代码创作。

## 关键表现接口

| 入口 | 现行约定 |
| --- | --- |
| [LKPresentationStyle](../Source/little_king/LKPresentationStyle.h) | 共享色板、字体、面板、按钮状态及按钮音；品质语义独立于主题 |
| [LKBattleArt](../Source/little_king/LKBattleArt.cpp)、[LKWorldArt](../Source/little_king/LKWorldArt.cpp)、[LKPolishArt](../Source/little_king/LKPolishArt.cpp) | 单位、区域、营地、节点、FX、动画、等级图与字体的稳定映射；自定义 Sprite／Icon 优先，缺资源保留回退 |
| [LKV083Art](../Source/little_king/LKV083Art.cpp)、[LKV084Art](../Source/little_king/LKV084Art.cpp) | 新单位、卡图、五状态与声音映射；区分 UnitId／CardId，例如 `Unit_WallCommon` 与 `Card_WallCommon` |
| [ULKUnitAnimationComponent](../Source/little_king/ULKUnitAnimationComponent.cpp) | 优先级：退场 > 受击 > 攻击 > 移动 > 待机；在单位和移动更新后采样。位移驱动步态，停止保留步态进度；控制和复活按实际状态处理；动画不决定伤害、冷却或 RNG |
| [ULKUnitMovementComponent](../Source/little_king/ULKUnitMovementComponent.cpp) | `ConsumeVisualTravel` 提供表现位移，普通软分离不累计步行；飓风位移另带 `bWindDriven`，受控时仍可更新风推朝向 |
| [ALKUnitBase](../Source/little_king/ALKUnitBase.cpp) | `SetVisualFacingRight` 保留镜像符号；精灵按稳定脚底位置排序，仅改显示相对 Z，不抬高逻辑根节点，不改变碰撞半径 |
| [ALKHomeBuildingActor](../Source/little_king/ALKHomeBuildingActor.cpp) | 按 BuildingId 绑定家园图；`Home_StatueSaintMaria` 映射到 `SP_Home_Statue`。神像 Lv2～4、金库 Lv2～5 用等级图，Lv1 沿用 A 图，超出级别取最高现有图 |
| [ULKPresentationSubsystem](../Source/little_king/ULKPresentationSubsystem.cpp) | 短时效果队列、World／声音 ID 节流、场地与曝光；[ALKPresentationHUD](../Source/little_king/ALKPresentationHUD.cpp) 绘制范围、状态、弹道和血条。效果在实际生效处发出，火球仍是唯一小震动来源 |
| [ULKGameData](../Source/little_king/ULKGameData.cpp) | `EnsurePresentationDefaults` 只补缺少的 SoundMap 键；已配置资源和显式空值均优先，空值表示静音。`bUseDefaultSoundSet=false` 停止补默认，不影响独立按钮音 |
| [ULKJourneyPresentationSubsystem](../Source/little_king/ULKJourneyPresentationSubsystem.cpp) | GameInstance 内管理音乐、转场、字体引用；音乐默认音量 0.25、交叉淡化 1.2 秒、最多双声部；`SetMusicVolume(0～1)` 是设置接口，设置页仍为占位 |

动画默认 4×4：0～3 待机、4～7 移动、8～11 攻击、12～13 受击、14～15 退场。24 个旧双足角色优先使用 `MovementFix/FB_Walk_<UnitId>` 四帧 Move，其他动作仍取原图集。动画仅绑定内置默认精灵；自定义精灵仍获得排序和左右镜像。帧定义见 [Polish/frames.json](../ArtSource/StorybookV1/Polish/frames.json)、[V083/frames.json](../ArtSource/StorybookV1/V083/frames.json)、[V084/frames.json](../ArtSource/StorybookV1/V084/frames.json)。UE `CustomPivotPoint` 使用整张纹理坐标，血条使用稳定待机边界，避免换行切帧或举武器造成跳动。

本轮调研优化复用上述资源，没有新增外部动画或生成图集。`LKAnimationMetadata` 为不同角色提供步幅、帧时长权重与接触标记，组件使用私有解析副本；非法字段独立回退。脚步来自有效步态接触区间，风移／冲刺不触发普通行走脚步；动作开始、完成和取消具有独立实例身份，结束时销毁通过 `RegisterActionComponent` 显式登记的临时表现组件，并停止其中的音频组件；既有全局 FX 与一次性声音仍沿用各自生命周期。逻辑前摇中断同步取消表现，正常攻击提交保留同一实例并进入接触和收招；动画事件回调取消或重置后不再采样旧动作。真实伤害仍由游戏攻击管线决定，动画 `Impact()` 仅同步接触姿势；具体接口和验证见 [实施记录](OptimizationImplementation.md)。

家园完整画布通常对应 500cm；战斗角色按各批元数据等比显示，不缩放 Actor 改变逻辑尺寸。墙按不透明轮廓宽度对应 300 UE 单位（3 米），旋转方向、施工进度和范围由代码提供，逻辑墙体用于寻路和受击。首版贴图以 sRGB、UI 组、双线性、无 mip 和 Paper2D 无光照遮罩材质为主，地面构建上限 1024、B／C 小图通常 512；这些是现有配置，未宣称完成最终内存或大缩放性能优化。

效果队列最多 128 项；同类型、75ms 内、25cm 内重复效果合并，结算和世界销毁时清理。声音总并发沿用 `SC_Combat / SC_UI / SC_Signals = 12 / 3 / 2`，音乐另为 2；治疗、点燃、脚步等另按稳定 ID 限频。v0.8.3 六声为 `FreezeSpell / HolySummon / BlackCloud / Lightning / Hurricane / DivineBlessing`，v0.8.4 七声为 `WallBuild / WallRepair / WallBreak / WallReflect / SiegeRam / TowerShelter / HealingStation`，具体间隔见对应 Art 入口。声音节流不改变技能冷却或墙体反伤规则。

## 维护与验证边界

源文件处理脚本集中在 `Scripts`：`PrepareStorybookAudio`、`PrepareWorldAudio`、`PreparePolishAudio`、`PrepareV083Audio`、`PrepareV084Audio` 处理或合成声音；`PreparePolishFrames`、`PrepareMovementFix`、`PrepareV083Frames`、`PrepareV084Frames` 只读图片生成帧元数据。各批 `Import*` 脚本导入对应资源目录，`Audit*` 检查来源与格式；字体导入依赖编辑器 Slate。表现变更应同步素材清单、实际生成回执、映射与相关验证记录。提示词可读副本由相应脚本输出至忽略追踪的 `Saved/Documentation`；受版控的完整来源仍是上表 JSON 与逐项回执。

声音为 48 kHz／16-bit／单声道 PCM WAV，短音首尾淡化、不循环；四首 BGM 为 44.1 kHz／16-bit／立体声。BGM 保留原音，处理包括去直流、边缘裁剪、1.5 秒余弦重叠循环与电平调整，峰值上限 −3 dBFS。峰值、格式、循环接缝和引擎生命周期检查不能替代真实耳机／扬声器的主观试听与最终混音。

各批源文件审计、历史 UE 回归及真实 720p／1080p 画面证据保留在上表报告和 [images](images)；本机 `Saved` 原始日志不保证在其他克隆中存在。历史数字只表示当期验证，不能充当本次重跑结果。打包与硬件验收继续后置。

前述文档归并阶段仅整理文档：旧文档中的 **142 条图片实际提示词、4 条音乐 Caption 与 4 条 Lyrics** 已逐项与现有 JSON 归档比对，忽略排版空白后全文一致，因此删除重复正文不会丢失实际输入。该阶段未修改原图、音频、许可、生成回执或历史验证报告，未重新运行 UE、素材导入或音频生成。后续代码优化的构建与回归单独记录在 [实施记录](OptimizationImplementation.md)。
