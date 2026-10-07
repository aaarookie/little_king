#include "LKV084Preview.h"
#if WITH_EDITOR
#include "ALKBattleGameMode.h"
#include "ALKUnitBuilding.h"
#include "ALKPlayerController.h"
#include "ULKGameData.h"
#include "ULKCardDefinition.h"
#include "ULKBattleHUDWidget.h"
#include "ULKDeckState.h"
#include "ULKSilverComponent.h"
#include "ULKUnitMovementComponent.h"
#include "ULKUnitAnimationComponent.h"
#include "LKV084Art.h"
#include "LKPolishArt.h"
#include "LKCardPresentation.h"
#include "LKBuildingGeometry.h"
#include "LKGameplayHelpers.h"
#include "PaperSpriteComponent.h"
#include "PaperFlipbook.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "CanvasItem.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Debug/DebugDrawService.h"
#include "GameFramework/HUD.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "ShaderCompiler.h"
#include "UnrealClient.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
int32 Frame=0;
FString Caption;
TWeakObjectPtr<ALKBattleGameMode> PreviewMode;
TArray<TWeakObjectPtr<ALKUnitBuilding>> Buildings;
TArray<TStrongObjectPtr<UTexture2D>> Icons;
bool bCards=false;
void Text(UCanvas* Canvas,const FString& Value,float X,float Y,float H,float Width,FLinearColor Color)
{
    UFont* Font=LKPolishArt::Font(false); if(!Font) { return; }
    int32 W=0,Height=0; UCanvas::ClippedStrLen(Font,1.f,1.f,W,Height,Value);
    const float Scale=FMath::Min(H/FMath::Max(1,Height),Width/FMath::Max(1,W));
    FCanvasTextItem Item(FVector2D(X,Y),FText::FromString(Value),Font,Color); Item.Scale=FVector2D(Scale); Item.EnableShadow(FLinearColor::Black); Canvas->DrawItem(Item);
}
void Rect(UCanvas* Canvas,float X,float Y,float W,float H,FLinearColor Color)
{ FCanvasTileItem Item(FVector2D(X,Y),FVector2D(W,H),Color); Item.BlendMode=SE_BLEND_Translucent; Canvas->DrawItem(Item); }
void Draw(UCanvas* Canvas,APlayerController* PC)
{
    auto* Mode=PreviewMode.Get(); if(!Canvas||!Mode) { return; }
    if(!PC) { PC=Mode->GetWorld()->GetFirstPlayerController(); }
    const float S=Canvas->ClipY/1080.f;
    const FLinearColor Pine(.10f,.23f,.20f),Paper(.90f,.84f,.68f),Gold(.74f,.59f,.31f);
    if(bCards) { Rect(Canvas,0,0,Canvas->ClipX,Canvas->ClipY,Pine); }
    Rect(Canvas,0,0,Canvas->ClipX,92*S,Pine);
    Text(Canvas,TEXT("Little King · v0.8.4 · 建筑与驻守"),28*S,14*S,28*S,Canvas->ClipX-56*S,Gold);
    Text(Canvas,Caption,28*S,51*S,23*S,Canvas->ClipX-56*S,Paper);
    if(bCards)
    {
        const float Gap=20*S,W=(Canvas->ClipX-5*Gap)/4,H=(Canvas->ClipY-170*S)/2;
        for(int32 I=0;I<LKV084Art::CardIds().Num();++I)
        {
            const auto* Card=Mode->FindCard(LKV084Art::CardIds()[I]); if(!Card) { continue; }
            const auto* Row=Mode->GetUnitRow(Card->BuildingUnitId);
            const float X=Gap+(I%4)*(W+Gap),Y=110*S+(I/4)*(H+Gap);
            Rect(Canvas,X,Y,W,H,FLinearColor(.15f,.31f,.26f));
            const float IW=FMath::Min(W-30*S,H-132*S);
            if(Icons.IsValidIndex(I)&&Icons[I].IsValid()&&Icons[I]->GetResource())
            {
                const float Scale=IW/FMath::Max(Icons[I]->GetSizeX(),Icons[I]->GetSizeY());
                const FVector2D Size(Icons[I]->GetSizeX()*Scale,Icons[I]->GetSizeY()*Scale);
                FCanvasTileItem Item(FVector2D(X+(W-Size.X)/2,Y+10*S+(IW-Size.Y)/2),Icons[I]->GetResource(),Size,FLinearColor::White);
                Item.BlendMode=SE_BLEND_Translucent; Canvas->DrawItem(Item);
            }
            Text(Canvas,Card->CardName.ToString(),X+16*S,Y+H-112*S,26*S,W-32*S,Paper);
            Text(Canvas,FString::Printf(TEXT("%d 银币 · %s"),Card->Cost,*LKCardPresentation::QualityName(Row->Quality).ToString()),X+16*S,Y+H-75*S,21*S,W-32*S,Gold);
            Text(Canvas,FString::Printf(TEXT("生命 %.0f · 施工 %.0f 秒"),Row->BaseHealth,Row->ConstructionSeconds),X+16*S,Y+H-40*S,19*S,W-32*S,Paper);
        }
    }
    else if(PC)
    {
        for(auto Weak:Buildings) { if(auto* B=Weak.Get())
        {
            FVector2D P;
            if(PC->ProjectWorldLocationToScreen(B->GetActorLocation()+FVector(-170,0,0),P,true))
            { Text(Canvas,B->GetDisplayName().ToString(),P.X-125*S,P.Y,24*S,250*S,Paper); }
        } }
    }
}
void Shot(UWorld* World,const FString& Name)
{
    int32 W=0,H=0;World->GetFirstPlayerController()->GetViewportSize(W,H);
    const FString Dir=FPaths::ProjectSavedDir()/TEXT("Screenshots/V084");IFileManager::Get().MakeDirectory(*Dir,true);
    FScreenshotRequest::RequestScreenshot(Dir/FString::Printf(TEXT("%s_%d.png"),*Name,H),true,false);
}
}
void LKV084Preview::Tick(ALKBattleGameMode* Mode)
{
    if(!Mode||!Mode->GetBattleHUDWidget()||(GShaderCompilingManager&&GShaderCompilingManager->IsCompiling())) { return; }
    ++Frame;UWorld* World=Mode->GetWorld();
    if(Frame==1)
    {
        PreviewMode=Mode;
        for(int32 I=0;I<Mode->AvailableHeroes.Num();++I) { Mode->DeployHero(ELKTeam::Player,Mode->AvailableHeroes[I],FVector((I-1)*700.f,-1300,0)); }
        Mode->ForceStartBattle();
        for(TActorIterator<ALKUnitBase> It(World);It;++It) { It->Destroy(); }
        auto* Camera=World->SpawnActor<ACameraActor>();Camera->SetActorLocationAndRotation(FVector(0,0,5000),FRotator(-90,0,0));
        Camera->GetCameraComponent()->ProjectionMode=ECameraProjectionMode::Orthographic;Camera->GetCameraComponent()->OrthoWidth=2600;
        Camera->GetCameraComponent()->bConstrainAspectRatio=false;World->GetFirstPlayerController()->SetViewTarget(Camera);
        Mode->GetBattleHUDWidget()->SetVisibility(ESlateVisibility::Hidden);
        UDebugDrawService::Register(TEXT("Game"),FDebugDrawDelegate::CreateStatic(&Draw));
        for(int32 I=0;I<LKV084Art::UnitIds().Num();++I)
        {
            auto* B=Cast<ALKUnitBuilding>(Mode->SpawnUnitForTeam(LKV084Art::UnitIds()[I],ELKTeam::Player,FVector(900,-1600,0)));
            if(!B) { UE_LOG(LogTemp,Error,TEXT("V084_PREVIEW_FAILED building"));FPlatformMisc::RequestExit(false);return; }
            B->SetActorLocation(I<4?FVector(270,(I-1.5f)*580.f,0):FVector(-290,(I-5)*700.f,0));
            if(B->IsWall()) { B->SetWallYaw(I*30.f); }
            B->SetCombatEnabled(true); B->SetActorTickEnabled(false);B->GetMovementComponent()->SetComponentTickEnabled(false);
            B->Tick(0.f);
            B->GetAnimationComponent()->TickComponent(0,LEVELTICK_All,nullptr);B->GetAnimationComponent()->SetComponentTickEnabled(false);
            Buildings.Add(B);
        }
        for(FName Id:LKV084Art::CardIds()) { Icons.Emplace(LKV084Art::CardIcon(Id).LoadSynchronous()); }
        Caption=TEXT("施工进度 · 四种可旋转墙体 · 三类功能建筑");
    }
    if(Frame==45) { Shot(World,TEXT("Construction")); }
    if(Frame==60)
    {
        for(auto Weak:Buildings) { if(auto* B=Weak.Get()) { B->Tick(21.f);B->GetAnimationComponent()->TickComponent(0,LEVELTICK_All,nullptr); } }
        Caption=TEXT("建造完成 · 墙体与方向 · 建筑生命条");
    }
    if(Frame==90) { Shot(World,TEXT("Completed")); }
    if(Frame==110) { bCards=true;Caption=TEXT("七张研究建筑 · 品质、费用和生命数值"); }
    if(Frame==140) { Shot(World,TEXT("BuildingCards")); }
    if(Frame==160) { bCards=false;Caption=TEXT("攻城车 · 真实图集姿态"); }
    if(Frame>=170 && Frame<490)
    {
        const int32 I=(Frame-170)/20;
        const FName State=I<4?FName("Idle"):I<8?FName("Move"):I<12?FName("Attack"):I<14?FName("Hit"):FName("Death");
        if((Frame-170)%20==0)
        {
            Caption=FString::Printf(TEXT("攻城车 · %s · 姿态 %d"),*State.ToString(),I+1);
            if(auto* Clip=LKV084Art::Animation("Unit_SiegeCart",State)) { Buildings[4]->GetSpriteComponent()->SetSprite(Clip->GetSpriteAtFrame(I<12?I%4:I%2)); }
        }
        if((Frame-170)%20==10) { Shot(World,FString::Printf(TEXT("CartPose_%02d"),I)); }
    }
    if(Frame==510)
    {
        if(auto* Clip=LKV084Art::Animation("Unit_SiegeCart","Idle")) { Buildings[4]->GetSpriteComponent()->SetSprite(Clip->GetSpriteAtFrame(0)); }
        Caption=TEXT("原生战斗手牌 · 七建筑轮换 · Q/E 墙体预览");
        Mode->GetBattleHUDWidget()->SetVisibility(ESlateVisibility::Visible);
        Mode->GetTeamSilver(ELKTeam::Player)->AddSilver(50.f);
        auto* PC=Cast<ALKPlayerController>(World->GetFirstPlayerController());
        auto* Deck=Mode->GetTeamDeck(ELKTeam::Player);
        for(int32 Attempt=0;Attempt<9 && !Deck->GetHand().Contains("Card_WallImperial");++Attempt) { Deck->PlayCard(0); }
        const int32 Index=Deck->GetHand().IndexOfByKey("Card_WallImperial");
        if(PC && Index!=INDEX_NONE) { PC->BeginCardPlacement(Index);PC->SetMouseLocation(420,390); }
    }
    if(Frame==550) { Shot(World,TEXT("NativeHUD")); }
    if(Frame==580)
    {
        if(auto* PC=Cast<ALKPlayerController>(World->GetFirstPlayerController())) { PC->CancelPlacement(); }
        Mode->GetBattleHUDWidget()->SetVisibility(ESlateVisibility::Hidden);
        Caption=TEXT("驻守与接诊 · 法师受庇护 · 牧师与受伤患者");
        for(const auto& Entry:{TPair<FName,int32>("Hero_Mage",5),TPair<FName,int32>("Unit_ElfPriest",6)})
        {
            auto* B=Buildings[Entry.Value].Get();
            auto* Unit=Mode->SpawnUnitForTeam(Entry.Key,ELKTeam::Player,B->GetActorLocation()+FVector(-300,0,0));
            if(!Unit) { UE_LOG(LogTemp,Error,TEXT("V084_PREVIEW_FAILED resident spawn"));continue; }
            Unit->SetActorTickEnabled(false);Unit->GetMovementComponent()->SetComponentTickEnabled(false);
            B->Tick(.6f);
            FVector Approach;
            if(B->GetOccupant()==Unit && B->FindServiceApproach(Unit,Approach)) { Unit->SetActorLocation(Approach);Unit->Tick(.01f); }
            if(!Unit->IsSheltered()) { UE_LOG(LogTemp,Error,TEXT("V084_PREVIEW_FAILED resident entry")); }
        }
        auto* Station=Buildings[6].Get();
        auto* Patient=Mode->SpawnUnitForTeam("Unit_Swordsman",ELKTeam::Player,Station->GetActorLocation()+FVector(-300,100,0));
        if(Patient)
        {
            Patient->SetActorTickEnabled(false);Patient->GetMovementComponent()->SetComponentTickEnabled(false);
            LKGameplay::ApplyDamage(Patient,Patient->GetMaxHealth()*.8f,nullptr);Station->Tick(.6f);
            FVector Approach;
            if(Station->GetPatient()==Patient && Station->FindServiceApproach(Patient,Approach)) { Patient->SetActorLocation(Approach);Patient->Tick(.01f); }
            if(!Station->IsPatientReady()) { UE_LOG(LogTemp,Error,TEXT("V084_PREVIEW_FAILED patient arrival")); }
        }
        else { UE_LOG(LogTemp,Error,TEXT("V084_PREVIEW_FAILED patient spawn")); }
    }
    if(Frame==620) { Shot(World,TEXT("StaffedBuildings")); }
    if(Frame==650) { UE_LOG(LogTemp,Display,TEXT("V084_PREVIEW_OK screenshots=21"));FPlatformMisc::RequestExit(false); }
}
#endif
