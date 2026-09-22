#include "ALKUnitBuilding.h"
#include "ULKUnitAnimationComponent.h"

#include "Engine/World.h"

#include "ALKBattleGameMode.h"
#include "LKDataTypes.h"
#include "LKLog.h"
#include "ULKGameData.h"
#include "ULKUnitMovementComponent.h"

ALKUnitBuilding::ALKUnitBuilding()
{
	// 建筑不移动
	if (MovementComponent)
	{
		MovementComponent->SetComponentTickEnabled(false);
	}
}

void ALKUnitBuilding::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (IsDead() || !IsCombatEnabled() || IsControlled())
	{
		return;
	}

	if (BuildingBehavior == ELKBuildingBehavior::Barracks)
	{
		SpawnTimer -= DeltaSeconds;
		if (SpawnTimer <= 0.f)
		{
			SpawnTimer = SpawnInterval;
			TrySpawnUnit();
		}
	}
}

void ALKUnitBuilding::UpdateStateMachine(float DeltaSeconds)
{
	if (BuildingBehavior != ELKBuildingBehavior::Turret)
	{
		return;
	}

	// 哨塔：只攻击射程内的敌人，不移动
	TargetRetryTimer -= DeltaSeconds;
	if (TargetRetryTimer <= 0.f)
	{
		AcquireTarget();
		TargetRetryTimer = 0.5f;
	}

	ALKUnitBase* TargetUnit = Cast<ALKUnitBase>(TargetActor.Get());
	if (!TargetUnit || !TargetUnit->IsTargetable() || DistanceTo2D(TargetUnit) > GetAttackRange())
	{
		// 目标死亡或已跑出射程：清除目标，下个重试周期重新索敌（否则哨塔会永远发呆）
		ChangeTarget(nullptr);
		State = ELKUnitState::Idle;
		return;
	}

	State = ELKUnitState::Attacking;
	TryAttack(DeltaSeconds);
}

void ALKUnitBuilding::TrySpawnUnit()
{
    ALKBattleGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ALKBattleGameMode>() : nullptr;
    if (!GM || !GM->GetGameData() || SpawnUnitId.IsNone()) { return; }
    const float Distance = GetBodyRadius() + GM->GetGameData()->UnitBodyRadius + 8.f;
    for (int32 Index = 0; Index < 8; ++Index)
    {
        const float Angle = (Team == ELKTeam::Player ? PI * 0.5f : -PI * 0.5f) + Index * PI * 0.25f;
        const FVector Location = GetActorLocation() + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Distance;
        if (GM->SpawnUnitForTeam(SpawnUnitId, Team, Location))
        {
            GetAnimationComponent()->Attack();
            UE_LOG(LogLKUnit, Log, TEXT("[Building] %s 出兵 %s"), *UnitId.ToString(), *SpawnUnitId.ToString());
            return;
        }
    }
}
