#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "ULKUnitAttributeSet.generated.h"

/**
 * 单位属性集（GAS）。
 * 所有战斗属性走 GAS，未来增伤/减伤/暴击等 Buff 全部通过 GameplayEffect 统一生效。
 */
UCLASS()
class ULKUnitAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	ATTRIBUTE_ACCESSORS_BASIC(ULKUnitAttributeSet, Health)
	ATTRIBUTE_ACCESSORS_BASIC(ULKUnitAttributeSet, MaxHealth)
	ATTRIBUTE_ACCESSORS_BASIC(ULKUnitAttributeSet, MoveSpeed)
	ATTRIBUTE_ACCESSORS_BASIC(ULKUnitAttributeSet, AttackRange)
	ATTRIBUTE_ACCESSORS_BASIC(ULKUnitAttributeSet, AttackDamage)
	ATTRIBUTE_ACCESSORS_BASIC(ULKUnitAttributeSet, AttackInterval)

	UPROPERTY(BlueprintReadOnly, Category = "LK|Attributes")
	FGameplayAttributeData Health;

	UPROPERTY(BlueprintReadOnly, Category = "LK|Attributes")
	FGameplayAttributeData MaxHealth;

	UPROPERTY(BlueprintReadOnly, Category = "LK|Attributes")
	FGameplayAttributeData MoveSpeed;

	UPROPERTY(BlueprintReadOnly, Category = "LK|Attributes")
	FGameplayAttributeData AttackRange;

	UPROPERTY(BlueprintReadOnly, Category = "LK|Attributes")
	FGameplayAttributeData AttackDamage;

	UPROPERTY(BlueprintReadOnly, Category = "LK|Attributes")
	FGameplayAttributeData AttackInterval;

	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;
};
