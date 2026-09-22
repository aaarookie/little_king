#include "ALKHomeGameMode.h"
#include "ULKJourneyPresentationSubsystem.h"
#include "LKCardPresentation.h"

#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/DataTable.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/StaticMeshComponent.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"

#include "ALKHomeBuildingActor.h"
#include "ALKHomePlayerController.h"
#include "LKEncounterContent.h"
#include "LKHomeContent.h"
#include "LKLog.h"
#include "LKUnitContent.h"
#include "ULKCardDefinition.h"
#include "ULKGameData.h"
#include "ULKHomeHUDWidget.h"
#include "ULKProfileSubsystem.h"
#include "ULKRunSubsystem.h"
#include "ULKSaveSlotSubsystem.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"

namespace
{
	const TCHAR* AttackTypeText(ELKAttackType Type)
	{
		return Type == ELKAttackType::Melee ? TEXT("近战") : TEXT("远程");
	}

	FText DescribeTrait(FName TraitId)
	{
		if (TraitId == "Trait_MageSpellReach") { return FText::FromString(TEXT("全场施法：本方所有法术可在战场任意位置落点")); }
		if (TraitId == "Trait_KnightTauntAura") { return FText::FromString(TEXT("嘲讽光环：半径内己方近战佣兵获得嘲讽")); }
		if (TraitId == "Taunt") { return FText::FromString(TEXT("嘲讽：敌方索敌优先攻击本单位")); }
		if (TraitId == "Trait_Sacrifice") { return FText::FromString(TEXT("献祭：每次亡灵召唤消耗自身 8% 最大生命")); }
		if (TraitId == "Trait_FaceFear") { return FText::FromString(TEXT("直面恐惧：受到远程伤害减少 30%")); }
		return FText::FromString(FString::Printf(TEXT("%s（暂无说明）"), *TraitId.ToString()));
	}
}

ALKHomeGameMode::ALKHomeGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = nullptr;
	PlayerControllerClass = ALKHomePlayerController::StaticClass();
	GameStateClass = AGameStateBase::StaticClass();
	PlayerStateClass = APlayerState::StaticClass();
	HUDClass = nullptr; // 家园界面是原生 UMG Widget，不是 AHUD
}

void ALKHomeGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (ULKSaveSlotSubsystem::RouteInitialPlayToMenu(GetWorld())) { SetActorTickEnabled(false); return; }
#if WITH_EDITOR
	// Explicit editor-only visual capture uses transient progress and separate save names.
	bPreviewMode = FParse::Param(FCommandLine::Get(), TEXT("HomeUIPreview"));
	if (bPreviewMode)
	{
		ULKProfileSubsystem::SetSlotNameOverrideForTest(TEXT("LittleKing_HomeUIPreview_") + FGuid::NewGuid().ToString(EGuidFormats::Digits));
		ULKProfileSubsystem::SetPersistentProfileEnabledForTest(true);
		ULKRunSubsystem::SetRunSlotNameOverrideForTest(TEXT("LittleKing_HomeUIPreview_Run"));
		if (ULKRunSubsystem* Run = GetRunSubsystem()) { Run->SetAutoSaveEnabled(false); }
	}
#endif
	EnsureGameData();
	SpawnPlaceholderScene();

	if (ULKProfileSubsystem* Profile = GetProfileSubsystem())
	{
		Profile->EnsureProfile();
#if WITH_EDITOR
		if (bPreviewMode) { Profile->AddGold(300); }
#endif
		SelectedRegionId = Profile->GetLastSelectedRegionId();
	}
	if (SelectedRegionId.IsNone()) { SelectedRegionId = LKHomeContent::DefaultRegionId(); }
	if (ULKRunSubsystem* Run = GetRunSubsystem(); Run && !Run->HasRun() && Run->HasSavedExpedition())
	{
		Run->LoadExpedition(); // Load before showing Continue/settlement; never create a replacement here.
	}

	// H4：远征终态金币入账（幂等；已入账的同一 SettlementId 不会重复加钱）。
	ApplyPendingSettlement();
	if (ULKRunSubsystem* Run = GetRunSubsystem(); Run && (Run->HasRun() || !Run->HasSavedExpedition())) { Run->ReconcileDepartureFunding(); }
	RefreshBuildingLevels();
	TryCreateHomeHUD();

	UE_LOG(LogLK, Log, TEXT("[Home] 家园就绪：金币 %d，建筑 %d 座，远征状态 %s"),
		GetGold(), BuildingActors.Num(), *GetExpeditionStatusText().ToString());
}

void ALKHomeGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bHUDCreateAttempted) { TryCreateHomeHUD(); }
#if WITH_EDITOR
	if (bPreviewMode && HomeHUD)
	{
		++PreviewFrame;
		const TArray<ELKHomePanel> Panels = {ELKHomePanel::None, ELKHomePanel::Statue, ELKHomePanel::Library,
			ELKHomePanel::HeroHouse, ELKHomePanel::Treasury, ELKHomePanel::Barracks, ELKHomePanel::WarRoom, ELKHomePanel::Gate};
		const int32 Index = (PreviewFrame - 120) / 90;
		if (PreviewFrame == 265 && FParse::Param(FCommandLine::Get(), TEXT("WorldArtHomePreview")))
		{
			// Exercise the real upgrade button in the existing isolated preview profile.
			UPanelWidget* Actions = Cast<UPanelWidget>(HomeHUD->GetWidgetFromName(TEXT("ActionsBox")));
			UUserWidget* Entry = Actions ? Cast<UUserWidget>(Actions->GetChildAt(0)) : nullptr;
			UButton* Upgrade = Entry ? Cast<UButton>(Entry->GetWidgetFromName(TEXT("RowButton"))) : nullptr;
			if (Upgrade && Upgrade->GetIsEnabled()) { Upgrade->OnClicked.Broadcast(); }
		}
		if (PreviewFrame >= 120 && Index < Panels.Num())
		{
			if ((PreviewFrame - 120) % 90 == 0)
			{
				HomeHUD->RequestClosePanel();
				if (Panels[Index] != ELKHomePanel::None) { HomeHUD->OpenPanel(Panels[Index]); }
			}
			if ((PreviewFrame - 120) % 90 == 60)
			{
				FVector Eye; FRotator Rotation;
				GetWorld()->GetFirstPlayerController()->GetPlayerViewPoint(Eye, Rotation);
				UE_LOG(LogLK, Log, TEXT("[HomePreview] View %s / %s"), *Eye.ToString(), *Rotation.ToString());
				int32 Width = 0, Height = 0;
				GetWorld()->GetFirstPlayerController()->GetViewportSize(Width, Height);
				const FString Dir = FPaths::ProjectSavedDir() / TEXT("Screenshots/HomeUI");
				IFileManager::Get().MakeDirectory(*Dir, true);
				FScreenshotRequest::RequestScreenshot(Dir / FString::Printf(TEXT("%dx%d-%02d.png"), Width, Height, Index), true, false);
			}
		}
		if (PreviewFrame > 120 + Panels.Num() * 90)
		{
			UGameplayStatics::DeleteGameInSlot(ULKProfileSubsystem::GetSlotNameA(), 0);
			UGameplayStatics::DeleteGameInSlot(ULKProfileSubsystem::GetSlotNameB(), 0);
			FPlatformMisc::RequestExit(false);
		}
	}
