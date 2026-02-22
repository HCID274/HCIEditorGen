#include "Agent/HCIAbilityKitAgentExecutionGate.h"

namespace
{
static const TCHAR* const HCI_Error_ToolNotWhitelisted = TEXT("E4002");
static const TCHAR* const HCI_Error_BlastRadiusExceeded = TEXT("E4004");
static const TCHAR* const HCI_Error_WriteNotConfirmed = TEXT("E4005");
static const TCHAR* const HCI_Error_TransactionRolledBack = TEXT("E4007");
}

bool FHCIAbilityKitAgentExecutionGate::IsWriteLikeCapability(const EHCIAbilityKitToolCapability Capability)
{
	return Capability == EHCIAbilityKitToolCapability::Write || Capability == EHCIAbilityKitToolCapability::Destructive;
}

FHCIAbilityKitAgentExecutionDecision FHCIAbilityKitAgentExecutionGate::EvaluateConfirmGate(
	const FHCIAbilityKitAgentExecutionStep& Step,
	const FHCIAbilityKitToolRegistry& Registry)
{
	FHCIAbilityKitAgentExecutionDecision Decision;
	Decision.bAllowed = false;
	Decision.StepId = Step.StepId;
	Decision.ToolName = Step.ToolName.ToString();
	Decision.bRequiresConfirm = Step.bRequiresConfirm;
	Decision.bUserConfirmed = Step.bUserConfirmed;

	const FHCIAbilityKitToolDescriptor* Tool = Registry.FindTool(Step.ToolName);
	if (!Tool)
	{
		Decision.ErrorCode = HCI_Error_ToolNotWhitelisted;
		Decision.Reason = TEXT("tool_not_whitelisted");
		Decision.Capability = TEXT("unknown");
		Decision.bWriteLike = true;
		return Decision;
	}

	Decision.Capability = FHCIAbilityKitToolRegistry::CapabilityToString(Tool->Capability);
	Decision.bWriteLike = IsWriteLikeCapability(Tool->Capability);

	if (!Decision.bWriteLike)
	{
		Decision.bAllowed = true;
		return Decision;
	}

	if (!Step.bRequiresConfirm)
	{
		Decision.ErrorCode = HCI_Error_WriteNotConfirmed;
		Decision.Reason = TEXT("write_step_requires_confirm");
		return Decision;
	}

	if (!Step.bUserConfirmed)
	{
		Decision.ErrorCode = HCI_Error_WriteNotConfirmed;
		Decision.Reason = TEXT("user_not_confirmed");
		return Decision;
	}

	Decision.bAllowed = true;
	return Decision;
}

FHCIAbilityKitAgentBlastRadiusDecision FHCIAbilityKitAgentExecutionGate::EvaluateBlastRadius(
	const FHCIAbilityKitAgentBlastRadiusCheckInput& Input,
	const FHCIAbilityKitToolRegistry& Registry)
{
	FHCIAbilityKitAgentBlastRadiusDecision Decision;
	Decision.bAllowed = false;
	Decision.RequestId = Input.RequestId;
	Decision.ToolName = Input.ToolName.ToString();
	Decision.TargetModifyCount = Input.TargetModifyCount;
	Decision.MaxAssetModifyLimit = MaxAssetModifyLimit;

	const FHCIAbilityKitToolDescriptor* Tool = Registry.FindTool(Input.ToolName);
	if (!Tool)
	{
		Decision.ErrorCode = HCI_Error_ToolNotWhitelisted;
		Decision.Reason = TEXT("tool_not_whitelisted");
		Decision.Capability = TEXT("unknown");
		Decision.bWriteLike = true;
		return Decision;
	}

	Decision.Capability = FHCIAbilityKitToolRegistry::CapabilityToString(Tool->Capability);
	Decision.bWriteLike = IsWriteLikeCapability(Tool->Capability);

	if (!Decision.bWriteLike)
	{
		Decision.bAllowed = true;
		return Decision;
	}

	if (Input.TargetModifyCount > MaxAssetModifyLimit)
	{
		Decision.ErrorCode = HCI_Error_BlastRadiusExceeded;
		Decision.Reason = TEXT("modify_limit_exceeded");
		return Decision;
	}

	Decision.bAllowed = true;
	return Decision;
}

FHCIAbilityKitAgentTransactionExecutionDecision FHCIAbilityKitAgentExecutionGate::EvaluateAllOrNothingTransaction(
	const FHCIAbilityKitAgentTransactionExecutionInput& Input,
	const FHCIAbilityKitToolRegistry& Registry)
{
	FHCIAbilityKitAgentTransactionExecutionDecision Decision;
	Decision.RequestId = Input.RequestId;
	Decision.TransactionMode = TEXT("all_or_nothing");
	Decision.TotalSteps = Input.Steps.Num();

	// Preflight whitelist validation happens before any write is attempted.
	for (int32 Index = 0; Index < Input.Steps.Num(); ++Index)
	{
		const FHCIAbilityKitAgentTransactionStepSimulation& Step = Input.Steps[Index];
		if (Registry.FindTool(Step.ToolName) == nullptr)
		{
			Decision.ErrorCode = HCI_Error_ToolNotWhitelisted;
			Decision.Reason = TEXT("tool_not_whitelisted");
			Decision.FailedStepIndex = Index + 1; // 1-based for logs/UI readability
			Decision.FailedStepId = Step.StepId;
			Decision.FailedToolName = Step.ToolName.ToString();
			return Decision;
		}
	}

	int32 SuccessfulExecutedSteps = 0;
	for (int32 Index = 0; Index < Input.Steps.Num(); ++Index)
	{
		const FHCIAbilityKitAgentTransactionStepSimulation& Step = Input.Steps[Index];
		Decision.ExecutedSteps = Index + 1;

		if (!Step.bShouldSucceed)
		{
			Decision.bCommitted = false;
			Decision.bRolledBack = true;
			Decision.ErrorCode = HCI_Error_TransactionRolledBack;
			Decision.Reason = TEXT("step_failed_all_or_nothing_rollback");
			Decision.CommittedSteps = 0;
			Decision.RolledBackSteps = SuccessfulExecutedSteps;
			Decision.FailedStepIndex = Index + 1; // 1-based for logs/UI readability
			Decision.FailedStepId = Step.StepId;
			Decision.FailedToolName = Step.ToolName.ToString();
			return Decision;
		}

		++SuccessfulExecutedSteps;
	}

	Decision.bCommitted = true;
	Decision.bRolledBack = false;
	Decision.CommittedSteps = SuccessfulExecutedSteps;
	Decision.RolledBackSteps = 0;
	Decision.FailedStepIndex = -1;
	return Decision;
}
