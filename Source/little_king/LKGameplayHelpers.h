#pragma once

#include "CoreMinimal.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class UAttributeSet;
struct FGameplayAttribute;
class AActor;

/**
 * GAS 便捷封装：运行时构造 GameplayEffect（无需美术/策划在编辑器建 GE 资产）。
 * 伤害/治疗数值一律走 SetByCaller（FName），统一数据流，便于未来增伤/减伤乘区。
 */
namespace LKGameplay
{
	/** 获取 Actor 的 ASC */
	UAbilitySystemComponent* GetASC(const AActor* Actor);

	/** 瞬时伤害（负数修正 Health） */
	void ApplyDamage(AActor* Target, float Amount, AActor* Instigator);

	/** 瞬时治疗 */
	void ApplyHeal(AActor* Target, float Amount, AActor* Instigator);

	/** 按最大生命百分比造成伤害（超时虚弱用） */
	void ApplyMaxHealthPercentDamage(AActor* Target, float Percent, AActor* Instigator);

	/** 属性修正（DurationSeconds <= 0 视为无限持续；支持 Buff/Debuff） */
	void ApplyAttributeModifier(AActor* Target, const FGameplayAttribute& Attribute, float Value, float DurationSeconds, AActor* Instigator);

	/** 读取属性 */
	float GetAttributeValue(const AActor* Actor, const FGameplayAttribute& Attribute, float Fallback = 0.f);
}
