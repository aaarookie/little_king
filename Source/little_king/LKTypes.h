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
	Building	UMETA(DisplayName = "建筑")
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
	SpellLocked			UMETA(DisplayName = "法术未解锁（需要法师英雄）"),
	HandEmpty			UMETA(DisplayName = "手牌无效"),
	WrongPhase			UMETA(DisplayName = "阶段错误"),
	HeroLimitReached	UMETA(DisplayName = "英雄数量已达上限"),
	AlreadyDeployed		UMETA(DisplayName = "该英雄已部署"),
	Unknown				UMETA(DisplayName = "未知错误")
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
	Hero	UMETA(DisplayName = "部署英雄")
};
