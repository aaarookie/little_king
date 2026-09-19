#include "ULKSaveSlotSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "LKHomeContent.h"
#include "ULKProfileSubsystem.h"
#include "ULKProfileSaveGame.h"
#include "ULKRunSubsystem.h"
#include "ULKRunSaveGame.h"

namespace
{
	bool Exists(const FString& Slot) { return UGameplayStatics::DoesSaveGameExist(Slot, 0); }
	FDateTime SaveTime(const FString& Slot, FDateTime Stored)
	{
		// 兼容尚无 SavedAtUtc 的旧桌面存档；仅用于显示排序，不据此校验存档内容。
		return Stored.GetTicks() > 0 ? Stored : IFileManager::Get().GetTimeStamp(*(FPaths::ProjectSavedDir() / TEXT("SaveGames") / (Slot + TEXT(".sav"))));
	}
}

FString ULKSaveSlotSubsystem::ProfileBase(int32 Index) const
{
	if (!TestNamespace.IsEmpty()) { return TestNamespace + FString::Printf(TEXT("_%02d_Profile"), Index + 1); }
	return Index == 0 ? TEXT("LittleKing_Profile") : FString::Printf(TEXT("LittleKing_Save%02d_Profile"), Index + 1);
}
FString ULKSaveSlotSubsystem::RunSlot(int32 Index) const
{
	if (!TestNamespace.IsEmpty()) { return TestNamespace + FString::Printf(TEXT("_%02d_Run"), Index + 1); }
	return Index == 0 ? TEXT("LittleKing_Run") : FString::Printf(TEXT("LittleKing_Save%02d_Run"), Index + 1);
}
FString ULKSaveSlotSubsystem::DeleteMarker(int32 Index) const { return ProfileBase(Index) + TEXT("_Deleting"); }

FLKSaveSlotSummary ULKSaveSlotSubsystem::InspectSlot(int32 Index) const
{
	FLKSaveSlotSummary Row;
	Row.Index = Index;
	if (Index < 0 || Index >= MaxSlots) { Row.Error = TEXT("存档编号无效"); return Row; }
	const FString Base = ProfileBase(Index);
	const bool bHasProfile = Exists(Base + TEXT("_A")) || Exists(Base + TEXT("_B"));
	Row.bHasRun = Exists(RunSlot(Index));
	Row.bExists = bHasProfile || Row.bHasRun;
	if (!Row.bExists) { return Row; }
	if (Exists(DeleteMarker(Index))) { Row.Error = TEXT("删除未完成，请再次删除以清理残留"); return Row; }
	ULKProfileSubsystem* Probe = NewObject<ULKProfileSubsystem>(GetGameInstance());
	Probe->ConfigureStorage(Base);
	FLKProfileState Profile;
	if (bHasProfile)
	{
		if (!Probe->ReadExistingProfile(Profile, Row.Error)) { return Row; }
		Row.Gold = Profile.Gold;
		const int32* Level = Profile.BuildingLevels.Find(LKHomeContent::BuildingId(ELKHomeBuilding::Statue));
		Row.StatueLevel = Level ? *Level : 1;
		for (const FString& SlotName : {Base + TEXT("_A"), Base + TEXT("_B")})
		{
			if (!Exists(SlotName)) { continue; }
			const ULKProfileSaveGame* Save = Cast<ULKProfileSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
			if (Save && Save->Profile.ProfileId == Profile.ProfileId && Save->Profile.Revision == Profile.Revision)
			{ Row.SavedAtUtc = FMath::Max(Row.SavedAtUtc, SaveTime(SlotName, Save->SavedAtUtc)); }
		}
	}
	if (Row.bHasRun)
	{
		const ULKRunSaveGame* Save = Cast<ULKRunSaveGame>(UGameplayStatics::LoadGameFromSlot(RunSlot(Index), 0));
		if (!Save || Save->SaveVersion != 1 || !ULKRunSubsystem::ValidateStoredRun(Save->RunState, Row.Error))
		{ if (Row.Error.IsEmpty()) { Row.Error = TEXT("远征文件不可读或版本不支持"); } return Row; }
		if (Save->RunState.SchemaVersion >= 5 && (!bHasProfile || !Save->RunState.ProfileId.IsValid()))
		{ Row.Error = TEXT("远征缺少关联的家园身份，保留文件并停止载入"); return Row; }
		if (Save->RunState.ProfileId.IsValid() && Save->RunState.ProfileId != Profile.ProfileId)
		{ Row.Error = TEXT("家园与远征身份不一致，保留文件并停止载入"); return Row; }
		Row.RunPhase = Save->RunState.Phase;
		Row.SavedAtUtc = FMath::Max(Row.SavedAtUtc, SaveTime(RunSlot(Index), Save->SavedAtUtc));
	}
	Row.bCanLoad = true;
	return Row;
}

TArray<FLKSaveSlotSummary> ULKSaveSlotSubsystem::ListSlots() const
{
	TArray<FLKSaveSlotSummary> Rows;
	for (int32 Index = 0; Index < MaxSlots; ++Index) { Rows.Add(InspectSlot(Index)); }
	return Rows;
}

int32 ULKSaveSlotSubsystem::GetMostRecentSlot() const
{
	int32 Best = INDEX_NONE;
	FDateTime Latest;
	for (const FLKSaveSlotSummary& Row : ListSlots())
	{
		if (Row.bCanLoad && (Best == INDEX_NONE || Row.SavedAtUtc > Latest)) { Best = Row.Index; Latest = Row.SavedAtUtc; }
	}
	return Best;
}

