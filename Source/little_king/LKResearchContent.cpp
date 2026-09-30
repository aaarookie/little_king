#include "LKResearchContent.h"
#include "LKExpeditionMercenaryContent.h"
#include "LKCardRules.h"
#include "LKBattleArt.h"
#include "ULKGameData.h"
#include "ULKCardDefinition.h"
#include "Misc/Crc.h"

namespace LKResearchContent
{
const TArray<FLKResearchDefinition>& All()
{
    static const TArray<FLKResearchDefinition> Rows={
        {"Spell_ResearchFireball_N2",FText::FromString(TEXT("强化火球术")),ELKMarketOfferKind::SpellBook,ELKSpellGrade::Novice2,0,120,100.f,2,ELKSpellEffect::Damage,110,250,0},
        {"Spell_ResearchFireball_M1",FText::FromString(TEXT("烈焰火球")),ELKMarketOfferKind::SpellBook,ELKSpellGrade::Intermediate1,1,260,30.f,3,ELKSpellEffect::Damage,210,280,0},
        {"Spell_ResearchFireball_A1",FText::FromString(TEXT("灼日火球")),ELKMarketOfferKind::SpellBook,ELKSpellGrade::Advanced1,2,480,6.f,4,ELKSpellEffect::Damage,380,320,0},
        {"Spell_ResearchFireball_D1",FText::FromString(TEXT("天火")),ELKMarketOfferKind::SpellBook,ELKSpellGrade::Divine1,3,800,1.f,5,ELKSpellEffect::Damage,650,360,0},
        {"Spell_ResearchHeal_M1",FText::FromString(TEXT("生命泉涌")),ELKMarketOfferKind::SpellBook,ELKSpellGrade::Intermediate1,1,260,30.f,3,ELKSpellEffect::Heal,160,320,.09f},
        {"Spell_ResearchHeal_A1",FText::FromString(TEXT("圣光潮汐")),ELKMarketOfferKind::SpellBook,ELKSpellGrade::Advanced1,2,480,6.f,4,ELKSpellEffect::Heal,220,340,.12f},
        {"Spell_ResearchHeal_D1",FText::FromString(TEXT("生命礼赞")),ELKMarketOfferKind::SpellBook,ELKSpellGrade::Divine1,3,800,1.f,5,ELKSpellEffect::Heal,300,380,.16f},
        {"Building_SiegeCatapult",FText::FromString(TEXT("攻城投石炮")),ELKMarketOfferKind::BuildingBlueprint,ELKSpellGrade::Novice1,1,350,1.f,4,ELKSpellEffect::None,0,0,0},
    };
    return Rows;
}
const FLKResearchDefinition* Find(FName Id) { return All().FindByPredicate([Id](const auto& R){return R.CardId==Id;}); }
bool ValidateMaterials(const TMap<FName,int32>& Materials)
{ for (const auto& P:Materials) { if (!Find(P.Key) || P.Value<1 || P.Value>10000) { return false; } } return true; }
void EnsureCards(ULKGameData& Data)
{
    for (const auto& R:All())
    {
        const int32 I=Data.CardLibrary.IndexOfByPredicate([&R](const auto& C){return C && C->CardId==R.CardId;});
        if (R.Kind==ELKMarketOfferKind::BuildingBlueprint)
        { if (I!=INDEX_NONE) { Data.CardLibrary[I]=DuplicateObject<ULKCardDefinition>(Data.CardLibrary[I].Get(),&Data); Data.CardLibrary[I]->bExpeditionOnly=false; } continue; }
        ULKCardDefinition* C=I==INDEX_NONE?NewObject<ULKCardDefinition>(&Data):DuplicateObject<ULKCardDefinition>(Data.CardLibrary[I].Get(),&Data);
        C->CardId=R.CardId; C->CardName=R.Name; C->CardType=ELKCardType::Spell; C->bExpeditionOnly=false;
        C->SpellGrade=R.Grade;
        if (I==INDEX_NONE) { C->Cost=R.SilverCost; C->SpellEffect=R.Effect; C->SpellValue=R.Value; C->SpellRadius=R.Radius; C->HeroHealPercent=R.HeroHealPercent; }
        if (C->Icon.IsNull())
        { const TCHAR* Name=R.Effect==ELKSpellEffect::Heal?TEXT("Spell_HealWave"):TEXT("Spell_Fireball"); C->Icon=TSoftObjectPtr<UTexture2D>(FSoftObjectPath(FString::Printf(TEXT("/Game/Icons/%s.%s"),Name,Name))); }
        C->Description=FText::FromString(TEXT("图书馆研究永久解锁；远征中的升级只增加本轮数值。"));
        if (I==INDEX_NONE) { Data.CardLibrary.Add(C); } else { Data.CardLibrary[I]=C; }
    }
}
void GenerateMarket(FLKDungeonNode& Node,int32 RunSeed,int32 Depth)
{
    if (Node.bMarketGenerated || Node.Type!=ELKDungeonNodeType::Market) { return; }
    // CRC is stable across platforms; never use FName comparison-index hashing for saved random content.
    FRandomStream Stream(int32(uint32(RunSeed)^FCrc::StrCrc32(*Node.NodeId.ToString())^0x08251u));
    auto Add=[&](ELKMarketOfferKind Kind,FName Id,int32 Price)
    { FLKMarketOffer O; O.OfferId=FGuid(Stream.GetUnsignedInt(),Stream.GetUnsignedInt(),Stream.GetUnsignedInt(),Stream.GetUnsignedInt()); O.Kind=Kind; O.CardId=Id; O.Price=Price; Node.MarketOffers.Add(O); };
    TArray<FName> Pool;
    for (const auto& D:LKExpeditionMercenaryContent::All()) { if (LKCardRules::IsTemporaryMercenary(D.Unit.UnitId)) { Pool.Add(D.Unit.UnitId); } }
    for (int32 I=Pool.Num()-1;I>0;--I) { Pool.Swap(I,Stream.RandRange(0,I)); }
    for (int32 I=0;I<FMath::Min(3,Pool.Num());++I)
    { const auto* D=LKExpeditionMercenaryContent::All().FindByPredicate([&](const auto& E){return E.Unit.UnitId==Pool[I];}); Add(ELKMarketOfferKind::Mercenary,Pool[I],20+D->Cost*15+int32(D->Unit.Quality)*10); }
    // One percent per market for a book; tier weight and geography jointly control rarity.
    if (Stream.FRand()<.01f)
    {
        float Total=0; for (const auto& R:All()) { if (R.Kind==ELKMarketOfferKind::SpellBook && Depth>=R.MinimumRegionDepth) { Total+=R.Weight; } }
        float Pick=Stream.FRand()*Total;
        for (const auto& R:All())
        { if (R.Kind!=ELKMarketOfferKind::SpellBook || Depth<R.MinimumRegionDepth) { continue; } Pick-=R.Weight; if (Pick<=0) { Add(R.Kind,R.CardId,R.Price); break; } }
    }
    if (Depth>=1 && Stream.FRand()<.005f) { const auto& R=All().Last(); Add(R.Kind,R.CardId,R.Price); }
    Node.bMarketGenerated=true;
}
}
