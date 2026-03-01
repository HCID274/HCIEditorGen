#include "AgentActions/ToolActions/HCIToolActionFactories.h"

#include "AgentActions/Support/HCIToolActionAssetPathNormalizer.h"
#include "AgentActions/Support/HCIToolActionEvidenceBuilder.h"
#include "AgentActions/Support/HCIToolActionParamParser.h"

#include "EditorAssetLibrary.h"

namespace
{
class FHCIScanAssetsToolAction final : public IHCIAgentToolAction
{
public:
	virtual FName GetToolName() const override
	{
		return TEXT("ScanAssets");
	}

	virtual bool DryRun(
		const FHCIAgentToolActionRequest& Request,
		FHCIAgentToolActionResult& OutResult) const override
	{
		return RunInternal(Request, OutResult);
	}

	virtual bool Execute(
		const FHCIAgentToolActionRequest& Request,
		FHCIAgentToolActionResult& OutResult) const override
	{
		return RunInternal(Request, OutResult);
	}

private:
	static bool RunInternal(
		const FHCIAgentToolActionRequest& Request,
		FHCIAgentToolActionResult& OutResult)
	{
		const FHCIToolActionParamParser Params(Request.Args);

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
		if (Directory.IsEmpty() || !FHCIToolActionAssetPathNormalizer::IsGamePathLoose(Directory))
		{
			Directory = TEXT("/Game/Temp");
		}

		const TArray<FString> AssetPaths = UEditorAssetLibrary::ListAssets(Directory, true, false);
		FHCIToolActionEvidenceBuilder::SetSucceeded(OutResult, TEXT("scan_assets_ok"), AssetPaths.Num());
		OutResult.Evidence.Add(TEXT("scan_root"), Directory);
		FHCIToolActionEvidenceBuilder::AddEvidenceInt(OutResult, TEXT("asset_count"), AssetPaths.Num());
		OutResult.Evidence.Add(TEXT("result"), FString::Printf(TEXT("scan_assets_ok count=%d"), AssetPaths.Num()));
		OutResult.Evidence.Add(TEXT("asset_path"), AssetPaths.Num() > 0 ? AssetPaths[0] : TEXT("-"));
		OutResult.Evidence.Add(TEXT("asset_paths"), FString::Join(AssetPaths, TEXT("|")));
		return true;
	}
};
}

TSharedPtr<IHCIAgentToolAction> HCIToolActionFactories::MakeScanAssetsToolAction()
{
	return MakeShared<FHCIScanAssetsToolAction>();
}

