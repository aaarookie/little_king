# 美术与声音统一规范 · Storybook V1

更新：2026-09-21。A/B/C/D 批已接入，替代本文早期的极繁装饰与斜视建议。需求见 [12](12-AssetRequest.md)，来源/完整提示词见 [36](36-ArtAudioSources.md)，操作见 [13](13_Asset_Solutions.md)。Sprint 6 不恢复，打包与硬件验收仍后置。

## 视觉方向

**温暖的手绘童话王国：水粉质感、松绿屋顶、奶油石墙、旧金装饰。** 保留已有卡面的温暖色调。插画可有笔触和景深，小单位强调轮廓；装饰集中在屋檐、门徽和面板边缘，正文与战场中心保持干净。

统一依靠材质、光源、色板和构图，不再用具体画师或工作室名称作新提示词锚点。不混入像素包、写实照片、现代科幻或明显 3D 渲染。

| 用途 | 色值（sRGB） |
| --- | --- |
| 松绿 / 苔绿 | #193A32 / #778458 |
| 羊皮纸 / 旧金 | #E7D7AE / #BE9650 |
| 暖石灰 / 灰陶红 | #C7BB99 / #A76D50 |
| UI 底色 / 面板 | #162520 / #1F362D |
| UI 按钮 / 悬停 | #2D4336 / #3F5743 |
| 正文 / 次级字 / 金线 | #F4E6C8 / #BDC1A4 / #D1B170 |

UI 统一入口为 `LKPresentationStyle.h`，色值先从 sRGB 转线性，避免整体发灰。品质白/绿/蓝/紫/橙仍是功能色，阵营、黄中线、非法落点和血条保留语义颜色，并配文字/形状。

## 插画、精灵与 UI 层级

- 菜单/卡面：细节较丰富的水粉插画，柔和上左暖光；中文、费用、品质框均由 UMG 绘制，不烘焙进图片。
- 战斗小精灵：同色板、栗棕轮廓，实际尺寸仍能看出职业与武器。单帧可回退，D 批已接入五态逐帧动画。当前相机仍正俯视，精灵铺在 XY 平面。
- 家园建筑：统一略高的正面三分之四插画视角，门朝画面下方，作为平面精灵面向现有相机；不因为图中屋顶可见就改斜视相机。
- UI：6px 小圆角、1px 旧金细线、宽松留白。共用面板/按钮用 Slate 原生绘制，保持缩放清晰，避免整张不可交互的生成 UI。
- 地图：地形美术与实际 DAG 分离；不在背景画固定假节点/路线，也不画没有碰撞的阻挡物。

B 批已补 20 个战斗单帧和 17 张同设计卡面，复核后保留 8 个既有精灵、7 张旧卡图。旧精灵更偏简洁卡通，暖色与轮廓方向一致；D 批以既有图作参考补齐多帧，保留这些角色身份。尺寸、真实画面和边界见 [38](38-BattleArtIntegration.md)。

C 批已补五区地面、三帐篷、五节点徽记及三 FX 零件；绘本战斗 HUD、短时状态图形与 28 个原创合成音同步接入，见 [40](40-WorldSkillsArtIntegration.md)。地面明度低于单位，战场固定曝光且关闭泛光；黄色中线、血条、范围圈不受地面氛围染色。地图纹理等比裁切，节点/连线仍由真实数据绘制。

## 交付规格

