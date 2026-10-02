#pragma once
#include "CoreMinimal.h"
#include "LKTypes.h"
class ALKBattleGameMode;
class ALKUnitBase;
class ULKCardDefinition;
namespace LKSpellExecutor
{
    ELKPlayResult Validate(const ALKBattleGameMode* GM, const ULKCardDefinition* Card, ELKTeam Team, const FVector& Location, bool bIgnorePlacement = false);
    bool Execute(ALKBattleGameMode* GM, const ULKCardDefinition* Card, ELKTeam Team, const FVector& Location, ALKUnitBase* Instigator = nullptr, bool bIgnorePlacement = false);
    bool PlanSummons(const ALKBattleGameMode* GM, const ULKCardDefinition* Card, ELKTeam Team, const FVector& Location, TArray<FVector>& Positions);
}
