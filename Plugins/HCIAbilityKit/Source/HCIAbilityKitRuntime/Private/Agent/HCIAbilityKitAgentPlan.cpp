#include "Agent/HCIAbilityKitAgentPlan.h"

#include "Dom/JsonObject.h"

FString FHCIAbilityKitAgentPlanContract::RiskLevelToString(EHCIAbilityKitAgentPlanRiskLevel RiskLevel)
{
	switch (RiskLevel)
	{
	case EHCIAbilityKitAgentPlanRiskLevel::ReadOnly:
		return TEXT("read_only");
	case EHCIAbilityKitAgentPlanRiskLevel::Write:
		return TEXT("write");
	case EHCIAbilityKitAgentPlanRiskLevel::Destructive:
		return TEXT("destructive");
	default:
		return TEXT("read_only");
	}
}

bool FHCIAbilityKitAgentPlanContract::ValidateMinimalContract(const FHCIAbilityKitAgentPlan& Plan, FString& OutError)
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
		OutError = TEXT("steps must not be empty");
		return false;
	}

	for (int32 Index = 0; Index < Plan.Steps.Num(); ++Index)
	{
		const FHCIAbilityKitAgentPlanStep& Step = Plan.Steps[Index];
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
	}

	return true;
}

