#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "LKTypes.h"
#include "ALKPresentationHUD.generated.h"

/** 无资产依赖的运行时反馈，不依赖 DrawDebug 或 UMG 蓝图节点。 */
UCLASS()
class ALKPresentationHUD : public AHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;
private:
	bool bBound = false;
	struct FDamageText { FVector WorldLocation; float Amount; bool bHeal; float Remaining; };
	TArray<FDamageText> DamageTexts;
	FString Tip;
	float TipRemaining = 0.f;
	UFUNCTION() void HandleResult(ELKPlayResult Result);
	UFUNCTION() void HandleDamage(FVector Location, float Amount, bool bHeal);
	void DrawWorldCircle(const FVector& Center, float Radius, FLinearColor Color, float Thickness = 1.f);
	bool ProjectPoint(const FVector& Location, FVector2D& Point) const;
};
