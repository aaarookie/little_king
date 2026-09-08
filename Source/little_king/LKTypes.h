#pragma once

#include "CoreMinimal.h"
#include "LKTypes.generated.h"

/** 阵营 */
UENUM(BlueprintType)
enum class ELKTeam : uint8
{
	Player	UMETA(DisplayName = "玩家"),
	Enemy	UMETA(DisplayName = "敌方")
};

/** 对局阶段 */
UENUM(BlueprintType)
enum class ELKGamePhase : uint8
{
	Deployment	UMETA(DisplayName = "部署"),
	Battle		UMETA(DisplayName = "战斗"),
	Result		UMETA(DisplayName = "结算")
};

/** 卡牌类型 */
UENUM(BlueprintType)
enum class ELKCardType : uint8
{
	Unit		UMETA(DisplayName = "角色"),
	Spell		UMETA(DisplayName = "法术"),
	Building	UMETA(DisplayName = "建筑")
};

/** 单位大类 */
UENUM(BlueprintType)
enum class ELKUnitClass : uint8
{
	Soldier		UMETA(DisplayName = "佣兵"),
	Hero		UMETA(DisplayName = "英雄"),
	Building	UMETA(DisplayName = "建筑"),
	Boss		UMETA(DisplayName = "首领")
};

/** 攻击类型 */
UENUM(BlueprintType)
enum class ELKAttackType : uint8
{
	Melee	UMETA(DisplayName = "近战"),
	Ranged	UMETA(DisplayName = "远程")
};

/** 建筑行为 */
UENUM(BlueprintType)
enum class ELKBuildingBehavior : uint8
{
	None		UMETA(DisplayName = "无"),
	Turret		UMETA(DisplayName = "哨塔（自动攻击）"),
	Barracks	UMETA(DisplayName = "兵营（周期出兵）")
};

/** 单位状态机 */
UENUM(BlueprintType)
enum class ELKUnitState : uint8
{
	Idle		UMETA(DisplayName = "待机"),
	Moving		UMETA(DisplayName = "移动"),
	Attacking	UMETA(DisplayName = "攻击"),
	Casting		UMETA(DisplayName = "施法"),
	Dead		UMETA(DisplayName = "死亡")
};

/** 打牌结果 */
UENUM(BlueprintType)
enum class ELKPlayResult : uint8
{
	Success				UMETA(DisplayName = "成功"),
	NotEnoughSilver		UMETA(DisplayName = "银币不足"),
	InvalidLocation		UMETA(DisplayName = "位置非法"),
	SpellLocked			UMETA(DisplayName = "无法在敌方半场施法（需要全场施法特性）"),
	HandEmpty			UMETA(DisplayName = "手牌无效"),
	WrongPhase			UMETA(DisplayName = "阶段错误"),
	HeroLimitReached	UMETA(DisplayName = "英雄数量已达上限"),
	AlreadyDeployed		UMETA(DisplayName = "该英雄已部署"),
	UnitLimitReached	UMETA(DisplayName = "单位数量已达上限"),
	Unknown				UMETA(DisplayName = "未知错误"),
	DeploymentIncomplete UMETA(DisplayName = "请先部署全部英雄"),
	InvalidCardData UMETA(DisplayName = "卡牌配置无效"),
	HeroMoveBlocked UMETA(DisplayName = "目标地点不可达（被阻挡或为建筑）")
};

/** 法术效果（原型期直接由 C++ 结算，后期替换为 GAS Ability） */
UENUM(BlueprintType)
enum class ELKSpellEffect : uint8
{
	None	UMETA(DisplayName = "无"),
	Damage	UMETA(DisplayName = "范围伤害"),
	Heal	UMETA(DisplayName = "范围治疗")
};

