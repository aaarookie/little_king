#include "LKPolishArtPreview.h"
#if WITH_EDITOR
#include "ALKBattleGameMode.h"
#include "ALKUnitBase.h"
#include "ALKHeroCamp.h"
#include "ALKHomeBuildingActor.h"
#include "ULKBattleHUDWidget.h"
#include "ULKUnitAnimationComponent.h"
#include "ULKUnitMovementComponent.h"
#include "ULKJourneyPresentationSubsystem.h"
#include "LKPolishArt.h"
#include "PaperSpriteComponent.h"
#include "PaperFlipbook.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"

void LKPolishArtPreview::Tick(ALKBattleGameMode* Mode)
{
    static int32 Frame=0;
    static TArray<TWeakObjectPtr<ALKUnitBase>> Units;
    if(!Mode->GetBattleHUDWidget()){return;}
    ++Frame; UWorld* World=Mode->GetWorld();
    auto Shot=[&](const FString& Name)
    {
        int32 W=0,H=0;World->GetFirstPlayerController()->GetViewportSize(W,H);
        const FString Dir=FPaths::ProjectSavedDir()/TEXT("Screenshots/PolishArt");
        IFileManager::Get().MakeDirectory(*Dir,true);
        FScreenshotRequest::RequestScreenshot(Dir/FString::Printf(TEXT("%s_%d.png"),*Name,H),true,false);
    };
    if(Frame==1)
    {
        for(int I=0;I<Mode->AvailableHeroes.Num();++I)
        {Mode->DeployHero(ELKTeam::Player,Mode->AvailableHeroes[I],FVector((I-1)*700.f,-1300,0));}
        Mode->ForceStartBattle();
        for(TActorIterator<ALKUnitBase> It(World);It;++It){It->Destroy();}
        for(TActorIterator<ALKHeroCamp> It(World);It;++It){It->Destroy();}
        ACameraActor* Camera=World->SpawnActor<ACameraActor>();
        Camera->SetActorLocationAndRotation(FVector(0,0,5000),FRotator(-90,0,0));
        Camera->GetCameraComponent()->ProjectionMode=ECameraProjectionMode::Orthographic;
        Camera->GetCameraComponent()->OrthoWidth=3000;
        Camera->GetCameraComponent()->bConstrainAspectRatio=false;
        World->GetFirstPlayerController()->SetViewTarget(Camera);
        Mode->GetBattleHUDWidget()->SetVisibility(ESlateVisibility::Hidden);
        for(int I=0;I<LKPolishArt::UnitIds().Num();++I)
        {
            auto* Unit=Mode->SpawnUnitForTeam(LKPolishArt::UnitIds()[I],I%7<3?ELKTeam::Player:ELKTeam::Enemy,
                FVector(975.f-I/7*650.f,(I%7-3)*600.f,0));
            if(!Unit){continue;}
            Unit->SetCombatEnabled(true);Unit->SetActorTickEnabled(false);
            Unit->GetMovementComponent()->SetComponentTickEnabled(false);
            Unit->GetAnimationComponent()->SetComponentTickEnabled(false);
            Units.Add(Unit);
        }
    }
    // Show every authored frame, including both death poses, in the real renderer.
    if(Frame>=60&&Frame<540)
    {
        const int Index=(Frame-60)/30;
        const FName State=Index<4?FName("Idle"):Index<8?FName("Move"):Index<12?FName("Attack"):Index<14?FName("Hit"):FName("Death");
        const int Local=Index<12?Index%4:Index%2;
        if((Frame-60)%30==0)
        {
            for(auto Weak:Units){if(auto* Unit=Weak.Get())
            {if(auto* Clip=LKPolishArt::Animation(Unit->GetUnitId(),State)){Unit->GetSpriteComponent()->SetSprite(Clip->GetSpriteAtFrame(Local));}}}
        }
        if((Frame-60)%30==15){Shot(FString::Printf(TEXT("Frames_%02d_%s"),Index,*State.ToString()));}
    }
    if(Frame==550)
    {
        if(auto* HUD=World->GetFirstPlayerController()->GetHUD()){HUD->bShowHUD=false;}
        for(auto Weak:Units){if(Weak.IsValid()){Weak->Destroy();}} Units.Reset();
        for(int I=0;I<9;++I)
        {
            const bool Statue=I<4;const int Level=Statue?I+1:I-3;
            auto* Building=World->SpawnActor<ALKHomeBuildingActor>();
            Building->SetActorLocation(FVector(800.f-I/5*1100.f,(I%5-2)*750.f,0));
            Building->InitializeBuilding(Statue?FName("Home_StatueSaintMaria"):FName("Home_Treasury"),
                FText::FromString(Statue?TEXT("圣玛丽亚神像"):TEXT("金库")),true);
            Building->SetDisplayedLevel(Level);
        }
    }
    if(Frame==620){Shot(TEXT("HomeLevels"));}
    if(Frame==650)
    {
        UE_LOG(LogTemp,Display,TEXT("POLISH_PREVIEW_OK frames=16 buildings=9 musicVoices=%d"),
            Mode->GetGameInstance()->GetSubsystem<ULKJourneyPresentationSubsystem>()->GetMusicVoiceCount());
        FPlatformMisc::RequestExit(false);
    }
}
#endif
