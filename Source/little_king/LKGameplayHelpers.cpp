#include "LKGameplayHelpers.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AttributeSet.h"
#include "GameplayEffect.h"
#include "ALKUnitBase.h"
#include "ULKUnitAttributeSet.h"
#include "LKLog.h"

namespace LKGameplay
{
	namespace
	{
		const FName DamageDataName = TEXT("LK.Damage");
		const FName HealDataName = TEXT("LK.Heal");

		/** 运行时 GE 模板缓存（按 DataName 缓存，避免每次伤害都 NewObject） */
		TMap<FName, TWeakObjectPtr<UGameplayEffect>>& GetGECache()
		{
			static TMap<FName, TWeakObjectPtr<UGameplayEffect>> Cache;
			return Cache;
		}

		/** 获取（或创建）一个"修正 Health 属性"的运行时 GE，数值走 SetByCaller */
		UGameplayEffect* GetOrCreateHealthGE(FName DataName)
		{
			if (TWeakObjectPtr<UGameplayEffect>* Cached = GetGECache().Find(DataName))
			{
				if (Cached->IsValid())
				{
					return Cached->Get();
				}
			}

			UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage(), FName(*FString::Printf(TEXT("LK_RuntimeGE_%s"), *DataName.ToString())));
			Effect->DurationPolicy = EGameplayEffectDurationType::Instant;

			FGameplayModifierInfo& Mod = Effect->Modifiers.AddDefaulted_GetRef();
			Mod.Attribute = ULKUnitAttributeSet::GetHealthAttribute();
			Mod.ModifierOp = EGameplayModOp::Additive;

			FSetByCallerFloat SetByCaller;
			SetByCaller.DataName = DataName;
			Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);

