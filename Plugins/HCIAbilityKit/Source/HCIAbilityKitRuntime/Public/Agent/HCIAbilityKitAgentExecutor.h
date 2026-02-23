#pragma once

#include "CoreMinimal.h"

#include "Agent/HCIAbilityKitAgentPlan.h"
#include "Agent/HCIAbilityKitAgentPlanValidator.h"
#include "Agent/HCIAbilityKitToolRegistry.h"

enum class EHCIAbilityKitAgentExecutorTerminationPolicy : uint8
{
	StopOnFirstFailure,
	ContinueOnFailure
};

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentExecutorOptions
{
	bool bValidatePlanBeforeExecute = true;
	bool bDryRun = true;
	EHCIAbilityKitAgentExecutorTerminationPolicy TerminationPolicy = EHCIAbilityKitAgentExecutorTerminationPolicy::StopOnFirstFailure;

	// F4 demo seam: used by tests/console demo to validate step-level failure convergence without real tool execution.
	int32 SimulatedFailureStepIndex = INDEX_NONE; // 0-based; INDEX_NONE disables simulation
	FString SimulatedFailureErrorCode = TEXT("E4101");
	FString SimulatedFailureReason = TEXT("simulated_tool_execution_failed");
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
	FString TerminationPolicy;
	FString TerminalStatus;
	FString TerminalReason;

	int32 TotalSteps = 0;
	int32 ExecutedSteps = 0;
	int32 SucceededSteps = 0;
	int32 FailedSteps = 0;
	int32 SkippedSteps = 0;
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
