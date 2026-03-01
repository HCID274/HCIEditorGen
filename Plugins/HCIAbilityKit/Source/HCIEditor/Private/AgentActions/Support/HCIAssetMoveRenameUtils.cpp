#include "AgentActions/Support/HCIAssetMoveRenameUtils.h"

#include "AgentActions/Support/HCIAssetPathUtils.h"
#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"
#include "Modules/ModuleManager.h"

namespace HCIAssetMoveRenameUtils
{
FHCIValidateAssetExistsResult ValidateSourceAssetExists(const FString& SourceAssetPath)
{
	FHCIValidateAssetExistsResult Result;
	Result.bOk = false;

	if (!SourceAssetPath.StartsWith(TEXT("/Game/")))
	{
		Result.ErrorCode = TEXT("E4009");
		Result.Reason = TEXT("asset_path_must_start_with_game");
		return Result;
	}

	if (!UEditorAssetLibrary::DoesAssetExist(SourceAssetPath))
	{
		Result.ErrorCode = TEXT("E4201");
		Result.Reason = TEXT("asset_not_found");
		return Result;
	}

	Result.bOk = true;
	return Result;
}

bool MoveAssetWithAssetTools(const FString& SourceAssetPath, const FString& DestinationAssetPath)
{
	UObject* SourceAsset = LoadObject<UObject>(nullptr, *SourceAssetPath);
	if (SourceAsset == nullptr)
	{
		return false;
	}

	FString DestinationPackagePath;
	FString DestinationAssetName;
	if (!HCIAssetPathUtils::TrySplitObjectPath(DestinationAssetPath, DestinationPackagePath, DestinationAssetName))
	{
		return false;
	}

	const FString DestinationDir = HCIAssetPathUtils::GetDirectoryFromPackagePath(DestinationPackagePath);
	if (DestinationDir.IsEmpty())
	{
		return false;
	}

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	TArray<FAssetRenameData> RenameRequests;
	RenameRequests.Emplace(SourceAsset, DestinationDir, DestinationAssetName);
	return AssetToolsModule.Get().RenameAssets(RenameRequests);
}
}


