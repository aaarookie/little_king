#pragma once
#if WITH_DEV_AUTOMATION_TESTS
#include "../LKHomeContent.h"
#include "../LKUnitContent.h"
#include "../LKWorldMapContent.h"
#include "../ULKCardDefinition.h"
#include "../ULKGameData.h"
#include "../ULKProfileSubsystem.h"
#include "../ULKRunSubsystem.h"
#include "Engine/GameInstance.h"

namespace LKWorldMapTest
{
inline bool Request(ULKProfileSubsystem *Profile, FLKExpeditionStartRequest &Out)
{
    ULKGameData *Data = NewObject<ULKGameData>();
    Data->EnsureDefaultDecks();
    Data->EnsureCardLibrary();
    FString Error;
    return LKHomeContent::BuildExpeditionStartRequest(
        *Profile, Data, LKHomeContent::DefaultRegionId(), [](FName Id) { return LKUnitContent::Find(Id); },
        [Data](FName Id) -> const ULKCardDefinition * {
            for (const ULKCardDefinition *Card : Data->CardLibrary)
            {
                if (Card && Card->CardId == Id)
                {
                    return Card;
                }
            }
            return nullptr;
        },
        Out, Error);
}
inline bool SelectBattle(ULKRunSubsystem *Run)
{
    for (int32 I = 0; I < 50 && !Run->IsTerminal(); ++I)
    {
        if (Run->GetRunPhase() == ELKRunPhase::EnteringBattle || Run->GetRunPhase() == ELKRunPhase::InBattle)
        {
            return true;
        }
        if (Run->HasPendingRewardChoice())
        {
            if (!Run->SkipReward())
            {
                return false;
            }
            continue;
        }
        if (Run->HasServiceNode())
        {
            if (!Run->ResolveServiceNode(Run->GetNode(Run->GetRunState().CurrentNodeId).Type == ELKDungeonNodeType::Rest
                                             ? TEXT("RestHeal")
                                             : TEXT("LeaveMarket")))
            {
                return false;
            }
            continue;
        }
        TArray<FName> Next = Run->GetNextNodeIds();
        if (Next.IsEmpty())
        {
            return false;
        }
        FName Pick = Next[0];
        for (FName Id : Next)
        {
            if (LKWorldMapContent::IsCombat(Run->GetNode(Id).Type))
            {
                Pick = Id;
                break;
            }
        }
        if (Run->SelectNode(Pick) == ELKNodeSelectionResult::Rejected)
        {
            return false;
        }
    }
    return false;
}
inline bool StartSelectedBattle(UGameInstance *Instance)
{
    ULKProfileSubsystem *Profile = Instance->GetSubsystem<ULKProfileSubsystem>();
    if (!Profile->EnsureProfile())
    {
        return false;
    }
    FLKExpeditionStartRequest Input;
    if (!Request(Profile, Input))
    {
        return false;
    }
    ULKRunSubsystem *Run = Instance->GetSubsystem<ULKRunSubsystem>();
    return Run->StartNewRun(Input) && SelectBattle(Run);
}
} // namespace LKWorldMapTest
#endif
