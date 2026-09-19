#pragma once
#include "CoreMinimal.h"
#include "LKRunTypes.h"

/** 世界版图和节点目录与 UMG/战斗 Actor 无关，生成结果完全保存到 Run。 */
namespace LKWorldMapContent
{
	constexpr int32 LayoutVersion = 1;
	const TArray<FLKWorldRegion>& Regions();
	FName StartNodeId();
	bool IsCombat(ELKDungeonNodeType Type);
	FText NodeTitle(ELKDungeonNodeType Type);
	FText NodeDescription(ELKDungeonNodeType Type);
	bool Generate(FLKRunState& State, FString& OutError);
	bool Validate(const FLKRunState& State, FString& OutError);
	bool Contains(const FLKWorldRegion& Region, FVector2D Point);
}