#endif
}

void ALKHomeGameMode::EnsureGameData()
{
	if (GameData && GameData->GetOuter() != this) { GameData = DuplicateObject<ULKGameData>(GameData, this); }
	if (!GameData)
	{
		GameData = NewObject<ULKGameData>(this, TEXT("DA_GameData_HomeRuntime"));
		UE_LOG(LogLK, Log, TEXT("[Home] 未配置 DA_GameData，已创建运行时默认配置"));
	}
	GameData->EnsureDefaultDecks();
	// H2：收藏页（图书馆/英雄之家/军营）需要可解析的卡牌目录，与战斗共用同一份注入逻辑。
	GameData->EnsureCardLibrary();
	ResolvedHomeUnits = LKUnitContent::Units();
	if (UDataTable* Table = GameData->UnitTable.LoadSynchronous())
	{
		for (FName Id : Table->GetRowNames())
		{
			if (const FLKUnitRow* Authored = Table->FindRow<FLKUnitRow>(Id, TEXT("HomeContent")))
			{
				const FLKUnitRow* Canonical = LKUnitContent::Find(Id);
				ResolvedHomeUnits.Add(Id, Canonical ? LKUnitContent::MergeAuthoredTuning(*Canonical, Authored) : *Authored);
			}
		}
	}
}

ULKProfileSubsystem* ALKHomeGameMode::GetProfileSubsystem() const
{
	const UGameInstance* Instance = GetGameInstance();
	return Instance ? Instance->GetSubsystem<ULKProfileSubsystem>() : nullptr;
}

ULKRunSubsystem* ALKHomeGameMode::GetRunSubsystem() const
{
	const UGameInstance* Instance = GetGameInstance();
	return Instance ? Instance->GetSubsystem<ULKRunSubsystem>() : nullptr;
}

ULKCardDefinition* ALKHomeGameMode::FindCard(FName CardId) const
{
	if (!GameData || CardId.IsNone()) { return nullptr; }
	for (ULKCardDefinition* Card : GameData->CardLibrary)
	{
		if (Card && Card->CardId == CardId) { return Card; }
	}
	return nullptr;
}

const FLKUnitRow* ALKHomeGameMode::GetUnitRow(FName UnitId) const
{
	return ResolvedHomeUnits.Find(UnitId);
}

int32 ALKHomeGameMode::GetGold() const
{
	const ULKProfileSubsystem* Profile = GetProfileSubsystem();
	return Profile ? Profile->GetGold() : 0;
}

int32 ALKHomeGameMode::GetBuildingLevel(FName BuildingId) const
{
	const ULKProfileSubsystem* Profile = GetProfileSubsystem();
	return Profile ? Profile->GetBuildingLevel(BuildingId) : 1;
}

bool ALKHomeGameMode::IsProfileUsable() const
{
	const ULKProfileSubsystem* Profile = GetProfileSubsystem();
	return Profile && Profile->HasProfile() && Profile->IsProfileUsable();
}

FText ALKHomeGameMode::GetProfileStatusText() const
{
	const ULKProfileSubsystem* Profile = GetProfileSubsystem();
	if (!Profile) { return FText::FromString(TEXT("永久档不可用")); }
	if (!Profile->HasProfile()) { return FText::FromString(TEXT("永久档未创建")); }
	if (!Profile->IsProfileUsable()) { return FText::FromString(TEXT("永久档写入不可用：升级与入账已禁用")); }
	return FText::GetEmpty();
}

FText ALKHomeGameMode::GetExpeditionStatusText() const
{
	const ULKRunSubsystem* Run = GetRunSubsystem();
	if (!Run) { return FText::FromString(TEXT("远征状态未知")); }
	if (Run->HasRunInProgress())
	{
		const FLKRunState State = Run->GetRunState();
		const FLKRegionDefinition* Region = LKHomeContent::FindRegion(State.RegionId);
		const FLKWorldRegion* WorldRegion = State.WorldRegions.FindByPredicate([&](const FLKWorldRegion& Item) { return Item.RegionId == State.RegionId; });
		const FString Name = WorldRegion ? WorldRegion->DisplayName.ToString() : Region ? Region->DisplayName.ToString() : TEXT("远征路线");
		return FText::FromString(FString::Printf(TEXT("远征进行中：%s · 已战 %d 场 · 钱包 %d 金币"),
			*Name, State.BattleHistory.Num(), State.WalletGold));
	}
	if (Run->HasRun())
	{
		const FLKRunState State = Run->GetRunState();
		if (State.Phase == ELKRunPhase::Completed) { return FText::FromString(TEXT("上次远征：已通关")); }
		if (State.Phase == ELKRunPhase::Failed) { return FText::FromString(TEXT("上次远征：失败")); }
		if (State.Phase == ELKRunPhase::Abandoned) { return FText::FromString(TEXT("上次远征：已放弃")); }
	}
	if (Run->HasSavedExpedition()) { return FText::FromString(TEXT("有可继续的远征存档")); }
	return FText::FromString(TEXT("暂无远征，可从大门出征"));
}

bool ALKHomeGameMode::HasActiveExpedition() const
{
	const ULKRunSubsystem* Run = GetRunSubsystem();
	return Run && Run->HasRunInProgress();
}

bool ALKHomeGameMode::CanContinueExpedition() const
{
	const ULKRunSubsystem* Run = GetRunSubsystem();
	if (!Run) { return false; }
	return Run->HasRunInProgress() || (!Run->HasRun() && Run->HasSavedExpedition());
}

FName ALKHomeGameMode::GetSelectedRegionId() const
{
	return SelectedRegionId.IsNone() ? LKHomeContent::DefaultRegionId() : SelectedRegionId;
}

void ALKHomeGameMode::SelectRegion(FName RegionId)
{
	if (!LKHomeContent::FindRegion(RegionId)) { return; }
	SelectedRegionId = RegionId;
	if (ULKProfileSubsystem* Profile = GetProfileSubsystem()) { Profile->SetLastSelectedRegionId(RegionId); }
}

