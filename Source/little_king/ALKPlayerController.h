#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "LKTypes.h"
#include "ALKPlayerController.generated.h"

class ULKDeckState;
class ULKSilverComponent;
class ALKBattleGameMode;
class ALKHeroCamp;

/**
 * 玩家控制器：
 * 1) 放置状态机 —— 点卡/英雄 -> 放置模式 -> 左键点战场结算 / 右键取消
 * 2) 把 UI（蓝图）操作翻译成 GameMode 的对局命令
 */
UCLASS()
class ALKPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ALKPlayerController();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** S5 屏幕震动（无资产方案）：短时随机抖动当前视角相机（幅度为世界单位） */
	void RequestCameraShake(float Intensity);
	bool GetGroundUnderMouse(FVector& Location) const { return DeprojectMouseToGround(Location); }
	ALKHeroCamp* GetSelectedCamp() const;
	int32 GetPlacingHandIndex() const { return PlacingHandIndex; }
	bool GetPlacementPreview(FVector& Location, float& Radius, bool& bValid, float* OutAttackRadius = nullptr) const;

	// ---------- 放置状态机 ----------
	UFUNCTION(BlueprintCallable, Category = "LK|Input")
	void BeginCardPlacement(int32 HandIndex);

	UFUNCTION(BlueprintCallable, Category = "LK|Input")
	void BeginHeroPlacement(FName HeroUnitId);

	UFUNCTION(BlueprintCallable, Category = "LK|Input")
	void CancelPlacement();

	UFUNCTION(BlueprintPure, Category = "LK|Input")
	bool IsPlacing() const { return PlacementMode != ELKPlacementMode::None; }

	UFUNCTION(BlueprintPure, Category = "LK|Input")
	ELKPlacementMode GetPlacementMode() const { return PlacementMode; }
	UFUNCTION(BlueprintPure, Category = "LK|Input")
	FName GetPlacingHeroId() const { return PlacingHeroId; }

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnPlacementStateChanged, bool, bPlacing, ELKPlacementMode, Mode, int32, HandIndex, FName, ItemId);
	UPROPERTY(BlueprintAssignable, Category = "LK|Input")
	FOnPlacementStateChanged OnPlacementStateChanged;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayResult, ELKPlayResult, Result);
	UPROPERTY(BlueprintAssignable, Category = "LK|Input")
	FOnPlayResult OnPlayResult;

	// ---------- 对局命令（UI 调用） ----------
	UFUNCTION(BlueprintCallable, Category = "LK|Battle")
	ELKPlayResult TryPlayCard(int32 HandIndex, const FVector& WorldLocation);

	UFUNCTION(BlueprintCallable, Category = "LK|Battle")
	ELKPlayResult TryPlayCardAtMouse(int32 HandIndex);

	UFUNCTION(BlueprintCallable, Category = "LK|Battle")
	ELKPlayResult DeployHero(FName HeroUnitId, const FVector& WorldLocation);

	UFUNCTION(BlueprintCallable, Category = "LK|Battle")
	ELKPlayResult DeployHeroAtMouse(FName HeroUnitId);

	UFUNCTION(BlueprintCallable, Category = "LK|Battle")
	void RequestStartBattle();

	UFUNCTION(BlueprintPure, Category = "LK|Battle")
	float GetSilver() const;

	UFUNCTION(BlueprintPure, Category = "LK|Battle")
	ULKDeckState* GetDeckState() const;

	UFUNCTION(BlueprintPure, Category = "LK|Battle")
	ULKSilverComponent* GetSilverComponent() const;

	UFUNCTION(BlueprintPure, Category = "LK|Battle")
	ELKGamePhase GetPhase() const;

protected:
	virtual void SetupInputComponent() override;

	void HandleLeftClick();
	void HandleRightClick();
	void TryPlaceActive();

private:
	bool DeprojectMouseToGround(FVector& OutLocation) const;
	ALKBattleGameMode* GetBattleGameMode() const;

	/** 无 Pawn 游戏：把视角接管到场景中第一个相机（各关卡通用） */
	void FindAndUseLevelCamera();

	ELKPlacementMode PlacementMode = ELKPlacementMode::None;
	int32 PlacingHandIndex = -1;
	FName PlacingHeroId = NAME_None;
	TWeakObjectPtr<ALKHeroCamp> SelectedCamp;
	ALKHeroCamp* FindCampAt(const FVector& Location) const;
	void SelectCamp(ALKHeroCamp* Camp);
	FRandomStream VisualRandom = FRandomStream(97131);
	TWeakObjectPtr<AActor> ShakenCamera;

	// ---------- S5 相机震动状态 ----------
	bool bCameraShakeActive = false;
	float CameraShakeRemaining = 0.f;
	float CameraShakeIntensity = 0.f;
	float CameraShakeDuration = 0.15f;
	FVector CameraShakeBaseLocation = FVector::ZeroVector;
};
