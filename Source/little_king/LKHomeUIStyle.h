#pragma once

#include "CoreMinimal.h"
#include "LKUnitContent.h"
#include "LKCardPresentation.h"

/** Shared placeholder colours. Display only: no gameplay identity is inferred from colour. */
namespace LKHomeUIStyle
{
    inline FLinearColor Accent(FName Id)
    {
        if (const FLKUnitRow* Row = LKUnitContent::Find(Id)) { return LKCardPresentation::QualityColor(Row->Quality); }
        const FString Name = Id.ToString();
        if (Name.Contains(TEXT("Statue")) || Name.Contains(TEXT("Knight"))) { return FLinearColor(0.83f, 0.60f, 0.25f); }
        if (Name.Contains(TEXT("Library")) || Name.Contains(TEXT("Mage"))) { return FLinearColor(0.48f, 0.40f, 0.76f); }
        if (Name.Contains(TEXT("HeroHouse")) || Name.Contains(TEXT("Ranger"))) { return FLinearColor(0.32f, 0.65f, 0.49f); }
        if (Name.Contains(TEXT("Treasury")) || Name.Contains(TEXT("Heal"))) { return FLinearColor(0.25f, 0.64f, 0.64f); }
        if (Name.Contains(TEXT("Barracks")) || Name.Contains(TEXT("Swordsman"))) { return FLinearColor(0.76f, 0.38f, 0.29f); }
        if (Name.Contains(TEXT("Gate")) || Name.Contains(TEXT("Region"))) { return FLinearColor(0.55f, 0.63f, 0.38f); }
        if (Name.Contains(TEXT("Fireball"))) { return FLinearColor(0.87f, 0.41f, 0.19f); }
        return FLinearColor(0.38f, 0.56f, 0.73f);
    }
}
