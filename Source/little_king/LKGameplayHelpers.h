#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "LKTypes.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class UAttributeSet;
class ULKGameData;
struct FGameplayAttribute;
class AActor;

/**
 * GAS 便捷封装：运行时构造 GameplayEffect（无需美术/策划在编辑器建 GE 资产）。
 * 伤害/治疗数值一律走 SetByCaller（FName），统一数据流，便于未来增伤/减伤乘区。
 * S5：所有伤害/治疗出口统一转发"受击反馈 + 伤害事件（飘字数据链）+ 音效触发点"。
 */
namespace LKGameplay
{
	/** 获取 Actor 的 ASC */
	UAbilitySystemComponent* GetASC(const AActor* Actor);

	/** 瞬时伤害（负数修正 Health）。bBypassInvulnerability=true 时无视无敌状态（"真伤"，超时虚弱用） */
	float ApplyDamage(AActor* Target, float Amount, AActor* Instigator, bool bBypassInvulnerability = false, const FLKCombatSource* SourceOverride = nullptr);

	/** 瞬时治疗 */
	float ApplyHeal(AActor* Target, float Amount, AActor* Instigator, const FLKCombatSource* SourceOverride = nullptr);
	FLKCombatSource MakeSource(AActor* Instigator, ELKCombatSourceKind Kind = ELKCombatSourceKind::Attack, FName ActionId = NAME_None);

	/** 按最大生命百分比造成伤害（超时虚弱用） */
	void ApplyMaxHealthPercentDamage(AActor* Target, float Percent, AActor* Instigator);

	/**
	 * 属性修正（DurationSeconds <= 0 视为无限持续；支持 Buff/Debuff）。
	 * 返回施加的 GE handle——无限 buff 需要按 handle 移除（光环进出范围）时用。
	 */
	FActiveGameplayEffectHandle ApplyAttributeModifier(AActor* Target, const FGameplayAttribute& Attribute, float Value, float DurationSeconds, AActor* Instigator);

	/** 按 DT_Units/Traits 约定的 StatName 找属性（Health/MaxHealth/MoveSpeed/AttackRange/AttackDamage/AttackInterval）；未知返回无效属性 */
	FGameplayAttribute FindAttributeByName(const FName& StatName);

	/** 读取属性（最终值，含所有 modifier） */
	float GetAttributeValue(const AActor* Actor, const FGameplayAttribute& Attribute, float Fallback = 0.f);

	/** 读取属性基础值（不含 modifier；特性百分比加成按基础值算，多个加成线性叠加不滚雪球） */
	float GetAttributeBaseValue(const AActor* Actor, const FGameplayAttribute& Attribute, float Fallback = 0.f);

	/** 播放一次性音效（查 GameData.SoundMap；未配置静默跳过，不报错） */
	void PlayOneShot(UWorld* World, const ULKGameData* GameData, FName SoundId, const FVector& Location, float VolumeScale = 1.f);
}
