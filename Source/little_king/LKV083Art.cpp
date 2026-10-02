#include "LKV083Art.h"
#include "PaperSprite.h"
#include "PaperFlipbook.h"
#include "Engine/Texture2D.h"

namespace LKV083Art
{
const TArray<FName>& UnitIds()
{
    static const TArray<FName> Ids = {
        "Unit_AngelWarrior_Uncommon", "Unit_AngelWarrior_Rare", "Unit_AngelWarrior_Epic",
        "Unit_AngelArcher_Uncommon", "Unit_AngelArcher_Rare", "Unit_AngelArcher_Epic",
        "Unit_AngelPriest_Uncommon", "Unit_AngelPriest_Rare", "Unit_AngelPriest_Epic",
        "Unit_DivineJudge", "Unit_ChosenHighPriest"
    };
    return Ids;
}
const TArray<FName>& SpellIds()
{
    static const TArray<FName> Ids = { "Spell_Freeze", "Spell_MariaNovice", "Spell_MariaIntermediate",
        "Spell_MariaAdvanced", "Spell_MariaDivine", "Spell_BlackCloud", "Spell_Lightning",
        "Spell_Hurricane", "Spell_DivineBlessing" };
    return Ids;
}
FName Identity(FName UnitId)
{
    if (!UnitIds().Contains(UnitId)) { return NAME_None; }
    const FString Id = UnitId.ToString();
    if (Id.StartsWith(TEXT("Unit_AngelWarrior_"))) { return "AngelWarrior"; }
    if (Id.StartsWith(TEXT("Unit_AngelArcher_"))) { return "AngelArcher"; }
    if (Id.StartsWith(TEXT("Unit_AngelPriest_"))) { return "AngelPriest"; }
    return UnitId == "Unit_DivineJudge" ? FName("DivineJudge") : FName("ChosenHighPriest");
}
TSoftObjectPtr<UPaperSprite> Sprite(FName UnitId)
{
    const FName Role = Identity(UnitId);
    if (Role.IsNone()) { return nullptr; }
    const FString Name = TEXT("SP_") + Role.ToString() + TEXT("_00");
    return TSoftObjectPtr<UPaperSprite>(FSoftObjectPath(TEXT("/Game/Art/StorybookV1/V083/Animations/") + Name + TEXT(".") + Name));
}
TSoftObjectPtr<UTexture2D> CardIcon(FName CardId)
{
    if (!SpellIds().Contains(CardId)) { return nullptr; }
    const FString Name = TEXT("T_Card_") + CardId.ToString();
    return TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Art/StorybookV1/V083/Cards/") + Name + TEXT(".") + Name));
}
UPaperFlipbook* Animation(FName UnitId, FName State)
{
    const FName Role = Identity(UnitId);
    if (Role.IsNone() || (State != "Idle" && State != "Move" && State != "Attack" && State != "Hit" && State != "Death")) { return nullptr; }
    const FString Name = TEXT("FB_") + Role.ToString() + TEXT("_") + State.ToString();
    return LoadObject<UPaperFlipbook>(nullptr, *(TEXT("/Game/Art/StorybookV1/V083/Animations/") + Name + TEXT(".") + Name), nullptr, LOAD_NoWarn);
}
const TArray<FName>& SoundIds()
{
    static const TArray<FName> Ids = { "FreezeSpell", "HolySummon", "BlackCloud", "Lightning", "Hurricane", "DivineBlessing" };
    return Ids;
}
FString SoundPath(FName Id)
{
    if (!SoundIds().Contains(Id)) { return FString(); }
    const FString Name = TEXT("S_") + Id.ToString();
    return TEXT("/Game/Art/StorybookV1/V083/Audio/") + Name + TEXT(".") + Name;
}
float SoundInterval(FName Id)
{
    if (Id == "FreezeSpell") { return .30f; }
    if (Id == "HolySummon" || Id == "Hurricane") { return .40f; }
    if (Id == "BlackCloud") { return .30f; }
    if (Id == "Lightning") { return .25f; }
    if (Id == "DivineBlessing") { return .60f; }
    return .08f;
}
}
