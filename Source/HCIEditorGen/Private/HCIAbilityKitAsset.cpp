// Fill out your copyright notice in the Description page of Project Settings.


#include "HCIEditorGen/Public/HCIAbilityKitAsset.h"
#if WITH_EDITORONLY_DATA
#include "EditorFramework/AssetImportData.h"
#include "UObject/AssetRegistryTagsContext.h"
#endif

#if WITH_EDITORONLY_DATA
void UHCIAbilityKitAsset::PostInitProperties()
{
	if (!HasAnyFlags(RF_ClassDefaultObject) && !AssetImportData)
	{
 		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
	}

	Super::PostInitProperties();
}

void UHCIAbilityKitAsset::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
	if (AssetImportData)
	{
 		Context.AddTag(UObject::FAssetRegistryTag(SourceFileTagName(), AssetImportData->GetSourceData().ToJson(),
		UObject::FAssetRegistryTag::TT_Hidden));
	}

	Super::GetAssetRegistryTags(Context);
}
#endif