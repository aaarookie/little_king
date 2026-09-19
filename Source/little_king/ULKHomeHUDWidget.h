#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LKHomeTypes.h"
#include "ULKHomeHUDWidget.generated.h"
class USpinBox;

class ALKHomeGameMode;
class ULKHomeListButtonWidget;
class UButton;
class UBorder;
class UHorizontalBox;
class UScrollBox;
class UTextBlock;
class UVerticalBox;
class USizeBox;

/**
 * 家园原生 HUD（H1/H2/H3/H4）：顶部资源与远征状态 + 建筑快捷栏 + 单张面板。
 *
 * 设计约束：
 *  - UI 只渲染 FLKHomePanelModel 并转发命令，不实现任何升级/战备/出征规则（规则在 Subsystem/GameMode）。
 *  - 同一时刻只有一张面板；遮罩挡住场景点击；Esc 关闭；战备有未保存草稿时先确认。
 *  - 蓝图子类可换肤：数据就绪时触发 OnHomePanelDataReadyBP / OnHomeTopBarChangedBP。
 */
UCLASS(Blueprintable, BlueprintType)
class ULKHomeHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeHomeHUD(ALKHomeGameMode* InGameMode);

	/** 按建筑稳定 ID 打开面板（鼠标点击/数字键/蓝图调用共用） */
	UFUNCTION(BlueprintCallable, Category = "LK|Home|UI")
	void OpenPanelForBuilding(FName BuildingId);
	UFUNCTION(BlueprintCallable, Category = "LK|Home|UI")
	void OpenPanel(ELKHomePanel Panel);
	/** 关闭当前面板；战备草稿未保存时进入确认状态 */
	UFUNCTION(BlueprintCallable, Category = "LK|Home|UI")
	void RequestClosePanel();
	/** 重读永久进度与远征状态并重建界面（幂等） */
	UFUNCTION(BlueprintCallable, Category = "LK|Home|UI")
	void Refresh();

	UFUNCTION(BlueprintPure, Category = "LK|Home|UI") bool IsPanelOpen() const { return CurrentPanel != ELKHomePanel::None; }
	UFUNCTION(BlueprintPure, Category = "LK|Home|UI") ELKHomePanel GetCurrentPanel() const { return CurrentPanel; }
	UFUNCTION(BlueprintPure, Category = "LK|Home|UI") FText GetGoldText() const;
	UFUNCTION(BlueprintPure, Category = "LK|Home|UI") FText GetExpeditionStatusText() const;
	UFUNCTION(BlueprintPure, Category = "LK|Home|UI") FText GetPanelStatusText() const;
	/** 最近一次操作结果（自动化与蓝图可读） */
	UFUNCTION(BlueprintPure, Category = "LK|Home|UI") const FText& GetLastMessage() const { return LastMessage; }
	UFUNCTION(BlueprintPure, Category = "LK|Home|UI") bool HasDirtyDraft() const { return bDraftDirty; }
	UFUNCTION(BlueprintPure, Category = "LK|Home|UI") FLKExpeditionLoadout GetDraftLoadout() const { return DraftLoadout; }

	/** 蓝图换肤入口：面板模型就绪 */
	UFUNCTION(BlueprintImplementableEvent, Category = "LK|Home|UI")
	void OnHomePanelDataReadyBP(const FLKHomePanelModel& Model);

	/** 蓝图换肤入口：顶部金币/远征状态变化 */
	UFUNCTION(BlueprintImplementableEvent, Category = "LK|Home|UI")
	void OnHomeTopBarChangedBP(const FText& InGoldText, const FText& InExpeditionStatus);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	void BuildNativeTree();
	void RebuildPanel();
	void RefreshTopBar();
	void ApplyMessage(const FText& Message);

	void HandleRowActivated(int32 Index);
	void HandleActionActivated(int32 Index);
	void HandleBuildingBarClicked(int32 Index);
	void ExecuteAction(FName ActionId);
	void ToggleDraftCard(FName CardId);
	void ResetDraftFromSaved();
	void UpdateDetail(const FLKHomePanelModel& Model);
	void CompleteDraftExit();
	FName GetCurrentUpgradeBuildingId() const;

	UFUNCTION() void HandleHomeRefreshRequested();
	UFUNCTION() void HandleProfileChanged();
	UFUNCTION() void HandleDepartureGoldChanged(float Value);
	UPROPERTY(Transient) TObjectPtr<UHorizontalBox> DepartureGoldRow;
	UPROPERTY(Transient) TObjectPtr<USpinBox> DepartureGoldInput;

	UPROPERTY(Transient) TObjectPtr<ALKHomeGameMode> HomeGameMode;

	UPROPERTY(Transient) TObjectPtr<UBorder> TopBar;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> GoldText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ExpeditionText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ProfileText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HintText;
	UPROPERTY(Transient) TObjectPtr<UHorizontalBox> BuildingBar;

	UPROPERTY(Transient) TObjectPtr<UBorder> Backdrop;
	UPROPERTY(Transient) TObjectPtr<UBorder> PanelHost;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PanelTitleText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PanelSubtitleText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PanelStatusText;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> RowsBox;
	UPROPERTY(Transient) TObjectPtr<UHorizontalBox> ActionsBox;
	UPROPERTY(Transient) TObjectPtr<UHorizontalBox> TabsBox;
	UPROPERTY(Transient) TObjectPtr<UScrollBox> RowsScroll;
	UPROPERTY(Transient) TObjectPtr<USizeBox> PanelSize;
	UPROPERTY(Transient) TObjectPtr<UBorder> DetailHost;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailTitle;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailCopy;
	UPROPERTY(Transient) TObjectPtr<UBorder> DetailBadge;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailGlyph;

	UPROPERTY(Transient) TArray<TObjectPtr<ULKHomeListButtonWidget>> RowWidgets;
	UPROPERTY(Transient) TArray<TObjectPtr<ULKHomeListButtonWidget>> ActionWidgets;
	UPROPERTY(Transient) TArray<TObjectPtr<ULKHomeListButtonWidget>> BuildingBarWidgets;

	ELKHomePanel CurrentPanel = ELKHomePanel::None;
	FName SelectedRowId;
	int32 BarracksTab = 0;
	bool bWarRoomHeroes = false;
	FLKExpeditionLoadout DraftLoadout;
	bool bDraftDirty = false;
	bool bConfirmDiscard = false;
	bool bConfirmAbandon = false;
	FText LastMessage;
	bool bBound = false;
	FLKHomePanelModel DisplayedModel;
	ELKHomePanel PendingPanel = ELKHomePanel::None;
	int32 SelectedHeroSlot = 0;
};
