#include "LKHomeContent.h"
#include "LKCardRules.h"

#include "LKEncounterContent.h"
#include "LKUnitContent.h"
#include "ULKCardDefinition.h"
#include "ULKGameData.h"
#include "ULKProfileSubsystem.h"

namespace
{
	const FName BuildingStatue = "Home_StatueSaintMaria";
	const FName BuildingLibrary = "Home_Library";
	const FName BuildingGate = "Home_Gate";
	const FName BuildingHeroHouse = "Home_HeroHouse";
	const FName BuildingTreasury = "Home_Treasury";
	const FName BuildingBarracks = "Home_Barracks";
	const FName BuildingWarRoom = "Home_WarRoom";

	const FName RegionUndeadFrontier = "Region_UndeadFrontier";

	FLKBuildingDefinition MakeBuilding(FName Id, const TCHAR* Name, const TCHAR* Description,
		bool bUpgradable, int32 MaxLevel, std::initializer_list<int32> Costs)
	{
		FLKBuildingDefinition Definition;
		Definition.BuildingId = Id;
		Definition.DisplayName = FText::FromString(Name);
		Definition.Description = FText::FromString(Description);
		Definition.bUpgradable = bUpgradable;
		Definition.MaxLevel = MaxLevel;
		for (int32 Cost : Costs) { Definition.UpgradeCosts.Add(Cost); }
		return Definition;
	}
}

namespace LKHomeContent
{
	int32 TreasuryDepartureGoldCap(int32 Level) { return 100 + 50 * (FMath::Clamp(Level, 1, TreasuryMaxLevel) - 1); }
	FName BuildingId(ELKHomeBuilding Building)
	{
		switch (Building)
		{
		case ELKHomeBuilding::Statue: return BuildingStatue;
		case ELKHomeBuilding::Library: return BuildingLibrary;
		case ELKHomeBuilding::Gate: return BuildingGate;
		case ELKHomeBuilding::HeroHouse: return BuildingHeroHouse;
		case ELKHomeBuilding::Treasury: return BuildingTreasury;
		case ELKHomeBuilding::Barracks: return BuildingBarracks;
		case ELKHomeBuilding::WarRoom: return BuildingWarRoom;
		default: return NAME_None;
		}
	}

	ELKHomeBuilding BuildingFromId(FName InBuildingId)
	{
		for (uint8 Index = 0; Index <= uint8(ELKHomeBuilding::WarRoom); ++Index)
		{
			const ELKHomeBuilding Building = ELKHomeBuilding(Index);
			if (BuildingId(Building) == InBuildingId) { return Building; }
		}
		return ELKHomeBuilding::Statue;
	}

	const TArray<FLKBuildingDefinition>& Buildings()
	{
		static const TArray<FLKBuildingDefinition> Definitions = {
			// 神像：1~4 级，每级 +20 个百分点（用户确定）
			MakeBuilding(BuildingStatue, TEXT("圣玛丽亚神像"),
				TEXT("每场战斗结束后按等级恢复玩家英雄生命：40% / 60% / 80% / 100% 最大生命。"),
				true, StatueMaxLevel, { 60, 120, 200 }),
			MakeBuilding(BuildingLibrary, TEXT("图书馆"),
				TEXT("查看永久解锁的法术与说明。首版只读，不提供购买或研究。"), false, 1, {}),
			MakeBuilding(BuildingGate, TEXT("大门"),
				TEXT("查看出征英雄、牌组和携带金币，从固定起点出发；已有远征时从这里继续。"), false, 1, {}),
			MakeBuilding(BuildingHeroHouse, TEXT("英雄之家"),
				TEXT("查看永久解锁英雄的属性、技能与特性。首版只读。"), false, 1, {}),
			MakeBuilding(BuildingTreasury, TEXT("金库"),
				TEXT("每级提升银币产速 10% 并增加 2 点银币上限；出征金币携带上限为 100/150/200/250/300。"),
				true, TreasuryMaxLevel, { 50, 100, 160, 240 }),
			MakeBuilding(BuildingBarracks, TEXT("军营"),
				TEXT("分页查看永久解锁的佣兵与战斗建筑。首版只读。"), false, 1, {}),
			MakeBuilding(BuildingWarRoom, TEXT("战备处"),
				TEXT("保存三名英雄与至少五种卡；部队容量上限 8 格，多格卡仍只占一张手牌。下次新远征生效。"), false, 1, {})
		};
		return Definitions;
	}

