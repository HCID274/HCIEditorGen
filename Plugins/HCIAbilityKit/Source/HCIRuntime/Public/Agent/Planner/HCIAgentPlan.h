#pragma once

#include "CoreMinimal.h"

class FJsonObject;

enum class EHCIAgentPlanRiskLevel : uint8
{
	ReadOnly,
	Write,
	Destructive
};

struct HCIRUNTIME_API FHCIAgentPlanStep
{
	struct FUiPresentation
	{
		bool bHasStepSummary = false;
		FString StepSummary;
		bool bHasIntentReason = false;
		FString IntentReason;
		bool bHasRiskWarning = false;
		FString RiskWarning;

		bool HasAnyField() const
		{
			return bHasStepSummary || bHasIntentReason || bHasRiskWarning;
		}
	};

	FString StepId;
	FName ToolName;
	TSharedPtr<FJsonObject> Args;
	EHCIAgentPlanRiskLevel RiskLevel = EHCIAgentPlanRiskLevel::ReadOnly;
	bool bRequiresConfirm = false;
	FString RollbackStrategy = TEXT("all_or_nothing");
	TArray<FString> ExpectedEvidence;
	FUiPresentation UiPresentation;
};

struct HCIRUNTIME_API FHCIAgentPlan
{
	int32 PlanVersion = 1;
	FString RequestId;
	FString Intent;
	FString AssistantMessage;
	TArray<FHCIAgentPlanStep> Steps;
};

class HCIRUNTIME_API FHCIAgentPlanContract
{
public:
	static FString RiskLevelToString(EHCIAgentPlanRiskLevel RiskLevel);
	static bool ValidateMinimalContract(const FHCIAgentPlan& Plan, FString& OutError);
};


