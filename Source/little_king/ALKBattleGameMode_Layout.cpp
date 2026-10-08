#include "ALKBattleGameMode.h"
#include "ALKBattleTerrain.h"
#include "ALKOpponentBrain.h"
#include "ALKHeroCamp.h"
#include "ALKUnitHero.h"
#include "ALKUnitBuilding.h"
#include "LKSkeletonCircle.h"
#include "ULKGameData.h"
#include "ULKDeckState.h"
#include "LKLog.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/PlatformMisc.h"

namespace
{
    ELKBattleTerrainKind TerrainKind(FName Id)
    {
        if(Id=="brush") return ELKBattleTerrainKind::Brush;
        if(Id=="swamp") return ELKBattleTerrainKind::Swamp;
        if(Id=="campfire") return ELKBattleTerrainKind::Campfire;
        if(Id=="mountain") return ELKBattleTerrainKind::Highland;
        if(Id=="road") return ELKBattleTerrainKind::Road;
        if(Id=="water") return ELKBattleTerrainKind::Water;
        if(Id=="flowers") return ELKBattleTerrainKind::Flowers;
        if(Id=="rock") return ELKBattleTerrainKind::Rock;
        if(Id=="tree") return ELKBattleTerrainKind::Tree;
        return ELKBattleTerrainKind::Grass;
    }
}

void ALKBattleGameMode::RegisterTacticalCircle(ALKSkeletonCircle* Circle)
{ if(IsValid(Circle)) { ActiveSkeletonCircles.AddUnique(Circle); } }
int32 ALKBattleGameMode::GetNextTacticalSpellSeed()
{ return static_cast<int32>(static_cast<uint32>(BattleRandom.GetInitialSeed())+TacticalSpellSerial++*7919u); }

bool ALKBattleGameMode::IsInsideLayoutDeploymentZone(const FVector& Location) const
{
    if(!AuthoredLayout) { return true; }
    bool HasZone=false;
    for(const auto& Object:AuthoredLayout->Objects)
    {
        if(Object.ModuleId!="player-zone") continue;
        HasZone=true;
        if(Object.Contains(Location,GameData->UnitBodyRadius)) { return true; }
    }
    return !HasZone;
}

bool ALKBattleGameMode::LoadBattleLayoutFile(const FString& Path,FString& Error)
{
    FLKBattleLayout Layout;
    if(!LKBattleLayout::Load(Path,Layout,Error)) return false;
    return ApplyBattleLayout(Layout,Error);
}

