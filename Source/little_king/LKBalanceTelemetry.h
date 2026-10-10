#pragma once
#include "CoreMinimal.h"
#include "LKTypes.h"
#include "LKRunTypes.h"

namespace LKBalanceTelemetry
{
    /** Local, bounded diagnostics only. Never influences simulation or save transactions. */
    bool AppendMatch(const FLKMatchStats& Stats, const FLKBattleContext& Context,
        const FString& Mode, FString& Error);
}
