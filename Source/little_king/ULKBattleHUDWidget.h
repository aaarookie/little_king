#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LKTypes.h"
#include "ULKBattleHUDWidget.generated.h"

class ALKPlayerController;
class ALKBattleGameState;
class ALKBattleGameMode;
class ULKDeckState;
class ULKSilverComponent;

/**
 * 战斗 HUD 基类：C++ 逻辑 + 蓝图表现（行业标准分工）。
 * C++ 自动绑定银币/手牌/阶段/胜负/放置状态事件，蓝图只需"覆写"下方事件刷新界面。
 *
 * 用法：
 *  1) 创建 Widget Blueprint，父类选 ULKBattleHUDWidget
 *  2) BP_ALKBattleGameMode 类默认值 -> HUDWidgetClass = 该 Widget
 */
UCLASS(Blueprintable, BlueprintType)
class ULKBattleHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// ---------- 蓝图实现事件（BP 里覆写，刷新界面） ----------
	UFUNCTION(BlueprintImplementableEvent, Category = "LK|HUD")
	void OnPhaseChanged(ELKGamePhase NewPhase);

	UFUNCTION(BlueprintImplementableEvent, Category = "LK|HUD")
	void OnHandChanged(const TArray<FName>& Hand, const TArray<int32>& Costs);

	UFUNCTION(BlueprintImplementableEvent, Category = "LK|HUD")
	void OnSilverChanged(float Silver, float Cap, float Delta);

	UFUNCTION(BlueprintImplementableEvent, Category = "LK|HUD")
	void OnMatchEnded(ELKTeam Winner);

	UFUNCTION(BlueprintImplementableEvent, Category = "LK|HUD")
	void OnPlacementStateChanged(bool bPlacing, ELKPlacementMode Mode, int32 HandIndex, FName ItemId);

	UFUNCTION(BlueprintImplementableEvent, Category = "LK|HUD")
	void OnPlayResult(ELKPlayResult Result);

	// ---------- 便捷查询（蓝图里直接用，省得 cast 链） ----------
	UFUNCTION(BlueprintPure, Category = "LK|HUD")
	ALKPlayerController* GetLKPlayerController() const;

	UFUNCTION(BlueprintPure, Category = "LK|HUD")
	ULKDeckState* GetDeck() const;

	UFUNCTION(BlueprintPure, Category = "LK|HUD")
	ULKSilverComponent* GetSilverComp() const;

	UFUNCTION(BlueprintPure, Category = "LK|HUD")
	ALKBattleGameState* GetBattleGameState() const;

	UFUNCTION(BlueprintPure, Category = "LK|HUD")
	ALKBattleGameMode* GetBattleGameMode() const;

protected:
	// ---------- C++ 事件转发（绑定引擎/游戏事件 -> 调用蓝图事件） ----------
	UFUNCTION()
	void HandlePhaseChanged(ELKGamePhase NewPhase);

	UFUNCTION()
	void HandleHandChanged();

	UFUNCTION()
	void HandleSilverChanged(float NewSilver, float Delta);

	UFUNCTION()
	void HandleMatchEnded(ELKTeam Winner);

	UFUNCTION()
	void HandlePlacementStateChanged(bool bPlacing, ELKPlacementMode Mode, int32 HandIndex, FName ItemId);

	UFUNCTION()
	void HandlePlayResult(ELKPlayResult Result);

	void BindEvents();
	void UnbindEvents();
	void PushInitialState();
};
