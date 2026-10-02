#include "LKV083Content.h"

#include "ULKCardDefinition.h"
#include "Engine/Texture2D.h"

namespace LKV083Content
{
const TMap<FName, FLKUnitRow>& Units()
{
    static const TMap<FName, FLKUnitRow> Rows = []
    {
        TMap<FName, FLKUnitRow> Result;
        auto Add = [&Result](FName Id, const TCHAR* Name, ELKQuality Quality, ELKAttackType Attack,
            float Health, float Power, float Interval, float Range, float Speed, bool bHealer = false)
        {
            FLKUnitRow& Row = Result.Add(Id);
            Row.UnitId = Id; Row.DisplayName = FText::FromString(Name);
            Row.UnitClass = ELKUnitClass::Soldier; Row.Quality = Quality; Row.Race = ELKRace::Angel;
            Row.AttackType = Attack; Row.BaseHealth = Health; Row.AttackDamage = Power;
            Row.AttackInterval = Interval; Row.AttackRange = Range; Row.MoveSpeed = Speed;
            Row.AttackWindup = .15f; Row.bBasicAttackHeals = bHealer;
            Row.PlaceholderColor = FLinearColor(.88f, .82f, .58f, 1.f);
        };
        // 与既有品质基线一致；牧师的 AttackDamage 表示单次基础治疗量。
        Add("Unit_AngelWarrior_Uncommon", TEXT("战斗天使"), ELKQuality::Uncommon, ELKAttackType::Melee, 160, 20, 1, 130, 300);
        Add("Unit_AngelArcher_Uncommon", TEXT("弓箭手天使"), ELKQuality::Uncommon, ELKAttackType::Ranged, 110, 16, 1.2f, 450, 300);
        Add("Unit_AngelPriest_Uncommon", TEXT("牧师天使"), ELKQuality::Uncommon, ELKAttackType::Ranged, 110, 16, 1.5f, 500, 280, true);
        Add("Unit_AngelWarrior_Rare", TEXT("战斗天使"), ELKQuality::Rare, ELKAttackType::Melee, 220, 25, 1, 130, 300);
        Add("Unit_AngelArcher_Rare", TEXT("弓箭手天使"), ELKQuality::Rare, ELKAttackType::Ranged, 150, 22, 1.2f, 500, 300);
        Add("Unit_AngelPriest_Rare", TEXT("牧师天使"), ELKQuality::Rare, ELKAttackType::Ranged, 150, 24, 1.5f, 550, 280, true);
        Add("Unit_AngelWarrior_Epic", TEXT("战斗天使"), ELKQuality::Epic, ELKAttackType::Melee, 320, 34, 1, 150, 300);
        Add("Unit_AngelArcher_Epic", TEXT("弓箭手天使"), ELKQuality::Epic, ELKAttackType::Ranged, 225, 32, 1.3f, 550, 300);
        Add("Unit_AngelPriest_Epic", TEXT("牧师天使"), ELKQuality::Epic, ELKAttackType::Ranged, 200, 36, 1.5f, 600, 280, true);
        Add("Unit_DivineJudge", TEXT("圣玛丽亚的神圣审判者"), ELKQuality::Legendary, ELKAttackType::Melee, 540, 60, 1.2f, 160, 280);
        Add("Unit_ChosenHighPriest", TEXT("圣玛丽亚的神选牧师长"), ELKQuality::Legendary, ELKAttackType::Ranged, 350, 65, 1.8f, 650, 280, true);
        return Result;
    }();
    return Rows;
}

const TArray<FName>& SpellIds()
{
    static const TArray<FName> Ids = { "Spell_Freeze", "Spell_MariaNovice", "Spell_MariaIntermediate",
        "Spell_MariaAdvanced", "Spell_MariaDivine", "Spell_BlackCloud", "Spell_Lightning", "Spell_Hurricane", "Spell_DivineBlessing" };
    return Ids;
}

bool ConfigureCard(ULKCardDefinition& Card, bool bInitializeTuning)
{
    if (!SpellIds().Contains(Card.CardId)) { return false; }
    const FName Id = Card.CardId;
    Card.CardType = ELKCardType::Spell; Card.bExpeditionOnly = false;
    Card.SpawnUnitId = NAME_None; Card.BuildingUnitId = NAME_None;
    Card.SummonedUnitIds.Reset(); Card.bGlobalPlacement = false;
    if (Id == "Spell_Freeze")
    {
        Card.SpellEffect = ELKSpellEffect::Freeze;
        Card.Description = FText::FromString(TEXT("冻结范围内敌人，不能移动、攻击或释放主动技能。远征升级增加冻结时长。"));
        if (bInitializeTuning) { Card.EffectDuration = 3.f; }
    }
    else if (Id == "Spell_BlackCloud")
    {
        Card.SpellEffect = ELKSpellEffect::BlackCloud;
        Card.Description = FText::FromString(TEXT("黑云内双方单位不能被单位发现；攻击者重新索敌，离开黑云立即恢复可发现。地面法术仍可影响区域内单位。远征升级增加持续时间。"));
        if (bInitializeTuning) { Card.EffectDuration = 5.f; }
    }
    else if (Id == "Spell_Lightning")
    {
        Card.SpellEffect = ELKSpellEffect::Lightning;
        Card.Description = FText::FromString(TEXT("只打击敌方非英雄：先打击当前生命最高者，延迟后重新选择当前生命最低者。远征升级增加伤害上限和第二击伤害，不提高第一击的生命比例。"));
        if (bInitializeTuning) { Card.MaxHealthDamageFraction = .2f; Card.SecondarySpellValue = 100.f; Card.SecondaryDelay = .2f; }
    }
    else if (Id == "Spell_Hurricane")
    {
        Card.SpellEffect = ELKSpellEffect::Hurricane;
        Card.Description = FText::FromString(TEXT("矩形范围内双方可移动角色被持续推向战场右侧；建筑与营地保持固定。远征升级增加推移速度，不扩大范围或延长时间。"));
        if (bInitializeTuning) { Card.EffectDuration = 5.f; Card.SpellHalfExtents = FVector2D(650.f, 350.f); Card.ForceMoveSpeed = 220.f; }
    }
    else if (Id == "Spell_DivineBlessing")
    {
        Card.SpellEffect = ELKSpellEffect::DivineBlessing; Card.bGlobalPlacement = true;
        Card.Description = FText::FromString(TEXT("恢复场上己方存活单位；失能英雄不能战内救起。施放后 150 秒内不会重新抽到，该冷却按战斗时间计算，每场重置。远征升级只增加英雄恢复比例，上限 100%。"));
        if (bInitializeTuning) { Card.DrawCooldown = 150.f; }
    }
    else
    {
        Card.SpellEffect = ELKSpellEffect::Reinforcements; Card.bGlobalPlacement = true;
        if (Id == "Spell_MariaDivine")
        {
            Card.SummonedUnitIds = { "Unit_DivineJudge", "Unit_ChosenHighPriest" };
            Card.Description = FText::FromString(TEXT("可在战场任意合法地点召唤传说神圣审判者与神选牧师长；牧师长普攻治疗受伤友军。两者没有额外技能或特性，仅能通过本法术召唤。远征升级增加召唤者的生命和攻击／治疗量。"));
        }
        else
        {
            const FString Suffix = Id == "Spell_MariaNovice" ? TEXT("Uncommon") : (Id == "Spell_MariaIntermediate" ? TEXT("Rare") : TEXT("Epic"));
            Card.SummonedUnitIds = { FName(*(TEXT("Unit_AngelWarrior_") + Suffix)), FName(*(TEXT("Unit_AngelArcher_") + Suffix)), FName(*(TEXT("Unit_AngelPriest_") + Suffix)) };
            Card.Description = FText::FromString(TEXT("可在战场任意合法地点召唤战斗天使、弓箭手天使与牧师天使；牧师普攻治疗受伤友军。天使没有额外技能或特性，仅能通过增援法术召唤。远征升级增加召唤者的生命和攻击／治疗量。"));
        }
    }
    if (Card.Icon.IsNull())
    {
        const FString Name = TEXT("T_Card_") + Id.ToString();
        Card.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Art/StorybookV1/V083/Cards/") + Name + TEXT(".") + Name));
    }
    return true;
}
}
