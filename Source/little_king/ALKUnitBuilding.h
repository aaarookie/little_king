#pragma once

#include "CoreMinimal.h"
#include "ALKUnitBase.h"
#include "LKTypes.h"
#include "ALKUnitBuilding.generated.h"

/**
 * 建筑单位（静态）：哨塔自动攻击 / 兵营周期出兵。
 * 建筑不移动；行为由数据表行 BuildingBehavior 决定。
 */
UCLASS()
class ALKUnitBuilding : public ALKUnitBase
{
	GENERATED_BODY()

public:
	ALKUnitBuilding();

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditDefaultsOnly, Category = "LK|Building")
	ELKBuildingBehavior BuildingBehavior = ELKBuildingBehavior::None;

	/** 兵营产出的单位 ID（由 GameMode 从数据表行注入） */
	UPROPERTY(EditDefaultsOnly, Category = "LK|Building")
	FName SpawnUnitId = NAME_None;

	UPROPERTY(EditDefaultsOnly, Category = "LK|Building", meta = (ClampMin = "1.0"))
	float SpawnInterval = 8.f;

protected:
	virtual void UpdateStateMachine(float DeltaSeconds) override;

	/** 兵营：周期出兵（由 GameMode 生成，位置在建筑附近） */
	void TrySpawnUnit();

	float SpawnTimer = 0.f;
};
