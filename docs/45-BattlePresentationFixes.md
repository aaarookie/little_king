# 战斗显示修复 · 重叠、朝向与步行

日期：2026-09-22～23。基线：已发布 `v0.8`，本轮修复统一纳入 `v0.8.1`，保留旧标签。版本范围见 [47 发布说明](47-v0.8.1ReleaseNotes.md)。

## 问题与修复

| 用户反馈 | 原因 | 修改后的行为 |
| --- | --- | --- |
| 营地、角色、敌人相互重叠时变黑或闪烁 | 所有 Masked Paper2D 精灵都在同一 Z 平面写深度，重叠处发生深度竞争；平面图片还允许投影 | 按稳定脚底位置决定前后遮挡，每个精灵使用独立显示深度，关闭平面精灵投影；营地和自定义单帧也参与排序 |
| 朝向与移动方向相反 | 动画组件没有根据行走位移更新左右朝向，部分旧 Move 图与其他动作朝向不一致 | 行走时朝向服从实际主动位移；站定攻击时朝向目标；移动与攻击使用一致的右向图，再按需镜像 |
| 移动像滑行、重复伸同一条腿 | 旧步行姿势重复，短暂停顿会重置动作时钟，软分离也被当作走路；运动模糊进一步抹掉帧差 | 24 个双足角色换用四姿势步行图，步频跟随主动行走距离；停下后保留步态进度；推挤/击退/瞬移不累计普通步行，关闭战场运动模糊 |

朝向采用**左右两向**；纯上下移动保留最近一次左右朝向，避免细小横向偏移导致连续翻面。这一轮没有新增四向/八向背面图。镜像只改精灵局部宽度，碰撞体、Actor 朝向和战斗坐标不随之旋转；死亡缩放和攻击脉冲保留镜像符号。

遮挡按照画面脚底顺序决定，画面下方的角色遮住上方角色，同一脚底行用稳定对象 ID 打破平局。脚底偏移初始化时从待机图计算，不随抬脚、武器或倒地姿势抖动。只调整 Sprite 的相对 Z，不移动逻辑根节点。当前每个单位遍历本世界单位排序，复杂度 O(N²)，适用于现有受限兵力规模；未来扩展到大量单位时可集中排序。

## 协作者需要知道的入口

| 文件/接口 | 职责 |
| --- | --- |
| `ULKUnitMovementComponent::ConsumeVisualTravel` | 只提供寻路实际走过的位移，在软分离前记录，每次消费后清零 |
| `ULKUnitAnimationComponent::Initialize / UpdateDepth / UpdateFacing / TickComponent` | 固定脚底偏移、精灵深度、左右朝向、距离驱动步态；在单位和移动组件 Tick 后运行 |
| `ALKUnitBase::SetVisualFacingRight` | 对当前图片及反馈基准比例同时保留左右符号；单位初始化禁用 CastShadow |
| `LKPolishArt::WalkingUnitIds / Animation` | 24 个双足单位优先读取独立的 `FB_Walk_<UnitId>`；缺新图时仍回退旧 Move，其他四态保持原入口 |
| `ULKPresentationSubsystem::SetupBattlefield` | 战场固定曝光基础上将 MotionBlurAmount / MotionBlurMax 设为 0 |
| `LKMovementFixPreview` / `LKBattleArtPreview` | 编辑器专用的隔离画面检查入口，覆盖左右行走、停止、单位与三种营地重叠 |
| `Tests/LKMovementPresentationTests.cpp` | 四项回归：分层、朝向、距离/中断、步行资源完整性 |
| `Scripts/PrepareMovementFix.py` | 只读 PNG alpha/哈希，生成四帧裁取元数据、来源清单及完整提示词文档 |
| `Scripts/ImportMovementFix.py` | UE 5.8 批量导入独立步行资源；不修改 D 批原图、原 Sprite 或 Flipbook |
| `Scripts/AuditMovementFix.py` | 检查源图/导入资产、最终测试、视口截图、原五存档、文档链接并生成验证摘要 |

新动画入口仍只覆盖内置默认精灵。协作者显式指定的自定义 Sprite 保留自己的图片，同时获得排序与左右镜像。后续增加新角色时，默认图应朝右；步行四帧必须有可辨识的接触、抬脚和换脚姿势，保持身体尺寸与脚底基线一致。

## 素材与来源

新增 24 张原始 RGBA 图、96 个步行姿势；引擎新增 24 Texture + 96 Sprite + 24 Flipbook = **144 个资源**，全部位于 `Content/Art/StorybookV1/MovementFix`。与 v0.8 的 775 个资源合计 **919** 个。运行时每单位仍为五态；旧 448 姿势留档，其中 96 个旧步行姿势由本轮资源替代。双头龙保留四足步行图，三种攻击/生产建筑保留原动作。

所有新图使用内置 `image_gen`，逐一参考本项目自己的旧角色图集，延续原身份、装备与绘本色板。没有增加第三方图片或声音。原生成 PNG 仅复制，不做像素后处理；引擎通过 SourceUV/SourceDimension/CustomPivot 裁取，PPU 对齐旧待机高度，脚底对齐旧素材。生成内容按适用工具条款管理，不标成 CC0。

