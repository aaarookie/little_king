#include "ULKUnitPassiveComponent.h"
#include "ALKUnitBase.h"
#include "ALKBattleGameMode.h"
#include "LKGameplayHelpers.h"
#include "LKLog.h"
#include "Engine/World.h"

ULKUnitPassiveComponent::ULKUnitPassiveComponent() { PrimaryComponentTick.bCanEverTick = false; }

void ULKUnitPassiveComponent::Initialize(const FLKUnitRow& Row)
{
    Ability = Row.PassiveAbility;
    HealPercent = FMath::IsFinite(Row.PassiveHealPercent) ? FMath::Clamp(Row.PassiveHealPercent, 0.f, 1.f) : 0.03f;
    RevivalThreshold = FMath::Max(1, Row.RevivalInitialThreshold);
    ThresholdStep = FMath::Max(1, Row.RevivalThresholdStep);
    BoneCount = 0; RevivalCount = 0;
}

bool ULKUnitPassiveComponent::CanSummonFrom(const ALKUnitBase* Victim) const
{
    const ALKUnitBase* OwnerUnit = Cast<ALKUnitBase>(GetOwner());
    return Ability == ELKPassiveAbility::UndeadSummoning && OwnerUnit && OwnerUnit->IsAlive()
        && Victim && Victim->IsDead() && Victim->IsSoldier() && Victim->GetTeam() != OwnerUnit->GetTeam();
}

bool ULKUnitPassiveComponent::TrySummonFrom(ALKUnitBase* Victim)
{
    ALKUnitBase* OwnerUnit = Cast<ALKUnitBase>(GetOwner());
    ALKBattleGameMode* GM = GetWorld()->GetAuthGameMode<ALKBattleGameMode>();
    if (!GM || GM->GetPhase() != ELKGamePhase::Battle || !CanSummonFrom(Victim)) { return false; }
    const FName SummonId = Victim->GetAttackType() == ELKAttackType::Melee ? FName("Unit_Skeleton") : FName("Unit_SkeletonArcher");
    ALKUnitBase* Summon = GM->SpawnUnitForTeam(SummonId, OwnerUnit->GetTeam(), Victim->GetActorLocation());
    if (!Summon) { return false; } // 上限/实体占用阻止转换时不献祭，也不延期补召。
    Summon->SetOwner(OwnerUnit);
    const float Cost = OwnerUnit->GetMaxHealth() * OwnerUnit->GetTraitEffectValue(ELKTraitEffect::SummoningHealthCost);
    const FLKCombatSource Source = LKGameplay::MakeSource(OwnerUnit, ELKCombatSourceKind::HealthCost, "Trait_Sacrifice");
    LKGameplay::ApplyDamage(OwnerUnit, Cost, OwnerUnit, true, &Source);
    UE_LOG(LogLKUnit, Log, TEXT("[Passive] 亡灵召唤 %s -> %s；献祭 %.1f"), *Victim->GetUnitId().ToString(), *SummonId.ToString(), Cost);
    return true;
}

void ULKUnitPassiveComponent::ObserveDefeat(const ALKUnitBase* Victim, bool bOwnerDefeatedInBatch)
{
    ALKUnitBase* OwnerUnit = Cast<ALKUnitBase>(GetOwner());
    if (!OwnerUnit || !Victim || Victim == OwnerUnit || Victim->GetTeam() != OwnerUnit->GetTeam()) { return; }
    if (Ability == ELKPassiveAbility::GiantBones && OwnerUnit->IsAlive()
        && (Victim->GetUnitId() == "Unit_Skeleton" || Victim->GetUnitId() == "Unit_SkeletonArcher"))
    {
        const FLKCombatSource Source = LKGameplay::MakeSource(OwnerUnit, ELKCombatSourceKind::Skill, "Skill_GiantBones");
        LKGameplay::ApplyHeal(OwnerUnit, OwnerUnit->GetMaxHealth() * HealPercent, OwnerUnit, &Source);
    }
    if (Ability == ELKPassiveAbility::BoneRegeneration && (OwnerUnit->IsAlive() || bOwnerDefeatedInBatch)
        && Victim->IsSkeleton() && (Victim->IsSoldier() || Victim->IsHero()))
    {
        BoneCount = int32(FMath::Min(int64(RevivalThreshold), int64(BoneCount) + (Victim->IsHero() ? 5 : 1)));
        UE_LOG(LogLKUnit, Log, TEXT("[Passive] 朽骨再生 %s %d/%d"), *OwnerUnit->GetName(), BoneCount, RevivalThreshold);
    }
}

bool ULKUnitPassiveComponent::TryRevive()
{
    ALKUnitBase* OwnerUnit = Cast<ALKUnitBase>(GetOwner());
    if (Ability != ELKPassiveAbility::BoneRegeneration || !OwnerUnit || !OwnerUnit->IsDead() || BoneCount < RevivalThreshold) { return false; }
    if (!OwnerUnit->ReviveDuringBattle()) { return false; }
    BoneCount = 0;
    RevivalThreshold = int32(FMath::Min(int64(MAX_int32), int64(RevivalThreshold) + ThresholdStep));
    ++RevivalCount;
    UE_LOG(LogLKUnit, Log, TEXT("[Passive] 骷髅王满血复活，第 %d 次，下次门槛 %d"), RevivalCount, RevivalThreshold);
    return true;
}
