#include "LKSpellExecutor.h"
#include "ALKBattleGameMode.h"
#include "ALKSpellField.h"
#include "ALKUnitBase.h"
#include "ULKCardDefinition.h"
#include "ULKGameData.h"
#include "ULKUnitStatusComponent.h"
#include "ULKPresentationSubsystem.h"
#include "LKGameplayHelpers.h"
#include "EngineUtils.h"
#include "GameplayEffect.h"
#include "ULKUnitAttributeSet.h"

namespace
{
bool Positive(float V) { return FMath::IsFinite(V) && V > 0.f; }
bool Fraction(float V) { return FMath::IsFinite(V) && V >= 0.f && V <= 1.f; }
}
bool LKSpellExecutor::PlanSummons(const ALKBattleGameMode* GM, const ULKCardDefinition* Card, ELKTeam Team, const FVector& Location, TArray<FVector>& Positions)
{
    Positions.Reset();
    if (!GM || !Card || !GM->GetGameData() || Card->SummonedUnitIds.IsEmpty()) { return false; }
    if (GM->GetGameData()->MaxUnitsPerTeam > 0 && GM->CountAliveUnits(Team) + Card->SummonedUnitIds.Num() > GM->GetGameData()->MaxUnitsPerTeam) { return false; }
    const float Body = GM->GetGameData()->UnitBodyRadius;
    const float Scale = GM->GetCardUpgradeScale(Card->CardId, Team);
    if (!Positive(Body) || !Positive(Scale)) { return false; }
    // A deterministic layout avoids consuming combat RNG on previews or failed casts.
    for (FName Id : Card->SummonedUnitIds)
    {
        const FLKUnitRow* Row = GM->GetUnitRow(Id);
        if (!Row || Row->UnitClass != ELKUnitClass::Soldier || !Positive(Row->BaseHealth) || !FMath::IsFinite(Row->MoveSpeed) || Row->MoveSpeed < 0.f
            || !FMath::IsFinite(Row->AttackDamage) || Row->AttackDamage < 0.f || !Positive(Row->AttackInterval) || !FMath::IsFinite(Row->AttackRange) || Row->AttackRange < 0.f
            || !FMath::IsFinite(Row->BaseHealth*Scale) || !FMath::IsFinite(Row->AttackDamage*Scale)) { return false; }
    }
    for (FName Id : Card->SummonedUnitIds)
    {
        const FLKUnitRow* Row = GM->GetUnitRow(Id);
        if (!Row || Row->UnitClass != ELKUnitClass::Soldier) { return false; }
        bool bFound = false;
        for (int32 Ring = 0; Ring <= 7 && !bFound; ++Ring)
        {
            const int32 Count = Ring == 0 ? 1 : 16;
            for (int32 I = 0; I < Count; ++I)
            {
                const float Angle = I * 2.f * PI / Count;
                const FVector P = FVector(Location.X, Location.Y, 0.f) + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Ring * (2.f * Body + 10.f);
                if (!GM->IsSpawnPointFree(P, Body)) { continue; }
                bool bOverlap = false;
                for (const FVector& Other : Positions) { if (FVector::DistSquared2D(P, Other) < FMath::Square(2.f * Body + 2.f)) { bOverlap = true; break; } }
                if (!bOverlap) { Positions.Add(P); bFound = true; break; }
            }
        }
        if (!bFound) { Positions.Reset(); return false; }
    }
    return true;
}
ELKPlayResult LKSpellExecutor::Validate(const ALKBattleGameMode* GM, const ULKCardDefinition* C, ELKTeam Team, const FVector& P, bool bIgnorePlacement)
{
    if (!GM || !GM->GetGameData() || !GM->GetWorld() || !C || C->CardType != ELKCardType::Spell || (C->SpellEffect != ELKSpellEffect::DivineBlessing && !Positive(C->SpellRadius)) || !FMath::IsFinite(C->DrawCooldown) || C->DrawCooldown < 0.f) { return ELKPlayResult::InvalidCardData; }
    if (!GM->IsInsideField(P)) { return ELKPlayResult::InvalidLocation; }
    if (!bIgnorePlacement && !C->bGlobalPlacement && !GM->CanPlaceSpellAt(Team, P)) { return ELKPlayResult::SpellLocked; }
    switch (C->SpellEffect)
    {
    case ELKSpellEffect::Damage: case ELKSpellEffect::Heal:
        return Positive(C->SpellValue) && Fraction(C->HeroHealPercent) ? ELKPlayResult::Success : ELKPlayResult::InvalidCardData;
    case ELKSpellEffect::Freeze: case ELKSpellEffect::BlackCloud:
        return Positive(C->EffectDuration) && Positive(C->EffectDuration*GM->GetCardUpgradeScale(C->CardId,Team)) ? ELKPlayResult::Success : ELKPlayResult::InvalidCardData;
    case ELKSpellEffect::Hurricane:
        return Positive(C->EffectDuration) && Positive(C->ForceMoveSpeed) && Positive(C->SpellHalfExtents.X) && Positive(C->SpellHalfExtents.Y) ? ELKPlayResult::Success : ELKPlayResult::InvalidCardData;
    case ELKSpellEffect::Lightning:
        return Positive(C->SpellValue) && Positive(C->SecondarySpellValue) && Positive(C->SecondaryDelay) && Positive(C->MaxHealthDamageFraction) && Fraction(C->MaxHealthDamageFraction) ? ELKPlayResult::Success : ELKPlayResult::InvalidCardData;
    case ELKSpellEffect::DivineBlessing:
        return Fraction(C->HeroHealPercent) ? ELKPlayResult::Success : ELKPlayResult::InvalidCardData;
    case ELKSpellEffect::Reinforcements:
    {
        if (GM->GetGameData()->MaxUnitsPerTeam > 0 && GM->CountAliveUnits(Team) + C->SummonedUnitIds.Num() > GM->GetGameData()->MaxUnitsPerTeam) { return ELKPlayResult::UnitLimitReached; }
        TArray<FVector> Positions;
        return PlanSummons(GM, C, Team, P, Positions) ? ELKPlayResult::Success : ELKPlayResult::InvalidLocation;
    }
    default: return ELKPlayResult::InvalidCardData;
    }
}
bool LKSpellExecutor::Execute(ALKBattleGameMode* GM, const ULKCardDefinition* C, ELKTeam Team, const FVector& P, ALKUnitBase* Instigator, bool bIgnorePlacement)
{
    if (Validate(GM, C, Team, P, bIgnorePlacement) != ELKPlayResult::Success || GM->GetPhase() != ELKGamePhase::Battle) { return false; }
    FLKCombatSource Source = LKGameplay::MakeSource(Instigator, Instigator ? ELKCombatSourceKind::Skill : ELKCombatSourceKind::Spell, C->CardId);
    Source.bHasTeam = true; Source.Team = Team; Source.bRangedSource = true;
    const float Scale = GM->GetCardUpgradeScale(C->CardId, Team);
    if (C->SpellEffect == ELKSpellEffect::BlackCloud || C->SpellEffect == ELKSpellEffect::Hurricane || C->SpellEffect == ELKSpellEffect::Lightning)
    {
        auto* Field = GM->GetWorld()->SpawnActor<ALKSpellField>();
        if (!Field || !Field->Initialize(GM, C, Team, P, Scale, Source)) { if (Field) { Field->Destroy(); } return false; }
    }
    if (C->SpellEffect == ELKSpellEffect::Reinforcements)
    {
        TArray<FVector> Positions;
        if (!PlanSummons(GM, C, Team, P, Positions)) { return false; }
        TArray<ALKUnitBase*> Created;
        for (int32 I = 0; I < Positions.Num(); ++I)
        {
            ALKUnitBase* Unit = GM->SpawnUnitForTeam(C->SummonedUnitIds[I], Team, Positions[I], ELKUnitClass::Soldier, C->CardId);
            if (!Unit) { for (ALKUnitBase* Other : Created) { Other->Destroy(); } return false; }
            Created.Add(Unit);
        }
        for (ALKUnitBase* Unit : Created) { ULKPresentationSubsystem::Emit(GM->GetWorld(), ELKVisualCue::Summon, Unit->GetActorLocation(), 85.f); }
        ULKPresentationSubsystem::Sound(GM->GetWorld(), "HolySummon", P);
        return true;
    }
    TArray<ALKUnitBase*> Targets;
    for (TActorIterator<ALKUnitBase> It(GM->GetWorld()); It; ++It)
    { if (It->IsTargetable() && (C->SpellEffect == ELKSpellEffect::DivineBlessing || FVector::DistSquared2D(P, It->GetActorLocation()) <= FMath::Square(C->SpellRadius))) { Targets.Add(*It); } }
    GM->BeginCombatBatch();
    if (C->SpellEffect == ELKSpellEffect::Lightning)
    {
        ALKUnitBase* Highest = nullptr;
        for (ALKUnitBase* Unit : Targets)
        { if (Unit->GetTeam() != Team && !Unit->IsHero() && (!Highest || Unit->GetHealth() > Highest->GetHealth() || (Unit->GetHealth() == Highest->GetHealth() && Unit->GetFName().LexicalLess(Highest->GetFName())))) { Highest = Unit; } }
        if (Highest)
        { LKGameplay::ApplyDamage(Highest, FMath::Min(Highest->GetMaxHealth() * C->MaxHealthDamageFraction, C->SpellValue * Scale), Instigator, false, &Source); ULKPresentationSubsystem::Emit(GM->GetWorld(), ELKVisualCue::Lightning, Highest->GetActorLocation()); }
    }
    for (ALKUnitBase* Unit : Targets)
    {
        if (!IsValid(Unit) || !Unit->IsTargetable()) { continue; }
        if (C->SpellEffect == ELKSpellEffect::Damage && Unit->GetTeam() != Team) { LKGameplay::ApplyDamage(Unit, C->SpellValue * Scale, Instigator, false, &Source); }
        else if (C->SpellEffect == ELKSpellEffect::Heal && Unit->GetTeam() == Team) { LKGameplay::ApplyHeal(Unit, (C->SpellValue + (Unit->IsHero() ? Unit->GetMaxHealth() * C->HeroHealPercent : 0.f)) * Scale, Instigator, &Source); }
        else if (C->SpellEffect == ELKSpellEffect::Freeze && Unit->GetTeam() != Team) { Unit->GetStatusComponent()->Freeze(C->EffectDuration * Scale); }
        else if (C->SpellEffect == ELKSpellEffect::DivineBlessing && Unit->GetTeam() == Team)
        { LKGameplay::ApplyHeal(Unit, Unit->GetMaxHealth() * (Unit->IsHero() ? FMath::Min(1.f, C->HeroHealPercent * Scale) : 1.f), Instigator, &Source); ULKPresentationSubsystem::Emit(GM->GetWorld(), ELKVisualCue::Heal, Unit->GetActorLocation()); }
    }
    if (C->CardId == "Spell_Fireball" || C->CardId.ToString().StartsWith(TEXT("Spell_ResearchFireball"))) { GM->NotifyFireballCast(P, C->SpellRadius); }
    if (C->SpellEffect == ELKSpellEffect::Freeze) { ULKPresentationSubsystem::Emit(GM->GetWorld(), ELKVisualCue::FreezeSpell, P, C->SpellRadius); ULKPresentationSubsystem::Sound(GM->GetWorld(), "FreezeSpell", P); }
    if (C->SpellEffect == ELKSpellEffect::DivineBlessing) { ULKPresentationSubsystem::Emit(GM->GetWorld(), ELKVisualCue::DivineBlessing, P, C->SpellRadius); ULKPresentationSubsystem::Sound(GM->GetWorld(), "DivineBlessing", P); }
    GM->EndCombatBatch();
    return true;
}
