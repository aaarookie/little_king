# 当前素材需求与制作清单

更新：2026-09-21。根据 v0.7 的家园、开始界面、五区域远征和全部新卡重新盘点，替代旧 Sprint 5～6 清单。A/B/C/D 批已完成；打包与硬件验收仍后置。风格按 [07](07-ArtStyleGuide.md)，来源与提示词见 [36](36-ArtAudioSources.md)，操作见 [13](13_Asset_Solutions.md)。

## 总量与制作批次

| 类别 | 当前需要 | 现状与安排 |
| --- | --- | --- |
| 战斗实体 | 28 种单位/建筑 | 28 种单帧完整；D 批每种新增五态、16 姿势，共 448 帧 |
| 卡牌 | 24 张 | 原七张保留，B 批另生成并绑定 17 张；共 24 张 |
| 角色档案 | 玩家英雄 3、敌方英雄 2、Boss 1 | 三玩家英雄已有插画；敌方随单位制作；巨魔王属于佣兵 |
| 家园 | 7 建筑、地面、道路、摆件 | A 批制作七透明单帧，地面与路先统一颜色；纹理/摆件后补 |
| 菜单/UI | 菜单插画、按钮/面板/卡框、品质/等级、资源/状态图标、指针 | A/B 主题扩展到战斗 HUD；C 批增加状态图形、节点反馈和升级叶环；专用指针/角花后续细化 |
| 地图 | 5 区底纹，普通/精英/Boss/市场/休息 5 类节点，方向/可达/出入口标记 | C 批已接入五纹理/五徽记与入口出口文字，真实 DAG 不变 |
| 战场 | 5 区地面变体、精英/Boss 氛围、3 英雄营地 | C 批完成地面、固定曝光、场外暗底、三帐篷；营地不可攻击、有体积、无血条 |
| 技能/状态 FX | 下表各类 | C 批三张透明零件与原生短时效果已接入；不是角色多帧动画 |
| 音效 | 8 原战斗键、UI、技能/经济/节点细分 | A 批 9 条 + C 批 28 条，共 37 WAV；GameData 36 个默认键及独立按钮声 |
| 音乐 | 菜单/家园共用、远征、战斗、Boss 各 1 条循环 | D 批四首本地 Music3 循环已接入，约 21～47 秒 |
| 字体 | 中文正文、标题、数字 | D 批 Noto Sans/Serif CJK SC，OTF 与 OFL 许可已入库 |

“已有”只表示原型阶段资源存在；“本批接入”须同时有引擎资产、代码绑定和实际画面。单帧、卡面、首版 FX 与战场环境已接入；D 批多帧、循环音乐、等级图、转场、字体已接入。

## 战斗实体逐项盘点

下表保留单帧来源记录；全部 28 个实体另已完成 D 批待机/移动/攻击/受击/退场共 16 姿势，运行时默认切换多帧，自定义精灵保留。新单位卡名、费用、品质、占格由 UI 绘制，不能烘焙进图。

| 稳定 ID | 中文 | 当前状态 |
| --- | --- | --- |
| Hero_Knight | 骑士 | 既有精灵/插画保留，B 批已复核 |
| Hero_Mage | 法师 | 既有精灵/插画保留，B 批已复核 |
| Hero_Ranger | 游侠 | 既有精灵/插画保留，B 批已复核 |
| Unit_Swordsman | 剑士 | 既有精灵/插画保留，B 批已复核 |
| Unit_Archer | 弓箭手 | 既有精灵/插画保留，B 批已复核 |
| Unit_Shieldbearer | 盾卫 | 既有精灵/插画保留，B 批已复核 |
| Building_ArrowTower | 箭塔 | 既有精灵/插画保留，B 批已复核 |
| Building_Barracks | 兵营 | 既有图保留，B 批修正纵向拉伸 |
| Unit_Skeleton | 骷髅兵 | B 批单帧已接入 |
| Unit_SkeletonArcher | 骷髅射手 | B 批单帧已接入 |
| Hero_Necromancer | 死灵法师 | B 批单帧已接入 |
| Hero_SkeletonGiant | 骷髅巨人 | B 批单帧已接入 |
| Boss_SkeletonKing | 骷髅王 | B 批单帧已接入 |
| Unit_ElfArcher | 精灵弓箭手 | B 批单帧已接入 |
| Unit_ElfWarrior | 精灵战士 | B 批单帧已接入 |
| Unit_ElfGuard | 精灵盾卫 | B 批单帧已接入 |
| Unit_ElfPriest | 精灵牧师 | B 批单帧已接入 |
| Unit_GoblinRogue | 哥布林大盗 | B 批单帧已接入 |
| Unit_Thief | 窃贼 | B 批单帧已接入 |
| Unit_ApprenticeMage | 学徒法师 | B 批单帧已接入 |
| Unit_GoblinBlade | 开道利刃 | B 批单帧已接入 |
| Unit_TrollWarrior | 巨魔战士 | B 批单帧已接入 |
| Unit_TrollSpearman | 巨魔投矛手 | B 批单帧已接入 |
| Unit_TrollMage | 巨魔法师 | B 批单帧已接入 |
| Unit_TrollKing | 巨魔王 | B 批单帧已接入 |
| Unit_TwoHeadedDragon | 双头龙 | B 批单帧已接入 |
| Building_SiegeCatapult | 攻城投石炮 | B 批单帧已接入 |
| Unit_Colossus | 擎天巨像 | B 批单帧已接入 |

