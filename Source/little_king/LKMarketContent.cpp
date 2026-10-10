#include "LKMarketContent.h"
#include "LKCardRules.h"
#include "LKExpeditionMercenaryContent.h"
#include "LKItemContent.h"
#include "LKResearchContent.h"
#include "LKUnitContent.h"
#include "LKLog.h"
#include "Misc/Crc.h"
#include "Misc/SecureHash.h"

namespace
{
struct FCandidate
{
    FName Id;
    ELKMarketOfferKind Kind = ELKMarketOfferKind::Mercenary;
    ELKQuality Quality = ELKQuality::Uncommon;
    int32 SilverCost = 1;
    int32 MinimumDepth = 0;
};

FRandomStream PurposeStream(const FLKDungeonNode& Node, int32 Seed, uint32 Purpose)
{
    // Separate streams and stable CRC prevent unrelated offer changes from rerolling research/items.
    return FRandomStream(int32(uint32(Seed)^FCrc::StrCrc32(*Node.NodeId.ToString())^Purpose^0x0B020000u));
}
FGuid MarketIdentity(const FLKDungeonNode& Node,int32 RunSeed,FGuid RunId,uint32 Purpose)
{
    // Hash full identities; a 32-bit random-stream state is insufficient for permanent instance IDs.
    // Length-prefix the node and use a canonical UTF-8 byte sequence and byte order across platforms.
    const FString NodeId=Node.NodeId.ToString();
    const FString Input=FString::Printf(TEXT("LittleKing.MarketIdentity.2|%s|%d|%d:%s|%08x"),
        *RunId.ToString(EGuidFormats::Digits),RunSeed,NodeId.Len(),*NodeId,Purpose);
    const FTCHARToUTF8 Utf8(*Input); uint8 Digest[20]; FSHA1::HashBuffer(Utf8.Get(),Utf8.Length(),Digest);
    auto Word=[&](int32 I){return (uint32(Digest[I])<<24)|(uint32(Digest[I+1])<<16)|(uint32(Digest[I+2])<<8)|uint32(Digest[I+3]);};
    const FGuid Result(Word(0),Word(4),Word(8),Word(12));
    return Result.IsValid()?Result:FGuid(0,0,0,1);
}
ELKQuality ResearchQuality(const FLKResearchDefinition& Definition)
{
    if (const FLKUnitRow* Unit=LKUnitContent::Find(Definition.CardId)) { return Unit->Quality; }
    return ELKQuality(FMath::Clamp(Definition.MinimumRegionDepth+1,1,4));
}
int32 PickWeighted(FRandomStream& Stream, const TArray<float>& Weights)
{
    float Total=0.f; for (float Weight:Weights) { Total+=FMath::Max(0.f,Weight); }
    if (Total<=0.f) { return INDEX_NONE; }
    float Pick=Stream.FRand()*Total;
    for (int32 I=0; I<Weights.Num(); ++I)
    { const float Weight=FMath::Max(0.f,Weights[I]); if (Weight>0.f && Pick<Weight) { return I; } Pick-=Weight; }
    for (int32 I=Weights.Num()-1; I>=0; --I) { if (Weights[I]>0.f) { return I; } }
    return INDEX_NONE;
}
}

