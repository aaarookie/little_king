#include "LKPolishArt.h"
#include "LKBattleArt.h"
#include "PaperFlipbook.h"
#include "PaperSprite.h"
#include "Engine/Font.h"

namespace LKPolishArt
{
const TArray<FName>& UnitIds()
{
    static const TArray<FName> Ids=[]
    {
        TArray<FName> Result={"Hero_Knight","Hero_Mage","Hero_Ranger","Unit_Swordsman","Unit_Archer","Unit_Shieldbearer","Building_ArrowTower","Building_Barracks"};
        Result.Append(LKBattleArt::UnitIds()); return Result;
    }();
    return Ids;
}
bool IsDefaultSprite(FName Id,const UPaperSprite* Sprite)
{
    if (!Sprite || !UnitIds().Contains(Id)) { return false; }
    const FString Path=Sprite->GetPathName();
    const FString Legacy=Id=="Hero_Ranger"?TEXT("/Game/Sprites/Hero_Ranger_s.Hero_Ranger_s")
        :TEXT("/Game/Sprites/")+Id.ToString()+TEXT("_sprite.")+Id.ToString()+TEXT("_sprite");
    return Path==Legacy || Path==LKBattleArt::Sprite(Id).ToString();
}
UPaperFlipbook* Animation(FName Id,FName State)
{
    if(!UnitIds().Contains(Id)){return nullptr;}
    const FString Name=TEXT("FB_")+Id.ToString()+TEXT("_")+State.ToString();
    return LoadObject<UPaperFlipbook>(nullptr,*(TEXT("/Game/Art/StorybookV1/Polish/Animations/")+Name+TEXT(".")+Name),nullptr,LOAD_NoWarn);
}
UPaperSprite* HomeLevel(FName Id,int32 Level)
{
    const FString Kind=Id=="Home_StatueSaintMaria"?TEXT("Statue"):Id=="Home_Treasury"?TEXT("Treasury"):TEXT("");
    if(Kind.IsEmpty()||Level<2){return nullptr;}
    const int32 Capped=FMath::Clamp(Level,2,Id=="Home_Treasury"?5:4);
    const FString Name=FString::Printf(TEXT("SP_Home_%s_Lv%d"),*Kind,Capped);
    return LoadObject<UPaperSprite>(nullptr,*(TEXT("/Game/Art/StorybookV1/Polish/Buildings/")+Name+TEXT(".")+Name),nullptr,LOAD_NoWarn);
}
UFont* Font(bool bTitle)
{
    const TCHAR* Name=bTitle?TEXT("F_Title"):TEXT("F_Body");
    return LoadObject<UFont>(nullptr,*FString::Printf(TEXT("/Game/Art/StorybookV1/Polish/Fonts/%s.%s"),Name,Name),nullptr,LOAD_NoWarn);
}
}