两张法术 `Spell_Fireball` / `Spell_HealWave` 保留原型卡面，C 批已补透明爆点/叶片及范围环。敌方英雄/Boss 不需要虚构成可获得卡牌。战斗兵营与家园军营属于不同用途，不直接混用碰撞和尺寸。

## 家园与 UI

| 稳定 ID / 页面 | A 批内容 | 后续增强 |
| --- | --- | --- |
| Home_StatueSaintMaria | 圣玛丽亚神像/神龛；C 批已加升级叶环 | D 批 Lv2～4 外观已接入，Lv1 保留 |
| Home_Library | 书徽圆塔图书馆 | 未来解锁/强化表现 |
| Home_Gate | 开放的双塔大门 | D 批统一转场已接入 |
| Home_HeroHouse | 盾徽英雄之家 | 培养功能后再扩展 |
| Home_Treasury | 钱币徽记金库；C 批已加升级叶环与短声 | D 批 Lv2～5 外观已接入，Lv1 保留 |
| Home_Barracks | 交叉剑盾军营 | 收藏头像 |
| Home_WarRoom | 地图桌战备帐篷 | 配队图标 |
| 开始、读取、删除 | 新插画、同主题按钮/面板 | D 批 Noto 标题、转场已接入 |
| 家园、奖励、替换、地图、恢复 | 松绿底、羊皮纸字、旧金线 | 角花、状态/资源图标 |
| 战斗 HUD | B 批卡面等比显示、文字分区；C 批统一部署/技能按钮及面板主题，保留原蓝图事件 | D 批角色五态、中文字体与界面动效已接入 |

七座建筑外观不改变原有点击半径、名称、升级/战备逻辑。家园主图中屋顶可见是插画透视，游戏相机仍为正俯视。

## FX 与声音覆盖

| 行为 | 需要的图形/FX | 声音状态 |
| --- | --- | --- |
| 普攻、发射、受击、死亡 | 已接小斩弧、箭/矛拖线、尘点，现有闪白保留 | A 批近战/受击/退场；C 批 BowShot / SpearShot / MagicShot（原创合成） |
| 出牌、开战、胜负、按钮 | 按下/落点圈、短转场 | CardPlay / BattleStart / Victory / Defeat / UIClick：A 批 |
| 火球/法师技能 | 已接爆点、范围环、短施法拖线 | FireballCast / FireballImpact；仅火球产生既有小震动 |
| 治疗/精灵自疗/共沐春色 | 已接绿金叶点、短治疗连线 | Heal，最短间隔 0.30 秒 |
| 嘲讽/集火/营地指令 | 已接盾标、目标环、落点旗 | 共用 Empower / CampCommand；嘲讽优先级不变 |
| 亡灵召唤/献祭/巨骨/复活 | 已接尘点、幽绿聚合、献祭环、复活环；保留计数文字 | Summon / Sacrifice / Heal / Revive；英雄失能无震动 |
| 背刺/战利品/模仿/冲刺 | 已接路径、钱币圈、符环、短拖线 | Backstab / Coin / Mimic / Dash；银币实际增加才提示 |
| 巨魔强化/第六击 | 已接绿金叶环、重击短弧 | Empower / HeavyHit |
| 眩晕/冰冻/点燃 | 已接星标、薄冰、带层数火点 | Stun / Freeze / Burn，现有状态文字保留 |
| 双头龙/攻城炮/巨像 | 已接冰火弹、石弹尘圈，巨像实际移动触发脚步 | IceBreath / FireBreath / SiegeShot / SiegeImpact / Footstep |
| 节点/休息/市场/升级 | 已接五类徽记、短脉动、状态强调、升级叶环 | NodeEnter / Rest / Market / Upgrade；市场商品尚未设计 |

## 优先级与验收

1. **A（已完成）：家园与声音基础。** 菜单插画、七建筑、共用主题、九音效、自动导入、来源台账与真实截图。
2. **B（已完成）：战斗识别。** 五种亡灵、八种精灵/人类/哥布林、七种巨魔/攻城，20 单帧和 17 卡图已接入，8 个旧精灵已复核；手牌与奖励页同步调整。详见 [38](38-BattleArtIntegration.md)。
3. **C（已完成）：世界与技能。** 16 张原图、28 声音、52 引擎资产；五区域场地、三营地、节点图标、技能 FX/专属音、战斗 HUD 主题。详见 [40](40-WorldSkillsArtIntegration.md) / [完整提示词 41](41-WorldArtPrompts.md)。
4. **D（已完成）：细化。** 28 动画图集、四首 Music3 循环、七等级图、转场与 Noto 中文字体，共 639 引擎资源。见 [42](42-PolishArtIntegration.md) / [43 提示词](43-PolishArtPrompts.md)。

验收看真实 alpha、等比显示、单位小尺寸识别、图片不挡点击、720p/1080p 文案、声音无削波/无意外循环/并发限制、来源可追溯、原玩家存档不变。实际改动和验证见 [A 批 37](37-ArtAudioIntegration.md) / [B 批 38](38-BattleArtIntegration.md) / [C 批 40](40-WorldSkillsArtIntegration.md) / [D 批 42](42-PolishArtIntegration.md)。原始合成声的主观听感和最终混音仍需后续试听，不计为硬件验收已完成。
