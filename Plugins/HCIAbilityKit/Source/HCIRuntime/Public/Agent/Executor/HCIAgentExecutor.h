#pragma once

#include "CoreMinimal.h"

#include "Agent/Planner/HCIAgentPlan.h"
#include "Agent/Tools/HCIAgentToolAction.h"
#include "Agent/Planner/HCIAgentPlanValidator.h"
#include "Agent/Tools/HCIToolRegistry.h"

enum class EHCIAgentExecutorTerminationPolicy : uint8
{
	StopOnFirstFailure,
	ContinueOnFailure
};

struct HCIRUNTIME_API FHCIAgentExecutorOptions
{
	bool bValidatePlanBeforeExecute = true;
	bool bDryRun = true;
	EHCIAgentExecutorTerminationPolicy TerminationPolicy = EHCIAgentExecutorTerminationPolicy::StopOnFirstFailure;

	// F5: integrate Stage E execution gates into executor preflight while still simulating execution.
	bool bEnablePreflightGates = false;
	bool bUserConfirmedWriteSteps = true;

	// F5 mock RBAC seam (default to an authorized Artist write user).
	FString MockUserName = TEXT("artist_a");
	FString MockResolvedRole = TEXT("Artist");
	bool bMockUserMatchedWhitelist = true;
	TArray<FString> MockAllowedCapabilities = {TEXT("read_only"), TEXT("write")};

	// F5 source control seam (default offline-local mode to avoid requiring SC in demo).
	bool bSourceControlEnabled = false;
	bool bSourceControlCheckoutSucceeded = false;

	// F5 LOD safety seam used only for SetMeshLODGroup steps.
	FString SimulatedLodTargetObjectClass = TEXT("UStaticMesh");
	bool bSimulatedLodTargetNaniteEnabled = false;

	// F4 demo seam: used by tests/console demo to validate step-level failure convergence without real tool execution.
	int32 SimulatedFailureStepIndex = INDEX_NONE; // 0-based; INDEX_NONE disables simulation
	FString SimulatedFailureErrorCode = TEXT("E4101");
	FString SimulatedFailureReason = TEXT("simulated_tool_execution_failed");

	// Stage I draft: optional tool action implementations for DryRun/Execute.
	TMap<FName, TSharedPtr<IHCIAgentToolAction>> ToolActions;

	// Stage L-SliceL1: optional UI-facing step begin callback (no semantic impact on execution).
	TFunction<void(int32 /*StepIndex*/, const FHCIAgentPlanStep& /*Step*/)> OnStepBegin;
};

struct HCIRUNTIME_API FHCIAgentExecutorStepResult
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

	FString FailurePhase;  // "-", "precheck", "preflight", "execute"
	FString PreflightGate; // "-", "confirm_gate", "blast_radius", "rbac", "source_control", "lod_safety"
};

struct HCIRUNTIME_API FHCIAgentExecutorRunResult
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
	bool bPreflightEnabled = false;
	FString TerminalStatus;
	FString TerminalReason;

	int32 TotalSteps = 0;
	int32 ExecutedSteps = 0;
	int32 SucceededSteps = 0;
	int32 FailedSteps = 0;
	int32 SkippedSteps = 0;
	int32 PreflightBlockedSteps = 0;
	int32 FailedStepIndex = INDEX_NONE; // 0-based
	FString FailedStepId;
	FString FailedToolName;
	FString FailedGate;

	FString StartedAtUtc;
	FString FinishedAtUtc;

	TArray<FHCIAgentExecutorStepResult> StepResults;
};

class HCIRUNTIME_API FHCIAgentExecutor
{
public:
	static bool ExecutePlan(
		const FHCIAgentPlan& Plan,
		const FHCIToolRegistry& ToolRegistry,
		FHCIAgentExecutorRunResult& OutResult);

	static bool ExecutePlan(
		const FHCIAgentPlan& Plan,
		const FHCIToolRegistry& ToolRegistry,
		const FHCIAgentPlanValidationContext& ValidationContext,
		const FHCIAgentExecutorOptions& Options,
		FHCIAgentExecutorRunResult& OutResult);
};


