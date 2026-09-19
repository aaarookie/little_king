#include "ULKGameViewportClient.h"
#include "LKMoneyCommand.h"

bool ULKGameViewportClient::Exec_Runtime(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	return LKMoneyCommand::TryExecute(InWorld, Cmd, Ar) || Super::Exec_Runtime(InWorld, Cmd, Ar);
}
