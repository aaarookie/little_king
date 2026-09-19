#pragma once
#include "CoreMinimal.h"
#include "Engine/GameViewportClient.h"
#include "ULKGameViewportClient.generated.h"

/** 在引擎 SHOW 显示标记命令之前，匹配完整调试短语。 */
UCLASS()
class ULKGameViewportClient : public UGameViewportClient
{
	GENERATED_BODY()
protected:
	virtual bool Exec_Runtime(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;
};
