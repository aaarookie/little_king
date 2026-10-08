#include "LKBattleLayout.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "LKUnitContent.h"
#include "LKEncounterContent.h"
#include "LKBalanceRules.h"

bool FLKBattleLayoutObject::Contains(const FVector& Point, float Padding) const
{
    const FVector Local = FRotator(0,Yaw,0).UnrotateVector(Point-Center);
    return FMath::Abs(Local.X)+Padding <= Size.X*.5f && FMath::Abs(Local.Y)+Padding <= Size.Y*.5f;
}

FName LKBattleLayout::UnitForModule(FName Module)
{
    static const TMap<FName,FName> Map = {
        {"knight","Hero_Knight"},{"mage","Hero_Mage"},{"ranger","Hero_Ranger"},
        {"swordsman","Unit_Swordsman"},{"archer","Unit_Archer"},{"skeleton","Unit_Skeleton"},
        {"skeleton-archer","Unit_SkeletonArcher"},{"necromancer","Hero_Necromancer"},
        {"skeleton-giant","Hero_SkeletonGiant"},{"skeleton-king","Boss_SkeletonKing"},
        {"wall-common","Unit_WallCommon"},{"wall-rare","Unit_WallRare"},{"wall-epic","Unit_WallEpic"},
        {"wall-imperial","Unit_WallImperial"},{"mage-tower","Unit_MageTower"},
        {"healing-station","Unit_HealingStation"},{"siege-cart","Unit_SiegeCart"},
        {"arrow-tower","Building_ArrowTower"},{"barracks","Building_Barracks"}};
    const FName* Id=Map.Find(Module); return Id?*Id:NAME_None;
}

bool LKBattleLayout::IsTerrain(FName Module)
{
    return TArray<FName>{"brush","swamp","campfire","mountain","grass","road","water","flowers","rock","tree"}.Contains(Module);
}

namespace
{
    bool Number(const TSharedPtr<FJsonObject>& Obj,const TCHAR* Key,double Min,double Max,double& Value,FString& Error,bool Integer=false)
    {
        if (!Obj || !Obj->TryGetNumberField(Key,Value) || !FMath::IsFinite(Value) || Value<Min || Value>Max || (Integer && Value!=FMath::FloorToDouble(Value)))
        { Error=FString::Printf(TEXT("字段 %s 必须是 %g～%g 的%s数值"),Key,Min,Max,Integer?TEXT("整数"):TEXT("有限")); return false; }
        return true;
    }
    bool Text(const TSharedPtr<FJsonObject>& Obj,const TCHAR* Key,FString& Value,FString& Error,int32 Min=1,int32 Max=120)
    {
        if (!Obj || !Obj->TryGetStringField(Key,Value) || Value.Len()<Min || Value.Len()>Max)
        { Error=FString::Printf(TEXT("字段 %s 缺失或文本长度非法"),Key); return false; }
        return true;
    }
    bool Identifier(const FString& Id)
    {
        if (Id.IsEmpty() || Id.Len()>80) { return false; }
        for (TCHAR C:Id) { if (!FChar::IsAlnum(C) && C!='_' && C!='.' && C!='-') { return false; } }
        return !Id.Equals(TEXT("None"),ESearchCase::IgnoreCase);
    }
    TSharedPtr<FJsonObject> Child(const TSharedPtr<FJsonObject>& Obj,const TCHAR* Key)
    { const TSharedPtr<FJsonObject>* Result=nullptr; return Obj && Obj->TryGetObjectField(Key,Result)?*Result:nullptr; }
}

