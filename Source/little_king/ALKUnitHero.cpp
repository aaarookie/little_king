#include "ALKUnitHero.h"

#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"
#include "GameplayTagContainer.h"

#include "LKLog.h"

ALKUnitHero::ALKUnitHero()
{
	// 英雄默认不做额外初始化；属性由数据表行驱动
}

void ALKUnitHero::BeginPlay()
{
	Super::BeginPlay();

	// 授予技能
	if (AbilitySystem)
	{
		for (const TSubclassOf<UGameplayAbility>& AbilityClass : Abilities)
		{
			if (AbilityClass)
			{
				FGameplayAbilitySpec Spec(AbilityClass, 1, INDEX_NONE, this);
				AbilitySystem->GiveAbility(Spec);
				UE_LOG(LogLKUnit, Log, TEXT("[Hero] %s 授予技能 %s"), *UnitId.ToString(), *AbilityClass->GetName());
			}
		}
	}
}

void ALKUnitHero::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (IsDead() || !IsCombatEnabled())
	{
		return;
	}

	AbilityCheckTimer -= DeltaSeconds;
	if (AbilityCheckTimer <= 0.f)
	{
		AbilityCheckTimer = AbilityCheckInterval;
		TryCastAbilities();
	}
}

void ALKUnitHero::TryCastAbilities()
{
	if (!AbilitySystem)
	{
		return;
	}

	// 原型规则：自动激活带 LK.Ability 标签且未在冷却中的技能
	// （S3 起：技能由 DT_Skills 驱动，配合 CooldownGameplayEffect 控制节奏）
	FGameplayTagContainer AbilityTags;
	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("LK.Ability"), false));
	if (!AbilityTags.IsEmpty())
	{
		AbilitySystem->TryActivateAbilitiesByTag(AbilityTags);
	}
}
