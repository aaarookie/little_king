#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "LKWorldMapTestHelpers.h"
#include "Tests/AutomationCommon.h"
#include "Components/Button.h"
#include "Components/SpinBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"
#include "../ALKHomeGameMode.h"
#include "../ALKHomePlayerController.h"
#include "../ALKBattleGameMode.h"
#include "../ALKPlayerController.h"
#include "../ULKBattleHUDWidget.h"
#include "../ULKRunRewardWidget.h"
#include "../LKHomeContent.h"
#include "../ULKGameData.h"
#include "../ULKCardDefinition.h"
#include "../ULKHomeHUDWidget.h"
#include "../ULKHomeListButtonWidget.h"
#include "../ULKProfileSubsystem.h"
#include "../ULKRunSubsystem.h"

namespace
{
    struct FHomeUIScope
    {
        FHomeUIScope()
        {
            ULKProfileSubsystem::SetSlotNameOverrideForTest(TEXT("LittleKing_HomeUITest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits));
            ULKProfileSubsystem::SetPersistentProfileEnabledForTest(true);
            ULKRunSubsystem::SetRunSlotNameOverrideForTest(TEXT("LittleKing_HomeUITestRun_") + FGuid::NewGuid().ToString(EGuidFormats::Digits));
        }
        ~FHomeUIScope()
        {
            UGameplayStatics::DeleteGameInSlot(ULKProfileSubsystem::GetSlotNameA(), 0);
            UGameplayStatics::DeleteGameInSlot(ULKProfileSubsystem::GetSlotNameB(), 0);
            UGameplayStatics::DeleteGameInSlot(ULKRunSubsystem::GetRunSlotName(), 0);
            ULKProfileSubsystem::ResetTestHooks();
            ULKRunSubsystem::SetRunSlotNameOverrideForTest(FString());
        }
    };
    bool ClickEntry(ULKHomeHUDWidget* HUD, const TCHAR* ListName, int32 Index)
    {
        UPanelWidget* List = Cast<UPanelWidget>(HUD->GetWidgetFromName(ListName));
        UUserWidget* Entry = List ? Cast<UUserWidget>(List->GetChildAt(Index)) : nullptr;
        UButton* Button = Entry ? Cast<UButton>(Entry->GetWidgetFromName(TEXT("RowButton"))) : nullptr;
        if (!Button || !Button->GetIsEnabled() || !Entry->GetIsEnabled()) { return false; }
        Button->OnClicked.Broadcast();
        return true;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKHomeUIInteractionTest, "LittleKing.HomeUI.DraftConfirmationAndDetails",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLKHomeUIInteractionTest::RunTest(const FString& Parameters)
{
    FHomeUIScope Scope;
    FTestWorldWrapper World;
    World.CreateTestWorld(EWorldType::Game);
    World.GetTestWorld()->GetWorldSettings()->DefaultGameMode = LoadClass<AGameModeBase>(nullptr,
        TEXT("/Game/blueprint/Home/BP_HomeGameMode.BP_HomeGameMode_C"));
    if (!World.BeginPlayInTestWorld()) { World.ForwardErrorMessages(this); return false; }
    ALKHomeGameMode* GM = World.GetTestWorld()->GetAuthGameMode<ALKHomeGameMode>();
    if (!TestNotNull(TEXT("Authored home GameMode"), GM)) { return false; }
    World.GetTestWorld()->SpawnActor<ALKHomePlayerController>();
    GM->Tick(0.f);
    ULKHomeHUDWidget* HUD = GM->GetHomeHUD();
    if (!TestNotNull(TEXT("HUD exists"), HUD)) { return false; }
    HUD->Refresh(); // Safe even before Slate constructs the native controls.
    const TSharedRef<SWidget> Slate = HUD->TakeWidget();
    HUD->OpenPanel(ELKHomePanel::Library);
    UTextBlock* Detail = Cast<UTextBlock>(HUD->GetWidgetFromName(TEXT("DetailCopy")));
    TestTrue(TEXT("First spell detail automatically visible"), Detail && Detail->GetText().ToString().Contains(TEXT("半径")));
    HUD->OpenPanel(ELKHomePanel::WarRoom);
    bool bHeroesTab = false;
    auto Model = [&]()
    {
        FLKHomePanelModel Result = GM->BuildPanelModel(ELKHomePanel::WarRoom, HUD->GetDraftLoadout(), NAME_None, 0);
        Result.Rows.RemoveAll([&](const FLKHomePanelRow& Row)
            { return Row.RowId.IsNone() || (bHeroesTab ? Row.bToggleable : !Row.bToggleable); });
        return Result;
    };
    auto ToggleFirstCard = [&]()
    {
        const FLKHomePanelModel Current = Model();
        const int32 Index = Current.Rows.IndexOfByPredicate([](const FLKHomePanelRow& Row) { return Row.bToggleable; });
        return ClickEntry(HUD, TEXT("RowsBox"), Index);
    };
    TestTrue(TEXT("Real card button toggles draft"), ToggleFirstCard());
    TestEqual(TEXT("Six cards in draft"), HUD->GetDraftLoadout().CardIds.Num(), 6);
    HUD->OpenPanel(ELKHomePanel::WarRoom);
    TestTrue(TEXT("Opening same building keeps draft"), HUD->HasDirtyDraft());
    HUD->OpenPanel(ELKHomePanel::Gate);
    TestTrue(TEXT("Continue editing confirmation button works"), ClickEntry(HUD, TEXT("ActionsBox"), 2));
    TestTrue(TEXT("Continue editing retains edits"), HUD->HasDirtyDraft() && HUD->GetCurrentPanel() == ELKHomePanel::WarRoom);
    HUD->OpenPanel(ELKHomePanel::Gate);
    TestTrue(TEXT("Discard confirmation button works"), ClickEntry(HUD, TEXT("ActionsBox"), 1));
    TestTrue(TEXT("Discard continues to requested building"), HUD->GetCurrentPanel() == ELKHomePanel::Gate && !HUD->HasDirtyDraft());
    TestEqual(TEXT("Discard leaves saved deck alone"), GM->GetProfileSubsystem()->GetSavedLoadout().CardIds.Num(), 7);
    HUD->OpenPanel(ELKHomePanel::WarRoom);
    ToggleFirstCard();
    HUD->RequestClosePanel();
    TestTrue(TEXT("Save confirmation button works"), ClickEntry(HUD, TEXT("ActionsBox"), 0));
    TestFalse(TEXT("Save and close closes panel"), HUD->IsPanelOpen());
    TestEqual(TEXT("Saved deck has six cards"), GM->GetProfileSubsystem()->GetSavedLoadout().CardIds.Num(), 6);
    HUD->OpenPanel(ELKHomePanel::WarRoom);
    TestTrue(TEXT("Hero tab button switches the visible list"), ClickEntry(HUD, TEXT("TabsBox"), 1));
    bHeroesTab = true;
    FLKHomePanelModel Current = Model();
    const FName SecondHero = HUD->GetDraftLoadout().HeroIds[1];
    int32 RowIndex = Current.Rows.IndexOfByPredicate([](const FLKHomePanelRow& Row) { return Row.HeroSlotIndex == 0; });
    TestTrue(TEXT("Hero slot selectable"), ClickEntry(HUD, TEXT("RowsBox"), RowIndex));
    RowIndex = Current.Rows.IndexOfByPredicate([SecondHero](const FLKHomePanelRow& Row)
        { return Row.RowId == SecondHero && Row.bSelectable && Row.HeroSlotIndex == INDEX_NONE; });
    TestTrue(TEXT("Hero candidate selectable"), ClickEntry(HUD, TEXT("RowsBox"), RowIndex));
    TestEqual(TEXT("Candidate swaps into chosen slot"), HUD->GetDraftLoadout().HeroIds[0], SecondHero);
    HUD->RequestClosePanel();
    ClickEntry(HUD, TEXT("ActionsBox"), 1);
    GM->GetProfileSubsystem()->AddGold(100);
    HUD->OpenPanel(ELKHomePanel::Statue);
    TestTrue(TEXT("Upgrade button enabled"), ClickEntry(HUD, TEXT("ActionsBox"), 0));
    TestTrue(TEXT("Refresh preserves success feedback"), HUD->GetPanelStatusText().ToString().Contains(TEXT("升级成功")));
    TestEqual(TEXT("Upgrade effect applied"), GM->GetBuildingLevel(LKHomeContent::BuildingId(ELKHomeBuilding::Statue)), 2);
    HUD->OpenPanel(ELKHomePanel::Gate);
    USpinBox* Carry = Cast<USpinBox>(HUD->GetWidgetFromName(TEXT("DepartureGoldInput")));
    if (TestNotNull(TEXT("Gate offers carry gold input"), Carry))
    {
        Carry->SetValue(25);
        TestEqual(TEXT("Changing actual input updates departure amount"), GM->GetDepartureGold(), 25);
        Carry->SetValue(999);
        TestEqual(TEXT("Carry clamps to bank balance"), GM->GetDepartureGold(), GM->GetGold());
        Carry->SetValue(0);
    }
    FLKExpeditionStartRequest Request;
    FString Error;
    if (!TestTrue(TEXT("Gate test creates validated expedition input"), LKHomeContent::BuildExpeditionStartRequest(
        *GM->GetProfileSubsystem(), GM->GetGameData(), LKHomeContent::DefaultRegionId(),
        [GM](FName Id) { return GM->GetUnitRow(Id); }, [GM](FName Id) -> const ULKCardDefinition* { return GM->FindCard(Id); }, Request, Error))) { return false; }
    ULKRunSubsystem* Run = GM->GetRunSubsystem();
    TestTrue(TEXT("Gate test starts isolated run"), Run->StartNewRun(Request));
    const FGuid RunId = Run->GetRunState().RunId;
    HUD->OpenPanel(ELKHomePanel::Gate);
    TestTrue(TEXT("Abandon button opens confirmation"), ClickEntry(HUD, TEXT("ActionsBox"), 1));
    TestTrue(TEXT("Opening confirmation does not end run"), Run->HasRunInProgress());
    TestTrue(TEXT("Keep expedition button works"), ClickEntry(HUD, TEXT("ActionsBox"), 0));
    TestEqual(TEXT("Cancel preserves run identity"), Run->GetRunState().RunId, RunId);
    ClickEntry(HUD, TEXT("ActionsBox"), 1);
    TestTrue(TEXT("Explicit confirm ends expedition"), ClickEntry(HUD, TEXT("ActionsBox"), 1));
    TestTrue(TEXT("Run abandoned"), Run->GetRunPhase() == ELKRunPhase::Abandoned);
    TestEqual(TEXT("Abandon grants zero gold"), Run->GetPendingSettlement().GoldAmount, 0);
    TestTrue(TEXT("Abandon feedback remains visible"), HUD->GetPanelStatusText().ToString().Contains(TEXT("已放弃")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKHomeUIAssetsTest, "LittleKing.HomeUI.AuthoredAssetsAndContent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLKHomeUIAssetsTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("Home map exists"), LKHomeContent::DoesMapExist(TEXT("L_Home")));
    UClass* Class = LoadClass<ALKHomeGameMode>(nullptr, TEXT("/Game/blueprint/Home/BP_HomeGameMode.BP_HomeGameMode_C"));
    if (!TestNotNull(TEXT("Home blueprint exists"), Class)) { return false; }
    const ALKHomeGameMode* Defaults = Class->GetDefaultObject<ALKHomeGameMode>();
    TestNotNull(TEXT("Authored GameData is assigned"), Defaults->GameData.Get());
    UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/Home/M_HomePlaceholder.M_HomePlaceholder"));
    if (TestNotNull(TEXT("Unlit colour material exists"), Material))
    {
        FLinearColor Value;
        TestTrue(TEXT("Building material has a real Color parameter"), Material->GetVectorParameterValue(FMaterialParameterInfo(TEXT("Color")), Value));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKHomeBattleUITest, "LittleKing.HomeUI.RosterButtonsAndSafeReturn",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLKHomeBattleUITest::RunTest(const FString& Parameters)
{
    FHomeUIScope Scope;
    FTestWorldWrapper World;
    if (!World.CreateTestWorld(EWorldType::Game)) { return false; }
    World.GetTestWorld()->GetWorldSettings()->DefaultGameMode = LoadClass<AGameModeBase>(nullptr,
        TEXT("/Game/blueprint/BP_ALKBattleGameMode.BP_ALKBattleGameMode_C"));
    if (!TestTrue(TEXT("Choose first world-map battle before travel"), LKWorldMapTest::StartSelectedBattle(World.GetTestWorld()->GetGameInstance()))) { return false; }
    if (!World.BeginPlayInTestWorld()) { World.ForwardErrorMessages(this); return false; }
    ALKBattleGameMode* GM = World.GetTestWorld()->GetAuthGameMode<ALKBattleGameMode>();
    ALKPlayerController* PC = World.GetTestWorld()->SpawnActor<ALKPlayerController>();
    if (!GM || !PC) { return false; }
    GM->Tick(0.f);
    GM->AvailableHeroes.Swap(0, 1); // The same ordered roster field filled by ApplyExpeditionContext.
    ULKBattleHUDWidget* HUD = GM->GetBattleHUDWidget();
    if (!TestNotNull(TEXT("Authored battle HUD"), HUD)) { return false; }
    const TSharedRef<SWidget> Slate = HUD->TakeWidget();
    const FName ButtonNames[] = {TEXT("Btn_Knight"), TEXT("Btn_Mage"), TEXT("Btn_Ranger")};
    for (int32 Slot = 0; Slot < 3; ++Slot)
    {
        UButton* Button = Cast<UButton>(HUD->GetWidgetFromName(ButtonNames[Slot]));
        if (!TestNotNull(TEXT("Existing deployment button"), Button)) { return false; }
        Button->OnClicked.Broadcast();
        TestEqual(TEXT("Actual click selects the hero in this expedition slot"), PC->GetPlacingHeroId(), GM->AvailableHeroes[Slot]);
    }
    PC->CancelPlacement();
    TestFalse(TEXT("Cannot return home during deployment"), GM->ReturnToHome());
    for (int32 Slot = 0; Slot < 3; ++Slot)
    {
        TestTrue(TEXT("Roster hero deploys"), GM->DeployHero(ELKTeam::Player, GM->AvailableHeroes[Slot],
            FVector(-700.f + Slot * 700.f, -1300.f, 0.f)) == ELKPlayResult::Success);
    }
    GM->ForceStartBattle();
    TestFalse(TEXT("Cannot return home during combat"), GM->ReturnToHome());
    GM->ForceEndMatch(ELKTeam::Player);
    ULKRunSubsystem* Run = World.GetTestWorld()->GetGameInstance()->GetSubsystem<ULKRunSubsystem>();
    const FLKRunState Before = Run->GetRunState();
    TestTrue(TEXT("Victory has pending reward"), Run->HasPendingRewardChoice());
    ULKRunRewardWidget* Reward = CreateWidget<ULKRunRewardWidget>(World.GetTestWorld(), ULKRunRewardWidget::StaticClass());
    Reward->InitializeReward(HUD);
    const TSharedRef<SWidget> RewardSlate = Reward->TakeWidget();
    UButton* Return = Cast<UButton>(Reward->GetWidgetFromName(TEXT("Btn_ReturnHome")));
    if (!TestNotNull(TEXT("Reward screen includes return-home button"), Return)) { return false; }
    Return->OnClicked.Broadcast(); // Queues map travel; the test world is destroyed before the travel tick.
    TestFalse(TEXT("Queued return prevents consuming a reward behind the transition"), GM->ChooseRunReward(0));
    TestFalse(TEXT("Queued return prevents skipping the preserved reward"), GM->SkipRunReward());
    TestTrue(TEXT("Returning preserves pending reward"), Run->HasPendingRewardChoice());
    TestEqual(TEXT("Returning preserves run identity"), Run->GetRunState().RunId, Before.RunId);
    TestEqual(TEXT("Returning preserves reward batch"), Run->GetRunState().PendingRewardBatchId, Before.PendingRewardBatchId);
    TestFalse(TEXT("Repeated return is ignored while travel is queued"), GM->ReturnToHome());
    TestTrue(TEXT("Safe point saved"), Run->HasSavedExpedition());
    return true;
}
#endif
