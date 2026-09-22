#include "ALKUnitBase.h"

#include "AbilitySystemComponent.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/SphereComponent.h"
#include "Components/SceneComponent.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"
#include "Engine/DataTable.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "Math/RotationMatrix.h"
#include "PaperSprite.h"
#include "PaperSpriteComponent.h"

#include "ALKProjectile.h"
#include "ALKBattleGameMode.h"
#include "LKDataTypes.h"
#include "LKGameplayHelpers.h"
#include "LKLog.h"
#include "ULKGameData.h"
#include "ULKTraitAuraComponent.h"
#include "ULKUnitAttributeSet.h"
#include "ULKUnitMovementComponent.h"
#include "ULKUnitPassiveComponent.h"
#include "ULKUnitActiveComponent.h"
#include "ULKUnitStatusComponent.h"
#include "ULKUnitAnimationComponent.h"
#include "LKRunTypes.h"
#include "ULKPresentationSubsystem.h"

ALKUnitBase::ALKUnitBase()
{
	PrimaryActorTick.bCanEverTick = true;

	SpriteComponent = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("Sprite"));
	LogicRoot = CreateDefaultSubobject<USceneComponent>(TEXT("LogicRoot"));
    SetRootComponent(LogicRoot);
    SpriteComponent->SetupAttachment(LogicRoot);
	SpriteComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    // Masked sprites need a small visual lift above the battle floor at Z=0.
    // Keep the logic root / body collision on the ground plane.
    SpriteComponent->SetRelativeLocation(FVector(0.f, 0.f, 8.f));

	// Paper2D 精灵默认"立着"（纸片竖在 XZ 平面、正面朝 +Y——为横版游戏设计）。
	// 本项目是俯视战场（相机 Pitch=-90 从 +Z 俯视），必须把精灵放倒：
	//   精灵正面 → 世界 +Z（朝向相机）
	//   精灵宽边 → 世界 +Y（屏幕右）
	//   精灵"头"  → 世界 +X（屏幕上方）
	// 若实际效果头朝下/镜像，把下面第一个向量的 Y 取反即可（-1,0,0→用 0,-1,0）。
	const FVector SpriteWidthDir(0.f, 1.f, 0.f);
	const FVector SpriteFaceDir(0.f, 0.f, 1.f);
	SpriteComponent->SetRelativeRotation(FRotationMatrix::MakeFromXY(SpriteWidthDir, SpriteFaceDir).Rotator());

	BodyCollision = CreateDefaultSubobject<USphereComponent>(TEXT("Body"));
	BodyCollision->SetupAttachment(LogicRoot);
	BodyCollision->SetSphereRadius(BodyRadius);
	BodyCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BodyCollision->SetCollisionObjectType(ECC_Pawn);
	BodyCollision->SetCollisionResponseToAllChannels(ECR_Overlap);
	BodyCollision->SetGenerateOverlapEvents(true);

	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetIsReplicated(false);

	UnitAttributes = CreateDefaultSubobject<ULKUnitAttributeSet>(TEXT("Attributes"));

	MovementComponent = CreateDefaultSubobject<ULKUnitMovementComponent>(TEXT("Movement"));

	// 光环组件：默认空挂（不启用），有光环特性（InitUnit）时注册修改器后生效
	TraitAuraComponent = CreateDefaultSubobject<ULKTraitAuraComponent>(TEXT("TraitAura"));
	PassiveComponent = CreateDefaultSubobject<ULKUnitPassiveComponent>(TEXT("Passive"));
    ActiveComponent = CreateDefaultSubobject<ULKUnitActiveComponent>(TEXT("NativeActive"));
    StatusComponent = CreateDefaultSubobject<ULKUnitStatusComponent>(TEXT("CombatStatus"));
    AnimationComponent = CreateDefaultSubobject<ULKUnitAnimationComponent>(TEXT("UnitAnimation"));
}

void ALKUnitBase::BeginPlay()
{
	Super::BeginPlay();

	if (AbilitySystem && UnitAttributes)
	{
		AbilitySystem->AddAttributeSetSubobject(UnitAttributes.Get());
		AbilitySystem->InitAbilityActorInfo(this, this);
	}
}

