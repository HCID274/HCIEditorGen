#pragma once

#include "CoreMinimal.h"

class FJsonObject;

enum class EHCIAbilityKitAgentPlanRiskLevel : uint8
{
	ReadOnly,
	Write,
	Destructive
};

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentPlanStep
{
	FString StepId;
	FName ToolName;
	TSharedPtr<FJsonObject> Args;
	EHCIAbilityKitAgentPlanRiskLevel RiskLevel = EHCIAbilityKitAgentPlanRiskLevel::ReadOnly;
	bool bRequiresConfirm = false;
	FString RollbackStrategy = TEXT("all_or_nothing");
	TArray<FString> ExpectedEvidence;
};

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentPlan
{
	int32 PlanVersion = 1;
	FString RequestId;
	FString Intent;
	TArray<FHCIAbilityKitAgentPlanStep> Steps;
};

class HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentPlanContract
{
public:
	static FString RiskLevelToString(EHCIAbilityKitAgentPlanRiskLevel RiskLevel);
	static bool ValidateMinimalContract(const FHCIAbilityKitAgentPlan& Plan, FString& OutError);
};

