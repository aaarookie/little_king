#include "ALKUnitBuilding.h"

#include "Engine/World.h"

#include "ALKBattleGameMode.h"
#include "LKDataTypes.h"
#include "LKLog.h"
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

	if (IsDead() || !IsCombatEnabled())
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
	if (!TargetUnit || TargetUnit->IsDead() || DistanceTo2D(TargetUnit) > GetAttackRange())
	{
		// 目标死亡或已跑出射程：清除目标，下个重试周期重新索敌（否则哨塔会永远发呆）
		TargetActor = nullptr;
		State = ELKUnitState::Idle;
		return;
	}

	State = ELKUnitState::Attacking;
	TryAttack(DeltaSeconds);
}

void ALKUnitBuilding::TrySpawnUnit()
{
	UWorld* World = GetWorld();
	ALKBattleGameMode* GameMode = World ? World->GetAuthGameMode<ALKBattleGameMode>() : nullptr;
	if (!GameMode || SpawnUnitId.IsNone())
	{
		return;
	}

	// 朝中线方向出兵：玩家（Y<0）向 +Y，敌方（Y>0）向 -Y
	const float ForwardSign = (Team == ELKTeam::Player) ? 1.f : -1.f;
	const FVector SpawnLoc = GetActorLocation() + FVector(0.f, ForwardSign * 100.f, 0.f);
	if (ALKUnitBase* Spawned = GameMode->SpawnUnitForTeam(SpawnUnitId, Team, SpawnLoc))
	{
		UE_LOG(LogLKUnit, Log, TEXT("[Building] %s 出兵 %s"), *UnitId.ToString(), *SpawnUnitId.ToString());
	}
}
