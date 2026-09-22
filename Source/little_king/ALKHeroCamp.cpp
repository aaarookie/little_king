#include "ALKHeroCamp.h"
#include "ALKUnitHero.h"
#include "ULKGameData.h"
#include "Components/SphereComponent.h"
#include "ULKUnitMovementComponent.h"
#include "LKWorldArt.h"
#include "PaperSprite.h"

ALKHeroCamp::ALKHeroCamp()
{
	UnitClass = ELKUnitClass::Building;
	MovementComponent->SetComponentTickEnabled(false);
}

void ALKHeroCamp::InitializeCamp(ALKUnitHero* Hero, ULKGameData* Data)
{
	LinkedHero = Hero;
	FLKUnitRow Row;
	Row.UnitId = TEXT("Building_HeroCamp");
	Row.UnitClass = ELKUnitClass::Building;
	Row.AttackDamage = 0.f;
	Row.MoveSpeed = 0.f;
    Row.Sprite = LKWorldArt::CampSprite(Hero->GetUnitId());
    Row.SpriteScale = FVector2D(1.f, 1.f);
	SetTeam(Hero->GetTeam());
	InitUnit(Row, Data);
	BodyRadius = Data->HeroCampBodyRadius;
	BodyCollision->SetSphereRadius(BodyRadius);
	SetInvulnerable(-1.f);
	SetCombatEnabled(false);
	Hero->SetCamp(this, Data->HeroCampMoveRadius);
}

ALKUnitHero* ALKHeroCamp::GetHero() const { return LinkedHero.Get(); }
