#pragma once

#include "CoreMinimal.h"
#include "ALKUnitBase.h"
#include "ALKUnitHero.generated.h"

class UGameplayAbility;

/**
 * 英雄单位：高属性 + 技能槽（GAS Ability）+ 特性（光环/羁绊）。
 * 英雄是胜负判定的核心：一方英雄全灭即败。
 */
UCLASS()
class ALKUnitHero : public ALKUnitBase
{
	GENERATED_BODY()

public:
	ALKUnitHero();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** 授予的技能（S3 起由 DT_Skills 驱动，当前可在 BP 子类中配置） */
	UPROPERTY(EditDefaultsOnly, Category = "LK|Hero")
	TArray<TSubclassOf<UGameplayAbility>> Abilities;

	/** 施放技能（原型：满足冷却时激活带 LK.Ability 标签的技能） */
	void TryCastAbilities();

protected:
	float AbilityCheckTimer = 0.f;
	float AbilityCheckInterval = 0.5f;
};