	const FLKBuildingDefinition* FindBuilding(FName InBuildingId)
	{
		return Buildings().FindByPredicate(
			[InBuildingId](const FLKBuildingDefinition& Definition) { return Definition.BuildingId == InBuildingId; });
	}

	ELKHomePanel PanelForBuilding(ELKHomeBuilding Building)
	{
		switch (Building)
		{
		case ELKHomeBuilding::Statue: return ELKHomePanel::Statue;
		case ELKHomeBuilding::Library: return ELKHomePanel::Library;
		case ELKHomeBuilding::Gate: return ELKHomePanel::Gate;
		case ELKHomeBuilding::HeroHouse: return ELKHomePanel::HeroHouse;
		case ELKHomeBuilding::Treasury: return ELKHomePanel::Treasury;
		case ELKHomeBuilding::Barracks: return ELKHomePanel::Barracks;
		case ELKHomeBuilding::WarRoom: return ELKHomePanel::WarRoom;
		default: return ELKHomePanel::None;
		}
	}

	FText PanelTitle(ELKHomePanel Panel)
	{
		switch (Panel)
		{
		case ELKHomePanel::Statue: return FText::FromString(TEXT("圣玛丽亚神像"));
		case ELKHomePanel::Library: return FText::FromString(TEXT("图书馆"));
		case ELKHomePanel::Gate: return FText::FromString(TEXT("大门 · 出征准备"));
		case ELKHomePanel::HeroHouse: return FText::FromString(TEXT("英雄之家"));
		case ELKHomePanel::Treasury: return FText::FromString(TEXT("金库"));
		case ELKHomePanel::Barracks: return FText::FromString(TEXT("军营"));
		case ELKHomePanel::WarRoom: return FText::FromString(TEXT("战备处"));
		default: return FText::FromString(TEXT("家园"));
		}
	}

	FText BuildingDisplayName(FName InBuildingId)
	{
		const FLKBuildingDefinition* Definition = FindBuilding(InBuildingId);
		return Definition ? Definition->DisplayName : FText::FromName(InBuildingId);
	}

	float StatueRecoveryPercent(int32 Level)
	{
		const int32 Clamped = FMath::Clamp(Level, 1, StatueMaxLevel);
		return FMath::Clamp(0.4f + 0.2f * float(Clamped - 1), 0.f, 1.f);
	}

	FText StatueRecoveryText(int32 Level)
	{
		return FText::FromString(FString::Printf(TEXT("%d%% 最大生命"), FMath::RoundToInt(StatueRecoveryPercent(Level) * 100.f)));
	}

	float TreasurySpeedMultiplier(int32 Level)
	{
		const int32 Clamped = FMath::Clamp(Level, 1, TreasuryMaxLevel);
		return 1.f + 0.1f * float(Clamped - 1);
	}

	int32 TreasuryCapBonus(int32 Level)
	{
		return 2 * (FMath::Clamp(Level, 1, TreasuryMaxLevel) - 1);
	}

	int32 UpgradeCost(FName InBuildingId, int32 CurrentLevel)
	{
		const FLKBuildingDefinition* Definition = FindBuilding(InBuildingId);
		if (!Definition || !Definition->bUpgradable) { return -1; }
		const int32 Level = FMath::Clamp(CurrentLevel, 1, Definition->MaxLevel);
		if (Level >= Definition->MaxLevel) { return -1; }
		const int32 Index = Level - 1;
		return Definition->UpgradeCosts.IsValidIndex(Index) ? Definition->UpgradeCosts[Index] : -1;
	}

	int32 TotalUpgradeCost(FName InBuildingId)
	{
		const FLKBuildingDefinition* Definition = FindBuilding(InBuildingId);
		if (!Definition || !Definition->bUpgradable) { return 0; }
		int32 Total = 0;
		for (int32 Level = 1; Level < Definition->MaxLevel; ++Level) { Total += FMath::Max(0, UpgradeCost(InBuildingId, Level)); }
		return Total;
	}

