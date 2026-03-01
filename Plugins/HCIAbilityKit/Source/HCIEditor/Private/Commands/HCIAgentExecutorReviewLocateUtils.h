#pragma once

#include "CoreMinimal.h"
#include "Agent/Executor/HCIDryRunDiff.h"

struct FHCIAgentExecutorReviewLocateResolvedRow
{
	int32 RowIndex = INDEX_NONE;
	FString AssetPath;
	FString ActorPath;
	FString ToolName;
	EHCIDryRunObjectType ObjectType = EHCIDryRunObjectType::Asset;
	EHCIDryRunLocateStrategy LocateStrategy = EHCIDryRunLocateStrategy::SyncBrowser;
};

class HCIEDITOR_API FHCIAgentExecutorReviewLocateUtils
{
public:
	static bool TryResolveRow(
		const FHCIDryRunDiffReport& Report,
		int32 RowIndex,
		FHCIAgentExecutorReviewLocateResolvedRow& OutResolved,
		FString& OutReason);

	static bool TryLocateResolvedRowInEditor(
		const FHCIAgentExecutorReviewLocateResolvedRow& Resolved,
		FString& OutReason);
};


