#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/OutputDeviceNull.h"
#include "Tests/AutomationCommon.h"
#include "Components/Button.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "../ALKStartMenuGameMode.h"
#include "../ALKStartMenuPlayerController.h"
#include "../ULKStartMenuWidget.h"
#include "../ULKSaveSlotSubsystem.h"
#include "../ULKProfileSubsystem.h"
#include "../ULKProfileSaveGame.h"
#include "../ULKRunSubsystem.h"
#include "../ULKRunSaveGame.h"
#include "../ULKGameViewportClient.h"
#include "../LKMoneyCommand.h"
#include "../LKHomeContent.h"
#include "../LKUnitContent.h"
#include "../ULKGameData.h"
#include "../ULKCardDefinition.h"

namespace
{
	constexpr auto Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	struct FSaveScope
	{
		ULKSaveSlotSubsystem* Saves;
		explicit FSaveScope(UGameInstance* Instance)
		{
			Saves = Instance->GetSubsystem<ULKSaveSlotSubsystem>();
			Saves->SetNamespaceForTest(TEXT("LittleKing_OptimizationTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits));
			ULKProfileSubsystem::SetPersistentProfileEnabledForTest(true);
		}
		~FSaveScope()
		{
			Saves->ResetSession();
			for (int32 Index = 0; Index < 8; ++Index)
			{
				for (const FString& Name : {Saves->ProfileBase(Index) + TEXT("_A"), Saves->ProfileBase(Index) + TEXT("_B"), Saves->RunSlot(Index), Saves->DeleteMarker(Index)})
				{ if (UGameplayStatics::DoesSaveGameExist(Name, 0)) { UGameplayStatics::DeleteGameInSlot(Name, 0); } }
			}
			ULKProfileSubsystem::ResetTestHooks();
		}
	};
	bool Click(ULKStartMenuWidget* Menu, const TCHAR* Name)
	{
		UUserWidget* Entry = Cast<UUserWidget>(Menu->GetWidgetFromName(Name));
		UButton* Button = Entry ? Cast<UButton>(Entry->GetWidgetFromName(TEXT("RowButton"))) : nullptr;
		if (!Button || !Button->GetIsEnabled()) { return false; }
		Button->OnClicked.Broadcast(); return true;
	}
	bool StartRun(UGameInstance* Instance)
	{
		ULKGameData* Data = NewObject<ULKGameData>(); Data->EnsureDefaultDecks(); Data->EnsureCardLibrary();
		FLKExpeditionStartRequest Request; FString Error;
		if (!LKHomeContent::BuildExpeditionStartRequest(*Instance->GetSubsystem<ULKProfileSubsystem>(), Data, LKHomeContent::DefaultRegionId(),
			[](FName Id) { return LKUnitContent::Find(Id); },
			[Data](FName Id) -> const ULKCardDefinition*
			{
				for (const ULKCardDefinition* Card : Data->CardLibrary) { if (Card && Card->CardId == Id) { return Card; } }
				return nullptr;
			}, Request, Error)) { return false; }
		return Instance->GetSubsystem<ULKRunSubsystem>()->StartNewRun(Request);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKSaveSlotsTest, "LittleKing.Optimization1.SaveIsolationAndRecovery", Flags)
bool FLKSaveSlotsTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper World; World.CreateTestWorld(EWorldType::Game);
	World.GetTestWorld()->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
	if (!World.BeginPlayInTestWorld()) { World.ForwardErrorMessages(this); return false; }
	UGameInstance* Instance = World.GetTestWorld()->GetGameInstance(); FSaveScope Scope(Instance);
	ULKSaveSlotSubsystem* Saves = Scope.Saves;
	ULKProfileSubsystem* Profile = Instance->GetSubsystem<ULKProfileSubsystem>();
	ULKRunSubsystem* Run = Instance->GetSubsystem<ULKRunSubsystem>();
	TestEqual(TEXT("No continue on first launch"), Saves->GetMostRecentSlot(), INDEX_NONE);
	TestFalse(TEXT("Invalid index cannot load"), Saves->LoadSlot(-1));
	TestTrue(TEXT("First new game created"), Saves->CreateNewGame());
	TestEqual(TEXT("First slot selected"), Saves->GetActiveSlot(), 0);
	const FGuid FirstProfileId = Profile->GetProfile().ProfileId;
	TestTrue(TEXT("Save gold to selected profile"), Profile->AddGold(137));
	TestTrue(TEXT("Create a real expedition with home snapshot"), StartRun(Instance));
	const FGuid FirstRunId = Run->GetRunState().RunId;
	TestEqual(TEXT("Active expedition resumes in battle"), Saves->GetEntryMap(), FName(TEXT("L_BattleTest")));
	TestFalse(TEXT("Cannot overwrite selected game"), Saves->CreateNewGame());
	TestFalse(TEXT("Cannot delete during game"), Saves->DeleteSlot(0));
	Saves->ResetSession();
	TestTrue(TEXT("Second game created"), Saves->CreateNewGame());
	TestEqual(TEXT("Independent gold starts at zero"), Profile->GetGold(), 0);
	TestFalse(TEXT("Independent game has no expedition"), Run->HasRun());
	TestEqual(TEXT("New game enters home"), Saves->GetEntryMap(), FName(TEXT("L_Home")));
	const FGuid SecondProfileId = Profile->GetProfile().ProfileId;
	Saves->ResetSession();
	TestTrue(TEXT("First game reloads"), Saves->LoadSlot(0));
	TestEqual(TEXT("Gold retained"), Profile->GetGold(), 137);
	TestEqual(TEXT("Profile identity retained"), Profile->GetProfile().ProfileId, FirstProfileId);
	TestEqual(TEXT("Expedition identity retained"), Run->GetRunState().RunId, FirstRunId);
	TestEqual(TEXT("Loaded older game becomes recent"), Saves->GetMostRecentSlot(), 0);
	Saves->ResetSession();
	for (int32 Index = 2; Index < 8; ++Index)
	{
		TestTrue(TEXT("Fill empty slot"), Saves->CreateNewGame());
		TestEqual(TEXT("Uses first free slot"), Saves->GetActiveSlot(), Index); Saves->ResetSession();
	}
	TestFalse(TEXT("Ninth game is rejected"), Saves->CreateNewGame());
	TestEqual(TEXT("Exactly eight occupied slots"), Saves->ListSlots().FilterByPredicate([](const FLKSaveSlotSummary& Row) { return Row.bExists; }).Num(), 8);
	TestTrue(TEXT("Delete first game"), Saves->DeleteSlot(0));
	TestFalse(TEXT("Run removed along with profile"), UGameplayStatics::DoesSaveGameExist(Saves->RunSlot(0), 0));
	TestTrue(TEXT("Other save still loads"), Saves->LoadSlot(1));
	TestEqual(TEXT("Other profile identity unchanged"), Profile->GetProfile().ProfileId, SecondProfileId); Saves->ResetSession();
	TestTrue(TEXT("Freed position reusable"), Saves->CreateNewGame());
	TestEqual(TEXT("Reuses position zero"), Saves->GetActiveSlot(), 0);
	TestTrue(TEXT("Reused slot is a new identity"), Profile->GetProfile().ProfileId != FirstProfileId);
	TestEqual(TEXT("Reused slot has fresh gold"), Profile->GetGold(), 0);
	TestFalse(TEXT("Reused slot has no old run"), Run->HasRun()); Saves->ResetSession();
	// 模拟删除中断：原档仍在，但删除标记已落盘。
	UGameplayStatics::SaveGameToSlot(NewObject<ULKDeletedSlotSaveGame>(), Saves->DeleteMarker(0), 0);
	TestFalse(TEXT("Interrupted delete cannot resurrect"), Saves->InspectSlot(0).bCanLoad);
	TestTrue(TEXT("Interrupted delete retains occupied status"), Saves->InspectSlot(0).bExists);
	TestTrue(TEXT("Retry completes interrupted delete"), Saves->DeleteSlot(0));
	// 损坏/未来格式占位，不能被新游戏悄悄覆盖；A/B 中有效副本仍可读。
	ULKProfileSaveGame* Future = NewObject<ULKProfileSaveGame>(); Future->SaveVersion = 999;
	UGameplayStatics::SaveGameToSlot(Future, Saves->ProfileBase(0) + TEXT("_A"), 0);
	TestTrue(TEXT("Unknown version remains occupied"), Saves->InspectSlot(0).bExists);
	TestFalse(TEXT("Unknown version is not loadable"), Saves->LoadSlot(0));
	TestFalse(TEXT("Unknown version not overwritten by ninth game"), Saves->CreateNewGame());
	TestTrue(TEXT("Explicit deletion can remove unknown version"), Saves->DeleteSlot(0));
	UGameplayStatics::SaveGameToSlot(Future, Saves->ProfileBase(1) + TEXT("_B"), 0);
	TestTrue(TEXT("Valid profile backup still loads"), Saves->LoadSlot(1)); Saves->ResetSession();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKStartMenuInteractionTest, "LittleKing.Optimization1.MenuButtonsAndAssets", Flags)
bool FLKStartMenuInteractionTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Start menu map exists"), LKHomeContent::DoesMapExist(TEXT("L_StartMenu")));
	TestEqual(TEXT("Project uses the custom console viewport"), GEngine->GameViewportClientClass.Get(), ULKGameViewportClient::StaticClass());
	FTestWorldWrapper World; World.CreateTestWorld(EWorldType::Game);
	World.GetTestWorld()->GetWorldSettings()->DefaultGameMode = ALKStartMenuGameMode::StaticClass();
	if (!World.BeginPlayInTestWorld()) { World.ForwardErrorMessages(this); return false; }
	FSaveScope Scope(World.GetTestWorld()->GetGameInstance());
	World.GetTestWorld()->SpawnActor<ALKStartMenuPlayerController>();
	ALKStartMenuGameMode* GM = World.GetTestWorld()->GetAuthGameMode<ALKStartMenuGameMode>(); GM->Tick(0);
	ULKStartMenuWidget* Menu = GM->GetMenuWidget();
	if (!TestNotNull(TEXT("Native start menu created"), Menu)) { return false; }
	const TSharedRef<SWidget> Slate = Menu->TakeWidget(); Menu->SetSuppressTravelForTest(true); Menu->ShowMainMenu();
	TestFalse(TEXT("Empty continue disabled"), Click(Menu, TEXT("ContinueButton")));
	TestTrue(TEXT("Settings interface reachable"), Click(Menu, TEXT("SettingsButton")));
	TestTrue(TEXT("Settings explains pending feature"), Menu->GetStatusText().Contains(TEXT("暂未开放")));
	TestTrue(TEXT("Actual New Game button creates save"), Click(Menu, TEXT("NewGameButton")));
	TestEqual(TEXT("New Game selects first slot"), Scope.Saves->GetActiveSlot(), 0);
	Scope.Saves->ResetSession(); Menu->ShowMainMenu();
	TestTrue(TEXT("Continue button loads existing game"), Click(Menu, TEXT("ContinueButton")));
	TestEqual(TEXT("Continue selects recent save"), Scope.Saves->GetActiveSlot(), 0);
	Scope.Saves->ResetSession(); Menu->ShowMainMenu();
	TestTrue(TEXT("Read menu opens"), Click(Menu, TEXT("LoadMenuButton")));
	TestTrue(TEXT("Shows load menu"), Menu->IsLoadMenuOpen());
	TestFalse(TEXT("Empty slot cannot load"), Click(Menu, TEXT("LoadSlot1Button")));
	TestTrue(TEXT("Delete opens confirmation"), Click(Menu, TEXT("DeleteSlot0Button")));
	TestEqual(TEXT("Confirmation targets exact slot"), Menu->GetPendingDelete(), 0);
	TestTrue(TEXT("Opening confirmation preserves save"), Scope.Saves->InspectSlot(0).bCanLoad);
	TestTrue(TEXT("Cancel button works"), Click(Menu, TEXT("CancelDeleteButton")));
	TestTrue(TEXT("Cancel preserves progress"), Scope.Saves->InspectSlot(0).bCanLoad);
	TestTrue(TEXT("Read slot button loads selected game"), Click(Menu, TEXT("LoadSlot0Button")));
	Scope.Saves->ResetSession(); Menu->ShowLoadMenu();
	Click(Menu, TEXT("DeleteSlot0Button"));
	TestTrue(TEXT("Confirm button deletes save"), Click(Menu, TEXT("ConfirmDeleteButton")));
	TestFalse(TEXT("Deleted save no longer exists"), Scope.Saves->InspectSlot(0).bExists);
	TestTrue(TEXT("Back button returns to main"), Click(Menu, TEXT("BackButton")));
	TestFalse(TEXT("Main menu visible again"), Menu->IsLoadMenuOpen());
	for (int32 Index = 0; Index < 8; ++Index) { Scope.Saves->CreateNewGame(); Scope.Saves->ResetSession(); }
	Menu->ShowMainMenu(); TestFalse(TEXT("New game disabled when all slots full"), Click(Menu, TEXT("NewGameButton")));
	TestTrue(TEXT("Full menu still permits reading/deleting"), Click(Menu, TEXT("LoadMenuButton")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKMoneyCommandTest, "LittleKing.Optimization1.MoneyConsoleCommand", Flags)
bool FLKMoneyCommandTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper World; World.CreateTestWorld(EWorldType::Game);
	World.GetTestWorld()->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
	if (!World.BeginPlayInTestWorld()) { World.ForwardErrorMessages(this); return false; }
	UGameInstance* Instance = World.GetTestWorld()->GetGameInstance(); FSaveScope Scope(Instance);
	ULKProfileSubsystem* Profile = Instance->GetSubsystem<ULKProfileSubsystem>();
	ULKGameViewportClient* Viewport = NewObject<ULKGameViewportClient>(GEngine); FOutputDeviceNull Out;
	TestTrue(TEXT("Command handled before choosing a game"), Viewport->Exec(World.GetTestWorld(), TEXT("show me the money"), Out));
	TestEqual(TEXT("Menu cheat does not create a save"), Scope.Saves->GetMostRecentSlot(), INDEX_NONE);
	Scope.Saves->CreateNewGame();
	TestTrue(TEXT("Exact command bypasses engine SHOW handler"), Viewport->Exec(World.GetTestWorld(), TEXT("show me the money"), Out));
	TestEqual(TEXT("Default gives 100 gold"), Profile->GetGold(), 100);
	Viewport->Exec(World.GetTestWorld(), TEXT("show me the money 250"), Out);
	TestEqual(TEXT("Explicit quantity applied"), Profile->GetGold(), 350);
	Viewport->Exec(World.GetTestWorld(), TEXT("SHOW  ME THE MONEY 50"), Out);
	TestEqual(TEXT("Whitespace and case supported"), Profile->GetGold(), 400);
	for (const TCHAR* Command : {TEXT("show me the money -1"), TEXT("show me the money 1.5"), TEXT("show me the money 999999999999999999999"), TEXT("show me the money 2147483648"), TEXT("show me the money abc"), TEXT("show me the money 2 3"), TEXT("show me the money 0")})
	{
		Viewport->Exec(World.GetTestWorld(), Command, Out); TestEqual(TEXT("Invalid or zero input does not alter gold"), Profile->GetGold(), 400);
	}
	TestFalse(TEXT("Normal engine SHOW remains untouched"), LKMoneyCommand::TryExecute(World.GetTestWorld(), TEXT("show collision"), Out));
	TestFalse(TEXT("Partial phrase not intercepted"), LKMoneyCommand::TryExecute(World.GetTestWorld(), TEXT("show me the"), Out));
	Viewport->Exec(World.GetTestWorld(), TEXT("show me the money 2147483647"), Out);
	TestEqual(TEXT("Gold saturates without integer overflow"), Profile->GetGold(), MAX_int32);
	Scope.Saves->ResetSession(); TestTrue(TEXT("Cheat gold reloads"), Scope.Saves->LoadSlot(0));
	TestEqual(TEXT("Cheat gold persisted"), Profile->GetGold(), MAX_int32);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKLegacySlotTest, "LittleKing.Optimization1.LegacyAndCrossSlotProtection", Flags)
bool FLKLegacySlotTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper World; World.CreateTestWorld(EWorldType::Game);
	World.GetTestWorld()->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
	if (!World.BeginPlayInTestWorld()) { World.ForwardErrorMessages(this); return false; }
	UGameInstance* Instance = World.GetTestWorld()->GetGameInstance();
	ULKSaveSlotSubsystem* Saves = Instance->GetSubsystem<ULKSaveSlotSubsystem>();
	// Only inspect the naming contract before isolating all actual file access.
	TestEqual(TEXT("Slot one preserves legacy profile names"), Saves->ProfileBase(0), FString(TEXT("LittleKing_Profile")));
	TestEqual(TEXT("Slot one preserves legacy run name"), Saves->RunSlot(0), FString(TEXT("LittleKing_Run")));
	FSaveScope Scope(Instance);
	ULKRunSubsystem* Run = Instance->GetSubsystem<ULKRunSubsystem>();
	ULKProfileSubsystem* Profile = Instance->GetSubsystem<ULKProfileSubsystem>();
	Saves->CreateNewGame(); Profile->AddGold(88); StartRun(Instance);
	ULKRunSaveGame* OriginalRun = Cast<ULKRunSaveGame>(UGameplayStatics::LoadGameFromSlot(Saves->RunSlot(0), 0));
	if (!TestNotNull(TEXT("Authored run saved"), OriginalRun)) { return false; }
	Saves->ResetSession(); Saves->CreateNewGame(); Saves->ResetSession();
	UGameplayStatics::SaveGameToSlot(OriginalRun, Saves->RunSlot(1), 0);
	TestFalse(TEXT("Run from a different profile is rejected"), Saves->LoadSlot(1));
	TestTrue(TEXT("Mismatched run is preserved"), UGameplayStatics::DoesSaveGameExist(Saves->RunSlot(1), 0));
	UGameplayStatics::DeleteGameInSlot(Saves->RunSlot(1), 0);
	TestTrue(TEXT("Unrelated valid save remains usable"), Saves->LoadSlot(0)); Saves->ResetSession();
	// Time fields absent in old saves default to zero; file time is used for menu metadata.
	for (const FString& Name : {Saves->ProfileBase(0) + TEXT("_A"), Saves->ProfileBase(0) + TEXT("_B")})
	{
		ULKProfileSaveGame* OldProfile = Cast<ULKProfileSaveGame>(UGameplayStatics::LoadGameFromSlot(Name, 0));
		if (OldProfile) { OldProfile->SavedAtUtc = FDateTime(); UGameplayStatics::SaveGameToSlot(OldProfile, Name, 0); }
	}
	UGameplayStatics::DeleteGameInSlot(Saves->RunSlot(0), 0);
	const FLKSaveSlotSummary OldRow = Saves->InspectSlot(0);
	TestTrue(TEXT("Pre-timestamp profile still loadable"), OldRow.bCanLoad && OldRow.SavedAtUtc.GetTicks() > 0);
	TestTrue(TEXT("Old profile retains gold"), Saves->LoadSlot(0) && Profile->GetGold() == 88); Saves->ResetSession();
	Saves->DeleteSlot(0);
	OriginalRun->RunState.SchemaVersion = 4; OriginalRun->RunState.ProfileId.Invalidate(); OriginalRun->RunState.bHomeRewardEligible = false;
	OriginalRun->SavedAtUtc = FDateTime();
	UGameplayStatics::SaveGameToSlot(OriginalRun, Saves->RunSlot(0), 0);
	TestTrue(TEXT("Pre-home run-only legacy save loads"), Saves->LoadSlot(0));
	TestEqual(TEXT("Legacy run migrates to current schema"), Run->GetRunState().SchemaVersion, ULKRunSubsystem::CurrentSchemaVersion);
	TestEqual(TEXT("Legacy run keeps run identity"), Run->GetRunState().RunId, OriginalRun->RunState.RunId);
	TestEqual(TEXT("Legacy run associates with new default home"), Run->GetRunState().ProfileId, Profile->GetProfile().ProfileId);
	TestEqual(TEXT("Legacy default home starts with zero gold"), Profile->GetGold(), 0);
	TestFalse(TEXT("Legacy migration never creates retroactive settlement"), Run->GetRunState().bHomeRewardEligible);
	Saves->ResetSession(); Saves->DeleteSlot(0);
	OriginalRun->RunState.SchemaVersion = 5;
	UGameplayStatics::SaveGameToSlot(OriginalRun, Saves->RunSlot(0), 0);
	TestFalse(TEXT("Current-schema run missing home cannot silently create replacement"), Saves->LoadSlot(0));
	TestFalse(TEXT("Missing home remains missing"), UGameplayStatics::DoesSaveGameExist(Saves->ProfileBase(0) + TEXT("_A"), 0));
	return true;
}
#endif
