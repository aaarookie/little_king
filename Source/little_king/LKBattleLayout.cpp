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
#include "LKHomeContent.h"
#include "LKCardRules.h"
#include "LKResearchContent.h"
#include "LKExpeditionMercenaryContent.h"
#include "Misc/DateTime.h"
#include "Internationalization/Regex.h"
#include <initializer_list>

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
    bool Fields(const TSharedPtr<FJsonObject>& Obj,std::initializer_list<const TCHAR*> Keys,FString& Error)
    {
        if (!Obj) { Error=TEXT("字段必须是对象"); return false; }
        for (const auto& Pair:Obj->Values)
        {
            bool Known=false; for (const TCHAR* Key:Keys) { if (FString(*Pair.Key).Equals(Key,ESearchCase::CaseSensitive)) { Known=true; break; } }
            if (!Known) { Error=FString(TEXT("不支持的字段："))+FString(*Pair.Key); return false; }
        }
        for (const TCHAR* Key:Keys) { if (!Obj->HasField(Key)) { Error=TEXT("缺少字段：")+FString(Key); return false; } }
        return true;
    }
    bool Number(const TSharedPtr<FJsonObject>& Obj,const TCHAR* Key,double Min,double Max,double& Value,FString& Error,bool Integer=false)
    {
        // UE's TryGet accessors convert strings and booleans; the portable JSON
        // contract requires the declared scalar type before reading its value.
        if (!Obj || !Obj->HasTypedField<EJson::Number>(Key) || !Obj->TryGetNumberField(Key,Value) || !FMath::IsFinite(Value) || Value<Min || Value>Max || (Integer && Value!=FMath::FloorToDouble(Value)))
        { Error=FString::Printf(TEXT("字段 %s 必须是 %g～%g 的%s数值"),Key,Min,Max,Integer?TEXT("整数"):TEXT("有限")); return false; }
        return true;
    }
    bool Text(const TSharedPtr<FJsonObject>& Obj,const TCHAR* Key,FString& Value,FString& Error,int32 Min=1,int32 Max=120)
    {
        if (!Obj || !Obj->HasTypedField<EJson::String>(Key) || !Obj->TryGetStringField(Key,Value) || Value.Len()<Min || Value.Len()>Max)
        { Error=FString::Printf(TEXT("字段 %s 缺失或文本长度非法"),Key); return false; }
        return true;
    }
    bool Identifier(const FString& Id)
    {
        if (Id.IsEmpty() || Id.Len()>80) { return false; }
        auto Alnum=[](TCHAR C) { return (C>='A' && C<='Z') || (C>='a' && C<='z') || (C>='0' && C<='9'); };
        if (!Alnum(Id[0])) { return false; }
        for (TCHAR C:Id) { if (!Alnum(C) && C!='_' && C!='.' && C!='-') { return false; } }
        return !Id.Equals(TEXT("None"),ESearchCase::IgnoreCase);
    }
    bool KnownCard(const FString& Id)
    {
        // Same native catalogs as the editor manifest; no assets/UObjects are loaded.
        for (FName Card:LKHomeContent::DefaultUnlockedCards()) { if (Card.ToString().Equals(Id,ESearchCase::CaseSensitive)) { return true; } }
        for (FName Card:LKCardRules::EnemyOnlyCardIds()) { if (Card.ToString().Equals(Id,ESearchCase::CaseSensitive)) { return true; } }
        for (const auto& Card:LKExpeditionMercenaryContent::All()) { if (Card.Unit.UnitId.ToString().Equals(Id,ESearchCase::CaseSensitive)) { return true; } }
        for (const auto& Card:LKResearchContent::All()) { if (Card.CardId.ToString().Equals(Id,ESearchCase::CaseSensitive)) { return true; } }
        return false;
    }
    bool Timestamp(const FString& Value)
    {
        // The portable subset is explicit: full date/time, mandatory timezone, up to
        // nanosecond precision. Reject date-only, overflow dates and trailing junk.
        const FRegexPattern Pattern(TEXT("^[0-9]{4}-(0[1-9]|1[0-2])-(0[1-9]|[12][0-9]|3[01])T([01][0-9]|2[0-3]):[0-5][0-9]:[0-5][0-9](\\.[0-9]{1,9})?(Z|[+-]([01][0-9]|2[0-3]):[0-5][0-9])$"));
        FRegexMatcher Matcher(Pattern,Value); FDateTime Date;
        return Matcher.FindNext() && Matcher.GetMatchBeginning()==0 && Matcher.GetMatchEnding()==Value.Len()
            && FDateTime::ParseIso8601(*Value,Date);
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
    if (!Text(Root,TEXT("format"),Format,Error) || !Format.Equals(TEXT("little-king.battle-layout"),ESearchCase::CaseSensitive)
        || !Number(Root,TEXT("version"),1,2,Version,Error,true))
    { if (Error.IsEmpty()) { Error=TEXT("地图格式或版本不支持"); } return false; }
    if (Version==1 ? !Fields(Root,{TEXT("format"),TEXT("version"),TEXT("meta"),TEXT("field"),TEXT("settings"),TEXT("objects")},Error)
        : !Fields(Root,{TEXT("format"),TEXT("version"),TEXT("meta"),TEXT("field"),TEXT("settings"),TEXT("objects"),TEXT("enemy"),TEXT("waves")},Error)) { return false; }
    FLKBattleLayout Result;
    const auto Meta=Child(Root,TEXT("meta")); FString MetaId,Description,Created,Updated;
    if (!Fields(Meta,{TEXT("id"),TEXT("title"),TEXT("description"),TEXT("createdAt"),TEXT("updatedAt")},Error)
        || !Text(Meta,TEXT("id"),MetaId,Error,1,80) || !Identifier(MetaId)
        || !Text(Meta,TEXT("title"),Result.Title,Error) || !Text(Meta,TEXT("description"),Description,Error,0,4000)
        || !Text(Meta,TEXT("createdAt"),Created,Error,1,40) || !Text(Meta,TEXT("updatedAt"),Updated,Error,1,40))
    { if(Error.IsEmpty()) Error=TEXT("地图信息 ID 非法"); return false; }
    if (!Timestamp(Created) || !Timestamp(Updated))
    { Error=TEXT("地图时间必须为 ISO 日期时间"); return false; }
    const auto Field=Child(Root,TEXT("field")); const auto Settings=Child(Root,TEXT("settings"));
    double Width=0,Height=0,Seed=0,Time=0,Grid=0;
    if (!Fields(Field,{TEXT("width"),TEXT("height"),TEXT("grid")},Error)
        || !Fields(Settings,{TEXT("encounterId"),TEXT("seed"),TEXT("battleTimeLimit")},Error)
        || !Number(Field,TEXT("grid"),.25,2,Grid,Error) || (Grid!=.25 && Grid!=.5 && Grid!=1 && Grid!=2))
    { if(Error.IsEmpty()) Error=TEXT("网格仅支持 0.25/0.5/1/2"); return false; }
    if (!Number(Field,TEXT("width"),8,100,Width,Error) || !Number(Field,TEXT("height"),8,100,Height,Error)
        || !Number(Child(Root,TEXT("settings")),TEXT("seed"),0,MAX_int32,Seed,Error,true)
        || !Number(Child(Root,TEXT("settings")),TEXT("battleTimeLimit"),1,3600,Time,Error,true)) { return false; }
    Result.FieldHalfExtent=FVector2D(Height*50,Width*50); Result.Seed=int32(Seed); Result.TimeLimit=Time;
    FString EncounterId;
    if(!Text(Child(Root,TEXT("settings")),TEXT("encounterId"),EncounterId,Error)) return false;
    if (!TArray<FString>{TEXT("Patrol"),TEXT("Elite"),TEXT("Boss"),TEXT("Encounter_UndeadPatrol"),TEXT("Encounter_UndeadElite"),TEXT("Encounter_SkeletonKing")}.ContainsByPredicate(
        [&](const FString& Name){return Name.Equals(EncounterId,ESearchCase::CaseSensitive);}))
    { Error=TEXT("试玩加载仅支持内置普通/精英/首领模板"); return false; }
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
        FLKBattleLayoutObject Item; FString Id,Module,Team,Note;
        double X=0,Y=0,W=0,H=0,Rotation=0,Count=0,Spawn=0;
        if (!Fields(Obj,{TEXT("id"),TEXT("moduleId"),TEXT("label"),TEXT("x"),TEXT("y"),TEXT("width"),TEXT("height"),TEXT("rotation"),TEXT("team"),TEXT("count"),TEXT("spawnAtSeconds"),TEXT("note")},Error)
            || !Text(Obj,TEXT("id"),Id,Error,1,80) || !Identifier(Id) || !Text(Obj,TEXT("moduleId"),Module,Error)
            || !Text(Obj,TEXT("label"),Item.Label,Error,1,100) || !Text(Obj,TEXT("team"),Team,Error) || !Text(Obj,TEXT("note"),Note,Error,0,4000)
            || !Number(Obj,TEXT("x"),0,Width,X,Error) || !Number(Obj,TEXT("y"),0,Height,Y,Error)
            || !Number(Obj,TEXT("width"),.1,100,W,Error) || !Number(Obj,TEXT("height"),.1,100,H,Error)
            || !Number(Obj,TEXT("rotation"),-3600,3600,Rotation,Error)
            || !Number(Obj,TEXT("count"),1,20,Count,Error,true) || !Number(Obj,TEXT("spawnAtSeconds"),0,300,Spawn,Error))
        { if(Error.IsEmpty()) Error=TEXT("地图对象 ID 非法"); return false; }
        Item.Id=FName(*Id); Item.ModuleId=FName(*Module); Item.UnitId=UnitForModule(Item.ModuleId);
        Item.Center=Position(X,Y); Item.Size=FVector2D(W*100,H*100); Item.Yaw=90+Rotation;
        const bool bPlayer=Team.Equals(TEXT("player"),ESearchCase::CaseSensitive);
        const bool bEnemy=Team.Equals(TEXT("enemy"),ESearchCase::CaseSensitive);
        const bool bNeutral=Team.Equals(TEXT("neutral"),ESearchCase::CaseSensitive);
        Item.Team=bPlayer?ELKTeam::Player:ELKTeam::Enemy; Item.Count=Count; Item.SpawnTime=Spawn;
        if (Ids.Contains(Item.Id)) { Error=TEXT("地图对象 ID 重复（包括大小写等价）"); return false; } Ids.Add(Item.Id);
        if (!bPlayer && !bEnemy && !bNeutral) { Error=TEXT("非法阵营"); return false; }
        if (!Module.Equals(Module.ToLower(),ESearchCase::CaseSensitive)) { Error=TEXT("模块 ID 须使用规范小写名称"); return false; }
        if ((Item.ModuleId=="player-zone" && !bPlayer) || (Item.ModuleId=="enemy-zone" && !bEnemy))
        { Error=TEXT("部署/出现区的阵营与模块不符"); return false; }
        if (TArray<FName>{"skeleton","skeleton-archer","necromancer","skeleton-giant","skeleton-king"}.Contains(Item.ModuleId) && !bEnemy)
        { Error=TEXT("敌军模块仅支持敌方阵营"); return false; }
        if (Item.UnitId.IsNone() && !IsTerrain(Item.ModuleId) && Item.ModuleId!="player-zone" && Item.ModuleId!="enemy-zone" && Item.ModuleId!="note")
        { Error=TEXT("未知地图模块：")+Module; return false; }
        // Use source doubles/metres and the editor's 1e-7 tolerance. Runtime yaw is a
        // float; rounding it first used to accept/reject different boundary documents.
        const double Radians=FMath::DegreesToRadians(Rotation);
        const double ExtentX=FMath::Abs(FMath::Cos(Radians))*W*.5+FMath::Abs(FMath::Sin(Radians))*H*.5;
        const double ExtentY=FMath::Abs(FMath::Sin(Radians))*W*.5+FMath::Abs(FMath::Cos(Radians))*H*.5;
        if (X-ExtentX < -1.e-7 || X+ExtentX > Width+1.e-7 || Y-ExtentY < -1.e-7 || Y+ExtentY > Height+1.e-7)
        { Error=TEXT("对象完整占地越界：")+Item.Label; return false; }
        if (!Item.UnitId.IsNone())
        {
            if(bNeutral) { Error=TEXT("角色/建筑必须指定阵营"); return false; }
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
        if(!Fields(Enemy,{TEXT("usesCards"),TEXT("cards"),TEXT("silverPerSecond"),TEXT("silverCap"),TEXT("startingSilver")},Error)
            || !Enemy->HasTypedField<EJson::Boolean>(TEXT("usesCards")) || !Enemy->TryGetBoolField(TEXT("usesCards"),Uses) || !Enemy->TryGetArrayField(TEXT("cards"),Cards)
            || Cards->Num()>64 || !Number(Enemy,TEXT("silverPerSecond"),0,100,Rate,Error)
            || !Number(Enemy,TEXT("silverCap"),1,1000,Cap,Error) || !Number(Enemy,TEXT("startingSilver"),0,Cap,Start,Error))
        { if(Error.IsEmpty()) Error=TEXT("敌方经济/牌组配置缺失或非法"); return false; }
        Result.Encounter.bEnemyUsesCards=Uses;
        Result.Encounter.EnemySilverPerSecond=Rate; Result.Encounter.EnemySilverCap=Cap; Result.Encounter.EnemyStartingSilver=Start;
        for(const auto& Value:*Cards)
        {
            FString Id; if(Value->Type!=EJson::String || !Value->TryGetString(Id) || !Identifier(Id) || !KnownCard(Id) || Result.Encounter.EnemyCards.Contains(FName(*Id)))
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
            FString Id,Label,Trigger,Target,UnitId; double X=0,Y=0,Radius=0,Count=0,At=0,Threshold=0;
            if(!Fields(Wave,{TEXT("id"),TEXT("label"),TEXT("trigger"),TEXT("time"),TEXT("targetId"),TEXT("healthThreshold"),TEXT("unitId"),TEXT("count"),TEXT("x"),TEXT("y"),TEXT("radius")},Error)
                || !Text(Wave,TEXT("id"),Id,Error,1,80) || !Identifier(Id) || !Text(Wave,TEXT("label"),Label,Error,1,100) || !Text(Wave,TEXT("trigger"),Trigger,Error)
                || !Text(Wave,TEXT("targetId"),Target,Error,0,80) || !Text(Wave,TEXT("unitId"),UnitId,Error)
                || !Number(Wave,TEXT("x"),0,Width,X,Error) || !Number(Wave,TEXT("y"),0,Height,Y,Error)
                || !Number(Wave,TEXT("radius"),0,10,Radius,Error) || !Number(Wave,TEXT("time"),0,3600,At,Error)
                || !Number(Wave,TEXT("healthThreshold"),.01,.99,Threshold,Error) || !Number(Wave,TEXT("count"),1,50,Count,Error,true))
            { if(Error.IsEmpty()) Error=TEXT("波次字段非法"); return false; }
            if(WaveIds.Contains(FName(*Id)) || Ids.Contains(FName(*Id))) { Error=TEXT("对象/波次 ID 重复"); return false; } WaveIds.Add(FName(*Id));
            FLKWaveEntry Entry; Entry.Time=At; Entry.UnitId=FName(*UnitId); Entry.Count=Count;
            Entry.bUseSpawnLocation=true; Entry.SpawnLocation=Position(X,Y); Entry.SpawnRadius=Radius*100;
            Entry.TriggerTargetTag=FName(*Target); Entry.HealthThreshold=Threshold;
            const FLKUnitRow* WaveUnit=LKUnitContent::Units().Find(Entry.UnitId);
            if(!WaveUnit || !WaveUnit->UnitId.ToString().Equals(UnitId,ESearchCase::CaseSensitive)) { Error=TEXT("波次兵种不存在或大小写不规范：")+UnitId; return false; }
            if(Trigger.Equals(TEXT("time"),ESearchCase::CaseSensitive))
            { if(!Target.IsEmpty()) { Error=TEXT("定时波次 targetId 必须为空"); return false; } Entry.Trigger=ELKWaveTrigger::Time; }
            else if(Trigger.Equals(TEXT("hero-health-below"),ESearchCase::CaseSensitive)) { Entry.Trigger=ELKWaveTrigger::HeroHealthBelow; }
            else if(Trigger.Equals(TEXT("hero-first-death"),ESearchCase::CaseSensitive)) { Entry.Trigger=ELKWaveTrigger::HeroFirstDeath; }
            else { Error=TEXT("未知波次条件"); return false; }
            if(Entry.Trigger!=ELKWaveTrigger::Time)
            {
                if(!Identifier(Target)) { Error=TEXT("条件目标 ID 非法"); return false; }
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
