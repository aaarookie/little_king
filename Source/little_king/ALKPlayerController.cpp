#include "ALKPlayerController.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"

#include "ALKBattleGameMode.h"
#include "ALKPlayerState.h"
#include "ALKHeroCamp.h"
#include "ALKUnitHero.h"
#include "ULKGameData.h"
#include "ULKUnitMovementComponent.h"
#include "LKLog.h"
#include "ULKCheatManager.h"
#include "ULKDeckState.h"
#include "ULKSilverComponent.h"

ALKPlayerController::ALKPlayerController()
{
	// 调试命令（PIE 中按 `~` 输入）：AddSilver/DrawCard/SpawnUnit/KillAll/WinMatch/StartBattle/ListUnits/InvulnerableHeroes
	// 注：UE5.8 中 CheatClass 位于 APlayerController（旧版本在 GameMode 上）
	CheatClass = ULKCheatManager::StaticClass();

	// S5 相机震动：控制器 Tick 驱动
	PrimaryActorTick.bCanEverTick = true;
}

void ALKPlayerController::BeginPlay()
{
	Super::BeginPlay();
	FindAndUseLevelCamera();

	// 鼠标同时操作 UI 与战场（无 Pawn 的纯 UI 驱动游戏）
	bShowMouseCursor = true;
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
}

void ALKPlayerController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (GetPhase() == ELKGamePhase::Result && IsPlacing()) { CancelPlacement(); }
    if (!bCameraShakeActive) { return; }
    AActor* Camera = ShakenCamera.Get();
    CameraShakeRemaining -= DeltaSeconds;
    if (!Camera || Camera != GetViewTarget() || CameraShakeRemaining <= 0.f)
    {
        if (Camera) { Camera->SetActorLocation(CameraShakeBaseLocation); }
        bCameraShakeActive = false;
        CameraShakeIntensity = 0.f;
        ShakenCamera.Reset();
        return;
    }
    const float Amp = CameraShakeIntensity * FMath::Clamp(CameraShakeRemaining / CameraShakeDuration, 0.f, 1.f);
    Camera->SetActorLocation(CameraShakeBaseLocation + FVector(VisualRandom.FRandRange(-Amp, Amp), VisualRandom.FRandRange(-Amp, Amp), 0.f));
}

void ALKPlayerController::RequestCameraShake(float Intensity)
{
    AActor* Camera = GetViewTarget();
    if (!FMath::IsFinite(Intensity) || Intensity <= 0.f || !IsValid(Camera)) { return; }
    if (!bCameraShakeActive || Camera != ShakenCamera.Get())
    {
        if (AActor* Previous = ShakenCamera.Get()) { Previous->SetActorLocation(CameraShakeBaseLocation); }
        CameraShakeBaseLocation = Camera->GetActorLocation();
        CameraShakeIntensity = Intensity;
        ShakenCamera = Camera;
    }
    else { CameraShakeIntensity = FMath::Max(CameraShakeIntensity, Intensity); }
    bCameraShakeActive = true;
    CameraShakeRemaining = CameraShakeDuration;
}

void ALKPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ALKPlayerController::HandleLeftClick);
	InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &ALKPlayerController::HandleRightClick);
}

void ALKPlayerController::HandleLeftClick()
{
    if (GetPhase() == ELKGamePhase::Result) { return; }
    if (PlacementMode == ELKPlacementMode::None || PlacementMode == ELKPlacementMode::HeroMove)
    {
        FVector Point;
        if (DeprojectMouseToGround(Point))
        {
            if (ALKHeroCamp* Camp = FindCampAt(Point)) { SelectCamp(Camp); return; }
        }
    }
    if (PlacementMode != ELKPlacementMode::None) { TryPlaceActive(); }
}

void ALKPlayerController::HandleRightClick()
{
	CancelPlacement();
}

void ALKPlayerController::TryPlaceActive()
{
	ELKPlayResult Result = ELKPlayResult::Unknown;

	if (PlacementMode == ELKPlacementMode::Card)
	{
		Result = TryPlayCardAtMouse(PlacingHandIndex);
	}
	else if (PlacementMode == ELKPlacementMode::Hero)
	{
		Result = DeployHeroAtMouse(PlacingHeroId);
	}

    else if (PlacementMode == ELKPlacementMode::HeroMove)
    {
        FVector Point;
        ALKHeroCamp* Camp = GetSelectedCamp();
        Result = GetPhase() == ELKGamePhase::Result ? ELKPlayResult::WrongPhase : ELKPlayResult::HeroMoveBlocked;
        if (Camp && Camp->GetHero() && DeprojectMouseToGround(Point) && Camp->GetHero()->CommandMove(Point))
        { Result = ELKPlayResult::Success; }
    }
    OnPlayResult.Broadcast(Result);

	// 成功或阶段错误（如开战时还在部署英雄）都退出放置模式
	if (Result == ELKPlayResult::Success || Result == ELKPlayResult::WrongPhase)
	{
		CancelPlacement();
	}
}

