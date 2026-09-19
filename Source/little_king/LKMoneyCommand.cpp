#include "LKMoneyCommand.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "ULKProfileSubsystem.h"
#include "ULKSaveSlotSubsystem.h"

bool LKMoneyCommand::TryExecute(UWorld* World, const TCHAR* Command, FOutputDevice& Out)
{
	TArray<FString> Words;
	FString(Command).ParseIntoArrayWS(Words);
	if (Words.Num() < 4 || !Words[0].Equals(TEXT("show"), ESearchCase::IgnoreCase)
		|| !Words[1].Equals(TEXT("me"), ESearchCase::IgnoreCase)
		|| !Words[2].Equals(TEXT("the"), ESearchCase::IgnoreCase)
		|| !Words[3].Equals(TEXT("money"), ESearchCase::IgnoreCase)) { return false; }
	int32 Amount = 100;
	bool bValidAmount = Words.Num() <= 5;
	if (Words.Num() == 5)
	{
		Amount = 0;
		for (TCHAR Digit : Words[4])
		{
			if (Digit < '0' || Digit > '9' || Amount > (MAX_int32 - (Digit - '0')) / 10)
			{ bValidAmount = false; break; }
			Amount = Amount * 10 + Digit - '0';
		}
	}
	if (!bValidAmount)
	{
		Out.Log(TEXT("用法：show me the money [非负整数]，省略数量为 100；数量不得超过 2147483647。"));
		return true;
	}
	UGameInstance* Instance = World ? World->GetGameInstance() : nullptr;
	ULKProfileSubsystem* Profile = Instance ? Instance->GetSubsystem<ULKProfileSubsystem>() : nullptr;
	const ULKSaveSlotSubsystem* Slots = Instance ? Instance->GetSubsystem<ULKSaveSlotSubsystem>() : nullptr;
	if (!Profile || !Profile->HasProfile() || !Slots || !Slots->HasSelectedGame())
	{ Out.Log(TEXT("请先继续、读取或创建游戏，再给予当前存档金币。")); return true; }
	const int32 Before = Profile->GetGold();
	if (Profile->AddGold(Amount))
	{ Out.Logf(TEXT("金币 +%d：%d → %d（已保存到存档 %d）"), Profile->GetGold() - Before, Before, Profile->GetGold(), Slots->GetActiveSlot() + 1); }
	else { Out.Log(TEXT("给予金币失败：存档未成功写入，本次数值未改变。")); }
	return true;
}
