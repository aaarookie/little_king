#include "ALKHomePlayerController.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"

#include "ALKHomeBuildingActor.h"
#include "ALKHomeGameMode.h"
#include "LKHomeContent.h"
#include "LKLog.h"
#include "ULKHomeHUDWidget.h"
#include "ULKCheatManager.h"

ALKHomePlayerController::ALKHomePlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	CheatClass = ULKCheatManager::StaticClass();
}

void ALKHomePlayerController::BeginPlay()
{
	Super::BeginPlay();
	FindAndUseLevelCamera();

	bShowMouseCursor = true;
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
}

void ALKHomePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (!InputComponent) { return; }

	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ALKHomePlayerController::HandleLeftClick);
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ALKHomePlayerController::HandleEscape);
	InputComponent->BindKey(EKeys::One, IE_Pressed, this, &ALKHomePlayerController::HandleNumberKey1);
	InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ALKHomePlayerController::HandleNumberKey2);
	InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ALKHomePlayerController::HandleNumberKey3);
	InputComponent->BindKey(EKeys::Four, IE_Pressed, this, &ALKHomePlayerController::HandleNumberKey4);
	InputComponent->BindKey(EKeys::Five, IE_Pressed, this, &ALKHomePlayerController::HandleNumberKey5);
	InputComponent->BindKey(EKeys::Six, IE_Pressed, this, &ALKHomePlayerController::HandleNumberKey6);
	InputComponent->BindKey(EKeys::Seven, IE_Pressed, this, &ALKHomePlayerController::HandleNumberKey7);
}

void ALKHomePlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ALKHomeBuildingActor* UnderCursor = FindBuildingUnderCursor();
	if (const ALKHomeGameMode* GM = GetHomeGameMode())
	{
		if (const ULKHomeHUDWidget* HUD = GM->GetHomeHUD(); HUD && HUD->IsPanelOpen()) { UnderCursor = nullptr; }
	}
	if (HoveredBuilding.Get() != UnderCursor)
	{
		if (ALKHomeBuildingActor* Previous = HoveredBuilding.Get()) { Previous->SetHighlighted(false); }
		HoveredBuilding = UnderCursor;
		if (UnderCursor) { UnderCursor->SetHighlighted(true); }
	}
}

ALKHomeGameMode* ALKHomePlayerController::GetHomeGameMode() const
{
	UWorld* World = GetWorld();
	return World ? World->GetAuthGameMode<ALKHomeGameMode>() : nullptr;
}

void ALKHomePlayerController::FindAndUseLevelCamera()
{
	UWorld* World = GetWorld();
	if (!World) { return; }

	// 与战斗控制器同一策略：优先 CameraActor，其次任意带 CameraComponent 的 Actor。
	for (TActorIterator<ACameraActor> It(World); It; ++It)
	{
		if (ACameraActor* Camera = *It)
		{
			SetViewTarget(Camera);
			UE_LOG(LogLK, Log, TEXT("[Home] 视角接管场景相机 %s"), *Camera->GetName());
			return;
		}
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->FindComponentByClass<UCameraComponent>())
		{
			SetViewTarget(*It);
			UE_LOG(LogLK, Log, TEXT("[Home] 视角接管相机组件（%s）"), *It->GetName());
			return;
		}
	}
	UE_LOG(LogLK, Warning, TEXT("[Home] 场景中没有相机（CameraActor / CameraComponent 都没有），请在家园地图里放一台俯视相机"));
}

bool ALKHomePlayerController::DeprojectMouseToGround(FVector& OutLocation) const
{
	FVector Origin, Direction;
	if (!DeprojectMousePositionToWorld(Origin, Direction) || FMath::IsNearlyZero(Direction.Z)) { return false; }
	const double Distance = -Origin.Z / Direction.Z;
	if (Distance < 0.0) { return false; }
	OutLocation = Origin + Direction * Distance;
	OutLocation.Z = 0.f;
	return !OutLocation.ContainsNaN();
}