UAbilitySystemComponent* ALKUnitBase::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void ALKUnitBase::InitUnit(const FLKUnitRow& Row, ULKGameData* InGameData, FName FallbackUnitId)
{
    StatusComponent->Clear();
	GameDataCached = InGameData;

	// UnitId 以行内字段为准；为空时用行名兜底（并告警提示补齐数据表）
	const FName RowUnitId = Row.UnitId;
	UnitId = RowUnitId.IsNone() ? FallbackUnitId : RowUnitId;
	if (RowUnitId.IsNone() && !FallbackUnitId.IsNone())
	{
		UE_LOG(LogLKUnit, Warning,
			TEXT("[Unit] 行 '%s' 的 UnitId 字段为空，已用行名兜底（建议把 DT_Units 该行的 UnitId 填成行名）"),
			*FallbackUnitId.ToString());
	}

	UnitClass = Row.UnitClass;
	DisplayName = Row.DisplayName.IsEmpty() ? FText::FromName(UnitId) : Row.DisplayName;
	bSkeleton = Row.bSkeleton;
	PlaceholderColor = Row.PlaceholderColor;
	PassiveComponent->Initialize(Row);
    ActiveComponent->Initialize(Row);
    Race = Row.Race;
    Quality = Row.Quality;
    bTargetsBuildingsOnly = Row.bTargetsBuildingsOnly;
	bIsMage = Row.bIsMage;
	AttackType = Row.AttackType;
	HeroTraits = Row.HeroTraits;
    // 迁移旧原型的默认属性特性；新版英雄默认行为来自独立的特性配置。
    if (IsHero() && InGameData)
    {
        HeroTraits.Remove(TEXT("Trait_KnightAura"));
        HeroTraits.Remove(TEXT("Trait_MageMight"));
        if (const FLKHeroTraitEntry* Entry = InGameData->DefaultHeroTraits.Find(UnitId))
        {
            for (FName Id : Entry->Traits) { HeroTraits.AddUnique(Id); }
        }
    }
	BodyRadius = InGameData ? InGameData->UnitBodyRadius : 50.f;
	// 索敌范围：行内值优先，0 表示用全局默认（DA_GameData.UnitAcquireRadius）。
	AcquireRadius = Row.AcquireRadius > 0.f ? Row.AcquireRadius
		: (InGameData ? InGameData->UnitAcquireRadius : 900.f);
    if (bTargetsBuildingsOnly) { AcquireRadius = GetAttackRange(); }
	BodyCollision->SetSphereRadius(BodyRadius);
	MovementComponent->SetSeparationRadius(BodyRadius);
	if (InGameData)
	{
		MovementComponent->SetFieldBounds(FVector2D(InGameData->FieldHalfWidth, InGameData->FieldHalfHeight));
	}

	// 精灵（占位期可能没有，用调试色块代替）
	BaseSpriteLocalScale = FVector(1.f, 1.f, 1.f);
	if (!Row.Sprite.IsNull())
	{
		if (UPaperSprite* Sprite = Row.Sprite.LoadSynchronous())
		{
			SpriteComponent->SetSprite(Sprite);

			// 精灵缩放必须作用在【组件本地坐标】上（宽=本地X、高=本地Z），
			// 且要在组件旋转【之前】生效——否则旋转后宽高与世界轴错位、非等比缩放会变形。
			// 不能缩 Actor：Actor 缩放沿世界轴，会把宽高缩反并连带缩放碰撞体。
			SpriteComponent->SetRelativeScale3D(FVector(Row.SpriteScale.X, 1.f, Row.SpriteScale.Y));
			BaseSpriteLocalScale = SpriteComponent->GetRelativeScale3D(); // S5 打击感动画的基准
		}
	}

	ApplyRowAttributes(Row);
    AnimationComponent->Initialize();

	// 数据就绪回调（英雄在此授予数据驱动技能等）
	OnUnitInitialized(Row);

	UE_LOG(LogLKUnit, Log, TEXT("[Unit] Spawn %s (class=%d, team=%d) @ %s"),
		*UnitId.ToString(), (int32)UnitClass, (int32)Team, *GetActorLocation().ToString());
}

void ALKUnitBase::OnUnitInitialized(const FLKUnitRow& Row)
{
	// 基类：应用特性（自身修饰/光环/嘲讽）——英雄子类覆写时先调 Super 再处理技能
	ApplyTraits();
}

bool ALKUnitBase::ApplyRunHeroState(const FLKRunHeroState& RunState)
{
    if (!IsHero() || IsDead() || RunState.HeroId != UnitId || !FMath::IsFinite(RunState.Health)
        || RunState.Health <= 0.f || !FMath::IsFinite(RunState.MaxHealth) || RunState.MaxHealth <= 0.f
		|| !FMath::IsFinite(RunState.BaseMaxHealth) || RunState.BaseMaxHealth < 0.f)
    { return false; }
    TSet<FName> UniqueTraits;
	for (FName Trait : RunState.Traits)
	{
		FLKTraitRow Resolved;
		if (Trait.IsNone() || UniqueTraits.Contains(Trait) || !ResolveTrait(Trait, Resolved)) { return false; }
		UniqueTraits.Add(Trait);
	}
    HeroTraits = UniqueTraits.Array();
    HeroTraits.Sort(FNameLexicalLess());
	ResetTransientRoomState();
	const float DesiredBaseMaximum = RunState.BaseMaxHealth > 0.f ? RunState.BaseMaxHealth : RunState.MaxHealth;
	AbilitySystem->SetNumericAttributeBase(ULKUnitAttributeSet::GetMaxHealthAttribute(), DesiredBaseMaximum);
    ApplyTraits();
    // BUG-017 排查埋点：打印上场时真正生效的永久特性，
    // 便于一眼确认"法师是否带着全场施法上场"，不必再靠猜。
    const FString TraitList = HeroTraits.Num() > 0
        ? FString::JoinBy(HeroTraits, TEXT("，"), [](const FName& Id) { return Id.ToString(); })
        : FString(TEXT("无"));
    UE_LOG(LogLKUnit, Log, TEXT("[Run] %s 应用永久特性：%s"), *UnitId.ToString(), *TraitList);
    const float CurrentMaximum = GetMaxHealth();
    if (!FMath::IsNearlyEqual(CurrentMaximum, RunState.MaxHealth, 0.1f))
    {
		UE_LOG(LogLKUnit, Verbose, TEXT("[Run] %s 最大生命快照 %.1f，新房重算 %.1f；按新房值钳制"),
            *UnitId.ToString(), RunState.MaxHealth, CurrentMaximum);
    }
    AbilitySystem->SetNumericAttributeBase(ULKUnitAttributeSet::GetHealthAttribute(), FMath::Clamp(RunState.Health, 0.f, CurrentMaximum));
    OnHealthChanged(GetHealth(), CurrentMaximum);
    return true;
}

void ALKUnitBase::ResetTransientRoomState()
{
    AnimationComponent->ResetPresentation();
    StatusComponent->Clear();
    if (ActiveComponent) { ActiveComponent->Stop(); }
	ChangeTarget(nullptr);
	ForcedTargetActor.Reset();
	ForcedTargetRemaining = 0.f;
	FocusWarningRemaining = 0.f;
	AuraTauntSources.Reset();
	TargetRetryTimer = 0.f;
	AttackCooldownRemaining = 0.f;
	CancelAttackWindup();
	if (MovementComponent) { MovementComponent->Stop(); }
}

