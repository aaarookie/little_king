#pragma once
#include "CoreMinimal.h"
#include "LKTypes.h"
struct FLKRunCardState;
class ULKCardDefinition;

/**
 * Deck budget is independent from the four visible hand slots.
 * 卡牌阵营权限的规范身份也在本命名空间注册：数据资产被误改也不会开放敌方专属卡。
 */
namespace LKCardRules
{
    constexpr int32 Capacity = 8;
    constexpr int32 MinimumCards = 5;
    constexpr int32 MinimumDepartureCards = 7;

    int32 Slots(FName CardId);
    int32 Used(const TArray<FName>& Cards);
    int32 Used(const TArray<FLKRunCardState>& Cards);
    /** Fixed cap: smallest released budget, then fewest cards. Flexible min/max: any final budget in range. */
    bool ValidateReplacement(const TArray<FLKRunCardState>& Cards, FName Incoming,
        const TArray<FName>& Removed, int32 MinimumSlots, int32 MaximumSlots, FString& Error);
    bool IsTemporaryMercenary(FName CardId);
    bool IsSpell(FName CardId);
    bool IsBuilding(FName CardId);
    float UpgradeMultiplier(int32 Level);

    // ---------- 阵营权限（EnemyOnly / Both / PlayerOnly） ----------
    /** 代码注册的规范阵营；未注册的扩展卡一律 Both。 */
    ELKCardFaction FactionOf(FName CardId);
    bool IsEnemyOnly(FName CardId);
    /** 玩家能否获取/持有/施放：Both 与 PlayerOnly 为 true，EnemyOnly 为 false。 */
    bool IsPlayerObtainable(FName CardId);
    /** 敌方能否使用（卡库解析、遭遇配置、敌方战术通道）。 */
    bool IsEnemyUsable(FName CardId);
    const TArray<FName>& EnemyOnlyCardIds();
    /** 旧档禁用卡的确定性替代顺序（剑士、弓箭手、盾卫、火球、治疗波、箭塔、兵营）。 */
    const TArray<FName>& ReplacementCardIds();
    /** 把规范阵营写回运行时卡牌副本（不写资产）。 */
    void ApplyCanonicalFaction(ULKCardDefinition& Card);
    /** 过滤玩家可见/可获取的卡 ID 列表（保持输入顺序，去重）。 */
    void FilterPlayerObtainable(const TArray<FName>& In, TArray<FName>& Out);
}