| 素材 | 规格与约束 |
| --- | --- |
| 菜单插画 | 首版 1536×1024 RGB，3:2，ScaleToFit，不拉伸 |
| 家园建筑 | 首版实际 1254×1254 RGBA；完整画布统一对应 500cm，中心枢轴，组件无碰撞 |
| 战斗单位/建筑 | B 批实际 1254×1254 RGBA，构建上限 512；完整画布高 230～410cm，保持等比，不能缩 Actor 改逻辑 |
| 卡图/头像 | B 批实际 1254×1254，构建上限 512；同设计重新构图，手牌等比显示，费用/品质/占格由 UI 绘制 |
| 场地 | C 批实际 1254×1254、构建上限 1024，运行时铺满规则场地；无阻挡性装饰，组件无碰撞 |
| 营地/节点 | C 批实际 1254×1254 RGBA、构建上限 512；营地完整画布高 220cm，徽记等比显示 |
| FX | C 批火球爆点/叶片/尘点为透明单张，1254×1254、构建上限 512；中心枢轴；原生时间变化，无序列帧 FPS |
| 字体 | D 批：正文/数字 Noto Sans CJK SC Regular，标题 Noto Serif CJK SC SemiBold；原始 OTF/OFL 已归档；Canvas 旧提示保留引擎回退 |
| 动画 | 每实体 4×4、16 姿势、5 动作；按真实 alpha 记录 SourceUV/尺寸；枢轴为整图像素坐标；无像素后处理 |

透明检查实际 alpha，禁止假棋盘背景。保存原图；改变外观另存版本。首版家园使用 MaskedUnlitSpriteMaterial、双线性过滤、无 mip、UI 贴图组。大范围相机缩放时再评估 mip 与内存，不把当前配置当作最终性能方案。

## 生成锚点与留档

```text
Use case: stylized-concept. Production art for Little King, a warm storybook fantasy strategy game.
Hand painted gouache with restrained watercolor grain, confident chestnut outlines,
simplified readable silhouettes, matte surfaces, gentle amber light from upper left.
Unified palette: deep pine green #193A32, moss #778458, parchment #E7D7AE,
antique gold #BE9650, warm limestone #C7BB99, muted terracotta #A76D50.
Elegant modest leaf and crown ornaments. No named artist, no existing franchise characters,
no text, no letters, no logo, no watermark, no photorealism, no 3D render, no pixel art.
```

每张实际完整提示词见 [A 批 prompts.json](../ArtSource/StorybookV1/Prompts/prompts.json)、[B 批单帧](../ArtSource/StorybookV1/Battle/catalog.json) 和 [B 批卡面](../ArtSource/StorybookV1/Battle/Prompts/cards.json)，尺寸、alpha、哈希见 [A 批图片清单](../ArtSource/StorybookV1/Images/manifest.json) 与 [B 批清单](../ArtSource/StorybookV1/Battle/manifest.json)。实际输出不是请求尺寸时按实际记录。使用内置 imagegen，没有额外 API 或第三方画作参考；B 批卡面仅参考本项目新生成的对应精灵。

C 批 16 张原图全部为纯文字生成，完整输入见 [41](41-WorldArtPrompts.md)，来源与实际参数见 [C 批清单](../ArtSource/StorybookV1/World/manifest.json)。保持原图不改像素；运行时着色、缩放和透明叠加属于显示处理。

## 声音方向

短、轻、有材质感：皮革/木头/柔和击打，系统提示使用短拨弦。不加入血腥惨叫、现代枪声、长混响或像素电子音。战斗声比操作信号更克制。

首批统一 48kHz、16-bit PCM、单声道，动作声约 0.01～0.6 秒、短句不超过 3 秒。按用途限制峰值：受击 -23dBFS、普攻 -17～-19dBFS、UI -20dBFS、结算 -14dBFS；这是峰值上限，不是 LUFS 响度认证。共享并发：战斗 12、UI 3、系统短句 2。

C 批新增 28 个专属发射/技能/状态/经济/节点音，用短拨弦、滤波噪声与柔和钟声程序合成，0.18～1.15 秒，峰值 -18～-28dBFS；不是实录弓弦或乐器录音。高频治疗、点燃、脚步按 World/ID 节流并共用并发预算。D 批已接四首本地 Music3 循环：提示词统一原声拨弦、木管、轻弦乐与手鼓；家园舒缓、远征推进、Boss 增强力度。实际长度 20.675～46.554 秒，44.1kHz/16bit/立体声，峰值不高于 -3dBFS，独立并发 2，1.2 秒切换淡化。见 [42](42-PolishArtIntegration.md)。循环接缝、重复疲劳与真实扬声器听感仍需单独试听。
