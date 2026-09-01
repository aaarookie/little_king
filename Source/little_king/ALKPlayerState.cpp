#include "ALKPlayerState.h"
#include "ULKDeckState.h"
#include "ULKSilverComponent.h"

ALKPlayerState::ALKPlayerState()
{
	Silver = CreateDefaultSubobject<ULKSilverComponent>(TEXT("Silver"));
	Deck = CreateDefaultSubobject<ULKDeckState>(TEXT("Deck"));
}
