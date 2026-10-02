#include "ULKGameData.h"
#include "LKResearchContent.h"
#include "LKLog.h"
#include "LKCardRules.h"
#include "ULKCardDefinition.h"
#include "LKExpeditionMercenaryContent.h"
#include "LKBattleArt.h"
#include "LKWorldArt.h"
#include "LKV083Art.h"
#include "Engine/Texture2D.h"
#include "Sound/SoundBase.h"

void ULKGameData::EnsurePresentationDefaults()
{
    if (!bUseDefaultSoundSet) { return; }
    for (FName Key : {FName("MeleeHit"), FName("RangedShoot"), FName("HitTaken"), FName("UnitDied"),
         FName("CardPlay"), FName("BattleStart"), FName("Victory"), FName("Defeat")})
    {
        if (SoundMap.Contains(Key)) { continue; }
        const FString Name = TEXT("S_") + Key.ToString();
        SoundMap.Add(Key, TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Art/StorybookV1/Audio/") + Name + TEXT(".") + Name)));
    }
    for (FName Key : LKV083Art::SoundIds())
    { if (!SoundMap.Contains(Key)) { SoundMap.Add(Key, TSoftObjectPtr<USoundBase>(FSoftObjectPath(LKV083Art::SoundPath(Key)))); } }
    for (FName Key : LKWorldArt::SoundIds())
    {
        if (!SoundMap.Contains(Key)) { SoundMap.Add(Key, TSoftObjectPtr<USoundBase>(FSoftObjectPath(LKWorldArt::SoundPath(Key)))); }
    }
}

ULKGameData::ULKGameData()
{
	EncounterTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/Data/DT_Encounters.DT_Encounters")));
    DefaultHeroTraits.FindOrAdd(TEXT("Hero_Mage")).Traits = { TEXT("Trait_MageSpellReach") };
    DefaultHeroTraits.FindOrAdd(TEXT("Hero_Knight")).Traits = { TEXT("Trait_KnightTauntAura") };
    FireballSkillHeroIds = { TEXT("Hero_Mage") };
}

void ULKGameData::EnsureDefaultDecks()
{
	const int32 Removed = DefaultPlayerDeck.RemoveAll([](FName Id) { return !LKCardRules::IsPlayerObtainable(Id); });
	if (Removed > 0)
	{
		for (FName Id : LKCardRules::ReplacementCardIds())
		{
			if (DefaultPlayerDeck.Num() >= LKCardRules::MinimumCards) { break; }
			DefaultPlayerDeck.AddUnique(Id);
		}
	}
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
		AddCard(TEXT("Spell_HealWave"),    TEXT("治疗波"), 2, ELKCardType::Spell,    NAME_None,                 ELKSpellEffect::Heal,   120.f,  300.f);
		for (auto& C:CardLibrary) { if (C && C->CardId=="Spell_HealWave") { C->HeroHealPercent=.06f; } }
		AddCard(TEXT("Building_ArrowTower"), TEXT("箭塔"), 2, ELKCardType::Building, TEXT("Building_ArrowTower"), ELKSpellEffect::None, 0.f,   0.f);
		AddCard(TEXT("Building_Barracks"), TEXT("兵营"),   2, ELKCardType::Building, TEXT("Building_Barracks"), ELKSpellEffect::None,   0.f,   0.f);
		UE_LOG(LogLK, Log, TEXT("[GameData] 已注入 %d 张内置卡（未配置 CardLibrary）"), CardLibrary.Num());
	}

	// 敌方专属骷髅卡（保留卡库条目与召唤单位，但玩家不能获取/施放：见 LKCardRules 阵营权限）。
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
		UE_LOG(LogLK, Log, TEXT("[GameData] 注入敌方专属卡：%s（%s，1 费）"), *Entry.Key.ToString(), Entry.Value);
	}

	// 敌方专属战术法术【骷髅法阵】：5 银币、区域减速 + 召唤，玩家不可获取。
	{
		const FName CircleId = "Spell_SkeletonCircle";
		ULKCardDefinition* Existing = nullptr;
		for (TObjectPtr<ULKCardDefinition>& Entry : CardLibrary)
		{
			if (Entry && Entry->CardId == CircleId) { Existing = Entry.Get(); break; }
		}
		// 共享资产不能被就地改写：需要时先复制一份到本资产私有副本。
		ULKCardDefinition* Card = (Existing && Existing->GetOuter() != this)
			? DuplicateObject<ULKCardDefinition>(Existing, this) : Existing;
		if (!Card)
		{
			Card = NewObject<ULKCardDefinition>(this, CircleId);
			CardLibrary.Add(Card);
			UE_LOG(LogLK, Log, TEXT("[GameData] 注入敌方专属法术：%s（骷髅法阵，5 费）"), *CircleId.ToString());
		}
		else if (Card != Existing) { CardLibrary[CardLibrary.IndexOfByKey(Existing)] = Card; }
		Card->CardId = CircleId;
		Card->CardName = FText::FromString(TEXT("骷髅法阵"));
		Card->Description = FText::FromString(TEXT("敌方专属：区域减速并在 3 秒内召唤最多 6 名骷髅兵。"));
		Card->Cost = 5;
		Card->CardType = ELKCardType::Spell;
		Card->SpellEffect = ELKSpellEffect::SummonZone;
		Card->SpellValue = 0.f; // 与 Damage/Heal 的 SpellValue 通道独立：法阵本身无直接伤害。
		Card->SpellRadius = Card->Circle.Radius;
		Card->SpellGrade = ELKSpellGrade::Intermediate1; // 仅用于显示的中阶一级占位。
		Card->bExpeditionOnly = true;
		if (Card->Icon.IsNull()) { Card->Icon = LKBattleArt::CardIcon("Unit_Skeleton"); }
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
    LKResearchContent::EnsureCards(*this);
    for (TObjectPtr<ULKCardDefinition>& Entry : CardLibrary)
    {
        if (!Entry) { continue; }
        // 阵营规范身份由代码登记：数据资产即使被误改成 Both，也不会把敌方专属卡开放给玩家。
        if (LKCardRules::FactionOf(Entry->CardId) != Entry->Faction)
        {
            if (Entry->GetOuter() != this) { Entry = DuplicateObject<ULKCardDefinition>(Entry.Get(), this); }
            LKCardRules::ApplyCanonicalFaction(*Entry);
        }
        if (!Entry->Icon.IsNull()) { continue; }
        const TSoftObjectPtr<UTexture2D> DefaultIcon = LKBattleArt::CardIcon(Entry->CardId);
        if (DefaultIcon.IsNull()) { continue; }
        // The library may contain a shared authored asset. Never write presentation defaults into it.
        if (Entry->GetOuter() != this) { Entry = DuplicateObject<ULKCardDefinition>(Entry.Get(), this); }
        Entry->Icon = DefaultIcon;
    }
}
