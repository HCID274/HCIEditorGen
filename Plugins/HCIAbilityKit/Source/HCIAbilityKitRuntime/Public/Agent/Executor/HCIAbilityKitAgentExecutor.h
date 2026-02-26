#pragma once

#include "CoreMinimal.h"

#include "Agent/Planner/HCIAbilityKitAgentPlan.h"
#include "Agent/Tools/HCIAbilityKitAgentToolAction.h"
#include "Agent/Planner/HCIAbilityKitAgentPlanValidator.h"
#include "Agent/Tools/HCIAbilityKitToolRegistry.h"

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
	TMap<FName, TSharedPtr<IHCIAbilityKitAgentToolAction>> ToolActions;

	// Stage L-SliceL1: optional UI-facing step begin callback (no semantic impact on execution).
	TFunction<void(int32 /*StepIndex*/, const FHCIAbilityKitAgentPlanStep& /*Step*/)> OnStepBegin;
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

	FString FailurePhase;  // "-", "precheck", "preflight", "execute"
	FString PreflightGate; // "-", "confirm_gate", "blast_radius", "rbac", "source_control", "lod_safety"
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
