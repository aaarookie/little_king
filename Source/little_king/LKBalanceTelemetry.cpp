#include "LKBalanceTelemetry.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

namespace
{
TSharedRef<FJsonObject> TeamJson(const FLKTeamMatchStats& Stats)
{
    auto Row=MakeShared<FJsonObject>();
    Row->SetNumberField(TEXT("kills"),Stats.Kills);
    Row->SetNumberField(TEXT("cards"),Stats.CardsPlayed);
    Row->SetNumberField(TEXT("damage"),Stats.Damage);
    Row->SetNumberField(TEXT("healing"),Stats.Healing);
    Row->SetNumberField(TEXT("silverSpent"),Stats.SilverSpent);
    Row->SetNumberField(TEXT("silverOverflow"),Stats.SilverOverflow);
    Row->SetNumberField(TEXT("silverAtCapSeconds"),Stats.SilverAtCapSeconds);
    Row->SetNumberField(TEXT("firstHeroDeathSeconds"),Stats.FirstHeroDeathSeconds);
    Row->SetNumberField(TEXT("peakUnits"),Stats.PeakUnits);
    return Row;
}
}

bool LKBalanceTelemetry::AppendMatch(const FLKMatchStats& Stats, const FLKBattleContext& Context,
    const FString& Mode, FString& Error)
{
    Error.Reset();
    const FString Directory=FPaths::ProjectSavedDir()/TEXT("Telemetry");
    const FString Path=Directory/(Mode==TEXT("automation")?TEXT("Battles-Automation.json"):TEXT("Battles.json"));
    IFileManager::Get().MakeDirectory(*Directory,true);
    TArray<TSharedPtr<FJsonValue>> History;
    FString Previous;
    // A damaged or externally enlarged log must not block a battle or allocate unbounded memory.
    const int64 PreviousBytes=IFileManager::Get().FileSize(*Path);
    if (PreviousBytes>=0 && PreviousBytes<=2*1024*1024 && FFileHelper::LoadFileToString(Previous,*Path))
    {
        TSharedPtr<FJsonObject> Old;
        const TArray<TSharedPtr<FJsonValue>>* Rows=nullptr;
        if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Previous),Old) && Old.IsValid()
            && Old->TryGetArrayField(TEXT("matches"),Rows)) { History=*Rows; }
    }
    if (History.Num()>=128) { History.RemoveAt(0,History.Num()-127); }
    auto Row=MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("utc"),FDateTime::UtcNow().ToIso8601());
    Row->SetStringField(TEXT("mode"),Mode);
    Row->SetStringField(TEXT("encounterId"),Context.EncounterId.ToString());
    Row->SetStringField(TEXT("regionId"),Context.RegionId.ToString());
    Row->SetStringField(TEXT("nodeId"),Context.NodeId.ToString());
    Row->SetNumberField(TEXT("seed"),Stats.Seed);
    Row->SetNumberField(TEXT("duration"),Stats.Duration);
    Row->SetBoolField(TEXT("playerWon"),Stats.Winner==ELKTeam::Player);
    Row->SetBoolField(TEXT("overtime"),Stats.bOvertime);
    Row->SetBoolField(TEXT("simultaneous"),Stats.bSimultaneousElimination);
    Row->SetObjectField(TEXT("player"),TeamJson(Stats.Player));
    Row->SetObjectField(TEXT("enemy"),TeamJson(Stats.Enemy));
    History.Add(MakeShared<FJsonValueObject>(Row));
    auto Root=MakeShared<FJsonObject>();
    Root->SetNumberField(TEXT("version"),1);
    Root->SetArrayField(TEXT("matches"),History);
    FString Json; FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Json));
    const FString Temp=Path+TEXT(".tmp");
    if (!FFileHelper::SaveStringToFile(Json,*Temp,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
        || !IFileManager::Get().Move(*Path,*Temp,true,true))
    { Error=TEXT("无法写入本地战斗统计"); return false; }
    return true;
}
