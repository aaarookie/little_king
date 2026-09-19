#pragma once
#include "CoreMinimal.h"
struct FLKRunCardState;

/** Deck budget is independent from the four visible hand slots. */
namespace LKCardRules
{
    constexpr int32 Capacity = 8;
    constexpr int32 MinimumCards = 5;
    int32 Slots(FName CardId);
    int32 Used(const TArray<FName>& Cards);
    int32 Used(const TArray<FLKRunCardState>& Cards);
}
