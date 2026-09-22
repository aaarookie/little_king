# 免费素材获取、制作与 UE 5.8 接入

更新：2026-09-22。本页替代旧泛用推荐清单，以 A/B/C/D 批实际采用的资源为准。风格 [07](07-ArtStyleGuide.md)，全部需求 [12](12-AssetRequest.md)，来源/提示词 [36](36-ArtAudioSources.md)，整合记录 [A 批 37](37-ArtAudioIntegration.md) / [B 批 38](38-BattleArtIntegration.md) / [C 批 40](40-WorldSkillsArtIntegration.md) / [D 批 42](42-PolishArtIntegration.md)。

## 直接试玩：本批不需要手动下载

PNG 原图、WAV、采用的原始 OGG、许可证、完整提示词和 `.uasset` 已放进工程。本批不需要你注册素材网站、购买素材或手工制作蓝图。停止 Play 并关闭编辑器，完整构建一次，再打开项目点击 Play。新增反射属性不适合只靠 Live Coding。

```powershell
& 'E:\epic\UE_5.8\Engine\Build\BatchFiles\Build.bat' little_kingEditor Win64 Development '-Project=E:\little_hero\little_king\little_king.uproject' -WaitMutex -NoHotReloadFromIDE -gather
```

看到 `Result: Succeeded` 后打开工程。开始页显示王国插画；家园显示七座建筑与统一色板。建筑依旧可点击，快捷键 1～7 保留。战斗原有八个声音事件及 C 批新增 28 个键自动获得默认素材；常用原生按钮带短按下音。远征地图显示五区纹理与节点徽记，战场显示对应地面、三种营地和技能效果。D 批进一步接入角色五态动画、神像/金库等级外观、四场景音乐、中文字体与换场表现；全素材隔离预览和字体重导入步骤见 42。

## 目录与资源责任

| 路径 | 内容 |
| --- | --- |
| `ArtSource/StorybookV1/Images` | 八张未改图的生成原图与 SHA-256/尺寸/alpha 清单 |
| `ArtSource/StorybookV1/Prompts/prompts.json` | 每张实际完整提示词，以资产名为键 |
| `ArtSource/StorybookV1/Audio` | 九条统一格式 WAV、处理记录、六个采用的原始 OGG |
| `ArtSource/StorybookV1/Licenses` | 原包许可证、来源 URL、下载包哈希 |
| `Content/Art/StorybookV1` | 引擎贴图、PaperSprite、SoundWave、三个并发配置 |
| `Scripts/ImportStorybookAssets.py` | 自动导入与配置，仅写本批资源目录 |
| `Scripts/PrepareStorybookAudio.py` | 可复现音频处理与原创短句合成 |

A 批未重存旧数据表；B 批仅修正 `DT_Units / Building_Barracks / SpriteScale.Y` 的拉伸。DA_GameData、战斗 HUD 蓝图、卡牌资产和地图不需要手动重存。家园图片通过稳定 BuildingId 绑定；神像正式 ID 是 `Home_StatueSaintMaria`，映射到 `SP_Home_Statue`。原地面/路材质只在运行时换色；不移动建筑、不改变命中范围。

## 从源文件重新导入（可选）

工程中已有导入结果，无须每次启动都做这一步。协作者调整原图/WAV 后，关闭编辑器，在 PowerShell 执行：

```powershell
& 'E:\epic\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'E:\little_hero\little_king\little_king.uproject' -run=pythonscript '-script=E:\little_hero\little_king\Scripts\ImportStorybookAssets.py' '-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities' -unattended -nop4 -nosplash -NullRHI
```

脚本自动启用编辑器 Python 工具，不要求永久改项目插件配置；看到 `STORYBOOK_IMPORT_OK` 才算完成。导入清单写入 `Saved/ArtAudioImport.json`。这一步不验证画面，导入后仍须 Play 检查。

- 新素材限定在 `/Game/Art/StorybookV1`。同名重导入会更新本批资产，不覆盖早期图片；改风格时建议新建 V2 并显式改绑定。
- PaperSprite 工厂的 `InitialTexture` 在 UE 5.8 Python 中不可用；脚本创建后设置 SourceTexture/SourceDimension/SourceUV，让编辑器重建几何，再设置 pixels per unit。
- 家园图片画布宽度统一对应 500cm，实际尺寸是 1254×1254。脚本按实际贴图宽度计算比例，不假定 AI 严格输出请求的 1024。
- 若个别精灵显示方块，先检查 BuildingId 与 `SP_` 路径映射，再检查导入日志和透明通道。

## 音频复现与调节

需要重新生成 WAV 时，使用装有 `numpy`、`soundfile` 的 Python 运行 `Scripts/PrepareStorybookAudio.py`。脚本先读已归档的 `Audio/Originals`，无需重新下载；输出为 48kHz/16-bit/mono。随后执行上面的导入脚本。

