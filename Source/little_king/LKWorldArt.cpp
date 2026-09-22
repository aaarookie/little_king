#include "LKWorldArt.h"
#include "Engine/Texture2D.h"
#include "PaperSprite.h"

namespace
{
template<class T> T* Asset(const FString& Folder, const FString& Name)
{
    return LoadObject<T>(nullptr, *(TEXT("/Game/Art/StorybookV1/World/") + Folder + TEXT("/") + Name + TEXT(".") + Name), nullptr, LOAD_NoWarn);
}
}
int32 LKWorldArt::RegionIndex(FName Id)
{
    for (int32 I = 0; I < 5; ++I) { if (Id == FName(*FString::Printf(TEXT("World_Region%d"), I))) { return I; } }
    return 0; // Standalone and legacy three-room expeditions use the meadow.
}
UTexture2D* LKWorldArt::GroundTexture(FName Id) { return Asset<UTexture2D>(TEXT("Textures"), FString::Printf(TEXT("T_Ground_%d"), RegionIndex(Id))); }
UPaperSprite* LKWorldArt::GroundSprite(FName Id) { return Asset<UPaperSprite>(TEXT("Sprites"), FString::Printf(TEXT("SP_Ground_%d"), RegionIndex(Id))); }
UPaperSprite* LKWorldArt::CampSprite(FName Id)
{
    const TCHAR* Name = Id == "Hero_Mage" ? TEXT("SP_Camp_Mage") : Id == "Hero_Ranger" ? TEXT("SP_Camp_Ranger") : TEXT("SP_Camp_Knight");
    return Asset<UPaperSprite>(TEXT("Sprites"), Name);
}
UTexture2D* LKWorldArt::NodeIcon(ELKDungeonNodeType Type)
{
    const TCHAR* Name = nullptr;
    switch (Type)
    {
    case ELKDungeonNodeType::Battle: Name = TEXT("T_Node_Battle"); break;
    case ELKDungeonNodeType::Elite: Name = TEXT("T_Node_Elite"); break;
    case ELKDungeonNodeType::Boss: Name = TEXT("T_Node_Boss"); break;
    case ELKDungeonNodeType::Market: Name = TEXT("T_Node_Market"); break;
    case ELKDungeonNodeType::Rest: Name = TEXT("T_Node_Rest"); break;
    default: return nullptr;
    }
    return Asset<UTexture2D>(TEXT("Textures"), Name);
}
UTexture2D* LKWorldArt::EffectTexture(FName Id)
{
    if (Id != "FX_EmberBurst" && Id != "FX_Leaf" && Id != "FX_Dust") { return nullptr; }
    return Asset<UTexture2D>(TEXT("Textures"), TEXT("T_")+Id.ToString());
}
const TArray<FName>& LKWorldArt::SoundIds()
{
    static const TArray<FName> Ids = {"BowShot", "SpearShot", "MagicShot", "FireballCast", "FireballImpact", "Heal", "Summon", "Sacrifice", "Revive", "Backstab", "Coin", "Mimic", "Dash", "Empower", "HeavyHit", "Stun", "Freeze", "Burn", "IceBreath", "FireBreath", "SiegeShot", "SiegeImpact", "Footstep", "CampCommand", "NodeEnter", "Rest", "Market", "Upgrade"};
    return Ids;
}
float LKWorldArt::SoundInterval(FName Id)
{
    static const TMap<FName, float> Gaps = {{"BowShot",.08f},{"SpearShot",.10f},{"MagicShot",.12f},{"FireballCast",.10f},{"FireballImpact",.10f},{"Heal",.30f},{"Summon",.20f},{"Sacrifice",.20f},{"Revive",.30f},{"Backstab",.15f},{"Coin",.18f},{"Mimic",.20f},{"Dash",.15f},{"Empower",.25f},{"HeavyHit",.15f},{"Stun",.20f},{"Freeze",.20f},{"Burn",.30f},{"IceBreath",.12f},{"FireBreath",.12f},{"SiegeShot",.20f},{"SiegeImpact",.20f},{"Footstep",.55f},{"CampCommand",.12f},{"NodeEnter",.20f},{"Rest",.40f},{"Market",.30f},{"Upgrade",.40f}};
    if (const float* Gap = Gaps.Find(Id)) { return *Gap; }
    return .08f;
}
FString LKWorldArt::SoundPath(FName Id)
{
    const FString Name = TEXT("S_") + Id.ToString();
    return TEXT("/Game/Art/StorybookV1/World/Audio/") + Name + TEXT(".") + Name;
}
