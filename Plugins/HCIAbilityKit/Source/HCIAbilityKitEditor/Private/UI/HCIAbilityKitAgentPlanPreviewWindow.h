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

enum class EHCIAbilityKitAgentExecutionLocateTargetKind : uint8
{
	Asset,
	Actor
};

struct FHCIAbilityKitAgentExecutionLocateTarget
{
	EHCIAbilityKitAgentExecutionLocateTargetKind Kind = EHCIAbilityKitAgentExecutionLocateTargetKind::Asset;
	FString DisplayLabel;
	FString TargetPath;
	FString Detail;
	FString SourceToolName;
	FString SourceEvidenceKey;
};

struct FHCIAbilityKitAgentPlanExecutionReport
{
	bool bRunOk = false;
	bool bDryRun = true;
	FString ExecutionMode = TEXT("-");
	FString TerminalStatus = TEXT("-");
	FString TerminalReason = TEXT("-");
	int32 SucceededSteps = 0;
	int32 FailedSteps = 0;
	int32 ScannedAssets = 0;
	FString SummaryText;
	FString SearchPathEvidenceText;
	TArray<FHCIAbilityKitAgentExecutorStepResult> StepResults;
	TArray<FHCIAbilityKitAgentExecutionLocateTarget> LocateTargets;
};

class HCIABILITYKITEDITOR_API FHCIAbilityKitAgentPlanPreviewWindow
{
public:
	static void OpenWindow(const FHCIAbilityKitAgentPlan& Plan, const FHCIAbilityKitAgentPlanPreviewContext& Context = FHCIAbilityKitAgentPlanPreviewContext());
	static TArray<TSharedPtr<FHCIAbilityKitAgentPlanPreviewRow>> BuildRows(const FHCIAbilityKitAgentPlan& Plan);
	static FHCIAbilityKitAgentPlanCommitRiskSummary BuildCommitRiskSummary(const FHCIAbilityKitAgentPlan& Plan);
	static FString BuildCommitConfirmMessage(const FHCIAbilityKitAgentPlan& Plan);
	static FString BuildSearchPathEvidenceSummary(const TArray<FHCIAbilityKitAgentExecutorStepResult>& StepResults);
	static void BuildLocateTargetsFromStepResults(
		const TArray<FHCIAbilityKitAgentExecutorStepResult>& StepResults,
		TArray<FHCIAbilityKitAgentExecutionLocateTarget>& OutTargets);
	static bool ExecutePlan(
		const FHCIAbilityKitAgentPlan& Plan,
		bool bDryRun,
		bool bUserConfirmedWriteSteps,
		FHCIAbilityKitAgentPlanExecutionReport& OutReport,
		TFunction<void(int32 /*StepIndex*/, int32 /*TotalSteps*/, const FHCIAbilityKitAgentPlanStep& /*Step*/)> OnStepBegin = nullptr);
};
