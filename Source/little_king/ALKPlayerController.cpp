#include "ALKPlayerController.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"

#include "ALKBattleGameMode.h"
#include "ALKPlayerState.h"
#include "LKLog.h"
#include "ULKDeckState.h"
#include "ULKSilverComponent.h"

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

void ALKPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ALKPlayerController::HandleLeftClick);
	InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &ALKPlayerController::HandleRightClick);
}

void ALKPlayerController::HandleLeftClick()
{
	if (PlacementMode != ELKPlacementMode::None)
	{
		TryPlaceActive();
	}
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

	OnPlayResult.Broadcast(Result);

	// 成功或阶段错误（如开战时还在部署英雄）都退出放置模式
	if (Result == ELKPlayResult::Success || Result == ELKPlayResult::WrongPhase)
	{
		CancelPlacement();
	}
}

void ALKPlayerController::BeginCardPlacement(int32 HandIndex)
{
	PlacementMode = ELKPlacementMode::Card;
	PlacingHandIndex = HandIndex;
	PlacingHeroId = NAME_None;
	OnPlacementStateChanged.Broadcast(true, PlacementMode, PlacingHandIndex, PlacingHeroId);
}

void ALKPlayerController::BeginHeroPlacement(FName HeroUnitId)
{
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
	FVector WorldLocation;
	FVector WorldDirection;
	if (!DeprojectMousePositionToWorld(WorldLocation, WorldDirection))
	{
		return false;
	}

	FHitResult Hit;
	const FVector End = WorldLocation + WorldDirection * 20000.f;
	if (GetWorld()->LineTraceSingleByChannel(Hit, WorldLocation, End, ECC_Visibility))
	{
		OutLocation = Hit.Location;
		return true;
	}

	// 兜底：直接取射线远处一点
	OutLocation = WorldLocation + WorldDirection * 8000.f;
	return true;
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
	if (ALKBattleGameMode* GameMode = GetBattleGameMode())
	{
		GameMode->ForceStartBattle();
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
