#include "ULKTraitAuraComponent.h"

#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "Engine/World.h"
#include "EngineUtils.h"

#include "ALKUnitBase.h"
#include "LKGameplayHelpers.h"
#include "LKLog.h"

ULKTraitAuraComponent::ULKTraitAuraComponent()
{
	PrimaryComponentTick.bCanEverTick = false; // 由所属单位驱动，不自己 Tick
}

void ULKTraitAuraComponent::AddAuraModifier(const FLKTraitModifier& Mod, float Radius)
{
	if (Radius <= 0.f || FMath::IsNearlyZero(Mod.Value))
	{
		return;
	}

	FLKAuraGroup Group;
	Group.Modifier = Mod;
	Group.Radius = Radius;
	Groups.Add(Group);

	UE_LOG(LogLKUnit, Log, TEXT("[Trait] %s 光环：半径 %.0f 内友军 %s %+.0f%%（按基础值）"),
		*GetOwner()->GetName(), Radius, *Mod.StatName.ToString(), Mod.Value * 100.f);
}

void ULKTraitAuraComponent::TickAura(float DeltaSeconds)
{
	ALKUnitBase* OwnerUnit = Cast<ALKUnitBase>(GetOwner());
	if (!OwnerUnit)
	{
		return;
	}

	// 主人死亡：一次性清掉所有已施加效果（此后 0.5s 内随 Actor 销毁）
	if (OwnerUnit->IsDead())
	{
		if (!bCleanedUp)
		{
			bCleanedUp = true;
			RemoveAllAuras();
		}
		return;
	}

	RefreshTimer -= DeltaSeconds;
	if (RefreshTimer > 0.f)
	{
		return;
	}
	RefreshTimer = RefreshInterval;

	for (FLKAuraGroup& Group : Groups)
	{
		RefreshGroup(OwnerUnit, Group);
	}
}

void ULKTraitAuraComponent::RefreshGroup(ALKUnitBase* OwnerUnit, FLKAuraGroup& Group)
{
	TArray<ALKUnitBase*> InRange;
	CollectTargetsInRange(OwnerUnit, Group, InRange);

	// 1) 圈内新目标：施加 GE 并记录 handle
	for (ALKUnitBase* Target : InRange)
	{
		if (Group.Applied.Contains(Target))
		{
			continue;
		}

		// 光环数值 = 目标当前【基础值】 × 百分比（与自身特性同规则：线性叠加、不滚雪球）
		const FGameplayAttribute Attr = LKGameplay::FindAttributeByName(Group.Modifier.StatName);
		if (!Attr.IsValid())
		{
			continue;
		}

		const float Base = LKGameplay::GetAttributeBaseValue(Target, Attr, 0.f);
		const FActiveGameplayEffectHandle Handle =
			LKGameplay::ApplyAttributeModifier(Target, Attr, Base * Group.Modifier.Value, 0.f, OwnerUnit);
		if (Handle.IsValid())
		{
			Group.Applied.Add(Target, Handle);
			UE_LOG(LogLKUnit, Log, TEXT("[Trait] 光环生效：%s -> %s（%s %+.1f）"),
				*OwnerUnit->GetUnitId().ToString(), *Target->GetUnitId().ToString(),
				*Group.Modifier.StatName.ToString(), Base * Group.Modifier.Value);
		}
	}

	// 2) 已施加但已失效（死亡/销毁/离圈/换阵营）：按 handle 移除
	//    用存储的 key（TWeakObjectPtr）移除，避免用裸指针重新构造 key 匹配不上
	TArray<TWeakObjectPtr<ALKUnitBase>> KeysToRemove;
	for (const TPair<TWeakObjectPtr<ALKUnitBase>, FActiveGameplayEffectHandle>& Pair : Group.Applied)
	{
		ALKUnitBase* Target = Pair.Key.Get();
		const bool bStillInRange = Target && InRange.Contains(Target);
		if (!bStillInRange)
		{
			KeysToRemove.Add(Pair.Key);
		}
	}

	for (const TWeakObjectPtr<ALKUnitBase>& Key : KeysToRemove)
	{
		ALKUnitBase* Target = Key.Get();
		if (Target && Target->IsAlive())
		{
			// 目标还活着（只是离圈）：真正移除 GE
			if (UAbilitySystemComponent* ASC = LKGameplay::GetASC(Target))
			{
				if (const FActiveGameplayEffectHandle* Handle = Group.Applied.Find(Key))
				{
					ASC->RemoveActiveGameplayEffect(*Handle);
				}
			}
			UE_LOG(LogLKUnit, Log, TEXT("[Trait] 光环移除：%s 离开 %s 的光环范围"),
				*Target->GetUnitId().ToString(), *OwnerUnit->GetUnitId().ToString());
		}
		// 目标已死/已销毁：GE 随其 ASC 一起消亡，无需手动移除
		Group.Applied.Remove(Key);
	}
}

void ULKTraitAuraComponent::CollectTargetsInRange(const ALKUnitBase* OwnerUnit, const FLKAuraGroup& Group, TArray<ALKUnitBase*>& OutTargets) const
{
	UWorld* World = GetWorld();
	if (!World || !OwnerUnit)
	{
		return;
	}

	const FVector OwnerLoc = OwnerUnit->GetActorLocation();
	for (TActorIterator<ALKUnitBase> It(World); It; ++It)
	{
		ALKUnitBase* Other = *It;
		if (!Other || Other == OwnerUnit || Other->IsDead())
		{
			continue;
		}
		if (Other->GetTeam() != OwnerUnit->GetTeam())
		{
			continue;
		}
		if (FVector::Dist2D(OwnerLoc, Other->GetActorLocation()) <= Group.Radius)
		{
			OutTargets.Add(Other);
		}
	}
}

void ULKTraitAuraComponent::RemoveAllAuras()
{
	for (FLKAuraGroup& Group : Groups)
	{
		for (const TPair<TWeakObjectPtr<ALKUnitBase>, FActiveGameplayEffectHandle>& Pair : Group.Applied)
		{
			ALKUnitBase* Target = Pair.Key.Get();
			if (Target && Target->IsAlive())
			{
				if (UAbilitySystemComponent* ASC = LKGameplay::GetASC(Target))
				{
					ASC->RemoveActiveGameplayEffect(Pair.Value);
				}
			}
		}
		Group.Applied.Reset();
	}
}

int32 ULKTraitAuraComponent::GetActiveEffectCount() const
{
	int32 Count = 0;
	for (const FLKAuraGroup& Group : Groups)
	{
		Count += Group.Applied.Num();
	}
	return Count;
}
