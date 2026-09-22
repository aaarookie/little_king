#include "LKPolishArtPreview.h"
#if WITH_EDITOR
#include "ULKJourneyPresentationSubsystem.h"
#include "ULKSaveSlotSubsystem.h"
#include "ALKHomeGameMode.h"
#include "ALKStartMenuGameMode.h"
#include "ALKBattleGameMode.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"
void LKPolishArtPreview::TickJourney(ULKJourneyPresentationSubsystem* Journey,float DeltaTime)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("PolishTravelPreview"))){return;}
    UWorld* World=Journey->GetWorld();if(!World||!World->IsGameWorld()||!World->GetFirstPlayerController()){return;}
    static int Stage=0;static float Time=0;static bool CurtainShot=false;
    Time+=DeltaTime;
    auto Shot=[&](const TCHAR* Name)
    {
        int32 W=0,H=0;World->GetFirstPlayerController()->GetViewportSize(W,H);
        const FString Dir=FPaths::ProjectSavedDir()/TEXT("Screenshots/PolishArt");IFileManager::Get().MakeDirectory(*Dir,true);
        FScreenshotRequest::RequestScreenshot(Dir/FString::Printf(TEXT("Travel_%s_%d.png"),Name,H),true,false);
    };
    if(Journey->IsTravelling())
    {
        if(!CurtainShot&&Time>.11f){Shot(TEXT("Curtain"));CurtainShot=true;}
        return;
    }
    auto* Saves=World->GetGameInstance()->GetSubsystem<ULKSaveSlotSubsystem>();
    auto* Mode=World->GetAuthGameMode();
    if(Time<3.f){return;}
    if(Stage==0&&Cast<ALKStartMenuGameMode>(Mode)){Shot(TEXT("Menu"));Stage=1;Time=0;}
    else if(Stage==1)
    {
        if(!Saves->CreateNewGame()){UE_LOG(LogTemp,Error,TEXT("POLISH_TRAVEL_CREATE_FAILED"));FPlatformMisc::RequestExit(false);return;}
        Journey->Travel(World,"/Game/Maps/L_Home");Stage=2;Time=0;
    }
    else if(Stage==2&&Cast<ALKHomeGameMode>(Mode)){Shot(TEXT("Home"));Stage=3;Time=0;}
    else if(Stage==3){Journey->Travel(World,"/Game/Maps/L_BattleTest");Stage=4;Time=0;}
    else if(Stage==4&&Cast<ALKBattleGameMode>(Mode)){Shot(TEXT("Battle"));Stage=5;Time=0;}
    else if(Stage==5){Journey->Travel(World,"/Game/Maps/L_Home");Stage=6;Time=0;}
    else if(Stage==6&&Cast<ALKHomeGameMode>(Mode)){Shot(TEXT("ReturnedHome"));Stage=7;Time=0;}
    else if(Stage==7)
    {
        UE_LOG(LogTemp,Display,TEXT("POLISH_TRAVEL_OK menu-home-battle-home music=%s voices=%d curtain=%d"),
            *Journey->GetMusicScene().ToString(),Journey->GetMusicVoiceCount(),Journey->IsTravelling());
        Saves->ResetSession();Saves->DeleteSlot(0);FPlatformMisc::RequestExit(false);
    }
    if(Time>40.f){UE_LOG(LogTemp,Error,TEXT("POLISH_TRAVEL_TIMEOUT stage=%d"),Stage);FPlatformMisc::RequestExit(false);}
}
#endif
