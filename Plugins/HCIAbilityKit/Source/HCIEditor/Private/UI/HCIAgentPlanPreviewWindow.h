#pragma once

#include "CoreMinimal.h"

#include "Agent/Executor/HCIAgentExecutor.h"
#include "Agent/Planner/HCIAgentPlan.h"

struct FHCIAgentPlanPreviewRow
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

struct FHCIAgentPlanPreviewContext
{
	FString RouteReason;
	FString PlannerProvider = TEXT("-");
	FString ProviderMode = TEXT("-");
	bool bFallbackUsed = false;
	FString FallbackReason = TEXT("-");
	int32 EnvScannedAssetCount = INDEX_NONE;
};

struct FHCIAgentPlanCommitRiskSummary
{
	int32 TotalSteps = 0;
	int32 WriteLikeSteps = 0;
	int32 DestructiveSteps = 0;
	bool bRequiresConfirmDialog = false;
};

enum class EHCIAgentExecutionLocateTargetKind : uint8
{
	Asset,
	Actor
};

struct FHCIAgentExecutionLocateTarget
{
	EHCIAgentExecutionLocateTargetKind Kind = EHCIAgentExecutionLocateTargetKind::Asset;
	FString DisplayLabel;
	FString TargetPath;
	FString Detail;
	FString SourceToolName;
	FString SourceEvidenceKey;
};

struct FHCIAgentPlanExecutionReport
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
	TArray<FHCIAgentExecutorStepResult> StepResults;
	TArray<FHCIAgentExecutionLocateTarget> LocateTargets;
};

class HCIEDITOR_API FHCIAgentPlanPreviewWindow
{
public:
	static void OpenWindow(const FHCIAgentPlan& Plan, const FHCIAgentPlanPreviewContext& Context = FHCIAgentPlanPreviewContext());
	static TArray<TSharedPtr<FHCIAgentPlanPreviewRow>> BuildRows(const FHCIAgentPlan& Plan);
	static FHCIAgentPlanCommitRiskSummary BuildCommitRiskSummary(const FHCIAgentPlan& Plan);
	static FString BuildCommitConfirmMessage(const FHCIAgentPlan& Plan);
	static FString BuildSearchPathEvidenceSummary(const TArray<FHCIAgentExecutorStepResult>& StepResults);
	static void BuildLocateTargetsFromStepResults(
		const TArray<FHCIAgentExecutorStepResult>& StepResults,
		TArray<FHCIAgentExecutionLocateTarget>& OutTargets);
	static bool ExecutePlan(
		const FHCIAgentPlan& Plan,
		bool bDryRun,
		bool bUserConfirmedWriteSteps,
		FHCIAgentPlanExecutionReport& OutReport,
		TFunction<void(int32 /*StepIndex*/, int32 /*TotalSteps*/, const FHCIAgentPlanStep& /*Step*/)> OnStepBegin = nullptr);
};


