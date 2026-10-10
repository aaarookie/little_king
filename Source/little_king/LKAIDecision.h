#pragma once

#include "CoreMinimal.h"
#include "LKAIObservation.h"

class ULKCardDefinition;
struct FLKUnitRow;

enum class ELKAIIntent : uint8 { Hold, Pressure, Protect, Breach };

struct FLKAIUtility
{
    float Benefit = 0.f;
    float Synergy = 0.f;
    float Cost = 0.f;
    float Risk = 0.f;
    float Total() const { return Benefit + Synergy - Cost - Risk; }
};

struct FLKAICandidate
{
    FName CardId;
    int32 HandIndex = INDEX_NONE;
    FVector Location = FVector::ZeroVector;
    float Yaw = 0.f;
    FLKAIUtility Utility;
};

struct FLKAIDecisionTrace
{
    uint32 ObservationRevision = 0;
    float BattleTime = 0.f;
    ELKAIIntent Intent = ELKAIIntent::Hold;
    FName IntentTarget;
    FName CardId;
    FVector Location = FVector::ZeroVector;
    FLKAIUtility Utility;
    int32 Candidates = 0, ValidationChecks = 0;
    float SilverBefore = 0.f, SilverAfter = 0.f;
    FString Result;
    TArray<FLKAICandidate> Shortlist;
};

namespace LKAIDecision
{
    constexpr int32 MaxCandidatesPerCard = 12;
    constexpr int32 MaxValidationChecks = 8;
    constexpr int32 MaxTraceHistory = 128;
    const TCHAR* IntentName(ELKAIIntent Intent);
    /** No live world or actor reads; scores consume the supplied observation only. */
    FLKAIUtility Score(const ULKCardDefinition& Card, const FLKUnitRow* Unit, const FLKAIObservation& Observation,
        const FVector& Location, float Silver, ELKAIIntent Intent, float UpgradeScale = 1.f);
    void Candidates(const ULKCardDefinition& Card, const FLKUnitRow* Unit, const FLKAIObservation& Observation,
        float Silver, ELKAIIntent Intent, int32 HandIndex, TArray<FLKAICandidate>& Out, float UpgradeScale = 1.f);
    FName ChooseIntentTarget(const FLKAIObservation& Observation, ELKAIIntent& OutIntent);
}
