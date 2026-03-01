#include "AgentActions/ToolActions/HCIToolActionFactories.h"

#include "AgentActions/Support/HCIToolActionAssetPathNormalizer.h"
#include "AgentActions/Support/HCIToolActionEvidenceBuilder.h"
#include "AgentActions/Support/HCIToolActionParamParser.h"

#include "EditorAssetLibrary.h"
#include "Engine/StaticMesh.h"

namespace
{
class FHCISetMeshLODGroupToolAction final : public IHCIAgentToolAction
{
public:
	virtual FName GetToolName() const override
	{
		return TEXT("SetMeshLODGroup");
	}

	virtual bool DryRun(
		const FHCIAgentToolActionRequest& Request,
		FHCIAgentToolActionResult& OutResult) const override
	{
		return RunInternal(Request, OutResult, true);
	}

	virtual bool Execute(
		const FHCIAgentToolActionRequest& Request,
		FHCIAgentToolActionResult& OutResult) const override
	{
		return RunInternal(Request, OutResult, false);
	}

	private:
	static bool RunInternal(
		const FHCIAgentToolActionRequest& Request,
		FHCIAgentToolActionResult& OutResult,
		const bool bIsDryRun)
	{
		TArray<FString> AssetPaths;
		FString LODGroup;
		const FHCIToolActionParamParser Params(Request.Args);
		if (!Params.TryGetRequiredStringArray(TEXT("asset_paths"), AssetPaths) ||
			!Params.TryGetRequiredString(TEXT("lod_group"), LODGroup))
		{
			return FHCIToolActionEvidenceBuilder::FailRequiredArgMissing(OutResult);
		}

		TArray<FString> ModifiedAssets;
		TArray<FString> FailedAssets;
		bool bNaniteBlocked = false;

		const FName LODGroupName(*LODGroup);
		struct FHCIPendingLodUpdate
		{
			TObjectPtr<UStaticMesh> Mesh = nullptr;
			FString AssetPath;
		};
		TArray<FHCIPendingLodUpdate> PendingUpdates;

		for (const FString& Path : AssetPaths)
		{
			FString AssetPath;
			FString ObjectPath;
			FHCIToolActionAssetPathNormalizer::NormalizeAssetPathVariants(Path, AssetPath, ObjectPath);

			if (!UEditorAssetLibrary::DoesAssetExist(AssetPath))
			{
				FailedAssets.Add(FString::Printf(TEXT("%s (not_found)"), *AssetPath));
				continue;
			}

			UObject* Asset = LoadObject<UObject>(nullptr, *ObjectPath);
			UStaticMesh* Mesh = Cast<UStaticMesh>(Asset);
			if (!Mesh)
			{
				FailedAssets.Add(FString::Printf(TEXT("%s (not_staticmesh)"), *AssetPath));
				continue;
			}

			if (Mesh->NaniteSettings.bEnabled)
			{
				bNaniteBlocked = true;
				FailedAssets.Add(FString::Printf(TEXT("%s (nanite_enabled_blocked)"), *AssetPath));
				continue;
			}

			if (Mesh->LODGroup == LODGroupName)
			{
				continue;
			}

			FHCIPendingLodUpdate& Update = PendingUpdates.AddDefaulted_GetRef();
			Update.Mesh = Mesh;
			Update.AssetPath = AssetPath;
		}

		if (bNaniteBlocked)
		{
			OutResult = FHCIAgentToolActionResult();
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4010");
			OutResult.Reason = TEXT("lod_tool_nanite_enabled_blocked");
			OutResult.EstimatedAffectedCount = 0;
			OutResult.Evidence.Add(TEXT("target_lod_group"), LODGroup);
			FHCIToolActionEvidenceBuilder::AddEvidenceInt(OutResult, TEXT("scanned_count"), AssetPaths.Num());
			OutResult.Evidence.Add(TEXT("modified_count"), TEXT("0"));
			FHCIToolActionEvidenceBuilder::AddEvidenceInt(OutResult, TEXT("failed_count"), FailedAssets.Num());
			OutResult.Evidence.Add(TEXT("failed_assets"), FString::Join(FailedAssets, TEXT(" | ")));
			OutResult.Evidence.Add(TEXT("result"), TEXT("lod_tool_nanite_enabled_blocked"));
			return false;
		}

		for (const FHCIPendingLodUpdate& Update : PendingUpdates)
		{
			if (!Update.Mesh)
			{
				FailedAssets.Add(TEXT("<invalid_mesh_ptr> (null_mesh_object)"));
				continue;
			}

			if (!bIsDryRun)
			{
				Update.Mesh->Modify();
				Update.Mesh->LODGroup = LODGroupName;
				Update.Mesh->PostEditChange();
				UEditorAssetLibrary::SaveAsset(Update.AssetPath, false);
			}

			ModifiedAssets.Add(Update.AssetPath);
		}

		OutResult = FHCIAgentToolActionResult();
		OutResult.bSucceeded = FailedAssets.Num() == 0 || ModifiedAssets.Num() > 0;
		OutResult.Reason = bIsDryRun ? TEXT("set_mesh_lod_group_dry_run_ok") : TEXT("set_mesh_lod_group_execute_ok");
		OutResult.EstimatedAffectedCount = ModifiedAssets.Num();
		
		OutResult.Evidence.Add(TEXT("target_lod_group"), LODGroup);
		FHCIToolActionEvidenceBuilder::AddEvidenceInt(OutResult, TEXT("scanned_count"), AssetPaths.Num());
		FHCIToolActionEvidenceBuilder::AddEvidenceInt(OutResult, TEXT("modified_count"), ModifiedAssets.Num());
		FHCIToolActionEvidenceBuilder::AddEvidenceInt(OutResult, TEXT("failed_count"), FailedAssets.Num());
		
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

TSharedPtr<IHCIAgentToolAction> HCIToolActionFactories::MakeSetMeshLODGroupToolAction()
{
	return MakeShared<FHCISetMeshLODGroupToolAction>();
}

