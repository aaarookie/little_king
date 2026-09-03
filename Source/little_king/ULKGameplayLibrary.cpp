#include "ULKGameplayLibrary.h"

#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"

#include "ALKUnitBase.h"
#include "LKGameplayHelpers.h"
#include "LKLog.h"
#include "ULKUnitAttributeSet.h"

namespace
{
	/** 半径内与单位碰撞体相交的 Actor 列表（ECC_Pawn 查询通道） */
	TArray<FOverlapResult> OverlapUnits(UWorld* World, const FVector& Center, float Radius, const AActor* Ignore)
	{
		TArray<FOverlapResult> Overlaps;
		if (!World || Radius <= 0.f)
		{
			return Overlaps;
		}

		FCollisionQueryParams Params;
		if (Ignore)
		{
			Params.AddIgnoredActor(Ignore);
		}
		World->OverlapMultiByChannel(Overlaps, Center, FQuat::Identity, ECC_Pawn,
			FCollisionShape::MakeSphere(Radius), Params);
		return Overlaps;
	}
}

int32 ULKGameplayLibrary::LK_ApplyDamageInRadius(AActor* CenterActor, float Radius, float Damage, AActor* Caster)
{
	if (!CenterActor || Damage <= 0.f)
	{
		return 0;
	}

	UWorld* World = CenterActor->GetWorld();
	if (!World)
	{
		return 0;
	}

	// 敌我判定：施法者是单位 -> 只打敌方；否则退化为"半径内除施法者外所有单位"，并告警提醒接线问题
	const ALKUnitBase* CasterUnit = Cast<ALKUnitBase>(Caster);
	if (!CasterUnit)
	{
		UE_LOG(LogLK, Warning,
			TEXT("[Skill] 施法者不是单位，范围伤害将作用于半径内所有单位（检查技能蓝图的 Caster 引脚）"));
	}

	int32 HitCount = 0;
	for (const FOverlapResult& Overlap : OverlapUnits(World, CenterActor->GetActorLocation(), Radius, Caster))
	{
		ALKUnitBase* Unit = Cast<ALKUnitBase>(Overlap.GetActor());
		if (!Unit || Unit->IsDead())
		{
			continue;
		}

		if (CasterUnit && Unit->GetTeam() == CasterUnit->GetTeam())
		{
			continue; // 不打友军
		}

		LKGameplay::ApplyDamage(Unit, Damage, Caster);
		++HitCount;
	}

	if (HitCount > 0)
	{
		UE_LOG(LogLKUnit, Log, TEXT("[Skill] 范围伤害 %s：半径 %.0f 命中 %d 个单位"),
			*CenterActor->GetName(), Radius, HitCount);
	}
	return HitCount;
}

int32 ULKGameplayLibrary::LK_ApplyHealInRadius(AActor* CenterActor, float Radius, float HealAmount, AActor* Caster)
{
	if (!CenterActor || HealAmount <= 0.f)
	{
		return 0;
	}

	UWorld* World = CenterActor->GetWorld();
	if (!World)
	{
		return 0;
	}

	const ALKUnitBase* CasterUnit = Cast<ALKUnitBase>(Caster);
	if (!CasterUnit)
	{
		UE_LOG(LogLK, Warning,
			TEXT("[Skill] 施法者不是单位，无法判断友军，治疗不生效（检查技能蓝图的 Caster 引脚）"));
		return 0;
	}

	int32 HitCount = 0;
	// 不忽略 Caster：治疗包含施法者自己（骑士鼓舞可以自奶）
	for (const FOverlapResult& Overlap : OverlapUnits(World, CenterActor->GetActorLocation(), Radius, nullptr))
	{
		ALKUnitBase* Unit = Cast<ALKUnitBase>(Overlap.GetActor());
		if (!Unit || Unit->IsDead() || Unit->GetTeam() != CasterUnit->GetTeam())
		{
			continue;
		}

		LKGameplay::ApplyHeal(Unit, HealAmount, Caster);
		++HitCount;
	}

	if (HitCount > 0)
	{
		UE_LOG(LogLKUnit, Log, TEXT("[Skill] 范围治疗 %s：半径 %.0f 治疗 %d 个单位"),
			*CenterActor->GetName(), Radius, HitCount);
	}
	return HitCount;
}

AActor* ULKGameplayLibrary::LK_GetNearestEnemy(AActor* Unit)
{
	ALKUnitBase* Self = Cast<ALKUnitBase>(Unit);
	return Self ? Self->FindNearestEnemy() : nullptr;
}

float ULKGameplayLibrary::LK_GetUnitHealth(AActor* Unit)
{
	return LKGameplay::GetAttributeValue(Unit, ULKUnitAttributeSet::GetHealthAttribute(), 0.f);
}

float ULKGameplayLibrary::LK_GetUnitMaxHealth(AActor* Unit)
{
	return LKGameplay::GetAttributeValue(Unit, ULKUnitAttributeSet::GetMaxHealthAttribute(), 0.f);
}