bool ALKBattleGameMode::ApplyBattleLayout(const FLKBattleLayout& Layout,FString& Error)
{
    Error.Reset();
    if(!GameData || !OpponentBrain || Phase!=ELKGamePhase::Deployment || bExpeditionBattle || !DeployedHeroes[0].IsEmpty())
    { Error=TEXT("仅能在独立试玩、尚未部署玩家英雄时载入地图"); return false; }
    for(FName CardId:Layout.Encounter.EnemyCards)
    { if(!FindCard(CardId)) { Error=TEXT("未知敌方卡牌：")+CardId.ToString(); return false; } }
    int32 Counts[2]={0,0}; TSet<FName> PlayerHeroes;
    for(const auto& Object:Layout.Objects)
    {
        if(Object.UnitId.IsNone() || Object.SpawnTime>0) continue;
        const auto* Row=GetUnitRow(Object.UnitId);
        if(!Row) { Error=TEXT("未知地图单位：")+Object.UnitId.ToString(); return false; }
        Counts[int32(Object.Team)]+=Object.Count;
        if(Object.Team==ELKTeam::Player && (Row->UnitClass==ELKUnitClass::Hero || Row->UnitClass==ELKUnitClass::Boss))
        {
            if(!AvailableHeroes.Contains(Object.UnitId) || PlayerHeroes.Contains(Object.UnitId))
            { Error=TEXT("我方固定英雄必须来自当前三英雄名单且不重复"); return false; }
            PlayerHeroes.Add(Object.UnitId);
        }
    }
    if(GameData->MaxUnitsPerTeam>0 && (Counts[0]+AvailableHeroes.Num()-PlayerHeroes.Num()>GameData->MaxUnitsPerTeam || Counts[1]>GameData->MaxUnitsPerTeam))
    { Error=TEXT("初始部队超过场上单位上限"); return false; }
    // Parsing/catalog failures above preserve the existing deployment. Geometry failure below
    // blocks this preview explicitly; no partial layout can be started or saved as an expedition.
    TArray<AActor*> Previous;
    for(TActorIterator<ALKUnitBase> It(GetWorld());It;++It)
    { It->OnUnitDied.RemoveDynamic(this,&ALKBattleGameMode::HandleUnitDied); Previous.Add(*It); }
    for(TActorIterator<ALKBattleTerrain> It(GetWorld());It;++It) { Previous.Add(*It); }
    for(AActor* Actor:Previous) Actor->Destroy();
    for(int32 Side=0;Side<2;++Side) { AliveHeroes[Side].Reset(); HeroCounts[Side]=0; DeployedHeroes[Side].Reset(); BuildingCounts[Side].Reset(); }
    GameData->FieldHalfWidth=Layout.FieldHalfExtent.X; GameData->FieldHalfHeight=Layout.FieldHalfExtent.Y;
    GameData->BattleTimeLimit=Layout.TimeLimit; GameData->BattleSeed=Layout.Seed;
    BattleRandom.Initialize(Layout.Seed); MatchStats.Seed=Layout.Seed;
    SeedTieBreaker=BattleRandom.RandRange(0,1)==0?ELKTeam::Player:ELKTeam::Enemy;
    CurrentEncounter=Layout.Encounter; EnemyHeroIds=Layout.Encounter.EnemyHeroIds;
    bEnemyUsesCards=Layout.Encounter.bEnemyUsesCards;
    AuthoredLayout=MakeShared<FLKBattleLayout>(Layout);
    TArray<AActor*> Created;
    auto Abort=[&](const FString& Reason)
    {
        Error=LayoutLoadError=Reason;
        for(AActor* Actor:Created) { if(IsValid(Actor)) Actor->Destroy(); }
        for(int32 Side=0;Side<2;++Side) { AliveHeroes[Side].Reset(); HeroCounts[Side]=0; DeployedHeroes[Side].Reset(); BuildingCounts[Side].Reset(); }
        OpponentBrain->ResetBattleState(); AuthoredLayout.Reset(); return false;
    };
    for(const auto& Object:Layout.Objects)
    {
        if(!LKBattleLayout::IsTerrain(Object.ModuleId)) continue;
        auto* Terrain=GetWorld()->SpawnActor<ALKBattleTerrain>();
        if(!Terrain) return Abort(TEXT("地形生成失败"));
        Created.Add(Terrain); Terrain->Tags.Add(Object.Id);
        if(!Terrain->Initialize(TerrainKind(Object.ModuleId),Object.Center,Object.Size,Object.Yaw)) return Abort(TEXT("地形参数非法"));
    }
    // Buildings first: later groups search around physical foundations, not through them.
    TArray<FLKBattleLayoutObject> SpawnObjects=Layout.Objects;
    SpawnObjects.StableSort([&](const auto& A,const auto& B)
    {
        const auto* RA=GetUnitRow(A.UnitId); const auto* RB=GetUnitRow(B.UnitId);
        return RA && RA->UnitClass==ELKUnitClass::Building && (!RB || RB->UnitClass!=ELKUnitClass::Building);
    });
    FRandomStream Placement(Layout.Seed);
    for(const auto& Object:SpawnObjects)
    {
        if(Object.UnitId.IsNone() || Object.SpawnTime>0) continue;
        for(int32 I=0;I<Object.Count;++I)
        {
            FVector Point=Object.Center;
            const auto* Row=GetUnitRow(Object.UnitId);
            const bool Wall=Row->BuildingBehavior==ELKBuildingBehavior::Wall;
            if(!Wall && !IsSpawnPointFree(Point,GameData->UnitBodyRadius,true))
            {
                if(Object.Count==1 || !FindFreeSpawnLocation(Object.Center,FMath::Max(250.f,Object.Size.GetMax()*.5f),Point,Placement,80,true))
                { return Abort(TEXT("出生位置被占用或过于贴近地形边缘：")+Object.Label); }
            }
            ALKUnitBase* Unit=SpawnUnitForTeam(Object.UnitId,Object.Team,Point,Row->UnitClass,NAME_None,Object.Yaw,nullptr,true);
            if(!Unit) return Abort(TEXT("单位生成失败，请检查边界/建筑重叠：")+Object.Label);
            Created.Add(Unit); Unit->Tags.Add(Object.Id);
            if(auto* Building=Cast<ALKUnitBuilding>(Unit)) Building->CompleteAuthoredConstruction();
            if(Unit->IsHero())
            {
                DeployedHeroes[int32(Object.Team)].Add(Object.UnitId);
                if(Object.Team==ELKTeam::Player)
                {
                    FVector CampPoint;
                    if(!FindFreeSpawnLocation(Point,250.f,CampPoint,Placement,80)) return Abort(TEXT("固定我方英雄附近没有营地位置"));
                    auto* Camp=GetWorld()->SpawnActor<ALKHeroCamp>(CampPoint,FRotator::ZeroRotator);
                    if(!Camp) return Abort(TEXT("营地生成失败"));
                    Created.Add(Camp); Camp->InitializeCamp(CastChecked<ALKUnitHero>(Unit),GameData);
                }
            }
        }
    }
    if(!OpponentBrain->ConfigureEncounter(CurrentEncounter)) return Abort(TEXT("敌方牌组初始化失败"));
    if(auto* Deck=GetTeamDeck(ELKTeam::Player)) { Deck->InitDeck(GameData->DefaultPlayerDeck,GameData->HandSize,Layout.Seed+1); }
    for(TActorIterator<ACameraActor> It(GetWorld());It;++It)
    {
        if(auto* Camera=It->GetCameraComponent();Camera && Camera->ProjectionMode==ECameraProjectionMode::Orthographic)
        { Camera->OrthoWidth=FMath::Max(Layout.FieldHalfExtent.Y*2.4f,Layout.FieldHalfExtent.X*2.4f*Camera->AspectRatio); }
    }
    LayoutLoadError.Reset();
    UE_LOG(LogLKBattle,Log,TEXT("[Layout] Loaded '%s': %d terrain/objects, %d waves, cards=%d silver=%.2f/%.0f. Standalone; no expedition writes."),
        *Layout.Title,Layout.Objects.Num(),CurrentEncounter.Waves.Num(),CurrentEncounter.EnemyCards.Num(),CurrentEncounter.EnemySilverPerSecond,CurrentEncounter.EnemySilverCap);
    return true;
}