void ALKUnitBase::ApplyTraits()
{
    TraitAuraComponent->ResetAuras();
    for (FActiveGameplayEffectHandle Handle : SelfTraitHandles)
    {
        AbilitySystem->RemoveActiveGameplayEffect(Handle);
    }
    SelfTraitHandles.Reset();
    ResolvedTraits.Reset();
    bTaunting = false;
    if (IsCamp() || IsDead()) { return; }
    for (FName Id : HeroTraits)
    {
        FLKTraitRow Row;
        if (!ResolveTrait(Id, Row)) { continue; }
        ResolvedTraits.Add(Id, Row);
        bTaunting |= Row.Effect == ELKTraitEffect::Taunt;
        if (Row.Effect == ELKTraitEffect::MeleeSoldierTauntAura)
        {
            TraitAuraComponent->AddTauntAura(Row.EffectRadius);
        }
        for (const FLKTraitModifier& Mod : Row.Modifiers)
        {
            if (Mod.AuraRadius > 0.f) { TraitAuraComponent->AddAuraModifier(Mod, Mod.AuraRadius); continue; }
            const FGameplayAttribute Attr = LKGameplay::FindAttributeByName(Mod.StatName);
            if (Attr.IsValid())
            {
                const float Base = LKGameplay::GetAttributeBaseValue(this, Attr, 0.f);
                SelfTraitHandles.Add(LKGameplay::ApplyAttributeModifier(this, Attr, Base * Mod.Value, 0.f, this));
            }
        }
    }
    TraitAuraComponent->TickAura(0.f);
}

bool ALKUnitBase::ResolveTrait(FName Id, FLKTraitRow& Row) const
{
    Row.TraitId = Id;
    if (Id == "Trait_Sacrifice") { Row.TraitName = FText::FromString(TEXT("献祭")); Row.Effect = ELKTraitEffect::SummoningHealthCost; Row.EffectValue = 0.08f; return true; }
    if (Id == "Trait_FaceFear") { Row.TraitName = FText::FromString(TEXT("直面恐惧")); Row.Effect = ELKTraitEffect::RangedDamageReduction; Row.EffectValue = 0.3f; return true; }
    if (Id == TEXT("Trait_MageSpellReach")) { Row.Effect = ELKTraitEffect::GlobalSpellPlacement; return true; }
    if (Id == TEXT("Trait_KnightTauntAura"))
    {
        Row.Effect = ELKTraitEffect::MeleeSoldierTauntAura;
        Row.EffectRadius = GameDataCached ? GameDataCached->KnightTauntAuraRadius : 400.f;
        return true;
    }
    if (Id == TEXT("Taunt")) { Row.Effect = ELKTraitEffect::Taunt; return true; }
    if (GameDataCached)
    {
        if (UDataTable* Table = GameDataCached->TraitTable.LoadSynchronous())
        {
            if (Table->GetRowStruct() == FLKTraitRow::StaticStruct())
            {
                if (const FLKTraitRow* Found = Table->FindRow<FLKTraitRow>(Id, TEXT("Traits"), false))
                {
                    Row = *Found;
                    if (Row.TraitId == TEXT("Taunt")) { Row.Effect = ELKTraitEffect::Taunt; }
                    return true;
                }
            }
        }
    }
    UE_LOG(LogLKUnit, Warning, TEXT("[Trait] 无效特性 %s"), *Id.ToString());
    return false;
}

bool ALKUnitBase::AddTrait(FName Id)
{
    FLKTraitRow Row;
    if (IsDead() || IsCamp() || HeroTraits.Contains(Id) || !ResolveTrait(Id, Row)) { return false; }
    HeroTraits.Add(Id);
    ApplyTraits();
    return true;
}

bool ALKUnitBase::RemoveTrait(FName Id)
{
    if (HeroTraits.Remove(Id) == 0) { return false; }
    ApplyTraits();
    return true;
}

bool ALKUnitBase::HasTraitEffect(ELKTraitEffect Effect) const
{
    if (!IsTargetable()) { return false; }
    for (const auto& Pair : ResolvedTraits) { if (Pair.Value.Effect == Effect) { return true; } }
    return false;
}

bool ALKUnitBase::IsTaunting() const
{
    if (!IsTargetable()) { return false; }
    if (bTaunting) { return true; }
    for (const auto& Pair : AuraTauntSources)
    {
        if (Pair.Key.IsValid() && Pair.Key->IsAlive() && Pair.Value > 0) { return true; }
    }
    return false;
}

void ALKUnitBase::AddAuraTauntSource(ALKUnitBase* Source)
{
    const bool bHadTaunt=IsTaunting();
    if (Source) { ++AuraTauntSources.FindOrAdd(Source); }
    if (!bHadTaunt && IsTaunting() && IsCombatEnabled()) { ULKPresentationSubsystem::Sound(GetWorld(), "Empower", GetActorLocation()); }
}
void ALKUnitBase::SetFocusWarning(float Seconds)
{
    if (FocusWarningRemaining<=0.f && Seconds>0.f) { ULKPresentationSubsystem::Sound(GetWorld(), "CampCommand", GetActorLocation()); }
    FocusWarningRemaining=FMath::Max(0.f,Seconds);
}
void ALKUnitBase::RemoveAuraTauntSource(ALKUnitBase* Source)
{
    if (int32* Count = AuraTauntSources.Find(Source)) { if (--*Count <= 0) { AuraTauntSources.Remove(Source); } }
}

