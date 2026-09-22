#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/WorldSettings.h"
#include "PaperSprite.h"
#include "PaperSpriteComponent.h"
#include "Engine/Texture2D.h"
#include "../LKWorldArt.h"
#include "../ULKPresentationSubsystem.h"
#include "../ALKBattleGameMode.h"
#include "../ALKHeroCamp.h"
#include "../ALKUnitHero.h"
#include "../ALKPlayerController.h"
#include "../ULKGameData.h"
#include "../ULKRunSubsystem.h"
#include "../LKGameplayHelpers.h"

namespace { constexpr auto WorldArtFlags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKWorldArtCatalogueTest,"LittleKing.Presentation.WorldArtCatalogue",WorldArtFlags)
bool FLKWorldArtCatalogueTest::RunTest(const FString& Parameters)
{
    TSet<UTexture2D*> Grounds;
    for(int I=0;I<5;++I)
    {
        const FName Id(*FString::Printf(TEXT("World_Region%d"),I));
        TestEqual(TEXT("Region maps to exact image"),LKWorldArt::RegionIndex(Id),I);
        UTexture2D* Texture=LKWorldArt::GroundTexture(Id); Grounds.Add(Texture);
        TestNotNull(TEXT("Ground texture exists"),Texture);
        TestNotNull(TEXT("Ground sprite exists"),LKWorldArt::GroundSprite(Id));
    }
    TestEqual(TEXT("Five distinct surfaces"),Grounds.Num(),5);
    TestEqual(TEXT("Legacy has stable fallback"),LKWorldArt::RegionIndex("Legacy"),0);
    TSet<UPaperSprite*> Camps;
    for(FName Id:{FName("Hero_Knight"),FName("Hero_Mage"),FName("Hero_Ranger")})
    { UPaperSprite* Sprite=LKWorldArt::CampSprite(Id); Camps.Add(Sprite); TestNotNull(TEXT("Camp image"),Sprite); }
    TestEqual(TEXT("Three distinct camps"),Camps.Num(),3);
    for(auto Type:{ELKDungeonNodeType::Battle,ELKDungeonNodeType::Elite,ELKDungeonNodeType::Boss,ELKDungeonNodeType::Market,ELKDungeonNodeType::Rest})
    { TestNotNull(TEXT("Node icon"),LKWorldArt::NodeIcon(Type)); }
    TestNull(TEXT("Start keeps its text marker"),LKWorldArt::NodeIcon(ELKDungeonNodeType::Event));
    for (FName Id:{FName("FX_EmberBurst"),FName("FX_Leaf"),FName("FX_Dust")})
    { TestNotNull(TEXT("Painterly FX texture"),LKWorldArt::EffectTexture(Id)); }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKWorldArtQueueTest,"LittleKing.Presentation.CueBudgetAndThrottling",WorldArtFlags)
bool FLKWorldArtQueueTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Env;
    if(!Env.CreateTestWorld(EWorldType::Game)){return false;}
    UWorld* World=Env.GetTestWorld();
    ULKPresentationSubsystem* P=World->GetSubsystem<ULKPresentationSubsystem>();
    if(!TestNotNull(TEXT("World owns presentation"),P)){return false;}
    for(int I=0;I<300;++I){ULKPresentationSubsystem::Emit(World,ELKVisualCue::Heal,FVector(I*100,0,0));}
    TestEqual(TEXT("Crowded effects remain bounded"),P->GetEffects().Num(),ULKPresentationSubsystem::MaxEffects);
    ULKPresentationSubsystem::Emit(World,ELKVisualCue::Heal,FVector(29900,0,0));
    TestEqual(TEXT("Repeat coalesces"),P->GetEffects().Num(),ULKPresentationSubsystem::MaxEffects);
    P->Prune(World->GetTimeSeconds()+2);
    TestTrue(TEXT("Effects expire without new gameplay event"),P->GetEffects().IsEmpty());
    TestTrue(TEXT("First heal can play"),P->AdmitSound("Heal",10));
    TestFalse(TEXT("Heal swarm is throttled"),P->AdmitSound("Heal",10.1));
    TestTrue(TEXT("Different cue independent"),P->AdmitSound("Coin",10.1));
    TestTrue(TEXT("Heal resumes after gap"),P->AdmitSound("Heal",10.31));
    TestFalse(TEXT("Unknown IDs do not grow throttle map"),P->AdmitSound("Typo",20));
    for(FName Id:LKWorldArt::SoundIds()){TestTrue(TEXT("Every known cue has interval"),LKWorldArt::SoundInterval(Id)>0);}
    ULKPresentationSubsystem::Emit(World,ELKVisualCue::Fireball,FVector::ZeroVector,200);
    P->ClearEffects(); TestTrue(TEXT("Result reset clears transient effects"),P->GetEffects().IsEmpty());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKWorldArtIntegrationTest,"LittleKing.Presentation.CampsGroundAndCombatEvents",WorldArtFlags)
bool FLKWorldArtIntegrationTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Env;
    if(!Env.CreateTestWorld(EWorldType::Game)){return false;}
    UWorld* World=Env.GetTestWorld();
    World->GetWorldSettings()->DefaultGameMode=ALKBattleGameMode::StaticClass();
    World->GetGameInstance()->GetSubsystem<ULKRunSubsystem>()->SetAutoSaveEnabled(false);
    if(!Env.BeginPlayInTestWorld()){Env.ForwardErrorMessages(this);return false;}
    ALKBattleGameMode* GM=World->GetAuthGameMode<ALKBattleGameMode>();
    World->SpawnActor<ALKPlayerController>(); GM->Tick(0.f);
    ULKPresentationSubsystem* P=World->GetSubsystem<ULKPresentationSubsystem>();
    UPaperSpriteComponent* Ground=P->GetGround();
    if(!TestNotNull(TEXT("Battle has surface"),Ground)){return false;}
    TestEqual(TEXT("Ground cannot block placement/navigation"),Ground->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
    TestFalse(TEXT("Ground owner is visible (GameMode would hide it)"),Ground->GetOwner()->IsHidden());
    const auto Bounds=Ground->CalcBounds(Ground->GetComponentTransform());
    TestTrue(TEXT("Ground covers rule-defined field"),FMath::IsNearlyEqual(Bounds.BoxExtent.X,double(GM->GetGameData()->FieldHalfWidth),1.)&&FMath::IsNearlyEqual(Bounds.BoxExtent.Y,double(GM->GetGameData()->FieldHalfHeight),1.));
    for(int I=0;I<GM->AvailableHeroes.Num();++I)
    { TestEqual(TEXT("Hero with illustrated camp can deploy"),GM->DeployHero(ELKTeam::Player,GM->AvailableHeroes[I],FVector((I-1)*700,-1300,0)),ELKPlayResult::Success); }
    GM->ForceStartBattle(); TestEqual(TEXT("All camps preserve battle start"),GM->GetPhase(),ELKGamePhase::Battle);
    ALKUnitHero* Hero=Cast<ALKUnitHero>(GM->SpawnUnitForTeam("Hero_Mage",ELKTeam::Player,FVector(800,-700,0)));
    if(!TestNotNull(TEXT("Additional event target"),Hero)){return false;}
    ALKHeroCamp* Camp=World->SpawnActor<ALKHeroCamp>(); Camp->InitializeCamp(Hero,GM->GetGameData());
    TestFalse(TEXT("Camp never targetable"),Camp->IsTargetable());
    TestEqual(TEXT("Camp retains physical radius"),Camp->GetBodyRadius(),GM->GetGameData()->HeroCampBodyRadius);
    TestNotNull(TEXT("Camp real sprite assigned"),Camp->GetSpriteComponent()->GetSprite());
    const float Health=Hero->GetHealth();
    LKGameplay::ApplyDamage(Hero,10.f,nullptr,true);
    const float BeforeHeal=Hero->GetHealth(); P->ClearEffects();
    LKGameplay::ApplyHeal(Hero,5.f,Hero);
    TestEqual(TEXT("Art leaves healing amount unchanged"),Hero->GetHealth(),BeforeHeal+5.f);
    TestTrue(TEXT("Real healing emits leaves"),P->GetEffects().ContainsByPredicate([](const FLKVisualCue& C){return C.Type==ELKVisualCue::Heal;}));
    GM->NotifyFireballCast(FVector(100,300,0),190.f);
    TestTrue(TEXT("Fireball uses actual area radius"),P->GetEffects().ContainsByPredicate([](const FLKVisualCue& C){return C.Type==ELKVisualCue::Fireball&&C.Radius==190.f;}));
    P->SetRegion("World_Region4",ELKDungeonNodeType::Boss);
    TestEqual(TEXT("Boss changes only surface"),Ground->GetSprite(),LKWorldArt::GroundSprite("World_Region4"));
    TestEqual(TEXT("Surface selection cannot change unit health"),Hero->GetHealth(),Health-5.f);
    GM->ForceEndMatch(ELKTeam::Player);
    TestTrue(TEXT("Result clears effects even without HUD draw"),P->GetEffects().IsEmpty());
    return true;
}
#endif
