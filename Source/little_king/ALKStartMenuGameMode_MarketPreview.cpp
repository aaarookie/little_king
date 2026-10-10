#include "ALKStartMenuGameMode.h"
#if WITH_EDITOR
#include "ULKStartMenuWidget.h"
#include "ULKRunNodeSelectWidget.h"
#include "ULKSaveSlotSubsystem.h"
#include "ULKProfileSubsystem.h"
#include "ULKRunSubsystem.h"
#include "ULKGameData.h"
#include "ULKCardDefinition.h"
#include "LKHomeContent.h"
#include "LKUnitContent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Components/Button.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"

// Explicit editor-only preview: a unique save namespace, real transactions, scripted victorious rooms.
void ALKStartMenuGameMode::TickMarketPreview()
{
    if (!MenuWidget) { return; }
    ++PreviewFrame;
    auto* Saves=GetGameInstance()->GetSubsystem<ULKSaveSlotSubsystem>();
    auto* Run=GetGameInstance()->GetSubsystem<ULKRunSubsystem>();
    auto Finish=[&]() { Saves->ResetSession(); for (int32 I=0;I<8;++I) { Saves->DeleteSlot(I); } FPlatformMisc::RequestExit(false); };
    auto Click=[&](FName Name)
    {
        auto* Entry=WorldMapPreviewWidget?Cast<UUserWidget>(WorldMapPreviewWidget->GetWidgetFromName(Name)):nullptr;
        auto* Button=Entry?Cast<UButton>(Entry->GetWidgetFromName(TEXT("RowButton"))):nullptr;
        if (Button && Button->GetIsEnabled()) { Button->OnClicked.Broadcast(); }
    };
    if (PreviewFrame==30)
    {
        if (!Saves->CreateNewGame()) { Finish(); return; }
        auto* Profile=GetGameInstance()->GetSubsystem<ULKProfileSubsystem>(); Profile->AddGold(300);
        auto* Data=NewObject<ULKGameData>(); Data->EnsureDefaultDecks(); Data->EnsureCardLibrary();
        FLKExpeditionStartRequest Request; FString Error;
        if (!LKHomeContent::BuildExpeditionStartRequest(*Profile,Data,LKHomeContent::DefaultRegionId(),
            [](FName Id){return LKUnitContent::Find(Id);},[Data](FName Id)->const ULKCardDefinition*
            { for (const ULKCardDefinition* Card:Data->CardLibrary) { if (Card && Card->CardId==Id) { return Card; } } return nullptr; },Request,Error))
        { Finish(); return; }
        Request.StartingGold=100; Request.Seed=9152026;
        if (!Run->StartNewRun(Request)) { Finish(); return; }
        for (int32 Step=0;Step<40;++Step)
        {
            if (Run->HasServiceNode())
            {
                if (Run->GetNode(Run->GetRunState().CurrentNodeId).Type==ELKDungeonNodeType::Market) { break; }
                if (!Run->ResolveServiceNode(TEXT("RestHeal"))) { Finish(); return; }
            }
            else if (Run->HasPendingRewardChoice()) { if (!Run->SkipReward()) { Finish(); return; } }
            else if (Run->GetRunPhase()==ELKRunPhase::EnteringBattle)
            {
                FLKBattleContext Context; if (!Run->BeginCurrentBattle(Context)) { Finish(); return; }
                FLKBattleOutcome Result; Result.RunId=Context.RunId;Result.NodeId=Context.NodeId;Result.AttemptId=Context.AttemptId;
                Result.bFinalized=true;Result.Stats.Winner=ELKTeam::Player;
                for (const auto& Hero:Context.PlayerHeroes)
                { FLKHeroBattleOutcome H;H.InstanceId=Hero.HeroId;H.RecoveredState=Hero;Result.PlayerHeroes.Add(H); }
                if (!Run->SubmitBattleOutcome(Result)) { Finish(); return; }
            }
            else
            { const auto Next=Run->GetNextNodeIds(); if (Next.IsEmpty() || Run->SelectNode(Next[0])==ELKNodeSelectionResult::Rejected) { Finish(); return; } }
        }
        if (!Run->HasServiceNode()) { Finish(); return; }
        MenuWidget->SetVisibility(ESlateVisibility::Collapsed);
        WorldMapPreviewWidget=CreateWidget<ULKRunNodeSelectWidget>(GetWorld(),ULKRunNodeSelectWidget::StaticClass());
        WorldMapPreviewWidget->InitializeRunView(Run);WorldMapPreviewWidget->AddToViewport(110);
    }
    if (!WorldMapPreviewWidget) { return; }
    if (PreviewFrame==180) { Click(TEXT("MarketOffer0")); }
    if (PreviewFrame==270)
    {
        const auto Offers=Run->GetMarketOffers();
        for (int32 I=0;I<Offers.Num();++I) { if (Offers[I].Kind==ELKMarketOfferKind::Item) { Click(FName(*FString::Printf(TEXT("MarketOffer%d"),I)));break; } }
    }
    if (PreviewFrame==350) { Click(TEXT("ConfirmMarketPurchase")); Click(TEXT("MarketTab4")); }
    if (PreviewFrame==360) { Click(TEXT("RunItem0")); }
    if (PreviewFrame>=120 && (PreviewFrame-120)%90==0 && PreviewFrame<=390)
    {
        int32 W=0,H=0;GetWorld()->GetFirstPlayerController()->GetViewportSize(W,H);
        const FString Dir=FPaths::ProjectSavedDir()/TEXT("Screenshots/Market");IFileManager::Get().MakeDirectory(*Dir,true);
        FScreenshotRequest::RequestScreenshot(Dir/FString::Printf(TEXT("%dx%d-%02d.png"),W,H,(PreviewFrame-120)/90),true,false);
    }
    if (PreviewFrame==450) { Finish(); }
}
#endif
