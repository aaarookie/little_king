#include "LKGameplayHelpers.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AttributeSet.h"
#include "GameplayEffect.h"
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

	void ApplyDamage(AActor* Target, float Amount, AActor* Instigator)
	{
		if (Amount <= 0.f)
		{
			return;
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
			ApplyDamage(Target, MaxHp * Percent, Instigator);
		}
	}

	void ApplyAttributeModifier(AActor* Target, const FGameplayAttribute& Attribute, float Value, float DurationSeconds, AActor* Instigator)
	{
		UAbilitySystemComponent* ASC = GetASC(Target);
		if (!ASC || !Attribute.IsValid())
		{
			return;
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
		ASC->ApplyGameplayEffectSpecToSelf(Spec);
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
}
