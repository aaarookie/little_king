#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ULKBattlePauseWidget.generated.h"
class UBorder;
class UButton;
class UTextBlock;
/** Always available pause control during deployment/combat; modal input remains live while world is paused. */
UCLASS()
class ULKBattlePauseWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void TogglePause();
    bool IsPauseOpen() const { return bOpen; }
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeTick(const FGeometry&,float) override;
private:
    UFUNCTION() void Resume();
    UFUNCTION() void Exit();
    UFUNCTION() void PauseClicked();
    UPROPERTY(Transient) TObjectPtr<UBorder> Modal;
    UPROPERTY(Transient) TObjectPtr<UButton> PauseButton;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Status;
    bool bOpen=false;
};
