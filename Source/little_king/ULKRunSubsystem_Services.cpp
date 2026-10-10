#include "ULKRunSubsystem.h"
#include "LKResearchContent.h"
#include "LKCardRules.h"
#include "LKBalanceRules.h"
#include "LKMarketContent.h"
#include "LKItemContent.h"
#include "LKLog.h"
#include "ULKProfileSubsystem.h"
#include "Engine/GameInstance.h"

TArray<FLKMarketOffer> ULKRunSubsystem::GetMarketOffers() const
{
    const FLKDungeonNode* N=FindNode(State.CurrentNodeId);
    return HasServiceNode() && N && N->Type==ELKDungeonNodeType::Market?N->MarketOffers:TArray<FLKMarketOffer>();
}
bool ULKRunSubsystem::SetDeckCapacityRule(int32 Min,int32 Max)
{
    if (!HasRunInProgress() || (State.Phase!=ELKRunPhase::ChoosingNode && State.Phase!=ELKRunPhase::ResolvingNode)
        || Min<1 || Max<Min || Max>64 || GetDeckCapacityUsed()>Max) { return false; }
    const FLKRunState Before=State; State.DeckCapacityMinimum=Min; State.DeckCapacityMaximum=Max;
    if (bAutoSaveEnabled && !SaveExpedition()) { State=Before; return false; } return true;
}
bool ULKRunSubsystem::ValidateRewardReplacement(int32 Index,const TArray<FName>& Removed,FString& Error) const
{
    if (!HasPendingRewardChoice() || !State.PendingRewardOffers.IsValidIndex(Index)) { Error=TEXT("奖励不可用"); return false; }
    return LKCardRules::ValidateReplacement(State.Cards,State.PendingRewardOffers[Index].CardId,Removed,State.DeckCapacityMinimum,State.DeckCapacityMaximum,Error);
}
bool ULKRunSubsystem::ValidateMarketPurchase(int32 Index,const TArray<FName>& Removed,FString& Error) const
{
    Error.Reset();
    const FLKDungeonNode* N=FindNode(State.CurrentNodeId);
    if (!HasServiceNode() || !N || N->Type!=ELKDungeonNodeType::Market || !N->MarketOffers.IsValidIndex(Index)) { Error=TEXT("市场商品不可用"); return false; }
    const FLKMarketOffer& O=N->MarketOffers[Index];
    if (!LKMarketContent::ValidateOffer(O,N->MarketGenerationVersion,Error)) { return false; }
    if (O.bSold || State.ProcessedWalletTransactions.Contains(O.OfferId) || O.Price<1 || State.WalletGold<O.Price) { Error=O.bSold?TEXT("商品已售出"):TEXT("金币不足或交易已处理"); return false; }
    if (LKMarketContent::IsTemporaryCard(O.Kind))
    {
        if (!LKCardRules::IsRunMarketCard(O.CardId) || State.Cards.ContainsByPredicate([&](const auto& C){return C.CardId==O.CardId;})) { Error=TEXT("该临时卡已持有或不可获取"); return false; }
        if (N->MarketGenerationVersion>=LKMarketContent::CurrentGenerationVersion
            && LKMarketContent::CardSilverCost(O.CardId,State.MarketSilverCosts)>State.BonusSnapshot.PlayerSilverCap)
        { Error=TEXT("临时卡费用超过本轮银币上限"); return false; }
        return LKCardRules::ValidateReplacement(State.Cards,O.CardId,Removed,State.DeckCapacityMinimum,State.DeckCapacityMaximum,Error);
    }
    if (!Removed.IsEmpty()) { Error=TEXT("道具与研究材料不替换手牌"); return false; }
    if (O.Kind==ELKMarketOfferKind::Item)
    {
        if (!State.bHomeRewardEligible) { Error=TEXT("本轮不参与家园结算，无法购买永久道具"); return false; }
        const ULKProfileSubsystem* Profile=ProfileSubsystemOverrideForTest.Get();
        if (!Profile && GetGameInstance()) { Profile=GetGameInstance()->GetSubsystem<ULKProfileSubsystem>(); }
        if (!Profile || !Profile->HasProfile() || Profile->GetProfile().ProfileId!=State.ProfileId)
        { Error=TEXT("对应家园道具仓库不可用，暂不能购买"); return false; }
        const auto& Owned=Profile->GetProfile().OwnedItems;
        if (Owned.Num()+State.CarriedItems.Num()>=10000)
        { Error=TEXT("家园道具仓库已满（含本轮预留），无法购买"); return false; }
        if (Owned.ContainsByPredicate([&](const auto& I){return I.InstanceId==O.ItemInstanceId;})
            || State.CarriedItems.ContainsByPredicate([&](const auto& I){return I.InstanceId==O.ItemInstanceId;}))
        { Error=TEXT("道具实例已持有"); return false; }
        return true;
    }
    if (LKMarketContent::IsResearch(O.Kind))
    {
        const auto* R=LKResearchContent::Find(O.CardId);
        if (!R || R->Kind!=O.Kind || State.CarriedResearchMaterials.FindRef(O.CardId)>=10000) { Error=TEXT("研究材料无效"); return false; }
        const ULKProfileSubsystem* Profile=ProfileSubsystemOverrideForTest.Get();
        if (!Profile && GetGameInstance()) { Profile=GetGameInstance()->GetSubsystem<ULKProfileSubsystem>(); }
        const bool bMatchingProfile=Profile && Profile->HasProfile() && Profile->GetProfile().ProfileId==State.ProfileId;
        if (bMatchingProfile && Profile->GetProfile().ResearchMaterials.FindRef(O.CardId)+State.CarriedResearchMaterials.FindRef(O.CardId)>=10000)
        { Error=TEXT("对应研究材料仓库已满（含本轮预留）"); return false; }
        if (N->MarketGenerationVersion>=LKMarketContent::CurrentGenerationVersion
            && (!State.bHomeRewardEligible || !bMatchingProfile || State.ResearchExclusions.Contains(O.CardId) || State.CarriedResearchMaterials.FindRef(O.CardId)>0))
        { Error=TEXT("已拥有或已携带对应研究内容"); return false; }
        return true;
    }
    Error=TEXT("未知商品类别"); return false;
}
bool ULKRunSubsystem::PurchaseMarketOffer(int32 Index,const TArray<FName>& Removed,FString& Error)
{
    if (!ValidateMarketPurchase(Index,Removed,Error)) { return false; }
    const FLKMarketOffer Offer=FindNode(State.CurrentNodeId)->MarketOffers[Index];
    const FLKRunState Before=State;
    State.WalletGold-=Offer.Price;
    FindNode(State.CurrentNodeId)->MarketOffers[Index].bSold=true;
    State.ProcessedWalletTransactions.AddUnique(Offer.OfferId);
    if (LKMarketContent::IsTemporaryCard(Offer.Kind))
    {
        State.Cards.RemoveAll([&](const auto& Card){return Removed.Contains(Card.CardId);});
        FLKRunCardState Card; Card.CardId=Offer.CardId; Card.bTemporaryAcquisition=true; State.Cards.Add(Card);
    }
    else if (Offer.Kind==ELKMarketOfferKind::Item)
    {
        FLKItemInstance Item; Item.InstanceId=Offer.ItemInstanceId; Item.ItemId=Offer.ItemId; Item.Quality=Offer.Quality;
        State.CarriedItems.Add(Item);
    }
    else { State.CarriedResearchMaterials.FindOrAdd(Offer.CardId)++; }
    if (bAutoSaveEnabled && !SaveExpedition()) { State=Before; Error=TEXT("交易保存失败，金币与物品均未改变"); return false; }
    UE_LOG(LogLK,Log,TEXT("[Market] purchased transaction=%s kind=%d card=%s item=%s price=%d wallet=%d temporary=%d carriedItems=%d"),
        *Offer.OfferId.ToString(),int32(Offer.Kind),*Offer.CardId.ToString(),*Offer.ItemId.ToString(),Offer.Price,State.WalletGold,
        LKMarketContent::IsTemporaryCard(Offer.Kind)?1:0,State.CarriedItems.Num());
    return true;
}
bool ULKRunSubsystem::UpgradeAtRest(FName Id,int32 Level,FString& Error)
{
    Error.Reset(); const FLKDungeonNode* N=FindNode(State.CurrentNodeId);
    if (!HasServiceNode() || !N || N->Type!=ELKDungeonNodeType::Rest) { Error=TEXT("只能在未结算的休息节点升级"); return false; }
    if (!LKCardRules::IsPlayerObtainable(Id) || (!LKCardRules::IsSpell(Id) && !LKCardRules::IsBuilding(Id))) { Error=TEXT("这里仅升级法术和建筑"); return false; }
    FLKRunCardState* C=State.Cards.FindByPredicate([Id](const auto& E){return E.CardId==Id;});
    if (!C || C->UpgradeLevel!=Level || Level<0 || Level>=50) { Error=TEXT("卡牌等级已变化或达到上限"); return false; }
    const FLKRunState Before=State; C->UpgradeLevel++;
    FindNode(State.CurrentNodeId)->bResolved=true; State.VisitedNodeIds.AddUnique(State.CurrentNodeId); State.Phase=ELKRunPhase::ChoosingNode;
    if (bAutoSaveEnabled && !SaveExpedition()) { State=Before; Error=TEXT("升级保存失败，休息机会未消耗"); return false; } return true;
}
bool ULKRunSubsystem::SaveForMenuExit()
{
    if (!HasRunInProgress()) { return false; }
    const FLKRunState Before=State;
    // State.Heroes/Cards and PendingBattle are pre-battle snapshots, never read active actors here.
    if (State.Phase==ELKRunPhase::InBattle)
    { State.Phase=ELKRunPhase::EnteringBattle; }
    if (bAutoSaveEnabled && !SaveExpedition()) { State=Before; return false; } return true;
}