namespace LKMarketContent
{
ELKMarketCategory CategoryOf(ELKMarketOfferKind Kind)
{
    if (Kind==ELKMarketOfferKind::Item) { return ELKMarketCategory::Item; }
    return IsResearch(Kind)?ELKMarketCategory::Research:ELKMarketCategory::TemporaryCard;
}
bool IsTemporaryCard(ELKMarketOfferKind Kind)
{ return Kind==ELKMarketOfferKind::Mercenary || Kind==ELKMarketOfferKind::TemporarySpell || Kind==ELKMarketOfferKind::TemporaryBuilding; }
bool IsResearch(ELKMarketOfferKind Kind)
{ return Kind==ELKMarketOfferKind::SpellBook || Kind==ELKMarketOfferKind::BuildingBlueprint; }
FText KindName(ELKMarketOfferKind Kind)
{
    switch (Kind)
    {
    case ELKMarketOfferKind::Mercenary: return FText::FromString(TEXT("临时佣兵"));
    case ELKMarketOfferKind::TemporarySpell: return FText::FromString(TEXT("临时法术"));
    case ELKMarketOfferKind::TemporaryBuilding: return FText::FromString(TEXT("临时建筑"));
    case ELKMarketOfferKind::SpellBook: return FText::FromString(TEXT("法术书"));
    case ELKMarketOfferKind::BuildingBlueprint: return FText::FromString(TEXT("建筑图纸"));
    case ELKMarketOfferKind::Item: return FText::FromString(TEXT("道具"));
    default: return FText::FromString(TEXT("无效商品"));
    }
}
int32 CardSilverCost(FName Id, const TMap<FName,int32>& SilverCosts)
{
    if (const int32* Cost=SilverCosts.Find(Id)) { return *Cost; }
    if (const FLKResearchDefinition* Research=LKResearchContent::Find(Id)) { return Research->SilverCost; }
    if (const auto* Mercenary=LKExpeditionMercenaryContent::All().FindByPredicate([Id](const auto& D){ return D.Unit.UnitId==Id; })) { return Mercenary->Cost; }
    return LKCardRules::IsRunMarketCard(Id)?2:0;
}
bool ValidateOffer(const FLKMarketOffer& Offer, int32 GenerationVersion, FString& Error)
{
    Error.Reset();
    if (!Offer.OfferId.IsValid() || Offer.Price<1 || Offer.Price>1000000
        || GenerationVersion<0 || GenerationVersion>CurrentGenerationVersion
        || uint8(Offer.Kind)>uint8(ELKMarketOfferKind::Item))
    { Error=TEXT("市场商品事务身份、价格或版本无效"); return false; }
    if (Offer.Kind==ELKMarketOfferKind::Item)
    {
        if (GenerationVersion<CurrentGenerationVersion || !Offer.CardId.IsNone() || Offer.ItemInstanceId==Offer.OfferId) { Error=TEXT("道具不能复用 CardId、交易身份或旧商品格式"); return false; }
        FLKItemInstance Item; Item.InstanceId=Offer.ItemInstanceId; Item.ItemId=Offer.ItemId; Item.Quality=Offer.Quality;
        return LKItemContent::ValidateInstances({Item},Error);
    }
    if (Offer.CardId.IsNone() || !Offer.ItemId.IsNone() || Offer.ItemInstanceId.IsValid())
    { Error=TEXT("卡牌与道具身份不可混用"); return false; }
    if (GenerationVersion<CurrentGenerationVersion && uint8(Offer.Kind)>uint8(ELKMarketOfferKind::BuildingBlueprint))
    { Error=TEXT("旧商品快照不支持新类别"); return false; }
    if (GenerationVersion>=CurrentGenerationVersion && uint8(Offer.Quality)>uint8(ELKQuality::Legendary))
    { Error=TEXT("商品品质无效"); return false; }
    if (GenerationVersion>=CurrentGenerationVersion)
    {
        ELKQuality Expected=ELKQuality::Common;
        if (const auto* Research=LKResearchContent::Find(Offer.CardId)) { Expected=ResearchQuality(*Research); }
        else if (const auto* Unit=LKUnitContent::Find(Offer.CardId)) { Expected=Unit->Quality; }
        if (Offer.Quality!=Expected) { Error=TEXT("商品品质与登记内容不一致"); return false; }
    }
    if (Offer.Kind==ELKMarketOfferKind::Mercenary && LKCardRules::IsTemporaryMercenary(Offer.CardId)) { return true; }
    if (Offer.Kind==ELKMarketOfferKind::TemporarySpell && LKCardRules::IsRunMarketCard(Offer.CardId) && LKCardRules::IsSpell(Offer.CardId)) { return true; }
    if (Offer.Kind==ELKMarketOfferKind::TemporaryBuilding && LKCardRules::IsRunMarketCard(Offer.CardId) && LKCardRules::IsBuilding(Offer.CardId)) { return true; }
    const FLKResearchDefinition* Research=LKResearchContent::Find(Offer.CardId);
    if (IsResearch(Offer.Kind) && Research && Research->Kind==Offer.Kind) { return true; }
    Error=TEXT("市场商品未登记或类别与内容不匹配"); return false;
}

void Generate(FLKDungeonNode& Node, int32 RunSeed, int32 RegionDepth, float SilverCap,
    const TArray<FName>& ResearchExclusions, const TMap<FName,int32>& CarriedResearchMaterials,
    const TArray<FLKRunCardState>& Cards, const TMap<FName,int32>& SilverCosts, FGuid RunId)
{
    if (Node.bMarketGenerated || Node.Type!=ELKDungeonNodeType::Market) { return; }
    if (!FMath::IsFinite(SilverCap) || SilverCap<=0.f) { return; }
    const int32 Depth=FMath::Clamp(RegionDepth,0,3);
    static const float MercenaryQualityWeights[4][3]={{80,20,0},{60,35,5},{40,45,15},{25,50,25}};
    static const float ItemQualityWeights[4][5]={{80,20,0,0,0},{55,35,10,0,0},{25,40,30,5,0},{10,25,40,20,5}};
    TArray<FCandidate> Candidates;
    for (const auto& Definition:LKExpeditionMercenaryContent::All())
    {
        if (!LKCardRules::IsTemporaryMercenary(Definition.Unit.UnitId)) { continue; }
        FCandidate C; C.Id=Definition.Unit.UnitId; C.Quality=Definition.Unit.Quality;
        C.SilverCost=CardSilverCost(C.Id,SilverCosts); Candidates.Add(C);
    }
    for (const auto& Definition:LKResearchContent::All())
    {
        FCandidate C; C.Id=Definition.CardId; C.Kind=Definition.Kind==ELKMarketOfferKind::SpellBook?ELKMarketOfferKind::TemporarySpell:ELKMarketOfferKind::TemporaryBuilding;
        C.MinimumDepth=Definition.MinimumRegionDepth; C.Quality=ResearchQuality(Definition);
        C.SilverCost=CardSilverCost(C.Id,SilverCosts); Candidates.Add(C);
    }
    for (FName Id:{FName("Spell_Fireball"),FName("Spell_HealWave"),FName("Building_ArrowTower"),FName("Building_Barracks")})
    {
        FCandidate C; C.Id=Id; C.Kind=LKCardRules::IsSpell(Id)?ELKMarketOfferKind::TemporarySpell:ELKMarketOfferKind::TemporaryBuilding;
        C.Quality=ELKQuality::Common;
        if (const auto* Unit=LKUnitContent::Find(Id)) { C.Quality=Unit->Quality; }
        C.SilverCost=CardSilverCost(Id,SilverCosts); Candidates.Add(C);
    }
    TSet<FName> Used;
    for (const auto& Card:Cards) { Used.Add(Card.CardId); }
    Node.MarketOffers.Reset();
    // Each card slot has a separate stream; empty-category fallback does not consume item/research RNG.
    for (int32 Slot=0; Slot<3; ++Slot)
    {
        const ELKMarketOfferKind Wanted=Slot==0?ELKMarketOfferKind::Mercenary:(Slot==1?ELKMarketOfferKind::TemporarySpell:ELKMarketOfferKind::TemporaryBuilding);
        TArray<int32> Eligible;
        auto Collect=[&](bool bSameKind)
        {
            Eligible.Reset();
            for (int32 I=0; I<Candidates.Num(); ++I)
            {
                const auto& C=Candidates[I];
                if (Used.Contains(C.Id) || C.MinimumDepth>Depth || C.SilverCost<1 || C.SilverCost>SilverCap
                    || (bSameKind && C.Kind!=Wanted)) { continue; }
                if (C.Kind==ELKMarketOfferKind::Mercenary)
                {
                    const int32 Tier=int32(C.Quality)-int32(ELKQuality::Uncommon);
                    if (Tier<0 || Tier>2 || MercenaryQualityWeights[Depth][Tier]<=0.f) { continue; }
                }
                Eligible.Add(I);
            }
            if (bSameKind && Wanted!=ELKMarketOfferKind::Mercenary && !Eligible.IsEmpty())
            {
                int32 Highest=-1; for (int32 I:Eligible) { Highest=FMath::Max(Highest,Candidates[I].MinimumDepth); }
                Eligible.RemoveAll([&](int32 I){return Candidates[I].MinimumDepth!=Highest;});
            }
        };
        Collect(true); if (Eligible.IsEmpty()) { Collect(false); }
        if (Eligible.IsEmpty()) { continue; }
        FRandomStream Stream=PurposeStream(Node,RunSeed,0xCA000010u+uint32(Slot));
        // Keep quality mass independent from how many characters that quality contains.
        TArray<float> Weights;
        for (int32 Index:Eligible)
        {
            const auto& C=Candidates[Index]; float Weight=1.f;
            if (C.Kind==ELKMarketOfferKind::Mercenary)
            {
                int32 Count=0; for (int32 Other:Eligible) { if (Candidates[Other].Kind==C.Kind && Candidates[Other].Quality==C.Quality) { ++Count; } }
                Weight=MercenaryQualityWeights[Depth][int32(C.Quality)-int32(ELKQuality::Uncommon)]/float(Count);
            }
            Weights.Add(Weight);
        }
        const int32 Choice=PickWeighted(Stream,Weights); if (Choice==INDEX_NONE) { continue; }
        const auto& C=Candidates[Eligible[Choice]];
        FLKMarketOffer Offer; Offer.OfferId=MarketIdentity(Node,RunSeed,RunId,0x0F000010u+uint32(Slot)); Offer.Kind=C.Kind; Offer.CardId=C.Id; Offer.Quality=C.Quality;
        Offer.Price=C.Kind==ELKMarketOfferKind::Mercenary?10+C.SilverCost*5+FMath::Max(0,int32(C.Quality)-1)*5:12+C.SilverCost*4+C.MinimumDepth*5;
        Node.MarketOffers.Add(Offer); Used.Add(C.Id);
    }
    FRandomStream ItemStream=PurposeStream(Node,RunSeed,0x17000001u);
    TArray<float> ItemWeights; for (int32 I=0; I<5; ++I) { ItemWeights.Add(ItemQualityWeights[Depth][I]); }
    const int32 ItemQuality=PickWeighted(ItemStream,ItemWeights);
    const auto& Item=LKItemContent::All()[ItemStream.RandRange(0,LKItemContent::All().Num()-1)];
    FLKMarketOffer ItemOffer; ItemOffer.OfferId=MarketIdentity(Node,RunSeed,RunId,0x0F000020u); ItemOffer.Kind=ELKMarketOfferKind::Item;
    ItemOffer.ItemInstanceId=MarketIdentity(Node,RunSeed,RunId,0x0F000021u); ItemOffer.ItemId=Item.ItemId; ItemOffer.Quality=ELKQuality(ItemQuality); ItemOffer.Price=5+ItemQuality*3;
    Node.MarketOffers.Add(ItemOffer);

    // Prefer the newest eligible geographic tier. Lower-tier fallback is explicit after ownership filtering.
    TArray<const FLKResearchDefinition*> ResearchPool; int32 HighestDepth=-1;
    for (const auto& Research:LKResearchContent::All())
    {
        if (Research.MinimumRegionDepth>Depth || ResearchExclusions.Contains(Research.CardId)
            || CarriedResearchMaterials.FindRef(Research.CardId)>0) { continue; }
        if (Research.MinimumRegionDepth>HighestDepth) { ResearchPool.Reset(); HighestDepth=Research.MinimumRegionDepth; }
        if (Research.MinimumRegionDepth==HighestDepth) { ResearchPool.Add(&Research); }
    }
    if (!ResearchPool.IsEmpty())
    {
        FRandomStream ResearchStream=PurposeStream(Node,RunSeed,0x5EA00001u);
        TArray<float> Weights; for (const auto* Research:ResearchPool) { Weights.Add(Research->Weight); }
        const int32 Choice=PickWeighted(ResearchStream,Weights);
        if (Choice!=INDEX_NONE)
        {
            const auto& Research=*ResearchPool[Choice];
            FLKMarketOffer Offer; Offer.OfferId=MarketIdentity(Node,RunSeed,RunId,0x0F000030u); Offer.Kind=Research.Kind;
            Offer.CardId=Research.CardId; Offer.Quality=ResearchQuality(Research); Offer.Price=Research.Price;
            Node.MarketOffers.Add(Offer);
        }
    }
    Node.MarketGenerationVersion=CurrentGenerationVersion; Node.bMarketGenerated=true;
    UE_LOG(LogLK, Verbose, TEXT("[Market] v%d node=%s depth=%d silver=%.1f temporary=%d items=1 research=%d candidates=%d"),
        CurrentGenerationVersion,*Node.NodeId.ToString(),Depth,SilverCap,
        Node.MarketOffers.Num()-1-(ResearchPool.IsEmpty()?0:1),ResearchPool.IsEmpty()?0:1,ResearchPool.Num());
}
}
