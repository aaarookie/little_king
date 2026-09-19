#include "LKUnitContent.h"

#include "LKUndeadContent.h"
#include "LKExpeditionMercenaryContent.h"

namespace LKUnitContent
{
const TMap<FName, FLKUnitRow>& Units()
{
    static const TMap<FName, FLKUnitRow> Rows = []
    {
        TMap<FName, FLKUnitRow> Result;
        auto Add = [&Result](FName Id, const TCHAR* Name, ELKUnitClass Class, ELKAttackType Attack,
            float Health, float Damage, float Range, float Interval, float Speed) -> FLKUnitRow&
        {
            FLKUnitRow& Row = Result.Add(Id);
            Row.UnitId = Id;
            Row.DisplayName = FText::FromString(Name);
            Row.UnitClass = Class;
            Row.AttackType = Attack;
            Row.BaseHealth = Health;
            Row.AttackDamage = Damage;
            Row.AttackRange = Range;
            Row.AttackInterval = Interval;
            Row.MoveSpeed = Speed;
            Row.AttackWindup = 0.15f;
            return Row;
        };

        FLKUnitRow& Knight = Add("Hero_Knight", TEXT("骑士"), ELKUnitClass::Hero, ELKAttackType::Melee,
            450.f, 25.f, 130.f, 1.f, 300.f);
        Knight.SkillCooldown = 8.f;

        FLKUnitRow& Mage = Add("Hero_Mage", TEXT("法师"), ELKUnitClass::Hero, ELKAttackType::Ranged,
            300.f, 20.f, 500.f, 1.3f, 280.f);
        Mage.SkillCooldown = 6.f;
        Mage.bIsMage = true; // 旧查询兼容；施法范围仍由 Trait_MageSpellReach 决定。

        FLKUnitRow& Ranger = Add("Hero_Ranger", TEXT("游侠"), ELKUnitClass::Hero, ELKAttackType::Ranged,
            350.f, 22.f, 400.f, 1.1f, 340.f);
        Ranger.SkillCooldown = 5.f;

        Add("Unit_Swordsman", TEXT("剑士"), ELKUnitClass::Soldier, ELKAttackType::Melee,
            120.f, 15.f, 120.f, 1.f, 300.f);
        Add("Unit_Archer", TEXT("弓箭手"), ELKUnitClass::Soldier, ELKAttackType::Ranged,
            80.f, 12.f, 450.f, 1.2f, 320.f);
        FLKUnitRow& Shieldbearer = Add("Unit_Shieldbearer", TEXT("盾卫"), ELKUnitClass::Soldier, ELKAttackType::Melee,
            220.f, 8.f, 120.f, 1.2f, 240.f);
        Shieldbearer.HeroTraits = { "Taunt" };

        FLKUnitRow& Tower = Add("Building_ArrowTower", TEXT("箭塔"), ELKUnitClass::Building, ELKAttackType::Ranged,
            600.f, 18.f, 500.f, 1.5f, 0.f);
        Tower.BuildingBehavior = ELKBuildingBehavior::Turret;

        FLKUnitRow& Barracks = Add("Building_Barracks", TEXT("兵营"), ELKUnitClass::Building, ELKAttackType::Melee,
            500.f, 10.f, 150.f, 1.f, 0.f);
        Barracks.BuildingBehavior = ELKBuildingBehavior::Barracks;
        Barracks.SpawnUnitId = "Unit_Swordsman";
        Barracks.SpawnInterval = 8.f;

        for (const TPair<FName, FLKUnitRow>& Pair : LKUndeadContent::Units())
        {
            Result.Add(Pair.Key, Pair.Value);
        }
        for (TPair<FName, FLKUnitRow>& Pair : Result)
        {
            FLKUnitRow& Row = Pair.Value;
            const bool bUndead = LKUndeadContent::Units().Contains(Pair.Key);
            Row.Race = bUndead ? ELKRace::Undead : (Row.UnitClass == ELKUnitClass::Building ? ELKRace::None : ELKRace::Human);
            if (Row.UnitClass == ELKUnitClass::Hero) { Row.Quality = bUndead ? ELKQuality::Rare : ELKQuality::Epic; }
            if (Row.UnitClass == ELKUnitClass::Boss) { Row.Quality = ELKQuality::Epic; }
            if (Pair.Key == "Unit_Shieldbearer" || Pair.Key == "Building_Barracks") { Row.Quality = ELKQuality::Uncommon; }
        }
        for (const FLKTemporaryMercenaryDefinition& Definition : LKExpeditionMercenaryContent::All())
        { Result.Add(Definition.Unit.UnitId, Definition.Unit); }
        return Result;
    }();
    return Rows;
}

const FLKUnitRow* Find(FName UnitId)
{
    return Units().Find(UnitId);
}

FLKUnitRow MergeAuthoredTuning(const FLKUnitRow& Canonical, const FLKUnitRow* Authored)
{
    FLKUnitRow Result = Authored ? *Authored : Canonical;
    if (Result.DisplayName.IsEmpty()) { Result.DisplayName = Canonical.DisplayName; }

    // 以下字段决定类、攻击管线、被动和建筑逻辑，不能因复制/漏填表行而漂移。
    Result.UnitId = Canonical.UnitId;
    Result.AttackType = Canonical.AttackType;
    Result.ProjectileId = Canonical.ProjectileId;
    Result.UnitClass = Canonical.UnitClass;
    Result.Quality = Canonical.Quality;
    Result.Race = Canonical.Race;
    Result.ActiveAbility = Canonical.ActiveAbility;
    Result.DeckSlots = Canonical.DeckSlots;
    Result.bTargetsBuildingsOnly = Canonical.bTargetsBuildingsOnly;
    if (Canonical.ActiveAbility != ELKActiveAbility::None && Result.SkillCooldown <= 0.f) { Result.SkillCooldown = Canonical.SkillCooldown; }
    Result.bSkeleton = Canonical.bSkeleton;
    Result.PassiveAbility = Canonical.PassiveAbility;
    Result.PassiveHealPercent = Canonical.PassiveHealPercent;
    Result.RevivalInitialThreshold = Canonical.RevivalInitialThreshold;
    Result.RevivalThresholdStep = Canonical.RevivalThresholdStep;
    Result.PlaceholderColor = Canonical.PlaceholderColor;
    Result.HeroTraits = Canonical.HeroTraits;
    Result.bIsMage = Canonical.bIsMage;
    Result.BuildingBehavior = Canonical.BuildingBehavior;
    Result.SpawnUnitId = Canonical.SpawnUnitId;
    return Result;
}
}
