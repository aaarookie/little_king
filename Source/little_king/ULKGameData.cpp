#include "ULKGameData.h"
#include "LKLog.h"
#include "ULKCardDefinition.h"
#include "LKExpeditionMercenaryContent.h"

ULKGameData::ULKGameData()
{
	EncounterTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/Data/DT_Encounters.DT_Encounters")));
    DefaultHeroTraits.FindOrAdd(TEXT("Hero_Mage")).Traits = { TEXT("Trait_MageSpellReach") };
    DefaultHeroTraits.FindOrAdd(TEXT("Hero_Knight")).Traits = { TEXT("Trait_KnightTauntAura") };
    FireballSkillHeroIds = { TEXT("Hero_Mage") };
}

void ULKGameData::EnsureDefaultDecks()
{
	if (DefaultPlayerDeck.Num() == 0)
	{
		DefaultPlayerDeck = {
			TEXT("Unit_Swordsman"), TEXT("Unit_Archer"), TEXT("Unit_Shieldbearer"),
            TEXT("Spell_Fireball"), TEXT("Spell_HealWave"),
            TEXT("Building_ArrowTower"), TEXT("Building_Barracks")
		};
	}

	if (DefaultEnemyDeck.Num() == 0)
	{
		DefaultEnemyDeck = {
			TEXT("Unit_Swordsman"), TEXT("Unit_Archer"), TEXT("Unit_Shieldbearer"),
            TEXT("Spell_Fireball"), TEXT("Spell_HealWave"),
            TEXT("Building_ArrowTower"), TEXT("Building_Barracks")
		};
	}

	UE_LOG(LogLK, Log, TEXT("[GameData] 默认牌库就绪: Player=%d 张, Enemy=%d 张"),
		DefaultPlayerDeck.Num(), DefaultEnemyDeck.Num());
}

void ULKGameData::EnsureCardLibrary()
{
	// 原型期内置卡牌库（编辑器配置 CardLibrary 后不再走这里）
	if (CardLibrary.Num() == 0)
	{
		auto AddCard = [this](FName Id, const TCHAR* Name, int32 Cost, ELKCardType Type,
			FName SpawnId, ELKSpellEffect SpellFx, float SpellValue, float SpellRadius)
		{
			ULKCardDefinition* Card = NewObject<ULKCardDefinition>(this, Id);
			Card->CardId = Id;
			Card->CardName = FText::FromString(Name);
			Card->Cost = Cost;
			Card->CardType = Type;
			Card->SpawnUnitId = SpawnId;
			Card->BuildingUnitId = SpawnId;
			Card->SpellEffect = SpellFx;
			Card->SpellValue = SpellValue;
			Card->SpellRadius = SpellRadius;
			CardLibrary.Add(Card);
		};

		AddCard(TEXT("Unit_Swordsman"),    TEXT("剑士"),   2, ELKCardType::Unit,     TEXT("Unit_Swordsman"),    ELKSpellEffect::None,   0.f,   0.f);
		AddCard(TEXT("Unit_Archer"),       TEXT("弓箭手"), 2, ELKCardType::Unit,     TEXT("Unit_Archer"),       ELKSpellEffect::None,   0.f,   0.f);
		AddCard(TEXT("Unit_Shieldbearer"), TEXT("盾卫"),   2, ELKCardType::Unit,     TEXT("Unit_Shieldbearer"), ELKSpellEffect::None,   0.f,   0.f);
		AddCard(TEXT("Spell_Fireball"),    TEXT("火球术"), 2, ELKCardType::Spell,    NAME_None,                 ELKSpellEffect::Damage, 60.f,  250.f);
		AddCard(TEXT("Spell_HealWave"),    TEXT("治疗波"), 2, ELKCardType::Spell,    NAME_None,                 ELKSpellEffect::Heal,   40.f,  300.f);
		AddCard(TEXT("Building_ArrowTower"), TEXT("箭塔"), 2, ELKCardType::Building, TEXT("Building_ArrowTower"), ELKSpellEffect::None, 0.f,   0.f);
		AddCard(TEXT("Building_Barracks"), TEXT("兵营"),   2, ELKCardType::Building, TEXT("Building_Barracks"), ELKSpellEffect::None,   0.f,   0.f);
		UE_LOG(LogLK, Log, TEXT("[GameData] 已注入 %d 张内置卡（未配置 CardLibrary）"), CardLibrary.Num());
	}

	// D3 奖励新卡：骷髅兵/骷髅射手（1 费文字卡，暂无美术）。
	// 无论 DA_GameData 是否已配置 CardLibrary，都保证运行时目录存在（只改运行时副本，不写资产）。
	const TPair<FName, const TCHAR*> SkeletonCards[] = {
		{ TEXT("Unit_Skeleton"), TEXT("骷髅兵") },
		{ TEXT("Unit_SkeletonArcher"), TEXT("骷髅射手") },
	};
	for (const TPair<FName, const TCHAR*>& Entry : SkeletonCards)
	{
		const bool bExists = CardLibrary.ContainsByPredicate(
			[&Entry](const TObjectPtr<ULKCardDefinition>& Card) { return Card && Card->CardId == Entry.Key; });
		if (bExists) { continue; }
		ULKCardDefinition* Card = NewObject<ULKCardDefinition>(this, Entry.Key);
		Card->CardId = Entry.Key;
		Card->CardName = FText::FromString(Entry.Value);
		Card->Cost = 1;
		Card->CardType = ELKCardType::Unit;
		Card->SpawnUnitId = Entry.Key;
		Card->BuildingUnitId = Entry.Key;
		CardLibrary.Add(Card);
		UE_LOG(LogLK, Log, TEXT("[GameData] 注入奖励卡：%s（%s，1 费，文字卡）"), *Entry.Key.ToString(), Entry.Value);
	}
    for (const FLKTemporaryMercenaryDefinition& Definition : LKExpeditionMercenaryContent::All())
    {
        const FName Id = Definition.Unit.UnitId;
        const TObjectPtr<ULKCardDefinition>* Existing = CardLibrary.FindByPredicate([Id](const TObjectPtr<ULKCardDefinition>& Card) { return Card && Card->CardId == Id; });
        // Never mutate shared DA assets: identity fixes live on a runtime copy.
        ULKCardDefinition* Card = Existing ? DuplicateObject<ULKCardDefinition>(Existing->Get(), this) : NewObject<ULKCardDefinition>(this);
        if (!Existing) { Card->Cost = Definition.Cost; }
        Card->CardId = Id; Card->CardName = Definition.Unit.DisplayName; Card->Description = Definition.Description;
        Card->CardType = Definition.Unit.UnitClass == ELKUnitClass::Building ? ELKCardType::Building : ELKCardType::Unit;
        Card->SpawnUnitId = Id; Card->BuildingUnitId = Id; Card->bExpeditionOnly = true;
        if (Existing) { CardLibrary[CardLibrary.IndexOfByKey(*Existing)] = Card; } else { CardLibrary.Add(Card); }
    }
}