void ALKPlayerController::BeginCardPlacement(int32 HandIndex)
{
	SelectedCamp.Reset();
	PlacementMode = ELKPlacementMode::Card;
	PlacingHandIndex = HandIndex;
	PlacingHeroId = NAME_None;
	OnPlacementStateChanged.Broadcast(true, PlacementMode, PlacingHandIndex, PlacingHeroId);
}

void ALKPlayerController::BeginHeroPlacement(FName HeroUnitId)
{
	SelectedCamp.Reset();
	PlacementMode = ELKPlacementMode::Hero;
	PlacingHeroId = HeroUnitId;
	PlacingHandIndex = -1;
	OnPlacementStateChanged.Broadcast(true, PlacementMode, PlacingHandIndex, PlacingHeroId);
}

void ALKPlayerController::CancelPlacement()
{
	if (PlacementMode == ELKPlacementMode::None)
	{
		return;
	}

	SelectedCamp.Reset();
	PlacementMode = ELKPlacementMode::None;
	PlacingHandIndex = -1;
	PlacingHeroId = NAME_None;
	OnPlacementStateChanged.Broadcast(false, ELKPlacementMode::None, -1, NAME_None);
}

void ALKPlayerController::FindAndUseLevelCamera()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 本项目 DefaultPawnClass = nullptr，引擎的 bAutoManageActiveCameraTarget
	// 只在 SetPawn/观战等路径触发，从不 SetPawn 的纯 UI 游戏不会自动接管相机，
	// 因此在这里手动把视角交给场景中的相机。
	// 按优先级查找：1) CameraActor  2) 任意带 CameraComponent 的 Actor（CineCameraActor 等）

	for (TActorIterator<ACameraActor> It(World); It; ++It)
	{
		ACameraActor* Camera = *It;
		if (Camera)
		{
			SetViewTarget(Camera);
			UE_LOG(LogLKBattle, Log, TEXT("[Battle] 视角接管场景相机 %s"), *Camera->GetName());
			return;
		}
	}

	TArray<AActor*> AllActors;
	UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);
	for (AActor* Actor : AllActors)
	{
		if (Actor && Actor->FindComponentByClass<UCameraComponent>())
		{
			SetViewTarget(Actor);
			UE_LOG(LogLKBattle, Log, TEXT("[Battle] 视角接管相机组件（%s）"), *Actor->GetName());
			return;
		}
	}

	UE_LOG(LogLKBattle, Warning, TEXT("[Battle] 场景中没有相机（CameraActor / CameraComponent 都没有），视角保持默认位置"));
}

ALKBattleGameMode* ALKPlayerController::GetBattleGameMode() const
{
	UWorld* World = GetWorld();
	return World ? World->GetAuthGameMode<ALKBattleGameMode>() : nullptr;
}

bool ALKPlayerController::DeprojectMouseToGround(FVector& OutLocation) const
{
    FVector Origin, Direction;
    if (!DeprojectMousePositionToWorld(Origin, Direction) || FMath::IsNearlyZero(Direction.Z)) { return false; }
    const double Distance = -Origin.Z / Direction.Z;
    if (Distance < 0.0) { return false; }
    OutLocation = Origin + Direction * Distance;
    OutLocation.Z = 0.f;
    return !OutLocation.ContainsNaN();
}

ELKPlayResult ALKPlayerController::TryPlayCard(int32 HandIndex, const FVector& WorldLocation)
{
	ALKBattleGameMode* GameMode = GetBattleGameMode();
	return GameMode ? GameMode->PlayCardForTeam(ELKTeam::Player, HandIndex, WorldLocation) : ELKPlayResult::Unknown;
}

ELKPlayResult ALKPlayerController::TryPlayCardAtMouse(int32 HandIndex)
{
	FVector Location;
	if (!DeprojectMouseToGround(Location))
	{
		return ELKPlayResult::InvalidLocation;
	}
	return TryPlayCard(HandIndex, Location);
}

ELKPlayResult ALKPlayerController::DeployHero(FName HeroUnitId, const FVector& WorldLocation)
{
	ALKBattleGameMode* GameMode = GetBattleGameMode();
	return GameMode ? GameMode->DeployHero(ELKTeam::Player, HeroUnitId, WorldLocation) : ELKPlayResult::Unknown;
}

ELKPlayResult ALKPlayerController::DeployHeroAtMouse(FName HeroUnitId)
{
	FVector Location;
	if (!DeprojectMouseToGround(Location))
	{
		return ELKPlayResult::InvalidLocation;
	}
	return DeployHero(HeroUnitId, Location);
}

