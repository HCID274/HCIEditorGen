#include "AgentActions/ToolActions/HCIAbilityKitToolActionFactories.h"

#include "AgentActions/Support/HCIAbilityKitAssetPathUtils.h"
#include "AgentActions/Support/HCIAbilityKitToolActionEvidenceBuilder.h"
#include "AgentActions/Support/HCIAbilityKitToolActionMoveRenameSupport.h"
#include "AgentActions/Support/HCIAbilityKitToolActionParamParser.h"

#include "EditorAssetLibrary.h"

namespace
{
class FHCIAbilityKitRenameAssetToolAction final : public IHCIAbilityKitAgentToolAction
{
public:
	virtual FName GetToolName() const override
	{
		return TEXT("RenameAsset");
	}

	virtual bool DryRun(
		const FHCIAbilityKitAgentToolActionRequest& Request,
		FHCIAbilityKitAgentToolActionResult& OutResult) const override
	{
		FString SourceAssetPath;
		FString NewName;
		const FHCIAbilityKitToolActionParamParser Params(Request.Args);
		if (!Params.TryGetRequiredString(TEXT("asset_path"), SourceAssetPath) ||
			!Params.TryGetRequiredString(TEXT("new_name"), NewName))
		{
			return FHCIAbilityKitToolActionEvidenceBuilder::FailRequiredArgMissing(OutResult);
		}

		OutResult = FHCIAbilityKitAgentToolActionResult();
		if (!HCIAbilityKitToolActionMoveRenameSupport::ValidateSourceAssetExists(SourceAssetPath, OutResult))
		{
			return false;
		}

		FString SourcePackagePath;
		FString SourceAssetName;
		if (!HCIAbilityKitAssetPathUtils::TrySplitObjectPath(SourceAssetPath, SourcePackagePath, SourceAssetName))
		{
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4009");
			OutResult.Reason = TEXT("asset_path_invalid");
			return false;
		}

		const FString SourceDir = HCIAbilityKitAssetPathUtils::GetDirectoryFromPackagePath(SourcePackagePath);
		const FString DestinationPackagePath = FString::Printf(TEXT("%s/%s"), *SourceDir, *NewName);
		const FString DestinationAssetPath = HCIAbilityKitAssetPathUtils::ToObjectPath(DestinationPackagePath, NewName);

		OutResult.bSucceeded = true;
		OutResult.Reason = TEXT("rename_dry_run_ok");
		OutResult.EstimatedAffectedCount = 1;
		OutResult.Evidence.Add(TEXT("asset_path"), SourceAssetPath);
		OutResult.Evidence.Add(TEXT("before"), SourceAssetPath);
		OutResult.Evidence.Add(TEXT("after"), DestinationAssetPath);
		OutResult.Evidence.Add(TEXT("result"), TEXT("rename_dry_run_ok"));
		return true;
	}

	virtual bool Execute(
		const FHCIAbilityKitAgentToolActionRequest& Request,
		FHCIAbilityKitAgentToolActionResult& OutResult) const override
	{
		if (!DryRun(Request, OutResult))
		{
			return false;
		}

		const FString SourceAssetPath = OutResult.Evidence.FindRef(TEXT("before"));
		const FString DestinationAssetPath = OutResult.Evidence.FindRef(TEXT("after"));
		if (SourceAssetPath.IsEmpty() || DestinationAssetPath.IsEmpty())
		{
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4303");
			OutResult.Reason = TEXT("rename_execute_missing_paths");
			return false;
		}

		if (SourceAssetPath.Equals(DestinationAssetPath, ESearchCase::CaseSensitive))
		{
			OutResult.bSucceeded = true;
			OutResult.Reason = TEXT("rename_noop_same_path");
			OutResult.Evidence.Add(TEXT("result"), TEXT("rename_noop_same_path"));
			return true;
		}

		const int32 DotIndex = DestinationAssetPath.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromStart);
		const FString DestinationPackagePath = (DotIndex == INDEX_NONE) ? DestinationAssetPath : DestinationAssetPath.Left(DotIndex);
		const FString DestinationDir = HCIAbilityKitAssetPathUtils::GetDirectoryFromPackagePath(DestinationPackagePath);
		if (!DestinationDir.IsEmpty())
		{
			UEditorAssetLibrary::MakeDirectory(DestinationDir);
		}

		const bool bRenamed = UEditorAssetLibrary::RenameAsset(SourceAssetPath, DestinationAssetPath);
		OutResult.bSucceeded = bRenamed;
		OutResult.ErrorCode = bRenamed ? FString() : TEXT("E4203");
		OutResult.Reason = bRenamed ? TEXT("rename_execute_ok") : TEXT("rename_execute_failed");
		if (bRenamed)
		{
			HCIAbilityKitToolActionMoveRenameSupport::FixupRedirectorReferencers(SourceAssetPath, OutResult);
		}
		OutResult.Evidence.Add(TEXT("result"), bRenamed ? TEXT("rename_execute_ok") : TEXT("rename_execute_failed"));
		return bRenamed;
	}
};
}

TSharedPtr<IHCIAbilityKitAgentToolAction> HCIAbilityKitToolActionFactories::MakeRenameAssetToolAction()
{
	return MakeShared<FHCIAbilityKitRenameAssetToolAction>();
}
