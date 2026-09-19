#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LKNavigation.h"
#include "ULKUnitMovementComponent.generated.h"

/**
 * 轻量移动组件：绕实体建筑的轻量路径 + 仅推自身的软分离 + 战场矩形边界。
 * 营地圈不再限制移动（英雄活动不限距离；圈仅显示与未来营地 buff 用）。
 * 刻意不用 UE NavMesh / CharacterMovement（2D 战场用不上）。
 */
UCLASS(ClassGroup = (LK), meta = (BlueprintSpawnableComponent))
class ULKUnitMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULKUnitMovementComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** 朝目标点移动（Speed 由调用方每帧传入，便于读 GAS 属性） */
	void MoveToward(const FVector& Destination, float Speed);
	bool CanReach(const FVector& Target) const;
    /** Skill movement obeys the same field/static-body geometry as normal navigation. */
    bool CanStandAt(const FVector& Location, bool bIncludeUnits = false) const;
    bool TeleportToFreePoint(const FVector& Location);
    FVector MoveSkillDelta(const FVector& Delta);

	void Stop() { bMoving = false; }

	bool IsMoving() const { return bMoving; }

	void SetSeparationRadius(float Radius) { SeparationRadius = FMath::Max(1.f, Radius); }

	/** 设置战场边界（InitUnit 时由单位注入），移动/分离后钳制位置，防止单位走出战场 */
	void SetFieldBounds(const FVector2D& InHalfExtent)
	{
		FieldHalfExtent = InHalfExtent;
		bHasFieldBounds = true;
	}

private:
	bool bMoving = false;
	FVector Destination = FVector::ZeroVector;
	float Speed = 0.f;
	float SeparationRadius = 50.f;

	FVector2D FieldHalfExtent = FVector2D(1200.f, 2000.f);
	bool bHasFieldBounds = false;
	TArray<FVector> Path;
	FVector PathDestination = FVector::ZeroVector;
	float RepathTimer = 0.f;
	void CollectNavigation(TArray<LKNavigation::FObstacle>& Obstacles, LKNavigation::FBounds& Bounds) const;

	/** 与周围单位径向分离 */
	void ApplySeparation(float DeltaTime);

	/** 把 Owner 位置钳制在战场矩形内 */
	void ClampToFieldBounds();
};
