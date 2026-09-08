#include "LKUndeadContent.h"

namespace LKUndeadContent
{
const TMap<FName, FLKUnitRow>& Units()
{
    static const TMap<FName, FLKUnitRow> Rows = []
    {
        TMap<FName, FLKUnitRow> Result;
        auto Add = [&Result](FName Id, const TCHAR* Name, ELKUnitClass Class, ELKAttackType Attack, float HP, float Damage, float Range, float Interval, float Speed, FLinearColor Color) -> FLKUnitRow&
        {
            FLKUnitRow& Row = Result.Add(Id);
            Row.UnitId = Id; Row.DisplayName = FText::FromString(Name); Row.UnitClass = Class;
            Row.AttackType = Attack; Row.BaseHealth = HP; Row.AttackDamage = Damage;
            Row.AttackRange = Range; Row.AttackInterval = Interval; Row.MoveSpeed = Speed;
            Row.PlaceholderColor = Color; Row.bSkeleton = true;
            return Row;
        };
        Add("Unit_Skeleton", TEXT("骷髅兵"), ELKUnitClass::Soldier, ELKAttackType::Melee, 65.f, 6.f, 150.f, 1.3f, 240.f, FLinearColor(0.8f, 0.8f, 0.65f));
        Add("Unit_SkeletonArcher", TEXT("骷髅射手"), ELKUnitClass::Soldier, ELKAttackType::Ranged, 45.f, 7.f, 400.f, 1.6f, 220.f, FLinearColor(0.45f, 0.7f, 0.75f));
        FLKUnitRow& Necro = Add("Hero_Necromancer", TEXT("死灵法师"), ELKUnitClass::Hero, ELKAttackType::Ranged, 180.f, 9.f, 480.f, 1.8f, 190.f, FLinearColor(0.7f, 0.3f, 0.9f));
        Necro.bSkeleton = false; Necro.PassiveAbility = ELKPassiveAbility::UndeadSummoning;
        Necro.HeroTraits = { "Trait_Sacrifice" };
        FLKUnitRow& Giant = Add("Hero_SkeletonGiant", TEXT("骷髅巨人"), ELKUnitClass::Hero, ELKAttackType::Melee, 280.f, 13.f, 160.f, 1.9f, 170.f, FLinearColor(0.55f, 0.75f, 0.25f));
        Giant.PassiveAbility = ELKPassiveAbility::GiantBones;
        FLKUnitRow& King = Add("Boss_SkeletonKing", TEXT("骷髅王"), ELKUnitClass::Boss, ELKAttackType::Melee, 420.f, 18.f, 170.f, 1.7f, 185.f, FLinearColor(0.95f, 0.65f, 0.15f));
        King.PassiveAbility = ELKPassiveAbility::BoneRegeneration;
        King.HeroTraits = { "Trait_FaceFear" };
        return Result;
    }();
    return Rows;
}

}
