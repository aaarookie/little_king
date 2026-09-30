#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "LKWorldMapTestHelpers.h"
#include "Tests/AutomationCommon.h"
#include "../LKCardRules.h"
#include "../LKResearchContent.h"
#include "../LKCardPresentation.h"
#include "../ULKRunSaveGame.h"
#include "../ULKProfileSaveGame.h"
#include "../ULKBattlePauseWidget.h"
#include "../ULKJourneyPresentationSubsystem.h"
#include "../ALKBattleGameMode.h"
#include "../ALKPlayerController.h"
#include "../ALKUnitBase.h"
#include "../ULKUnitAttributeSet.h"
#include "../ULKDeckState.h"
#include "../ULKSilverComponent.h"
#include "AbilitySystemComponent.h"
#include "EngineUtils.h"
#include "../ALKHomeGameMode.h"
#include "../ULKHomeHUDWidget.h"
#include "../ULKRunNodeSelectWidget.h"
#include "../ULKWorldMapWidget.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerState.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Kismet/GameplayStatics.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "HAL/FileManager.h"

namespace
{
constexpr auto Flags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;
struct FScope
{
    FString Base=TEXT("LittleKing_V082Test_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
    ULKProfileSubsystem* Profile; ULKRunSubsystem* Run;
    FScope()
    {
        ULKProfileSubsystem::SetPersistentProfileEnabledForTest(true);
        ULKProfileSubsystem::SetSlotNameOverrideForTest(Base);
        ULKRunSubsystem::SetRunSlotNameOverrideForTest(Base+TEXT("_Run"));
        Profile=NewObject<ULKProfileSubsystem>(NewObject<UGameInstance>()); Profile->ConfigureStorage(Base); Profile->EnsureProfile();
        Run=NewObject<ULKRunSubsystem>(NewObject<UGameInstance>()); Run->ConfigureStorage(Base+TEXT("_Run")); Run->SetProfileSubsystemOverrideForTest(Profile);
    }
    ~FScope()
    {
        for (const auto& S:{Base+TEXT("_A"),Base+TEXT("_B"),Base+TEXT("_Run"),Base+TEXT("_Fixture")})
        { if (UGameplayStatics::DoesSaveGameExist(S,0)) { UGameplayStatics::DeleteGameInSlot(S,0); } }
        ULKProfileSubsystem::ResetTestHooks(); ULKRunSubsystem::SetRunSlotNameOverrideForTest(FString());
    }
    FLKExpeditionStartRequest Request()
    { FLKExpeditionStartRequest R; LKWorldMapTest::Request(Profile,R); R.Seed=3082; return R; }
    bool Restore(const FLKRunState& State)
    { auto* S=NewObject<ULKRunSaveGame>(); S->RunState=State; return UGameplayStatics::SaveGameToSlot(S,Base+TEXT("_Fixture"),0) && Run->LoadExpeditionFromSlot(Base+TEXT("_Fixture"),true); }
};
TArray<FLKRunCardState> Cards(int32 Count)
{ TArray<FLKRunCardState> Result; const auto& Ids=LKHomeContent::DefaultUnlockedCards(); for(int32 I=0;I<Count;++I) { FLKRunCardState C; C.CardId=Ids[I]; Result.Add(C); } return Result; }
FLKBattleOutcome Outcome(const FLKBattleContext& C,bool Win=true)
{
    FLKBattleOutcome O; O.RunId=C.RunId; O.NodeId=C.NodeId; O.AttemptId=C.AttemptId; O.bFinalized=true; O.Stats.Winner=Win?ELKTeam::Player:ELKTeam::Enemy;
    for(const auto& H:C.PlayerHeroes) { FLKHeroBattleOutcome R; R.InstanceId=H.HeroId; R.RecoveredState=H; R.RecoveredState.Health=H.MaxHealth*.4f; O.PlayerHeroes.Add(R); } return O;
}
bool Win(ULKRunSubsystem* Run)
{ FLKBattleContext C; return Run->BeginCurrentBattle(C) && Run->SubmitBattleOutcome(Outcome(C)); }
FLKRunState ServiceFixture(const FLKRunState& Source,ELKDungeonNodeType Type)
{
    FLKRunState S=Source;
    auto* N=S.Nodes.FindByPredicate([&](const auto& N){return N.Type==Type && N.RegionId==S.WorldRegions[1].RegionId;});
    check(N); S.CurrentNodeId=N->NodeId; S.RegionId=N->RegionId; S.Phase=ELKRunPhase::ResolvingNode; S.PendingBattle=FLKBattleContext(); N->bResolved=false;
    return S;
}
bool Click(UUserWidget* W,const TCHAR* Name)
{
    auto* Entry=Cast<UUserWidget>(W->GetWidgetFromName(Name)); auto* B=Entry?Cast<UButton>(Entry->GetWidgetFromName(TEXT("RowButton"))):Cast<UButton>(W->GetWidgetFromName(Name));
    if (!B || !B->GetIsEnabled()) { return false; } B->OnClicked.Broadcast(); return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV082Capacity,"LittleKing.V082.Capacity.ExamplesAndDynamicRanges",Flags)
bool FLKV082Capacity::RunTest(const FString&)
{
    FString E; auto C=Cards(7);
    TestTrue(TEXT("7+2: remove a single one-slot card"),LKCardRules::ValidateReplacement(C,"Unit_TrollWarrior",{"Unit_Swordsman"},8,8,E));
    TestFalse(TEXT("7+2: excess two-slot release is forbidden"),LKCardRules::ValidateReplacement(C,"Unit_TrollWarrior",{"Unit_Swordsman","Unit_Archer"},8,8,E));
    C=Cards(6); TestTrue(TEXT("6+3: one-slot release suffices"),LKCardRules::ValidateReplacement(C,"Unit_Colossus",{"Unit_Swordsman"},8,8,E));
    C.Last().CardId="Unit_TrollMage"; // seven slots, including a two-slot choice
    TestFalse(TEXT("A larger card cannot replace the smaller feasible release"),LKCardRules::ValidateReplacement(C,"Unit_TrollWarrior",{"Unit_TrollMage"},8,8,E));
    C=Cards(7); FLKRunCardState T; T.CardId="Unit_TrollWarrior"; C.Add(T); // nine slots
    TestTrue(TEXT("Range 8-10: 9+1 can remove two one-slot cards"),LKCardRules::ValidateReplacement(C,"Unit_ElfArcher",{"Unit_Swordsman","Unit_Archer"},8,10,E));
    TestTrue(TEXT("Range 8-10: 9+1 also allows no discard"),LKCardRules::ValidateReplacement(C,"Unit_ElfArcher",{},8,10,E));
    TestTrue(TEXT("Range permits exchanging the two-slot troll"),LKCardRules::ValidateReplacement(C,"Unit_ElfArcher",{"Unit_TrollWarrior"},8,10,E));
    TestFalse(TEXT("Range rejects dropping below eight slots"),LKCardRules::ValidateReplacement(C,"Unit_ElfArcher",{"Unit_TrollWarrior","Unit_Swordsman"},8,10,E));
    TestFalse(TEXT("Unknown removed card rejected"),LKCardRules::ValidateReplacement(C,"Unit_ElfArcher",{"Missing"},8,10,E));
    TestFalse(TEXT("Duplicate removal rejected"),LKCardRules::ValidateReplacement(C,"Unit_ElfArcher",{"Unit_Swordsman","Unit_Swordsman"},8,10,E));
    C=Cards(5); TestFalse(TEXT("Keep five unique cards for a full cycling hand"),LKCardRules::ValidateReplacement(C,"Unit_Colossus",{"Unit_Swordsman","Unit_Archer"},6,6,E));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV082DepartureRest,"LittleKing.V082.DepartureAndRest.UpgradesAndAcquisitionGuards",Flags)
bool FLKV082DepartureRest::RunTest(const FString&)
{
    FScope S; auto R=S.Request(); R.Cards.SetNum(6);
    TestFalse(TEXT("Six-card formal departure rejected"),S.Run->StartNewRun(R));
    TestFalse(TEXT("Rejected start leaves no run"),S.Run->HasRun());
    R=S.Request(); TestTrue(TEXT("Seven-card departure accepted"),S.Run->StartNewRun(R));
    FString E; TestFalse(TEXT("Upgrade outside a rest node rejected"),S.Run->UpgradeAtRest("Spell_HealWave",0,E));
    const auto Before=S.Run->GetRunState(); auto Rest=ServiceFixture(Before,ELKDungeonNodeType::Rest);
    TestTrue(TEXT("Restore rest safety point"),S.Restore(Rest));
    TestFalse(TEXT("Rest does not upgrade mercenaries"),S.Run->UpgradeAtRest("Unit_Swordsman",0,E));
    TestFalse(TEXT("Stale level rejected"),S.Run->UpgradeAtRest("Spell_HealWave",1,E));
    S.Run->ConfigureStorage(S.Base+TEXT("?:/Unwritable")); TestTrue(TEXT("Load existing rest into blocked storage"),S.Run->LoadExpeditionFromSlot(S.Base+TEXT("_Fixture"),true));
    TestFalse(TEXT("Failed save preserves upgrade opportunity"),S.Run->UpgradeAtRest("Spell_HealWave",0,E));
    TestTrue(TEXT("Rest still pending"),S.Run->HasServiceNode());
    S.Run->SetAutoSaveEnabled(false); TestTrue(TEXT("Rest spell upgrade commits"),S.Run->UpgradeAtRest("Spell_HealWave",0,E));
    TestEqual(TEXT("Upgrade adds exactly one level"),S.Run->GetRunState().Cards.FindByPredicate([](const auto& C){return C.CardId=="Spell_HealWave";})->UpgradeLevel,1);
    TestEqual(TEXT("Upgrade uses rest without healing"),S.Run->GetRunState().Heroes[0].Health,Rest.Heroes[0].Health);
    TestFalse(TEXT("Rest cannot be used twice"),S.Run->UpgradeAtRest("Building_ArrowTower",0,E));
    FLKRunRewardOffer O; O.CardId="Spell_Fireball"; O.Kind=ELKRunRewardKind::UpgradeCard; O.LevelAfter=1;
    TestFalse(TEXT("Battle rewards cannot upgrade spells"),S.Run->OfferRewardBatch({O}));
    O.CardId="Building_ArrowTower"; TestFalse(TEXT("Battle rewards cannot upgrade buildings"),S.Run->OfferRewardBatch({O}));
    O.Kind=ELKRunRewardKind::AddCard; O.CardId="Building_SiegeCatapult"; TestFalse(TEXT("New buildings cannot be expedition rewards"),S.Run->OfferRewardBatch({O}));
    O.CardId="Spell_ResearchFireball_N2"; TestFalse(TEXT("New spells cannot be expedition rewards"),S.Run->OfferRewardBatch({O}));
    O.CardId="Unit_ElfArcher"; TestTrue(TEXT("Temporary mercenary reward accepted"),S.Run->OfferRewardBatch({O}));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV082Research,"LittleKing.V082.Market.MaterialResearchAtomicity",Flags)
bool FLKV082Research::RunTest(const FString&)
{
    FScope S; TestTrue(TEXT("Start"),S.Run->StartNewRun(S.Request())); FString E;
    auto Market=ServiceFixture(S.Run->GetRunState(),ELKDungeonNodeType::Market); Market.WalletGold=1000;
    auto* N=Market.Nodes.FindByPredicate([&](const auto& N){return N.NodeId==Market.CurrentNodeId;}); N->MarketOffers.Reset(); N->bMarketGenerated=true;
    for (FName Id:{FName("Unit_ElfArcher"),FName("Spell_ResearchFireball_N2"),FName("Building_SiegeCatapult")})
    { FLKMarketOffer O; O.OfferId=FGuid::NewGuid(); O.CardId=Id; const auto* R=LKResearchContent::Find(Id); O.Kind=R?R->Kind:ELKMarketOfferKind::Mercenary; O.Price=R?R->Price:50; N->MarketOffers.Add(O); }
    TestTrue(TEXT("Restore deterministic market fixture"),S.Restore(Market));
    TestTrue(TEXT("Buy spellbook"),S.Run->PurchaseMarketOffer(1,{},E));
    TestFalse(TEXT("Repeated purchase rejected"),S.Run->PurchaseMarketOffer(1,{},E));
    TestTrue(TEXT("Buy blueprint"),S.Run->PurchaseMarketOffer(2,{},E));
    TestEqual(TEXT("Research purchases do not create combat cards"),S.Run->GetRunState().Cards.Num(),7);
    TestEqual(TEXT("Wallet charged exact prices once"),S.Run->GetWalletGold(),530);
    TestTrue(TEXT("Market reload retains sold state"),S.Run->LoadExpeditionFromSlot(S.Base+TEXT("_Run"),true));
    TestTrue(TEXT("Both material offers stay sold"),S.Run->GetMarketOffers()[1].bSold && S.Run->GetMarketOffers()[2].bSold);
    TestFalse(TEXT("Sold after reload cannot be bought again"),S.Run->PurchaseMarketOffer(1,{},E));
    TestTrue(TEXT("Buy mercenary without removing when it fits"),S.Run->PurchaseMarketOffer(0,{},E));
    TestEqual(TEXT("Mercenary fills eighth slot"),S.Run->GetDeckCapacityUsed(),8);
    TestTrue(TEXT("Abandon outside battle carries all materials"),S.Run->AbandonCurrentRun());
    TestEqual(TEXT("All unspent gold returned"),S.Profile->GetGold(),480);
    TestEqual(TEXT("Book delivered to home"),S.Profile->GetProfile().ResearchMaterials.FindRef("Spell_ResearchFireball_N2"),1);
    TestEqual(TEXT("Blueprint delivered to home"),S.Profile->GetProfile().ResearchMaterials.FindRef("Building_SiegeCatapult"),1);
    TestTrue(TEXT("Settlement retry is idempotent"),S.Run->ApplyPendingSettlementToProfile());
    TestEqual(TEXT("No duplicate book from settlement retry"),S.Profile->GetProfile().ResearchMaterials.FindRef("Spell_ResearchFireball_N2"),1);
    TestTrue(TEXT("Research consumes book and unlocks spell"),S.Profile->ResearchCard("Spell_ResearchFireball_N2",E));
    TestTrue(TEXT("New spell permanently unlocked"),S.Profile->IsCardUnlocked("Spell_ResearchFireball_N2"));
    TestFalse(TEXT("Consumed book removed"),S.Profile->GetProfile().ResearchMaterials.Contains("Spell_ResearchFireball_N2"));
    TestFalse(TEXT("Duplicate research rejected"),S.Profile->ResearchCard("Spell_ResearchFireball_N2",E));
    const auto P=S.Profile->GetProfile(); S.Profile->ConfigureStorage(S.Base+TEXT("?:/Unwritable")); S.Profile->SetProfileForTest(P);
    TestFalse(TEXT("Failed research save does not consume blueprint"),S.Profile->ResearchCard("Building_SiegeCatapult",E));
    TestEqual(TEXT("Blueprint retained after failed save"),S.Profile->GetProfile().ResearchMaterials.FindRef("Building_SiegeCatapult"),1);
    TestFalse(TEXT("Building not unlocked on failed save"),S.Profile->IsCardUnlocked("Building_SiegeCatapult"));
    S.Profile->SetAutoSaveEnabled(false); TestTrue(TEXT("Research building via same catalog"),S.Profile->ResearchCard("Building_SiegeCatapult",E));
    auto Loadout=LKHomeContent::DefaultLoadout(); Loadout.CardIds[6]="Building_SiegeCatapult";
    TestEqual(TEXT("Researched building accepted in war room"),S.Profile->SaveLoadout(Loadout,nullptr),ELKLoadoutResult::Success);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV082Rarity,"LittleKing.V082.Market.RarityDepthAndFrozenContent",Flags)
bool FLKV082Rarity::RunTest(const FString&)
{
    int32 Books=0,Blueprints=0;
    for (int32 Seed=0;Seed<20000;++Seed)
    {
        FLKDungeonNode N; N.NodeId="World_R0_L5_N0"; N.Type=ELKDungeonNodeType::Market;
        LKResearchContent::GenerateMarket(N,Seed,Seed%4);
        TestTrue(TEXT("Three mercenary entries plus at most two research items"),N.MarketOffers.Num()>=3 && N.MarketOffers.Num()<=5);
        const auto Frozen=N.MarketOffers; LKResearchContent::GenerateMarket(N,Seed+1,3);
        TestEqual(TEXT("Repeated generation retains frozen offer identity"),N.MarketOffers[0].OfferId,Frozen[0].OfferId);
        for (const auto& O:N.MarketOffers)
        {
            if (O.Kind==ELKMarketOfferKind::Mercenary) { TestTrue(TEXT("Market cards are temporary mercenaries"),LKCardRules::IsTemporaryMercenary(O.CardId)); }
            else
            { const auto* R=LKResearchContent::Find(O.CardId); if(!TestNotNull(TEXT("Research definition exists"),R)) { return false; }
              TestTrue(TEXT("Region gate enforced"),Seed%4>=R->MinimumRegionDepth); TestEqual(TEXT("Catalog price frozen"),O.Price,R->Price);
              if(O.Kind==ELKMarketOfferKind::SpellBook) { ++Books; } else { ++Blueprints; } }
        }
    }
    TestTrue(TEXT("Fixed sample verifies about one percent book probability"),Books>100 && Books<350);
    TestTrue(TEXT("Blueprints remain rarer than books"),Blueprints>20 && Blueprints<Books);
    const auto* Low=LKResearchContent::Find("Spell_ResearchFireball_N2"); const auto* High=LKResearchContent::Find("Spell_ResearchFireball_A1");
    TestTrue(TEXT("Higher grade costs more and has lower weight"),High->Price>Low->Price && High->Weight<Low->Weight);
    AddInfo(FString::Printf(TEXT("20,000 frozen markets: %d books, %d blueprints"),Books,Blueprints)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV082Resume,"LittleKing.V082.Resume.LockedRetryAndFailureSettlement",Flags)
bool FLKV082Resume::RunTest(const FString&)
{
    FScope S; TestTrue(TEXT("Start"),S.Run->StartNewRun(S.Request()));
    TestTrue(TEXT("Choose battle"),LKWorldMapTest::SelectBattle(S.Run)); FLKBattleContext C;
    TestTrue(TEXT("Begin"),S.Run->BeginCurrentBattle(C)); const auto Before=S.Run->GetRunState();
    TestFalse(TEXT("Battle abandonment forbidden"),S.Run->AbandonCurrentRun());
    TestFalse(TEXT("No changing fork during combat"),S.Run->SelectNode(S.Run->GetNextNodeIds()[0])!=ELKNodeSelectionResult::Rejected);
    TestTrue(TEXT("Exit persists prebattle snapshot"),S.Run->SaveForMenuExit());
    TestEqual(TEXT("Exit becomes entering same locked battle"),S.Run->GetRunPhase(),ELKRunPhase::EnteringBattle);
    TestFalse(TEXT("Cannot abandon deployment after exit"),S.Run->AbandonCurrentRun());
    TestTrue(TEXT("Cold load"),S.Run->LoadExpeditionFromSlot(S.Base+TEXT("_Run"),true)); FLKBattleContext Retry;
    TestTrue(TEXT("Retry starts"),S.Run->BeginCurrentBattle(Retry));
    TestEqual(TEXT("Chosen node frozen"),Retry.NodeId,C.NodeId); TestEqual(TEXT("Attempt identity frozen"),Retry.AttemptId,C.AttemptId);
    TestEqual(TEXT("Battle random seed frozen"),Retry.Seed,C.Seed); TestTrue(TEXT("Enemy hero roster frozen"),Retry.EnemyHeroIds==C.EnemyHeroIds);
    TestEqual(TEXT("Heroes restore prebattle health"),Retry.PlayerHeroes[0].Health,C.PlayerHeroes[0].Health);
    auto Loss=S.Run->GetRunState(); Loss.WalletGold=101; Loss.CarriedResearchMaterials.Add("Spell_ResearchFireball_N2",2);
    TestTrue(TEXT("Restore funded failure fixture"),S.Restore(Loss)); TestTrue(TEXT("Resume same attempt"),S.Run->BeginCurrentBattle(Retry));
    TestTrue(TEXT("Failure accepted"),S.Run->SubmitBattleOutcome(Outcome(Retry,false)));
    TestEqual(TEXT("101 remaining gold returns floor(80 percent)=80"),S.Profile->GetGold(),80);
    TestEqual(TEXT("Failure carries every purchased book"),S.Profile->GetProfile().ResearchMaterials.FindRef("Spell_ResearchFireball_N2"),2);
    TestTrue(TEXT("Failure receipt retry succeeds"),S.Run->ApplyPendingSettlementToProfile()); TestEqual(TEXT("Failure gold not duplicated"),S.Profile->GetGold(),80);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV082Region,"LittleKing.V082.Map.FifteenStopsPerRegion",Flags)
bool FLKV082Region::RunTest(const FString&)
{
    FScope S; TestTrue(TEXT("Start map"),S.Run->StartNewRun(S.Request())); const auto State=S.Run->GetRunState();
    for (const auto& Region:State.WorldRegions)
    {
        for (FName Entry:Region.EntryNodeIds)
        {
            FName Id=Entry; int32 Stops=0,Battles=0,Rest=0,Markets=0;
            while (!Id.IsNone())
            { const auto N=S.Run->GetNode(Id); if (N.RegionId!=Region.RegionId) { break; } ++Stops; Battles+=LKWorldMapContent::IsCombat(N.Type); Rest+=N.Type==ELKDungeonNodeType::Rest; Markets+=N.Type==ELKDungeonNodeType::Market;
              Id=N.NextNodeIds.IsEmpty()?NAME_None:N.NextNodeIds[0]; }
            TestEqual(TEXT("Every route visits fifteen layers"),Stops,15); TestTrue(TEXT("About ten combat stops"),Battles==9 || Battles==10);
            TestEqual(TEXT("Three healing/upgrade opportunities"),Rest,3); TestEqual(TEXT("Two frozen markets"),Markets,2);
        }
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV082Heal,"LittleKing.V082.Healing.HeroPercentAndFixedUnitAmount",Flags)
bool FLKV082Heal::RunTest(const FString&)
{
    FScope S; FTestWorldWrapper World; if(!World.CreateTestWorld(EWorldType::Game)) { return false; }
    World.GetTestWorld()->GetWorldSettings()->DefaultGameMode=ALKBattleGameMode::StaticClass();
    if(!World.BeginPlayInTestWorld()) { World.ForwardErrorMessages(this); return false; }
    auto* GM=World.GetTestWorld()->GetAuthGameMode<ALKBattleGameMode>(); World.GetTestWorld()->SpawnActor<ALKPlayerController>(); GM->Tick(0.f);
    auto* Asset=LoadObject<ULKCardDefinition>(nullptr,TEXT("/Game/Data/C_HealWave.C_HealWave"));
    if(!TestNotNull(TEXT("Authored Heal Wave available"),Asset)) { return false; }
    TestEqual(TEXT("Authored flat amount"),Asset->SpellValue,120.f); TestTrue(TEXT("Authored hero fraction"),FMath::IsNearlyEqual(Asset->HeroHealPercent,.06f));
    for (int32 I=0;I<GM->AvailableHeroes.Num();++I) { TestEqual(TEXT("Deploy heroes"),GM->DeployHero(ELKTeam::Player,GM->AvailableHeroes[I],FVector((I-1)*700,-1300,0)),ELKPlayResult::Success); }
    GM->ForceStartBattle(); ALKUnitBase* Knight=nullptr; ALKUnitBase* Ranger=nullptr;
    for(TActorIterator<ALKUnitBase> It(World.GetTestWorld());It;++It)
    { if(It->GetTeam()!=ELKTeam::Player) { continue; } if(It->GetUnitId()=="Hero_Knight") { Knight=*It; } if(It->GetUnitId()=="Hero_Ranger") { Ranger=*It; } }
    if(!TestNotNull(TEXT("Knight"),Knight)||!TestNotNull(TEXT("Ranger"),Ranger)) { return false; }
    auto* Soldier=GM->SpawnUnitForTeam("Unit_Swordsman",ELKTeam::Player,Knight->GetActorLocation()+FVector(100,0,0));
    if(!TestNotNull(TEXT("Soldier"),Soldier)) { return false; }
    Soldier->GetAbilitySystemComponent()->SetNumericAttributeBase(ULKUnitAttributeSet::GetMaxHealthAttribute(),1000.f);
    Soldier->GetAbilitySystemComponent()->SetNumericAttributeBase(ULKUnitAttributeSet::GetHealthAttribute(),500.f);
    Knight->GetAbilitySystemComponent()->SetNumericAttributeBase(ULKUnitAttributeSet::GetHealthAttribute(),Knight->GetMaxHealth()*.5f);
    const float Old=Knight->GetHealth(); Ranger->SetActorLocation(Knight->GetActorLocation()+FVector(150,0,0)); Ranger->Die();
    auto* Deck=GM->GetTeamDeck(ELKTeam::Player); int32 Seed=0;
    do { Deck->InitDeck({"Spell_HealWave","Unit_Swordsman","Unit_Archer","Spell_Fireball","Building_ArrowTower"},4,Seed++); } while(!Deck->GetHand().Contains("Spell_HealWave") && Seed<100);
    GM->GetTeamSilver(ELKTeam::Player)->AddSilver(100);
    TestEqual(TEXT("Cast actual Heal Wave"),GM->PlayCardForTeam(ELKTeam::Player,Deck->GetHand().IndexOfByKey("Spell_HealWave"),Knight->GetActorLocation()),ELKPlayResult::Success);
    TestTrue(TEXT("Hero receives 120 plus six percent maximum life"),FMath::IsNearlyEqual(Knight->GetHealth()-Old,120.f+Knight->GetMaxHealth()*.06f,.01f));
    TestEqual(TEXT("Mercenary receives only fixed 120"),Soldier->GetHealth(),620.f);
    TestEqual(TEXT("Incapacitated hero cannot be revived by combat healing"),Ranger->GetHealth(),0.f);
    TestTrue(TEXT("Upgrade scales the whole effect by ten percent"),FMath::IsNearlyEqual(LKCardRules::UpgradeMultiplier(1),1.1f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV082UI,"LittleKing.V082.UI.LibraryMarketRestPauseAndRender",Flags)
bool FLKV082UI::RunTest(const FString&)
{
    FScope S; FTestWorldWrapper World; if(!World.CreateTestWorld(EWorldType::Game)) { return false; }
    auto* P=World.GetTestWorld()->GetGameInstance()->GetSubsystem<ULKProfileSubsystem>();
    P->ConfigureStorage(S.Base); auto State=S.Profile->GetProfile(); State.ResearchMaterials.Add("Spell_ResearchFireball_N2",1); P->SetProfileForTest(State);
    World.GetTestWorld()->GetWorldSettings()->DefaultGameMode=ALKHomeGameMode::StaticClass();
    if(!World.BeginPlayInTestWorld()) { World.ForwardErrorMessages(this); return false; }
    auto* Home=World.GetTestWorld()->GetAuthGameMode<ALKHomeGameMode>();
    auto Render=[&](const TCHAR* Name,UUserWidget* W)
    {
        if(!FApp::CanEverRender()) { return; }
        const auto Slate=W->TakeWidget();
        // Offscreen rendering does not tick the game-instance reveal animation.
        if(auto* Journey=W->GetGameInstance()->GetSubsystem<ULKJourneyPresentationSubsystem>()) { Journey->Tick(1.f); }
        for (int32 Width:{1280,1920})
        {
            const int32 Height=Width==1280?720:1080;
            W->ForceLayoutPrepass(); FWidgetRenderer Renderer(true); auto* Target=Renderer.DrawWidget(Slate,FVector2D(Width,Height));
            if(FString(Name)==TEXT("MarketConfirm"))
            { if(auto* Scroll=Cast<UScrollBox>(W->GetWidgetFromName(TEXT("ServiceDetailScroll")))) { Scroll->SetScrollOffset(100000.f); } }
            Renderer.DrawWidget(Target,Slate,FVector2D(Width,Height),.016f); Renderer.DrawWidget(Target,Slate,FVector2D(Width,Height),.016f);
            TArray<FColor> Pixels; FReadSurfaceDataFlags ReadFlags; ReadFlags.SetLinearToGamma(false);
            if(!TestTrue(TEXT("UMG pixels rendered"),Target && Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,ReadFlags))) { return; }
            TArray64<uint8> PNG; FImageUtils::PNGCompressImageArray(Width,Height,Pixels,PNG);
            const auto Dir=FPaths::ProjectSavedDir()/TEXT("Screenshots/V082"); IFileManager::Get().MakeDirectory(*Dir,true);
            TestTrue(TEXT("Save UI capture"),FFileHelper::SaveArrayToFile(PNG,*(Dir/FString::Printf(TEXT("%s-%dx%d.png"),Name,Width,Height))));
        }
    };
    auto* Library=CreateWidget<ULKHomeHUDWidget>(World.GetTestWorld(),ULKHomeHUDWidget::StaticClass()); Library->InitializeHomeHUD(Home); Library->TakeWidget(); Library->OpenPanel(ELKHomePanel::Library);
    const auto Model=Home->BuildPanelModel(ELKHomePanel::Library,LKHomeContent::DefaultLoadout(),"Spell_ResearchFireball_N2",0);
    const auto* ResearchRow=Model.Rows.FindByPredicate([](const auto& R){return R.RowId=="Spell_ResearchFireball_N2";});
    TestTrue(TEXT("Library displays the correct research tier"),ResearchRow && ResearchRow->Detail.ToString().Contains(TEXT("初阶二级")));
    Home->GameData->DeckCapacityMaximum=10;
    const auto DraftModel=Home->BuildPanelModel(ELKHomePanel::WarRoom,LKHomeContent::DefaultLoadout(),NAME_None,0);
    TestTrue(TEXT("Home UI reflects dynamic capacity"),DraftModel.Subtitle.ToString().Contains(TEXT("7/10 格")));
    Home->GameData->DeckCapacityMaximum=8;
    TestTrue(TEXT("Library exposes material research action"),Model.Actions.ContainsByPredicate([](const auto& A){return A.ActionId=="Research:Spell_ResearchFireball_N2" && A.bEnabled;}));
    auto* Rows=Cast<UPanelWidget>(Library->GetWidgetFromName(TEXT("RowsBox")));
    auto* Material=Rows?Cast<UUserWidget>(Rows->GetChildAt(Rows->GetChildrenCount()-1)):nullptr;
    auto* MaterialButton=Material?Cast<UButton>(Material->GetWidgetFromName(TEXT("RowButton"))):nullptr;
    if (TestNotNull(TEXT("Material appears in actual library list"),MaterialButton)) { MaterialButton->OnClicked.Broadcast(); }
    Render(TEXT("Library"),Library);
    TestTrue(TEXT("Start UI expedition fixture"),S.Run->StartNewRun(S.Request()));
    auto* Map=CreateWidget<ULKRunNodeSelectWidget>(World.GetTestWorld(),ULKRunNodeSelectWidget::StaticClass()); Map->InitializeRunView(S.Run); Map->TakeWidget(); Map->RefreshNodeSelect();
    if(auto* Graph=Cast<ULKWorldMapWidget>(Map->GetWidgetFromName(TEXT("WorldMapCanvas"))))
    {
        auto Snapshot=S.Run->GetRunState(); const FVector2D View(700,360);
        for(const auto& Region:Snapshot.WorldRegions)
        {
            Snapshot.RegionId=Region.RegionId; Graph->SetMapState(Snapshot,NAME_None);
            for(const auto& N:Snapshot.Nodes)
            {
                if(N.RegionId!=Region.RegionId) { continue; }
                const auto Point=Graph->NodeLocalPosition(N.NodeId,View);
                TestEqual(TEXT("Projected node center selects itself"),Graph->HitTestNode(Point,View),N.NodeId);
                for(const auto& Other:Snapshot.Nodes)
                { if(Other.RegionId==Region.RegionId && Other.Layer==N.Layer && Other.NodeId!=N.NodeId)
                  { TestTrue(TEXT("Dense same-layer lanes remain separated"),FMath::Abs(Point.Y-Graph->NodeLocalPosition(Other.NodeId,View).Y)>=35.9); } }
            }
        }
        Graph->SetMapState(S.Run->GetRunState(),Map->GetSelectedNode());
    }
    Render(TEXT("Region"),Map);
    auto Rest=ServiceFixture(S.Run->GetRunState(),ELKDungeonNodeType::Rest); TestTrue(TEXT("Restore UI rest"),S.Restore(Rest)); Map->RefreshNodeSelect(); Render(TEXT("Rest"),Map);
    const int32 SpellIndex=Rest.Cards.IndexOfByPredicate([](const auto& C){return C.CardId=="Spell_HealWave";});
    TestTrue(TEXT("Actual rest upgrade button works"),Click(Map,*FString::Printf(TEXT("RestUpgrade%d"),SpellIndex)));
    TestFalse(TEXT("UI upgrade consumes rest"),S.Run->HasServiceNode());
    auto Market=ServiceFixture(S.Run->GetRunState(),ELKDungeonNodeType::Market); Market.WalletGold=1000;
    auto* N=Market.Nodes.FindByPredicate([&](const auto& N){return N.NodeId==Market.CurrentNodeId;}); N->MarketOffers.Reset(); FLKMarketOffer O; O.OfferId=FGuid::NewGuid(); O.CardId="Unit_TrollWarrior"; O.Price=50; N->MarketOffers.Add(O);
    TestTrue(TEXT("Restore UI market"),S.Restore(Market)); Map->RefreshNodeSelect(); TestTrue(TEXT("Select actual market product"),Click(Map,TEXT("MarketOffer0")));
    Render(TEXT("MarketReplacement"),Map);
    TestFalse(TEXT("Confirm unavailable until minimal replacement"),Click(Map,TEXT("ConfirmMarketPurchase")));
    TestTrue(TEXT("Choose one-slot replacement"),Click(Map,TEXT("MarketReplace0")));
    if(auto* Scroll=Cast<UScrollBox>(Map->GetWidgetFromName(TEXT("ServiceDetailScroll")))) { Scroll->ScrollToEnd(); }
    Render(TEXT("MarketConfirm"),Map);
    TestTrue(TEXT("Confirm purchase via UI"),Click(Map,TEXT("ConfirmMarketPurchase")));
    TestEqual(TEXT("UI transaction respects weighted slots"),S.Run->GetDeckCapacityUsed(),8);
    FTestWorldWrapper Battle; if(!Battle.CreateTestWorld(EWorldType::Game)) { return false; }
    Battle.GetTestWorld()->GetWorldSettings()->DefaultGameMode=ALKBattleGameMode::StaticClass();
    if(!Battle.BeginPlayInTestWorld()) { Battle.ForwardErrorMessages(this); return false; }
    auto* PC=Battle.GetTestWorld()->SpawnActor<ALKPlayerController>(); if(!PC->PlayerState) { PC->PlayerState=Battle.GetTestWorld()->SpawnActor<APlayerState>(); }
    auto* Pause=CreateWidget<ULKBattlePauseWidget>(Battle.GetTestWorld(),ULKBattlePauseWidget::StaticClass()); Pause->TakeWidget(); Pause->TogglePause();
    TestTrue(TEXT("Pause menu opens"),Pause->IsPauseOpen()); TestTrue(TEXT("World paused"),UGameplayStatics::IsGamePaused(Battle.GetTestWorld())); Render(TEXT("Pause"),Pause);
    Pause->TogglePause(); TestFalse(TEXT("Resume closes pause"),Pause->IsPauseOpen()); TestFalse(TEXT("World resumed"),UGameplayStatics::IsGamePaused(Battle.GetTestWorld()));
    return true;
}
#endif
