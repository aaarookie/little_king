#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameFramework/SaveGame.h"
#include "LKRunTypes.h"
#include "ULKSaveSlotSubsystem.generated.h"

USTRUCT(BlueprintType)
struct FLKSaveSlotSummary
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 Index = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly) bool bExists = false;
	UPROPERTY(BlueprintReadOnly) bool bCanLoad = false;
	UPROPERTY(BlueprintReadOnly) int32 Gold = 0;
	UPROPERTY(BlueprintReadOnly) int32 StatueLevel = 1;
	UPROPERTY(BlueprintReadOnly) bool bHasRun = false;
	UPROPERTY(BlueprintReadOnly) ELKRunPhase RunPhase = ELKRunPhase::Inactive;
	UPROPERTY(BlueprintReadOnly) FDateTime SavedAtUtc;
	UPROPERTY(BlueprintReadOnly) FString Error;
};

/** 删除标记先落盘；中断删除后的残留文件不会变成可继续的旧档。 */
UCLASS()
class ULKDeletedSlotSaveGame : public USaveGame
{
	GENERATED_BODY()
};

/** 八个逻辑档，每档包含 Profile A/B 和 Run。选择保存在当前 GameInstance，不修改全局测试钩子。 */
UCLASS()
class ULKSaveSlotSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	static constexpr int32 MaxSlots = 8;
	TArray<FLKSaveSlotSummary> ListSlots() const;
	FLKSaveSlotSummary InspectSlot(int32 Index) const;
	int32 GetMostRecentSlot() const;
	bool CreateNewGame();
	bool LoadSlot(int32 Index);
	bool DeleteSlot(int32 Index);
	void ResetSession();
	int32 GetActiveSlot() const { return ActiveSlot; }
	bool HasSelectedGame() const { return ActiveSlot != INDEX_NONE; }
	const FString& GetLastError() const { return LastError; }
	FName GetEntryMap() const;
	FString ProfileBase(int32 Index) const;
	FString RunSlot(int32 Index) const;
	FString DeleteMarker(int32 Index) const;
	/** 仅供自动化/截图使用；实例级隔离，不访问已有玩家档。 */
	void SetNamespaceForTest(const FString& Prefix) { TestNamespace = Prefix; }
	/** 两张既有地图启动前调用；每次新 PIE 先回开始菜单，自动化/显式截图模式除外。 */
	static bool RouteInitialPlayToMenu(UWorld* World);
private:
	int32 ActiveSlot = INDEX_NONE;
	FString LastError;
	FString TestNamespace;
};
