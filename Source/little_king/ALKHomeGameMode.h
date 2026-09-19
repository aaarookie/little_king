#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "LKHomeTypes.h"
#include "LKDataTypes.h"
#include "ALKHomeGameMode.generated.h"

class ALKHomeBuildingActor;
class ULKGameData;
class ULKHomeHUDWidget;
class ULKProfileSubsystem;
class ULKRunSubsystem;
class ULKCardDefinition;

/** 家园进度/面板需要整体刷新（升级成功、战备保存、远征状态变化） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHomeRefreshRequested);

/**
 * 家园地图 GameMode（H1/H3/H4）。
 *
 * 职责：家园世界启动、七座建筑占位生成与点击调度、面板数据模型、出征/继续/结算协调。
 * 不生成敌人、不启动战斗计时、不创建任何战斗单位——家园不是战斗场景。
 *
 * 规则归属：升级/战备/结算的校验与提交在 ULKProfileSubsystem，出征状态机在 ULKRunSubsystem；
 * 本类只做"读状态 + 组装模型 + 转发命令"，UI 不重复实现任何规则。
 */
UCLASS()
class ALKHomeGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ALKHomeGameMode();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// ---------- 配置 ----------
	/** 与战斗共用同一个 DA_GameData（运行时复制一份，不写回资产） */
	UPROPERTY(EditDefaultsOnly, Category = "LK|Home")
	TObjectPtr<ULKGameData> GameData;

	/** 家园 HUD 类；留空则使用原生 ULKHomeHUDWidget */
	UPROPERTY(EditDefaultsOnly, Category = "LK|Home")
	TSubclassOf<ULKHomeHUDWidget> HUDWidgetClass;

	/** 世界里没有建筑 Actor 时自动生成七座占位建筑（美术可改为手工摆放） */
	UPROPERTY(EditDefaultsOnly, Category = "LK|Home")
	bool bSpawnPlaceholderBuildings = true;

	/** 没有地面时生成一块占位地面，避免俯视相机看不到参照物 */
	UPROPERTY(EditDefaultsOnly, Category = "LK|Home")
	bool bSpawnPlaceholderGround = true;

	/** 出征时打开的战斗地图（区域定义可覆盖） */
	UPROPERTY(EditDefaultsOnly, Category = "LK|Home")
	FName BattleMapName = "L_BattleTest";

	/** 远征终态"返回家园"的地图名 */
	UPROPERTY(EditDefaultsOnly, Category = "LK|Home")
	FName HomeMapName = "L_Home";

	// ---------- 查询 ----------
	UFUNCTION(BlueprintPure, Category = "LK|Home") ULKGameData* GetGameData() const { return GameData; }
	UFUNCTION(BlueprintPure, Category = "LK|Home") ULKProfileSubsystem* GetProfileSubsystem() const;
	UFUNCTION(BlueprintPure, Category = "LK|Home") ULKRunSubsystem* GetRunSubsystem() const;
	UFUNCTION(BlueprintPure, Category = "LK|Home") ULKHomeHUDWidget* GetHomeHUD() const { return HomeHUD; }
	ULKCardDefinition* FindCard(FName CardId) const;
	const struct FLKUnitRow* GetUnitRow(FName UnitId) const;

	UFUNCTION(BlueprintPure, Category = "LK|Home") int32 GetGold() const;
	UFUNCTION(BlueprintPure, Category = "LK|Home") int32 GetBuildingLevel(FName BuildingId) const;
	UFUNCTION(BlueprintPure, Category = "LK|Home") bool IsProfileUsable() const;
	UFUNCTION(BlueprintPure, Category = "LK|Home") FText GetProfileStatusText() const;
	/** 顶部远征状态：无远征 / 远征进行中（第 N 战）/ 上次通关 / 上次失败 */
	UFUNCTION(BlueprintPure, Category = "LK|Home") FText GetExpeditionStatusText() const;
	UFUNCTION(BlueprintPure, Category = "LK|Home") bool HasActiveExpedition() const;
	UFUNCTION(BlueprintPure, Category = "LK|Home") bool CanContinueExpedition() const;
	UFUNCTION(BlueprintPure, Category = "LK|Home") FName GetSelectedRegionId() const;

	// ---------- 面板数据与命令 ----------
	/** 组装面板数据（UI 只渲染；Draft/SelectedRowId/TabIndex 是界面状态） */
	FLKHomePanelModel BuildPanelModel(ELKHomePanel Panel, const FLKExpeditionLoadout& DraftLoadout,
		FName SelectedRowId, int32 TabIndex) const;

	/** 升级（单次提交，失败不扣金币） */
	ELKUpgradeResult RequestUpgrade(FName BuildingId, int32 ExpectedLevel);
	/** 保存战备（草稿由 UI 维护，这里做严格校验与落盘） */
	ELKLoadoutResult RequestSaveLoadout(const FLKExpeditionLoadout& DraftLoadout);
	/** 出征：校验战备/区域/无进行中远征 → 冻结快照 → 创建新 RunId → 切图 */
	ELKExpeditionStartResult RequestStartExpedition(FName RegionId);
	int32 GetDepartureGold() const;
	int32 GetDepartureGoldLimit() const;
	void SetDepartureGold(int32 Amount);
	/** 继续远征：保留原 RunId/AttemptId/待选奖励，回到原房开战前 */
	ELKExpeditionStartResult RequestContinueExpedition();
	/** 大门二次确认后放弃当前远征；存档失败保持原轮。 */
	UFUNCTION(BlueprintCallable, Category = "LK|Home")
	bool RequestAbandonExpedition();
	/** 把远征终态金币入账（幂等；家园每次进入都会尝试一次） */
	ELKSettlementResult ApplyPendingSettlement();
	/** 选择区域（仅记录展示偏好） */
	void SelectRegion(FName RegionId);

	// ---------- 建筑 ----------
	const TArray<TObjectPtr<ALKHomeBuildingActor>>& GetBuildingActors() const { return BuildingActors; }
	ALKHomeBuildingActor* FindBuildingActor(FName BuildingId) const;
	/** 七座建筑的建议布局位置（俯视相机：+X 向上，+Y 向右） */
	static FVector GetBuildingLayoutLocation(ELKHomeBuilding Building);
	/** 升级/读档后刷新建筑等级标签 */
	void RefreshBuildingLevels();

	UPROPERTY(BlueprintAssignable, Category = "LK|Home")
	FOnHomeRefreshRequested OnHomeRefreshRequested;

