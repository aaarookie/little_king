#include "LKV084Art.h"
#include "PaperSprite.h"
#include "PaperFlipbook.h"
#include "Engine/Texture2D.h"

namespace LKV084Art
{
const TArray<FName>& UnitIds()
{
    static const TArray<FName> Ids = { "Unit_WallCommon", "Unit_WallRare", "Unit_WallEpic", "Unit_WallImperial",
        "Unit_SiegeCart", "Unit_MageTower", "Unit_HealingStation" };
    return Ids;
}
const TArray<FName>& CardIds()
{
    static const TArray<FName> Ids = { "Card_WallCommon", "Card_WallRare", "Card_WallEpic", "Card_WallImperial",
        "Card_SiegeCart", "Card_MageTower", "Card_HealingStation" };
    return Ids;
}
FName Identity(FName Id)
{
    if (!UnitIds().Contains(Id) && !CardIds().Contains(Id)) { return NAME_None; }
    return FName(*Id.ToString().Mid(5));
}
TSoftObjectPtr<UPaperSprite> Sprite(FName UnitId)
{
    if (!UnitIds().Contains(UnitId)) { return nullptr; }
    const FString Name = TEXT("SP_") + Identity(UnitId).ToString() + TEXT("_00");
    return TSoftObjectPtr<UPaperSprite>(FSoftObjectPath(TEXT("/Game/Art/StorybookV1/V084/Animations/") + Name + TEXT(".") + Name));
}
TSoftObjectPtr<UTexture2D> CardIcon(FName CardId)
{
    if (!CardIds().Contains(CardId)) { return nullptr; }
    const FString Name = TEXT("T_") + CardId.ToString();
    return TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Art/StorybookV1/V084/Cards/") + Name + TEXT(".") + Name));
}
UPaperFlipbook* Animation(FName UnitId, FName State)
{
    if (!UnitIds().Contains(UnitId) || (State != "Idle" && State != "Move" && State != "Attack" && State != "Hit" && State != "Death")) { return nullptr; }
    const FString Name = TEXT("FB_") + Identity(UnitId).ToString() + TEXT("_") + State.ToString();
    return LoadObject<UPaperFlipbook>(nullptr, *(TEXT("/Game/Art/StorybookV1/V084/Animations/") + Name + TEXT(".") + Name), nullptr, LOAD_NoWarn);
}
const TArray<FName>& SoundIds()
{
    static const TArray<FName> Ids = { "WallBuild", "WallRepair", "WallBreak", "WallReflect", "SiegeRam", "TowerShelter", "HealingStation" };
    return Ids;
}
FString SoundPath(FName Id)
{
    if (!SoundIds().Contains(Id)) { return FString(); }
    const FString Name = TEXT("S_") + Id.ToString();
    return TEXT("/Game/Art/StorybookV1/V084/Audio/") + Name + TEXT(".") + Name;
}
float SoundInterval(FName Id)
{
    if (Id == "WallBuild") { return .4f; }
    if (Id == "WallRepair") { return .3f; }
    if (Id == "WallBreak") { return .25f; }
    if (Id == "WallReflect") { return .12f; }
    if (Id == "SiegeRam") { return .25f; }
    if (Id == "TowerShelter") { return .4f; }
    if (Id == "HealingStation") { return .45f; }
    return .08f;
}
}
