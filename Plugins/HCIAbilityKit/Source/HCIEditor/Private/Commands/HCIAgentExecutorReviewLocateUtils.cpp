#include "Commands/HCIAgentExecutorReviewLocateUtils.h"

#include "Editor.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "UObject/SoftObjectPath.h"

namespace
{
static bool HCI_ReviewLocate_TryLocateActorByPathCameraFocus(const FString& ActorPath, FString& OutReason)
{
	if (!GEditor)
	{
		OutReason = TEXT("g_editor_unavailable");
		return false;
	}

	UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
	if (!EditorWorld)
	{
		OutReason = TEXT("editor_world_unavailable");
		return false;
	}

	for (TActorIterator<AActor> It(EditorWorld); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}

		if (Actor->GetPathName().Equals(ActorPath, ESearchCase::CaseSensitive))
		{
			GEditor->MoveViewportCamerasToActor(*Actor, false);
			OutReason = TEXT("ok");
			return true;
		}
	}

	OutReason = TEXT("actor_not_found");
	return false;
}

static bool HCI_ReviewLocate_TryLocateAssetSyncBrowser(const FString& AssetPath, FString& OutReason)
{
	if (!GEditor)
	{
		OutReason = TEXT("g_editor_unavailable");
		return false;
	}

	FSoftObjectPath SoftPath(AssetPath);
	UObject* Object = SoftPath.ResolveObject();
	if (!Object)
	{
		Object = SoftPath.TryLoad();
	}

	if (!Object)
	{
		OutReason = TEXT("asset_load_failed");
		return false;
	}

	TArray<UObject*> Objects;
	Objects.Add(Object);
	GEditor->SyncBrowserToObjects(Objects);
	OutReason = TEXT("ok");
	return true;
}
} // namespace

bool FHCIAgentExecutorReviewLocateUtils::TryResolveRow(
	const FHCIDryRunDiffReport& Report,
	int32 RowIndex,
	FHCIAgentExecutorReviewLocateResolvedRow& OutResolved,
	FString& OutReason)
{
	OutResolved = FHCIAgentExecutorReviewLocateResolvedRow();

	if (Report.DiffItems.Num() == 0)
	{
		OutReason = TEXT("no_preview_state");
		return false;
	}

	if (RowIndex < 0 || !Report.DiffItems.IsValidIndex(RowIndex))
	{
		OutReason = TEXT("row_index_out_of_range");
		return false;
	}

	const FHCIDryRunDiffItem& Item = Report.DiffItems[RowIndex];
	OutResolved.RowIndex = RowIndex;
	OutResolved.AssetPath = Item.AssetPath;
	OutResolved.ActorPath = Item.ActorPath;
	OutResolved.ToolName = Item.ToolName;
	OutResolved.ObjectType = Item.ObjectType;
	OutResolved.LocateStrategy = Item.LocateStrategy;

	OutReason = TEXT("ok");
	return true;
}

bool FHCIAgentExecutorReviewLocateUtils::TryLocateResolvedRowInEditor(
	const FHCIAgentExecutorReviewLocateResolvedRow& Resolved,
	FString& OutReason)
{
	if (Resolved.LocateStrategy == EHCIDryRunLocateStrategy::CameraFocus)
	{
		const FString ActorPathToLocate = Resolved.ActorPath.IsEmpty() ? Resolved.AssetPath : Resolved.ActorPath;
		return HCI_ReviewLocate_TryLocateActorByPathCameraFocus(ActorPathToLocate, OutReason);
	}

	return HCI_ReviewLocate_TryLocateAssetSyncBrowser(Resolved.AssetPath, OutReason);
}

