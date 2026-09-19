#include "LKExpeditionMercenaryContent.h"
#include "LKCardPresentation.h"

namespace LKExpeditionMercenaryContent
{
const TArray<FLKTemporaryMercenaryDefinition>& All()
{
    static const TArray<FLKTemporaryMercenaryDefinition> Definitions = []
    {
        TArray<FLKTemporaryMercenaryDefinition> Result;
        auto Add = [&Result](FName Id, const TCHAR* Name, ELKQuality Quality, ELKRace Race, ELKAttackType Attack,
            float Health, float Damage, float Interval, float Range, float Speed, int32 Cost, const TCHAR* Description) -> FLKUnitRow&
        {
            FLKTemporaryMercenaryDefinition& Definition = Result.AddDefaulted_GetRef();
            Definition.Cost = Cost; Definition.Description = FText::FromString(Description);
            FLKUnitRow& Row = Definition.Unit;
            Row.UnitId = Id; Row.DisplayName = FText::FromString(Name);
            Row.Quality = Quality; Row.Race = Race; Row.AttackType = Attack;
            Row.BaseHealth = Health; Row.AttackDamage = Damage; Row.AttackInterval = Interval;
            Row.AttackRange = Range; Row.MoveSpeed = Speed;
            Row.PlaceholderColor = LKCardPresentation::QualityColor(Quality);
            return Row;
        };
        FLKUnitRow& Archer = Add("Unit_ElfArcher", TEXT("精灵弓箭手"), ELKQuality::Uncommon, ELKRace::Elf, ELKAttackType::Ranged,
            110, 16, 1.2f, 450, 320, 3, TEXT("普通攻击命中后恢复自身 3% 最大生命。\n仅本次远征有效。"));
        Archer.PassiveAbility = ELKPassiveAbility::AttackRenewal; Archer.PassiveHealPercent = .03f;
        FLKUnitRow& Warrior = Add("Unit_ElfWarrior", TEXT("精灵战士"), ELKQuality::Uncommon, ELKRace::Elf, ELKAttackType::Melee,
            160, 20, 1, 120, 300, 3, TEXT("普通攻击命中后恢复自身 4% 最大生命。\n仅本次远征有效。"));
        Warrior.PassiveAbility = ELKPassiveAbility::AttackRenewal; Warrior.PassiveHealPercent = .04f;
        FLKUnitRow& Guard = Add("Unit_ElfGuard", TEXT("精灵盾卫"), ELKQuality::Uncommon, ELKRace::Elf, ELKAttackType::Melee,
            300, 10, 1.2f, 120, 240, 3, TEXT("普通攻击命中后恢复自身 5% 最大生命。\n仅本次远征有效。"));
        Guard.PassiveAbility = ELKPassiveAbility::AttackRenewal; Guard.PassiveHealPercent = .05f;
        FLKUnitRow& Priest = Add("Unit_ElfPriest", TEXT("精灵牧师"), ELKQuality::Rare, ELKRace::Elf, ELKAttackType::Ranged,
            110, 16, 1.2f, 450, 300, 4, TEXT("共沐春色：己方精灵实际回血时，为生命比例最低的存活己方英雄恢复等量生命。复制治疗不连锁。\n基础数值：优秀。仅本次远征有效。"));
        Priest.PassiveAbility = ELKPassiveAbility::SharedSpring;
        FLKUnitRow& Rogue = Add("Unit_GoblinRogue", TEXT("哥布林大盗"), ELKQuality::Rare, ELKRace::Goblin, ELKAttackType::Melee,
            160, 20, 1, 120, 330, 4, TEXT("被动：免疫嘲讽。\n背刺（8秒）：闪现到最远敌人身后，造成 300% 攻击伤害，30% 概率获得 1 银币。锁定该目标，目标死亡刷新技能。\n基础数值：优秀。仅本次远征有效。"));
        Rogue.PassiveAbility = ELKPassiveAbility::TauntImmunity;
        Rogue.ActiveAbility = ELKActiveAbility::Backstab; Rogue.SkillCooldown = 8; Rogue.SkillDamageMultiplier = 3;
        FLKUnitRow& Thief = Add("Unit_Thief", TEXT("窃贼"), ELKQuality::Uncommon, ELKRace::Human, ELKAttackType::Melee,
            120, 22, 1, 120, 330, 3, TEXT("战利品：自身当前攻击目标死亡时，获得 1 银币，不要求最后一击。\n生命数值：普通；攻击略高于优秀。仅本次远征有效。"));
        Thief.PassiveAbility = ELKPassiveAbility::Loot;
        FLKUnitRow& Apprentice = Add("Unit_ApprenticeMage", TEXT("学徒法师"), ELKQuality::Uncommon, ELKRace::Human, ELKAttackType::Ranged,
            110, 8, 1.3f, 450, 280, 3, TEXT("模仿施法（10秒）：随机免费施放完整卡组中的一张初阶法术；没有则施放初阶火球。不会消耗或轮换手牌。\n攻击数值低于普通。仅本次远征有效。"));
        Apprentice.ActiveAbility = ELKActiveAbility::MimicSpell; Apprentice.SkillCooldown = 10;
        FLKUnitRow& Blade = Add("Unit_GoblinBlade", TEXT("开道利刃"), ELKQuality::Rare, ELKRace::Goblin, ELKAttackType::Melee,
            220, 25, 1, 130, 340, 4, TEXT("让开！（5秒）：向前冲刺，对路径内敌人各造成一次 200% 攻击伤害，并向两侧击退。建筑受伤但不被推走。\n仅本次远征有效。"));
        Blade.ActiveAbility = ELKActiveAbility::MakeWay; Blade.SkillCooldown = 5;
        FLKUnitRow& TrollWarrior = Add("Unit_TrollWarrior", TEXT("巨魔战士"), ELKQuality::Uncommon, ELKRace::Troll, ELKAttackType::Melee,
            220, 25, 1.05f, 130, 270, 4, TEXT("稀有基础数值，占用 2 格部队容量。仅本次远征有效。"));
        TrollWarrior.DeckSlots = 2;
        FLKUnitRow& Spearman = Add("Unit_TrollSpearman", TEXT("巨魔投矛手"), ELKQuality::Uncommon, ELKRace::Troll, ELKAttackType::Ranged,
            145, 42, 2, 600, 250, 4, TEXT("稀有基础数值，攻击较慢、单次伤害高。占用 2 格部队容量。仅本次远征有效。"));
        Spearman.DeckSlots = 2;
        FLKUnitRow& TrollMage = Add("Unit_TrollMage", TEXT("巨魔法师"), ELKQuality::Rare, ELKRace::Troll, ELKAttackType::Melee,
            220, 25, 1.2f, 130, 250, 5, TEXT("巨魔中罕见的有强大的智慧能学习魔法！占用 2 格部队容量。仅本次远征有效。"));
        TrollMage.DeckSlots = 2; TrollMage.ActiveAbility = ELKActiveAbility::TrollEmpower; TrollMage.SkillCooldown = 20;
        FLKUnitRow& King = Add("Unit_TrollKing", TEXT("巨魔王"), ELKQuality::Epic, ELKRace::Troll, ELKAttackType::Melee,
            380, 40, 1.15f, 150, 250, 7, TEXT("巨魔中最强大的家伙！巨魔王！：免疫眩晕；存活友方巨魔战士使最大生命 +20% 并保持生命比例，投矛手使攻击 +20%，同类不叠加。\n杀吧！：每第六次攻击造成 150% 伤害、击退 3 米并眩晕 2 秒。占 2 格。仅本次远征有效。"));
        King.DeckSlots = 2; King.PassiveAbility = ELKPassiveAbility::TrollKing;
        FLKUnitRow& Dragon = Add("Unit_TwoHeadedDragon", TEXT("双头龙"), ELKQuality::Epic, ELKRace::Dragon, ELKAttackType::Ranged,
            225, 38, 2.2f, 650, 260, 6, TEXT("双头吐息：每次随机冰/火。各龙共享目标历史：冰冰冻结 1 秒，火火叠加点燃并刷新至 3 秒（每秒每层 1% 最大生命），冰火交替造成 120% 伤害。首次命中无额外效果。仅本次远征有效。"));
        Dragon.PassiveAbility = ELKPassiveAbility::TwinBreath;
        FLKUnitRow& Catapult = Add("Building_SiegeCatapult", TEXT("攻城投石炮"), ELKQuality::Uncommon, ELKRace::None, ELKAttackType::Ranged,
            320, 28, 3, 100000, 0, 4, TEXT("全图抛射，仅攻击敌方建筑；无视其他单位的嘲讽，不会误击拦路佣兵。营地不可攻击。仅本次远征有效。"));
        Catapult.UnitClass = ELKUnitClass::Building; Catapult.BuildingBehavior = ELKBuildingBehavior::Turret;
        Catapult.bTargetsBuildingsOnly = true; Catapult.AcquireRadius = 100000;
        FLKUnitRow& Colossus = Add("Unit_Colossus", TEXT("擎天巨像"), ELKQuality::Epic, ELKRace::Construct, ELKAttackType::Melee,
            650, 70, 2, 160, 100, 12, TEXT("传说基础数值，移动非常缓慢。占用 3 格部队容量，12 银币。仅本次远征有效。"));
        Colossus.DeckSlots = 3;
        return Result;
    }();
    return Definitions;
}
bool IsTemporary(FName CardId)
{
    return All().ContainsByPredicate([CardId](const FLKTemporaryMercenaryDefinition& Definition) { return Definition.Unit.UnitId == CardId; });
}
}
