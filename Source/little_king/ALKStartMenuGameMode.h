#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ALKStartMenuGameMode.generated.h"

class ULKStartMenuWidget;
class ULKRunNodeSelectWidget;
UCLASS()
class ALKStartMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	ALKStartMenuGameMode();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	ULKStartMenuWidget* GetMenuWidget() const { return MenuWidget; }
	UPROPERTY(EditDefaultsOnly, Category="Start Menu") TSubclassOf<ULKStartMenuWidget> MenuWidgetClass;
private:
	void TryCreateMenu();
	UPROPERTY(Transient) TObjectPtr<ULKStartMenuWidget> MenuWidget;
	UPROPERTY(Transient) TObjectPtr<ULKRunNodeSelectWidget> WorldMapPreviewWidget;
#if WITH_EDITOR
	void TickWorldMapPreview();
	bool bWorldMapPreview = false;
	bool bPreviewMode = false;
	int32 PreviewFrame = 0;
#endif
};