	const TArray<FLKRegionDefinition>& Regions()
	{
		static const TArray<FLKRegionDefinition> Definitions = []
		{
			FLKRegionDefinition Frontier;
			Frontier.RegionId = RegionUndeadFrontier;
			Frontier.DisplayName = FText::FromString(TEXT("亡灵边境"));
			Frontier.EnemyTheme = FText::FromString(TEXT("骷髅兵、骷髅射手、死灵法师、骷髅巨人、骷髅王"));
			Frontier.RouteSummary = FText::FromString(TEXT("四层推进：普通 → 普通/休息 → 精英/休息 → 首领；选择休息时实际战斗 2~4 场"));
			Frontier.RewardSummary = FText::FromString(TEXT("每房胜利获得金币：普通 10 / 精英 20 / 首领 40；通关额外 +20"));
			Frontier.MapName = "L_BattleTest";
			Frontier.bEnabled = true;
			return TArray<FLKRegionDefinition>{ Frontier };
		}();
		return Definitions;
	}

	const FLKRegionDefinition* FindRegion(FName RegionId)
	{
		return Regions().FindByPredicate(
			[RegionId](const FLKRegionDefinition& Definition) { return Definition.RegionId == RegionId; });
	}

	FName DefaultRegionId() { return RegionUndeadFrontier; }

	const TArray<FName>& DefaultUnlockedHeroes()
	{
		static const TArray<FName> Heroes = { "Hero_Knight", "Hero_Mage", "Hero_Ranger" };
		return Heroes;
	}

	const TArray<FName>& DefaultUnlockedCards()
	{
		static const TArray<FName> Cards = {
			"Unit_Swordsman", "Unit_Archer", "Unit_Shieldbearer",
			"Spell_Fireball", "Spell_HealWave",
			"Building_ArrowTower", "Building_Barracks" };
		return Cards;
	}

	const TArray<FName>& DefaultUnlockedRegions()
	{
		static const TArray<FName> Regions = { RegionUndeadFrontier };
		return Regions;
	}

	bool IsDefaultUnlockedHero(FName HeroId) { return DefaultUnlockedHeroes().Contains(HeroId); }
	bool IsDefaultUnlockedCard(FName CardId) { return DefaultUnlockedCards().Contains(CardId); }
	bool IsDefaultUnlockedRegion(FName RegionId) { return DefaultUnlockedRegions().Contains(RegionId); }

	bool IsPlayerHeroCandidate(FName HeroId)
	{
		// 显式白名单：敌方死灵法师/骷髅巨人/骷髅王不会因为属于 Hero/Boss 就进入玩家可选名单。
		return DefaultUnlockedHeroes().Contains(HeroId);
	}

	int32 MinStartingDeck() { return 5; }
	int32 MaxStartingDeck() { return 8; }

	FLKExpeditionLoadout DefaultLoadout()
	{
		FLKExpeditionLoadout Loadout;
		Loadout.HeroIds = DefaultUnlockedHeroes();
		Loadout.CardIds = DefaultUnlockedCards();
		return Loadout;
	}

