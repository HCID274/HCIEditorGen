#pragma once

#include "CoreMinimal.h"

#include "Agent/Executor/HCIAbilityKitAgentExecutor.h"
#include "Agent/Planner/HCIAbilityKitAgentPlan.h"

struct FHCIAbilityKitAgentPlanPreviewRow
{
	int32 StepIndex = INDEX_NONE;
	FString StepId;
	FString ToolName;
	FString RiskLevel;
	FString StepStateLabel;
	TArray<FString> AssetObjectPaths;
	FString AssetCountLabel = TEXT("0");
	FString ArgsPreview;
	TArray<FString> ResolvedAssetObjectPaths;
	bool bLocateEnabled = false;
	FString LocateTooltip;
};

struct FHCIAbilityKitAgentPlanPreviewContext
{
	FString RouteReason;
	FString PlannerProvider = TEXT("-");
	FString ProviderMode = TEXT("-");
	bool bFallbackUsed = false;
	FString FallbackReason = TEXT("-");
	int32 EnvScannedAssetCount = INDEX_NONE;
};

struct FHCIAbilityKitAgentPlanCommitRiskSummary
{
	int32 TotalSteps = 0;
	int32 WriteLikeSteps = 0;
	int32 DestructiveSteps = 0;
	bool bRequiresConfirmDialog = false;
};

class HCIABILITYKITEDITOR_API FHCIAbilityKitAgentPlanPreviewWindow
{
public:
	static void OpenWindow(const FHCIAbilityKitAgentPlan& Plan, const FHCIAbilityKitAgentPlanPreviewContext& Context = FHCIAbilityKitAgentPlanPreviewContext());
	static TArray<TSharedPtr<FHCIAbilityKitAgentPlanPreviewRow>> BuildRows(const FHCIAbilityKitAgentPlan& Plan);
	static FHCIAbilityKitAgentPlanCommitRiskSummary BuildCommitRiskSummary(const FHCIAbilityKitAgentPlan& Plan);
	static FString BuildCommitConfirmMessage(const FHCIAbilityKitAgentPlan& Plan);
	static FString BuildSearchPathEvidenceSummary(const TArray<FHCIAbilityKitAgentExecutorStepResult>& StepResults);
};