void ALKPlayerController::RequestStartBattle()
{
    if (ALKBattleGameMode* GM = GetBattleGameMode())
    {
        if (GM->CanStartBattle()) { CancelPlacement(); GM->ForceStartBattle(); }
        else
        {
            OnPlayResult.Broadcast(GetPhase() != ELKGamePhase::Deployment ? ELKPlayResult::WrongPhase
                : (!GM->HasValidDecks() ? ELKPlayResult::InvalidCardData : ELKPlayResult::DeploymentIncomplete));
        }
    }
}

float ALKPlayerController::GetSilver() const
{
	const ALKPlayerState* PS = GetPlayerState<ALKPlayerState>();
	return PS ? PS->Silver->GetSilver() : 0.f;
}

ULKDeckState* ALKPlayerController::GetDeckState() const
{
	const ALKPlayerState* PS = GetPlayerState<ALKPlayerState>();
	return PS ? PS->Deck : nullptr;
}

ULKSilverComponent* ALKPlayerController::GetSilverComponent() const
{
	const ALKPlayerState* PS = GetPlayerState<ALKPlayerState>();
	return PS ? PS->Silver : nullptr;
}

ELKGamePhase ALKPlayerController::GetPhase() const
{
	const ALKBattleGameMode* GameMode = GetBattleGameMode();
	return GameMode ? GameMode->GetPhase() : ELKGamePhase::Deployment;
}

ALKHeroCamp* ALKPlayerController::GetSelectedCamp() const { return SelectedCamp.Get(); }

ALKHeroCamp* ALKPlayerController::FindCampAt(const FVector& Location) const
{
    for (TActorIterator<ALKHeroCamp> It(GetWorld()); It; ++It)
    {
        if (It->GetTeam() == ELKTeam::Player && It->GetHero() && It->GetHero()->IsAlive()
            && FVector::Dist2D(Location, It->GetActorLocation()) <= It->GetBodyRadius()) { return *It; }
    }
    return nullptr;
}

void ALKPlayerController::SelectCamp(ALKHeroCamp* Camp)
{
    SelectedCamp = Camp;
    PlacementMode = ELKPlacementMode::HeroMove;
    PlacingHandIndex = -1;
    PlacingHeroId = NAME_None;
    OnPlacementStateChanged.Broadcast(true, PlacementMode, -1, NAME_None);
}

bool ALKPlayerController::GetPlacementPreview(FVector& Location, float& Radius, bool& bValid, float* OutAttackRadius) const
{
    if (OutAttackRadius) { *OutAttackRadius = 0.f; }
    ALKBattleGameMode* GM = GetBattleGameMode();
    if (!IsPlacing() || !GM || !GM->GetGameData() || !DeprojectMouseToGround(Location)) { return false; }
    bValid = false;
    Radius = 50.f;
    if (PlacementMode == ELKPlacementMode::Card)
    {
        ULKDeckState* Deck = GetDeckState();
        if (!Deck || !Deck->GetHand().IsValidIndex(PlacingHandIndex)) { return false; }
        const ULKCardDefinition* Card = GM->FindCard(Deck->GetHand()[PlacingHandIndex]);
        if (!Card) { return false; }
        if (OutAttackRadius) { *OutAttackRadius = GM->GetBuildingPlacementAttackRange(Card->CardId); }
        if (Card->CardType == ELKCardType::Spell) { Radius = Card->SpellRadius; }
        else { Radius = GM->GetGameData()->UnitBodyRadius; }
        bValid = GM->ValidateCardPlay(ELKTeam::Player, PlacingHandIndex, Location) == ELKPlayResult::Success;
    }
    else if (PlacementMode == ELKPlacementMode::Hero)
    {
        Radius = FMath::Max(GM->GetGameData()->HeroCampMoveRadius, 2.f * GM->GetGameData()->UnitBodyRadius + GM->GetGameData()->HeroCampBodyRadius + 9.f);
        bValid = GM->ValidateHeroDeployment(ELKTeam::Player, PlacingHeroId, Location) == ELKPlayResult::Success;
    }
    else if (ALKHeroCamp* Camp = GetSelectedCamp())
    {
        if (ALKUnitHero* Hero = Camp->GetHero())
        {
            Radius = Hero->GetBodyRadius();
            // 活动范围不限距离：仅当目标在场内且不与建筑/营地重叠时有效（真正可达性在点击时再验）。
            bValid = Hero->IsAlive() && GM->IsInsideField(Location);
            for (TActorIterator<ALKUnitBase> It(GetWorld()); It && bValid; ++It)
            {
                if (It->IsAlive() && It->IsBuilding() && FVector::Dist2D(Location, It->GetActorLocation()) < Radius + It->GetBodyRadius() + 1.f) { bValid = false; }
            }
        }
    }
    return true;
}
