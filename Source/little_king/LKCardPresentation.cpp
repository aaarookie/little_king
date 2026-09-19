#include "LKCardPresentation.h"
#include "LKUnitContent.h"
#include "ULKCardDefinition.h"
#include "LKCardRules.h"

namespace LKCardPresentation
{
FText QualityName(ELKQuality Quality)
{
    static const TCHAR* Names[] = { TEXT("普通"), TEXT("优秀"), TEXT("稀有"), TEXT("史诗"), TEXT("传说") };
    return FText::FromString(Names[FMath::Clamp(int32(Quality), 0, 4)]);
}
FLinearColor QualityColor(ELKQuality Quality)
{
    static const FLinearColor Colors[] = { {.9f,.92f,.95f}, {.32f,.88f,.49f}, {.3f,.64f,1.f}, {.76f,.46f,1.f}, {1.f,.57f,.18f} };
    return Colors[FMath::Clamp(int32(Quality), 0, 4)];
}
FText RaceName(ELKRace Race)
{
    static const TCHAR* Names[] = { TEXT("人类"), TEXT("精灵"), TEXT("哥布林"), TEXT("亡灵"), TEXT("建筑"), TEXT("巨魔"), TEXT("龙"), TEXT("构装体") };
    return FText::FromString(Names[FMath::Clamp(int32(Race), 0, 7)]);
}
FText SpellGradeName(ELKSpellGrade Grade)
{
    static const TCHAR* Names[] = { TEXT("初阶一级"), TEXT("初阶二级"), TEXT("初阶三级"),
        TEXT("中阶一级"), TEXT("中阶二级"), TEXT("中阶三级"), TEXT("高阶一级"), TEXT("高阶二级"), TEXT("高阶三级"),
        TEXT("神阶一级"), TEXT("神阶二级"), TEXT("神阶三级"), TEXT("神阶四级"), TEXT("神阶五级") };
    return FText::FromString(Names[FMath::Clamp(int32(Grade), 0, 13)]);
}
FText Classification(const ULKCardDefinition& Card)
{
    if (Card.CardType == ELKCardType::Spell) { return SpellGradeName(Card.SpellGrade); }
    const FLKUnitRow* Row = LKUnitContent::Find(Card.CardType == ELKCardType::Building ? Card.BuildingUnitId : Card.SpawnUnitId);
    return QualityName(Row ? Row->Quality : ELKQuality::Common);
}
FLinearColor Color(const ULKCardDefinition& Card)
{
    if (Card.CardType == ELKCardType::Spell) { return QualityColor(ELKQuality(FMath::Min(4, int32(Card.SpellGrade) / 3))); }
    const FLKUnitRow* Row = LKUnitContent::Find(Card.CardType == ELKCardType::Building ? Card.BuildingUnitId : Card.SpawnUnitId);
    return QualityColor(Row ? Row->Quality : ELKQuality::Common);
}
FText Label(const ULKCardDefinition& Card)
{
    return FText::FromString(FString::Printf(TEXT("%s · %s%s"), *Card.CardName.ToString(), *Classification(Card).ToString(),
        LKCardRules::Slots(Card.CardId) > 1 ? *FString::Printf(TEXT(" · %d格"), LKCardRules::Slots(Card.CardId)) : TEXT("")));
}
FText Detail(const ULKCardDefinition& Card, const FLKUnitRow* TunedRow)
{
    FString SkillText;
    FString Text = FString::Printf(TEXT("%s · %d 银币\n部队占格 %d%s"), *Classification(Card).ToString(), Card.Cost,
        LKCardRules::Slots(Card.CardId), Card.bExpeditionOnly ? TEXT(" · 远征临时") : TEXT(""));
    if (Card.CardType == ELKCardType::Spell)
    {
        Text += FString::Printf(TEXT("\n%s %.0f · 半径 %.0f"), Card.SpellEffect == ELKSpellEffect::Heal ? TEXT("治疗") : TEXT("伤害"), Card.SpellValue, Card.SpellRadius);
    }
    else if (const FLKUnitRow* Row = TunedRow ? TunedRow : LKUnitContent::Find(Card.CardType == ELKCardType::Building ? Card.BuildingUnitId : Card.SpawnUnitId))
    {
        const FString RangeText = Row->bTargetsBuildingsOnly ? TEXT("全图") : FString::Printf(TEXT("%.0f"), Row->AttackRange);
        Text += FString::Printf(TEXT("\n%s · %s\n基础生命 %.0f · 攻击 %.0f\n间隔 %.1f 秒 · 射程 %s"),
            *RaceName(Row->Race).ToString(), Row->AttackType == ELKAttackType::Melee ? TEXT("近战") : TEXT("远程"),
            Row->BaseHealth, Row->AttackDamage, Row->AttackInterval, *RangeText);
        switch (Row->PassiveAbility)
        {
        case ELKPassiveAbility::AttackRenewal:
            SkillText = FString::Printf(TEXT("普攻命中后恢复自身 %.0f%% 最大生命。"), Row->PassiveHealPercent * 100.f); break;
        case ELKPassiveAbility::SharedSpring:
            SkillText = TEXT("共沐春色：己方精灵实际回血时，为生命比例最低的存活己方英雄恢复等量生命。复制治疗不连锁。"); break;
        case ELKPassiveAbility::TauntImmunity: SkillText = TEXT("被动：免疫敌方嘲讽。"); break;
        case ELKPassiveAbility::Loot: SkillText = TEXT("战利品：自身当前攻击目标死亡时获得 1 银币，不要求最后一击。"); break;
        default: break;
        }
        if (Row->ActiveAbility == ELKActiveAbility::Backstab)
        {
            SkillText += FString::Printf(TEXT("\n背刺（%.0f秒）：闪现到最远敌人身后，造成 %.0f%% 攻击伤害；%.0f%% 概率获得 1 银币。锁定目标至下次施放，目标死亡刷新冷却。"),
                Row->SkillCooldown, Row->SkillDamageMultiplier * 100.f, Row->SkillSilverChance * 100.f);
        }
        else if (Row->ActiveAbility == ELKActiveAbility::MimicSpell)
        {
            SkillText += FString::Printf(TEXT("模仿施法（%.0f秒）：随机免费施放完整卡组中的一张初阶法术；没有则施放初阶火球。不消耗或轮换手牌。"), Row->SkillCooldown);
        }
        else if (Row->ActiveAbility == ELKActiveAbility::MakeWay)
        {
            SkillText += FString::Printf(TEXT("让开！（%.0f秒）：向前冲刺 %.0f，对路径内敌人各造成一次 %.0f%% 攻击伤害，向两侧击退 %.0f。建筑受伤但不被推走。"),
                Row->SkillCooldown, Row->SkillDashDistance, Row->SkillDamageMultiplier * 100.f, Row->SkillKnockbackDistance);
        }
        else if (Row->ActiveAbility == ELKActiveAbility::TrollEmpower)
        {
            SkillText = FString::Printf(TEXT("俺寻思这个魔法厉害！（%.0f秒）：强化自身和最近友方巨魔，优先巨魔王，持续 %.0f 秒。移速 +%.0f%%、攻击间隔缩短 %.0f%%、受到伤害 -20%%。每次出手为自身积累 25%% 眩晕值，满值眩晕 2 秒并清零。巨魔王免疫眩晕。"),
                Row->SkillCooldown, Row->EmpowerDuration, (Row->EmpowerMoveMultiplier - 1.f) * 100.f, (1.f - Row->EmpowerIntervalMultiplier) * 100.f);
        }
        if (Row->bTargetsBuildingsOnly) { Text += TEXT("\n攻击范围：全图 · 仅敌方建筑"); }
    }
    if (SkillText.IsEmpty()) { SkillText = Card.Description.ToString(); }
    if (!SkillText.IsEmpty()) { Text += TEXT("\n\n") + SkillText; }
    return FText::FromString(Text);
}
}
