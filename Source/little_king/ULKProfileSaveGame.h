#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "LKHomeTypes.h"
#include "ULKProfileSaveGame.generated.h"

/**
 * 家园永久档文件（H1）。外层 SaveVersion 与 FLKProfileState.SchemaVersion 分开：
 * 外层结构不变时只升 Profile.SchemaVersion 并写迁移分支。
 */
UCLASS()
class ULKProfileSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 SaveVersion = 1;

	UPROPERTY()
	FLKProfileState Profile;
	/** 优化一：新增兼容字段，旧文件缺省为 0；用于开始菜单排序。 */
	UPROPERTY() FDateTime SavedAtUtc;
};
