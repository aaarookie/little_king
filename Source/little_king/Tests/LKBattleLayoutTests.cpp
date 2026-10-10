#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "../ALKBattleGameMode.h"
#include "../ALKBattleTerrain.h"
#include "../ALKOpponentBrain.h"
#include "../ALKPlayerController.h"
#include "../ALKUnitBase.h"
#include "../LKBattleLayout.h"
#include "../LKGameplayHelpers.h"
#include "../ULKGameData.h"
#include "../ULKRunSubsystem.h"

namespace
{
constexpr EAutomationTestFlags LayoutFlags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;
FString ExamplePath() { return FPaths::Combine(FPaths::ProjectDir(),TEXT("Tools/BattleDesigner/examples/terrain-ambush.json")); }
TSharedPtr<FJsonObject> JsonRoot(const FString& Text)
{
    TSharedPtr<FJsonObject> Root;
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Root);
    return Root;
}
FString JsonText(const TSharedPtr<FJsonObject>& Root)
{
    FString Text; FJsonSerializer::Serialize(Root.ToSharedRef(),TJsonWriterFactory<>::Create(&Text)); return Text;
}
struct FLayoutWorld
{
    FTestWorldWrapper Env;
    UWorld* World=nullptr;
    ALKBattleGameMode* GM=nullptr;
    bool Open()
    {
        if(!Env.CreateTestWorld(EWorldType::Game)) return false;
        World=Env.GetTestWorld();
        World->GetWorldSettings()->DefaultGameMode=ALKBattleGameMode::StaticClass();
        auto* Run=World->GetGameInstance()->GetSubsystem<ULKRunSubsystem>();
        Run->ConfigureStorage(TEXT("LittleKing_LayoutTest_")+FGuid::NewGuid().ToString(EGuidFormats::Digits));
        Run->SetAutoSaveEnabled(false);
        if(!Env.BeginPlayInTestWorld()) return false;
        GM=World->GetAuthGameMode<ALKBattleGameMode>();
        World->SpawnActor<ALKPlayerController>(); GM->Tick(0.f);
        GM->GetGameData()->bDrawDebugShapes=false;
        return GM->GetPhase()==ELKGamePhase::Deployment;
    }
    bool DeployAll()
    {
        for(int32 I=0;I<GM->AvailableHeroes.Num();++I)
        { if(GM->DeployHero(ELKTeam::Player,GM->AvailableHeroes[I],FVector((I-1)*500.f,-1450.f,0))!=ELKPlayResult::Success) return false; }
        return GM->CanStartBattle();
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKLayoutParserTest,"LittleKing.Scenario.LayoutParserBoundaries",LayoutFlags)
bool FLKLayoutParserTest::RunTest(const FString& Parameters)
{
    FString Text,Error;
    if(!TestTrue(TEXT("Checked-in editor example exists"),FFileHelper::LoadFileToString(Text,*ExamplePath()))) return false;
    FLKBattleLayout Good;
    if(!TestTrue(TEXT("Editor v2 document parses"),LKBattleLayout::Parse(Text,Good,Error))) { AddInfo(Error); return false; }
    TestEqual(TEXT("40 by 24 meter design converts to UE half extents"),Good.FieldHalfExtent,FVector2D(1200,2000));
    TestEqual(TEXT("Three editable event waves"),Good.Encounter.Waves.Num(),3);
    TestEqual(TEXT("Small enemy deck preserved"),Good.Encounter.EnemyCards.Num(),3);
    TestEqual(TEXT("Authored silver regeneration"),Good.Encounter.EnemySilverPerSecond,1.5f);
    const auto* Commander=Good.Objects.FindByPredicate([](const auto& O){return O.ModuleId=="necromancer";});
    if(!TestNotNull(TEXT("Commander object decoded"),Commander)) return false;
    TestEqual(TEXT("Screen x/y map to UE Y/-X"),Commander->Center,FVector(200,1600,0));
    TestEqual(TEXT("Clockwise orientation starts at UE yaw90"),Commander->Yaw,90.f);
    TestEqual(TEXT("First-death reinforcement allows enemy hero"),Good.Encounter.Waves.Last().UnitId,FName("Hero_SkeletonGiant"));
    const auto* Zone=Good.Objects.FindByPredicate([](const auto& O){return O.ModuleId=="player-zone";});
    TestTrue(TEXT("Deployment OBB contains its center"),Zone && Zone->Contains(FVector(0,-1500,0),50.f));
    TestFalse(TEXT("Deployment OBB rejects a body crossing its edge"),Zone && Zone->Contains(FVector(0,-1120,0),50.f));

    auto Reject=[&](const TCHAR* Name,TFunctionRef<void(TSharedPtr<FJsonObject>)> Mutate)
    {
        auto Root=JsonRoot(Text); Mutate(Root);
        FLKBattleLayout Kept=Good;
        TestFalse(Name,LKBattleLayout::Parse(JsonText(Root),Kept,Error));
        TestFalse(TEXT("Invalid document returns an actionable error"),Error.IsEmpty());
        TestEqual(TEXT("Failed parse preserves prior layout title"),Kept.Title,Good.Title);
        TestEqual(TEXT("Failed parse preserves prior object count"),Kept.Objects.Num(),Good.Objects.Num());
        TestEqual(TEXT("Failed parse preserves prior wave count"),Kept.Encounter.Waves.Num(),Good.Encounter.Waves.Num());
    };
    Reject(TEXT("Future schema version rejected"),[](auto R){R->SetNumberField(TEXT("version"),3);});
    Reject(TEXT("Duplicate IDs compare case-insensitively"),[](auto R){auto A=R->GetArrayField(TEXT("objects")); A[1]->AsObject()->SetStringField(TEXT("id"),A[0]->AsObject()->GetStringField(TEXT("id")).ToUpper());});
    Reject(TEXT("Unknown module rejected"),[](auto R){R->GetArrayField(TEXT("objects"))[1]->AsObject()->SetStringField(TEXT("moduleId"),TEXT("mystery-terrain"));});
    Reject(TEXT("Rotated footprint must fit entirely in field"),[](auto R){auto O=R->GetArrayField(TEXT("objects"))[1]->AsObject();O->SetNumberField(TEXT("x"),1);O->SetNumberField(TEXT("rotation"),45);});
    Reject(TEXT("Fractional group count rejected"),[](auto R){R->GetArrayField(TEXT("objects"))[6]->AsObject()->SetNumberField(TEXT("count"),1.5);});
    Reject(TEXT("Neutral combat unit rejected"),[](auto R){R->GetArrayField(TEXT("objects"))[6]->AsObject()->SetStringField(TEXT("team"),TEXT("neutral"));});
    Reject(TEXT("Missing v2 wave list rejected"),[](auto R){R->RemoveField(TEXT("waves"));});
    Reject(TEXT("Non-hero trigger target rejected"),[](auto R){R->GetArrayField(TEXT("waves"))[1]->AsObject()->SetStringField(TEXT("targetId"),R->GetArrayField(TEXT("objects"))[6]->AsObject()->GetStringField(TEXT("id")));});
    Reject(TEXT("Missing trigger target rejected"),[](auto R){R->GetArrayField(TEXT("waves"))[1]->AsObject()->SetStringField(TEXT("targetId"),TEXT("does-not-exist"));});
    Reject(TEXT("Unknown wave trigger rejected"),[](auto R){R->GetArrayField(TEXT("waves"))[0]->AsObject()->SetStringField(TEXT("trigger"),TEXT("repeated-death"));});
    Reject(TEXT("Unknown reinforcement unit rejected"),[](auto R){R->GetArrayField(TEXT("waves"))[0]->AsObject()->SetStringField(TEXT("unitId"),TEXT("Unit_Unknown"));});
    Reject(TEXT("Zero-size wave rejected"),[](auto R){R->GetArrayField(TEXT("waves"))[0]->AsObject()->SetNumberField(TEXT("count"),0);});
    Reject(TEXT("Oversize wave radius rejected"),[](auto R){R->GetArrayField(TEXT("waves"))[0]->AsObject()->SetNumberField(TEXT("radius"),11);});
    Reject(TEXT("Negative silver regeneration rejected"),[](auto R){R->GetObjectField(TEXT("enemy"))->SetNumberField(TEXT("silverPerSecond"),-1);});
    Reject(TEXT("Starting silver cannot exceed capacity"),[](auto R){R->GetObjectField(TEXT("enemy"))->SetNumberField(TEXT("startingSilver"),999);});
    Reject(TEXT("Enabled empty enemy hand rejected"),[](auto R){R->GetObjectField(TEXT("enemy"))->SetArrayField(TEXT("cards"),{});});
    Reject(TEXT("Duplicate enemy cards rejected"),[](auto R){auto E=R->GetObjectField(TEXT("enemy"));auto A=E->GetArrayField(TEXT("cards"));const auto Duplicate=A[0];A.Add(Duplicate);E->SetArrayField(TEXT("cards"),A);});
    // The Node mutation suite uses this same checked-in editor document and categories.
    for (const FString Container : {TEXT("root"),TEXT("meta"),TEXT("field"),TEXT("settings"),TEXT("enemy"),TEXT("object"),TEXT("wave")})
    {
        Reject(*FString::Printf(TEXT("Unknown fields rejected in %s"),*Container),[Container](auto R)
        {
            auto Target=Container==TEXT("root") ? R : Container==TEXT("object") ? R->GetArrayField(TEXT("objects"))[0]->AsObject()
                : Container==TEXT("wave") ? R->GetArrayField(TEXT("waves"))[0]->AsObject() : R->GetObjectField(Container);
            Target->SetNumberField(TEXT("unknownField"),1);
        });
    }
    for (const FString Id : {TEXT("None"),TEXT("nOnE"),TEXT("_leading"),TEXT("-leading"),TEXT("角色"),TEXT("space id")})
    {
        Reject(TEXT("Portable ASCII object identifier"),[Id](auto R){R->GetArrayField(TEXT("objects"))[0]->AsObject()->SetStringField(TEXT("id"),Id);});
        Reject(TEXT("Portable ASCII map identifier"),[Id](auto R){R->GetObjectField(TEXT("meta"))->SetStringField(TEXT("id"),Id);});
        Reject(TEXT("Portable ASCII wave identifier"),[Id](auto R){R->GetArrayField(TEXT("waves"))[0]->AsObject()->SetStringField(TEXT("id"),Id);});
    }
    for (const FString Date : {TEXT("2026-02-30T00:00:00Z"),TEXT("2026-10-10"),TEXT("2026-10-10T12:00:00Zjunk"),TEXT("2026-10-10T24:00:00Z"),TEXT("0000-01-01T00:00:00Z"),TEXT("2026-10-10T12:00:00+25:00")})
    { Reject(TEXT("Invalid timestamp rejected"),[Date](auto R){R->GetObjectField(TEXT("meta"))->SetStringField(TEXT("createdAt"),Date);}); }
    Reject(TEXT("Unknown card rejected while parsing"),[](auto R){R->GetObjectField(TEXT("enemy"))->SetArrayField(TEXT("cards"),{MakeShared<FJsonValueString>(TEXT("Card_Unknown"))});});
    Reject(TEXT("Module casing is canonical"),[](auto R){R->GetArrayField(TEXT("objects"))[0]->AsObject()->SetStringField(TEXT("moduleId"),TEXT("Brush"));});
    Reject(TEXT("Format casing is canonical"),[](auto R){R->SetStringField(TEXT("format"),TEXT("Little-King.Battle-Layout"));});
    Reject(TEXT("Encounter casing is canonical"),[](auto R){R->GetObjectField(TEXT("settings"))->SetStringField(TEXT("encounterId"),TEXT("patrol"));});
    Reject(TEXT("Card casing is canonical"),[](auto R){R->GetObjectField(TEXT("enemy"))->SetArrayField(TEXT("cards"),{MakeShared<FJsonValueString>(TEXT("unit_skeleton"))});});
    Reject(TEXT("Unit casing is canonical"),[](auto R){R->GetArrayField(TEXT("waves"))[0]->AsObject()->SetStringField(TEXT("unitId"),TEXT("unit_skeleton"));});
    Reject(TEXT("Team casing is canonical"),[](auto R){R->GetArrayField(TEXT("objects"))[0]->AsObject()->SetStringField(TEXT("team"),TEXT("Neutral"));});
    Reject(TEXT("Trigger casing is canonical"),[](auto R){R->GetArrayField(TEXT("waves"))[0]->AsObject()->SetStringField(TEXT("trigger"),TEXT("Time"));});
    Reject(TEXT("Field name casing is canonical"),[](auto R){R->GetObjectField(TEXT("meta"))->SetStringField(TEXT("Title"),TEXT("Wrong case"));});
    // TryGet* in UE performs coercion; import must reject it just like JS/schema.
    Reject(TEXT("String version is not a JSON number"),[](auto R){R->SetStringField(TEXT("version"),TEXT("2"));});
    Reject(TEXT("String field width is not a JSON number"),[](auto R){R->GetObjectField(TEXT("field"))->SetStringField(TEXT("width"),TEXT("40"));});
    Reject(TEXT("Boolean grid is not a JSON number"),[](auto R){R->GetObjectField(TEXT("field"))->SetBoolField(TEXT("grid"),true);});
    Reject(TEXT("String seed is not a JSON number"),[](auto R){R->GetObjectField(TEXT("settings"))->SetStringField(TEXT("seed"),TEXT("2026"));});
    Reject(TEXT("Boolean object delay is not a JSON number"),[](auto R){R->GetArrayField(TEXT("objects"))[6]->AsObject()->SetBoolField(TEXT("spawnAtSeconds"),true);});
    Reject(TEXT("String wave count is not a JSON number"),[](auto R){R->GetArrayField(TEXT("waves"))[0]->AsObject()->SetStringField(TEXT("count"),TEXT("2"));});
    Reject(TEXT("Boolean wave time is not a JSON number"),[](auto R){R->GetArrayField(TEXT("waves"))[0]->AsObject()->SetBoolField(TEXT("time"),false);});
    Reject(TEXT("String card switch is not a JSON boolean"),[](auto R){R->GetObjectField(TEXT("enemy"))->SetStringField(TEXT("usesCards"),TEXT("false"));});
    Reject(TEXT("Numeric card switch is not a JSON boolean"),[](auto R){R->GetObjectField(TEXT("enemy"))->SetNumberField(TEXT("usesCards"),1);});
    Reject(TEXT("Numeric map ID is not a JSON string"),[](auto R){R->GetObjectField(TEXT("meta"))->SetNumberField(TEXT("id"),123);});
    Reject(TEXT("Numeric title is not a JSON string"),[](auto R){R->GetObjectField(TEXT("meta"))->SetNumberField(TEXT("title"),123);});
    Reject(TEXT("Boolean description is not a JSON string"),[](auto R){R->GetObjectField(TEXT("meta"))->SetBoolField(TEXT("description"),false);});
    Reject(TEXT("Numeric wave label is not a JSON string"),[](auto R){R->GetArrayField(TEXT("waves"))[0]->AsObject()->SetNumberField(TEXT("label"),123);});
    Reject(TEXT("Card array entries must be JSON strings"),[](auto R){R->GetObjectField(TEXT("enemy"))->SetArrayField(TEXT("cards"),{MakeShared<FJsonValueNumber>(123)});});
    Reject(TEXT("Timestamp cannot have trailing newline"),[](auto R){R->GetObjectField(TEXT("meta"))->SetStringField(TEXT("createdAt"),TEXT("2026-10-10T12:00:00Z\n"));});
    Reject(TEXT("Missing wave label rejected"),[](auto R){R->GetArrayField(TEXT("waves"))[0]->AsObject()->RemoveField(TEXT("label"));});
    Reject(TEXT("Cross-object-wave IDs reject case variants"),[](auto R){R->GetArrayField(TEXT("waves"))[0]->AsObject()->SetStringField(TEXT("id"),R->GetArrayField(TEXT("objects"))[0]->AsObject()->GetStringField(TEXT("id")).ToUpper());});
    Reject(TEXT("Time wave has no target"),[](auto R){R->GetArrayField(TEXT("waves"))[0]->AsObject()->SetStringField(TEXT("targetId"),TEXT("unrelated"));});
    Reject(TEXT("Invalid grid rejected"),[](auto R){R->GetObjectField(TEXT("field"))->SetNumberField(TEXT("grid"),.75);});
    Reject(TEXT("Legacy object delay matches 300 second editor bound"),[](auto R){R->GetArrayField(TEXT("objects"))[6]->AsObject()->SetNumberField(TEXT("spawnAtSeconds"),301);});
    auto CaseReference=JsonRoot(Text);
    auto Conditional=CaseReference->GetArrayField(TEXT("waves"))[1]->AsObject();
    Conditional->SetStringField(TEXT("targetId"),Conditional->GetStringField(TEXT("targetId")).ToUpper());
    FLKBattleLayout CaseLayout;
    TestTrue(TEXT("Case-insensitive stable hero reference accepted"),LKBattleLayout::Parse(JsonText(CaseReference),CaseLayout,Error));
    for (const FString Date : {TEXT("2024-02-29T23:59:59Z"),TEXT("2026-10-10T12:00:00.123456789+08:00"),TEXT("0001-01-01T00:00:00Z")})
    {
        auto ValidDate=JsonRoot(Text); ValidDate->GetObjectField(TEXT("meta"))->SetStringField(TEXT("createdAt"),Date);
        TestTrue(TEXT("Portable timestamp legal boundaries accepted"),LKBattleLayout::Parse(JsonText(ValidDate),CaseLayout,Error));
    }
    FLKBattleLayout Kept=Good;
    TestFalse(TEXT("Malformed JSON rejected without mutation"),LKBattleLayout::Parse(TEXT("{"),Kept,Error));
    TestEqual(TEXT("Malformed JSON retains old title"),Kept.Title,Good.Title);
    auto Legacy=JsonRoot(Text); Legacy->SetNumberField(TEXT("version"),1); Legacy->RemoveField(TEXT("enemy"));Legacy->RemoveField(TEXT("waves"));
    TestTrue(TEXT("Legacy v1 layout remains readable"),LKBattleLayout::Parse(JsonText(Legacy),Kept,Error));
    TestFalse(TEXT("Legacy preview does not silently enable enemy cards"),Kept.Encounter.bEnemyUsesCards);
    Legacy->SetObjectField(TEXT("enemy"),JsonRoot(Text)->GetObjectField(TEXT("enemy")));
    TestFalse(TEXT("V1 cannot smuggle unversioned v2 fields"),LKBattleLayout::Parse(JsonText(Legacy),Kept,Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKLayoutApplicationTest,"LittleKing.Scenario.LayoutRuntimeAndDeployment",LayoutFlags)
bool FLKLayoutApplicationTest::RunTest(const FString& Parameters)
{
    FLayoutWorld S; if(!TestTrue(TEXT("Independent preview world starts"),S.Open())) return false;
    FString Error; FLKBattleLayout Layout;
    if(!TestTrue(TEXT("Example loads"),LKBattleLayout::Load(ExamplePath(),Layout,Error))) return false;
    if(!TestTrue(TEXT("Example applies to real battle world"),S.GM->ApplyBattleLayout(Layout,Error))) { AddInfo(Error); return false; }
    TestTrue(TEXT("Custom battle mode enabled"),S.GM->IsAuthoredLayout());
    TestEqual(TEXT("Title exposed for HUD"),S.GM->GetLayoutTitle(),Layout.Title);
    TestEqual(TEXT("Initial roster has six archers and one commander"),S.GM->CountAliveUnits(ELKTeam::Enemy),7);
    int32 Terrains=0,HighlandArchers=0; bool bCommanderTagged=false;
    const auto* Commander=Layout.Objects.FindByPredicate([](const auto& O){return O.ModuleId=="necromancer";});
    for(TActorIterator<ALKBattleTerrain> It(S.World);It;++It) ++Terrains;
    for(TActorIterator<ALKUnitBase> It(S.World);It;++It)
    {
        if(It->GetUnitId()=="Hero_Necromancer" && Commander && It->ActorHasTag(Commander->Id)) bCommanderTagged=true;
        if(It->GetUnitId()=="Unit_SkeletonArcher" && ALKBattleTerrain::HighlandAt(*It)) ++HighlandArchers;
    }
    TestEqual(TEXT("All five terrain objects instantiate"),Terrains,5);
    TestEqual(TEXT("All three highland archers stay on authored plateau"),HighlandArchers,3);
    TestTrue(TEXT("Hero carries stable layout tag for condition waves"),bCommanderTagged);
    FLKBattleLayout Bad=Layout;Bad.Encounter.EnemyCards.Add("Card_DoesNotExist");
    TestFalse(TEXT("Missing card rejected before mutating existing world"),S.GM->ApplyBattleLayout(Bad,Error));
    TestEqual(TEXT("Invalid import preserves existing units"),S.GM->CountAliveUnits(ELKTeam::Enemy),7);
    TestTrue(TEXT("Valid layout may be replaced before player deployment"),S.GM->ApplyBattleLayout(Layout,Error));
    TestEqual(TEXT("Reload replaces units instead of duplicating"),S.GM->CountAliveUnits(ELKTeam::Enemy),7);
    TestTrue(TEXT("Player camp anchor lies inside deployment area"),S.GM->IsInsideLayoutDeploymentZone(FVector(0,-1450,0)));
    TestFalse(TEXT("Central lane lies outside deployment area"),S.GM->IsInsideLayoutDeploymentZone(FVector(0,-300,0)));
    TestEqual(TEXT("Outside-zone hero placement rejected"),S.GM->DeployHero(ELKTeam::Player,S.GM->AvailableHeroes[0],FVector(0,-300,0)),ELKPlayResult::InvalidLocation);
    if(!TestTrue(TEXT("All three heroes can deploy inside authored zone"),S.DeployAll())) return false;
    TestFalse(TEXT("Reload refused after hero deployment"),S.GM->ApplyBattleLayout(Layout,Error));
    S.GM->ForceStartBattle();
    TestEqual(TEXT("Authored map becomes playable"),S.GM->GetPhase(),ELKGamePhase::Battle);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKLayoutVictoryTest,"LittleKing.Scenario.LayoutDeathReinforcementVictory",LayoutFlags)
bool FLKLayoutVictoryTest::RunTest(const FString& Parameters)
{
    FLayoutWorld S; if(!TestTrue(TEXT("Independent preview world starts"),S.Open())) return false;
    FString Error; FLKBattleLayout Layout;
    if(!TestTrue(TEXT("Example loads"),LKBattleLayout::Load(ExamplePath(),Layout,Error))) return false;
    FLKWaveEntry Death=Layout.Encounter.Waves.Last(); Death.Time=1.f;
    Layout.Encounter.Waves={Death};Layout.Encounter.bEnemyUsesCards=false;Layout.Encounter.EnemyCards.Reset();
    if(!TestTrue(TEXT("Death-reinforcement fixture applies"),S.GM->ApplyBattleLayout(Layout,Error))) { AddInfo(Error); return false; }
    if(!TestTrue(TEXT("Player deploys complete roster"),S.DeployAll())) return false;
    S.GM->ForceStartBattle();
    FLKCombatSource Source;Source.Kind=ELKCombatSourceKind::Spell;Source.bRangedSource=true;Source.bHasTeam=true;Source.Team=ELKTeam::Player;
    TArray<ALKUnitBase*> InitialEnemies;
    for(TActorIterator<ALKUnitBase> It(S.World);It;++It)
    { if(It->GetTeam()==ELKTeam::Enemy && It->IsAlive() && !It->IsCamp()) InitialEnemies.Add(*It); }
    S.GM->BeginCombatBatch();
    for(auto* Unit:InitialEnemies) LKGameplay::ApplyDamage(Unit,Unit->GetMaxHealth()*10.f,nullptr,true,&Source);
    S.GM->EndCombatBatch();
    TestEqual(TEXT("All original enemies defeated"),S.GM->CountAliveUnits(ELKTeam::Enemy),0);
    TestEqual(TEXT("Death event prevents early victory during delay"),S.GM->GetPhase(),ELKGamePhase::Battle);
    S.GM->Tick(.5f);TestEqual(TEXT("Waiting half-second retains battle"),S.GM->CountAliveUnits(ELKTeam::Enemy),0);
    S.GM->Tick(.51f);
    TestEqual(TEXT("Death event spawns the authored hero reinforcement"),S.GM->CountAliveUnits(ELKTeam::Enemy),1);
    ALKUnitBase* Giant=nullptr;
    for(TActorIterator<ALKUnitBase> It(S.World);It;++It)
    { if(It->GetTeam()==ELKTeam::Enemy && It->IsAlive() && It->GetUnitId()=="Hero_SkeletonGiant") Giant=*It; }
    if(!TestNotNull(TEXT("Spawned reinforcement is skeleton giant"),Giant)) return false;
    TestEqual(TEXT("New hero holds battle open"),S.GM->GetPhase(),ELKGamePhase::Battle);
    LKGameplay::ApplyDamage(Giant,Giant->GetMaxHealth()*10.f,nullptr,true,&Source);
    S.GM->Tick(.01f);
    TestEqual(TEXT("Clear after final wave ends battle"),S.GM->GetPhase(),ELKGamePhase::Result);
    TestEqual(TEXT("Custom-map clear awards player victory"),S.GM->GetMatchStats().Winner,ELKTeam::Player);
    return true;
}
#endif
