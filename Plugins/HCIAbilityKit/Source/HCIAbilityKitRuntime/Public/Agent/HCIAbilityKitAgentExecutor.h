#pragma once

#include "CoreMinimal.h"

#include "Agent/HCIAbilityKitAgentPlan.h"
#include "Agent/HCIAbilityKitAgentPlanValidator.h"
#include "Agent/HCIAbilityKitToolRegistry.h"

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentExecutorOptions
{
	bool bValidatePlanBeforeExecute = true;
	bool bDryRun = true;
};

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentExecutorStepResult
{
	int32 StepIndex = INDEX_NONE; // 0-based
	FString StepId;
	FString ToolName;
	FString Capability;
	FString RiskLevel;
	bool bWriteLike = false;

	bool bAttempted = false;
	bool bSucceeded = false;
	FString Status;

	int32 TargetCountEstimate = 0;
	TArray<FString> EvidenceKeys;
	TMap<FString, FString> Evidence;

	FString ErrorCode;
	FString Reason;
};

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentExecutorRunResult
{
	bool bAccepted = false;
	bool bCompleted = false;
	FString ErrorCode;
	FString Reason;

	FString RequestId;
	FString Intent;
	int32 PlanVersion = 0;

	FString ExecutionMode;
	FString TerminalStatus;
	FString TerminalReason;

	int32 TotalSteps = 0;
	int32 ExecutedSteps = 0;
	int32 SucceededSteps = 0;
	int32 FailedSteps = 0;
	int32 FailedStepIndex = INDEX_NONE; // 0-based
	FString FailedStepId;
	FString FailedToolName;

	FString StartedAtUtc;
	FString FinishedAtUtc;

	TArray<FHCIAbilityKitAgentExecutorStepResult> StepResults;
};

class HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentExecutor
{
public:
	static bool ExecutePlan(
		const FHCIAbilityKitAgentPlan& Plan,
		const FHCIAbilityKitToolRegistry& ToolRegistry,
		FHCIAbilityKitAgentExecutorRunResult& OutResult);

	static bool ExecutePlan(
		const FHCIAbilityKitAgentPlan& Plan,
		const FHCIAbilityKitToolRegistry& ToolRegistry,
		const FHCIAbilityKitAgentPlanValidationContext& ValidationContext,
		const FHCIAbilityKitAgentExecutorOptions& Options,
		FHCIAbilityKitAgentExecutorRunResult& OutResult);
};

