#include "LKCardRules.h"
#include "LKUnitContent.h"
#include "LKRunTypes.h"

namespace LKCardRules
{
int32 Slots(FName CardId)
{
    const FLKUnitRow* Row = LKUnitContent::Find(CardId);
    return Row ? FMath::Clamp(Row->DeckSlots, 1, Capacity) : 1;
}
int32 Used(const TArray<FName>& Cards)
{
    int32 Result = 0;
    for (FName Id : Cards) { Result += Slots(Id); }
    return Result;
}
int32 Used(const TArray<FLKRunCardState>& Cards)
{
    int32 Result = 0;
    for (const FLKRunCardState& Card : Cards) { Result += Slots(Card.CardId); }
    return Result;
}
}
