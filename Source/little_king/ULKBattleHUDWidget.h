#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LKTypes.h"
#include "ULKBattleHUDWidget.generated.h"

class ALKPlayerController;
class ALKBattleGameState;
class ALKBattleGameMode;
class ULKCardDefinition;
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
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	int32 LastWholeSilver = -1;

	// ---------- 蓝图实现事件（BP 里覆写，刷新界面） ----------
	UFUNCTION(BlueprintImplementableEvent, Category = "LK|HUD")
	void OnPhaseChanged(ELKGamePhase NewPhase);

	/** bPlayable[i] = false 表示该卡当前不可打出（空槽/银币不足/阶段错误，不因法师死亡灰卡） */
	UFUNCTION(BlueprintImplementableEvent, Category = "LK|HUD")
	void OnHandChanged(const TArray<FName>& Hand, const TArray<int32>& Costs, const TArray<bool>& bPlayable);

	UFUNCTION(BlueprintImplementableEvent, Category = "LK|HUD")
	void OnSilverChanged(float Silver, float Cap, float Delta);

	UFUNCTION(BlueprintImplementableEvent, Category = "LK|HUD")
	void OnMatchEnded(ELKTeam Winner);

	/** S5 飘字数据链：任何伤害/治疗生效时触发（Amount 恒为正；bIsHeal 区分伤害/治疗） */
	UFUNCTION(BlueprintImplementableEvent, Category = "LK|HUD")
	void OnDamageEventBP(FVector WorldLocation, float Amount, bool bIsHeal);

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

	/** 按 CardId 查卡牌定义（图标/名称/费用），HUD 显示卡面用 */
	UFUNCTION(BlueprintPure, Category = "LK|HUD")
	ULKCardDefinition* GetCardDefinition(FName CardId) const;

	/** 按 CardId 直接取卡牌图标（已加载的 Texture2D；无卡/无图标返回空）——免去蓝图软引用加载 */
	UFUNCTION(BlueprintPure, Category = "LK|HUD")
	class UTexture2D* GetCardIcon(FName CardId) const;

	/** S5 飘字定位：世界坐标 -> 视口坐标（失败返回 false，通常不会） */
	UFUNCTION(BlueprintPure, Category = "LK|HUD")
	bool WorldToScreen(FVector WorldLocation, FVector2D& OutScreenLocation) const;

protected:
	// ---------- C++ 事件转发（绑定引擎/游戏事件 -> 调用蓝图事件） ----------
	UFUNCTION()
	void HandlePhaseChanged(ELKGamePhase NewPhase);

	UFUNCTION()
	void HandleHandChanged();

	UFUNCTION()
	void HandleSpellLockChanged(bool bUnlocked);

	UFUNCTION()
	void HandleSilverChanged(float NewSilver, float Delta);

	UFUNCTION()
	void HandleMatchEnded(ELKTeam Winner);

	UFUNCTION()
	void HandleDamageEvent(FVector WorldLocation, float Amount, bool bIsHeal);

	UFUNCTION()
	void HandlePlacementStateChanged(bool bPlacing, ELKPlacementMode Mode, int32 HandIndex, FName ItemId);

	UFUNCTION()
	void HandlePlayResult(ELKPlayResult Result);

	void BindEvents();
	void UnbindEvents();
	void PushInitialState();
};
