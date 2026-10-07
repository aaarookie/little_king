#include "LKV084Content.h"

#include "ULKCardDefinition.h"
#include "ULKGameData.h"
#include "Engine/Texture2D.h"

namespace
{
struct FBuildingCard
{
    FName CardId;
    FName UnitId;
    const TCHAR* Name;
    int32 Cost;
    const TCHAR* Description;
};
const TArray<FBuildingCard>& Definitions()
{
    static const TArray<FBuildingCard> Rows = {
        {"Card_WallCommon", "Unit_WallCommon", TEXT("普通防御墙"), 3,
            TEXT("Q / E 旋转；允许墙体部分重叠。施工完成后阻挡移动，目标被完全包围时敌人先破墙。位置、朝向非常贴近旧墙时，品质不高于旧墙则修复新墙最大生命量；品质更高则替换并重新施工。修复不超过旧墙最大生命。")},
        {"Card_WallRare", "Unit_WallRare", TEXT("稀有防御墙"), 5,
            TEXT("Q / E 旋转；允许墙体部分重叠。施工完成后阻挡移动。贴合旧墙时，品质不高于旧墙则修复新墙最大生命量；品质更高则替换并重新施工。休息升级只强化数值，不提升品质。")},
        {"Card_WallEpic", "Unit_WallEpic", TEXT("史诗防御墙"), 7,
            TEXT("继承防御墙旋转、重叠、修复与替换规则。受到攻击时反弹伤害给伤害来源；每个来源独立冷却 0.5 秒，反弹不会再次触发反弹。")},
        {"Card_WallImperial", "Unit_WallImperial", TEXT("禁军之墙"), 10,
            TEXT("继承防御墙及反弹规则。建成后，墙前 3 米友军攻击 +10%，墙后 3 米友军每秒恢复 1% 最大生命。多个墙的攻击加成按百分比相加，治疗分别结算；同一单位可同时享有不同墙的前后增益。")},
        {"Card_SiegeCart", "Unit_SiegeCart", TEXT("攻城车"), 6,
            TEXT("可移动建筑：寻找最近敌方可攻击建筑，接近后每 3 秒造成其 10% 最大生命伤害；对防御墙为 20%。没有敌方建筑时停留原地。营地不可攻击，不攻击英雄或佣兵。")},
        {"Card_MageTower", "Unit_MageTower", TEXT("法师塔"), 8,
            TEXT("接纳一名己方法师：优先英雄，其次品质最高的佣兵，再比较银币费用，同级同费随机选择。驻塔法师受到庇护，敌人必须先摧毁塔；主动技能冷却缩短 50%。没有可入住法师时不产生战斗效果。")},
        {"Card_HealingStation", "Unit_HealingStation", TEXT("牧师治疗站"), 5,
            TEXT("接纳一名己方牧师：优先英雄，其次最高品质佣兵，再比较费用，同级同费随机。驻站牧师受到庇护。每次接诊一名生命低于 50% 且比例最低的己方英雄或佣兵，患者停止战斗并前来治疗，每次恢复牧师通常治疗量的 1.5 倍，满血后重新参战。没有牧师时不接诊。")},
    };
    return Rows;
}
}

