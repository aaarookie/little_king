#include "LKWorldArtPreview.h"
#if WITH_EDITOR
#include "ALKBattleGameMode.h"
#include "ALKUnitHero.h"
#include "ALKHeroCamp.h"
#include "ULKBattleHUDWidget.h"
#include "ULKPresentationSubsystem.h"
#include "ULKUnitMovementComponent.h"
#include "ULKUnitStatusComponent.h"
#include "ULKDeckState.h"
#include "ULKSilverComponent.h"
#include "ULKRunSubsystem.h"
#include "ULKProfileSubsystem.h"
#include "ULKGameData.h"
#include "ULKCardDefinition.h"
#include "ULKRunNodeSelectWidget.h"
#include "ULKWorldMapWidget.h"
#include "LKHomeContent.h"
#include "LKUnitContent.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"

void LKWorldArtPreview::Tick(ALKBattleGameMode* Mode)
{
    static int32 Frame=0;
    static TWeakObjectPtr<ULKRunNodeSelectWidget> Map;
    static FString ProfileBase;
    if(!Mode->GetBattleHUDWidget()){return;}
    ++Frame;
    UWorld* World=Mode->GetWorld();
    ULKPresentationSubsystem* P=World->GetSubsystem<ULKPresentationSubsystem>();
    ULKRunSubsystem* Run=Mode->GetGameInstance()->GetSubsystem<ULKRunSubsystem>();
    auto Shot=[&](const FString& Name)
    {
        const FString Dir=FPaths::ProjectSavedDir()/TEXT("Screenshots/WorldArt");
        IFileManager::Get().MakeDirectory(*Dir,true);
        FScreenshotRequest::RequestScreenshot(Dir/Name,true,false);
    };
    auto Freeze=[&]()
    {
        for(TActorIterator<ALKUnitBase> It(World);It;++It)
        { It->SetActorTickEnabled(false); It->GetMovementComponent()->SetComponentTickEnabled(false); }
    };
    if(Frame==1)
    {
        for(int I=0;I<Mode->AvailableHeroes.Num();++I)
        { Mode->DeployHero(ELKTeam::Player,Mode->AvailableHeroes[I],FVector((1-I)*650.f,-1350,0)); }
        ACameraActor* Camera=World->SpawnActor<ACameraActor>();
        Camera->SetActorLocationAndRotation(FVector(0,0,5000),FRotator(-90,0,0));
        Camera->GetCameraComponent()->ProjectionMode=ECameraProjectionMode::Orthographic;
        Camera->GetCameraComponent()->OrthoWidth=2800;
        Camera->GetCameraComponent()->bConstrainAspectRatio=false;
        World->GetFirstPlayerController()->SetViewTarget(Camera);
        for(TActorIterator<ALKUnitHero> It(World);It;++It)
        { if(It->GetTeam()==ELKTeam::Player){It->SetActorLocation(It->GetActorLocation()+FVector(0,220,0));} }
        Freeze();
    }
    if(Frame==30){Shot(TEXT("Deployment.png"));}
    if(Frame==40)
    {
        Mode->ForceStartBattle();
        for(TActorIterator<ALKUnitBase> It(World);It;++It){if(It->GetTeam()==ELKTeam::Enemy){It->Destroy();}}
        const TArray<FName> Enemy={"Hero_Necromancer","Hero_SkeletonGiant","Boss_SkeletonKing","Unit_SkeletonArcher","Unit_Skeleton","Building_SiegeCatapult"};
        const TArray<FName> Player={"Unit_TwoHeadedDragon","Unit_TrollKing","Unit_Colossus","Unit_ElfPriest","Unit_GoblinRogue","Unit_ElfGuard"};
        for(int I=0;I<6;++I)
        {
            Mode->SpawnUnitForTeam(Enemy[I],ELKTeam::Enemy,FVector(700-(I/2)*650.f,550+(I%2)*650.f,0));
            Mode->SpawnUnitForTeam(Player[I],ELKTeam::Player,FVector(700-(I/2)*650.f,-400-(I%2)*360.f,0));
        }
        Mode->GetTeamDeck(ELKTeam::Player)->InitDeck({"Unit_TrollWarrior","Unit_TwoHeadedDragon","Building_SiegeCatapult","Spell_Fireball","Spell_HealWave"},4,14);
        Mode->GetTeamSilver(ELKTeam::Player)->AddSilver(20);
        Freeze();
    }
    for(int I=0;I<5;++I)
    {
        if(Frame==45+I*60){P->SetRegion(FName(*FString::Printf(TEXT("World_Region%d"),I)),I==4?ELKDungeonNodeType::Boss:I==3?ELKDungeonNodeType::Elite:ELKDungeonNodeType::Battle);}
        if(Frame==85+I*60){Shot(FString::Printf(TEXT("Region%d.png"),I));}
    }
    if(Frame==340)
    {
        for(TActorIterator<ALKUnitBase> It(World);It;++It)
        {
            if(It->GetUnitId()=="Unit_ElfGuard"){It->GetStatusComponent()->Stun(2);}
            if(It->GetUnitId()=="Unit_SkeletonArcher"){It->GetStatusComponent()->Freeze(2);}
            if(It->GetUnitId()=="Unit_TrollKing"){It->GetStatusComponent()->Empower(8,1.3f,.75f);}
            if(It->GetUnitId()=="Hero_SkeletonGiant"){FLKCombatSource Source;It->GetStatusComponent()->Ignite(Source,nullptr);}
        }
        ULKPresentationSubsystem::Fireball(World,FVector(300,500,0),200);
        ULKPresentationSubsystem::Emit(World,ELKVisualCue::Heal,FVector(-450,-650,0),160);
        ULKPresentationSubsystem::Emit(World,ELKVisualCue::Revive,FVector(-650,1200,0),160);
        ULKPresentationSubsystem::Emit(World,ELKVisualCue::Command,FVector(950,-900,0));
        ULKPresentationSubsystem::Emit(World,ELKVisualCue::HealLink,FVector(650,-1100,0),60,FVector(-450,-650,0));
    }
    if(Frame==345){Shot(TEXT("Effects.png"));}
    if(Frame==390)
    {
        ULKProfileSubsystem::SetPersistentProfileEnabledForTest(true);
        ProfileBase=TEXT("LittleKing_WorldArtPreview_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
        ULKProfileSubsystem* Profile=Mode->GetGameInstance()->GetSubsystem<ULKProfileSubsystem>();
        Profile->ConfigureStorage(ProfileBase); Profile->SetAutoSaveEnabled(false);
        Profile->EnsureProfile();
        FLKExpeditionStartRequest Request; FString Error;
        const bool Built=LKHomeContent::BuildExpeditionStartRequest(*Profile,Mode->GetGameData(),LKHomeContent::DefaultRegionId(),
            [](FName Id){return LKUnitContent::Find(Id);},[Mode](FName Id){return Mode->FindCard(Id);},Request,Error);
        Request.Seed=9152026;
        if(Built && Run->StartNewRun(Request))
        {
            Mode->GetBattleHUDWidget()->SetVisibility(ESlateVisibility::Hidden);
            Map=CreateWidget<ULKRunNodeSelectWidget>(World,ULKRunNodeSelectWidget::StaticClass());
            Map->InitializeRunView(Run); Map->AddToViewport(200);
        }
        else { UE_LOG(LogTemp,Error,TEXT("WORLD_ART_PREVIEW_MAP_FAILED %s"),*Error); }
    }
    if(Frame==440){Shot(TEXT("WorldMap.png"));}
    if(Frame==470 && Map.IsValid())
    {
        Map->SelectMapNode("World_R0_L1_N0");
        if(ULKWorldMapWidget* Canvas=Cast<ULKWorldMapWidget>(Map->GetWidgetFromName("WorldMapCanvas"))){Canvas->FocusRegion("World_Region0");}
    }
    if(Frame==520){Shot(TEXT("RegionMap.png"));}
    if(Frame==550 && Map.IsValid())
    { Run->SelectNode("World_R0_L1_N0"); Map->RefreshNodeSelect(); }
    if(Frame==595){Shot(TEXT("Market.png"));}
    if(Frame==630 && Map.IsValid())
    { Run->ResolveServiceNode("LeaveMarket");Run->SelectNode("World_R0_L2_N0");Map->RefreshNodeSelect(); }
    if(Frame==675){Shot(TEXT("Rest.png"));}
    if(Frame==710)
    {
        for(const FString& S:{ProfileBase+TEXT("_A"),ProfileBase+TEXT("_B")})
        { if(!ProfileBase.IsEmpty()&&UGameplayStatics::DoesSaveGameExist(S,0)){UGameplayStatics::DeleteGameInSlot(S,0);} }
        FPlatformMisc::RequestExit(false);
    }
}
#endif