void ALKUnitBase::EndPlay(const EEndPlayReason::Type Reason)
{
    TraitAuraComponent->ResetAuras();
    ULKUnitStatusComponent::RefreshTeamSupport(GetWorld());
    Super::EndPlay(Reason);
}

void ALKUnitBase::ApplyRowAttributes(const FLKUnitRow& Row)
{
	if (!AbilitySystem)
	{
		return;
	}

	AbilitySystem->SetNumericAttributeBase(ULKUnitAttributeSet::GetHealthAttribute(), Row.BaseHealth);
	AbilitySystem->SetNumericAttributeBase(ULKUnitAttributeSet::GetMaxHealthAttribute(), Row.BaseHealth);
	AbilitySystem->SetNumericAttributeBase(ULKUnitAttributeSet::GetMoveSpeedAttribute(), Row.MoveSpeed);
	AbilitySystem->SetNumericAttributeBase(ULKUnitAttributeSet::GetAttackRangeAttribute(), Row.AttackRange);
	AbilitySystem->SetNumericAttributeBase(ULKUnitAttributeSet::GetAttackDamageAttribute(), Row.AttackDamage);
	AbilitySystem->SetNumericAttributeBase(ULKUnitAttributeSet::GetAttackIntervalAttribute(), Row.AttackInterval);

	// S5：攻击前摇（行可配，0 = 无前摇）
	AttackWindup = FMath::Clamp(Row.AttackWindup, 0.f, 0.5f);
}

void ALKUnitBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bDead)
	{
		// S5 死亡表现：缩小 + 淡出（动画结束后由 LifeSpan 销毁）
		if (SpriteComponent && DeathAnimRemaining > 0.f)
		{
			DeathAnimRemaining -= DeltaSeconds;
			const float t = FMath::Clamp(DeathAnimRemaining / DeathAnimDuration, 0.f, 1.f);
			SpriteComponent->SetRelativeScale3D(BaseSpriteLocalScale * (AnimationComponent->HasAnimations() ? 1.f : FMath::Max(t, 0.05f)));
            if (IsHero() && AnimationComponent->HasAnimations())
            {
                // Heroes remain incapacitated in the room; keep the authored fallen pose visible.
                const float Shade = .55f + .45f * t;
                SpriteComponent->SetSpriteColor(FLinearColor(Shade, Shade, Shade, 1.f));
            }
            else { SpriteComponent->SetSpriteColor(FLinearColor(1.f, 1.f, 1.f, t)); }
		}
		return;
	}

	// 无敌计时（部署阶段也走表，不影响开战冻结逻辑）
	if (bInvulnerable && InvulnerableRemaining > 0.f)
	{
		InvulnerableRemaining -= DeltaSeconds;
		if (InvulnerableRemaining <= 0.f)
		{
			bInvulnerable = false;
			InvulnerableRemaining = 0.f;
			UE_LOG(LogLKUnit, Log, TEXT("[Unit] %s 无敌结束"), *UnitId.ToString());
		}
	}

	// 强制目标到期（集火窗口结束自动解除）
	TickForcedTarget(DeltaSeconds);

	// 光环刷新（内部 0.5s 节流）
	if (TraitAuraComponent)
	{
		TraitAuraComponent->TickAura(DeltaSeconds);
	}

	TickCombatFeedback(DeltaSeconds);

	DrawDebugShape();

	// 部署阶段：单位冻结（不索敌/不移动/不攻击），开战后由 GameMode 打开
	if (!bCombatEnabled)
	{
        if (IsManualMoving()) { UpdateStateMachine(DeltaSeconds); return; }
		if (State != ELKUnitState::Idle)
		{
			State = ELKUnitState::Idle;
			MovementComponent->Stop();
		}
		return;
	}

    StatusComponent->TickStatus(DeltaSeconds);
    if (IsDead() || !IsCombatEnabled()) { return; }
	AttackCooldownRemaining = FMath::Max(0.f, AttackCooldownRemaining - DeltaSeconds);
    HitStopRemaining = FMath::Max(0.f, HitStopRemaining - DeltaSeconds);
    FocusWarningRemaining = FMath::Max(0.f, FocusWarningRemaining - DeltaSeconds);
    if (ActiveComponent->TickAbility(DeltaSeconds)) { State = ELKUnitState::Casting; return; }
    if (IsControlled()) { MovementComponent->Stop(); CancelAttackWindup(); State = ELKUnitState::Idle; return; }
    if (IsDead()) { return; }
    UpdateStateMachine(DeltaSeconds);
}

bool ALKUnitBase::IsSkillMoving() const { return ActiveComponent && ActiveComponent->IsDashing(); }
bool ALKUnitBase::IsControlled() const { return StatusComponent && StatusComponent->IsControlled(); }

/** S5：攻击脉冲回弹 + 受击闪白恢复（纯视觉） */
void ALKUnitBase::TickCombatFeedback(float DeltaSeconds)
{
	if (!SpriteComponent || SpriteComponent->GetSprite() == nullptr)
	{
		// 无精灵（调试色块期）：只清计时，不做精灵表现
		AttackPulseRemaining = 0.f;
		HitFlashRemaining = 0.f;
		return;
	}

	// 攻击缩放脉冲：命中瞬间 1.15 -> 1.0 线性回弹
	if (AttackPulseRemaining > 0.f)
	{
		AttackPulseRemaining -= DeltaSeconds;
		const float t = FMath::Clamp(AttackPulseRemaining / 0.15f, 0.f, 1.f);
		SpriteComponent->SetRelativeScale3D(BaseSpriteLocalScale * (1.f + 0.15f * t));
		if (AttackPulseRemaining <= 0.f)
		{
			SpriteComponent->SetRelativeScale3D(BaseSpriteLocalScale);
		}
	}

	// 受击闪白恢复（TriggerHitFlash 置白，0.08s 后还原）
	if (HitFlashRemaining > 0.f)
	{
		HitFlashRemaining -= DeltaSeconds;
		if (HitFlashRemaining <= 0.f)
		{
			SpriteComponent->SetSpriteColor(FLinearColor::White);
		}
	}
}

