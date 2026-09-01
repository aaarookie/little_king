#include "ULKGameData.h"
#include "LKLog.h"

void ULKGameData::EnsureDefaultDecks()
{
	if (DefaultPlayerDeck.Num() == 0)
	{
		DefaultPlayerDeck = {
			TEXT("Unit_Swordsman"), TEXT("Unit_Swordsman"), TEXT("Unit_Swordsman"),
			TEXT("Unit_Archer"), TEXT("Unit_Archer"),
			TEXT("Unit_Shieldbearer"), TEXT("Unit_Shieldbearer"),
			TEXT("Spell_Fireball")
		};
	}

	if (DefaultEnemyDeck.Num() == 0)
	{
		DefaultEnemyDeck = {
			TEXT("Unit_Swordsman"), TEXT("Unit_Swordsman"), TEXT("Unit_Swordsman"),
			TEXT("Unit_Archer"), TEXT("Unit_Archer"),
			TEXT("Unit_Shieldbearer"), TEXT("Unit_Shieldbearer"),
			TEXT("Spell_Fireball")
		};
	}

	UE_LOG(LogLK, Log, TEXT("[GameData] 默认牌库就绪: Player=%d 张, Enemy=%d 张"),
		DefaultPlayerDeck.Num(), DefaultEnemyDeck.Num());
}