- [完整提示词](46-MovementFixPrompts.md)
- [源文件、参考、原始输出路径、SHA-256 与逐帧元数据](../ArtSource/StorybookV1/MovementFix/manifest.json)
- [统一素材来源台账](36-ArtAudioSources.md)

## UE 5.8 新手使用与复核

本轮代码和 `.uasset` 都已在工程内，无须手动下载素材、编辑材质或重新搭建蓝图。

1. 若编辑器一直开着，先保存自己的场景/蓝图并关闭编辑器，避免继续使用旧 DLL。
2. 用 Visual Studio 打开工程，选择 **Development Editor / Win64**，生成 `little_king`；也可在项目根目录 PowerShell 执行下方 Build 命令。
3. 打开 `little_king.uproject`，点击运行，从开始界面进入存档和远征。部署英雄后观察营地遮挡，开始战斗后观察双方相向移动；点营地再点另一侧位置，确认英雄立即朝行走方向翻面，到达后面向攻击目标。
4. 特别看停止后再次移动、冰冻恢复、相邻单位交错、英雄失能/复活；不应出现原地挤动就频繁切步行、翻面被攻击缩放取消或图片交叉闪黑。

```powershell
& 'E:\epic\UE_5.8\Engine\Build\BatchFiles\Build.bat' little_kingEditor Win64 Development '-Project=E:\little_hero\little_king\little_king.uproject' -WaitMutex -NoHotReloadFromIDE
```

若协作者只取得原图、或未来重新制作步行图，先用装有 Pillow/Numpy 的 Python 运行 `python -B Scripts/PrepareMovementFix.py`，再关闭编辑器并运行导入命令。正常取得完整工程不必重复导入。

```powershell
& 'E:\epic\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'E:\little_hero\little_king\little_king.uproject' -run=pythonscript '-script=E:\little_hero\little_king\Scripts\ImportMovementFix.py' '-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities' -unattended -nop4 -nosplash -NullRHI
```

独立画面检查命令如下，结果位于 `Saved/Screenshots/MovementFix`。它通过编辑器专用开关关闭玩家存档读写、使用独立存储名和 GameData 副本，并会自动退出；不要将此开关加入正常游戏启动参数。真实游戏的营地体积和禁止重叠部署规则仍然有效，展示场景只在生成之后临时排列对象制造遮挡。

```powershell
& 'E:\epic\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'E:\little_hero\little_king\little_king.uproject' '/Game/Maps/L_BattleTest' -game -MovementFixPreview -RenderOffscreen -windowed -forceres -ResX=1920 -ResY=1080 -unattended -nop4 -nosplash
```

## 验证记录

已完成 UE 5.8.1 `little_kingEditor / Win64 / Development` 构建。运行时代码最终全量 `LittleKing` **87/87 通过**：43 项无警告、44 项带既有测试环境/预期异常警告，0 失败、0 未执行。`Saved/Automation/MovementFix02/index.json` 保留原始报告。其后只为编辑器展示加入“着色器编译完成再截图”的等待并重新构建，不重复声称另跑一轮全量回归。

720p / 1080p 均完成真实渲染：每个分辨率 16 张左右步行样本、2 张停止样本、6 张角色/营地前后遮挡样本，合计 48 张。人工检查左右朝向、抬脚/落脚、停止、重叠换序；展示会等待着色器就绪，避免将编辑器首次编译时的黑色占位材质误当成最终画面。代表截图按原字节归档如下。

| 内容 | 1080p | 720p |
| --- | --- | --- |
| 向右步行 | [截图](images/MovementFix_Walk_Right_02_1080.png) | [截图](images/MovementFix_Walk_Right_02_720.png) |
| 向左步行 | [截图](images/MovementFix_Walk_Left_02_1080.png) | [截图](images/MovementFix_Walk_Left_02_720.png) |
| 敌方移到前方 | [截图](images/MovementFix_Overlap_230_1080.png) | [截图](images/MovementFix_Overlap_230_720.png) |
| 敌方回到后方 | [截图](images/MovementFix_Overlap_250_1080.png) | [截图](images/MovementFix_Overlap_250_720.png) |

24 源图 alpha/哈希、96 帧边界、144 引擎资源和源码检查通过；原有五个玩家存档与本轮开始前的基线哈希一致，未留下额外预览存档，`DT_Units` 数值表哈希不变。导入时出现 Paper2D 工厂的旧默认材质缺失警告，脚本随后显式指定正确 MaskedUnlit 材质；命令执行成功且资源通过引擎加载和渲染检查。来源、原始报告及截图哈希见 [验证摘要](validation/MovementFix-Validation.json)。

无需用户补做资源接线。本轮保留旧非步行动作素材，新增图仍是四姿势生成动画，尚未扩展为手工逐帧或多方向动画。打包与硬件测试继续后置。