void ALKUnitBase::TriggerHitFlash()
{
	// 精灵颜色乘大数提亮；深色像素不保证纯白，严格白闪需配套材质。
	if (!SpriteComponent || SpriteComponent->GetSprite() == nullptr)
	{
		return;
	}
	HitFlashRemaining = 0.08f;
    AnimationComponent->Hit();
	SpriteComponent->SetSpriteColor(FLinearColor(4.f, 4.f, 4.f, 1.f));
}

void ALKUnitBase::UpdateStateMachine(float DeltaSeconds)
{
    if (IsBuilding()) { return; }
    TargetRetryTimer -= DeltaSeconds;
    if (TargetRetryTimer <= 0.f) { AcquireTarget(); TargetRetryTimer = 0.15f; }
    ALKUnitBase* Target = Cast<ALKUnitBase>(TargetActor.Get());
    if (Target && ShouldReleaseTarget(Target))
    {
        // 普通目标离开索敌范围 / 嘲讽者离开嘲讽半径：释放并立刻重新索敌（下一个目标）。
        ChangeTarget(nullptr);
        TargetRetryTimer = 0.f;
        Target = nullptr;
    }
    if (!Target || !Target->IsTargetable() || !CanPursueTarget(Target))
    {
        ChangeTarget(nullptr);
        // 无目标：非英雄单位向"全图最近敌人"行军（保证双方仍会接触），
        // 英雄保持由玩家指挥/回到集结点（子类处理）。
        if (!IsHero() && IsCombatEnabled())
        {
            if (AActor* FarEnemy = FindNearestEnemy(false, true))
            {
                State = ELKUnitState::Moving;
                MovementComponent->MoveToward(FarEnemy->GetActorLocation(), GetMoveSpeed());
                return;
            }
        }
        State = ELKUnitState::Idle;
        MovementComponent->Stop();
        return;
    }
    if (DistanceTo2D(Target) > GetAttackRange())
    {
        CancelAttackWindup();
        State = ELKUnitState::Moving;
        MovementComponent->MoveToward(GetChaseDestination(Target), GetMoveSpeed());
    }
    else
    {
        State = ELKUnitState::Attacking;
        MovementComponent->Stop();
        TryAttack(DeltaSeconds);
    }
}

bool ALKUnitBase::ShouldReleaseTarget(const ALKUnitBase* Target) const
{
    if (!Target) { return true; }
    if (ActiveComponent->GetLockedTarget() == Target) { return false; }
    // 集火指令目标不受索敌范围限制（AI 战术可跨图）。
    if (ForcedTargetActor.Get() == Target) { return false; }
    const float Distance = DistanceTo2D(Target);
    if (Target->IsTaunting() && PassiveComponent->GetAbility() != ELKPassiveAbility::TauntImmunity)
    {
        // 嘲讽者只在离开嘲讽吸引半径（含滞回）后释放。
        const float TauntRadius = GameDataCached ? GameDataCached->TauntAcquireRadius : 500.f;
        return Distance > TauntRadius * 1.15f;
    }
    // 普通目标离开索敌范围（含滞回，避免边界抖动）后释放，等待重索敌。
    return Distance > AcquireRadius * 1.15f;
}

bool ALKUnitBase::CanPursueTarget(const ALKUnitBase* Target) const
{
    return Target && Target->IsTargetable() && Target->GetTeam() != Team
        && (!bTargetsBuildingsOnly || Target->IsBuilding())
        && (!IsBuilding() || DistanceTo2D(Target) <= GetAttackRange());
}

