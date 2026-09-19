#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ULKStartMenuWidget.generated.h"

class ULKSaveSlotSubsystem;
class UVerticalBox;
class UTextBlock;
class UPanelWidget;
class USizeBox;

UCLASS()
class ULKStartMenuWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void InitializeMenu(ULKSaveSlotSubsystem* InSaves);
	void ShowMainMenu();
	void ShowLoadMenu();
	void RequestDelete(int32 SlotIndex);
	/** 预留给未来设置界面；本阶段只提示尚未开放。 */
	UFUNCTION(BlueprintImplementableEvent, Category="Start Menu") void OnSettingsRequested();
	bool IsLoadMenuOpen() const { return bLoadMenu; }
	int32 GetPendingDelete() const { return PendingDelete; }
	FString GetStatusText() const;
#if WITH_DEV_AUTOMATION_TESTS
	void SetSuppressTravelForTest(bool bValue) { bSuppressTravel = bValue; }
#endif
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
private:
	void BuildTree();
	void Refresh();
	void SetStatus(const FString& Message);
	void EnterGame(int32 SlotIndex, bool bNew);
	void HandleAction(int32 Action);
	void AddButton(UPanelWidget* Parent, const TCHAR* Name, const FString& Label, int32 Action, bool bEnabled = true, bool bPrimary = false);
	UPROPERTY(Transient) TObjectPtr<ULKSaveSlotSubsystem> Saves;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> Body;
	UPROPERTY(Transient) TObjectPtr<USizeBox> MenuFrame;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PageTitle;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PageSubtitle;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Status;
	bool bLoadMenu = false;
	bool bTravelQueued = false;
	bool bSuppressTravel = false;
	int32 PendingDelete = INDEX_NONE;
};