	ELKLoadoutResult ValidateLoadout(const FLKExpeditionLoadout& Loadout, const ULKGameData* Data, FString& OutError)
	{
		OutError.Reset();

		if (Loadout.HeroIds.Num() != 3)
		{
			OutError = FString::Printf(TEXT("英雄必须恰好 3 名（当前 %d 名）"), Loadout.HeroIds.Num());
			return ELKLoadoutResult::WrongHeroCount;
		}
		TSet<FName> Heroes;
		for (FName HeroId : Loadout.HeroIds)
		{
			if (HeroId.IsNone()) { OutError = TEXT("英雄 ID 为空"); return ELKLoadoutResult::UnknownHero; }
			if (Heroes.Contains(HeroId)) { OutError = FString::Printf(TEXT("英雄重复：%s"), *HeroId.ToString()); return ELKLoadoutResult::DuplicateHero; }
			Heroes.Add(HeroId);
			if (!IsPlayerHeroCandidate(HeroId))
			{
				OutError = FString::Printf(TEXT("英雄未永久解锁：%s"), *HeroId.ToString());
				return ELKLoadoutResult::LockedHero;
			}
			if (Data && !LKUnitContent::Find(HeroId))
			{
				OutError = FString::Printf(TEXT("英雄定义缺失：%s"), *HeroId.ToString());
				return ELKLoadoutResult::UnknownHero;
			}
		}

		const int32 CardCount = Loadout.CardIds.Num();
		if (CardCount < MinStartingDeck())
		{
			OutError = FString::Printf(TEXT("初始牌组至少 %d 张（当前 %d 张）"), MinStartingDeck(), CardCount);
			return ELKLoadoutResult::DeckTooSmall;
		}
		if (LKCardRules::Used(Loadout.CardIds) > MaxStartingDeck())
		{
			OutError = FString::Printf(TEXT("部队容量最多 %d 格（当前 %d 格）"), MaxStartingDeck(), LKCardRules::Used(Loadout.CardIds));
			return ELKLoadoutResult::DeckTooLarge;
		}
		TSet<FName> Cards;
		for (FName CardId : Loadout.CardIds)
		{
			if (CardId.IsNone()) { OutError = TEXT("卡牌 ID 为空"); return ELKLoadoutResult::UnknownCard; }
			if (Cards.Contains(CardId)) { OutError = FString::Printf(TEXT("卡牌重复：%s"), *CardId.ToString()); return ELKLoadoutResult::DuplicateCard; }
			Cards.Add(CardId);
			if (!IsDefaultUnlockedCard(CardId))
			{
				OutError = FString::Printf(TEXT("卡牌未永久解锁：%s"), *CardId.ToString());
				return ELKLoadoutResult::LockedCard;
			}
			if (Data && !Data->CardLibrary.ContainsByPredicate(
				[CardId](const ULKCardDefinition* Card) { return Card && Card->CardId == CardId; }))
			{
				OutError = FString::Printf(TEXT("卡牌定义缺失：%s（不在 DA_GameData.CardLibrary）"), *CardId.ToString());
				return ELKLoadoutResult::UnknownCard;
			}
		}
		return ELKLoadoutResult::Success;
	}

	FText LoadoutResultText(ELKLoadoutResult Result)
	{
		switch (Result)
		{
		case ELKLoadoutResult::Success: return FText::FromString(TEXT("战备已保存，下次新远征生效"));
		case ELKLoadoutResult::NoProfile: return FText::FromString(TEXT("永久档不可用，无法保存"));
		case ELKLoadoutResult::WrongHeroCount: return FText::FromString(TEXT("必须恰好选择 3 名英雄"));
		case ELKLoadoutResult::DuplicateHero: return FText::FromString(TEXT("英雄重复"));
		case ELKLoadoutResult::LockedHero: return FText::FromString(TEXT("英雄尚未永久解锁"));
		case ELKLoadoutResult::UnknownHero: return FText::FromString(TEXT("英雄定义缺失"));
		case ELKLoadoutResult::DeckTooSmall: return FText::FromString(TEXT("初始牌组至少 5 张"));
		case ELKLoadoutResult::DeckTooLarge: return FText::FromString(TEXT("部队容量最多 8 格"));
		case ELKLoadoutResult::DuplicateCard: return FText::FromString(TEXT("卡牌重复"));
		case ELKLoadoutResult::LockedCard: return FText::FromString(TEXT("卡牌尚未永久解锁"));
		case ELKLoadoutResult::UnknownCard: return FText::FromString(TEXT("卡牌定义缺失"));
		case ELKLoadoutResult::SaveFailed: return FText::FromString(TEXT("存档写入失败，配置未保存"));
		default: return FText::FromString(TEXT("未知结果"));
		}
	}

	FLKHomeRewardRules DefaultRewardRules()
	{
		FLKHomeRewardRules Rules;
		Rules.NormalWin = 10;
		Rules.EliteWin = 20;
		Rules.BossWin = 40;
		Rules.RunCompletedBonus = 20;
		Rules.FailureKeepPercent = 1.f;
		Rules.AbandonKeepPercent = 1.f;
		Rules.RuleVersion = 2;
		return Rules;
	}