void ULKSaveSlotSubsystem::ResetSession()
{
	ActiveSlot = INDEX_NONE;
	GetGameInstance()->GetSubsystem<ULKProfileSubsystem>()->ConfigureStorage(FString());
	GetGameInstance()->GetSubsystem<ULKRunSubsystem>()->ConfigureStorage(FString());
}

bool ULKSaveSlotSubsystem::CreateNewGame()
{
	LastError.Reset();
	if (HasSelectedGame()) { LastError = TEXT("请先回到开始界面"); return false; }
	for (const FLKSaveSlotSummary& Row : ListSlots())
	{
		if (Row.bExists) { continue; }
		if (Exists(DeleteMarker(Row.Index)) && !UGameplayStatics::DeleteGameInSlot(DeleteMarker(Row.Index), 0))
		{ LastError = TEXT("无法清理旧删除标记，请检查存档目录权限"); return false; }
		ULKProfileSubsystem* Profile = GetGameInstance()->GetSubsystem<ULKProfileSubsystem>();
		Profile->ConfigureStorage(ProfileBase(Row.Index));
		GetGameInstance()->GetSubsystem<ULKRunSubsystem>()->ConfigureStorage(RunSlot(Row.Index));
		if (!Profile->EnsureProfile() || !Profile->IsProfileUsable())
		{ LastError = TEXT("创建存档失败：") + Profile->GetLastError(); ResetSession(); return false; }
		ActiveSlot = Row.Index;
		return true;
	}
	LastError = TEXT("已达到 8 个存档上限，请在读取存档中删除一个旧档后再创建");
	return false;
}

bool ULKSaveSlotSubsystem::LoadSlot(int32 Index)
{
	LastError.Reset();
	if (HasSelectedGame()) { LastError = TEXT("请先回到开始界面"); return false; }
	const FLKSaveSlotSummary Row = InspectSlot(Index);
	if (!Row.bCanLoad) { LastError = Row.Error.IsEmpty() ? TEXT("此位置没有可读取的存档") : Row.Error; return false; }
	ULKProfileSubsystem* Profile = GetGameInstance()->GetSubsystem<ULKProfileSubsystem>();
	ULKRunSubsystem* Run = GetGameInstance()->GetSubsystem<ULKRunSubsystem>();
	Profile->ConfigureStorage(ProfileBase(Index));
	Run->ConfigureStorage(RunSlot(Index));
	if (!Profile->EnsureProfile() || !Profile->IsProfileUsable() || (Row.bHasRun && !Run->LoadExpedition()))
	{ LastError = TEXT("读取存档失败，原文件已保留"); ResetSession(); return false; }
	if (!Run->ReconcileDepartureFunding()) { LastError = TEXT("出发金币交接未完成，原存档已保留"); ResetSession(); return false; }
	if (!Profile->TouchSelectedProfile())
	{ LastError = TEXT("无法保存最近使用时间，请检查存档目录权限后重试"); ResetSession(); return false; }
	ActiveSlot = Index;
	return true;
}

bool ULKSaveSlotSubsystem::DeleteSlot(int32 Index)
{
	LastError.Reset();
	if (Index < 0 || Index >= MaxSlots || HasSelectedGame()) { LastError = TEXT("只能在开始界面删除有效编号的存档"); return false; }
	if (!InspectSlot(Index).bExists) { LastError = TEXT("该存档已不存在"); return false; }
	if (!UGameplayStatics::SaveGameToSlot(NewObject<ULKDeletedSlotSaveGame>(), DeleteMarker(Index), 0))
	{ LastError = TEXT("无法开始删除，原存档未改动"); return false; }
	bool bOk = true;
	for (const FString& Name : {RunSlot(Index), ProfileBase(Index) + TEXT("_B"), ProfileBase(Index) + TEXT("_A")})
	{
		if (Exists(Name) && !UGameplayStatics::DeleteGameInSlot(Name, 0)) { bOk = false; }
	}
	if (!bOk) { LastError = TEXT("删除未完成，残留存档已禁用；检查目录权限后重试删除"); return false; }
	UGameplayStatics::DeleteGameInSlot(DeleteMarker(Index), 0); // Empty marked slots remain safe/reusable if this cleanup fails.
	return true;
}

FName ULKSaveSlotSubsystem::GetEntryMap() const
{
	const ULKRunSubsystem* Run = GetGameInstance()->GetSubsystem<ULKRunSubsystem>();
	return Run->HasRunInProgress() ? FName(TEXT("L_BattleTest")) : FName(TEXT("L_Home"));
}

bool ULKSaveSlotSubsystem::RouteInitialPlayToMenu(UWorld* World)
{
	if (!World || !World->IsGameWorld() || !World->GetGameInstance()) { return false; }
#if WITH_DEV_AUTOMATION_TESTS
	if (FAutomationTestFramework::Get().GetCurrentTest()) { return false; }
#endif
#if WITH_EDITOR
	if (FParse::Param(FCommandLine::Get(), TEXT("HomeUIPreview"))) { return false; }
#endif
	const ULKSaveSlotSubsystem* Slots = World->GetGameInstance()->GetSubsystem<ULKSaveSlotSubsystem>();
	if (Slots && !Slots->HasSelectedGame())
	{
		UGameplayStatics::OpenLevel(World, TEXT("L_StartMenu"));
		return true;
	}
	return false;
}
