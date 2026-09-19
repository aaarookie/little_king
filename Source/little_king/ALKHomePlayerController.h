#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ALKHomePlayerController.generated.h"

class ALKHomeBuildingActor;
class ALKHomeGameMode;

/**
 * 家园控制器（H1）：接管场景相机、鼠标点建筑、Esc 关面板、数字键快捷打开建筑。
 * 家园没有 Pawn，也不需要移动输入；点击判定用"鼠标投影到地面 + 建筑命中半径"。
 */
UCLASS()
class ALKHomePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ALKHomePlayerController();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupInputComponent() override;

	/** 用屏幕坐标打开建筑面板（鼠标点击与自动化测试共用） */
	UFUNCTION(BlueprintCallable, Category = "LK|Home")
	bool TryOpenBuildingAtScreenPosition(const FVector2D& ScreenPosition);

	/** 当前鼠标悬停的建筑（用于高亮与调试） */
	UFUNCTION(BlueprintPure, Category = "LK|Home")
	ALKHomeBuildingActor* GetHoveredBuilding() const { return HoveredBuilding.Get(); }

protected:
	void HandleLeftClick();
	void HandleEscape();
	void HandleNumberKey1() { OpenBuildingByLayoutIndex(0); }
	void HandleNumberKey2() { OpenBuildingByLayoutIndex(1); }
	void HandleNumberKey3() { OpenBuildingByLayoutIndex(2); }
	void HandleNumberKey4() { OpenBuildingByLayoutIndex(3); }
	void HandleNumberKey5() { OpenBuildingByLayoutIndex(4); }
	void HandleNumberKey6() { OpenBuildingByLayoutIndex(5); }
	void HandleNumberKey7() { OpenBuildingByLayoutIndex(6); }

	void FindAndUseLevelCamera();
	bool DeprojectMouseToGround(FVector& OutLocation) const;
	ALKHomeBuildingActor* FindBuildingUnderCursor() const;
	void OpenBuildingByLayoutIndex(int32 Index);
	ALKHomeGameMode* GetHomeGameMode() const;

private:
	TWeakObjectPtr<ALKHomeBuildingActor> HoveredBuilding;
};
