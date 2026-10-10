#pragma once
#include "CoreMinimal.h"
#include "LKRunTypes.h"

struct FLKItemDefinition
{
    FName ItemId;
    FText Name;
    FText Description;
    ELKItemKind Kind = ELKItemKind::Accessory;
    bool bPlaceholder = true;
};

namespace LKItemContent
{
    const TArray<FLKItemDefinition>& All();
    const FLKItemDefinition* Find(FName ItemId);
    bool ValidateInstances(const TArray<FLKItemInstance>& Items, FString& Error);
    bool EqualInstances(const TArray<FLKItemInstance>& A, const TArray<FLKItemInstance>& B);
}
