#include "LKBattleArt.h"
#include "PaperSprite.h"
#include "Engine/Texture2D.h"

namespace LKBattleArt
{
const TArray<FName>& UnitIds()
{
    static const TArray<FName> Ids = {
        "Unit_Skeleton", "Unit_SkeletonArcher", "Hero_Necromancer", "Hero_SkeletonGiant", "Boss_SkeletonKing",
        "Unit_ElfArcher", "Unit_ElfWarrior", "Unit_ElfGuard", "Unit_ElfPriest", "Unit_GoblinRogue", "Unit_Thief",
        "Unit_ApprenticeMage", "Unit_GoblinBlade", "Unit_TrollWarrior", "Unit_TrollSpearman", "Unit_TrollMage",
        "Unit_TrollKing", "Unit_TwoHeadedDragon", "Building_SiegeCatapult", "Unit_Colossus"
    };
    return Ids;
}
TSoftObjectPtr<UPaperSprite> Sprite(FName UnitId)
{
    if (!UnitIds().Contains(UnitId)) { return nullptr; }
    const FString Name = TEXT("SP_") + UnitId.ToString();
    return TSoftObjectPtr<UPaperSprite>(FSoftObjectPath(TEXT("/Game/Art/StorybookV1/Battle/Sprites/") + Name + TEXT(".") + Name));
}
TSoftObjectPtr<UTexture2D> CardIcon(FName CardId)
{
    if (!UnitIds().Contains(CardId) || CardId.ToString().StartsWith(TEXT("Hero_")) || CardId.ToString().StartsWith(TEXT("Boss_"))) { return nullptr; }
    const FString Name = TEXT("T_Card_") + CardId.ToString();
    return TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Art/StorybookV1/Battle/Cards/") + Name + TEXT(".") + Name));
}
}
