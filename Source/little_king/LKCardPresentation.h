#pragma once
#include "CoreMinimal.h"
#include "LKTypes.h"
class ULKCardDefinition;
struct FLKUnitRow;

/** Shared content labels for native UI and existing Widget Blueprints. */
namespace LKCardPresentation
{
    constexpr int32 DeckCapacity = 8;
    FText QualityName(ELKQuality Quality);
    FLinearColor QualityColor(ELKQuality Quality);
    FText RaceName(ELKRace Race);
    FText SpellGradeName(ELKSpellGrade Grade);
    FText Classification(const ULKCardDefinition& Card);
    FLinearColor Color(const ULKCardDefinition& Card);
    FText Label(const ULKCardDefinition& Card);
    FText Detail(const ULKCardDefinition& Card, const FLKUnitRow* TunedRow = nullptr);
}
