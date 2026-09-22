#include "LKMovementFixPreview.h"
#if WITH_EDITOR
#include "ALKBattleGameMode.h"
#include "ALKUnitBase.h"
#include "ALKHeroCamp.h"
#include "ULKBattleHUDWidget.h"
#include "ULKUnitAnimationComponent.h"
#include "ULKUnitMovementComponent.h"
#include "ULKGameData.h"
#include "LKPolishArt.h"
#include "LKWorldArt.h"
#include "PaperSpriteComponent.h"
#include "PaperSprite.h"
#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"
#include "ShaderCompiler.h"

void LKMovementFixPreview::Tick(ALKBattleGameMode* Mode)
{
    static int32 Frame=0;
    static TArray<TWeakObjectPtr<ALKUnitBase>> Units;
    static TWeakObjectPtr<ACameraActor> Camera;
    if(!Mode->GetBattleHUDWidget()){return;}
    // Cold editor caches can briefly draw the compiling-material placeholder.
    // Wait for the real shader before advancing capture frames.
    if(GShaderCompilingManager&&GShaderCompilingManager->IsCompiling()){return;}
    ++Frame;UWorld* World=Mode->GetWorld();
    auto Shot=[&](const FString& Name)
    {
        int32 W=0,H=0;World->GetFirstPlayerController()->GetViewportSize(W,H);
        const FString Dir=FPaths::ProjectSavedDir()/TEXT("Screenshots/MovementFix");
        IFileManager::Get().MakeDirectory(*Dir,true);
        FScreenshotRequest::RequestScreenshot(Dir/FString::Printf(TEXT("%s_%d.png"),*Name,H),true,false);
    };
    auto Clear=[&]()
    {for(TActorIterator<ALKUnitBase> It(World);It;++It){It->Destroy();}Units.Reset();};
    auto Add=[&](FName Id,ELKTeam Team,FVector Position)
    {
        // Use a legal staging point, then arrange deliberate visual overlaps.
        // Normal gameplay must continue rejecting deployment inside a camp.
        auto* Unit=Mode->SpawnUnitForTeam(Id,Team,FVector(900,-1600,0));
        if(Unit)
        {
            Unit->SetActorLocation(Position);
            Unit->SetCombatEnabled(true);Unit->SetActorTickEnabled(false);
            Unit->GetMovementComponent()->SetFieldBounds(FVector2D(1600,2600));
            Unit->GetMovementComponent()->SetComponentTickEnabled(false);
            Unit->GetAnimationComponent()->SetComponentTickEnabled(false);
            Units.Add(Unit);
        }
        return Unit;
    };
    if(Frame==1)
    {
        for(int I=0;I<Mode->AvailableHeroes.Num();++I)
        {Mode->DeployHero(ELKTeam::Player,Mode->AvailableHeroes[I],FVector((I-1)*700.f,-1300,0));}
        Mode->ForceStartBattle();Clear();
        Camera=World->SpawnActor<ACameraActor>();
        Camera->SetActorLocationAndRotation(FVector(0,0,5000),FRotator(-90,0,0));
        Camera->GetCameraComponent()->ProjectionMode=ECameraProjectionMode::Orthographic;
        Camera->GetCameraComponent()->OrthoWidth=2500;
        Camera->GetCameraComponent()->bConstrainAspectRatio=false;
        World->GetFirstPlayerController()->SetViewTarget(Camera.Get());
        Mode->GetBattleHUDWidget()->SetVisibility(ESlateVisibility::Hidden);
        if(auto* HUD=World->GetFirstPlayerController()->GetHUD()){HUD->bShowHUD=false;}
        for(int I=0;I<LKPolishArt::WalkingUnitIds().Num();++I)
        {Add(LKPolishArt::WalkingUnitIds()[I],I%2?ELKTeam::Enemy:ELKTeam::Player,FVector(840.f-I/6*560.f,(I%6-2.5f)*650.f-120,0));}
        if(Units.Num()!=24){UE_LOG(LogTemp,Error,TEXT("MOVEMENT_FIX_PREVIEW_FAILED missing walkers: %d"),Units.Num());FPlatformMisc::RequestExit(false);return;}
    }
    if(Frame>=60&&Frame<180)
    {
        const bool Forward=Frame<120;
        const bool Walking=Frame<108||(Frame>=120&&Frame<168);
        for(auto Weak:Units){if(auto* Unit=Weak.Get())
        {
            auto* Move=Unit->GetMovementComponent();
            if(Walking)
            {
                Move->MoveToward(Unit->GetActorLocation()+FVector(0,Forward?500:-500,0),100);
                Move->TickComponent(.05f,LEVELTICK_All,nullptr);
            }
            else{Move->Stop();}
            Unit->GetAnimationComponent()->TickComponent(.05f,LEVELTICK_All,nullptr);
        }}
        if(Frame%6==5&&Walking){Shot(FString::Printf(TEXT("Walk_%s_%02d"),Forward?TEXT("Right"):TEXT("Left"),(Frame%60)/6));}
        if(Frame==115||Frame==175){Shot(Forward?TEXT("Stopped_Right"):TEXT("Stopped_Left"));}
    }
    if(Frame==190)
    {
        Clear();Camera->GetCameraComponent()->OrthoWidth=900;
        for(int I=0;I<3;++I)
        {
            auto* Camp=World->SpawnActor<ALKHeroCamp>();
            Camp->SetActorLocation(FVector(30,(I-1)*400,0));
            FLKUnitRow Row;Row.UnitId="Building_HeroCamp";Row.UnitClass=ELKUnitClass::Building;
            Row.Sprite=LKWorldArt::CampSprite(I==0?FName("Hero_Knight"):I==1?FName("Hero_Mage"):FName("Hero_Ranger"));
            Camp->InitUnit(Row,NewObject<ULKGameData>(Mode));
            Camp->SetActorTickEnabled(false);Camp->GetMovementComponent()->SetComponentTickEnabled(false);
            Camp->GetAnimationComponent()->SetComponentTickEnabled(false);Units.Add(Camp);
            Add(I==0?FName("Hero_Knight"):I==1?FName("Unit_TrollWarrior"):FName("Unit_ElfArcher"),ELKTeam::Player,FVector(-40,(I-1)*400-35,0));
            Add(I==0?FName("Unit_Skeleton"):I==1?FName("Boss_SkeletonKing"):FName("Unit_SkeletonArcher"),ELKTeam::Enemy,FVector(10,(I-1)*400+35,0));
        }
        if(Units.Num()!=9){UE_LOG(LogTemp,Error,TEXT("MOVEMENT_FIX_PREVIEW_FAILED missing overlap actors: %d"),Units.Num());FPlatformMisc::RequestExit(false);return;}
    }
    if(Frame>=190&&Frame<270)
    {
        // Intentionally intersect full images, including exact-position ties.
        // Manual component ticks isolate rendering from combat and collision AI.
        if(Frame==220||Frame==240)
        {
            for(int I=0;I<Units.Num();++I){if(I%3==2&&Units[I].IsValid())
            {auto P=Units[I]->GetActorLocation();P.X=Frame==220?-140:80;Units[I]->SetActorLocation(P);}}
        }
        for(auto Weak:Units){if(auto* Unit=Weak.Get())
        {
            if(Frame%20==0&&!Unit->IsCamp()){Unit->GetAnimationComponent()->Attack();}
            Unit->GetAnimationComponent()->TickComponent(.05f,LEVELTICK_All,nullptr);
        }}
        if(Frame==205||Frame==210||Frame==225||Frame==230||Frame==245||Frame==250)
        {Shot(FString::Printf(TEXT("Overlap_%03d"),Frame));}
    }
    if(Frame==280)
    {UE_LOG(LogTemp,Display,TEXT("MOVEMENT_FIX_PREVIEW_OK walkers=24 walkSamples=16 stops=2 overlapSamples=6"));FPlatformMisc::RequestExit(false);}
}
#endif
