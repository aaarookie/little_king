#pragma once
#include "CoreMinimal.h"
#include "LKDataTypes.h"

struct FLKTemporaryMercenaryDefinition
{
    FLKUnitRow Unit;
    int32 Cost = 3;
    FText Description;
};

namespace LKExpeditionMercenaryContent
{
    const TArray<FLKTemporaryMercenaryDefinition>& All();
    bool IsTemporary(FName CardId);
}
