#include "HCIAsset.h"

#include "Audit/HCIAuditTagNames.h"

#if WITH_EDITORONLY_DATA
#include "EditorFramework/AssetImportData.h"
#include "UObject/AssetRegistryTagsContext.h"
#endif

#if WITH_EDITORONLY_DATA
void UHCIAsset::PostInitProperties()
{
	Super::PostInitProperties();

	// 如果当前对象不是类默认对象（CDO）且尚未创建导入数据对象，则进行初始化
	if (!HasAnyFlags(RF_ClassDefaultObject) && !AssetImportData)
	{
		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
	}
}

void UHCIAsset::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
	Context.AddTag(UObject::FAssetRegistryTag(HCIAuditTagNames::Id, Id, UObject::FAssetRegistryTag::TT_Alphabetical));
	Context.AddTag(UObject::FAssetRegistryTag(HCIAuditTagNames::DisplayName, DisplayName, UObject::FAssetRegistryTag::TT_Alphabetical));
	Context.AddTag(UObject::FAssetRegistryTag(HCIAuditTagNames::Damage, LexToString(Damage), UObject::FAssetRegistryTag::TT_Numerical));
	Context.AddTag(UObject::FAssetRegistryTag(
		HCIAuditTagNames::RepresentingMesh,
		RepresentingMesh.ToSoftObjectPath().ToString(),
		UObject::FAssetRegistryTag::TT_Alphabetical));
	if (TriangleCountLod0Expected >= 0)
	{
		Context.AddTag(UObject::FAssetRegistryTag(
			HCIAuditTagNames::TriangleExpectedLod0,
			LexToString(TriangleCountLod0Expected),
			UObject::FAssetRegistryTag::TT_Numerical));
	}

	// 将源文件的导入信息添加到资产注册表标签中，使源文件路径在内容浏览器中可见
	if (AssetImportData)
	{
		Context.AddTag(UObject::FAssetRegistryTag(SourceFileTagName(), AssetImportData->GetSourceData().ToJson(), UObject::FAssetRegistryTag::TT_Hidden));
	}

	Super::GetAssetRegistryTags(Context);
}
#endif

