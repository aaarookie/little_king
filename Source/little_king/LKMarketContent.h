#pragma once
#include "CoreMinimal.h"
#include "LKRunTypes.h"

namespace LKMarketContent
{
    constexpr int32 CurrentGenerationVersion = 2;
    constexpr int32 MaximumOffers = 5;
    ELKMarketCategory CategoryOf(ELKMarketOfferKind Kind);
    bool IsTemporaryCard(ELKMarketOfferKind Kind);
    bool IsResearch(ELKMarketOfferKind Kind);
    /** New generation only. Already generated node inventories are immutable. */
    void Generate(FLKDungeonNode& Node, int32 RunSeed, int32 RegionDepth,
        float SilverCap, const TArray<FName>& ResearchExclusions,
        const TMap<FName,int32>& CarriedResearchMaterials, const TArray<FLKRunCardState>& Cards,
        const TMap<FName,int32>& SilverCosts = TMap<FName,int32>(), FGuid RunId = FGuid());
    int32 CardSilverCost(FName CardId, const TMap<FName,int32>& SilverCosts);
    bool ValidateOffer(const FLKMarketOffer& Offer, int32 GenerationVersion, FString& Error);
    FText KindName(ELKMarketOfferKind Kind);
}
