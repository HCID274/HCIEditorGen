#include "AgentActions/ToolActions/HCIToolActionFactories.h"

#include "AgentActions/Support/HCIToolActionAssetPathNormalizer.h"
#include "AgentActions/Support/HCIToolActionEvidenceBuilder.h"
#include "AgentActions/Support/HCIToolActionParamParser.h"

#include "EditorAssetLibrary.h"
#include "Engine/StaticMesh.h"

namespace
{
class FHCIScanMeshTriangleCountToolAction final : public IHCIAgentToolAction
{
public:
	virtual FName GetToolName() const override
	{
		return TEXT("ScanMeshTriangleCount");
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
	struct FHCIStaticMeshTriangleEntry
	{
		FString AssetPath;
		int32 TriangleCountLod0 = 0;
	};

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
		if (Directory.IsEmpty() || !FHCIToolActionAssetPathNormalizer::IsGamePathLoose(Directory))
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
			FHCIToolActionAssetPathNormalizer::NormalizeAssetPathVariants(CandidatePath, AssetPath, ObjectPath);
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
		FHCIToolActionEvidenceBuilder::SetSucceeded(OutResult, TEXT("scan_mesh_triangle_count_ok"), MeshEntries.Num());
		OutResult.Evidence.Add(TEXT("scan_root"), Directory);
		FHCIToolActionEvidenceBuilder::AddEvidenceInt(OutResult, TEXT("scanned_count"), CandidateAssetPaths.Num());
		FHCIToolActionEvidenceBuilder::AddEvidenceInt(OutResult, TEXT("mesh_count"), MeshEntries.Num());
		FHCIToolActionEvidenceBuilder::AddEvidenceInt(OutResult, TEXT("max_triangle_count"), MaxEntry ? MaxEntry->TriangleCountLod0 : 0);
		OutResult.Evidence.Add(TEXT("max_triangle_asset"), MaxEntry ? MaxEntry->AssetPath : TEXT("-"));
		FHCIToolActionEvidenceBuilder::AddEvidenceJoinedOrNone(OutResult, TEXT("top_meshes"), TopMeshes, TEXT(" | "));
		OutResult.Evidence.Add(TEXT("result"), TEXT("scan_mesh_triangle_count_ok"));
		return true;
	}
};
}

TSharedPtr<IHCIAgentToolAction> HCIToolActionFactories::MakeScanMeshTriangleCountToolAction()
{
	return MakeShared<FHCIScanMeshTriangleCountToolAction>();
}