/** 放置模式（UI 交互状态机：点卡/英雄 -> 放置模式 -> 点战场 -> 结算） */
UENUM(BlueprintType)
enum class ELKPlacementMode : uint8
{
	None	UMETA(DisplayName = "无"),
	Card	UMETA(DisplayName = "出牌"),
	Hero	UMETA(DisplayName = "部署英雄"),
	HeroMove UMETA(DisplayName = "营地指挥")
};

/** 特性行为；数值修饰仍由 DT_Traits.Modifiers 提供。 */
UENUM(BlueprintType)
enum class ELKTraitEffect : uint8
{
	Attributes,
	Taunt,
	GlobalSpellPlacement,
	MeleeSoldierTauntAura,
    SummoningHealthCost,
    RangedDamageReduction
};

UENUM(BlueprintType)
enum class ELKPassiveAbility : uint8 { None, UndeadSummoning, GiantBones, BoneRegeneration };

/** 遭遇强度只描述内容与奖励层级；路线节点类型仍由 ELKDungeonNodeType 决定。 */
UENUM(BlueprintType)
enum class ELKEncounterRank : uint8
{
	Normal UMETA(DisplayName = "普通"),
	Elite UMETA(DisplayName = "精英"),
	Boss UMETA(DisplayName = "首领")
};

UENUM(BlueprintType)
enum class ELKCombatSourceKind : uint8 { Attack, Projectile, Spell, Skill, Overtime, HealthCost, Revival };

USTRUCT(BlueprintType)
struct FLKCombatSource
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) bool bHasTeam = false;
	UPROPERTY(BlueprintReadOnly) ELKTeam Team = ELKTeam::Player;
	UPROPERTY(BlueprintReadOnly) FName UnitId;
	UPROPERTY(BlueprintReadOnly) FName InstanceId;
	UPROPERTY(BlueprintReadOnly) FName ActionId;
	UPROPERTY(BlueprintReadOnly) ELKCombatSourceKind Kind = ELKCombatSourceKind::Attack;
	/** 发射/施法时快照；发射者失能或销毁后仍能判定远程减伤。 */
	UPROPERTY(BlueprintReadOnly) bool bRangedSource = false;
};

USTRUCT(BlueprintType)
struct FLKCombatEvent
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FLKCombatSource Source;
	UPROPERTY(BlueprintReadOnly) ELKTeam TargetTeam = ELKTeam::Player;
	UPROPERTY(BlueprintReadOnly) FName TargetUnitId;
	UPROPERTY(BlueprintReadOnly) FName TargetInstanceId;
	UPROPERTY(BlueprintReadOnly) FVector Location = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) float RequestedAmount = 0.f;
	UPROPERTY(BlueprintReadOnly) float ActualAmount = 0.f;
	UPROPERTY(BlueprintReadOnly) float HealthBefore = 0.f;
	UPROPERTY(BlueprintReadOnly) float HealthAfter = 0.f;
	UPROPERTY(BlueprintReadOnly) bool bIsHeal = false;
	UPROPERTY(BlueprintReadOnly) bool bKilled = false;
};

USTRUCT(BlueprintType)
struct FLKTeamMatchStats
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 Kills = 0;
	UPROPERTY(BlueprintReadOnly) int32 CardsPlayed = 0;
	UPROPERTY(BlueprintReadOnly) float Damage = 0.f;
	UPROPERTY(BlueprintReadOnly) float Healing = 0.f;
};

USTRUCT(BlueprintType)
struct FLKMatchStats
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FLKTeamMatchStats Player;
	UPROPERTY(BlueprintReadOnly) FLKTeamMatchStats Enemy;
	UPROPERTY(BlueprintReadOnly) float Duration = 0.f;
	UPROPERTY(BlueprintReadOnly) int32 Seed = 0;
	UPROPERTY(BlueprintReadOnly) bool bOvertime = false;
	UPROPERTY(BlueprintReadOnly) bool bSimultaneousElimination = false;
	UPROPERTY(BlueprintReadOnly) ELKTeam Winner = ELKTeam::Player;
};
