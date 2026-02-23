#pragma once

#include "CoreMinimal.h"
#include "Agent/HCIAbilityKitDryRunDiff.h"

struct FHCIAbilityKitAgentExecutorReviewLocateResolvedRow
{
	int32 RowIndex = INDEX_NONE;
	FString AssetPath;
	FString ActorPath;
	FString ToolName;
	EHCIAbilityKitDryRunObjectType ObjectType = EHCIAbilityKitDryRunObjectType::Asset;
	EHCIAbilityKitDryRunLocateStrategy LocateStrategy = EHCIAbilityKitDryRunLocateStrategy::SyncBrowser;
};

class FHCIAbilityKitAgentExecutorReviewLocateUtils
{
public:
	static bool TryResolveRow(
		const FHCIAbilityKitDryRunDiffReport& Report,
		int32 RowIndex,
		FHCIAbilityKitAgentExecutorReviewLocateResolvedRow& OutResolved,
		FString& OutReason);

	static bool TryLocateResolvedRowInEditor(
		const FHCIAbilityKitAgentExecutorReviewLocateResolvedRow& Resolved,
		FString& OutReason);
};
