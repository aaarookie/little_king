#include "ALKStartMenuPlayerController.h"
#include "ULKCheatManager.h"

ALKStartMenuPlayerController::ALKStartMenuPlayerController()
{
	bShowMouseCursor = true;
	CheatClass = ULKCheatManager::StaticClass();
}
void ALKStartMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
}
