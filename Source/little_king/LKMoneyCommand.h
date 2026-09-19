#pragma once
#include "CoreMinimal.h"
class UWorld;
namespace LKMoneyCommand
{
	/** 完整四词匹配；其他 show 指令返回 false，交还引擎。 */
	bool TryExecute(UWorld* World, const TCHAR* Command, FOutputDevice& Out);
}
