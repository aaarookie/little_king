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
    if (IsManualMoving()) { return; }

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

void ALKUnitHero::TryCastAbilities()
{
	if (!IsAlive() || !IsCombatEnabled() || !AbilitySystem || !bAbilitiesResolved || !GetTarget() || IsManualMoving())
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
    CampMoveRadius = FMath::Max(Radius, 2.f * GetBodyRadius() + Camp->GetBodyRadius() + 9.f);
    RallyPoint = GetActorLocation();
}

bool ALKUnitHero::CommandMove(const FVector& Destination)
{
    const ALKBattleGameMode* GM = GetWorld()->GetAuthGameMode<ALKBattleGameMode>();
    if (!IsAlive() || !HeroCamp.IsValid() || !GM || GM->GetPhase() == ELKGamePhase::Result
        || !MovementComponent->CanReach(Destination)) { return false; }
    RallyPoint = FVector(Destination.X, Destination.Y, 0.f);
    bManualMoving = true;
    ChangeTarget(nullptr);
    AbilitySystem->CancelAllAbilities();
    MovementComponent->MoveToward(RallyPoint, GetMoveSpeed());
    return true;
}

bool ALKUnitHero::CanPursueTarget(const ALKUnitBase* Target) const
{
    return Super::CanPursueTarget(Target) && (CampMoveRadius <= 0.f
        || FVector::Dist2D(CampCenter, Target->GetActorLocation()) <= CampMoveRadius - GetBodyRadius() + GetAttackRange());
}

FVector ALKUnitHero::GetChaseDestination(const ALKUnitBase* Target) const
{
    FVector Destination = Super::GetChaseDestination(Target);
    if (CampMoveRadius > 0.f)
    {
        Destination = CampCenter + (Destination - CampCenter).GetClampedToMaxSize2D(CampMoveRadius - GetBodyRadius());
    }
    return Destination;
}

void ALKUnitHero::UpdateStateMachine(float DeltaSeconds)
{
    if (bManualMoving)
    {
        CancelAttackWindup();
        if (FVector::Dist2D(GetActorLocation(), RallyPoint) <= 5.f)
        {
            bManualMoving = false;
            MovementComponent->Stop();
            TargetRetryTimer = 0.f;
        }
        else
        {
            State = ELKUnitState::Moving;
            MovementComponent->MoveToward(RallyPoint, GetMoveSpeed());
            return;
        }
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
    if (!bEnabled) { bManualMoving = false; }
}