FVector ALKHomeGameMode::GetBuildingLayoutLocation(ELKHomeBuilding Building)
{
	// 俯视相机（Pitch=-90）下：世界 +X 在屏幕上向上，世界 +Y 向右。
	switch (Building)
	{
	case ELKHomeBuilding::Statue:		return FVector(700.f, 0.f, 0.f);
	case ELKHomeBuilding::Library:		return FVector(300.f, -700.f, 0.f);
	case ELKHomeBuilding::HeroHouse:	return FVector(300.f, 700.f, 0.f);
	case ELKHomeBuilding::Treasury:		return FVector(-300.f, -700.f, 0.f);
	case ELKHomeBuilding::Barracks:		return FVector(-300.f, 700.f, 0.f);
	case ELKHomeBuilding::WarRoom:		return FVector(-700.f, -300.f, 0.f);
	case ELKHomeBuilding::Gate:			return FVector(-700.f, 300.f, 0.f);
	default:							return FVector::ZeroVector;
	}
}

void ALKHomeGameMode::SpawnPlaceholderScene()
{
	UWorld* World = GetWorld();
	if (!World) { return; }

	// Recolour only the project's known placeholder ground/path materials, at runtime.
	// Authored level layout, arbitrary meshes and shared material assets remain intact.
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		UStaticMeshComponent* Mesh = It->GetStaticMeshComponent();
		UMaterialInterface* Existing = Mesh ? Mesh->GetMaterial(0) : nullptr;
		if (!Existing || !Existing->GetPathName().StartsWith(TEXT("/Game/Materials/Home/"))) { continue; }
		const bool bGround = Existing->GetName() == TEXT("M_HomePlaceholder");
		const bool bPath = Existing->GetName().StartsWith(TEXT("MI_Home"));
		if (!bGround && !bPath) { continue; }
		UMaterialInstanceDynamic* Material = Mesh->CreateDynamicMaterialInstance(0, Existing);
		if (Material) { Material->SetVectorParameterValue(TEXT("Color"), FLinearColor::FromSRGBColor(
			bGround ? FColor(99, 112, 76) : FColor(174, 160, 122))); }
	}

	// 已有手工摆放的建筑 Actor：尊重它们，只为缺失的 BuildingId 补占位。
	for (TActorIterator<ALKHomeBuildingActor> It(World); It; ++It)
	{
		BuildingActors.AddUnique(*It);
	}

	if (bSpawnPlaceholderGround && !GroundActor)
	{
		if (UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")))
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AStaticMeshActor* Ground = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),
				FVector(0.f, 0.f, -20.f), FRotator::ZeroRotator, Params);
			if (Ground)
			{
				Ground->GetStaticMeshComponent()->SetStaticMesh(PlaneMesh);
				Ground->GetStaticMeshComponent()->SetWorldScale3D(FVector(30.f, 30.f, 1.f));
				Ground->GetStaticMeshComponent()->SetMobility(EComponentMobility::Static);
				Ground->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				GroundActor = Ground;
			}
		}
	}

	if (!bSpawnPlaceholderBuildings) { return; }

	for (const FLKBuildingDefinition& Definition : LKHomeContent::Buildings())
	{
		if (FindBuildingActor(Definition.BuildingId)) { continue; }
		const ELKHomeBuilding Building = LKHomeContent::BuildingFromId(Definition.BuildingId);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ALKHomeBuildingActor* Actor = World->SpawnActor<ALKHomeBuildingActor>(ALKHomeBuildingActor::StaticClass(),
			GetBuildingLayoutLocation(Building), FRotator::ZeroRotator, Params);
		if (!Actor) { continue; }
		Actor->InitializeBuilding(Definition.BuildingId, Definition.DisplayName, Definition.bUpgradable);
		BuildingActors.Add(Actor);
	}
}

ALKHomeBuildingActor* ALKHomeGameMode::FindBuildingActor(FName BuildingId) const
{
	for (ALKHomeBuildingActor* Actor : BuildingActors)
	{
		if (Actor && Actor->GetBuildingId() == BuildingId) { return Actor; }
	}
	return nullptr;
}

void ALKHomeGameMode::RefreshBuildingLevels()
{
	for (ALKHomeBuildingActor* Actor : BuildingActors)
	{
		if (!Actor) { continue; }
		Actor->SetDisplayedLevel(GetBuildingLevel(Actor->GetBuildingId()));
	}
}

void ALKHomeGameMode::TryCreateHomeHUD()
{
	if (HomeHUD || bHUDCreateAttempted) { return; }
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC) { return; }
	bHUDCreateAttempted = true;

	const TSubclassOf<ULKHomeHUDWidget> WidgetClass = HUDWidgetClass
		? HUDWidgetClass
		: TSubclassOf<ULKHomeHUDWidget>(ULKHomeHUDWidget::StaticClass());
	HomeHUD = CreateWidget<ULKHomeHUDWidget>(GetWorld(), WidgetClass);
	if (HomeHUD)
	{
		HomeHUD->InitializeHomeHUD(this);
		HomeHUD->AddToViewport(0);
		UE_LOG(LogLK, Log, TEXT("[Home] 家园 HUD 已创建：%s"), *WidgetClass->GetName());
	}
	else
	{
		UE_LOG(LogLK, Warning, TEXT("[Home] 家园 HUD 创建失败：%s"), *WidgetClass->GetName());
	}
}

// ---------------- 面板数据 ----------------

FText ALKHomeGameMode::BuildBuildingDetail(FName BuildingId) const
{
	const FLKBuildingDefinition* Definition = LKHomeContent::FindBuilding(BuildingId);
	if (!Definition) { return FText::FromString(TEXT("未知建筑")); }
	return Definition->Description;
}