protected:
	void EnsureGameData();
	void SpawnPlaceholderScene();
	void TryCreateHomeHUD();
	/** 切到战斗地图；地图不存在时明确报错并保留远征（可重试） */
	bool OpenBattleMap(FName MapName);
	FText BuildBuildingDetail(FName BuildingId) const;
	FText BuildHeroDetail(FName HeroId) const;
	FText BuildCardDetail(FName CardId) const;
	FLKHomePanelModel BuildUpgradePanel(ELKHomePanel Panel) const;
	FLKHomePanelModel BuildCollectionPanel(ELKHomePanel Panel, FName SelectedRowId) const;
	FLKHomePanelModel BuildBarracksPanel(FName SelectedRowId, int32 TabIndex) const;
	FLKHomePanelModel BuildGatePanel(FName SelectedRowId) const;
	FLKHomePanelModel BuildWarRoomPanel(const FLKExpeditionLoadout& DraftLoadout) const;

private:
	int32 DepartureGold = 0;
	UPROPERTY(Transient) TArray<TObjectPtr<ALKHomeBuildingActor>> BuildingActors;
	UPROPERTY(Transient) TObjectPtr<ULKHomeHUDWidget> HomeHUD;
	UPROPERTY(Transient) TObjectPtr<AActor> GroundActor;
	UPROPERTY(Transient) FName SelectedRegionId;
	UPROPERTY(Transient) TMap<FName, FLKUnitRow> ResolvedHomeUnits;
	bool bHUDCreateAttempted = false;
	bool bSettlementAttempted = false;
#if WITH_EDITOR
	int32 PreviewFrame = 0;
	bool bPreviewMode = false;
#endif
};