技术处理包括转单声道、必要时由 44.1kHz 升采样、去 DC、6ms 边缘淡化、按用途峰值归一化。三段短拨弦由五次谐波合成，不含第三方采样；音符、时长与制作说明在脚本和音频 manifest 中。

默认战斗声音由 `ULKGameData::EnsurePresentationDefaults` 在运行时填入，仅补缺少的键。编辑器已配置的 SoundMap 项优先，显式空条目表示该事件静音。`Audio → bUseDefaultSoundSet` 可关闭这套自动补全；它不影响你自己配置的声音，也不控制 Slate 按钮音。按钮音在 `LKPresentationStyle::StyleButton` 中统一配置，未来设置菜单再接总音量。

并发资产 `SC_Combat/SC_UI/SC_Signals` 分别限制 12/3/2 个同组声音，超额不新播。已检测格式与峰值不削波；尚未做真实扬声器/耳机混音验收。D 批四首独立循环音乐已接入，音乐并发另为 2，不把结算短句当 BGM。

## 免费来源与未来下载

本批采用的外部文件仅来自以下 Kenney CC0 包，下载和许可证核对日为 2026-09-19：

- [Impact Sounds](https://kenney.nl/assets/impact-sounds)：近战、轻受击。
- [RPG Audio](https://kenney.nl/assets/rpg-audio)：短挥动、皮革落地、书本放置。
- [Interface Sounds](https://kenney.nl/assets/interface-sounds)：短点击。

完整包可在官方页面点击 Download，再选 Continue without donating；无需购买 All-in-1。若以后需要更多候选，可下载到 `Saved/ArtAudioDownloads/<包名>`，保留 License.txt。当前采用的六个原文件已经随工程归档，因此**没有必须由你手动下载的项目**。

[Music Jingles](https://kenney.nl/assets/music-jingles) 已核对并缓存候选，本批最终未采用其中的短曲，不能把它写成音乐已接入。其他网站或包不因“免费”就视为可商用，实际采用时逐文件登记许可、作者和改动。AI 输出记录为项目生成，不写成 CC0；旧 ChatGPT 命名图片的历史提示词缺失，诚实保留这一状态。

## 后续协作

B 批的 20 个单帧、17 张卡面和默认绑定已接入；源文件在 `ArtSource/StorybookV1/Battle`，引擎资源在 `Content/Art/StorybookV1/Battle`。专用导入脚本为 `Scripts/ImportBattleArt.py`，其 UE 5.8 操作、可选换图步骤与验证见 [38](38-BattleArtIntegration.md)。本批同样无须手动下载或逐一建立蓝图。

C 批已完成五区地面、三营地、五节点图标、三 FX 零件、28 个专属声音及战斗 HUD 主题，共 52 个引擎资产。完整新手教程、调参入口和隔离预览命令见 [40](40-WorldSkillsArtIntegration.md)。

### C 批资源重导入与声音复现（可选）

源文件在 `ArtSource/StorybookV1/World`，引擎资源在 `Content/Art/StorybookV1/World`。只有协作者改过源文件才需要重导入；已有结果可直接 Play。关闭编辑器后执行：

```powershell
& 'E:\epic\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'E:\little_hero\little_king\little_king.uproject' -run=pythonscript '-script=E:\little_hero\little_king\Scripts\ImportWorldArt.py' '-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities' -unattended -nop4 -nosplash -NullRHI
```

完成标志是 `WORLD_ART_IMPORT_OK assets=52`，导入清单为 `Saved/WorldArtImport.json`。该脚本不会修改地图、数据表或蓝图。先有 A 批资源时，C 批声音直接复用其三个并发资产；完整工程已包含全部结果。

重新合成 C 批声音使用带 numpy 的 Python 运行 `Scripts/PrepareWorldAudio.py`，WAV 写入采用标准库 wave，不需要 soundfile；A 批原音频处理脚本仍需 soundfile。`Scripts/AuditWorldArt.py` 另需 Pillow，检查原图 alpha/哈希与 WAV 格式/峰值，更新来源清单及 41 提示词文档；只读图像，不做像素加工。

D 批已完成：28 组五态动画、四首本地 Music3 音乐、七等级外观、转场与 Noto 字体，共 639 资产。详细新手教程、可选重导入、Music3 参数与截图命令见 [42](42-PolishArtIntegration.md)。字体导入须用正常编辑器的 Python 输入，不能用无 Slate 的命令模式。每次交付更新需求状态、来源台账、实际提示词与导入路径，最后在真实相机和 UI 尺寸检查。内容尚未设计（例如市场商品）的素材不提前编造。
