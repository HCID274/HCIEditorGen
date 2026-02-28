#include "AgentActions/ToolActions/HCIAbilityKitToolActionFactories.h"

#include "AgentActions/Support/HCIAbilityKitToolActionAssetPathNormalizer.h"
#include "AgentActions/Support/HCIAbilityKitToolActionEvidenceBuilder.h"
#include "AgentActions/Support/HCIAbilityKitToolActionParamParser.h"

#include "EditorAssetLibrary.h"
#include "Engine/Texture2D.h"

namespace
{
class FHCIAbilityKitSetTextureMaxSizeToolAction final : public IHCIAbilityKitAgentToolAction
{
public:
	virtual FName GetToolName() const override
	{
		return TEXT("SetTextureMaxSize");
	}

	virtual bool DryRun(
		const FHCIAbilityKitAgentToolActionRequest& Request,
		FHCIAbilityKitAgentToolActionResult& OutResult) const override
	{
		return RunInternal(Request, OutResult, true);
	}

	virtual bool Execute(
		const FHCIAbilityKitAgentToolActionRequest& Request,
		FHCIAbilityKitAgentToolActionResult& OutResult) const override
	{
		return RunInternal(Request, OutResult, false);
	}

private:
	static bool RunInternal(
		const FHCIAbilityKitAgentToolActionRequest& Request,
		FHCIAbilityKitAgentToolActionResult& OutResult,
		const bool bIsDryRun)
	{
		TArray<FString> AssetPaths;
		int32 MaxSize = 0;
		const FHCIAbilityKitToolActionParamParser Params(Request.Args);
		if (!Params.TryGetRequiredStringArray(TEXT("asset_paths"), AssetPaths) ||
			!Params.TryGetRequiredInt(TEXT("max_size"), MaxSize))
		{
			return FHCIAbilityKitToolActionEvidenceBuilder::FailRequiredArgMissing(OutResult);
		}

		TArray<FString> ModifiedAssets;
		TArray<FString> FailedAssets;

		for (const FString& Path : AssetPaths)
		{
			FString AssetPath;
			FString ObjectPath;
			FHCIAbilityKitToolActionAssetPathNormalizer::NormalizeAssetPathVariants(Path, AssetPath, ObjectPath);

			if (!UEditorAssetLibrary::DoesAssetExist(AssetPath))
			{
				FailedAssets.Add(FString::Printf(TEXT("%s (not_found)"), *AssetPath));
				continue;
			}

			UObject* Asset = LoadObject<UObject>(nullptr, *ObjectPath);
			UTexture2D* Texture = Cast<UTexture2D>(Asset);
			if (!Texture)
			{
				FailedAssets.Add(FString::Printf(TEXT("%s (not_texture2d)"), *AssetPath));
				continue;
			}

			if (Texture->MaxTextureSize == MaxSize)
			{
				continue;
			}

			if (!bIsDryRun)
			{
				Texture->Modify();
				Texture->MaxTextureSize = MaxSize;
				Texture->PostEditChange();
				UEditorAssetLibrary::SaveAsset(AssetPath, false);
			}

			ModifiedAssets.Add(AssetPath);
		}

		OutResult = FHCIAbilityKitAgentToolActionResult();
		OutResult.bSucceeded = FailedAssets.Num() == 0 || ModifiedAssets.Num() > 0;
		OutResult.Reason = bIsDryRun ? TEXT("set_texture_max_size_dry_run_ok") : TEXT("set_texture_max_size_execute_ok");
		OutResult.EstimatedAffectedCount = ModifiedAssets.Num();
		
		FHCIAbilityKitToolActionEvidenceBuilder::AddEvidenceInt(OutResult, TEXT("target_max_size"), MaxSize);
		FHCIAbilityKitToolActionEvidenceBuilder::AddEvidenceInt(OutResult, TEXT("scanned_count"), AssetPaths.Num());
		FHCIAbilityKitToolActionEvidenceBuilder::AddEvidenceInt(OutResult, TEXT("modified_count"), ModifiedAssets.Num());
		FHCIAbilityKitToolActionEvidenceBuilder::AddEvidenceInt(OutResult, TEXT("failed_count"), FailedAssets.Num());
		
		if (ModifiedAssets.Num() > 0)
		{
			OutResult.Evidence.Add(TEXT("modified_assets"), FString::Join(ModifiedAssets, TEXT(" | ")));
		}
		if (FailedAssets.Num() > 0)
		{
			OutResult.Evidence.Add(TEXT("failed_assets"), FString::Join(FailedAssets, TEXT(" | ")));
		}
		
		OutResult.Evidence.Add(TEXT("result"), OutResult.Reason);
		return OutResult.bSucceeded;
	}
};
}

TSharedPtr<IHCIAbilityKitAgentToolAction> HCIAbilityKitToolActionFactories::MakeSetTextureMaxSizeToolAction()
{
	return MakeShared<FHCIAbilityKitSetTextureMaxSizeToolAction>();
}
