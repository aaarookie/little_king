#include "ULKUnitPassiveComponent.h"
#include "ALKUnitBase.h"
#include "ALKBattleGameMode.h"
#include "LKGameplayHelpers.h"
#include "LKLog.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ULKUnitActiveComponent.h"
#include "ULKSilverComponent.h"
#include "ULKPresentationSubsystem.h"

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
    ULKPresentationSubsystem::Emit(GetWorld(), ELKVisualCue::Summon, Summon->GetActorLocation(), 100.f);
    ULKPresentationSubsystem::Sound(GetWorld(), "Summon", Summon->GetActorLocation());
    const float Cost = OwnerUnit->GetMaxHealth() * OwnerUnit->GetTraitEffectValue(ELKTraitEffect::SummoningHealthCost);
    const FLKCombatSource Source = LKGameplay::MakeSource(OwnerUnit, ELKCombatSourceKind::HealthCost, "Trait_Sacrifice");
    LKGameplay::ApplyDamage(OwnerUnit, Cost, OwnerUnit, true, &Source);
    if (Cost > 0.f)
    { ULKPresentationSubsystem::Emit(GetWorld(), ELKVisualCue::Sacrifice, OwnerUnit->GetActorLocation()); ULKPresentationSubsystem::Sound(GetWorld(), "Sacrifice", OwnerUnit->GetActorLocation()); }
    UE_LOG(LogLKUnit, Log, TEXT("[Passive] 亡灵召唤 %s -> %s；献祭 %.1f"), *Victim->GetUnitId().ToString(), *SummonId.ToString(), Cost);
    return true;
}

void ULKUnitPassiveComponent::ObserveDefeat(const ALKUnitBase* Victim, bool bOwnerDefeatedInBatch)
{
    ALKUnitBase* OwnerUnit = Cast<ALKUnitBase>(GetOwner());
    if (OwnerUnit && Victim && OwnerUnit->IsAlive())
    {
        OwnerUnit->GetActiveComponent()->ObserveDefeat(Victim);
        if (Ability == ELKPassiveAbility::Loot && OwnerUnit->GetTarget() == Victim && Victim->GetTeam() != OwnerUnit->GetTeam())
        {
            if (ALKBattleGameMode* GM = GetWorld()->GetAuthGameMode<ALKBattleGameMode>())
            { if (ULKSilverComponent* Silver = GM->GetTeamSilver(OwnerUnit->GetTeam()))
                { const float Before = Silver->GetSilver(); Silver->AddSilver(1.f);
                  if (Silver->GetSilver() > Before) { ULKPresentationSubsystem::Emit(GetWorld(), ELKVisualCue::Coin, OwnerUnit->GetActorLocation()); ULKPresentationSubsystem::Sound(GetWorld(), "Coin", OwnerUnit->GetActorLocation()); } } }
        }
    }
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

void ULKUnitPassiveComponent::ObserveCombatEvent(const FLKCombatEvent& Event)
{
    ALKUnitBase* OwnerUnit = Cast<ALKUnitBase>(GetOwner());
    if (!OwnerUnit || !OwnerUnit->IsTargetable() || !OwnerUnit->IsCombatEnabled() || Event.ActualAmount <= 0.f) { return; }
    if (Ability == ELKPassiveAbility::AttackRenewal && !Event.bIsHeal && Event.Source.InstanceId == OwnerUnit->GetFName()
        && Event.TargetTeam != OwnerUnit->GetTeam()
        && (Event.Source.Kind == ELKCombatSourceKind::Attack || Event.Source.Kind == ELKCombatSourceKind::Projectile))
    {
        const FLKCombatSource Source = LKGameplay::MakeSource(OwnerUnit, ELKCombatSourceKind::Skill, "Skill_AttackRenewal");
        LKGameplay::ApplyHeal(OwnerUnit, OwnerUnit->GetMaxHealth() * HealPercent, OwnerUnit, &Source);
    }
    if (Ability != ELKPassiveAbility::SharedSpring || !Event.bIsHeal || Event.TargetTeam != OwnerUnit->GetTeam()
        || Event.Source.ActionId == "Skill_SharedSpring") { return; }
    bool bElfHealed = false;
    ALKUnitBase* Lowest = nullptr;
    float LowestRatio = TNumericLimits<float>::Max();
    for (TActorIterator<ALKUnitBase> It(GetWorld()); It; ++It)
    {
        ALKUnitBase* Unit = *It;
        if (!Unit->IsTargetable() || Unit->GetTeam() != OwnerUnit->GetTeam()) { continue; }
        if (Unit->GetFName() == Event.TargetInstanceId && Unit->GetRace() == ELKRace::Elf) { bElfHealed = true; }
        if (!Unit->IsHero()) { continue; }
        const float Ratio = Unit->GetHealth() / FMath::Max(1.f, Unit->GetMaxHealth());
        if (Ratio < LowestRatio || (Ratio == LowestRatio && Lowest && Unit->GetFName().LexicalLess(Lowest->GetFName())))
        { Lowest = Unit; LowestRatio = Ratio; }
    }
    if (bElfHealed && Lowest)
    {
        const FLKCombatSource Source = LKGameplay::MakeSource(OwnerUnit, ELKCombatSourceKind::Skill, "Skill_SharedSpring");
        if (LKGameplay::ApplyHeal(Lowest, Event.ActualAmount, OwnerUnit, &Source) > 0.f)
        { ULKPresentationSubsystem::Emit(GetWorld(), ELKVisualCue::HealLink, Lowest->GetActorLocation(), 60.f, Event.Location); }
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
    ULKPresentationSubsystem::Emit(GetWorld(), ELKVisualCue::Revive, OwnerUnit->GetActorLocation(), 180.f);
    ULKPresentationSubsystem::Sound(GetWorld(), "Revive", OwnerUnit->GetActorLocation());
    UE_LOG(LogLKUnit, Log, TEXT("[Passive] 骷髅王满血复活，第 %d 次，下次门槛 %d"), RevivalCount, RevivalThreshold);
    return true;
}