void ALKBattleGameMode::TickLayoutPreview(float DeltaSeconds)
{
#if WITH_EDITOR
    if(BattleLayoutFile.IsEmpty()) return;
    LayoutPreviewElapsed+=DeltaSeconds;
    if(FParse::Param(FCommandLine::Get(),TEXT("LKLayoutAutoStart")) && IsAuthoredLayout() && Phase==ELKGamePhase::Deployment)
    {
        TryInitPlayerState();
        for(FName Id:AvailableHeroes)
        {
            if(IsHeroDeployed(Id)) continue;
            bool Placed=false;
            for(float Y=-GameData->FieldHalfHeight+220.f;Y<-200.f && !Placed;Y+=250.f)
            for(float X=-GameData->FieldHalfWidth+220.f;X<GameData->FieldHalfWidth-220.f && !Placed;X+=300.f)
            { Placed=DeployHero(ELKTeam::Player,Id,FVector(X,Y,0))==ELKPlayResult::Success; }
        }
        ForceStartBattle();
    }
    FString Capture; float At=3.f,Quit=0.f;
    FParse::Value(FCommandLine::Get(),TEXT("LKLayoutCaptureAt="),At);
    FParse::Value(FCommandLine::Get(),TEXT("LKLayoutQuitAfter="),Quit);
    if(!bLayoutScreenshotTaken && LayoutPreviewElapsed>=At && FParse::Value(FCommandLine::Get(),TEXT("LKLayoutScreenshot="),Capture))
    { FScreenshotRequest::RequestScreenshot(Capture,true,false); bLayoutScreenshotTaken=true; }
    if(Quit>0 && LayoutPreviewElapsed>=Quit) { FPlatformMisc::RequestExit(false); }
#endif
}
