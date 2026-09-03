#pragma once

#include "CoreMinimal.h"
#include "ALKUnitBase.h"
#include "ALKUnitHero.generated.h"

class UGameplayAbility;

/**
 * 英雄单位：高属性 + 技能槽（GAS Ability）+ 特性（光环/羁绊）。
 * 英雄是胜负判定的核心：一方英雄全灭即败。
 *
 * 技能授予（S3，数据驱动）：
 *  1) 类默认 Abilities（BP 子类可配，预留）优先；
 *  2) 为空则查 ULKGameData::HeroAbilityMap[UnitId]（DA_GameData 里配置，原型主路径）。
 * 施放节奏：战斗开启后每 0.5s 尝试激活带 LK.Ability 标签的技能，激活成功进入"简单冷却"
 * （一个计时字段，不建 GAS 冷却 GE 资产；S4 内容阶段再正规化）。
 */
UCLASS()
class ALKUnitHero : public ALKUnitBase
{
	GENERATED_BODY()

public:
	ALKUnitHero();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** 直接配置的技能（BP 子类可配；为空时改用数据资产 HeroAbilityMap） */
	UPROPERTY(EditDefaultsOnly, Category = "LK|Hero")
	TArray<TSubclassOf<UGameplayAbility>> Abilities;

	/** 技能冷却（秒）；DT_Units 行 SkillCooldown > 0 时以行值为准 */
	UPROPERTY(EditDefaultsOnly, Category = "LK|Hero", meta = (ClampMin = "0.1"))
	float SkillCooldownSeconds = 5.f;

	/** 施放技能（由 Tick 周期调用；冷却结束且激活成功时进入冷却） */
	void TryCastAbilities();

protected:
	virtual void OnUnitInitialized(const FLKUnitRow& Row) override;

	float AbilityCheckTimer = 0.f;
	float AbilityCheckInterval = 0.5f;
	float SkillCooldownRemaining = 0.f;
	bool bAbilitiesResolved = false;
};