namespace LKV084Content
{
const TMap<FName, FLKUnitRow>& Units()
{
    static const TMap<FName, FLKUnitRow> Rows = []
    {
        TMap<FName, FLKUnitRow> Result;
        auto Add = [&Result](FName Id, const TCHAR* Name, ELKQuality Quality, ELKBuildingBehavior Behavior, float Health) -> FLKUnitRow&
        {
            FLKUnitRow& Row = Result.Add(Id);
            Row.UnitId = Id; Row.DisplayName = FText::FromString(Name);
            Row.UnitClass = ELKUnitClass::Building; Row.Race = ELKRace::None; Row.Quality = Quality;
            Row.BuildingBehavior = Behavior; Row.BaseHealth = Health; Row.MoveSpeed = 0.f;
            Row.AttackDamage = 0.f; Row.AttackRange = 0.f; Row.AcquireRadius = 0.f;
            Row.AttackType = ELKAttackType::Melee; Row.AttackInterval = 1.f;
            Row.PlaceholderColor = FLinearColor(.65f, .57f, .43f, 1.f);
            return Row;
        };
        FLKUnitRow& Common = Add("Unit_WallCommon", TEXT("普通防御墙"), ELKQuality::Common, ELKBuildingBehavior::Wall, 1000.f);
        Common.ConstructionSeconds = 10.f;
        FLKUnitRow& Rare = Add("Unit_WallRare", TEXT("稀有防御墙"), ELKQuality::Rare, ELKBuildingBehavior::Wall, 5000.f);
        Rare.ConstructionSeconds = 15.f;
        FLKUnitRow& Epic = Add("Unit_WallEpic", TEXT("史诗防御墙"), ELKQuality::Epic, ELKBuildingBehavior::Wall, 10000.f);
        Epic.ConstructionSeconds = 20.f; Epic.ReflectDamage = 10.f;
        FLKUnitRow& Imperial = Add("Unit_WallImperial", TEXT("禁军之墙"), ELKQuality::Legendary, ELKBuildingBehavior::Wall, 20000.f);
        Imperial.ConstructionSeconds = 20.f; Imperial.ReflectDamage = 50.f;
        FLKUnitRow& Cart = Add("Unit_SiegeCart", TEXT("攻城车"), ELKQuality::Rare, ELKBuildingBehavior::SiegeCart, 320.f);
        Cart.MoveSpeed = 30.f; Cart.AttackInterval = 3.f; Cart.AttackRange = 160.f;
        Cart.AcquireRadius = 100000.f; Cart.AttackWindup = .35f; Cart.bTargetsBuildingsOnly = true;
        Add("Unit_MageTower", TEXT("法师塔"), ELKQuality::Epic, ELKBuildingBehavior::MageTower, 5000.f);
        Add("Unit_HealingStation", TEXT("牧师治疗站"), ELKQuality::Rare, ELKBuildingBehavior::HealingStation, 2000.f);
        return Result;
    }();
    return Rows;
}

const TArray<FName>& CardIds()
{
    static const TArray<FName> Ids = []
    { TArray<FName> Result; for (const auto& D : Definitions()) { Result.Add(D.CardId); } return Result; }();
    return Ids;
}

FName UnitIdForCard(FName CardId)
{
    for (const auto& D : Definitions()) { if (D.CardId == CardId) { return D.UnitId; } }
    return NAME_None;
}

bool ConfigureCard(ULKCardDefinition& Card, bool bInitializeTuning)
{
    const auto* D = Definitions().FindByPredicate([&Card](const auto& Row) { return Row.CardId == Card.CardId; });
    if (!D) { return false; }
    Card.CardName = FText::FromString(D->Name); Card.Description = FText::FromString(D->Description);
    Card.CardType = ELKCardType::Building; Card.BuildingUnitId = D->UnitId; Card.SpawnUnitId = NAME_None;
    Card.bExpeditionOnly = false; Card.SpellEffect = ELKSpellEffect::None; Card.bGlobalPlacement = false;
    Card.DrawCooldown = 0.f; Card.SummonedUnitIds.Reset();
    if (bInitializeTuning)
    {
        Card.Cost = D->Cost;
        // 连续墙段不能被旧的同类建筑上限拦住；作者可显式覆盖限制。
        Card.BuildingTypeLimitOverride = Units().FindChecked(D->UnitId).BuildingBehavior == ELKBuildingBehavior::Wall ? -1 : -2;
    }
    if (Card.Icon.IsNull())
    {
        const FString Name = TEXT("T_") + Card.CardId.ToString();
        Card.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Art/StorybookV1/V084/Cards/") + Name + TEXT(".") + Name));
    }
    return true;
}

void EnsureCards(ULKGameData& Data)
{
    for (const auto& D : Definitions())
    {
        const int32 Index = Data.CardLibrary.IndexOfByPredicate([&D](const auto& Card) { return Card && Card->CardId == D.CardId; });
        ULKCardDefinition* Card = Index == INDEX_NONE ? NewObject<ULKCardDefinition>(&Data) : DuplicateObject<ULKCardDefinition>(Data.CardLibrary[Index].Get(), &Data);
        Card->CardId = D.CardId; ConfigureCard(*Card, Index == INDEX_NONE);
        if (Index == INDEX_NONE) { Data.CardLibrary.Add(Card); } else { Data.CardLibrary[Index] = Card; }
    }
}
}
