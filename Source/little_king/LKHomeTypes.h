#pragma once

#include "CoreMinimal.h"
#include "LKRunTypes.h"
#include "LKHomeTypes.generated.h"

/**
 * 阶段 3 家园（H0~H5）数据契约。
 *
 * 三层数据严格分离（见 docs/26-HomeDevelopmentPlan.md 第 1 节）：
 *  - 永久进度：FLKProfileState（金币、建筑等级、永久解锁、已保存战备、已处理事务）
 *  - 本次远征：FLKRunState 新增字段（区域、出征快照、加成快照、金币结算回执，见 LKRunTypes.h）
 *  - 当前战斗：银币/手牌/场上单位，每房重建（不进入本文件）
 *
 * 本文件放家园专属类型：建筑/区域定义、永久档、面板模型；默认数值与校验在 LKHomeContent，
 * 读写与事务在 ULKProfileSubsystem。
 */

/** 家园七座建筑（稳定枚举；存档用 FName 稳定 ID，不随中文名改变） */
UENUM(BlueprintType)
enum class ELKHomeBuilding : uint8
{
	Statue		UMETA(DisplayName = "圣玛丽亚神像"),
	Library		UMETA(DisplayName = "图书馆"),
	Gate		UMETA(DisplayName = "大门"),
	HeroHouse	UMETA(DisplayName = "英雄之家"),
	Treasury	UMETA(DisplayName = "金库"),
	Barracks	UMETA(DisplayName = "军营"),
	WarRoom		UMETA(DisplayName = "战备处")
};

/** 当前打开的建筑面板（None = 场景浏览） */
UENUM(BlueprintType)
enum class ELKHomePanel : uint8
{
	None		UMETA(DisplayName = "无"),
	Statue		UMETA(DisplayName = "圣玛丽亚神像"),
	Library		UMETA(DisplayName = "图书馆"),
	Gate		UMETA(DisplayName = "大门"),
	HeroHouse	UMETA(DisplayName = "英雄之家"),
	Treasury	UMETA(DisplayName = "金库"),
	Barracks	UMETA(DisplayName = "军营"),
	WarRoom		UMETA(DisplayName = "战备处")
};

/** 建筑升级事务结果（单次提交：校验 → 扣费+升级 → 写盘 → 发布） */
UENUM(BlueprintType)
enum class ELKUpgradeResult : uint8
{
	Success				UMETA(DisplayName = "升级成功"),
	NoProfile			UMETA(DisplayName = "永久档不可用"),
	UnknownBuilding		UMETA(DisplayName = "未知建筑"),
	NotUpgradable		UMETA(DisplayName = "该建筑不可升级"),
	MaxLevel			UMETA(DisplayName = "已满级"),
	LevelMismatch		UMETA(DisplayName = "等级已变化，请刷新"),
	InsufficientGold	UMETA(DisplayName = "金币不足"),
	SaveFailed			UMETA(DisplayName = "存档写入失败，未扣费")
};

/** 战备保存校验结果 */
UENUM(BlueprintType)
enum class ELKLoadoutResult : uint8
{
	Success				UMETA(DisplayName = "已保存"),
	NoProfile			UMETA(DisplayName = "永久档不可用"),
	WrongHeroCount		UMETA(DisplayName = "必须恰好三名英雄"),
	DuplicateHero		UMETA(DisplayName = "英雄重复"),
	LockedHero			UMETA(DisplayName = "英雄未永久解锁"),
	UnknownHero			UMETA(DisplayName = "英雄不存在"),
	DeckTooSmall		UMETA(DisplayName = "牌组不足五张"),
	DeckTooLarge		UMETA(DisplayName = "牌组超过起始上限七张"),
	DuplicateCard		UMETA(DisplayName = "卡牌重复"),
	LockedCard			UMETA(DisplayName = "卡牌未永久解锁"),
	UnknownCard			UMETA(DisplayName = "卡牌定义缺失"),
	SaveFailed			UMETA(DisplayName = "存档写入失败，配置未保存")
};

/** 金币入账（远征终态交接）结果 */
UENUM(BlueprintType)
enum class ELKSettlementResult : uint8
{
	Success				UMETA(DisplayName = "已入账"),
	AlreadyApplied		UMETA(DisplayName = "该结算已入账"),
	NoProfile			UMETA(DisplayName = "永久档不可用"),
	NoPendingSettlement	UMETA(DisplayName = "没有待结算远征"),
	IdentityMismatch	UMETA(DisplayName = "远征与永久档身份不匹配"),
	SaveFailed			UMETA(DisplayName = "存档写入失败，未入账")
};

/** 出征队伍 + 初始牌组见 LKRunTypes.h（FLKExpeditionLoadout，属于远征契约） */

