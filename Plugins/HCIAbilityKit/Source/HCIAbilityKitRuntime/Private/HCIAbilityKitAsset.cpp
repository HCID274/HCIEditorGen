#include "HCIAbilityKitAsset.h"

#if WITH_EDITORONLY_DATA
#include "EditorFramework/AssetImportData.h"
#include "UObject/AssetRegistryTagsContext.h"
#endif

#if WITH_EDITORONLY_DATA
void UHCIAbilityKitAsset::PostInitProperties()
{
	Super::PostInitProperties();

	// 如果当前对象不是类默认对象（CDO）且尚未创建导入数据对象，则进行初始化
	if (!HasAnyFlags(RF_ClassDefaultObject) && !AssetImportData)
	{
		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
	}
}

void UHCIAbilityKitAsset::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
	// 将源文件的导入信息添加到资产注册表标签中，使源文件路径在内容浏览器中可见
	if (AssetImportData)
	{
		Context.AddTag(UObject::FAssetRegistryTag(SourceFileTagName(), AssetImportData->GetSourceData().ToJson(), UObject::FAssetRegistryTag::TT_Hidden));
	}

	Super::GetAssetRegistryTags(Context);
}
#endif
