#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LKRunTypes.h"
#include "ULKWorldMapWidget.generated.h"

/** 代码绘制固定地理边界、箭头及节点。点击只选择/查看，前进由外部按钮确认。 */
UCLASS()
class ULKWorldMapWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	DECLARE_DELEGATE_OneParam(FOnNodePicked, FName);
	FOnNodePicked OnNodePicked;
	void SetMapState(const FLKRunState& InState, FName InSelected);
	void FocusRegion(FName RegionId);
	FName HitTestNode(FVector2D LocalPoint, FVector2D Size) const;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect, FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool Enabled) const override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
private:
	FVector2D Project(FVector2D Point, FVector2D Size) const;
	UPROPERTY(Transient) FLKRunState Snapshot;
    UPROPERTY(Transient) TMap<FName, TObjectPtr<class UTexture2D>> RegionTextures;
    UPROPERTY(Transient) TMap<uint8, TObjectPtr<class UTexture2D>> NodeTextures;
	FName Selected, Focus;
	FVector2D ViewMin = FVector2D::ZeroVector, ViewMax = FVector2D(1,1);
};
