#include "AgentActions/ToolActions/HCIAbilityKitToolActionFactories.h"

#include "AgentActions/Support/HCIAbilityKitToolActionAssetPathNormalizer.h"
#include "AgentActions/Support/HCIAbilityKitToolActionEvidenceBuilder.h"
#include "AgentActions/Support/HCIAbilityKitToolActionParamParser.h"

#include "EditorAssetLibrary.h"

namespace
{
class FHCIAbilityKitScanAssetsToolAction final : public IHCIAbilityKitAgentToolAction
{
public:
	virtual FName GetToolName() const override
	{
		return TEXT("ScanAssets");
	}

	virtual bool DryRun(
		const FHCIAbilityKitAgentToolActionRequest& Request,
		FHCIAbilityKitAgentToolActionResult& OutResult) const override
	{
		return RunInternal(Request, OutResult);
	}

	virtual bool Execute(
		const FHCIAbilityKitAgentToolActionRequest& Request,
		FHCIAbilityKitAgentToolActionResult& OutResult) const override
	{
		return RunInternal(Request, OutResult);
	}

private:
	static bool RunInternal(
		const FHCIAbilityKitAgentToolActionRequest& Request,
		FHCIAbilityKitAgentToolActionResult& OutResult)
	{
		const FHCIAbilityKitToolActionParamParser Params(Request.Args);

		FString Directory = TEXT("/Game/Temp");
		FString DirectoryArg;
		if (Params.TryGetOptionalStringFieldRaw(TEXT("directory"), DirectoryArg))
		{
			Directory = DirectoryArg;
		}
		if (Params.TryGetOptionalStringFieldRaw(TEXT("target_root"), DirectoryArg))
		{
			Directory = DirectoryArg;
		}
		if (Directory.IsEmpty() || !FHCIAbilityKitToolActionAssetPathNormalizer::IsGamePathLoose(Directory))
		{
			Directory = TEXT("/Game/Temp");
		}

		const TArray<FString> AssetPaths = UEditorAssetLibrary::ListAssets(Directory, true, false);
		FHCIAbilityKitToolActionEvidenceBuilder::SetSucceeded(OutResult, TEXT("scan_assets_ok"), AssetPaths.Num());
		OutResult.Evidence.Add(TEXT("scan_root"), Directory);
		FHCIAbilityKitToolActionEvidenceBuilder::AddEvidenceInt(OutResult, TEXT("asset_count"), AssetPaths.Num());
		OutResult.Evidence.Add(TEXT("result"), FString::Printf(TEXT("scan_assets_ok count=%d"), AssetPaths.Num()));
		OutResult.Evidence.Add(TEXT("asset_path"), AssetPaths.Num() > 0 ? AssetPaths[0] : TEXT("-"));
		OutResult.Evidence.Add(TEXT("asset_paths"), FString::Join(AssetPaths, TEXT("|")));
		return true;
	}
};
}

TSharedPtr<IHCIAbilityKitAgentToolAction> HCIAbilityKitToolActionFactories::MakeScanAssetsToolAction()
{
	return MakeShared<FHCIAbilityKitScanAssetsToolAction>();
}
