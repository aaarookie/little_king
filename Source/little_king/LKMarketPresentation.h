#pragma once
#include "CoreMinimal.h"
#include "LKRunTypes.h"
class ULKCardDefinition;

/** Presentation only. Purchase eligibility stays in ULKRunSubsystem. */
namespace LKMarketPresentation
{
    FText OfferName(const FLKMarketOffer& Offer, const ULKCardDefinition* Card);
    FText Classification(const FLKMarketOffer& Offer, const ULKCardDefinition* Card);
    FText OfferDetail(const FLKMarketOffer& Offer, const ULKCardDefinition* Card, const FLKRunState& Run);
    FText ItemName(const FLKItemInstance& Item);
    FText ItemDetail(const FLKItemInstance& Item);
    FText PurchaseMessage(ELKMarketOfferKind Kind);
}