	int32 GoldForRewardTier(const FLKHomeRewardRules& Rules, int32 RewardTier)
	{
		switch (FMath::Max(1, RewardTier))
		{
		case 1: return FMath::Max(0, Rules.NormalWin);
		case 2: return FMath::Max(0, Rules.EliteWin);
		default: return FMath::Max(0, Rules.BossWin);
		}
	}

	FName DefaultHomeMapName() { return "L_Home"; }
	FName DefaultBattleMapName() { return "L_BattleTest"; }

	bool DoesMapExist(FName MapName)
	{
		if (MapName.IsNone()) { return false; }
		const FString PackageName = FString::Printf(TEXT("/Game/Maps/%s"), *MapName.ToString());
		return FPackageName::DoesPackageExist(PackageName);
	}

	bool BuildExpeditionStartRequest(const ULKProfileSubsystem& Profile, const ULKGameData* Data, FName RegionId,
		TFunctionRef<const FLKUnitRow*(FName)> ResolveUnitRow,
		TFunctionRef<const ULKCardDefinition*(FName)> ResolveCard,
		FLKExpeditionStartRequest& OutRequest, FString& OutError)
	{
		OutError.Reset();
		OutRequest = FLKExpeditionStartRequest();

		if (!Profile.HasProfile()) { OutError = TEXT("永久档不可用"); return false; }
		const FLKRegionDefinition* Region = FindRegion(RegionId);
		if (!Region) { OutError = FString::Printf(TEXT("未知区域 %s"), *RegionId.ToString()); return false; }
		if (!Profile.IsRegionUnlocked(RegionId)) { OutError = FString::Printf(TEXT("区域未解锁 %s"), *RegionId.ToString()); return false; }

		const FLKExpeditionLoadout Loadout = Profile.GetSavedLoadout();
		if (ValidateLoadout(Loadout, Data, OutError) != ELKLoadoutResult::Success) { return false; }

		OutRequest.ProfileId = Profile.GetProfile().ProfileId;
		OutRequest.RegionId = RegionId;
		OutRequest.Loadout = Loadout;
		// 正式新远征每次抽取新种子；之后随完整节点图保存，读档绝不重抽。
		// 调试复现可以显式覆盖 Request.Seed，单场 BattleSeed 不再固定整轮地图。
		OutRequest.Seed = int32(GetTypeHash(FGuid::NewGuid()) & MAX_int32);
		OutRequest.BonusSnapshot = Profile.BuildBonusSnapshot(Data, RegionId);
		OutRequest.RewardRules = DefaultRewardRules();
		OutRequest.bEligibleForHomeReward = true;

		for (FName HeroId : Loadout.HeroIds)
		{
			const FLKUnitRow* Row = ResolveUnitRow(HeroId);
			if (!Row) { OutError = FString::Printf(TEXT("英雄定义缺失：%s"), *HeroId.ToString()); return false; }
			FLKRunHeroState Hero;
			Hero.HeroId = HeroId;
			Hero.Health = Hero.MaxHealth = Row->BaseHealth;
			Hero.BaseMaxHealth = Row->BaseHealth;
			for (FName Trait : Row->HeroTraits) { Hero.Traits.AddUnique(Trait); }
			if (Data)
			{
				if (const FLKHeroTraitEntry* Entry = Data->DefaultHeroTraits.Find(HeroId))
				{
					for (FName Trait : Entry->Traits) { Hero.Traits.AddUnique(Trait); }
				}
			}
			Hero.Traits.Sort(FNameLexicalLess());
			OutRequest.Heroes.Add(Hero);
		}
		for (FName CardId : Loadout.CardIds)
		{
			FLKRunCardState Card;
			Card.CardId = CardId;
			Card.UpgradeLevel = 0;
			OutRequest.Cards.Add(Card);
		}
		if (!LKEncounterContent::BuildValidatedCatalog(Data, ResolveUnitRow, ResolveCard, OutRequest.Encounters, OutError))
		{
			return false;
		}
		return true;
	}
}
