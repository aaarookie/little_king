#include "ALKUnitHero.h"
#include "ALKHeroCamp.h"
#include "ULKUnitMovementComponent.h"
#include "ALKBattleGameMode.h"
#include "Engine/World.h"

#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"
#include "GameplayTagContainer.h"

#include "LKDataTypes.h"
#include "LKLog.h"
#include "ULKGameData.h"

ALKUnitHero::ALKUnitHero()
{
	// 英雄默认不做额外初始化；属性由数据表行驱动
}

void ALKUnitHero::BeginPlay()
{
	Super::BeginPlay();

	// 授予类默认技能（BP 子类在 Abilities 里配置的技能；单位是 C++ 直接生成时一般为空）
	if (AbilitySystem)
	{
		for (const TSubclassOf<UGameplayAbility>& AbilityClass : Abilities)
		{
			if (AbilityClass)
			{
				FGameplayAbilitySpec Spec(AbilityClass, 1, INDEX_NONE, this);
				AbilitySystem->GiveAbility(Spec);
				UE_LOG(LogLKUnit, Log, TEXT("[Hero] %s 授予技能(类默认) %s"),
					*UnitId.ToString(), *AbilityClass->GetName());
			}
		}
	}
}

void ALKUnitHero::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (IsDead() || !IsCombatEnabled())
	{
		return;
	}

	// 冷却自然流逝（简单冷却：一个计时字段，不用 GE 冷却资产）
	SkillCooldownRemaining = FMath::Max(0.f, SkillCooldownRemaining - DeltaSeconds);
    if (IsManualMoving() || IsControlled()) { return; }

	AbilityCheckTimer -= DeltaSeconds;
	if (AbilityCheckTimer <= 0.f)
	{
		AbilityCheckTimer = AbilityCheckInterval;
		TryCastAbilities();
	}
}

void ALKUnitHero::OnUnitInitialized(const FLKUnitRow& Row)
{
	Super::OnUnitInitialized(Row);

	// 数据行可覆盖冷却（0 = 用类默认值 5 秒）
	if (Row.SkillCooldown > 0.f)
	{
		SkillCooldownSeconds = Row.SkillCooldown;
	}

	if (bAbilitiesResolved)
	{
		return;
	}
	bAbilitiesResolved = true;

	// 类默认技能优先（BeginPlay 已授予）；为空才走数据资产
	if (Abilities.Num() > 0)
	{
		return;
	}

	if (!AbilitySystem || !GameDataCached)
	{
		return;
	}

	if (const FLKHeroSkillEntry* Entry = GameDataCached->HeroAbilityMap.Find(UnitId))
	{
		for (const TSubclassOf<UGameplayAbility>& AbilityClass : Entry->Abilities)
		{
			if (AbilityClass)
			{
				FGameplayAbilitySpec Spec(AbilityClass, 1, INDEX_NONE, this);
				AbilitySystem->GiveAbility(Spec);
				UE_LOG(LogLKUnit, Log, TEXT("[Hero] %s 授予技能(数据驱动) %s"),
					*UnitId.ToString(), *AbilityClass->GetName());
			}
		}
	}
	else
	{
		UE_LOG(LogLKUnit, Log, TEXT("[Hero] %s 未配置技能（HeroAbilityMap 无此英雄条目，不影响战斗）"),
			*UnitId.ToString());
	}
}

void ALKUnitHero::ResetTransientRoomState()
{
	Super::ResetTransientRoomState();
	bManualMoving = false;
	ManualMoveStuckTimer = 0.f;
	RallyPoint = GetActorLocation();
	AbilityCheckTimer = 0.f;
	SkillCooldownRemaining = 0.f;
	if (AbilitySystem) { AbilitySystem->CancelAllAbilities(); }
}

void ALKUnitHero::TryCastAbilities()
{
	if (!IsAlive() || !IsCombatEnabled() || !AbilitySystem || !bAbilitiesResolved || !GetTarget() || IsManualMoving() || IsControlled())
	{
		return;
	}

	// 冷却中不尝试
	if (SkillCooldownRemaining > 0.f)
	{
		return;
	}

	const FGameplayTag AbilityTag = FGameplayTag::RequestGameplayTag(TEXT("LK.Ability"), false);
	if (!AbilityTag.IsValid())
	{
		return;
	}

	FGameplayTagContainer AbilityTags;
	AbilityTags.AddTag(AbilityTag);

	// 激活成功才进入冷却（技能没配/正在激活中返回 false，不扣冷却、下个周期重试）
	ALKBattleGameMode* GM = GetWorld()->GetAuthGameMode<ALKBattleGameMode>();
    if (GM) { GM->BeginCombatBatch(); }
    const bool bActivated = AbilitySystem->TryActivateAbilitiesByTag(AbilityTags);
    if (bActivated && GM && GameDataCached->FireballSkillHeroIds.Contains(UnitId))
    {
        GM->NotifyFireballCast(GetActorLocation());
    }
    if (GM) { GM->EndCombatBatch(); }
	if (bActivated)
	{
		SkillCooldownRemaining = SkillCooldownSeconds;
		UE_LOG(LogLKUnit, Log, TEXT("[Hero] %s 施放技能，进入冷却 %.1f 秒"),
			*UnitId.ToString(), SkillCooldownSeconds);
	}
}


