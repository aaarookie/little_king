#include "ALKStartMenuGameMode.h"
#include "ALKStartMenuPlayerController.h"
#include "ULKStartMenuWidget.h"
#include "ULKSaveSlotSubsystem.h"
#include "ULKProfileSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#if WITH_EDITOR
#include "ULKRunNodeSelectWidget.h"
#include "ULKWorldMapWidget.h"
#include "ULKRunSubsystem.h"
#include "LKHomeContent.h"
#include "LKUnitContent.h"
#include "ULKGameData.h"
#include "ULKCardDefinition.h"
#endif

ALKStartMenuGameMode::ALKStartMenuGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = nullptr;
	HUDClass = nullptr;
	PlayerControllerClass = ALKStartMenuPlayerController::StaticClass();
	MenuWidgetClass = ULKStartMenuWidget::StaticClass();
}
void ALKStartMenuGameMode::BeginPlay()
{
	Super::BeginPlay();
	ULKSaveSlotSubsystem* Saves = GetGameInstance()->GetSubsystem<ULKSaveSlotSubsystem>();
	Saves->ResetSession();
#if WITH_EDITOR
	bPreviewMode = FParse::Param(FCommandLine::Get(), TEXT("StartMenuPreview"));
	bWorldMapPreview = FParse::Param(FCommandLine::Get(), TEXT("WorldMapPreview"));
	if (bPreviewMode || bWorldMapPreview) { Saves->SetNamespaceForTest(TEXT("LittleKing_MenuPreview_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)); }
#endif
	TryCreateMenu();
}
void ALKStartMenuGameMode::TryCreateMenu()
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (MenuWidget || !PC) { return; }
	MenuWidget = CreateWidget<ULKStartMenuWidget>(GetWorld(), MenuWidgetClass ? MenuWidgetClass.Get() : ULKStartMenuWidget::StaticClass());
	if (MenuWidget)
	{
		MenuWidget->InitializeMenu(GetGameInstance()->GetSubsystem<ULKSaveSlotSubsystem>());
		MenuWidget->AddToViewport(100);
	}
}
void ALKStartMenuGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TryCreateMenu();
#if WITH_EDITOR
	if (bWorldMapPreview) { TickWorldMapPreview(); return; }
	if (bPreviewMode && MenuWidget)
	{
		++PreviewFrame;
		ULKSaveSlotSubsystem* Saves = GetGameInstance()->GetSubsystem<ULKSaveSlotSubsystem>();
		// 只在显式截图模式下创建独立测试档；普通启动不产生任何新存档。
		if (PreviewFrame == 180)
		{
			for (int32 Index = 0; Index < 8; ++Index)
			{
				if (Saves->CreateNewGame()) { GetGameInstance()->GetSubsystem<ULKProfileSubsystem>()->AddGold((Index + 1) * 100); }
				Saves->ResetSession();
			}
			MenuWidget->ShowMainMenu();
		}
		if (PreviewFrame == 270) { MenuWidget->ShowLoadMenu(); }
		if (PreviewFrame == 360) { MenuWidget->RequestDelete(2); }
		if (PreviewFrame >= 150 && (PreviewFrame - 150) % 90 == 0 && PreviewFrame <= 420)
		{
			int32 Width = 0, Height = 0;
			GetWorld()->GetFirstPlayerController()->GetViewportSize(Width, Height);
			const FString Dir = FPaths::ProjectSavedDir() / TEXT("Screenshots/StartMenu");
			IFileManager::Get().MakeDirectory(*Dir, true);
			FScreenshotRequest::RequestScreenshot(Dir / FString::Printf(TEXT("%dx%d-%02d.png"), Width, Height, (PreviewFrame - 150) / 90), true, false);
		}
		if (PreviewFrame == 480)
		{
			for (int32 Index = 0; Index < 8; ++Index) { Saves->DeleteSlot(Index); }
			FPlatformMisc::RequestExit(false);
		}
	}
#endif
}

#if WITH_EDITOR
// 显式命令行截图模式：独立命名空间及正常出征 API，不读取或改写玩家档。
void ALKStartMenuGameMode::TickWorldMapPreview()
{
    if (!MenuWidget) { return; }
    ++PreviewFrame;
    ULKSaveSlotSubsystem* Saves = GetGameInstance()->GetSubsystem<ULKSaveSlotSubsystem>();
    ULKRunSubsystem* Run = GetGameInstance()->GetSubsystem<ULKRunSubsystem>();
    if (PreviewFrame == 30)
    {
        if (!Saves->CreateNewGame()) { FPlatformMisc::RequestExit(false); return; }
        ULKProfileSubsystem* Profile = GetGameInstance()->GetSubsystem<ULKProfileSubsystem>();
        Profile->AddGold(300);
        ULKGameData* Data = NewObject<ULKGameData>(); Data->EnsureDefaultDecks(); Data->EnsureCardLibrary();
        FLKExpeditionStartRequest Request; FString Error;
        if (!LKHomeContent::BuildExpeditionStartRequest(*Profile,Data,LKHomeContent::DefaultRegionId(),
            [](FName Id){return LKUnitContent::Find(Id);},[Data](FName Id)->const ULKCardDefinition*
            {for(const ULKCardDefinition* Card:Data->CardLibrary){if(Card&&Card->CardId==Id){return Card;}}return nullptr;},Request,Error))
        { FPlatformMisc::RequestExit(false); return; }
        Request.StartingGold=50; Request.Seed=9152026;
        for (FLKRunHeroState& Hero:Request.Heroes) { Hero.Health=Hero.MaxHealth*.4f; }
        if (!Run->StartNewRun(Request)) { FPlatformMisc::RequestExit(false); return; }
        MenuWidget->SetVisibility(ESlateVisibility::Collapsed);
        WorldMapPreviewWidget=CreateWidget<ULKRunNodeSelectWidget>(GetWorld(),ULKRunNodeSelectWidget::StaticClass());
        WorldMapPreviewWidget->InitializeRunView(Run);WorldMapPreviewWidget->AddToViewport(110);
    }
    if (!WorldMapPreviewWidget) { return; }
    if (PreviewFrame==150)
    {
        if (ULKWorldMapWidget* Map=Cast<ULKWorldMapWidget>(WorldMapPreviewWidget->GetWidgetFromName(TEXT("WorldMapCanvas"))))
        { Map->FocusRegion(TEXT("World_Region0")); }
    }
    if (PreviewFrame==240) { Run->SelectNode(TEXT("World_R0_L1_N0"));WorldMapPreviewWidget->RefreshNodeSelect(); }
    if (PreviewFrame==330)
    { Run->ResolveServiceNode(TEXT("LeaveMarket"));Run->SelectNode(TEXT("World_R0_L2_N0"));WorldMapPreviewWidget->RefreshNodeSelect(); }
    if (PreviewFrame>=120 && (PreviewFrame-120)%90==0 && PreviewFrame<=390)
    {
        int32 Width=0,Height=0;GetWorld()->GetFirstPlayerController()->GetViewportSize(Width,Height);
        const FString Dir=FPaths::ProjectSavedDir()/TEXT("Screenshots/WorldMap");IFileManager::Get().MakeDirectory(*Dir,true);
        FScreenshotRequest::RequestScreenshot(Dir/FString::Printf(TEXT("%dx%d-%02d.png"),Width,Height,(PreviewFrame-120)/90),true,false);
    }
    if (PreviewFrame==450)
    { Saves->ResetSession();for(int32 I=0;I<8;++I){Saves->DeleteSlot(I);}FPlatformMisc::RequestExit(false); }
}
#endif
