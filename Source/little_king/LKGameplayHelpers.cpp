#include "LKGameplayHelpers.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AttributeSet.h"
#include "GameplayEffect.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "ALKUnitBase.h"
#include "ALKBattleGameMode.h"
#include "ULKGameData.h"
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

		/** S5：伤害/治疗事件 -> GameMode（HUD 飘字数据链） */

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

    FLKCombatSource MakeSource(AActor* Instigator, ELKCombatSourceKind Kind, FName ActionId)
    {
        FLKCombatSource Source;
        Source.Kind = Kind;
        Source.ActionId = ActionId;
        if (const ALKUnitBase* Unit = Cast<ALKUnitBase>(Instigator))
        {
            Source.bHasTeam = true; Source.Team = Unit->GetTeam();
            Source.UnitId = Unit->GetUnitId(); Source.InstanceId = Unit->GetFName();
        }
        return Source;
    }

    float ApplyDamage(AActor* Target, float Amount, AActor* Instigator, bool bBypassInvulnerability, const FLKCombatSource* SourceOverride)
    {
        ALKUnitBase* Unit = Cast<ALKUnitBase>(Target);
        if (!IsValid(Unit) || !Unit->IsTargetable() || !FMath::IsFinite(Amount) || Amount <= 0.f) { return 0.f; }
        ALKBattleGameMode* GM = Unit->GetWorld()->GetAuthGameMode<ALKBattleGameMode>();
        if (GM && GM->GetPhase() != ELKGamePhase::Battle) { return 0.f; }
        FLKCombatEvent Event;
        Event.Source = SourceOverride ? *SourceOverride : MakeSource(Instigator);
        Event.TargetTeam = Unit->GetTeam(); Event.TargetUnitId = Unit->GetUnitId(); Event.TargetInstanceId = Unit->GetFName();
        Event.Location = Unit->GetActorLocation(); Event.RequestedAmount = Amount; Event.HealthBefore = Unit->GetHealth();
        if (GM) { GM->BeginCombatBatch(); }
        if (bBypassInvulnerability || !Unit->IsInvulnerable()) { ApplyHealthDelta(Unit, DamageDataName, -Amount, Instigator); }
        Event.HealthAfter = IsValid(Unit) ? Unit->GetHealth() : 0.f;
        Event.ActualAmount = FMath::Clamp(Event.HealthBefore - Event.HealthAfter, 0.f, Event.HealthBefore);
        Event.bKilled = Event.HealthBefore > 0.f && Event.HealthAfter <= 0.f;
        if (Event.ActualAmount > 0.f && IsValid(Unit))
        {
            Unit->TriggerHitFlash();
            PlayOneShot(Unit->GetWorld(), GM ? GM->GetGameData() : nullptr, TEXT("HitTaken"), Event.Location, 0.5f);
        }
        if (GM) { GM->RecordCombatEvent(Event); GM->EndCombatBatch(); }
        return Event.ActualAmount;
    }

    float ApplyHeal(AActor* Target, float Amount, AActor* Instigator, const FLKCombatSource* SourceOverride)
    {
        ALKUnitBase* Unit = Cast<ALKUnitBase>(Target);
        if (!IsValid(Unit) || !Unit->IsTargetable() || !FMath::IsFinite(Amount) || Amount <= 0.f) { return 0.f; }
        ALKBattleGameMode* GM = Unit->GetWorld()->GetAuthGameMode<ALKBattleGameMode>();
        if (GM && GM->GetPhase() != ELKGamePhase::Battle) { return 0.f; }
        FLKCombatEvent Event;
        Event.Source = SourceOverride ? *SourceOverride : MakeSource(Instigator);
        Event.TargetTeam = Unit->GetTeam(); Event.TargetUnitId = Unit->GetUnitId(); Event.TargetInstanceId = Unit->GetFName();
        Event.Location = Unit->GetActorLocation(); Event.RequestedAmount = Amount;
        Event.bIsHeal = true; Event.HealthBefore = Unit->GetHealth();
        if (GM) { GM->BeginCombatBatch(); }
        ApplyHealthDelta(Unit, HealDataName, Amount, Instigator);
        Event.HealthAfter = IsValid(Unit) ? Unit->GetHealth() : Event.HealthBefore;
        Event.ActualAmount = FMath::Max(0.f, Event.HealthAfter - Event.HealthBefore);
        if (GM) { GM->RecordCombatEvent(Event); GM->EndCombatBatch(); }
        return Event.ActualAmount;
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
			FLKCombatSource Source = MakeSource(Instigator, ELKCombatSourceKind::Overtime, TEXT("Overtime"));
            ApplyDamage(Target, MaxHp * Percent, Instigator, true, &Source);
		}
	}

	FActiveGameplayEffectHandle ApplyAttributeModifier(AActor* Target, const FGameplayAttribute& Attribute, float Value, float DurationSeconds, AActor* Instigator)
	{
		UAbilitySystemComponent* ASC = GetASC(Target);
		if (!ASC || !Attribute.IsValid())
		{
			return FActiveGameplayEffectHandle();
		}

		UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage());
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

	void PlayOneShot(UWorld* World, const ULKGameData* GameData, FName SoundId, const FVector& Location, float VolumeScale)
	{
		if (!World || !GameData || SoundId.IsNone())
		{
			return;
		}

		const TSoftObjectPtr<USoundBase>* Found = GameData->SoundMap.Find(SoundId);
		if (!Found || Found->IsNull())
		{
			return; // 未配置的键静默跳过（不报错，教程 B 允许只配 3 个）
		}

		const ALKBattleGameMode* GM = World->GetAuthGameMode<ALKBattleGameMode>();
        USoundBase* Sound = GM ? GM->FindPreloadedSound(SoundId) : Found->Get();
		if (!Sound)
		{
			UE_LOG(LogLK, Warning, TEXT("[Sound] 键 '%s' 配置了但资源加载失败"), *SoundId.ToString());
			return;
		}

		UGameplayStatics::PlaySoundAtLocation(World, Sound, Location, VolumeScale);
	}
}
