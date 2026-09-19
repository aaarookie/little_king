#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ALKStartMenuPlayerController.generated.h"

UCLASS()
class ALKStartMenuPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	ALKStartMenuPlayerController();
	virtual void BeginPlay() override;
};
