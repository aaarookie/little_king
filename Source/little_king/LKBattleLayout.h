#pragma once
#include "CoreMinimal.h"
#include "LKDataTypes.h"

/** Portable battle designer document. Coordinates are converted once, at the JSON boundary. */
struct FLKBattleLayoutObject
{
    FName Id, ModuleId, UnitId;
    FString Label;
    FVector Center = FVector::ZeroVector;
    FVector2D Size = FVector2D(100,100);
    float Yaw = 90.f;
    ELKTeam Team = ELKTeam::Enemy;
    int32 Count = 1;
    float SpawnTime = 0.f;
    bool Contains(const FVector& Point, float Padding = 0.f) const;
};

struct FLKBattleLayout
{
    FString Title;
    FVector2D FieldHalfExtent = FVector2D(1200,2000);
    int32 Seed = 12345;
    float TimeLimit = 300.f;
    FLKEncounterRow Encounter;
    TArray<FLKBattleLayoutObject> Objects;
};

namespace LKBattleLayout
{
    constexpr int64 MaxFileBytes = 2 * 1024 * 1024;
    FName UnitForModule(FName Module);
    bool IsTerrain(FName Module);
    bool Parse(const FString& Json, FLKBattleLayout& Out, FString& Error);
    bool Load(const FString& Path, FLKBattleLayout& Out, FString& Error);
}