FText ALKHomeGameMode::BuildHeroDetail(FName HeroId) const
{
	const FLKUnitRow* Row = GetUnitRow(HeroId);
	if (!Row) { return FText::FromString(FString::Printf(TEXT("缺少单位行：%s"), *HeroId.ToString())); }

	FString Text = LKCardPresentation::QualityName(Row->Quality).ToString() + TEXT(" · ") + LKCardPresentation::RaceName(Row->Race).ToString() + TEXT("\n");
    Text += FString::Printf(TEXT("生命 %.0f · 攻击 %.0f · 射程 %.0f · 间隔 %.1fs · 移速 %.0f · %s\n"),
		Row->BaseHealth, Row->AttackDamage, Row->AttackRange, Row->AttackInterval, Row->MoveSpeed, AttackTypeText(Row->AttackType));

	TArray<FName> Traits = Row->HeroTraits;
	if (GameData)
	{
		if (const FLKHeroTraitEntry* Entry = GameData->DefaultHeroTraits.Find(HeroId))
		{
			for (FName Trait : Entry->Traits) { Traits.AddUnique(Trait); }
		}
	}
	Text += Traits.IsEmpty() ? TEXT("特性：无\n") : FString::Printf(TEXT("特性：%s\n"), *FString::JoinBy(Traits, TEXT("、"),
		[](const FName& Trait) { return DescribeTrait(Trait).ToString(); }));

	if (GameData)
	{
		if (const FLKHeroSkillEntry* Skills = GameData->HeroAbilityMap.Find(HeroId))
		{
			const TCHAR* Skill = HeroId == "Hero_Knight" ? TEXT("治疗鼓舞：治疗身边友军")
				: HeroId == "Hero_Mage" ? TEXT("火球：对目标附近敌人造成范围伤害")
				: HeroId == "Hero_Ranger" ? TEXT("箭雨：对目标附近敌人造成范围伤害") : TEXT("主动技能");
			Text += Skills->Abilities.IsEmpty() ? TEXT("技能：未配置")
				: FString::Printf(TEXT("技能：%s（冷却 %.1fs）"), Skill, Row->SkillCooldown > 0.f ? Row->SkillCooldown : 5.f);
		}
		else
		{
			Text += TEXT("技能：未配置");
		}
	}
	return FText::FromString(Text);
}

FText ALKHomeGameMode::BuildCardDetail(FName CardId) const
{
	const ULKCardDefinition* Card = FindCard(CardId);
	if (!Card) { return FText::FromString(FString::Printf(TEXT("缺少卡牌定义：%s（不在 DA_GameData.CardLibrary）"), *CardId.ToString())); }

	FString Text = FString::Printf(TEXT("%s · 费用 %d · %s"), *Card->CardName.ToString(), Card->Cost,
		Card->CardType == ELKCardType::Spell ? TEXT("法术") : (Card->CardType == ELKCardType::Building ? TEXT("战斗建筑") : TEXT("佣兵")));
    Text += TEXT(" · ") + LKCardPresentation::Classification(*Card).ToString();
    if (!Card->Description.IsEmpty()) { Text += TEXT("\n") + Card->Description.ToString(); }

	if (Card->CardType == ELKCardType::Spell)
	{
		const TCHAR* Effect = Card->SpellEffect == ELKSpellEffect::Heal ? TEXT("范围治疗") : TEXT("范围伤害");
		Text += FString::Printf(TEXT("\n%s %.0f · 半径 %.0f\n落点规则：己方半场；有全场施法特性时可全场落点"),
			Effect, Card->SpellValue, Card->SpellRadius);
	}
	else
	{
		const FName UnitId = Card->CardType == ELKCardType::Building ? Card->BuildingUnitId : Card->SpawnUnitId;
		if (const FLKUnitRow* Row = GetUnitRow(UnitId))
		{
			Text += FString::Printf(TEXT("\n生成 %s：生命 %.0f · 攻击 %.0f · 射程 %.0f · %s"),
				*Row->DisplayName.ToString(), Row->BaseHealth, Row->AttackDamage, Row->AttackRange, AttackTypeText(Row->AttackType));
			if (Row->BuildingBehavior == ELKBuildingBehavior::Barracks)
			{
				Text += FString::Printf(TEXT("\n兵营：每 %.1f 秒产出 %s"), Row->SpawnInterval, *Row->SpawnUnitId.ToString());
			}
			else if (Row->BuildingBehavior == ELKBuildingBehavior::Turret)
			{
				Text += TEXT("\n哨塔：只在射程内选目标");
			}
		}
		else
		{
			Text += FString::Printf(TEXT("\n单位定义缺失：%s"), *UnitId.ToString());
		}
	}
	return FText::FromString(Text);
}

FLKHomePanelModel ALKHomeGameMode::BuildUpgradePanel(ELKHomePanel Panel) const
{
	FLKHomePanelModel Model;
	Model.Panel = Panel;
	Model.Title = LKHomeContent::PanelTitle(Panel);

	const ELKHomeBuilding Building = Panel == ELKHomePanel::Statue ? ELKHomeBuilding::Statue : ELKHomeBuilding::Treasury;
	const FName BuildingId = LKHomeContent::BuildingId(Building);
	const FLKBuildingDefinition* Definition = LKHomeContent::FindBuilding(BuildingId);
	if (!Definition) { Model.Status = FText::FromString(TEXT("缺少建筑定义")); return Model; }

	const int32 Level = GetBuildingLevel(BuildingId);
	const bool bMaxLevel = Level >= Definition->MaxLevel;
	const int32 Cost = LKHomeContent::UpgradeCost(BuildingId, Level);
	const int32 Gold = GetGold();

	auto AddRow = [&Model](const FString& Label, const FString& Value)
	{
		FLKHomePanelRow Row;
		Row.Label = FText::FromString(Label);
		Row.Value = FText::FromString(Value);
		Model.Rows.Add(Row);
	};

	AddRow(TEXT("当前等级"), FString::Printf(TEXT("Lv%d / Lv%d"), Level, Definition->MaxLevel));
	if (Building == ELKHomeBuilding::Statue)
	{
		AddRow(TEXT("当前战后恢复"), FString::Printf(TEXT("%d%% 最大生命（失能者恢复到同比例）"), FMath::RoundToInt(LKHomeContent::StatueRecoveryPercent(Level) * 100.f)));
		if (!bMaxLevel)
		{
			AddRow(TEXT("升级后恢复"), FString::Printf(TEXT("%d%% 最大生命"), FMath::RoundToInt(LKHomeContent::StatueRecoveryPercent(Level + 1) * 100.f)));
		}
		AddRow(TEXT("作用范围"), TEXT("每场战斗结束后结算一次；只影响玩家英雄，敌人与佣兵不受影响"));
		AddRow(TEXT("不生效处"), TEXT("治疗波/骑士技能/休息节点 30% 恢复不叠加神像效果"));
	}
	else
	{
		const float BaseSpeed = GameData ? GameData->SilverPerSecond : (1.f / 3.f);
		const float BaseCap = GameData ? GameData->SilverCap : 5.f;
		AddRow(TEXT("当前银币产速"), FString::Printf(TEXT("%.3f / 秒（基础的 %d%%）"), BaseSpeed * LKHomeContent::TreasurySpeedMultiplier(Level), FMath::RoundToInt(LKHomeContent::TreasurySpeedMultiplier(Level) * 100.f)));
		AddRow(TEXT("当前银币上限"), FString::Printf(TEXT("%.0f"), BaseCap + float(LKHomeContent::TreasuryCapBonus(Level))));
		AddRow(TEXT("出征携带金币上限"), FString::Printf(TEXT("%d 金币（途中获得的金币不受此限制）"), LKHomeContent::TreasuryDepartureGoldCap(Level)));
		if (!bMaxLevel)
		{
			AddRow(TEXT("升级后产速"), FString::Printf(TEXT("%.3f / 秒（基础的 %d%%）"), BaseSpeed * LKHomeContent::TreasurySpeedMultiplier(Level + 1), FMath::RoundToInt(LKHomeContent::TreasurySpeedMultiplier(Level + 1) * 100.f)));
			AddRow(TEXT("升级后上限"), FString::Printf(TEXT("%.0f"), BaseCap + float(LKHomeContent::TreasuryCapBonus(Level + 1))));
			AddRow(TEXT("升级后携带金币"), FString::Printf(TEXT("%d 金币"), LKHomeContent::TreasuryDepartureGoldCap(Level + 1)));
		}
		AddRow(TEXT("作用范围"), TEXT("只影响玩家每场战斗的起始经济；敌方经济由遭遇配置决定"));
	}

	if (bMaxLevel)
	{
		Model.Status = FText::FromString(TEXT("已满级"));
	}
	else
	{
		AddRow(TEXT("升级费用"), FString::Printf(TEXT("%d 金币"), Cost));
		AddRow(TEXT("当前金币"), FString::Printf(TEXT("%d"), Gold));
		const bool bAffordable = Cost >= 0 && Gold >= Cost;
		Model.Status = bAffordable
			? FText::FromString(TEXT("升级后保存；下次新远征生效"))
			: FText::FromString(FString::Printf(TEXT("金币不足，还差 %d"), FMath::Max(0, Cost - Gold)));
		FLKHomePanelAction Action;
		Action.Label = FText::FromString(FString::Printf(TEXT("升级到 Lv%d（%d 金币）"), Level + 1, FMath::Max(0, Cost)));
		Action.ActionId = TEXT("Upgrade");
		Action.bEnabled = bAffordable && IsProfileUsable();
		Model.Actions.Add(Action);
	}

	FLKHomePanelAction Close;
	Close.Label = FText::FromString(TEXT("关闭"));
	Close.ActionId = TEXT("Close");
	Model.Actions.Add(Close);
	Model.Subtitle = BuildBuildingDetail(BuildingId);
	return Model;
}

