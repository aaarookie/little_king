#pragma once

#include "CoreMinimal.h"
#include "LKTypes.h"

class ALKBattleGameMode;
class ALKUnitBase;

/** Values are copied only after discovery succeeds. Decision code never dereferences Actor. */
struct FLKAIUnitObservation
{
    FName Id;
    FName UnitId;
    TWeakObjectPtr<ALKUnitBase> Actor;
    FVector Position = FVector::ZeroVector;
    float Health = 0.f, MaxHealth = 1.f, Damage = 0.f, Interval = 1.f, Range = 0.f;
    float WallYaw = 0.f;
    bool bHero = false, bBuilding = false, bWall = false, bMobile = true;
    bool bHealer = false, bMage = false, bControlled = false, bConcealed = false;
    bool bSheltered = false, bHighland = false, bBusyWithService = false;
    ELKAttackType AttackType = ELKAttackType::Melee;
    float HealthRatio() const { return FMath::Clamp(Health / FMath::Max(1.f, MaxHealth), 0.f, 1.f); }
    float DPS() const { return Damage / FMath::Max(.1f, Interval); }
};

struct FLKAIObservation
{
    uint32 Revision = 0;
    float BattleTime = 0.f;
    ELKTeam Team = ELKTeam::Enemy;
    TArray<FLKAIUnitObservation> Allies;
    TArray<FLKAIUnitObservation> Enemies;
    FVector2D FieldHalfSize = FVector2D(1200.f, 2000.f);
    int32 UnitLimit = 20;
    bool bGlobalSpellPlacement = false;
    const FLKAIUnitObservation* FindEnemy(FName Id) const;
};

namespace LKAIObservation
{
    /** Team discovery is the union of living non-camp observers' existing discovery rules. */
    bool CanTeamDiscover(const ALKBattleGameMode* GM, ELKTeam Team, const ALKUnitBase* Target);
    FLKAIObservation Capture(const ALKBattleGameMode* GM, ELKTeam Team, float BattleTime, uint32 Revision);
}
