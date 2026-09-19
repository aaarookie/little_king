#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ULKHomeBuildingLabelWidget.generated.h"

class UTextBlock;
class UBorder;

/** Screen-space UMG labels use Slate's Unicode font fallback, including Chinese. */
UCLASS()
class ULKHomeBuildingLabelWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void SetLabel(const FText& InTitle, const FText& InHint, FLinearColor InAccent);
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
private:
    void UpdateLabel();
    FText Title, Hint;
    FLinearColor Accent = FLinearColor::White;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> TitleText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> HintText;
    UPROPERTY(Transient) TObjectPtr<UBorder> AccentBorder;
};
