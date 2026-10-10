#include "LKMarketPresentation.h"
#include "LKMarketContent.h"
#include "LKItemContent.h"
#include "LKCardPresentation.h"
#include "LKCardRules.h"
#include "LKResearchContent.h"
#include "LKUnitContent.h"
#include "ULKCardDefinition.h"

namespace LKMarketPresentation
{
FText OfferName(const FLKMarketOffer& Offer, const ULKCardDefinition* Card)
{
    if (Offer.Kind == ELKMarketOfferKind::Item)
    {
        const auto* Definition = LKItemContent::Find(Offer.ItemId);
        return Definition ? Definition->Name : FText::FromName(Offer.ItemId);
    }
    if (Card) { return Card->CardName; }
    if (const auto* Research = LKResearchContent::Find(Offer.CardId)) { return Research->Name; }
    if (const auto* Unit = LKUnitContent::Find(Offer.CardId)) { return Unit->DisplayName; }
    return FText::FromName(Offer.CardId);
}
FText Classification(const FLKMarketOffer& Offer, const ULKCardDefinition* Card)
{
    const FText Quality = Offer.Kind == ELKMarketOfferKind::Item || !Card
        ? LKCardPresentation::QualityName(Offer.Quality) : LKCardPresentation::Classification(*Card);
    return FText::FromString(LKMarketContent::KindName(Offer.Kind).ToString() + TEXT(" · ") + Quality.ToString());
}
FText ItemName(const FLKItemInstance& Item)
{
    const auto* Definition = LKItemContent::Find(Item.ItemId);
    return Definition ? Definition->Name : FText::FromName(Item.ItemId);
}
FText ItemDetail(const FLKItemInstance& Item)
{
    const auto* Definition = LKItemContent::Find(Item.ItemId);
    const TCHAR* Kind = Definition && Definition->Kind == ELKItemKind::Equipment ? TEXT("装备") : TEXT("饰品");
    FString Copy = FString::Printf(TEXT("%s\n%s · %s\n不占手牌容量 · 远征结束后带回家园"),
        *ItemName(Item).ToString(), *LKCardPresentation::QualityName(Item.Quality).ToString(), Kind);
    if (Definition)
    {
        Copy += TEXT("\n\n") + Definition->Description.ToString();
        if (Definition->bPlaceholder) { Copy += TEXT("\n占位道具 · 暂无属性，不提供战斗加成。品质不代表当前战力。"); }
    }
    return FText::FromString(Copy);
}
FText OfferDetail(const FLKMarketOffer& Offer, const ULKCardDefinition* Card, const FLKRunState& Run)
{
    FString Copy = FString::Printf(TEXT("%s\n%s\n售价 %d 金币 · 钱包 %d"), *OfferName(Offer, Card).ToString(),
        *Classification(Offer, Card).ToString(), Offer.Price, Run.WalletGold);
    if (Offer.Kind == ELKMarketOfferKind::Item)
    {
        FLKItemInstance Item; Item.ItemId = Offer.ItemId; Item.Quality = Offer.Quality;
        Copy += TEXT("\n\n") + ItemDetail(Item).ToString();
    }
    else
    {
        Copy += LKMarketContent::IsTemporaryCard(Offer.Kind)
            ? TEXT("\n\n仅本次远征可用；购买后加入循环牌组。不会永久解锁，远征结束后移除。")
            : TEXT("\n\n购买的是研究材料，不会立即加入手牌。远征结束后带回图书馆研究，永久解锁对应卡牌。");
        if (Card)
        {
            Copy += TEXT("\n\n") + LKCardPresentation::Detail(*Card).ToString();
            const int32 SilverCost=LKMarketContent::CardSilverCost(Offer.CardId,Run.MarketSilverCosts);
            if (float(SilverCost) > Run.BonusSnapshot.PlayerSilverCap)
            { Copy += LKMarketContent::IsTemporaryCard(Offer.Kind)
                ? TEXT("\n本轮银币上限不足以打出这张临时牌；旧店库存保留原商品，请谨慎购买。")
                : TEXT("\n当前远征银币上限不足；解锁后仍需提升金库才能使用。"); }
        }
    }
    if (Offer.bSold) { Copy += TEXT("\n\n该商品已售出。"); }
    else if (Run.WalletGold < Offer.Price)
    { Copy += FString::Printf(TEXT("\n\n还差 %d 金币。"), Offer.Price - Run.WalletGold); }
    return FText::FromString(Copy);
}
FText PurchaseMessage(ELKMarketOfferKind Kind)
{
    return FText::FromString(LKMarketContent::IsTemporaryCard(Kind) ? TEXT("已购入本轮牌组，购买已保存。")
        : Kind == ELKMarketOfferKind::Item ? TEXT("道具已放入远征背包，结束后带回家园。")
        : TEXT("研究材料已收入行囊，结束后带回图书馆。"));
}
}