			GetGECache().Add(DataName, Effect);
			return Effect;
		}

		FGameplayEffectContextHandle MakeContext(UAbilitySystemComponent* ASC, AActor* Instigator)
		{
			FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
			if (Instigator)
			{
				Context.AddInstigator(Instigator, Instigator);
			}
			return Context;
		}

		void ApplyHealthDelta(AActor* Target, FName DataName, float Delta, AActor* Instigator)
		{
			UAbilitySystemComponent* ASC = GetASC(Target);
			if (!ASC || FMath::IsNearlyZero(Delta))
			{
				return;
			}

			UGameplayEffect* GE = GetOrCreateHealthGE(DataName);
			if (!GE)
			{
				UE_LOG(LogLK, Warning, TEXT("[LKGameplay] 创建运行时 GE 失败: %s"), *DataName.ToString());
				return;
			}

			FGameplayEffectSpec Spec(GE, MakeContext(ASC, Instigator), 1.f);
			Spec.SetSetByCallerMagnitude(DataName, Delta);
			ASC->ApplyGameplayEffectSpecToSelf(Spec);
		}
	}

	UAbilitySystemComponent* GetASC(const AActor* Actor)
	{
		if (!Actor)
		{
			return nullptr;
		}

		if (const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Actor))
		{
			return ASI->GetAbilitySystemComponent();
		}

		const UActorComponent* Comp = Actor->FindComponentByClass<UAbilitySystemComponent>();
		return const_cast<UAbilitySystemComponent*>(Cast<UAbilitySystemComponent>(Comp));
	}

	void ApplyDamage(AActor* Target, float Amount, AActor* Instigator, bool bBypassInvulnerability)
	{
		if (Amount <= 0.f)
		{
			return;
		}

		// 无敌拦截：普通伤害一律挡下；只有显式"真伤"（bBypassInvulnerability，超时虚弱用）能穿透。
		// 所有敌方伤害（近战/弹道/法术/技能）都汇聚到这里，所以在此拦截一处生效全局。
		if (!bBypassInvulnerability)
		{
			if (const ALKUnitBase* Unit = Cast<ALKUnitBase>(Target))
			{
				if (Unit->IsInvulnerable())
				{
					UE_LOG(LogLK, Verbose, TEXT("[LKGameplay] %s 处于无敌，免疫 %.1f 点伤害"),
						*Unit->GetUnitId().ToString(), Amount);
					return;
				}
			}
		}

		ApplyHealthDelta(Target, DamageDataName, -Amount, Instigator);
	}

	void ApplyHeal(AActor* Target, float Amount, AActor* Instigator)
	{
		if (Amount <= 0.f)
		{
			return;
		}
		ApplyHealthDelta(Target, HealDataName, Amount, Instigator);
	}

	void ApplyMaxHealthPercentDamage(AActor* Target, float Percent, AActor* Instigator)
	{
		UAbilitySystemComponent* ASC = GetASC(Target);
		if (!ASC || Percent <= 0.f)
		{
			return;
		}

		const float MaxHp = ASC->GetNumericAttribute(ULKUnitAttributeSet::GetMaxHealthAttribute());
		if (MaxHp > 0.f)
		{
			// 超时虚弱 = 真伤：穿透无敌（无敌单位也会被虚弱磨死，保证无平局）
			ApplyDamage(Target, MaxHp * Percent, Instigator, true);
		}
	}

	FActiveGameplayEffectHandle ApplyAttributeModifier(AActor* Target, const FGameplayAttribute& Attribute, float Value, float DurationSeconds, AActor* Instigator)
	{
		UAbilitySystemComponent* ASC = GetASC(Target);
		if (!ASC || !Attribute.IsValid())
		{
			return FActiveGameplayEffectHandle();
		}

		UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage(), TEXT("LK_RuntimeModifier"));
		if (DurationSeconds > 0.f)
		{
			Effect->DurationPolicy = EGameplayEffectDurationType::HasDuration;
			// 注：UE5.8 中 DurationMagnitude 类型为 FGameplayEffectModifierMagnitude
			Effect->DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(DurationSeconds));
		}
		else
		{
			Effect->DurationPolicy = EGameplayEffectDurationType::Infinite;
		}

		FGameplayModifierInfo& Mod = Effect->Modifiers.AddDefaulted_GetRef();
		Mod.Attribute = Attribute;
		Mod.ModifierOp = EGameplayModOp::Additive;
		Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Value));

		FGameplayEffectSpec Spec(Effect, MakeContext(ASC, Instigator), 1.f);
		return ASC->ApplyGameplayEffectSpecToSelf(Spec);
	}

	FGameplayAttribute FindAttributeByName(const FName& StatName)
	{
		if (StatName == TEXT("Health"))			{ return ULKUnitAttributeSet::GetHealthAttribute(); }
		if (StatName == TEXT("MaxHealth"))		{ return ULKUnitAttributeSet::GetMaxHealthAttribute(); }
		if (StatName == TEXT("MoveSpeed"))		{ return ULKUnitAttributeSet::GetMoveSpeedAttribute(); }
		if (StatName == TEXT("AttackRange"))	{ return ULKUnitAttributeSet::GetAttackRangeAttribute(); }
		if (StatName == TEXT("AttackDamage"))	{ return ULKUnitAttributeSet::GetAttackDamageAttribute(); }
		if (StatName == TEXT("AttackInterval"))	{ return ULKUnitAttributeSet::GetAttackIntervalAttribute(); }

		UE_LOG(LogLK, Warning, TEXT("[LKGameplay] 未知属性名 '%s'（可选：Health/MaxHealth/MoveSpeed/AttackRange/AttackDamage/AttackInterval）"),
			*StatName.ToString());
		return FGameplayAttribute();
	}

	float GetAttributeValue(const AActor* Actor, const FGameplayAttribute& Attribute, float Fallback)
	{
		UAbilitySystemComponent* ASC = GetASC(Actor);
		if (!ASC || !Attribute.IsValid())
		{
			return Fallback;
		}
		return ASC->GetNumericAttribute(Attribute);
	}

	float GetAttributeBaseValue(const AActor* Actor, const FGameplayAttribute& Attribute, float Fallback)
	{
		UAbilitySystemComponent* ASC = GetASC(Actor);
		if (!ASC || !Attribute.IsValid())
		{
			return Fallback;
		}
		return ASC->GetNumericAttributeBase(Attribute);
	}
}
