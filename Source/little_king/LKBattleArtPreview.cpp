#include "LKBattleArtPreview.h"
#include "LKWorldArtPreview.h"
#include "LKPolishArtPreview.h"
#include "LKMovementFixPreview.h"
#include "LKV083Preview.h"
#include "LKV083Art.h"
#if WITH_EDITOR
#include "ALKBattleGameMode.h"
#include "ALKUnitBase.h"
#include "LKSkeletonCircle.h"
#include "ALKHeroCamp.h"
#include "LKBattleArt.h"
#include "ULKGameData.h"
#include "ULKRunSubsystem.h"
#include "ULKProfileSubsystem.h"
#include "ULKBattleHUDWidget.h"
#include "ULKDeckState.h"
#include "ULKSilverComponent.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "HighResScreenshot.h"
#include "UnrealClient.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

namespace LKBattleArtPreview
{
bool RunsBattle() { return FParse::Param(FCommandLine::Get(), TEXT("V083Sandbox")) || FParse::Param(FCommandLine::Get(), TEXT("V083HUDPreview")); }
bool Enabled() { return FParse::Param(FCommandLine::Get(), TEXT("V083HUDPreview")) || FParse::Param(FCommandLine::Get(), TEXT("V083Sandbox")) || FParse::Param(FCommandLine::Get(), TEXT("V083Preview")) || FParse::Param(FCommandLine::Get(), TEXT("BalanceV1Preview")) || FParse::Param(FCommandLine::Get(), TEXT("BattleArtPreview")) || FParse::Param(FCommandLine::Get(), TEXT("WorldArtPreview")) || FParse::Param(FCommandLine::Get(), TEXT("PolishArtPreview")) || FParse::Param(FCommandLine::Get(), TEXT("MovementFixPreview")); }
void Prepare(ALKBattleGameMode* Mode, TObjectPtr<ULKGameData>& Data)
{
    // Separate process, explicit editor-only option, no player save reads or writes.
    ULKProfileSubsystem::SetPersistentProfileEnabledForTest(false);
    ULKRunSubsystem* Run = Mode->GetGameInstance()->GetSubsystem<ULKRunSubsystem>();
    Run->ConfigureStorage(TEXT("LittleKing_BattleArtPreview_") + FGuid::NewGuid().ToString(EGuidFormats::Digits));
    Run->SetAutoSaveEnabled(false);
    ULKGameData* Source = Data ? Data.Get() : LoadObject<ULKGameData>(nullptr, TEXT("/Game/Data/DA_GameData.DA_GameData"));
    Data = Source ? DuplicateObject<ULKGameData>(Source, Mode) : NewObject<ULKGameData>(Mode);
    Data->bEnableExpeditionFlow = false;
    Data->bDrawDebugShapes = false;
    if (FParse::Param(FCommandLine::Get(), TEXT("V083Sandbox")) || FParse::Param(FCommandLine::Get(), TEXT("V083HUDPreview")))
    {
        Data->DefaultPlayerDeck=LKV083Art::SpellIds();
        Data->SilverCap=20.f; Data->SilverPerSecond=2.f;
        Data->EnemyEncounterId="Patrol";
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("BalanceV1Preview"))) { Data->EnemyEncounterId="Patrol"; }
}
static void TickBalancePreview(ALKBattleGameMode* Mode)
{
    static int32 Frame=0;
    static TWeakObjectPtr<ALKSkeletonCircle> Circle;
    if (!Mode->GetBattleHUDWidget()) { return; }
    ++Frame;
    auto FreezeActors=[&]()
    { for (TActorIterator<ALKUnitBase> It(Mode->GetWorld()); It; ++It) { It->SetActorTickEnabled(false); } };
    auto Shot=[&](const TCHAR* Name)
    {
        const FString Dir=FPaths::ProjectSavedDir()/TEXT("Screenshots/BalanceV1");
        IFileManager::Get().MakeDirectory(*Dir,true);
        FScreenshotRequest::RequestScreenshot(Dir/Name,true,false);
    };
    if (Frame==1)
    {
        for (int32 I=0; I<Mode->AvailableHeroes.Num(); ++I)
        { Mode->DeployHero(ELKTeam::Player,Mode->AvailableHeroes[I],FVector((I-1)*700.f,-850.f,0)); }
        Mode->ForceStartBattle();
        Mode->GetTeamSilver(ELKTeam::Player)->AddSilver(5.f);
        Mode->SpawnUnitForTeam("Unit_ElfWarrior",ELKTeam::Player,FVector(-200,-600,0));
        Mode->SpawnUnitForTeam("Unit_ElfArcher",ELKTeam::Player,FVector(150,-600,0));
        Mode->GetTeamSilver(ELKTeam::Enemy)->AddSilver(5.f);
        Circle=Mode->CastEnemyTacticalSpell("Spell_SkeletonCircle",FVector(0,-600,0));
        if (Circle.IsValid()) { Circle->SetActorTickEnabled(false); }
        FreezeActors();
    }
    if (Frame==80) { Shot(TEXT("Warning.png")); }
    if (Frame==100 && Circle.IsValid()) { Circle->AdvanceForTest(2.25f); FreezeActors(); }
    if (Frame==150) { Shot(TEXT("Active.png")); }
    if (Frame==190) { FPlatformMisc::RequestExit(false); }
}
void Tick(ALKBattleGameMode* Mode)
{
    if (FParse::Param(FCommandLine::Get(), TEXT("V083HUDPreview"))) { LKV083Preview::TickHUD(Mode); return; }
    if (FParse::Param(FCommandLine::Get(), TEXT("V083Sandbox"))) { return; }
    if (FParse::Param(FCommandLine::Get(), TEXT("V083Preview"))) { LKV083Preview::Tick(Mode); return; }
    if (FParse::Param(FCommandLine::Get(), TEXT("BalanceV1Preview"))) { TickBalancePreview(Mode); return; }
    if (FParse::Param(FCommandLine::Get(), TEXT("MovementFixPreview"))) { LKMovementFixPreview::Tick(Mode); return; }
    if (FParse::Param(FCommandLine::Get(), TEXT("PolishArtPreview"))) { LKPolishArtPreview::Tick(Mode); return; }
    if (FParse::Param(FCommandLine::Get(), TEXT("WorldArtPreview"))) { LKWorldArtPreview::Tick(Mode); return; }
    static int32 Frame = 0;
    if (!Mode->GetBattleHUDWidget()) { return; }
    ++Frame;
    UWorld* World = Mode->GetWorld();
    auto Clear = [&]()
    {
        for (TActorIterator<ALKUnitBase> It(World); It; ++It) { It->Destroy(); }
        for (TActorIterator<ALKHeroCamp> It(World); It; ++It) { It->Destroy(); }
    };
    auto Spawn = [&](FName Id, ELKTeam Team, FVector Position)
    {
        if (ALKUnitBase* Unit = Mode->SpawnUnitForTeam(Id, Team, Position))
        { Unit->SetCombatEnabled(true); Unit->SetActorTickEnabled(false); }
    };
    auto Shot = [&](const TCHAR* Name)
    {
        const FString Directory = FPaths::ProjectSavedDir() / TEXT("Screenshots/BattleArt");
        IFileManager::Get().MakeDirectory(*Directory, true);
        FScreenshotRequest::RequestScreenshot(Directory / Name, true, false);
    };
    if (Frame == 1)
    {
        for (int32 I = 0; I < Mode->AvailableHeroes.Num(); ++I)
        { Mode->DeployHero(ELKTeam::Player, Mode->AvailableHeroes[I], FVector((I - 1) * 700.f, -1300, 0)); }
        Mode->ForceStartBattle();
        Clear();
        ACameraActor* Camera = World->SpawnActor<ACameraActor>();
        Camera->SetActorLocationAndRotation(FVector(0, 0, 5000), FRotator(-90, 0, 0));
        Camera->GetCameraComponent()->ProjectionMode = ECameraProjectionMode::Orthographic;
        Camera->GetCameraComponent()->OrthoWidth = 3000;
        Camera->GetCameraComponent()->bConstrainAspectRatio = false;
        World->GetFirstPlayerController()->SetViewTarget(Camera);
        Mode->GetBattleHUDWidget()->SetVisibility(ESlateVisibility::Hidden);
        TArray<FName> Ids = { "Hero_Knight", "Hero_Mage", "Hero_Ranger", "Unit_Swordsman", "Unit_Archer", "Unit_Shieldbearer", "Building_ArrowTower", "Building_Barracks" };
        Ids.Append(LKBattleArt::UnitIds());
        for (int32 I = 0; I < Ids.Num(); ++I)
        { Spawn(Ids[I], I % 7 < 3 ? ELKTeam::Player : ELKTeam::Enemy, FVector(975.f - I / 7 * 650.f, (I % 7 - 3) * 600.f, 0)); }
    }
    if (Frame == 120) { Shot(TEXT("UnitGrid-Viewport.png")); }
    if (Frame == 160)
    {
        Clear();
        Mode->GetBattleHUDWidget()->SetVisibility(ESlateVisibility::Visible);
        Mode->GetTeamDeck(ELKTeam::Player)->InitDeck({"Unit_TrollWarrior", "Unit_TrollMage", "Unit_TwoHeadedDragon", "Building_SiegeCatapult", "Unit_ElfPriest"}, 4, 14);
        Mode->GetTeamSilver(ELKTeam::Player)->SetCap(13);
        Mode->GetTeamSilver(ELKTeam::Player)->AddSilver(13);
        const TArray<FName> Player = {"Hero_Knight", "Unit_ElfArcher", "Unit_TrollWarrior", "Unit_TrollKing", "Unit_Colossus", "Unit_TwoHeadedDragon"};
        const TArray<FName> Enemy = {"Hero_Necromancer", "Unit_SkeletonArcher", "Unit_Skeleton", "Hero_SkeletonGiant", "Boss_SkeletonKing", "Building_SiegeCatapult"};
        for (int32 I = 0; I < Player.Num(); ++I)
        {
            const float X = 800.f - (I / 2) * 500.f;
            Spawn(Player[I], ELKTeam::Player, FVector(X, -600.f - I % 2 * 650.f, 0));
            Spawn(Enemy[I], ELKTeam::Enemy, FVector(X, 600.f + I % 2 * 650.f, 0));
        }
    }
    if (Frame == 260) { Shot(TEXT("Battle-Viewport.png")); }
    if (Frame == 300) { FPlatformMisc::RequestExit(false); }
}
}
#endif