FLKHomePanelModel ALKHomeGameMode::BuildCollectionPanel(ELKHomePanel Panel, FName SelectedRowId) const
{
	FLKHomePanelModel Model;
	Model.Panel = Panel;
	Model.Title = LKHomeContent::PanelTitle(Panel);

	const ULKProfileSubsystem* Profile = GetProfileSubsystem();
	TArray<FName> Unlocked;
	if (Profile && Profile->HasProfile())
	{
		Unlocked = Panel == ELKHomePanel::HeroHouse ? Profile->GetProfile().UnlockedHeroIds : Profile->GetProfile().UnlockedCardIds;
	}
	if (Unlocked.IsEmpty())
	{
		Unlocked = Panel == ELKHomePanel::HeroHouse ? LKHomeContent::DefaultUnlockedHeroes() : LKHomeContent::DefaultUnlockedCards();
	}

	for (FName Id : Unlocked)
	{
		FLKHomePanelRow Row;
		Row.RowId = Id;
		Row.bSelectable = true;
		Row.bEnabled = true;
		if (Panel == ELKHomePanel::HeroHouse)
		{
			const FLKUnitRow* Unit = GetUnitRow(Id);
			Row.Label = Unit ? Unit->DisplayName : FText::FromName(Id);
			Row.Value = Unit ? FText::FromString(FString::Printf(TEXT("生命 %.0f · 攻击 %.0f · %s"), Unit->BaseHealth, Unit->AttackDamage, AttackTypeText(Unit->AttackType)))
				: FText::FromString(TEXT("定义缺失"));
			Row.Detail = BuildHeroDetail(Id);
		}
		else
		{
			const ULKCardDefinition* Card = FindCard(Id);
			// 图书馆只列法术：从永久解锁集里按卡牌类型筛选，不从手牌/临时牌组反推。
			if (Panel == ELKHomePanel::Library && (!Card || Card->CardType != ELKCardType::Spell)) { continue; }
			Row.Label = Card ? LKCardPresentation::Label(*Card) : FText::FromName(Id);
			Row.Value = Card ? FText::FromString(FString::Printf(TEXT("费用 %d · %s"), Card->Cost,
				Card->CardType == ELKCardType::Spell ? TEXT("法术") : (Card->CardType == ELKCardType::Building ? TEXT("战斗建筑") : TEXT("佣兵"))))
				: FText::FromString(TEXT("定义缺失"));
			Row.Detail = BuildCardDetail(Id);
		}
		Model.Rows.Add(Row);
	}

	if (Panel == ELKHomePanel::Library)
	{
		Model.Subtitle = FText::FromString(TEXT("永久解锁的法术；配牌请到战备处"));
		Model.Status = FText::FromString(TEXT("点击法术查看效果；前往战备处调整出征牌组"));
	}
	else
	{
		Model.Subtitle = FText::FromString(TEXT("查看已解锁英雄的属性、技能与特性"));
		Model.Status = FText::FromString(TEXT("前往战备处配置出征队伍"));
	}

	const FLKHomePanelRow* Selected = Model.Rows.FindByPredicate(
		[SelectedRowId](const FLKHomePanelRow& Row) { return Row.RowId == SelectedRowId; });
	if (Selected && !Selected->Detail.IsEmpty()) { Model.Status = Selected->Detail; }

	FLKHomePanelAction Close;
	Close.Label = FText::FromString(TEXT("关闭"));
	Close.ActionId = TEXT("Close");
	Model.Actions.Add(Close);
	return Model;
}

