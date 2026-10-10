#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LKRunTypes.h"
#include "ULKRunNodeSelectWidget.generated.h"

class ULKBattleHUDWidget;
class ULKRunSubsystem;
class ULKWorldMapWidget;
class UTextBlock;
class UVerticalBox;
class UHorizontalBox;
class USizeBox;
class ULKHomeListButtonWidget;
class ULKGameData;
class ULKCardDefinition;

/** 优化二：五区域世界地图、动态可选节点列表、市场/休息详情；旧档也可继续。 */
UCLASS(Blueprintable, BlueprintType)
class ULKRunNodeSelectWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void InitializeNodeSelect(ULKBattleHUDWidget* InOwnerHUD);
    UFUNCTION(BlueprintCallable, Category="LK|Run|UI") void RefreshNodeSelect();
    /** HUD polling keeps live buttons/Slate capture unless their displayed data changes. */
    void RefreshNodeSelectIfChanged();
    UFUNCTION(BlueprintPure, Category="LK|Run|UI") int32 GetDisplayedNodeCount() const { return DisplayedNodeIds.Num(); }
    void SelectMapNode(FName NodeId);
    FName GetSelectedNode() const { return SelectedNodeId; }
    /** 独立截图/集成测试绑定真实 Run；无 HUD 时不负责战斗切图。 */
    void InitializeRunView(ULKRunSubsystem* InRun);
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
    UFUNCTION(BlueprintImplementableEvent, Category="LK|Run|UI") void OnNodeDataReadyBP(int32 NodeCount);
    UPROPERTY(BlueprintReadOnly, Transient, Category="LK|Run|UI") TObjectPtr<ULKBattleHUDWidget> OwnerHUD;
private:
    void BuildNativeTree();
    void HandleAction(int32 Index);
    void RefreshDetail();
    void RefreshMarketCatalog();
    const ULKCardDefinition* FindCardDefinition(FName CardId);
    bool GetSelectedPurchaseValidation(FString& Error) const;
    ULKRunSubsystem* GetRun() const;
    ULKHomeListButtonWidget* AddAction(UPanelWidget* Parent, const FString& Name, const FString& Label, int32 Index, bool bEnabled=true);
    UPROPERTY(Transient) TObjectPtr<ULKRunSubsystem> ExplicitRun;
    UPROPERTY(Transient) TObjectPtr<ULKWorldMapWidget> Map;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Summary;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Detail;
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> ServiceActions;
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> FixedServiceActions;
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> MapColumn;
    UPROPERTY(Transient) TObjectPtr<class UBorder> MarketPane;
    UPROPERTY(Transient) TObjectPtr<UHorizontalBox> MarketTabs;
    UPROPERTY(Transient) TObjectPtr<class UUniformGridPanel> MarketGrid;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> MarketHint;
    UPROPERTY(Transient) TObjectPtr<USizeBox> SidebarSize;
    UPROPERTY(Transient) TObjectPtr<ULKGameData> PreviewCardData;
    UPROPERTY(Transient) TObjectPtr<ULKHomeListButtonWidget> AbandonAction;
    int32 SelectedMarketIndex=INDEX_NONE;
    int32 MarketTab = 0;
    int32 SelectedInventoryIndex = INDEX_NONE;
    TArray<FName> MarketRemoved;
    bool bConfirmAbandon=false;
    UPROPERTY(Transient) TObjectPtr<class UImage> NodeIllustration;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Status;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> ChoicesTitle;
    UPROPERTY(Transient) TObjectPtr<USizeBox> ChoicesFrame;
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> Choices;
    UPROPERTY(Transient) TObjectPtr<UHorizontalBox> RegionTabs;
    UPROPERTY(Transient) TObjectPtr<ULKHomeListButtonWidget> PrimaryAction;
    UPROPERTY(Transient) TObjectPtr<ULKHomeListButtonWidget> RunInventoryButton;
    TArray<FName> DisplayedNodeIds;
    TArray<FName> RegionIds;
    FName SelectedNodeId, LastCurrentNodeId;
    UPROPERTY(Transient) FLKRunState PresentedState;
    TWeakObjectPtr<ULKRunSubsystem> PresentedRun;
    FString PresentedPurchaseError;
    bool bPresentedPurchaseValid = false;
    bool bHasPresentedState = false;
    float FeedbackRemaining = 0.f;
};
