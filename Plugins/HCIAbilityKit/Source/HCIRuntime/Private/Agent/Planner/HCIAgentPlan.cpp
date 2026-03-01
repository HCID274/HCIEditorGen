#include "Agent/Planner/HCIAgentPlan.h"

#include "Dom/JsonObject.h"

FString FHCIAgentPlanContract::RiskLevelToString(EHCIAgentPlanRiskLevel RiskLevel)
{
	switch (RiskLevel)
	{
	case EHCIAgentPlanRiskLevel::ReadOnly:
		return TEXT("read_only");
	case EHCIAgentPlanRiskLevel::Write:
		return TEXT("write");
	case EHCIAgentPlanRiskLevel::Destructive:
		return TEXT("destructive");
	default:
		return TEXT("read_only");
	}
}

bool FHCIAgentPlanContract::ValidateMinimalContract(const FHCIAgentPlan& Plan, FString& OutError)
{
	OutError.Reset();

	if (Plan.PlanVersion <= 0)
	{
		OutError = TEXT("plan_version must be >= 1");
		return false;
	}
	if (Plan.RequestId.IsEmpty())
	{
		OutError = TEXT("request_id is required");
		return false;
	}
	if (Plan.Intent.IsEmpty())
	{
		OutError = TEXT("intent is required");
		return false;
	}
	if (Plan.Steps.Num() <= 0)
	{
		const FString AssistantMessage = Plan.AssistantMessage.TrimStartAndEnd();
		if (AssistantMessage.IsEmpty())
		{
			OutError = TEXT("steps must not be empty when assistant_message is empty");
			return false;
		}
		return true;
	}

	for (int32 Index = 0; Index < Plan.Steps.Num(); ++Index)
	{
		const FHCIAgentPlanStep& Step = Plan.Steps[Index];
		if (Step.StepId.IsEmpty())
		{
			OutError = FString::Printf(TEXT("steps[%d].step_id is required"), Index);
			return false;
		}
		if (Step.ToolName.IsNone())
		{
			OutError = FString::Printf(TEXT("steps[%d].tool_name is required"), Index);
			return false;
		}
		if (!Step.Args.IsValid())
		{
			OutError = FString::Printf(TEXT("steps[%d].args is required"), Index);
			return false;
		}
		if (Step.RollbackStrategy.IsEmpty())
		{
			OutError = FString::Printf(TEXT("steps[%d].rollback_strategy is required"), Index);
			return false;
		}
		if (Step.ExpectedEvidence.Num() <= 0)
		{
			OutError = FString::Printf(TEXT("steps[%d].expected_evidence must not be empty"), Index);
			return false;
		}
	}

	return true;
}