FLKHomePanelModel ALKHomeGameMode::BuildBarracksPanel(FName SelectedRowId, int32 TabIndex) const
{
	FLKHomePanelModel Model;
	Model.Panel = ELKHomePanel::Barracks;
	Model.Title = LKHomeContent::PanelTitle(ELKHomePanel::Barracks);
	Model.Subtitle = FText::FromString(TEXT("查看已解锁的佣兵与战斗建筑"));

	const ULKProfileSubsystem* Profile = GetProfileSubsystem();
	TArray<FName> Unlocked = (Profile && Profile->HasProfile()) ? Profile->GetProfile().UnlockedCardIds : LKHomeContent::DefaultUnlockedCards();

	for (FName CardId : Unlocked)
	{
		const ULKCardDefinition* Card = FindCard(CardId);
		if (!Card) { continue; }
		const bool bBuilding = Card->CardType == ELKCardType::Building;
		if (Card->CardType == ELKCardType::Spell) { continue; }
		if ((TabIndex == 1) != bBuilding) { continue; }

		FLKHomePanelRow Row;
		Row.RowId = CardId;
		Row.bSelectable = true;
		Row.Label = LKCardPresentation::Label(*Card);
		const FName UnitId = bBuilding ? Card->BuildingUnitId : Card->SpawnUnitId;
		const FLKUnitRow* Unit = GetUnitRow(UnitId);
		Row.Value = Unit ? FText::FromString(FString::Printf(TEXT("费用 %d · 生命 %.0f · 攻击 %.0f · 射程 %.0f"),
			Card->Cost, Unit->BaseHealth, Unit->AttackDamage, Unit->AttackRange))
			: FText::FromString(FString::Printf(TEXT("费用 %d · 单位定义缺失 %s"), Card->Cost, *UnitId.ToString()));
		Row.Detail = BuildCardDetail(CardId);
		Model.Rows.Add(Row);
	}

	const FLKHomePanelRow* Selected = Model.Rows.FindByPredicate(
		[SelectedRowId](const FLKHomePanelRow& Row) { return Row.RowId == SelectedRowId; });
	if (Selected && !Selected->Detail.IsEmpty()) { Model.Status = Selected->Detail; }
	else { Model.Status = FText::FromString(TEXT("点击条目查看详情")); }

	FLKHomePanelAction TabSoldiers;
	TabSoldiers.Label = FText::FromString(TEXT("佣兵"));
	TabSoldiers.ActionId = TEXT("Tab0");
	TabSoldiers.bEnabled = TabIndex != 0;
	Model.Actions.Add(TabSoldiers);

	FLKHomePanelAction TabBuildings;
	TabBuildings.Label = FText::FromString(TEXT("战斗建筑"));
	TabBuildings.ActionId = TEXT("Tab1");
	TabBuildings.bEnabled = TabIndex != 1;
	Model.Actions.Add(TabBuildings);

	FLKHomePanelAction Close;
	Close.Label = FText::FromString(TEXT("关闭"));
	Close.ActionId = TEXT("Close");
	Model.Actions.Add(Close);
	return Model;
}

int32 ALKHomeGameMode::GetDepartureGoldLimit() const
{
    const ULKProfileSubsystem* Profile = GetProfileSubsystem();
    return Profile ? FMath::Min(Profile->GetGold(), Profile->GetDepartureGoldCap()) : 0;
}
int32 ALKHomeGameMode::GetDepartureGold() const { return FMath::Clamp(DepartureGold, 0, GetDepartureGoldLimit()); }
void ALKHomeGameMode::SetDepartureGold(int32 Amount) { DepartureGold = FMath::Clamp(Amount, 0, GetDepartureGoldLimit()); }

