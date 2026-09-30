#pragma once
#include "CoreMinimal.h"
#include "LKRunTypes.h"
class ULKGameData;
class ULKCardDefinition;

struct FLKResearchDefinition
{
    FName CardId;
    FText Name;
    ELKMarketOfferKind Kind;
    ELKSpellGrade Grade;
    int32 MinimumRegionDepth;
    int32 Price;
    float Weight;
    int32 SilverCost;
    ELKSpellEffect Effect;
    float Value;
    float Radius;
    float HeroHealPercent;
};
namespace LKResearchContent
{
    const TArray<FLKResearchDefinition>& All();
    const FLKResearchDefinition* Find(FName CardId);
    void EnsureCards(ULKGameData& Data);
    void GenerateMarket(FLKDungeonNode& Node, int32 RunSeed, int32 RegionDepth);
    bool ValidateMaterials(const TMap<FName,int32>& Materials);
}
