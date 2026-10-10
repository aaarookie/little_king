#include "LKAIObservation.h"
#include "ALKBattleGameMode.h"
#include "ALKUnitBase.h"
#include "ALKUnitBuilding.h"
#include "ULKGameData.h"
#include "ULKUnitStatusComponent.h"
#include "EngineUtils.h"

const FLKAIUnitObservation* FLKAIObservation::FindEnemy(FName Id) const
{
    return Enemies.FindByPredicate([Id](const auto& Unit) { return Unit.Id == Id; });
}

namespace
{
FLKAIUnitObservation ReadKnownUnit(ALKUnitBase* Unit)
{
    FLKAIUnitObservation V;
    V.Id = Unit->GetFName(); V.UnitId = Unit->GetUnitId(); V.Actor = Unit;
    V.Position = Unit->GetActorLocation(); V.Health = Unit->GetHealth(); V.MaxHealth = Unit->GetMaxHealth();
    V.Damage = Unit->GetAttackDamage(); V.Interval = Unit->GetAttackInterval(); V.Range = Unit->GetAttackRange();
    V.bHero = Unit->IsHero(); V.bBuilding = Unit->IsBuilding(); V.bMobile = !V.bBuilding || Unit->IsMobileBuilding();
    V.bHealer = Unit->BasicAttackHeals(); V.bMage = Unit->IsMage(); V.bControlled = Unit->IsControlled();
    V.bConcealed = Unit->GetStatusComponent()->IsConcealed(); V.AttackType = Unit->GetAttackType();
    V.bSheltered = Unit->IsSheltered(); V.bHighland = Unit->IsHighlandResident();
    V.bBusyWithService = Unit->GetServiceBuilding() || Unit->GetPatientStation();
    if (const auto* Building = Cast<ALKUnitBuilding>(Unit)) { V.bWall = Building->IsWall(); V.WallYaw = Building->GetWallYaw(); }
    return V;
}
}

bool LKAIObservation::CanTeamDiscover(const ALKBattleGameMode* GM, ELKTeam Team, const ALKUnitBase* Target)
{
    if (!GM || !GM->GetWorld() || !IsValid(Target) || !Target->IsTargetable()) { return false; }
    if (Target->GetTeam() == Team) { return true; }
    for (TActorIterator<ALKUnitBase> It(GM->GetWorld()); It; ++It)
    {
        if (It->GetTeam() == Team && It->IsTargetable() && It->IsCombatEnabled() && It->CanDiscoverTarget(Target)) { return true; }
    }
    return false;
}

FLKAIObservation LKAIObservation::Capture(const ALKBattleGameMode* GM, ELKTeam Team, float BattleTime, uint32 Revision)
{
    FLKAIObservation Result; Result.Team = Team; Result.Revision = Revision; Result.BattleTime = BattleTime;
    if (!GM || !GM->GetWorld() || !GM->GetGameData()) { return Result; }
    Result.FieldHalfSize = FVector2D(GM->GetGameData()->FieldHalfWidth, GM->GetGameData()->FieldHalfHeight);
    Result.UnitLimit = GM->GetGameData()->MaxUnitsPerTeam;
    Result.bGlobalSpellPlacement = GM->HasGlobalSpellPlacement(Team);
    TArray<ALKUnitBase*> Observers;
    for (TActorIterator<ALKUnitBase> It(GM->GetWorld()); It; ++It)
    {
        if (It->GetTeam() == Team && It->IsTargetable() && It->IsCombatEnabled())
        { Observers.Add(*It); Result.Allies.Add(ReadKnownUnit(*It)); }
    }
    for (TActorIterator<ALKUnitBase> It(GM->GetWorld()); It; ++It)
    {
        if (It->GetTeam() == Team || !It->IsTargetable() || !It->IsCombatEnabled()) { continue; }
        for (const ALKUnitBase* Observer : Observers)
        {
            if (Observer->CanDiscoverTarget(*It)) { Result.Enemies.Add(ReadKnownUnit(*It)); break; }
        }
    }
    auto Sort = [](const auto& A, const auto& B) { return A.Id.LexicalLess(B.Id); };
    Result.Allies.Sort(Sort); Result.Enemies.Sort(Sort);
    return Result;
}
