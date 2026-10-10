#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "LKWorldMapTestHelpers.h"
#include "../LKMarketContent.h"
#include "../LKItemContent.h"
#include "../LKResearchContent.h"
#include "../LKCardRules.h"
#include "../LKExpeditionMercenaryContent.h"
#include "../LKBalanceRules.h"
#include "../ULKRunSaveGame.h"
#include "../ULKProfileSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

namespace
{
constexpr auto MarketBackendFlags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;
FLKDungeonNode FreshMarket(FName Id=TEXT("World_R0_L5_N0"))
{ FLKDungeonNode N; N.NodeId=Id; N.Type=ELKDungeonNodeType::Market; return N; }
const FLKMarketOffer* OfferOf(const FLKDungeonNode& N,ELKMarketOfferKind Kind)
{ return N.MarketOffers.FindByPredicate([Kind](const auto& O){return O.Kind==Kind;}); }
const FLKMarketOffer* ResearchOf(const FLKDungeonNode& N)
{ return N.MarketOffers.FindByPredicate([](const auto& O){return LKMarketContent::IsResearch(O.Kind);}); }
struct FMarketScope
{
    FString Base=TEXT("LittleKing_MarketBackend_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
    ULKProfileSubsystem* Profile=nullptr;
    ULKRunSubsystem* Run=nullptr;
    FMarketScope()
    {
        ULKProfileSubsystem::SetPersistentProfileEnabledForTest(true);
        ULKProfileSubsystem::SetSlotNameOverrideForTest(Base);
        ULKRunSubsystem::SetRunSlotNameOverrideForTest(Base+TEXT("_Run"));
        Profile=NewObject<ULKProfileSubsystem>(NewObject<UGameInstance>()); Profile->ConfigureStorage(Base); Profile->EnsureProfile();
        Run=NewObject<ULKRunSubsystem>(NewObject<UGameInstance>()); Run->ConfigureStorage(Base+TEXT("_Run")); Run->SetProfileSubsystemOverrideForTest(Profile);
    }
    ~FMarketScope()
    {
        for (const auto& S:{Base+TEXT("_A"),Base+TEXT("_B"),Base+TEXT("_Run"),Base+TEXT("_Run_Pending"),Base+TEXT("_Fixture"),Base+TEXT("_Fixture_Pending")})
        { if (UGameplayStatics::DoesSaveGameExist(S,0)) { UGameplayStatics::DeleteGameInSlot(S,0); } }
        ULKProfileSubsystem::ResetTestHooks(); ULKRunSubsystem::SetRunSlotNameOverrideForTest(FString());
    }
    bool Start()
    { FLKExpeditionStartRequest R; if (!LKWorldMapTest::Request(Profile,R)) { return false; } R.Seed=9112026; return Run->StartNewRun(R); }
    bool Restore(const FLKRunState& State)
    { auto* Save=NewObject<ULKRunSaveGame>(); Save->RunState=State; return UGameplayStatics::SaveGameToSlot(Save,Base+TEXT("_Fixture"),0) && Run->LoadExpeditionFromSlot(Base+TEXT("_Fixture"),true); }
    FLKRunState MarketState()
    {
        FLKRunState S=Run->GetRunState();
        auto* N=S.Nodes.FindByPredicate([&](const auto& Node){return Node.Type==ELKDungeonNodeType::Market && Node.RegionId==S.WorldRegions[0].RegionId;});
        check(N); S.CurrentNodeId=N->NodeId; S.RegionId=N->RegionId; S.Phase=ELKRunPhase::ResolvingNode; S.PendingBattle=FLKBattleContext(); S.WalletGold=500; N->bResolved=false;
        LKMarketContent::Generate(*N,S.Seed,0,S.BonusSnapshot.PlayerSilverCap,S.ResearchExclusions,S.CarriedResearchMaterials,S.Cards,S.MarketSilverCosts,S.RunId);
        return S;
    }
};
TArray<FName> Replacement(const FLKRunState& State,FName Incoming)
{
    // Find a legal real-rule choice; separate capacity tests check the optimization rule itself.
    for (uint32 Mask=0;Mask<(1u<<State.Cards.Num());++Mask)
    {
        TArray<FName> Removed; for (int32 I=0;I<State.Cards.Num();++I) { if (Mask&(1u<<I)) { Removed.Add(State.Cards[I].CardId); } }
        FString Error; if (LKCardRules::ValidateReplacement(State.Cards,Incoming,Removed,State.DeckCapacityMinimum,State.DeckCapacityMaximum,Error)) { return Removed; }
    }
    return {};
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKMarketStageAudit,"LittleKing.Market.Generation.StageProbabilityAudit",MarketBackendFlags)
bool FLKMarketStageAudit::RunTest(const FString&)
{
    constexpr int32 Samples=4000;
    const float ExpectedMerc[4][3]={{.8f,.2f,0},{.6f,.35f,.05f},{.4f,.45f,.15f},{.25f,.5f,.25f}};
    const float ExpectedItem[4][5]={{.8f,.2f,0,0,0},{.55f,.35f,.1f,0,0},{.25f,.4f,.3f,.05f,0},{.1f,.25f,.4f,.2f,.05f}};
    TArray<TSharedPtr<FJsonValue>> Stages;
    for (int32 Depth=0;Depth<4;++Depth)
    {
        int32 Merc[3]={},Items[5]={},ResearchCount=0,MinResearch=MAX_int32,MaxResearch=0,MaxCardPrice=0;
        bool bValid=true,bSilverValid=true,bDistinct=true,bCategories=true,bStageResearch=true;
        for (int32 Seed=100000;Seed<100000+Samples;++Seed)
        {
            auto N=FreshMarket(); LKMarketContent::Generate(N,Seed,Depth,20.f,{},{},{});
            bCategories&=N.MarketOffers.Num()==5 && OfferOf(N,ELKMarketOfferKind::Mercenary) && OfferOf(N,ELKMarketOfferKind::TemporarySpell) && OfferOf(N,ELKMarketOfferKind::TemporaryBuilding) && OfferOf(N,ELKMarketOfferKind::Item) && ResearchOf(N);
            TSet<FName> CardIds; TSet<FGuid> Transactions;
            for (const auto& O:N.MarketOffers)
            {
                FString Error; bValid&=LKMarketContent::ValidateOffer(O,N.MarketGenerationVersion,Error);
                bDistinct&=!Transactions.Contains(O.OfferId); Transactions.Add(O.OfferId);
                if (LKMarketContent::IsTemporaryCard(O.Kind))
                { bDistinct&=!CardIds.Contains(O.CardId); CardIds.Add(O.CardId); bSilverValid&=LKMarketContent::CardSilverCost(O.CardId,{})<=20; MaxCardPrice=FMath::Max(MaxCardPrice,O.Price); }
                if (O.Kind==ELKMarketOfferKind::Mercenary) { const int32 Tier=int32(O.Quality)-1; if (Tier>=0 && Tier<3) { ++Merc[Tier]; } else { bValid=false; } }
                if (O.Kind==ELKMarketOfferKind::Item) { if (int32(O.Quality)<5) { ++Items[int32(O.Quality)]; } else { bValid=false; } bValid&=O.Price>=5 && O.Price<=17; }
                if (LKMarketContent::IsResearch(O.Kind))
                { const auto* R=LKResearchContent::Find(O.CardId); bStageResearch&=R && R->MinimumRegionDepth==Depth; ++ResearchCount; MinResearch=FMath::Min(MinResearch,O.Price); MaxResearch=FMath::Max(MaxResearch,O.Price); }
            }
            auto Limited=FreshMarket(); LKMarketContent::Generate(Limited,Seed,Depth,5.f,{},{},{});
            for (const auto& O:Limited.MarketOffers) { if (LKMarketContent::IsTemporaryCard(O.Kind)) { bSilverValid&=LKMarketContent::CardSilverCost(O.CardId,{})<=5; } }
        }
        TestTrue(TEXT("Every generated offer has valid independent identity"),bValid && bDistinct);
        TestTrue(TEXT("One mercenary, spell, building, item and research at full eligibility"),bCategories);
        TestTrue(TEXT("Silver filtering makes temporary cards castable"),bSilverValid);
        TestTrue(TEXT("Research targets latest available geographic stage"),bStageResearch);
        TestEqual(TEXT("One pursuable research opportunity per market"),ResearchCount,Samples);
        TestTrue(TEXT("Research range is reachable"),MinResearch>=25 && MaxResearch<=180 && (Depth!=0 || MaxResearch<=40));
        TArray<TSharedPtr<FJsonValue>> MercCounts,ItemCounts,MercRates,ItemRates;
        for (int32 I=0;I<3;++I)
        { const float Rate=float(Merc[I])/Samples; TestTrue(TEXT("Mercenary quality mass agrees with configured stage within 3 percentage points"),FMath::Abs(Rate-ExpectedMerc[Depth][I])<.03f); MercCounts.Add(MakeShared<FJsonValueNumber>(Merc[I])); MercRates.Add(MakeShared<FJsonValueNumber>(Rate)); }
        for (int32 I=0;I<5;++I)
        { const float Rate=float(Items[I])/Samples; TestTrue(TEXT("Item quality mass agrees with stage within 3 percentage points"),FMath::Abs(Rate-ExpectedItem[Depth][I])<.03f); ItemCounts.Add(MakeShared<FJsonValueNumber>(Items[I])); ItemRates.Add(MakeShared<FJsonValueNumber>(Rate)); }
        auto Stage=MakeShared<FJsonObject>(); Stage->SetNumberField(TEXT("regionDepth"),Depth); Stage->SetArrayField(TEXT("mercenaryCountsUncommonRareEpic"),MercCounts); Stage->SetArrayField(TEXT("mercenaryRates"),MercRates); Stage->SetArrayField(TEXT("itemCountsCommonThroughLegendary"),ItemCounts); Stage->SetArrayField(TEXT("itemRates"),ItemRates);
        Stage->SetNumberField(TEXT("researchOffers"),ResearchCount); Stage->SetNumberField(TEXT("researchMinPrice"),MinResearch); Stage->SetNumberField(TEXT("researchMaxPrice"),MaxResearch); Stage->SetNumberField(TEXT("temporaryMaxPrice"),MaxCardPrice); Stage->SetBoolField(TEXT("silverCapFilterValid"),bSilverValid); Stages.Add(MakeShared<FJsonValueObject>(Stage));
        AddInfo(FString::Printf(TEXT("Stage %d / %d fixed seeds: mercenary %d/%d/%d, research %d price %d-%d"),Depth,Samples,Merc[0],Merc[1],Merc[2],ResearchCount,MinResearch,MaxResearch));
    }
    auto Audit=MakeShared<FJsonObject>(); Audit->SetNumberField(TEXT("generationVersion"),LKMarketContent::CurrentGenerationVersion); Audit->SetNumberField(TEXT("seedStart"),100000); Audit->SetNumberField(TEXT("samplesPerDepth"),Samples); Audit->SetNumberField(TEXT("unfilteredSilverCap"),20); Audit->SetNumberField(TEXT("filteredSilverCap"),5); Audit->SetStringField(TEXT("scope"),TEXT("Empty ownership pools; generation audit, not player win-rate evidence. Prices are gold; card affordability filter is battle silver cap.")); Audit->SetArrayField(TEXT("stages"),Stages);
    FString Json; const auto Writer=TJsonWriterFactory<>::Create(&Json); FJsonSerializer::Serialize(Audit,Writer);
    const FString Directory=FPaths::ProjectSavedDir()/TEXT("Optimization"); IFileManager::Get().MakeDirectory(*Directory,true);
    TestTrue(TEXT("Audit JSON saved after actual generation"),FFileHelper::SaveStringToFile(Json,*(Directory/TEXT("MarketAudit.json"))));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKMarketIndependentStreams,"LittleKing.Market.Generation.FrozenStreamsAndOwnership",MarketBackendFlags)
bool FLKMarketIndependentStreams::RunTest(const FString&)
{
    const FGuid RunId(1,2,3,4); auto A=FreshMarket(),B=FreshMarket();
    LKMarketContent::Generate(A,915,2,20.f,{},{},{},{},RunId);
    TArray<FLKRunCardState> Held; for (const auto& D:LKExpeditionMercenaryContent::All()) { FLKRunCardState C; C.CardId=D.Unit.UnitId; Held.Add(C); }
    LKMarketContent::Generate(B,915,2,2.f,{},{},Held,{},RunId);
    const auto* ItemA=OfferOf(A,ELKMarketOfferKind::Item); const auto* ItemB=OfferOf(B,ELKMarketOfferKind::Item); const auto* ResearchA=ResearchOf(A); const auto* ResearchB=ResearchOf(B);
    if (!TestNotNull(TEXT("Independent item"),ItemA) || !TestNotNull(TEXT("Filtered independent item"),ItemB) || !TestNotNull(TEXT("Independent research"),ResearchA) || !TestNotNull(TEXT("Filtered research"),ResearchB)) { return false; }
    TestTrue(TEXT("Changing temporary eligibility preserves item content and identity"),ItemA->ItemId==ItemB->ItemId && ItemA->Quality==ItemB->Quality && ItemA->ItemInstanceId==ItemB->ItemInstanceId);
    TestTrue(TEXT("Changing temporary eligibility preserves research and identity"),ResearchA->CardId==ResearchB->CardId && ResearchA->OfferId==ResearchB->OfferId);
    auto OtherRun=FreshMarket(); LKMarketContent::Generate(OtherRun,915,2,20.f,{},{},{},{},FGuid(5,6,7,8));
    TestTrue(TEXT("Same seed, new run retains content but changes permanent instance identity"),OfferOf(OtherRun,ELKMarketOfferKind::Item)->ItemId==ItemA->ItemId && OfferOf(OtherRun,ELKMarketOfferKind::Item)->ItemInstanceId!=ItemA->ItemInstanceId);
    auto NearRun=FreshMarket(); LKMarketContent::Generate(NearRun,915,2,20.f,{},{},{},{},FGuid(1,2,3,5));
    bool bContentStable=A.MarketOffers.Num()==NearRun.MarketOffers.Num(),bIdentitiesDifferent=bContentStable;
    for (int32 I=0;I<A.MarketOffers.Num() && I<NearRun.MarketOffers.Num();++I)
    { const auto& X=A.MarketOffers[I]; const auto& Y=NearRun.MarketOffers[I]; bContentStable&=X.Kind==Y.Kind && X.CardId==Y.CardId && X.ItemId==Y.ItemId && X.Price==Y.Price && X.Quality==Y.Quality; bIdentitiesDifferent&=X.OfferId!=Y.OfferId; }
    TestTrue(TEXT("Only a RunId bit changes all transaction identities while every content field stays stable"),bContentStable && bIdentitiesDifferent);
    auto OtherNode=FreshMarket(TEXT("World_R0_L5_N1")); LKMarketContent::Generate(OtherNode,915,2,20.f,{},{},{},{},RunId);
    TestTrue(TEXT("Same seed and run, complete node ID gives a distinct item and transaction identity"),OfferOf(OtherNode,ELKMarketOfferKind::Item)->ItemInstanceId!=ItemA->ItemInstanceId && OfferOf(OtherNode,ELKMarketOfferKind::Item)->OfferId!=ItemA->OfferId);
    TestTrue(TEXT("Item ownership identity is distinct from its purchase identity"),ItemA->ItemInstanceId!=ItemA->OfferId);
    auto Same=FreshMarket(); LKMarketContent::Generate(Same,915,2,20.f,{},{},{},{},RunId);
    TestTrue(TEXT("Complete identity tuple is deterministic"),OfferOf(Same,ELKMarketOfferKind::Item)->ItemInstanceId==ItemA->ItemInstanceId && OfferOf(Same,ELKMarketOfferKind::Item)->OfferId==ItemA->OfferId);
    const auto Frozen=A.MarketOffers; A.MarketOffers[0].bSold=true;
    LKMarketContent::Generate(A,99,0,1.f,{ResearchA->CardId},{},Held);
    TestTrue(TEXT("Reopening cannot reroll or reset sold snapshot"),A.MarketOffers.Num()==Frozen.Num() && A.MarketOffers[0].OfferId==Frozen[0].OfferId && A.MarketOffers[0].Price==Frozen[0].Price && A.MarketOffers[0].bSold);
    TMap<FName,int32> Carried; Carried.Add(ResearchA->CardId,1); auto Next=FreshMarket(TEXT("World_R0_L11_N0")); LKMarketContent::Generate(Next,915,2,20.f,{},Carried,{},{},RunId);
    TestTrue(TEXT("Next market excludes already carried research"),ResearchOf(Next) && ResearchOf(Next)->CardId!=ResearchA->CardId);
    TArray<FName> Exclusions; for (const auto& R:LKResearchContent::All()) { if (R.MinimumRegionDepth==2) { Exclusions.Add(R.CardId); } }
    auto Fallback=FreshMarket(); LKMarketContent::Generate(Fallback,915,2,20.f,Exclusions,{},{});
    TestTrue(TEXT("Owned newest stage explicitly falls back to lower unknown research"),ResearchOf(Fallback) && LKResearchContent::Find(ResearchOf(Fallback)->CardId)->MinimumRegionDepth==1);
    Exclusions.Reset(); for (const auto& R:LKResearchContent::All()) { Exclusions.Add(R.CardId); }
    auto Complete=FreshMarket(); LKMarketContent::Generate(Complete,915,3,20.f,Exclusions,{},{});
    TestFalse(TEXT("All research owned means an empty research category"),ResearchOf(Complete)!=nullptr);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKMarketTransactions,"LittleKing.Market.Transactions.ThreeCategoriesSaveAndSettlement",MarketBackendFlags)
bool FLKMarketTransactions::RunTest(const FString&)
{
    FMarketScope Scope; if (!TestTrue(TEXT("Start isolated formal run"),Scope.Start())) { return false; }
    TestTrue(TEXT("Future markets are generated only when first visited"),Scope.Run->GetRunState().Nodes.FilterByPredicate([](const auto& N){return N.Type==ELKDungeonNodeType::Market && N.bMarketGenerated;}).IsEmpty());
    const auto Fixture=Scope.MarketState(); if (!TestTrue(TEXT("Load current market"),Scope.Restore(Fixture))) { return false; }
    auto Offers=Scope.Run->GetMarketOffers(); FString Error;
    const int32 ItemIndex=Offers.IndexOfByPredicate([](const auto& O){return O.Kind==ELKMarketOfferKind::Item;});
    const int32 SpellIndex=Offers.IndexOfByPredicate([](const auto& O){return O.Kind==ELKMarketOfferKind::TemporarySpell;});
    const int32 BuildingIndex=Offers.IndexOfByPredicate([](const auto& O){return O.Kind==ELKMarketOfferKind::TemporaryBuilding;});
    const int32 ResearchIndex=Offers.IndexOfByPredicate([](const auto& O){return LKMarketContent::IsResearch(O.Kind);});
    if (!TestTrue(TEXT("Three categories exist"),ItemIndex>=0 && SpellIndex>=0 && BuildingIndex>=0 && ResearchIndex>=0)) { return false; }
    const auto Permanent=Scope.Profile->GetProfile().UnlockedCardIds;
    TestFalse(TEXT("Items cannot discard a card"),Scope.Run->ValidateMarketPurchase(ItemIndex,{"Unit_Swordsman"},Error));
    TestTrue(TEXT("Buy independent placeholder item"),Scope.Run->PurchaseMarketOffer(ItemIndex,{},Error));
    TestEqual(TEXT("Item does not take hand slots"),Scope.Run->GetRunState().Cards.Num(),Fixture.Cards.Num());
    TestTrue(TEXT("Buy a temporary spell"),Scope.Run->PurchaseMarketOffer(SpellIndex,Replacement(Scope.Run->GetRunState(),Offers[SpellIndex].CardId),Error));
    TestTrue(TEXT("Buy a temporary building through replacement rules"),Scope.Run->PurchaseMarketOffer(BuildingIndex,Replacement(Scope.Run->GetRunState(),Offers[BuildingIndex].CardId),Error));
    const auto Cards=Scope.Run->GetRunState().Cards;
    TestTrue(TEXT("Both temporary acquisitions are retained with lifetime metadata"),Cards.ContainsByPredicate([&](const auto& C){return C.CardId==Offers[SpellIndex].CardId && C.bTemporaryAcquisition;}) && Cards.ContainsByPredicate([&](const auto& C){return C.CardId==Offers[BuildingIndex].CardId && C.bTemporaryAcquisition;}));
    TestTrue(TEXT("Buy separate research material"),Scope.Run->PurchaseMarketOffer(ResearchIndex,{},Error));
    TestTrue(TEXT("Temporary purchases do not grant permanent unlocks"),Scope.Profile->GetProfile().UnlockedCardIds==Permanent);
    const int32 Charged=Offers[ItemIndex].Price+Offers[SpellIndex].Price+Offers[BuildingIndex].Price+Offers[ResearchIndex].Price;
    TestEqual(TEXT("Exactly four purchase prices charged"),Scope.Run->GetWalletGold(),500-Charged);
    const auto Item=Scope.Run->GetCarriedItems()[0];
    TestTrue(TEXT("Cold reload restores purchases"),Scope.Run->LoadExpeditionFromSlot(Scope.Base+TEXT("_Run"),true));
    TestEqual(TEXT("Carried instance survives save"),Scope.Run->GetCarriedItems()[0].InstanceId,Item.InstanceId);
    TestFalse(TEXT("Reload cannot consume purchase twice"),Scope.Run->PurchaseMarketOffer(ItemIndex,{},Error));
    TestTrue(TEXT("Abandon carries inventory home"),Scope.Run->AbandonCurrentRun());
    TestEqual(TEXT("One owned item at home"),Scope.Profile->GetOwnedItems().Num(),1);
    TestEqual(TEXT("Home retains stable ItemId and instance"),Scope.Profile->GetOwnedItems()[0].InstanceId,Item.InstanceId);
    TestEqual(TEXT("Unspent gold returns"),Scope.Profile->GetGold(),500-Charged);
    TestEqual(TEXT("Research material returns independently"),Scope.Profile->GetProfile().ResearchMaterials.FindRef(Offers[ResearchIndex].CardId),1);
    TestTrue(TEXT("Settlement retry is idempotent"),Scope.Run->ApplyPendingSettlementToProfile());
    TestEqual(TEXT("Retry adds no duplicate item"),Scope.Profile->GetOwnedItems().Num(),1);
    TestTrue(TEXT("Only home research permanently unlocks purchased book/blueprint"),Scope.Profile->ResearchCard(Offers[ResearchIndex].CardId,Error));
    TestTrue(TEXT("Home profile cold reload retains item"),Scope.Profile->TouchSelectedProfile());
    auto* Reload=NewObject<ULKProfileSubsystem>(NewObject<UGameInstance>()); Reload->ConfigureStorage(Scope.Base);
    TestTrue(TEXT("Load profile with owned item"),Reload->EnsureProfile());
    TestEqual(TEXT("Profile persisted stable instance"),Reload->GetOwnedItems()[0].InstanceId,Item.InstanceId);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKMarketRollback,"LittleKing.Market.Transactions.RollbackAndWarehouseReservation",MarketBackendFlags)
bool FLKMarketRollback::RunTest(const FString&)
{
    FMarketScope Scope; if (!TestTrue(TEXT("Start"),Scope.Start())) { return false; }
    const auto Fixture=Scope.MarketState(); if (!Scope.Restore(Fixture)) { return false; }
    const auto Offers=Scope.Run->GetMarketOffers(); const int32 ItemIndex=Offers.IndexOfByPredicate([](const auto& O){return O.Kind==ELKMarketOfferKind::Item;}); FString Error;
    if (!TestTrue(TEXT("Item fixture"),ItemIndex>=0)) { return false; }
    Scope.Run->ConfigureStorage(Scope.Base+TEXT("?:/Unwritable")); TestTrue(TEXT("Load into blocked storage"),Scope.Run->LoadExpeditionFromSlot(Scope.Base+TEXT("_Fixture"),true));
    TestFalse(TEXT("Failed save rejects transaction"),Scope.Run->PurchaseMarketOffer(ItemIndex,{},Error));
    TestTrue(TEXT("Explicit rollback explanation"),Error.Contains(TEXT("保存失败")));
    TestEqual(TEXT("Wallet rolled back"),Scope.Run->GetWalletGold(),Fixture.WalletGold);
    TestTrue(TEXT("Inventory, sold status and processed identity rolled back"),Scope.Run->GetCarriedItems().IsEmpty() && !Scope.Run->GetMarketOffers()[ItemIndex].bSold && Scope.Run->GetRunState().ProcessedWalletTransactions==Fixture.ProcessedWalletTransactions);
    Scope.Run->ConfigureStorage(Scope.Base+TEXT("_Run")); TestTrue(TEXT("Restore valid storage"),Scope.Restore(Fixture));
    auto Profile=Scope.Profile->GetProfile();
    for (int32 I=0;I<9999;++I) { FLKItemInstance Item; Item.ItemId=TEXT("Item_Accessory01"); Item.InstanceId=FGuid(0xABC,0,1,uint32(I+1)); Profile.OwnedItems.Add(Item); }
    Scope.Profile->SetProfileForTest(Profile); Scope.Profile->SetAutoSaveEnabled(false);
    TestTrue(TEXT("9999 warehouse allows exactly one reservation"),Scope.Run->PurchaseMarketOffer(ItemIndex,{},Error));
    auto Second=Scope.Run->GetRunState(); auto* Node=Second.Nodes.FindByPredicate([&](const auto& N){return N.NodeId==Second.CurrentNodeId;});
    FLKMarketOffer Extra=Offers[ItemIndex]; Extra.OfferId=FGuid::NewGuid(); Extra.ItemInstanceId=FGuid::NewGuid(); Extra.bSold=false;
    // Replace an unsold temporary slot: remain within the serialized five-offer limit.
    const int32 ReplaceIndex=Node->MarketOffers.IndexOfByPredicate([](const auto& O){return O.Kind==ELKMarketOfferKind::Mercenary;}); Node->MarketOffers[ReplaceIndex]=Extra;
    TestTrue(TEXT("Restore second legal independent item"),Scope.Restore(Second));
    const int32 Before=Scope.Run->GetWalletGold(); TestFalse(TEXT("Second item rejected by owned plus carried reservation"),Scope.Run->PurchaseMarketOffer(ReplaceIndex,{},Error));
    TestTrue(TEXT("Warehouse reservation reason exposed to UI"),Error.Contains(TEXT("本轮预留")));
    TestEqual(TEXT("Capacity refusal does not spend gold"),Scope.Run->GetWalletGold(),Before);
    const int32 ResearchIndex=Scope.Run->GetMarketOffers().IndexOfByPredicate([](const auto& O){return LKMarketContent::IsResearch(O.Kind);});
    TestTrue(TEXT("Warehouse limit does not block research purchase"),Scope.Run->PurchaseMarketOffer(ResearchIndex,{},Error));
    TestTrue(TEXT("Legal reservation settles gold and materials without deadlock"),Scope.Run->AbandonCurrentRun());
    TestEqual(TEXT("Warehouse ends exactly at limit"),Scope.Profile->GetOwnedItems().Num(),10000);
    TestTrue(TEXT("Material settlement not lost to warehouse limit"),!Scope.Profile->GetProfile().ResearchMaterials.IsEmpty());
    FLKSettlementReceipt Duplicate=Scope.Run->GetPendingSettlement(); Duplicate.SettlementId=FGuid::NewGuid();
    const int32 Gold=Scope.Profile->GetGold(); TestEqual(TEXT("Another receipt cannot duplicate owned identity"),Scope.Profile->ApplySettlement(Duplicate),ELKSettlementResult::IdentityMismatch); TestEqual(TEXT("Rejected identity preserves gold"),Scope.Profile->GetGold(),Gold);
    Duplicate=Scope.Run->GetPendingSettlement(); Duplicate.SettlementId=FGuid::NewGuid(); Duplicate.bEligibleForHomeReward=false;
    TestEqual(TEXT("Authority rejects ineligible settlement without consuming receipt"),Scope.Profile->ApplySettlement(Duplicate),ELKSettlementResult::NoPendingSettlement);
    TestFalse(TEXT("Ineligible receipt not recorded"),Scope.Profile->HasProcessedSettlement(Duplicate.SettlementId));
    Duplicate.bEligibleForHomeReward=true; Duplicate.ProfileId.Invalidate();
    TestEqual(TEXT("Eligible receipt must name its owning profile"),Scope.Profile->ApplySettlement(Duplicate),ELKSettlementResult::IdentityMismatch);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKProfileReadBackCleanup,"LittleKing.Market.Save.ProfileReadBackFailureCleanup",MarketBackendFlags)
bool FLKProfileReadBackCleanup::RunTest(const FString&)
{
    FMarketScope Scope;
    auto P=Scope.Profile->GetProfile(); P.Gold=77; P.ResearchMaterials.Add(TEXT("Spell_Freeze"),1); Scope.Profile->SetProfileForTest(P);
    if (!TestTrue(TEXT("Commit isolated profile with material"),Scope.Profile->SaveForTest())) { return false; }
    const auto Before=Scope.Profile->GetProfile(); FString Error;
    Scope.Profile->FailNextReadBackForTest(); TestFalse(TEXT("Transient read failure rejects research after actual candidate write"),Scope.Profile->ResearchCard(TEXT("Spell_Freeze"),Error));
    TestTrue(TEXT("Research failure identifies read-back failure"),Error.Contains(TEXT("读回")));
    TestTrue(TEXT("Memory retains old revision, material and locked identity"),Scope.Profile->GetProfile().Revision==Before.Revision && Scope.Profile->GetProfile().ResearchMaterials.FindRef(TEXT("Spell_Freeze"))==1 && !Scope.Profile->IsCardUnlocked(TEXT("Spell_Freeze")));
    const int32 RemainingSlots=int32(UGameplayStatics::DoesSaveGameExist(Scope.Base+TEXT("_A"),0))+int32(UGameplayStatics::DoesSaveGameExist(Scope.Base+TEXT("_B"),0));
    TestEqual(TEXT("Failed candidate removed while one committed slot remains"),RemainingSlots,1);
    auto ColdProfile=[&]()
    { auto* Reload=NewObject<ULKProfileSubsystem>(NewObject<UGameInstance>()); Reload->ConfigureStorage(Scope.Base); TestTrue(TEXT("Cold load old good slot"),Reload->EnsureProfile()); return Reload; };
    auto* Reload=ColdProfile(); TestTrue(TEXT("Cold load cannot resurrect failed research"),Reload->GetProfile().Revision==Before.Revision && Reload->GetProfile().ResearchMaterials.FindRef(TEXT("Spell_Freeze"))==1 && !Reload->IsCardUnlocked(TEXT("Spell_Freeze")));
    Scope.Profile->FailNextReadBackForTest(); TestFalse(TEXT("Transient read failure rolls back gold mutation"),Scope.Profile->AddGold(5));
    TestEqual(TEXT("Failed gold mutation cannot reappear on cold load"),ColdProfile()->GetGold(),77);
    FLKSettlementReceipt Receipt; Receipt.SettlementId=FGuid::NewGuid(); Receipt.RunId=FGuid::NewGuid(); Receipt.ProfileId=Before.ProfileId; Receipt.GoldAmount=23; Receipt.TerminalPhase=ELKRunPhase::Completed; Receipt.ResearchMaterials.Add(TEXT("Spell_BlackCloud"),1);
    FLKItemInstance Item; Item.ItemId=TEXT("Item_Accessory01"); Item.InstanceId=FGuid::NewGuid(); Receipt.Items.Add(Item);
    Scope.Profile->FailNextReadBackForTest(); TestEqual(TEXT("Read failure rejects settlement candidate"),Scope.Profile->ApplySettlement(Receipt),ELKSettlementResult::SaveFailed);
    Reload=ColdProfile(); TestTrue(TEXT("Failed receipt adds no gold, material, item or processed identity after restart"),Reload->GetGold()==77 && Reload->GetOwnedItems().IsEmpty() && !Reload->GetProfile().ResearchMaterials.Contains(TEXT("Spell_BlackCloud")) && !Reload->HasProcessedSettlement(Receipt.SettlementId));
    TestEqual(TEXT("Same receipt succeeds after transient failure"),Scope.Profile->ApplySettlement(Receipt),ELKSettlementResult::Success);
    Reload=ColdProfile(); TestTrue(TEXT("Successful retry is persisted once"),Reload->GetGold()==100 && Reload->GetOwnedItems().Num()==1 && Reload->GetProfile().ResearchMaterials.FindRef(TEXT("Spell_BlackCloud"))==1 && Reload->HasProcessedSettlement(Receipt.SettlementId));
    TestEqual(TEXT("Successful receipt remains idempotent"),Scope.Profile->ApplySettlement(Receipt),ELKSettlementResult::AlreadyApplied);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKRestartSettlementGuard,"LittleKing.Market.Transactions.RestartPreservesPendingSettlement",MarketBackendFlags)
bool FLKRestartSettlementGuard::RunTest(const FString&)
{
    FMarketScope Scope; if (!Scope.Start() || !Scope.Restore(Scope.MarketState())) { return false; }
    const auto Offers=Scope.Run->GetMarketOffers(); FString Error;
    const int32 ItemIndex=Offers.IndexOfByPredicate([](const auto& O){return O.Kind==ELKMarketOfferKind::Item;}); const int32 ResearchIndex=Offers.IndexOfByPredicate([](const auto& O){return LKMarketContent::IsResearch(O.Kind);});
    if (!TestTrue(TEXT("Buy assets whose pending receipt must survive restart"),ItemIndex>=0 && ResearchIndex>=0 && Scope.Run->PurchaseMarketOffer(ItemIndex,{},Error) && Scope.Run->PurchaseMarketOffer(ResearchIndex,{},Error))) { return false; }
    Scope.Profile->FailNextReadBackForTest(); TestTrue(TEXT("Terminal snapshot is saved even when home receipt application fails"),Scope.Run->AbandonCurrentRun());
    const auto Pending=Scope.Run->GetRunState(); const auto Receipt=Pending.PendingSettlement;
    TestTrue(TEXT("Unapplied terminal receipt retains its assets"),Receipt.SettlementId.IsValid() && !Receipt.bProfileApplied && Receipt.Items.Num()==1 && !Receipt.ResearchMaterials.IsEmpty());
    FLKExpeditionStartRequest Request; if (!LKWorldMapTest::Request(Scope.Profile,Request)) { return false; }
    auto Invalid=Request; Invalid.Cards.SetNum(6); TestFalse(TEXT("Invalid new request rejected before touching old receipt"),Scope.Run->RestartRun(Invalid));
    TestFalse(TEXT("Invalid request does not consume old receipt"),Scope.Profile->HasProcessedSettlement(Receipt.SettlementId));
    Scope.Profile->FailNextReadBackForTest(); TestFalse(TEXT("Any restart caller refuses to overwrite unpaid terminal receipt"),Scope.Run->RestartRun(Request));
    const auto Preserved=Scope.Run->GetRunState();
    TestTrue(TEXT("Rejected restart retains run, phase, wallet, receipt and carried item"),Preserved.RunId==Pending.RunId && Preserved.Phase==Pending.Phase && Preserved.WalletGold==Pending.WalletGold && Preserved.PendingSettlement.SettlementId==Receipt.SettlementId && !Preserved.PendingSettlement.bProfileApplied && LKItemContent::EqualInstances(Preserved.CarriedItems,Pending.CarriedItems));
    TestTrue(TEXT("Failed receipt commit consumes no home assets"),Scope.Profile->GetGold()==0 && Scope.Profile->GetOwnedItems().IsEmpty() && !Scope.Profile->HasProcessedSettlement(Receipt.SettlementId));
    TestTrue(TEXT("Restart proceeds after normal receipt application recovers"),Scope.Run->RestartRun(Request));
    TestTrue(TEXT("New run replaces only the settled previous run"),Scope.Run->GetRunState().RunId!=Pending.RunId && !Scope.Run->HasPendingSettlement() && Scope.Run->GetCarriedItems().IsEmpty());
    TestEqual(TEXT("Previous receipt gold safely brought home"),Scope.Profile->GetGold(),Receipt.GoldAmount);
    TestTrue(TEXT("Previous receipt item and material safely brought home"),Scope.Profile->GetOwnedItems().Num()==1 && Scope.Profile->GetOwnedItems()[0].InstanceId==Receipt.Items[0].InstanceId && Scope.Profile->GetProfile().ResearchMaterials.FindRef(Offers[ResearchIndex].CardId)==1);
    TestTrue(TEXT("New eligibility includes just-delivered research material"),Scope.Run->GetRunState().ResearchExclusions.Contains(Offers[ResearchIndex].CardId));
    TestEqual(TEXT("Retrying old receipt cannot pay twice after restart"),Scope.Profile->ApplySettlement(Receipt),ELKSettlementResult::AlreadyApplied);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKMarketMigration,"LittleKing.Market.Save.LegacyMigrationAndCorruptionGuards",MarketBackendFlags)
bool FLKMarketMigration::RunTest(const FString&)
{
    TestEqual(TEXT("Mercenary enum remains zero"),int32(ELKMarketOfferKind::Mercenary),0); TestEqual(TEXT("Spellbook enum remains one"),int32(ELKMarketOfferKind::SpellBook),1); TestEqual(TEXT("Blueprint enum remains two"),int32(ELKMarketOfferKind::BuildingBlueprint),2);
    FMarketScope Scope; if (!Scope.Start()) { return false; }
    auto Current=Scope.MarketState(); FString Error; TestTrue(TEXT("Current fixture valid"),ULKRunSubsystem::ValidateStoredRun(Current,Error));
    auto Invalid=Current; auto* N=Invalid.Nodes.FindByPredicate([&](const auto& Node){return Node.NodeId==Invalid.CurrentNodeId;}); auto* Item=N->MarketOffers.FindByPredicate([](const auto& O){return O.Kind==ELKMarketOfferKind::Item;}); Item->CardId=TEXT("Unit_Swordsman"); TestFalse(TEXT("ItemId and CardId cannot share one offer"),ULKRunSubsystem::ValidateStoredRun(Invalid,Error));
    TestFalse(TEXT("Item definition cannot become a player or enemy hand card"),LKCardRules::IsPlayerObtainable(TEXT("Item_Accessory01")) || LKCardRules::IsEnemyUsable(TEXT("Item_Accessory01")));
    Invalid=Current; Invalid.Cards[0].CardId=TEXT("Item_Accessory01"); TestFalse(TEXT("Stored deck rejects item identity"),ULKRunSubsystem::ValidateStoredRun(Invalid,Error));
    Invalid=Current; Invalid.Nodes.FindByPredicate([&](const auto& Node){return Node.NodeId==Invalid.CurrentNodeId;})->MarketGenerationVersion=99; TestFalse(TEXT("Future generator version rejected"),ULKRunSubsystem::ValidateStoredRun(Invalid,Error));
    Invalid=Current; Invalid.Nodes.FindByPredicate([&](const auto& Node){return Node.NodeId==Invalid.CurrentNodeId;})->MarketOffers[0].bSold=true; TestFalse(TEXT("Sold snapshot without wallet receipt rejected"),ULKRunSubsystem::ValidateStoredRun(Invalid,Error));
    Invalid=Current; Invalid.SchemaVersion=10; TestFalse(TEXT("Old schema cannot smuggle appended kinds"),ULKRunSubsystem::ValidateStoredRun(Invalid,Error));
    auto Legacy=Current; Legacy.SchemaVersion=10; Legacy.ResearchExclusions.Reset(); Legacy.MarketSilverCosts.Reset();
    for (auto& Node:Legacy.Nodes) { Node.MarketOffers.Reset(); Node.bMarketGenerated=false; Node.MarketGenerationVersion=0; }
    auto* Old=Legacy.Nodes.FindByPredicate([&](const auto& Node){return Node.NodeId==Legacy.CurrentNodeId;}); Old->bMarketGenerated=true;
    FLKMarketOffer Saved; Saved.OfferId=FGuid::NewGuid(); Saved.Kind=ELKMarketOfferKind::Mercenary; Saved.CardId=TEXT("Unit_ElfArcher"); Saved.Price=123; Saved.bSold=true; Old->MarketOffers.Add(Saved); Legacy.ProcessedWalletTransactions.Add(Saved.OfferId);
    TestTrue(TEXT("Legacy schema 10 accepted"),ULKRunSubsystem::ValidateStoredRun(Legacy,Error)); TestTrue(TEXT("Load and migrate legacy run"),Scope.Restore(Legacy));
    TestEqual(TEXT("Migrated to schema 11"),Scope.Run->GetRunState().SchemaVersion,11); TestEqual(TEXT("Legacy run keeps compatibility generator"),Scope.Run->GetRunState().MarketGenerationVersion,1);
    const auto Restored=Scope.Run->GetMarketOffers()[0]; TestTrue(TEXT("Legacy identity, price and sold state preserved"),Restored.OfferId==Saved.OfferId && Restored.Price==123 && Restored.bSold && Restored.CardId==Saved.CardId);
    auto P=Scope.Profile->GetProfile(); P.SchemaVersion=2; P.Gold=77; P.Revision+=100; P.ResearchMaterials.Add(TEXT("Spell_Freeze"),1); const auto ProfileId=P.ProfileId;
    auto* ProfileSave=NewObject<ULKProfileSaveGame>(); ProfileSave->Profile=P; TestTrue(TEXT("Write isolated schema 2 fixture"),UGameplayStatics::SaveGameToSlot(ProfileSave,Scope.Base+TEXT("_A"),0));
    auto* Reload=NewObject<ULKProfileSubsystem>(NewObject<UGameInstance>()); Reload->ConfigureStorage(Scope.Base); TestTrue(TEXT("Load profile 2 without rewriting ownership"),Reload->EnsureProfile());
    TestTrue(TEXT("Profile 3 migration preserves identity, gold, materials and empty items"),Reload->GetProfile().SchemaVersion==3 && Reload->GetProfile().ProfileId==ProfileId && Reload->GetGold()==77 && Reload->GetProfile().ResearchMaterials.FindRef(TEXT("Spell_Freeze"))==1 && Reload->GetOwnedItems().IsEmpty());
    TestTrue(TEXT("First successful mutation persists profile 3"),Reload->TouchSelectedProfile());
    FLKProfileState Read; TestTrue(TEXT("Read migrated profile"),Reload->ReadExistingProfile(Read,Error)); TestEqual(TEXT("Stored profile schema 3"),Read.SchemaVersion,3);
    FLKItemInstance I; I.InstanceId=FGuid::NewGuid(); I.ItemId=TEXT("Item_Accessory01"); TestTrue(TEXT("Stable item definition valid"),LKItemContent::ValidateInstances({I},Error)); TestFalse(TEXT("Duplicate instance rejected"),LKItemContent::ValidateInstances({I,I},Error)); I.ItemId=TEXT("Unit_Swordsman"); TestFalse(TEXT("Card identity cannot act as item identity"),LKItemContent::ValidateInstances({I},Error));
    return true;
}
#endif
