#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ALKHomeBuildingActor.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class USphereComponent;
class UMaterialInstanceDynamic;
class UWidgetComponent;
class ULKHomeBuildingLabelWidget;
class UPaperSpriteComponent;

/**
 * 家园建筑占位 Actor（H1）。只负责：位置、可点击体积、稳定 BuildingId、名称/等级标签与悬停反馈。
 *
 * 刻意**不**继承 ALKUnitBase：家园建筑没有 ASC、血条、索敌、攻击，也不计入战斗单位数量。
 * 美术可替换 Mesh/材质或改用 Widget Blueprint 展示；BuildingId 必须保持稳定（存档身份）。
 */
UCLASS(Blueprintable, BlueprintType)
class ALKHomeBuildingActor : public AActor
{
	GENERATED_BODY()

public:
	ALKHomeBuildingActor();

	/** 稳定建筑 ID（Home_*）；由 GameMode 或编辑器设置 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LK|Home")
	FName BuildingId;

	/** 点击命中半径（世界单位）；默认按建筑体量给出，编辑器可调 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LK|Home", meta = (ClampMin = "10.0"))
	float HitRadius = 300.f;

	/** 占位方块尺寸（长 × 宽 × 高，世界单位） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LK|Home")
	FVector PlaceholderSize = FVector(300.f, 340.f, 120.f);

	UFUNCTION(BlueprintPure, Category = "LK|Home") FName GetBuildingId() const { return BuildingId; }
	UFUNCTION(BlueprintPure, Category = "LK|Home") FText GetBuildingName() const { return DisplayName; }
	UFUNCTION(BlueprintPure, Category = "LK|Home") int32 GetDisplayedLevel() const { return DisplayedLevel; }
	UFUNCTION(BlueprintPure, Category = "LK|Home") bool IsUpgradable() const { return bUpgradable; }

	/** GameMode 自动生成时初始化；也会刷新占位外观与标签 */
	void InitializeBuilding(FName InBuildingId, const FText& InName, bool bInUpgradable);

	/** 刷新等级标签（升级成功后由 GameMode 调用） */
	void SetDisplayedLevel(int32 InLevel);

	/** 悬停/选中反馈：占位方块描边色与轻微缩放（不只靠颜色区分，标签始终显示名称） */
	void SetHighlighted(bool bInHighlighted);
	UFUNCTION(BlueprintPure, Category = "LK|Home") bool IsHighlighted() const { return bHighlighted; }
	bool HasIllustration() const;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LK|Home")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LK|Home")
	TObjectPtr<UStaticMeshComponent> PlaceholderMesh;
	UPROPERTY(VisibleAnywhere, Category = "LK|Home") TObjectPtr<UPaperSpriteComponent> Illustration;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LK|Home")
	TObjectPtr<UTextRenderComponent> NameLabel;

	UPROPERTY(VisibleAnywhere, Category = "LK|Home") TObjectPtr<UWidgetComponent> ScreenLabel;
	UPROPERTY(Transient) TObjectPtr<ULKHomeBuildingLabelWidget> LabelWidget;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LK|Home")
	TObjectPtr<USphereComponent> HitSphere;

private:
	void ApplyPlaceholderAppearance();
	void RefreshLabel();

	UPROPERTY(Transient) FText DisplayName;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> PlaceholderMaterial;
	int32 DisplayedLevel = 1;
	bool bUpgradable = false;
	bool bHighlighted = false;
	bool bInitialized = false;
};
