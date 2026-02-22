#include "Agent/HCIAbilityKitAgentExecutionGate.h"

namespace
{
static const TCHAR* const HCI_Error_ToolNotWhitelisted = TEXT("E4002");
static const TCHAR* const HCI_Error_WriteNotConfirmed = TEXT("E4005");
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

