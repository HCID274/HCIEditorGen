#include "Agent/HCIAbilityKitAgentExecutor.h"

#include "Agent/HCIAbilityKitAgentExecutionGate.h"
#include "Common/HCIAbilityKitTimeFormat.h"
#include "Dom/JsonObject.h"

namespace
{
static FString HCI_GetCapabilityString(const FHCIAbilityKitToolDescriptor* Tool)
{
	return Tool ? FHCIAbilityKitToolRegistry::CapabilityToString(Tool->Capability) : FString(TEXT("-"));
}

static bool HCI_IsWriteLike(const FHCIAbilityKitToolDescriptor* Tool)
{
	return Tool && FHCIAbilityKitAgentExecutionGate::IsWriteLikeCapability(Tool->Capability);
}

static FString HCI_TerminationPolicyToString(const EHCIAbilityKitAgentExecutorTerminationPolicy Policy)
{
	switch (Policy)
	{
	case EHCIAbilityKitAgentExecutorTerminationPolicy::ContinueOnFailure:
		return TEXT("continue_on_failure");
	case EHCIAbilityKitAgentExecutorTerminationPolicy::StopOnFirstFailure:
	default:
		return TEXT("stop_on_first_failure");
	}
}

static int32 HCI_GetTargetCountEstimate(const FHCIAbilityKitAgentPlanStep& Step)
{
	if (!Step.Args.IsValid())
	{
		return 0;
	}

	const TArray<TSharedPtr<FJsonValue>>* AssetPaths = nullptr;
	if (Step.Args->TryGetArrayField(TEXT("asset_paths"), AssetPaths) && AssetPaths != nullptr)
	{
		return AssetPaths->Num();
	}

	double MaxActorCount = 0.0;
	if (Step.Args->TryGetNumberField(TEXT("max_actor_count"), MaxActorCount))
	{
		return FMath::Max(0, FMath::RoundToInt(MaxActorCount));
	}

	return 0;
}

static FString HCI_TryGetFirstAssetPath(const TSharedPtr<FJsonObject>& Args)
{
	if (!Args.IsValid())
	{
		return FString();
	}

	const TArray<TSharedPtr<FJsonValue>>* AssetPaths = nullptr;
	if (!Args->TryGetArrayField(TEXT("asset_paths"), AssetPaths) || AssetPaths == nullptr || AssetPaths->Num() == 0)
	{
		return FString();
	}

	const FString Value = (*AssetPaths)[0].IsValid() ? (*AssetPaths)[0]->AsString() : FString();
	return Value;
}

static FString HCI_BuildEvidenceValue(const FString& EvidenceKey, const FHCIAbilityKitAgentPlanStep& Step)
{
	if (EvidenceKey == TEXT("asset_path"))
	{
		const FString FirstAssetPath = HCI_TryGetFirstAssetPath(Step.Args);
		return FirstAssetPath.IsEmpty() ? TEXT("-") : FirstAssetPath;
	}

	if (EvidenceKey == TEXT("actor_path"))
	{
		return TEXT("/Temp/EditorSelection/DemoActor");
	}

	if (EvidenceKey == TEXT("before"))
	{
		return TEXT("simulated_before");
	}

	if (EvidenceKey == TEXT("after"))
	{
		return TEXT("simulated_after");
	}

	if (EvidenceKey == TEXT("issue"))
	{
		return TEXT("simulated_issue");
	}

	if (EvidenceKey == TEXT("evidence"))
	{
		return TEXT("simulated_evidence");
	}

	if (EvidenceKey == TEXT("result"))
	{
		return TEXT("simulated_success");
	}

	return TEXT("simulated");
}

static void HCI_InitRunResultBase(const FHCIAbilityKitAgentPlan& Plan, FHCIAbilityKitAgentExecutorRunResult& OutResult)
{
	OutResult = FHCIAbilityKitAgentExecutorRunResult();
	OutResult.RequestId = Plan.RequestId;
	OutResult.Intent = Plan.Intent;
	OutResult.PlanVersion = Plan.PlanVersion;
	OutResult.TotalSteps = Plan.Steps.Num();
}

static FHCIAbilityKitAgentExecutorStepResult& HCI_AddStepResultSkeleton(
	const FHCIAbilityKitAgentPlanStep& Step,
	const int32 StepIndex,
	const FHCIAbilityKitToolRegistry& ToolRegistry,
	FHCIAbilityKitAgentExecutorRunResult& OutResult)
{
	const FHCIAbilityKitToolDescriptor* Tool = ToolRegistry.FindTool(Step.ToolName);
	FHCIAbilityKitAgentExecutorStepResult& StepResult = OutResult.StepResults.AddDefaulted_GetRef();
	StepResult.StepIndex = StepIndex;
	StepResult.StepId = Step.StepId;
	StepResult.ToolName = Step.ToolName.ToString();
	StepResult.Capability = HCI_GetCapabilityString(Tool);
	StepResult.RiskLevel = FHCIAbilityKitAgentPlanContract::RiskLevelToString(Step.RiskLevel);
	StepResult.bWriteLike = HCI_IsWriteLike(Tool);
	StepResult.TargetCountEstimate = HCI_GetTargetCountEstimate(Step);
	StepResult.EvidenceKeys = Step.ExpectedEvidence;
	StepResult.FailurePhase = TEXT("-");
	StepResult.PreflightGate = TEXT("-");
	return StepResult;
}

struct FHCIAbilityKitAgentExecutorPreflightDecision
{
	bool bAllowed = true;
	FString FailedGate;
	FString ErrorCode;
	FString Reason;
};

static FHCIAbilityKitAgentExecutorPreflightDecision HCI_EvaluateExecutorPreflight(
	const FHCIAbilityKitAgentPlan& Plan,
	const FHCIAbilityKitAgentPlanStep& Step,
	const FHCIAbilityKitAgentExecutorStepResult& StepResult,
	const FHCIAbilityKitToolRegistry& ToolRegistry,
	const FHCIAbilityKitAgentExecutorOptions& Options)
{
	FHCIAbilityKitAgentExecutorPreflightDecision Preflight;
	if (!Options.bEnablePreflightGates)
	{
		return Preflight;
	}

	const FHCIAbilityKitAgentExecutionDecision ConfirmDecision = FHCIAbilityKitAgentExecutionGate::EvaluateConfirmGate(
		{Step.StepId, Step.ToolName, Step.bRequiresConfirm, Options.bUserConfirmedWriteSteps},
		ToolRegistry);
	if (!ConfirmDecision.bAllowed)
	{
		Preflight.bAllowed = false;
		Preflight.FailedGate = TEXT("confirm_gate");
		Preflight.ErrorCode = ConfirmDecision.ErrorCode;
		Preflight.Reason = ConfirmDecision.Reason;
		return Preflight;
	}

	const FHCIAbilityKitAgentBlastRadiusDecision BlastDecision = FHCIAbilityKitAgentExecutionGate::EvaluateBlastRadius(
		{Plan.RequestId, Step.ToolName, StepResult.TargetCountEstimate},
		ToolRegistry);
	if (!BlastDecision.bAllowed)
	{
		Preflight.bAllowed = false;
		Preflight.FailedGate = TEXT("blast_radius");
		Preflight.ErrorCode = BlastDecision.ErrorCode;
		Preflight.Reason = BlastDecision.Reason;
		return Preflight;
	}

	const FHCIAbilityKitAgentMockRbacDecision RbacDecision = FHCIAbilityKitAgentExecutionGate::EvaluateMockRbac(
		{
			Plan.RequestId,
			Options.MockUserName,
			Options.MockResolvedRole,
			Options.bMockUserMatchedWhitelist,
			Step.ToolName,
			StepResult.TargetCountEstimate,
			Options.MockAllowedCapabilities},
		ToolRegistry);
	if (!RbacDecision.bAllowed)
	{
		Preflight.bAllowed = false;
		Preflight.FailedGate = TEXT("rbac");
		Preflight.ErrorCode = RbacDecision.ErrorCode;
		Preflight.Reason = RbacDecision.Reason;
		return Preflight;
	}

	const FHCIAbilityKitAgentSourceControlDecision SourceControlDecision = FHCIAbilityKitAgentExecutionGate::EvaluateSourceControlFailFast(
		{Plan.RequestId, Step.ToolName, Options.bSourceControlEnabled, Options.bSourceControlCheckoutSucceeded},
		ToolRegistry);
	if (!SourceControlDecision.bAllowed)
	{
		Preflight.bAllowed = false;
		Preflight.FailedGate = TEXT("source_control");
		Preflight.ErrorCode = SourceControlDecision.ErrorCode;
		Preflight.Reason = SourceControlDecision.Reason;
		return Preflight;
	}

	const FHCIAbilityKitAgentLodToolSafetyDecision LodSafetyDecision = FHCIAbilityKitAgentExecutionGate::EvaluateLodToolSafety(
		{Plan.RequestId, Step.ToolName, Options.SimulatedLodTargetObjectClass, Options.bSimulatedLodTargetNaniteEnabled},
		ToolRegistry);
	if (!LodSafetyDecision.bAllowed)
	{
		Preflight.bAllowed = false;
		Preflight.FailedGate = TEXT("lod_safety");
		Preflight.ErrorCode = LodSafetyDecision.ErrorCode;
		Preflight.Reason = LodSafetyDecision.Reason;
		return Preflight;
	}

	return Preflight;
}
}

