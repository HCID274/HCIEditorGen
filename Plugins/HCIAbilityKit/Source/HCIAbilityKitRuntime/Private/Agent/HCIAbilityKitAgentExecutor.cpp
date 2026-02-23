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

	for (int32 StepIndex = 0; StepIndex < Plan.Steps.Num(); ++StepIndex)
	{
		const FHCIAbilityKitAgentPlanStep& Step = Plan.Steps[StepIndex];
		const FHCIAbilityKitToolDescriptor* Tool = ToolRegistry.FindTool(Step.ToolName);

		FHCIAbilityKitAgentExecutorStepResult& StepResult = OutResult.StepResults.AddDefaulted_GetRef();
		StepResult.StepIndex = StepIndex;
		StepResult.StepId = Step.StepId;
		StepResult.ToolName = Step.ToolName.ToString();
		StepResult.Capability = HCI_GetCapabilityString(Tool);
		StepResult.RiskLevel = FHCIAbilityKitAgentPlanContract::RiskLevelToString(Step.RiskLevel);
		StepResult.bWriteLike = HCI_IsWriteLike(Tool);
		StepResult.bAttempted = true;
		StepResult.TargetCountEstimate = HCI_GetTargetCountEstimate(Step);
		StepResult.EvidenceKeys = Step.ExpectedEvidence;

		if (Tool == nullptr)
		{
			StepResult.bSucceeded = false;
			StepResult.Status = TEXT("failed");
			StepResult.ErrorCode = TEXT("E4002");
			StepResult.Reason = TEXT("tool_not_whitelisted");

			OutResult.FailedSteps = 1;
			OutResult.FailedStepIndex = StepIndex;
			OutResult.FailedStepId = Step.StepId;
			OutResult.FailedToolName = Step.ToolName.ToString();
			OutResult.ErrorCode = StepResult.ErrorCode;
			OutResult.Reason = StepResult.Reason;
			OutResult.TerminalStatus = TEXT("failed");
			OutResult.TerminalReason = TEXT("executor_tool_not_whitelisted");
			OutResult.ExecutedSteps = StepIndex + 1;
			OutResult.SucceededSteps = StepIndex;
			OutResult.bCompleted = false;
			OutResult.FinishedAtUtc = FHCIAbilityKitTimeFormat::FormatNowBeijingIso8601();
			return false;
		}

		for (const FString& EvidenceKey : Step.ExpectedEvidence)
		{
			StepResult.Evidence.Add(EvidenceKey, HCI_BuildEvidenceValue(EvidenceKey, Step));
		}

		StepResult.bSucceeded = true;
		StepResult.Status = TEXT("succeeded");
		StepResult.Reason = Options.bDryRun ? TEXT("simulated_dry_run_success") : TEXT("simulated_apply_success");
	}

	OutResult.ExecutedSteps = OutResult.TotalSteps;
	OutResult.SucceededSteps = OutResult.TotalSteps;
	OutResult.FailedSteps = 0;
	OutResult.bCompleted = true;
	OutResult.TerminalStatus = TEXT("completed");
	OutResult.TerminalReason = Options.bDryRun ? TEXT("executor_skeleton_simulated_dry_run_completed") : TEXT("executor_skeleton_simulated_apply_completed");
	OutResult.FinishedAtUtc = FHCIAbilityKitTimeFormat::FormatNowBeijingIso8601();
	return true;
}
