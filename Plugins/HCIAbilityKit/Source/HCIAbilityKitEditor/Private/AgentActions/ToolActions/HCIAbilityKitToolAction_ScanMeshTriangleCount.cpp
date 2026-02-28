#include "AgentActions/ToolActions/HCIAbilityKitToolActionFactories.h"

#include "AgentActions/ToolActions/HCIAbilityKitAgentToolActions_LegacyShared.h"

#include "EditorAssetLibrary.h"
#include "Engine/StaticMesh.h"

namespace
{
class FHCIAbilityKitScanMeshTriangleCountToolAction final : public IHCIAbilityKitAgentToolAction
{
public:
	virtual FName GetToolName() const override
	{
		return TEXT("ScanMeshTriangleCount");
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
	struct FHCIStaticMeshTriangleEntry
	{
		FString AssetPath;
		int32 TriangleCountLod0 = 0;
	};

	static bool RunInternal(
		const FHCIAbilityKitAgentToolActionRequest& Request,
		FHCIAbilityKitAgentToolActionResult& OutResult)
	{
		FString Directory = TEXT("/Game/Temp");
		if (Request.Args.IsValid())
		{
			Request.Args->TryGetStringField(TEXT("directory"), Directory);
		}
		if (Directory.IsEmpty() || !Directory.StartsWith(TEXT("/Game")))
		{
			Directory = TEXT("/Game/Temp");
		}

		const TArray<FString> CandidateAssetPaths = UEditorAssetLibrary::ListAssets(Directory, true, false);
		TArray<FHCIStaticMeshTriangleEntry> MeshEntries;
		MeshEntries.Reserve(CandidateAssetPaths.Num());

		for (const FString& CandidatePath : CandidateAssetPaths)
		{
			FString AssetPath;
			FString ObjectPath;
			HCI_NormalizeAssetPathVariants(CandidatePath, AssetPath, ObjectPath);
			if (!UEditorAssetLibrary::DoesAssetExist(AssetPath))
			{
				continue;
			}

			UObject* Asset = LoadObject<UObject>(nullptr, *ObjectPath);
			UStaticMesh* Mesh = Cast<UStaticMesh>(Asset);
			if (Mesh == nullptr)
			{
				continue;
			}

			const int32 TriangleCountLod0 = Mesh->GetNumTriangles(0);
			FHCIStaticMeshTriangleEntry& Entry = MeshEntries.AddDefaulted_GetRef();
			Entry.AssetPath = AssetPath;
			Entry.TriangleCountLod0 = FMath::Max(0, TriangleCountLod0);
		}

		MeshEntries.Sort(
			[](const FHCIStaticMeshTriangleEntry& Lhs, const FHCIStaticMeshTriangleEntry& Rhs)
			{
				if (Lhs.TriangleCountLod0 != Rhs.TriangleCountLod0)
				{
					return Lhs.TriangleCountLod0 > Rhs.TriangleCountLod0;
				}
				return Lhs.AssetPath < Rhs.AssetPath;
			});

		const int32 TopN = FMath::Min(10, MeshEntries.Num());
		TArray<FString> TopMeshes;
		TopMeshes.Reserve(TopN);
		for (int32 Index = 0; Index < TopN; ++Index)
		{
			const FHCIStaticMeshTriangleEntry& Entry = MeshEntries[Index];
			TopMeshes.Add(FString::Printf(TEXT("%s:%d"), *Entry.AssetPath, Entry.TriangleCountLod0));
		}

		const FHCIStaticMeshTriangleEntry* MaxEntry = MeshEntries.Num() > 0 ? &MeshEntries[0] : nullptr;
		OutResult = FHCIAbilityKitAgentToolActionResult();
		OutResult.bSucceeded = true;
		OutResult.Reason = TEXT("scan_mesh_triangle_count_ok");
		OutResult.EstimatedAffectedCount = MeshEntries.Num();
		OutResult.Evidence.Add(TEXT("scan_root"), Directory);
		OutResult.Evidence.Add(TEXT("scanned_count"), FString::FromInt(CandidateAssetPaths.Num()));
		OutResult.Evidence.Add(TEXT("mesh_count"), FString::FromInt(MeshEntries.Num()));
		OutResult.Evidence.Add(TEXT("max_triangle_count"), FString::FromInt(MaxEntry ? MaxEntry->TriangleCountLod0 : 0));
		OutResult.Evidence.Add(TEXT("max_triangle_asset"), MaxEntry ? MaxEntry->AssetPath : TEXT("-"));
		OutResult.Evidence.Add(TEXT("top_meshes"), TopMeshes.Num() > 0 ? FString::Join(TopMeshes, TEXT(" | ")) : TEXT("none"));
		OutResult.Evidence.Add(TEXT("result"), TEXT("scan_mesh_triangle_count_ok"));
		return true;
	}
};
}

TSharedPtr<IHCIAbilityKitAgentToolAction> HCIAbilityKitToolActionFactories::MakeScanMeshTriangleCountToolAction()
{
	return MakeShared<FHCIAbilityKitScanMeshTriangleCountToolAction>();
}