/** 区域定义（首版一个真实区域，复用 D4 路线生成） */
USTRUCT(BlueprintType)
struct FLKRegionDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName RegionId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText EnemyTheme;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText RouteSummary;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText RewardSummary;
	/** 战斗地图名（首版 L_BattleTest） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName MapName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bEnabled = true;
};

/** 建筑定义（代码拥有用途与等级上限；费用与效果数值见 LKHomeContent） */
USTRUCT(BlueprintType)
struct FLKBuildingDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName BuildingId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bUpgradable = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MaxLevel = 1;
	/** 升到下一级的金币费用：索引 0 = 1→2；长度 = MaxLevel - 1 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<int32> UpgradeCosts;
};

/** 家园永久档（独立槽 LittleKing_Profile；不保存 Widget/Actor/World/ASC/手牌） */
USTRUCT(BlueprintType)
struct FLKProfileState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 SchemaVersion = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FGuid ProfileId;
	/** 每次成功写入 +1；A/B 双槽按最大有效修订选择 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Revision = 0;
	/** 家园金币（升级货币；与战斗银币完全独立） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Gold = 0;
	/** 出发资金暂存凭据：先从家园扣除，再写 Run；断电后的孤立凭据可退款。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FGuid FundedRunId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 FundedRunGold = 0;
	/** 建筑稳定 ID -> 等级（缺失视为 1 级） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FName, int32> BuildingLevels;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> UnlockedHeroIds;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> UnlockedCardIds;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> UnlockedRegionIds;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FLKExpeditionLoadout SavedLoadout;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName LastSelectedRegionId;
	/** 已入账的结算回执 ID（幂等：同一 SettlementId 只入账一次） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FGuid> ProcessedSettlementIds;
};

/** 远征终态的金币结算回执见 LKRunTypes.h（FLKSettlementReceipt，冻结在 Run 档） */

/** 面板行模型（原生 UI 只渲染，不含业务规则） */
USTRUCT(BlueprintType)
struct FLKHomePanelRow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FText Label;
	UPROPERTY(BlueprintReadOnly) FText Value;
	/** 行标识（卡牌 ID / 区域 ID / 建筑 ID；只读页用于选中详情） */
	UPROPERTY(BlueprintReadOnly) FName RowId;
	/** 可选中的行（只读页点击查看详情） */
	UPROPERTY(BlueprintReadOnly) bool bSelectable = false;
	/** 可勾选的行（战备处草稿） */
	UPROPERTY(BlueprintReadOnly) bool bToggleable = false;
	UPROPERTY(BlueprintReadOnly) bool bChecked = false;
	UPROPERTY(BlueprintReadOnly) bool bEnabled = true;
	UPROPERTY(BlueprintReadOnly) FText Detail;
	/** UI-only identifier for a draft hero slot. Candidate rows leave this at INDEX_NONE. */
	UPROPERTY(BlueprintReadOnly) int32 HeroSlotIndex = INDEX_NONE;
};

/** 面板按钮模型 */
USTRUCT(BlueprintType)
struct FLKHomePanelAction
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FText Label;
	UPROPERTY(BlueprintReadOnly) FName ActionId;
	UPROPERTY(BlueprintReadOnly) bool bEnabled = true;
};

/** 整张面板的数据（GameMode 生成，原生 UI 渲染；蓝图子类可换肤） */
USTRUCT(BlueprintType)
struct FLKHomePanelModel
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) ELKHomePanel Panel = ELKHomePanel::None;
	UPROPERTY(BlueprintReadOnly) FText Title;
	UPROPERTY(BlueprintReadOnly) FText Subtitle;
	UPROPERTY(BlueprintReadOnly) FText Status;
	UPROPERTY(BlueprintReadOnly) TArray<FLKHomePanelRow> Rows;
	UPROPERTY(BlueprintReadOnly) TArray<FLKHomePanelAction> Actions;
};

/** 出征启动结果 */
UENUM(BlueprintType)
enum class ELKExpeditionStartResult : uint8
{
	Success				UMETA(DisplayName = "已出征"),
	NoProfile			UMETA(DisplayName = "永久档不可用"),
	NoLoadout			UMETA(DisplayName = "战备无效，请先到战备处保存"),
	UnknownRegion		UMETA(DisplayName = "未知区域"),
	RegionLocked		UMETA(DisplayName = "区域未解锁"),
	RunInProgress		UMETA(DisplayName = "已有远征进行中，请先继续或结束它"),
	SaveFailed			UMETA(DisplayName = "存档写入失败，未出征"),
	MapLoadFailed		UMETA(DisplayName = "地图加载失败，可重试")
};