ALKHomeBuildingActor* ALKHomePlayerController::FindBuildingUnderCursor() const
{
	FVector Point;
	const ALKHomeGameMode* GM = GetHomeGameMode();
	if (!GM || !DeprojectMouseToGround(Point)) { return nullptr; }

	ALKHomeBuildingActor* Best = nullptr;
	float BestDistance = TNumericLimits<float>::Max();
	for (ALKHomeBuildingActor* Actor : GM->GetBuildingActors())
	{
		if (!Actor) { continue; }
		const float Distance = FVector::Dist2D(Point, Actor->GetActorLocation());
		const float Reach = FMath::Max(Actor->HitRadius, Actor->PlaceholderSize.GetMax() * 0.5f);
		if (Distance <= Reach && Distance < BestDistance) { BestDistance = Distance; Best = Actor; }
	}
	return Best;
}

void ALKHomePlayerController::HandleLeftClick()
{
	// 面板打开时点击交给 UI（遮罩），不再命中场景建筑。
	if (ALKHomeGameMode* GM = GetHomeGameMode())
	{
		if (ULKHomeHUDWidget* HUD = GM->GetHomeHUD())
		{
			if (HUD->IsPanelOpen()) { return; }
		}
	}

	float MouseX = 0.f, MouseY = 0.f;
	GetMousePosition(MouseX, MouseY);
	TryOpenBuildingAtScreenPosition(FVector2D(MouseX, MouseY));
}

bool ALKHomePlayerController::TryOpenBuildingAtScreenPosition(const FVector2D& ScreenPosition)
{
	ALKHomeGameMode* GM = GetHomeGameMode();
	if (!GM) { return false; }

	// 用给定屏幕坐标重新做一次拾取（自动化测试可传固定坐标）。
	FVector Origin, Direction;
	if (!DeprojectScreenPositionToWorld(ScreenPosition.X, ScreenPosition.Y, Origin, Direction) || FMath::IsNearlyZero(Direction.Z))
	{
		return false;
	}
	const double Distance = -Origin.Z / Direction.Z;
	if (Distance < 0.0) { return false; }
	const FVector Point = Origin + FVector(Direction.X, Direction.Y, 0.0) * Distance;

	ALKHomeBuildingActor* Best = nullptr;
	float BestDistance = TNumericLimits<float>::Max();
	for (ALKHomeBuildingActor* Actor : GM->GetBuildingActors())
	{
		if (!Actor) { continue; }
		const float ActorDistance = FVector::Dist2D(Point, Actor->GetActorLocation());
		const float Reach = FMath::Max(Actor->HitRadius, Actor->PlaceholderSize.GetMax() * 0.5f);
		if (ActorDistance <= Reach && ActorDistance < BestDistance) { BestDistance = ActorDistance; Best = Actor; }
	}
	if (!Best) { return false; }

	UE_LOG(LogLK, Log, TEXT("[Home] 点击建筑 %s"), *Best->GetBuildingId().ToString());
	if (ULKHomeHUDWidget* HUD = GM->GetHomeHUD())
	{
		HUD->OpenPanelForBuilding(Best->GetBuildingId());
		return true;
	}
	return false;
}

void ALKHomePlayerController::OpenBuildingByLayoutIndex(int32 Index)
{
	ALKHomeGameMode* GM = GetHomeGameMode();
	if (!GM) { return; }
	const TArray<FLKBuildingDefinition>& Definitions = LKHomeContent::Buildings();
	if (!Definitions.IsValidIndex(Index)) { return; }
	if (ULKHomeHUDWidget* HUD = GM->GetHomeHUD())
	{
		HUD->OpenPanelForBuilding(Definitions[Index].BuildingId);
	}
}

void ALKHomePlayerController::HandleEscape()
{
	if (ALKHomeGameMode* GM = GetHomeGameMode())
	{
		if (ULKHomeHUDWidget* HUD = GM->GetHomeHUD())
		{
			if (HUD->IsPanelOpen()) { HUD->RequestClosePanel(); }
		}
	}
}