FVector ALKUnitBase::GetChaseDestination(const ALKUnitBase* Target) const
{
    const FVector ToSelf = (GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D();
    return Target->GetActorLocation() + ToSelf * FMath::Max(GetAttackRange() * 0.85f, GetBodyRadius() + Target->GetBodyRadius() + 2.f);
}

void ALKUnitBase::CancelAttackWindup() { bWindupActive = false; WindupTarget = nullptr; WindupRemaining = 0.f; }
void ALKUnitBase::ChangeTarget(AActor* NewTarget)
{
    if (TargetActor.Get() != NewTarget) { CancelAttackWindup(); }
    TargetActor = NewTarget;
}

void ALKUnitBase::SetTarget(AActor* NewTarget)
{
    if (ALKUnitBase* Locked = ActiveComponent->GetLockedTarget()) { ChangeTarget(Locked); return; }
    AActor* Taunter = FindNearestEnemy(true);
    ChangeTarget(Taunter ? Taunter : (CanPursueTarget(Cast<ALKUnitBase>(NewTarget)) ? NewTarget : nullptr));
}

void ALKUnitBase::AcquireTarget()
{
    if (ALKUnitBase* Locked = ActiveComponent->GetLockedTarget()) { ChangeTarget(Locked); return; }
    // 嘲讽始终高于集火、普通目标指定和最近目标。
    if (AActor* Taunter = FindNearestEnemy(true)) { ChangeTarget(Taunter); return; }
    if (ALKUnitBase* Forced = Cast<ALKUnitBase>(ForcedTargetActor.Get()))
    {
        if (CanPursueTarget(Forced)) { ChangeTarget(Forced); return; }
    }
    ChangeTarget(FindNearestEnemy());
}

void ALKUnitBase::SetForcedTarget(AActor* InTarget, float DurationSeconds)
{
    ALKUnitBase* Unit = Cast<ALKUnitBase>(InTarget);
    ForcedTargetActor = DurationSeconds > 0.f && CanPursueTarget(Unit) ? InTarget : nullptr;
    ForcedTargetRemaining = ForcedTargetActor.IsValid() ? DurationSeconds : 0.f;
    AcquireTarget();
}

void ALKUnitBase::TickForcedTarget(float DeltaSeconds)
{
    if (ForcedTargetRemaining <= 0.f) { return; }
    ForcedTargetRemaining -= DeltaSeconds;
    if (ForcedTargetRemaining <= 0.f || !ForcedTargetActor.IsValid())
    {
        ForcedTargetActor = nullptr;
        ForcedTargetRemaining = 0.f;
        ChangeTarget(nullptr);
        TargetRetryTimer = 0.f;
    }
}

AActor* ALKUnitBase::FindNearestEnemy(bool bTauntersOnly, bool bIgnoreAcquireRange) const
{
    if (bTauntersOnly && PassiveComponent->GetAbility() == ELKPassiveAbility::TauntImmunity) { return nullptr; }
    AActor* Best = nullptr;
    float BestDistance = TNumericLimits<float>::Max();
    if (!GetWorld()) { return nullptr; }
    for (TActorIterator<ALKUnitBase> It(GetWorld()); It; ++It)
    {
        ALKUnitBase* Other = *It;
        if (!CanPursueTarget(Other)) { continue; }
        const float Distance = DistanceTo2D(Other);
        if (bTauntersOnly)
        {
            // 嘲讽吸引半径单独配置（TauntAcquireRadius）。
            if (!Other->IsTaunting() || Distance > (GameDataCached ? GameDataCached->TauntAcquireRadius : 500.f)) { continue; }
        }
        else if (!bIgnoreAcquireRange && Distance > AcquireRadius)
        {
            // 索敌范围：范围内才锁定战斗；行军方向用 bIgnoreAcquireRange 取全图最近。
            continue;
        }
        if (Distance < BestDistance) { BestDistance = Distance; Best = Other; }
    }
    return Best;
}

void ALKUnitBase::TryAttack(float DeltaSeconds)
{
    if (IsControlled()) { CancelAttackWindup(); return; }
    if (AActor* Taunter = FindNearestEnemy(true)) { ChangeTarget(Taunter); }
    ALKUnitBase* Target = Cast<ALKUnitBase>(TargetActor.Get());
    if (!Target || !CanPursueTarget(Target) || DistanceTo2D(Target) > GetAttackRange() || IsManualMoving())
    {
        CancelAttackWindup(); return;
    }
    if (bWindupActive)
    {
        if (WindupTarget.Get() != Target) { CancelAttackWindup(); return; }
        WindupRemaining -= DeltaSeconds;
    }
    else
    {
        if (AttackCooldownRemaining > 0.f) { return; }
        bWindupActive = true;
        WindupTarget = Target;
        WindupRemaining = AttackWindup;
        AnimationComponent->Attack(AttackWindup);
        // Interval 明确为连续两次起手间隔；前摇包含其中，移动和视觉停顿不暂停它。
        AttackCooldownRemaining = FMath::Max(GetAttackInterval(), AttackWindup);
        if (WindupRemaining > 0.f) { return; }
    }
    if (WindupRemaining <= 0.f)
    {
        CancelAttackWindup();
        PerformAttack(Target);
    }
}

void ALKUnitBase::PerformAttack(AActor* Target)
{
    ALKUnitBase* Victim = Cast<ALKUnitBase>(Target);
    if (!CanPursueTarget(Victim) || IsControlled() || !IsAlive() || !IsCombatEnabled()) { return; }
    AttackPulseRemaining = .15f; HitStopRemaining = .06f;
    AnimationComponent->Impact();
    const bool bKingStrike = StatusComponent->BeginAttack();
    const float Damage = GetAttackDamage() * (bKingStrike ? 1.5f : 1.f);
    ALKBattleGameMode* GM = GetWorld()->GetAuthGameMode<ALKBattleGameMode>();
    if (GM) { GM->BeginCombatBatch(); }
    if (AttackType == ELKAttackType::Ranged)
    {
        const FVector Muzzle = GetActorLocation() + FVector(0, 0, 20);
        const FVector Direction = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
        ELKBreathHead Head = ELKBreathHead::None;
        if (PassiveComponent->GetAbility() == ELKPassiveAbility::TwinBreath)
        {
            const bool bIce = GM ? GM->GetBattleRandom().RandRange(0, 1) == 0 : FMath::RandBool();
            Head = bIce ? ELKBreathHead::Ice : ELKBreathHead::Fire;
        }
        ALKProjectile* Projectile = GM ? GM->AcquireProjectile(Muzzle, Damage, Team, this, Direction)
            : GetWorld()->SpawnActor<ALKProjectile>(ALKProjectile::StaticClass(), Muzzle, FRotator::ZeroRotator);
        if (Projectile)
        {
            if (!GM) { Projectile->Init(Damage, Team, this, Direction); }
            Projectile->SetAttackPayload(Head, bTargetsBuildingsOnly ? Victim : nullptr);
        }
        const FName Shot = Head == ELKBreathHead::Ice ? FName("IceBreath") : Head == ELKBreathHead::Fire ? FName("FireBreath")
            : bTargetsBuildingsOnly ? FName("SiegeShot") : UnitId == "Unit_TrollSpearman" ? FName("SpearShot")
            : (bIsMage || UnitId == "Unit_ElfPriest" || UnitId == "Unit_ApprenticeMage" || UnitId == "Hero_Necromancer") ? FName("MagicShot") : FName("BowShot");
        ULKPresentationSubsystem::Sound(GetWorld(), Shot, GetActorLocation());
    }
    else
    {
        const FLKCombatSource Source = LKGameplay::MakeSource(this, bKingStrike ? ELKCombatSourceKind::Skill : ELKCombatSourceKind::Attack,
            bKingStrike ? FName("Skill_KillThem") : FName("MeleeAttack"));
        LKGameplay::ApplyDamage(Victim, Damage, this, false, &Source);
        if (bKingStrike && Victim->IsAlive())
        {
            Victim->CancelAttackWindup();
            const FVector Away = (Victim->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
            Victim->GetMovementComponent()->MoveSkillDelta(Away * 300.f);
            Victim->GetStatusComponent()->Stun(2.f);
        }
        ULKPresentationSubsystem::Emit(GetWorld(), bKingStrike || UnitId == "Unit_Colossus" ? ELKVisualCue::HeavyHit : ELKVisualCue::Slash, Target->GetActorLocation(), bKingStrike ? 140.f : 70.f);
        if (bKingStrike || UnitId == "Unit_Colossus") { ULKPresentationSubsystem::Sound(GetWorld(), "HeavyHit", Target->GetActorLocation()); }
        else { LKGameplay::PlayOneShot(GetWorld(), GameDataCached, TEXT("MeleeHit"), Target->GetActorLocation(), .8f); }
    }
    StatusComponent->AfterAttack();
    if (GM) { GM->EndCombatBatch(); }
}

float ALKUnitBase::DistanceTo2D(const AActor* Other) const
{
	if (!Other)
	{
		return TNumericLimits<float>::Max();
	}
	return FVector::Dist2D(GetActorLocation(), Other->GetActorLocation());
}

float ALKUnitBase::GetHealth() const
{
	return LKGameplay::GetAttributeValue(this, ULKUnitAttributeSet::GetHealthAttribute(), 1.f);
}

float ALKUnitBase::GetMaxHealth() const
{
	return LKGameplay::GetAttributeValue(this, ULKUnitAttributeSet::GetMaxHealthAttribute(), 1.f);
}

float ALKUnitBase::GetBaseMaxHealth() const
{
	return LKGameplay::GetAttributeBaseValue(this, ULKUnitAttributeSet::GetMaxHealthAttribute(), 1.f);
}

float ALKUnitBase::GetAttackDamage() const
{
	return LKGameplay::GetAttributeValue(this, ULKUnitAttributeSet::GetAttackDamageAttribute(), 0.f) * StatusComponent->AttackMultiplier();
}

float ALKUnitBase::GetAttackRange() const
{
    if (bTargetsBuildingsOnly)
    { return GameDataCached ? FVector2D(GameDataCached->FieldHalfWidth, GameDataCached->FieldHalfHeight).Size() * 2.f + 200.f : 100000.f; }
	return LKGameplay::GetAttributeValue(this, ULKUnitAttributeSet::GetAttackRangeAttribute(), 0.f);
}

float ALKUnitBase::GetAttackInterval() const
{
	return LKGameplay::GetAttributeValue(this, ULKUnitAttributeSet::GetAttackIntervalAttribute(), 1.f) * StatusComponent->IntervalMultiplier();
}

float ALKUnitBase::GetMoveSpeed() const
{
	return LKGameplay::GetAttributeValue(this, ULKUnitAttributeSet::GetMoveSpeedAttribute(), 0.f) * StatusComponent->MoveMultiplier();
}

void ALKUnitBase::SetInvulnerable(float DurationSeconds)
{
	if (DurationSeconds == 0.f)
	{
		bInvulnerable = false;
		InvulnerableRemaining = 0.f;
		UE_LOG(LogLKUnit, Log, TEXT("[Unit] %s 解除无敌"), *UnitId.ToString());
		return;
	}

	bInvulnerable = true;
	InvulnerableRemaining = (DurationSeconds > 0.f) ? DurationSeconds : -1.f;
	const FString DurationText = (DurationSeconds > 0.f)
		? FString::Printf(TEXT("%.1f 秒"), DurationSeconds)
		: FString(TEXT("永久"));
	UE_LOG(LogLKUnit, Log, TEXT("[Unit] %s 进入无敌（%s）"), *UnitId.ToString(), *DurationText);
}

float ALKUnitBase::GetTraitEffectValue(ELKTraitEffect Effect) const
{
    float Value = 0.f;
    for (const auto& Pair : ResolvedTraits)
    {
        if (Pair.Value.Effect == Effect && FMath::IsFinite(Pair.Value.EffectValue)) { Value += Pair.Value.EffectValue; }
    }
    return FMath::Clamp(Value, 0.f, 1.f);
}

void ALKUnitBase::RestoreHeroLife(float Health, bool bResumeCombat)
{
    AnimationComponent->ResetPresentation();
    bDead = false; State = ELKUnitState::Idle;
    DeathAnimRemaining = 0.f; SetLifeSpan(0.f);
    SpriteComponent->SetRelativeScale3D(BaseSpriteLocalScale);
    SpriteComponent->SetSpriteColor(FLinearColor::White);
    BodyCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ForcedTargetActor.Reset(); ForcedTargetRemaining = 0.f; FocusWarningRemaining = 0.f;
    SetCombatEnabled(bResumeCombat);
    if (bResumeCombat) { ApplyTraits(); }
    AbilitySystem->SetNumericAttributeBase(ULKUnitAttributeSet::GetHealthAttribute(), FMath::Clamp(Health, 0.f, GetMaxHealth()));
    OnHealthChanged(GetHealth(), GetMaxHealth());
}

bool ALKUnitBase::ReviveDuringBattle()
{
    const ALKBattleGameMode* GM = GetWorld()->GetAuthGameMode<ALKBattleGameMode>();
    if (!IsHero() || !bDead || !GM || GM->GetPhase() != ELKGamePhase::Battle) { return false; }
    RestoreHeroLife(GetMaxHealth(), true);
    return true;
}

void ALKUnitBase::RecoverAfterBattle(float Percent)
{
    const ALKBattleGameMode* GM = GetWorld()->GetAuthGameMode<ALKBattleGameMode>();
    if (!IsHero() || !GM || GM->GetPhase() != ELKGamePhase::Result) { return; }
    const float Recovered = LKRunRules::RecoveredHealth(bDead ? 0.f : GetHealth(), GetMaxHealth(), Percent);
    if (Recovered > 0.f) { RestoreHeroLife(Recovered, false); }
}

void ALKUnitBase::Die()
{
	if (bDead || IsCamp())
	{
		return;
	}

	ALKBattleGameMode* GM = GetWorld()->GetAuthGameMode<ALKBattleGameMode>();
	if (GM && GM->GetPhase() == ELKGamePhase::Result) { return; }
	if (GM) { GM->BeginCombatBatch(); }
	bDead = true;
    SetCombatEnabled(false);
    AbilitySystem->SetNumericAttributeBase(ULKUnitAttributeSet::GetHealthAttribute(), 0.f);
    AbilitySystem->CancelAllAbilities();
    CancelAttackWindup();
	State = ELKUnitState::Dead;
	MovementComponent->Stop();

	// 佣兵/建筑淡出后销毁；英雄保留失能实例，供战内被动复活和战后恢复。
	DeathAnimRemaining = DeathAnimDuration;
	SetLifeSpan(IsHero() ? 0.f : DeathAnimDuration);
	if (BodyCollision)
	{
		BodyCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// 光环主人死亡：立即收回对友军施加的光环效果（防止 buff 永久残留）
	if (TraitAuraComponent)
	{
		TraitAuraComponent->ResetAuras();
	}

	// 音效：单位死亡
	if (UWorld* World = GetWorld())
	{
		LKGameplay::PlayOneShot(World, GameDataCached, TEXT("UnitDied"), GetActorLocation(), 1.f);
        ULKPresentationSubsystem::Emit(World, ELKVisualCue::Death, GetActorLocation());
	}

    StatusComponent->Clear();
    ULKUnitStatusComponent::RefreshTeamSupport(GetWorld());
	OnUnitDied.Broadcast(this);

	UE_LOG(LogLKUnit, Log, TEXT("[Unit] %s (%s) %s"), *UnitId.ToString(), *GetName(), IsHero() ? TEXT("失能") : TEXT("死亡"));
	if (GM) { GM->EndCombatBatch(); }
}

void ALKUnitBase::DrawDebugShape() const
{
	if (!GameDataCached)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 无敌期用金色标识（普通绿/红之外一眼可辨）
	const FColor Color = bInvulnerable
		? FColor(255, 200, 0)
		: (Team == ELKTeam::Player) ? FColor::Green : FColor::Red;
	const bool bHasSprite = (SpriteComponent->GetSprite() != nullptr);

	// 1) 调试色块：仅"无精灵 + 开启调试形状"时显示（占位期可视化）
	if (!bHasSprite && GameDataCached->bDrawDebugShapes)
	{
		DrawDebugBox(World, GetActorLocation() + FVector(0.f, 0.f, 10.f),
			FVector(BodyRadius, BodyRadius, 10.f), Color, false, -1.f, 0, 2.f);
	}

	// 2) 脚下阵营色环：有精灵/无精灵都显示（精灵上线后的敌我标识，绿=玩家/红=敌方）
	//    无精灵时随 bDrawDebugShapes，有精灵时随 bDrawTeamRing
	const bool bShowRing = false; // 阵营环已由原生 HUD 绘制，保留调试框和索敌线。
	if (bShowRing)
	{
		const FVector RingCenter = GetActorLocation() + FVector(0.f, 0.f, 8.f);
		// 平面参数：YAxis=(1,0,0)、ZAxis=(0,1,0) → 圆环画在 XY 平面（贴地），否则默认是竖立的
		DrawDebugCircle(World, RingCenter, BodyRadius, 24, Color, false, -1.f, 0, 3.f,
			FVector(1.f, 0.f, 0.f), FVector(0.f, 1.f, 0.f), false);
	}

	// 3) 索敌线：单位 -> 当前目标（调试 AI 行为用，随调试形状开关）
	if (GameDataCached->bDrawDebugShapes && TargetActor.IsValid())
	{
		DrawDebugLine(World,
			GetActorLocation() + FVector(0.f, 0.f, 20.f),
			TargetActor->GetActorLocation() + FVector(0.f, 0.f, 20.f),
			Color, false, -1.f, 0, 1.f);
	}

	// 4) 集火线：单位 -> 强制目标（青色；AI 集火窗口可视化，随调试形状开关）
	if (GameDataCached->bDrawDebugShapes && ForcedTargetActor.IsValid())
	{
		DrawDebugLine(World,
			GetActorLocation() + FVector(0.f, 0.f, 24.f),
			ForcedTargetActor->GetActorLocation() + FVector(0.f, 0.f, 24.f),
			FColor::Cyan, false, -1.f, 0, 2.f);
	}
}

void ALKUnitBase::SetCombatEnabled(bool bEnabled)
{
    if (!bEnabled && ActiveComponent) { ActiveComponent->Stop(); }
    if (!bEnabled && StatusComponent) { StatusComponent->Clear(); }
    bCombatEnabled = bEnabled;
    if (!bEnabled)
    {
        MovementComponent->Stop();
        CancelAttackWindup();
        ChangeTarget(nullptr);
        AbilitySystem->CancelAllAbilities();
    }
}
