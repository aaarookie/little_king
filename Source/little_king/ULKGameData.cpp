#include "ULKGameData.h"
#include "LKLog.h"

ULKGameData::ULKGameData()
{
    DefaultHeroTraits.FindOrAdd(TEXT("Hero_Mage")).Traits = { TEXT("Trait_MageSpellReach") };
    DefaultHeroTraits.FindOrAdd(TEXT("Hero_Knight")).Traits = { TEXT("Trait_KnightTauntAura") };
    FireballSkillHeroIds = { TEXT("Hero_Mage") };
}

void ULKGameData::EnsureDefaultDecks()
{
	if (DefaultPlayerDeck.Num() == 0)
	{
		DefaultPlayerDeck = {
			TEXT("Unit_Swordsman"), TEXT("Unit_Archer"), TEXT("Unit_Shieldbearer"),
            TEXT("Spell_Fireball"), TEXT("Spell_HealWave"),
            TEXT("Building_ArrowTower"), TEXT("Building_Barracks")
		};
	}

	if (DefaultEnemyDeck.Num() == 0)
	{
		DefaultEnemyDeck = {
			TEXT("Unit_Swordsman"), TEXT("Unit_Archer"), TEXT("Unit_Shieldbearer"),
            TEXT("Spell_Fireball"), TEXT("Spell_HealWave"),
            TEXT("Building_ArrowTower"), TEXT("Building_Barracks")
		};
	}

	UE_LOG(LogLK, Log, TEXT("[GameData] 默认牌库就绪: Player=%d 张, Enemy=%d 张"),
		DefaultPlayerDeck.Num(), DefaultEnemyDeck.Num());
}