bool FHCIAbilityKitAgentExecutor::ExecutePlan(
	const FHCIAbilityKitAgentPlan& Plan,
	const FHCIAbilityKitToolRegistry& ToolRegistry,
	FHCIAbilityKitAgentExecutorRunResult& OutResult)
{
	const FHCIAbilityKitAgentPlanValidationContext DefaultValidationContext;
	const FHCIAbilityKitAgentExecutorOptions DefaultOptions;
	return ExecutePlan(Plan, ToolRegistry, DefaultValidationContext, DefaultOptions, OutResult);
}

bool FHCIAbilityKitAgentExecutor::ExecutePlan(
	const FHCIAbilityKitAgentPlan& Plan,
	const FHCIAbilityKitToolRegistry& ToolRegistry,
	const FHCIAbilityKitAgentPlanValidationContext& ValidationContext,
	const FHCIAbilityKitAgentExecutorOptions& Options,
	FHCIAbilityKitAgentExecutorRunResult& OutResult)
{
	HCI_InitRunResultBase(Plan, OutResult);
	OutResult.ExecutionMode = Options.bDryRun ? TEXT("simulate_dry_run") : TEXT("simulate_apply");
	OutResult.TerminationPolicy = HCI_TerminationPolicyToString(Options.TerminationPolicy);
	OutResult.bPreflightEnabled = Options.bEnablePreflightGates;
	OutResult.StartedAtUtc = FHCIAbilityKitTimeFormat::FormatNowBeijingIso8601();

	if (Options.bValidatePlanBeforeExecute)
	{
		FHCIAbilityKitAgentPlanValidationResult ValidationResult;
		if (!FHCIAbilityKitAgentPlanValidator::ValidatePlan(Plan, ToolRegistry, ValidationContext, ValidationResult))
		{
			OutResult.bAccepted = false;
			OutResult.bCompleted = false;
			OutResult.ErrorCode = ValidationResult.ErrorCode;
			OutResult.Reason = ValidationResult.Reason;
			OutResult.TerminalStatus = TEXT("rejected_precheck");
			OutResult.TerminalReason = TEXT("validator_rejected_plan");
			OutResult.FailedStepIndex = ValidationResult.FailedStepIndex;
			OutResult.FailedStepId = ValidationResult.FailedStepId;
			OutResult.FailedToolName = ValidationResult.FailedToolName;
			OutResult.FinishedAtUtc = FHCIAbilityKitTimeFormat::FormatNowBeijingIso8601();
			return false;
		}
	}

	OutResult.bAccepted = true;
	OutResult.StepResults.Reserve(Plan.Steps.Num());
	bool bSawFailure = false;

	for (int32 StepIndex = 0; StepIndex < Plan.Steps.Num(); ++StepIndex)
	{
		const FHCIAbilityKitAgentPlanStep& Step = Plan.Steps[StepIndex];
		const FHCIAbilityKitToolDescriptor* Tool = ToolRegistry.FindTool(Step.ToolName);
		FHCIAbilityKitAgentExecutorStepResult& StepResult = HCI_AddStepResultSkeleton(Step, StepIndex, ToolRegistry, OutResult);
		StepResult.bAttempted = true;

		if (Tool == nullptr)
		{
			StepResult.bSucceeded = false;
			StepResult.Status = TEXT("failed");
			StepResult.ErrorCode = TEXT("E4002");
			StepResult.Reason = TEXT("tool_not_whitelisted");
			StepResult.FailurePhase = TEXT("execute");
		}
		else
		{
			const FHCIAbilityKitAgentExecutorPreflightDecision PreflightDecision =
				HCI_EvaluateExecutorPreflight(Plan, Step, StepResult, ToolRegistry, Options);
			if (!PreflightDecision.bAllowed)
			{
				StepResult.bSucceeded = false;
				StepResult.Status = TEXT("failed");
				StepResult.ErrorCode = PreflightDecision.ErrorCode;
				StepResult.Reason = PreflightDecision.Reason;
				StepResult.FailurePhase = TEXT("preflight");
				StepResult.PreflightGate = PreflightDecision.FailedGate.IsEmpty() ? TEXT("-") : PreflightDecision.FailedGate;
				OutResult.PreflightBlockedSteps += 1;
			}
			else if (Options.SimulatedFailureStepIndex == StepIndex)
			{
				StepResult.bSucceeded = false;
				StepResult.Status = TEXT("failed");
				StepResult.ErrorCode = Options.SimulatedFailureErrorCode.IsEmpty() ? TEXT("E4101") : Options.SimulatedFailureErrorCode;
				StepResult.Reason = Options.SimulatedFailureReason.IsEmpty() ? TEXT("simulated_tool_execution_failed") : Options.SimulatedFailureReason;
				StepResult.FailurePhase = TEXT("execute");
				StepResult.PreflightGate = Options.bEnablePreflightGates ? TEXT("passed") : TEXT("-");
			}
			else
			{
				for (const FString& EvidenceKey : Step.ExpectedEvidence)
				{
					StepResult.Evidence.Add(EvidenceKey, HCI_BuildEvidenceValue(EvidenceKey, Step));
				}

				StepResult.bSucceeded = true;
				StepResult.Status = TEXT("succeeded");
				StepResult.Reason = Options.bDryRun ? TEXT("simulated_dry_run_success") : TEXT("simulated_apply_success");
				StepResult.PreflightGate = Options.bEnablePreflightGates ? TEXT("passed") : TEXT("-");
			}
		}

		if (StepResult.bSucceeded)
		{
			OutResult.SucceededSteps += 1;
			continue;
		}

		bSawFailure = true;
		OutResult.FailedSteps += 1;
		if (OutResult.FailedStepIndex == INDEX_NONE)
		{
			OutResult.FailedStepIndex = StepIndex;
			OutResult.FailedStepId = Step.StepId;
			OutResult.FailedToolName = Step.ToolName.ToString();
			OutResult.FailedGate = StepResult.FailurePhase == TEXT("preflight") ? StepResult.PreflightGate : FString(TEXT("-"));
			OutResult.ErrorCode = StepResult.ErrorCode;
			OutResult.Reason = StepResult.Reason;
		}

		if (Options.TerminationPolicy == EHCIAbilityKitAgentExecutorTerminationPolicy::StopOnFirstFailure)
		{
			OutResult.ExecutedSteps = StepIndex + 1;
			OutResult.SkippedSteps = FMath::Max(0, Plan.Steps.Num() - OutResult.ExecutedSteps);

			for (int32 RemainingIndex = StepIndex + 1; RemainingIndex < Plan.Steps.Num(); ++RemainingIndex)
			{
				const FHCIAbilityKitAgentPlanStep& RemainingStep = Plan.Steps[RemainingIndex];
				FHCIAbilityKitAgentExecutorStepResult& SkippedRow =
					HCI_AddStepResultSkeleton(RemainingStep, RemainingIndex, ToolRegistry, OutResult);
				SkippedRow.bAttempted = false;
				SkippedRow.bSucceeded = false;
				SkippedRow.Status = TEXT("skipped");
				SkippedRow.Reason = TEXT("terminated_by_stop_on_first_failure");
			}

			OutResult.bCompleted = false;
			OutResult.TerminalStatus = TEXT("failed");
			OutResult.TerminalReason =
				(StepResult.FailurePhase == TEXT("preflight"))
					? TEXT("executor_preflight_gate_failed_stop_on_first_failure")
					: TEXT("executor_step_failed_stop_on_first_failure");
			OutResult.FinishedAtUtc = FHCIAbilityKitTimeFormat::FormatNowBeijingIso8601();
			return false;
		}
	}

	OutResult.ExecutedSteps = OutResult.TotalSteps;
	OutResult.SkippedSteps = 0;
	if (bSawFailure)
	{
		OutResult.bCompleted = false;
		OutResult.TerminalStatus = TEXT("completed_with_failures");
		OutResult.TerminalReason = (OutResult.PreflightBlockedSteps > 0)
									 ? TEXT("executor_preflight_gate_failed_continue_on_failure")
									 : TEXT("executor_step_failed_continue_on_failure");
		OutResult.FinishedAtUtc = FHCIAbilityKitTimeFormat::FormatNowBeijingIso8601();
		return false;
	}

	OutResult.bCompleted = true;
	OutResult.TerminalStatus = TEXT("completed");
	OutResult.TerminalReason = Options.bDryRun ? TEXT("executor_skeleton_simulated_dry_run_completed") : TEXT("executor_skeleton_simulated_apply_completed");
	OutResult.FinishedAtUtc = FHCIAbilityKitTimeFormat::FormatNowBeijingIso8601();
	return true;
}
