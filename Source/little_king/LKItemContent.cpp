#include "LKItemContent.h"

namespace LKItemContent
{
const TArray<FLKItemDefinition>& All()
{
    static const TArray<FLKItemDefinition> Definitions = {
        { "Item_Accessory01", FText::FromString(TEXT("饰品占位")), FText::FromString(TEXT("占位·暂无属性。可购买、携带和带回道具仓库；当前不装备，不影响战力。")), ELKItemKind::Accessory, true },
        { "Item_Equipment01", FText::FromString(TEXT("装备占位")), FText::FromString(TEXT("占位·暂无属性。可购买、携带和带回道具仓库；当前不装备，不影响战力。")), ELKItemKind::Equipment, true }
    };
    return Definitions;
}
const FLKItemDefinition* Find(FName Id)
{
    return All().FindByPredicate([Id](const FLKItemDefinition& Definition){ return Definition.ItemId==Id; });
}
bool ValidateInstances(const TArray<FLKItemInstance>& Items, FString& Error)
{
    Error.Reset();
    if (Items.Num()>10000) { Error=TEXT("道具库存超出保存上限"); return false; }
    TSet<FGuid> Seen;
    for (const FLKItemInstance& Item:Items)
    {
        if (!Item.InstanceId.IsValid() || Seen.Contains(Item.InstanceId) || !Find(Item.ItemId)
            || uint8(Item.Quality)>uint8(ELKQuality::Legendary))
        { Error=TEXT("道具身份、实例或品质无效"); return false; }
        Seen.Add(Item.InstanceId);
    }
    return true;
}
bool EqualInstances(const TArray<FLKItemInstance>& A, const TArray<FLKItemInstance>& B)
{
    if (A.Num()!=B.Num()) { return false; }
    for (int32 I=0; I<A.Num(); ++I)
    { if (A[I].InstanceId!=B[I].InstanceId || A[I].ItemId!=B[I].ItemId || A[I].Quality!=B[I].Quality) { return false; } }
    return true;
}
}