bool LKBattleLayout::Parse(const FString& Json,FLKBattleLayout& Out,FString& Error)
{
    Error.Reset();
    if (FTCHARToUTF8(*Json).Length()>MaxFileBytes) { Error=TEXT("地图文件超过 2 MiB"); return false; }
    TSharedPtr<FJsonObject> Root;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Root) || !Root.IsValid())
    { Error=TEXT("不是合法的地图 JSON"); return false; }
    FString Format; double Version=0;
    if (!Text(Root,TEXT("format"),Format,Error) || Format!=TEXT("little-king.battle-layout")
        || !Number(Root,TEXT("version"),1,2,Version,Error,true))
    { if (Error.IsEmpty()) { Error=TEXT("地图格式或版本不支持"); } return false; }
    FLKBattleLayout Result;
    if (!Text(Child(Root,TEXT("meta")),TEXT("title"),Result.Title,Error)) { return false; }
    const auto Field=Child(Root,TEXT("field")); double Width=0,Height=0,Seed=0,Time=0;
    if (!Number(Field,TEXT("width"),8,100,Width,Error) || !Number(Field,TEXT("height"),8,100,Height,Error)
        || !Number(Child(Root,TEXT("settings")),TEXT("seed"),0,MAX_int32,Seed,Error,true)
        || !Number(Child(Root,TEXT("settings")),TEXT("battleTimeLimit"),1,3600,Time,Error,true)) { return false; }
    Result.FieldHalfExtent=FVector2D(Height*50,Width*50); Result.Seed=int32(Seed); Result.TimeLimit=Time;
    FString EncounterId;
    if(!Text(Child(Root,TEXT("settings")),TEXT("encounterId"),EncounterId,Error)) return false;
    if(const auto* Template=LKEncounterContent::FindBuiltIn(FName(*EncounterId)))
    { Result.Encounter.Rank=Template->Rank; Result.Encounter.RewardTier=Template->RewardTier; }
    else { Error=TEXT("试玩加载仅支持内置普通/精英/首领模板"); return false; }
    auto Position=[&](double X,double Y) { return FVector((Height*.5-Y)*100,(X-Width*.5)*100,0); };
    const TArray<TSharedPtr<FJsonValue>>* Objects=nullptr;
    if (!Root->TryGetArrayField(TEXT("objects"),Objects) || Objects->Num()>500)
    { Error=TEXT("objects 必须为数组，最多 500 个对象"); return false; }
    TSet<FName> Ids,HeroIds;
    int32 InitialCounts[2]={0,0};
    for (const auto& Value:*Objects)
    {
        const auto Obj=Value->Type==EJson::Object?Value->AsObject():nullptr;
        FLKBattleLayoutObject Item; FString Id,Module,Team;
        double X=0,Y=0,W=0,H=0,Rotation=0,Count=0,Spawn=0;
        if (!Text(Obj,TEXT("id"),Id,Error) || !Identifier(Id) || !Text(Obj,TEXT("moduleId"),Module,Error)
            || !Text(Obj,TEXT("label"),Item.Label,Error) || !Text(Obj,TEXT("team"),Team,Error)
            || !Number(Obj,TEXT("x"),0,Width,X,Error) || !Number(Obj,TEXT("y"),0,Height,Y,Error)
            || !Number(Obj,TEXT("width"),.1,100,W,Error) || !Number(Obj,TEXT("height"),.1,100,H,Error)
            || !Number(Obj,TEXT("rotation"),-3600,3600,Rotation,Error)
            || !Number(Obj,TEXT("count"),1,20,Count,Error,true) || !Number(Obj,TEXT("spawnAtSeconds"),0,3600,Spawn,Error))
        { if(Error.IsEmpty()) Error=TEXT("地图对象 ID 非法"); return false; }
        Item.Id=FName(*Id); Item.ModuleId=FName(*Module); Item.UnitId=UnitForModule(Item.ModuleId);
        Item.Center=Position(X,Y); Item.Size=FVector2D(W*100,H*100); Item.Yaw=90+Rotation;
        Item.Team=Team==TEXT("player")?ELKTeam::Player:ELKTeam::Enemy; Item.Count=Count; Item.SpawnTime=Spawn;
        if (Ids.Contains(Item.Id)) { Error=TEXT("地图对象 ID 重复（包括大小写等价）"); return false; } Ids.Add(Item.Id);
        if (Team!=TEXT("player") && Team!=TEXT("enemy") && Team!=TEXT("neutral")) { Error=TEXT("非法阵营"); return false; }
        if (Item.UnitId.IsNone() && !IsTerrain(Item.ModuleId) && Item.ModuleId!="player-zone" && Item.ModuleId!="enemy-zone" && Item.ModuleId!="note")
        { Error=TEXT("未知地图模块：")+Module; return false; }
        const FVector U=FRotator(0,Item.Yaw,0).Vector(); const FVector V=FVector(-U.Y,U.X,0);
        const FVector Extent=U.GetAbs()*Item.Size.X*.5f+V.GetAbs()*Item.Size.Y*.5f;
        if (FMath::Abs(Item.Center.X)+Extent.X>Result.FieldHalfExtent.X+.01f || FMath::Abs(Item.Center.Y)+Extent.Y>Result.FieldHalfExtent.Y+.01f)
        { Error=TEXT("对象完整占地越界：")+Item.Label; return false; }
        if (!Item.UnitId.IsNone())
        {
            if(Team==TEXT("neutral")) { Error=TEXT("角色/建筑必须指定阵营"); return false; }
            const FLKUnitRow* Row=LKUnitContent::Units().Find(Item.UnitId);
            if(!Row) { Error=TEXT("地图引用未知兵种"); return false; }
            const bool Hero=Row->UnitClass==ELKUnitClass::Hero || Row->UnitClass==ELKUnitClass::Boss;
            if (Hero && (Count!=1 || Spawn!=0)) { Error=TEXT("地图英雄须单个、开局出现；后续英雄请用波次添加"); return false; }
            if(Spawn>0 && Item.Team!=ELKTeam::Enemy) { Error=TEXT("延时地图对象目前仅支持敌方，请用开局我方部署"); return false; }
            if (Hero && Item.Team==ELKTeam::Enemy)
            {
                if (HeroIds.Contains(Item.UnitId)) { Error=TEXT("初始敌方英雄类型不能重复，请将同类后续英雄放入波次"); return false; }
                HeroIds.Add(Item.UnitId); Result.Encounter.EnemyHeroIds.Add(Item.UnitId);
            }
            if (Spawn==0) { InitialCounts[int32(Item.Team)]+=Count; }
        }
        Result.Objects.Add(Item);
    }
    if (Result.Encounter.EnemyHeroIds.IsEmpty()) { Error=TEXT("至少放置一名开局敌方英雄/首领作为遭遇目标"); return false; }
    Result.Encounter.EncounterId="Encounter_AuthoredLayout";
    Result.Encounter.DisplayName=FText::FromString(Result.Title);
    Result.Encounter.EnemySpell.bEnabled=false;
    Result.Encounter.AI.bEnableCounter=Result.Encounter.AI.bEnablePush=false;
    Result.Encounter.AI.bEnableFocus=false;
    if(Version==2)
    {
        const auto Enemy=Child(Root,TEXT("enemy")); double Rate=0,Cap=0,Start=0; bool Uses=false;
        const TArray<TSharedPtr<FJsonValue>>* Cards=nullptr;
        if(!Enemy || !Enemy->TryGetBoolField(TEXT("usesCards"),Uses) || !Enemy->TryGetArrayField(TEXT("cards"),Cards)
            || Cards->Num()>64 || !Number(Enemy,TEXT("silverPerSecond"),0,100,Rate,Error)
            || !Number(Enemy,TEXT("silverCap"),1,1000,Cap,Error) || !Number(Enemy,TEXT("startingSilver"),0,Cap,Start,Error))
        { if(Error.IsEmpty()) Error=TEXT("敌方经济/牌组配置缺失或非法"); return false; }
        Result.Encounter.bEnemyUsesCards=Uses;
        Result.Encounter.EnemySilverPerSecond=Rate; Result.Encounter.EnemySilverCap=Cap; Result.Encounter.EnemyStartingSilver=Start;
        for(const auto& Value:*Cards)
        {
            FString Id; if(!Value->TryGetString(Id) || !Identifier(Id) || Result.Encounter.EnemyCards.Contains(FName(*Id)))
            { Error=TEXT("敌方牌组含非法或重复卡牌"); return false; }
            Result.Encounter.EnemyCards.Add(FName(*Id));
        }
        if(Uses && Cards->IsEmpty()) { Error=TEXT("启用敌方手牌时须至少一张卡"); return false; }
        const TArray<TSharedPtr<FJsonValue>>* Waves=nullptr;
        if(!Root->TryGetArrayField(TEXT("waves"),Waves) || Waves->Num()>64) { Error=TEXT("波次数量必须在 0～64"); return false; }
        TSet<FName> WaveIds;
        for(const auto& Value:*Waves)
        {
            const auto Wave=Value->Type==EJson::Object?Value->AsObject():nullptr;
            FString Id,Trigger,Target,UnitId; double X=0,Y=0,Radius=0,Count=0,At=0,Threshold=0;
            if(!Text(Wave,TEXT("id"),Id,Error) || !Identifier(Id) || !Text(Wave,TEXT("trigger"),Trigger,Error)
                || !Text(Wave,TEXT("targetId"),Target,Error,0,80) || !Text(Wave,TEXT("unitId"),UnitId,Error)
                || !Number(Wave,TEXT("x"),0,Width,X,Error) || !Number(Wave,TEXT("y"),0,Height,Y,Error)
                || !Number(Wave,TEXT("radius"),0,10,Radius,Error) || !Number(Wave,TEXT("time"),0,3600,At,Error)
                || !Number(Wave,TEXT("healthThreshold"),.01,.99,Threshold,Error) || !Number(Wave,TEXT("count"),1,50,Count,Error,true))
            { if(Error.IsEmpty()) Error=TEXT("波次字段非法"); return false; }
            if(WaveIds.Contains(FName(*Id))) { Error=TEXT("波次 ID 重复"); return false; } WaveIds.Add(FName(*Id));
            FLKWaveEntry Entry; Entry.Time=At; Entry.UnitId=FName(*UnitId); Entry.Count=Count;
            Entry.bUseSpawnLocation=true; Entry.SpawnLocation=Position(X,Y); Entry.SpawnRadius=Radius*100;
            Entry.TriggerTargetTag=FName(*Target); Entry.HealthThreshold=Threshold;
            if(!LKUnitContent::Units().Contains(Entry.UnitId)) { Error=TEXT("波次兵种不存在：")+UnitId; return false; }
            if(Trigger==TEXT("time")) { Entry.Trigger=ELKWaveTrigger::Time; }
            else if(Trigger==TEXT("hero-health-below")) { Entry.Trigger=ELKWaveTrigger::HeroHealthBelow; }
            else if(Trigger==TEXT("hero-first-death")) { Entry.Trigger=ELKWaveTrigger::HeroFirstDeath; }
            else { Error=TEXT("未知波次条件"); return false; }
            if(Entry.Trigger!=ELKWaveTrigger::Time)
            {
                const auto* TargetObject=Result.Objects.FindByPredicate([&](const auto& O){return O.Id==Entry.TriggerTargetTag;});
                const auto* Row=TargetObject?LKUnitContent::Units().Find(TargetObject->UnitId):nullptr;
                if(!Row || (Row->UnitClass!=ELKUnitClass::Hero && Row->UnitClass!=ELKUnitClass::Boss))
                { Error=TEXT("条件波次必须引用地图里的开局英雄对象 ID"); return false; }
            }
            Result.Encounter.Waves.Add(Entry);
        }
    }
    for(const auto& Item:Result.Objects)
    {
        if(Item.UnitId.IsNone() || Item.SpawnTime<=0) { continue; }
        FLKWaveEntry Entry; Entry.Time=Item.SpawnTime; Entry.UnitId=Item.UnitId; Entry.Count=Item.Count;
        Entry.bUseSpawnLocation=true; Entry.SpawnLocation=Item.Center; Entry.SpawnRadius=FMath::Max(200.f,Item.Size.GetMax()*.5f);
        Result.Encounter.Waves.Add(Entry);
    }
    if(Result.Encounter.Waves.Num()>64) { Error=TEXT("包含旧延时对象后波次超过 64 条"); return false; }
    LKBalanceRules::EnsureSnapshot(Result.Encounter);
    Out=MoveTemp(Result); return true;
}

bool LKBattleLayout::Load(const FString& Path,FLKBattleLayout& Out,FString& Error)
{
    FString Full=FPaths::IsRelative(Path)?FPaths::Combine(FPaths::ProjectDir(),Path):Path;
    const int64 Bytes=IFileManager::Get().FileSize(*Full);
    if(Bytes<0 || Bytes>MaxFileBytes) { Error=TEXT("地图文件不存在或超过 2 MiB：")+Full; return false; }
    FString Json;
    if(!FFileHelper::LoadFileToString(Json,*Full)) { Error=TEXT("无法读取地图文件：")+Full; return false; }
    return Parse(Json,Out,Error);
}
