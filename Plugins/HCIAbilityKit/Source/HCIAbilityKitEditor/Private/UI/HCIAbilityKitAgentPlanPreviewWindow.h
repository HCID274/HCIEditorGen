#pragma once

#include "CoreMinimal.h"

#include "Agent/HCIAbilityKitAgentPlan.h"

struct FHCIAbilityKitAgentPlanPreviewRow
{
	int32 StepIndex = INDEX_NONE;
	FString StepId;
	FString ToolName;
	FString RiskLevel;
	TArray<FString> AssetObjectPaths;
	FString AssetCountLabel = TEXT("0");
	TArray<FString> ResolvedAssetObjectPaths;
	bool bLocateEnabled = false;
	FString LocateTooltip;
};

class FHCIAbilityKitAgentPlanPreviewWindow
{
public:
	static void OpenWindow(const FHCIAbilityKitAgentPlan& Plan, int32 EnvScannedAssetCount = INDEX_NONE);
	static TArray<TSharedPtr<FHCIAbilityKitAgentPlanPreviewRow>> BuildRows(const FHCIAbilityKitAgentPlan& Plan);
};