FLKHomePanelModel ALKHomeGameMode::BuildGatePanel(FName SelectedRowId) const
{
    FLKHomePanelModel Model; Model.Panel = ELKHomePanel::Gate;
    Model.Title = FText::FromString(TEXT("大门 · 出征准备"));
    Model.Subtitle = FText::FromString(TEXT("固定从西境原野出发 · 五区域大地图 · 节点与连接每次重新生成"));
    const ULKProfileSubsystem* Profile = GetProfileSubsystem();
    const ULKRunSubsystem* Run = GetRunSubsystem();
    const bool bActive = Run && Run->HasRunInProgress();
    const FLKExpeditionLoadout Loadout = bActive ? Run->GetRunState().InitialLoadout : Profile ? Profile->GetSavedLoadout() : LKHomeContent::DefaultLoadout();
    for (FName HeroId : Loadout.HeroIds)
    {
        FLKHomePanelRow Row; Row.RowId = HeroId; Row.bSelectable = true;
        const FLKUnitRow* Unit = GetUnitRow(HeroId);
        Row.Label = Unit ? Unit->DisplayName : FText::FromName(HeroId);
        Row.Value = FText::FromString(TEXT("出征英雄")); Row.Detail = BuildHeroDetail(HeroId); Model.Rows.Add(Row);
    }
    for (FName CardId : Loadout.CardIds)
    {
        FLKHomePanelRow Row; Row.RowId = CardId; Row.bSelectable = true;
        const ULKCardDefinition* Card = FindCard(CardId);
        Row.Label = Card ? LKCardPresentation::Label(*Card) : FText::FromName(CardId);
        Row.Value = FText::FromString(TEXT("部队容量上限 8 格")); Row.Detail = BuildCardDetail(CardId); Model.Rows.Add(Row);
    }
    FLKHomePanelRow Money;
    Money.Label = FText::FromString(bActive ? TEXT("远征钱包") : TEXT("金币携带上限"));
    Money.Value = FText::FromString(bActive ? FString::Printf(TEXT("%d 金币 · 出发时 %d"), Run->GetWalletGold(), Run->GetRunState().StartingGold)
        : FString::Printf(TEXT("金库上限 %d · 当前可携带 %d"), Profile ? Profile->GetDepartureGoldCap() : 100, GetDepartureGoldLimit()));
    Model.Rows.Insert(Money, 0);
    auto AddAction = [&Model](FName Id, const TCHAR* Label, bool Enabled = true)
    { FLKHomePanelAction Action; Action.ActionId = Id; Action.Label = FText::FromString(Label); Action.bEnabled = Enabled; Model.Actions.Add(Action); };
    if (CanContinueExpedition())
    {
        AddAction(TEXT("Continue"), TEXT("继续远征"));
        if (bActive) { AddAction(TEXT("Abandon"), TEXT("结束本轮并带回金币")); }
        Model.Status = FText::FromString(TEXT("原队伍与路线保持不变；结束本轮将全额带回剩余钱包。"));
    }
    else
    {
        AddAction(TEXT("Start"), TEXT("出发"), IsProfileUsable());
        Model.Status = FText::FromString(TEXT("下方设置携带金币；胜利、失败或主动结束都全额带回剩余金币。"));
    }
    AddAction(TEXT("OpenWarRoom"), TEXT("前往战备处")); AddAction(TEXT("Close"), TEXT("关闭"));
    return Model;
}
FLKHomePanelModel ALKHomeGameMode::BuildWarRoomPanel(const FLKExpeditionLoadout& DraftLoadout) const
{
	FLKHomePanelModel Model;
	Model.Panel = ELKHomePanel::WarRoom;
	Model.Title = LKHomeContent::PanelTitle(ELKHomePanel::WarRoom);

	const ULKProfileSubsystem* Profile = GetProfileSubsystem();
	const FLKExpeditionLoadout Saved = Profile ? Profile->GetSavedLoadout() : LKHomeContent::DefaultLoadout();

	// 英雄槽（当前只有三名已解锁；未来新增解锁英雄可自动出现在列表里）
	TArray<FName> HeroPool = (Profile && Profile->HasProfile()) ? Profile->GetProfile().UnlockedHeroIds : LKHomeContent::DefaultUnlockedHeroes();
	for (int32 Slot = 0; Slot < 3; ++Slot)
	{
		FLKHomePanelRow Row;
		Row.Label = FText::FromString(FString::Printf(TEXT("英雄槽 %d"), Slot + 1));
		Row.RowId = DraftLoadout.HeroIds.IsValidIndex(Slot) ? DraftLoadout.HeroIds[Slot] : NAME_None;
		Row.bToggleable = false;
		Row.bEnabled = true;
		Row.bSelectable = true;
		Row.HeroSlotIndex = Slot;
		if (const FLKUnitRow* Unit = GetUnitRow(Row.RowId))
		{
			Row.Value = Unit->DisplayName;
		}
		else
		{
			Row.Value = FText::FromString(TEXT("（空）"));
		}
		Model.Rows.Add(Row);
	}

	// 卡池：勾选 = 进入草稿牌组
	for (FName HeroId : HeroPool)
	{
		FLKHomePanelRow Row;
		Row.RowId = HeroId;
		Row.bSelectable = true;
		const FLKUnitRow* Unit = GetUnitRow(HeroId);
		Row.Label = Unit ? Unit->DisplayName : FText::FromName(HeroId);
		Row.Value = FText::FromString(TEXT("选择上方英雄槽，再点击替换或交换位置"));
		Row.Detail = BuildHeroDetail(HeroId);
		Model.Rows.Add(Row);
	}

	// 卡池：勾选 = 进入草稿牌组
	TArray<FName> CardPool = (Profile && Profile->HasProfile()) ? Profile->GetProfile().UnlockedCardIds : LKHomeContent::DefaultUnlockedCards();
	for (FName CardId : CardPool)
	{
		const ULKCardDefinition* Card = FindCard(CardId);
		FLKHomePanelRow Row;
		Row.RowId = CardId;
		Row.bToggleable = true;
		Row.bChecked = DraftLoadout.CardIds.Contains(CardId);
		Row.Label = Card ? LKCardPresentation::Label(*Card) : FText::FromName(CardId);
		Row.Value = Card ? FText::FromString(FString::Printf(TEXT("%d 费 · %s"), Card->Cost,
			Card->CardType == ELKCardType::Spell ? TEXT("法术") : (Card->CardType == ELKCardType::Building ? TEXT("建筑") : TEXT("佣兵"))))
			: FText::FromString(TEXT("定义缺失"));
		Row.Detail = BuildCardDetail(CardId);
		Model.Rows.Add(Row);
	}

	// 平均费用与校验状态
	int32 TotalCost = 0;
	for (FName CardId : DraftLoadout.CardIds)
	{
		if (const ULKCardDefinition* Card = FindCard(CardId)) { TotalCost += Card->Cost; }
	}
	const float AverageCost = DraftLoadout.CardIds.IsEmpty() ? 0.f : float(TotalCost) / float(DraftLoadout.CardIds.Num());

	FString Error;
	const ELKLoadoutResult Validation = LKHomeContent::ValidateLoadout(DraftLoadout, GameData, Error);
	Model.Subtitle = FText::FromString(FString::Printf(TEXT("英雄 %d/3 · 牌组 %d 张（%d～%d）· 平均费用 %.2f"),
		DraftLoadout.HeroIds.Num(), DraftLoadout.CardIds.Num(), LKHomeContent::MinStartingDeck(), LKHomeContent::MaxStartingDeck(), AverageCost));
	Model.Status = Validation == ELKLoadoutResult::Success
		? FText::FromString(TEXT("配置合法：保存后下次新远征生效（不影响进行中的远征）"))
		: FText::FromString(Error);

	FLKHomePanelAction Save;
	Save.Label = FText::FromString(TEXT("保存战备"));
	Save.ActionId = TEXT("SaveLoadout");
	Save.bEnabled = Validation == ELKLoadoutResult::Success && IsProfileUsable();
	Model.Actions.Add(Save);

	FLKHomePanelAction Reset;
	Reset.Label = FText::FromString(TEXT("恢复已保存"));
	Reset.ActionId = TEXT("ResetDraft");
	Reset.bEnabled = true;
	Model.Actions.Add(Reset);

	FLKHomePanelAction Close;
	Close.Label = FText::FromString(TEXT("关闭"));
	Close.ActionId = TEXT("Close");
	Model.Actions.Add(Close);

	// 顶部摘要行：已保存配置（用于对比草稿）
	FLKHomePanelRow SavedRow;
	SavedRow.Label = FText::FromString(TEXT("已保存"));
	SavedRow.Value = FText::FromString(FString::Printf(TEXT("%d 英雄 · %d 卡"), Saved.HeroIds.Num(), Saved.CardIds.Num()));
	Model.Rows.Insert(SavedRow, 0);
	return Model;
}

FLKHomePanelModel ALKHomeGameMode::BuildPanelModel(ELKHomePanel Panel, const FLKExpeditionLoadout& DraftLoadout,
	FName SelectedRowId, int32 TabIndex) const
{
	switch (Panel)
	{
	case ELKHomePanel::Statue:
	case ELKHomePanel::Treasury:
		return BuildUpgradePanel(Panel);
	case ELKHomePanel::Library:
	case ELKHomePanel::HeroHouse:
		return BuildCollectionPanel(Panel, SelectedRowId);
	case ELKHomePanel::Barracks:
		return BuildBarracksPanel(SelectedRowId, TabIndex);
	case ELKHomePanel::Gate:
		return BuildGatePanel(SelectedRowId);
	case ELKHomePanel::WarRoom:
		return BuildWarRoomPanel(DraftLoadout);
	default:
	{
		FLKHomePanelModel Model;
		Model.Panel = ELKHomePanel::None;
		return Model;
	}
	}
}

// ---------------- 命令 ----------------

ELKUpgradeResult ALKHomeGameMode::RequestUpgrade(FName BuildingId, int32 ExpectedLevel)
{
	ULKProfileSubsystem* Profile = GetProfileSubsystem();
	if (!Profile || !Profile->EnsureProfile() || !Profile->HasProfile()) { return ELKUpgradeResult::NoProfile; }
	const ELKUpgradeResult Result = Profile->UpgradeBuilding(BuildingId, ExpectedLevel, FGuid::NewGuid());
	if (Result == ELKUpgradeResult::Success)
	{
		RefreshBuildingLevels();
		OnHomeRefreshRequested.Broadcast();
	}
	return Result;
}