void ALKUnitHero::SetCamp(ALKHeroCamp* Camp, float Radius)
{
    HeroCamp = Camp;
    CampCenter = Camp->GetActorLocation();
    // CampMoveRadius 仅作"营地范围圈"的显示半径与未来营地 buff 范围使用，不再限制英雄活动。
    CampMoveRadius = FMath::Max(Radius, 2.f * GetBodyRadius() + Camp->GetBodyRadius() + 9.f);
    RallyPoint = GetActorLocation();
}

bool ALKUnitHero::CommandMove(const FVector& Destination)
{
    const ALKBattleGameMode* GM = GetWorld()->GetAuthGameMode<ALKBattleGameMode>();
    // 活动范围不限距离：只校验目标地点可达性（出界/建筑与营地占据/路径被阻挡 → false）。
    if (!IsAlive() || !HeroCamp.IsValid() || !GM || GM->GetPhase() == ELKGamePhase::Result
        || !MovementComponent->CanReach(Destination)) { return false; }
    RallyPoint = FVector(Destination.X, Destination.Y, 0.f);
    bManualMoving = true;
    ManualMoveStuckTimer = 0.f;
    ManualMoveLastLocation = GetActorLocation();
    ChangeTarget(nullptr);
    AbilitySystem->CancelAllAbilities();
    MovementComponent->MoveToward(RallyPoint, GetMoveSpeed());
    return true;
}

bool ALKUnitHero::CanPursueTarget(const ALKUnitBase* Target) const
{
    // 英雄活动不再受营地范围限制：可全图追击。
    return Super::CanPursueTarget(Target);
}

FVector ALKUnitHero::GetChaseDestination(const ALKUnitBase* Target) const
{
    return Super::GetChaseDestination(Target);
}

void ALKUnitHero::UpdateStateMachine(float DeltaSeconds)
{
    if (bManualMoving)
    {
        CancelAttackWindup();
        // 移动指令强行打断战斗（用户规则 2026-09-09）：期间不索敌、不攻击，只走向落点，
        // 到达后再由下方战斗 FSM 恢复自动战斗。
        const bool bArrived = FVector::Dist2D(GetActorLocation(), RallyPoint) <= 5.f;
        if (!bArrived)
        {
            // 卡住检测：被单位/建筑挡住、或落点被占（贴脸推挤）导致长时间无位移 →
            // 结束指令交给战斗 FSM，避免"永远走不到点、也永远不打"的死锁（BUG-016 场景的收敛出口）。
            const float StuckTimeout = GameDataCached ? FMath::Max(0.2f, GameDataCached->HeroMoveStuckTimeout) : 1.5f;
            const bool bMoved = FVector::Dist2D(GetActorLocation(), ManualMoveLastLocation) >= 1.f;
            ManualMoveStuckTimer = bMoved ? 0.f : ManualMoveStuckTimer + DeltaSeconds;
            ManualMoveLastLocation = GetActorLocation();
            if (ManualMoveStuckTimer < StuckTimeout)
            {
                State = ELKUnitState::Moving;
                MovementComponent->MoveToward(RallyPoint, GetMoveSpeed());
                return;
            }
            UE_LOG(LogLKUnit, Verbose, TEXT("[Hero] %s 手动移动卡住 %.1f 秒（落点 %s），恢复自动战斗"),
                *UnitId.ToString(), ManualMoveStuckTimer, *RallyPoint.ToCompactString());
        }

        bManualMoving = false;
        ManualMoveStuckTimer = 0.f;
        MovementComponent->Stop();
        TargetRetryTimer = 0.f;
        // 恢复自动战斗：清空目标让战斗 FSM 重新索敌（含嘲讽/集火优先级）。
        ChangeTarget(nullptr);
    }
    if (!IsCombatEnabled()) { State = ELKUnitState::Idle; return; }
    Super::UpdateStateMachine(DeltaSeconds);
    if (State == ELKUnitState::Idle && HeroCamp.IsValid() && FVector::Dist2D(GetActorLocation(), RallyPoint) > 5.f)
    {
        MovementComponent->MoveToward(RallyPoint, GetMoveSpeed());
    }
}

ALKHeroCamp* ALKUnitHero::GetCamp() const { return HeroCamp.Get(); }

void ALKUnitHero::SetCombatEnabled(bool bEnabled)
{
    Super::SetCombatEnabled(bEnabled);
    if (!bEnabled) { bManualMoving = false; ManualMoveStuckTimer = 0.f; }
}
