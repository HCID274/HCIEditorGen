#include "HCIAbilityKitAsset.h"

#if WITH_EDITORONLY_DATA
#include "EditorFramework/AssetImportData.h"
#include "UObject/AssetRegistryTagsContext.h"
#endif

#if WITH_EDITORONLY_DATA
void UHCIAbilityKitAsset::PostInitProperties()
{
	Super::PostInitProperties();

	// 如果不是 CDO（类默认对象）且导入数据对象为空，则创建一个新的导入数据对象喵
	if (!HasAnyFlags(RF_ClassDefaultObject) && !AssetImportData)
	{
		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
	}
}

void UHCIAbilityKitAsset::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
	// 将源文件的导入信息添加到资产注册表标签中，方便在内容浏览器中查看源路径信息喵
	if (AssetImportData)
	{
		Context.AddTag(UObject::FAssetRegistryTag(SourceFileTagName(), AssetImportData->GetSourceData().ToJson(), UObject::FAssetRegistryTag::TT_Hidden));
	}

	Super::GetAssetRegistryTags(Context);
}
#endif
