#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "LKRunTypes.h"
#include "ULKRunSaveGame.generated.h"

/**
 * D5 远征存档：只保存可序列化的 RunState 快照（值类型，无 Actor/ASC/GE 句柄）。
 * 版本号与 RunState.SchemaVersion 解耦：SaveVersion 管文件格式，SchemaVersion 管状态结构；
 * 载入时两者任一超出版本支持范围都明确拒绝，不静默当新档覆盖。
 */
UCLASS()
class ULKRunSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** 本文件格式版本；载入时 > 当前值拒绝 */
	UPROPERTY(VisibleAnywhere, Category = "LK|Run")
	int32 SaveVersion = 1;

	UPROPERTY(VisibleAnywhere, Category = "LK|Run")
	FLKRunState RunState;
	UPROPERTY() FDateTime SavedAtUtc;
};