ELKLoadoutResult ALKHomeGameMode::RequestSaveLoadout(const FLKExpeditionLoadout& DraftLoadout)
{
	ULKProfileSubsystem* Profile = GetProfileSubsystem();
	if (!Profile || !Profile->EnsureProfile() || !Profile->HasProfile()) { return ELKLoadoutResult::NoProfile; }
	const ELKLoadoutResult Result = Profile->SaveLoadout(DraftLoadout, GameData);
	if (Result == ELKLoadoutResult::Success) { OnHomeRefreshRequested.Broadcast(); }
	return Result;
}

ELKExpeditionStartResult ALKHomeGameMode::RequestStartExpedition(FName RegionId)
{
	RegionId = LKHomeContent::DefaultRegionId(); // 固定起点；旧参数保留蓝图/调用方兼容。
	ULKProfileSubsystem* Profile = GetProfileSubsystem();
	ULKRunSubsystem* Run = GetRunSubsystem();
	if (!Profile || !Run || !Profile->EnsureProfile() || !Profile->HasProfile()) { return ELKExpeditionStartResult::NoProfile; }
	if (!Profile->IsProfileUsable()) { return ELKExpeditionStartResult::SaveFailed; }

	const FLKRegionDefinition* Region = LKHomeContent::FindRegion(RegionId);
	if (!Region) { return ELKExpeditionStartResult::UnknownRegion; }
	if (!Profile->IsRegionUnlocked(RegionId)) { return ELKExpeditionStartResult::RegionLocked; }
	if (Run->HasRunInProgress()) { return ELKExpeditionStartResult::RunInProgress; }
	if (Run->HasPendingSettlement() && !Run->GetPendingSettlement().bProfileApplied && Run->IsHomeRewardEligible())
	{ ApplyPendingSettlement(); if (!Run->GetPendingSettlement().bProfileApplied) { return ELKExpeditionStartResult::SaveFailed; } }
	if (!LKHomeContent::DoesMapExist(BattleMapName)) { return ELKExpeditionStartResult::MapLoadFailed; }

	FString Error;
	FLKExpeditionStartRequest Request;
	if (!LKHomeContent::BuildExpeditionStartRequest(*Profile, GameData, RegionId,
		[this](FName UnitId) { return GetUnitRow(UnitId); },
		[this](FName CardId) { return FindCard(CardId); },
		Request, Error))
	{
		UE_LOG(LogLK, Warning, TEXT("[Home] 出征被拒绝：%s"), *Error);
		return ELKExpeditionStartResult::NoLoadout;
	}

	const bool bRestart = Run->HasRun() && Run->IsTerminal();
	Request.StartingGold = GetDepartureGold();
	const bool bStarted = bRestart ? Run->RestartRun(Request) : Run->StartNewRun(Request);
	if (!bStarted)
	{
		UE_LOG(LogLK, Error, TEXT("[Home] 创建远征失败（%s）"), bRestart ? TEXT("重开") : TEXT("新建"));
		return ELKExpeditionStartResult::SaveFailed;
	}

	SelectRegion(RegionId);
	OnHomeRefreshRequested.Broadcast();
	if (!OpenBattleMap(Region->MapName.IsNone() ? BattleMapName : Region->MapName))
	{
		return ELKExpeditionStartResult::MapLoadFailed;
	}
	return ELKExpeditionStartResult::Success;
}

ELKExpeditionStartResult ALKHomeGameMode::RequestContinueExpedition()
{
	ULKRunSubsystem* Run = GetRunSubsystem();
	if (!Run) { return ELKExpeditionStartResult::NoProfile; }
	if (!Run->HasRunInProgress() && !Run->HasSavedExpedition())
	{
		return ELKExpeditionStartResult::RunInProgress;
	}

	FName MapName = BattleMapName;
	if (Run->HasRun())
	{
		const FLKRunState State = Run->GetRunState();
		if (const FLKRegionDefinition* Region = LKHomeContent::FindRegion(State.RegionId))
		{
			if (!Region->MapName.IsNone()) { MapName = Region->MapName; }
		}
	}
	if (!OpenBattleMap(MapName)) { return ELKExpeditionStartResult::MapLoadFailed; }
	return ELKExpeditionStartResult::Success;
}

bool ALKHomeGameMode::RequestAbandonExpedition()
{
	ULKRunSubsystem* Run = GetRunSubsystem();
	if (!Run || !Run->AbandonCurrentRun()) { return false; }
	ApplyPendingSettlement();
	OnHomeRefreshRequested.Broadcast();
	return true;
}

bool ALKHomeGameMode::OpenBattleMap(FName MapName)
{
	UWorld* World = GetWorld();
	if (!World || MapName.IsNone()) { return false; }
	if (!LKHomeContent::DoesMapExist(MapName))
	{
		UE_LOG(LogLK, Error, TEXT("[Home] 找不到地图 %s：请确认工程里存在 Content/Maps/%s.umap"), *MapName.ToString(), *MapName.ToString());
		return false;
	}
	UE_LOG(LogLK, Log, TEXT("[Home] 前往战斗地图 %s"), *MapName.ToString());
	ULKJourneyPresentationSubsystem::Travel(this, MapName);
	return true;
}

ELKSettlementResult ALKHomeGameMode::ApplyPendingSettlement()
{
	ULKRunSubsystem* Run = GetRunSubsystem();
	ULKProfileSubsystem* Profile = GetProfileSubsystem();
	if (!Run || !Profile || !Profile->EnsureProfile() || !Profile->HasProfile())
	{
		return ELKSettlementResult::NoProfile;
	}
	if (!Run->HasRun() || !Run->IsTerminal()) { return ELKSettlementResult::NoPendingSettlement; }

	const FLKSettlementReceipt Receipt = Run->GetPendingSettlement();
	if (!Receipt.SettlementId.IsValid()) { return ELKSettlementResult::NoPendingSettlement; }
	if (Receipt.bProfileApplied) { return ELKSettlementResult::AlreadyApplied; }

	const ELKSettlementResult Result = Profile->ApplySettlement(Receipt);
	if (Result == ELKSettlementResult::Success || Result == ELKSettlementResult::AlreadyApplied)
	{
		Run->MarkPendingSettlementApplied();
		bSettlementAttempted = true;
		OnHomeRefreshRequested.Broadcast();
	}
	return Result;
}
