#include "ULKRunSubsystem.h"
#include "LKResearchContent.h"
#include "LKCardRules.h"
#include "LKBalanceRules.h"

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
bool ULKRunSubsystem::PurchaseMarketOffer(int32 Index,const TArray<FName>& Removed,FString& Error)
{
    Error.Reset();
    FLKDungeonNode* N=FindNode(State.CurrentNodeId);
    if (!HasServiceNode() || !N || N->Type!=ELKDungeonNodeType::Market || !N->MarketOffers.IsValidIndex(Index)) { Error=TEXT("市场商品不可用"); return false; }
    const FLKMarketOffer O=N->MarketOffers[Index];
    if (O.bSold || State.ProcessedWalletTransactions.Contains(O.OfferId) || O.Price<1 || State.WalletGold<O.Price) { Error=O.bSold?TEXT("商品已售出"):TEXT("金币不足或交易已处理"); return false; }
    TArray<FLKRunCardState> Cards=State.Cards;
    if (O.Kind==ELKMarketOfferKind::Mercenary)
    {
        if (!LKCardRules::IsTemporaryMercenary(O.CardId) || Cards.ContainsByPredicate([&](const auto& C){return C.CardId==O.CardId;})) { Error=TEXT("该佣兵已持有或不可获取"); return false; }
        if (!LKCardRules::ValidateReplacement(Cards,O.CardId,Removed,State.DeckCapacityMinimum,State.DeckCapacityMaximum,Error)) { return false; }
        Cards.RemoveAll([&](const auto& C){return Removed.Contains(C.CardId);}); FLKRunCardState C; C.CardId=O.CardId; Cards.Add(C);
    }
    else
    {
        const auto* R=LKResearchContent::Find(O.CardId);
        if (!Removed.IsEmpty() || !R || R->Kind!=O.Kind || State.CarriedResearchMaterials.FindRef(O.CardId)>=10000) { Error=TEXT("研究材料无效"); return false; }
    }
    const FLKRunState Before=State;
    State.WalletGold-=O.Price; N->MarketOffers[Index].bSold=true; State.ProcessedWalletTransactions.AddUnique(O.OfferId);
    if (O.Kind==ELKMarketOfferKind::Mercenary) { State.Cards=MoveTemp(Cards); }
    else { State.CarriedResearchMaterials.FindOrAdd(O.CardId)++; }
    if (bAutoSaveEnabled && !SaveExpedition()) { State=Before; Error=TEXT("交易保存失败，金币与物品均未改变"); return false; } return true;
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
