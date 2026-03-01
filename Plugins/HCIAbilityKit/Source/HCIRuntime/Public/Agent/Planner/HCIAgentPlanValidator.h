#pragma once

#include "CoreMinimal.h"

#include "Agent/Planner/HCIAgentPlan.h"
#include "Agent/Tools/HCIToolRegistry.h"

struct HCIRUNTIME_API FHCIAgentPlanValidationContext
{
	// F2 mock seam: caller can mark asset paths as metadata-insufficient to trigger E4012 without touching UE assets.
	TSet<FString> MockMetadataUnavailableAssetPaths;

	// PlanReady hard gate: if true, modify intents must include at least one write step.
	bool bRequireWriteStepForModifyIntent = false;

	// PlanReady hard gate: if true, args marked as pipeline-required must bind variable templates.
	bool bRequirePipelineInputs = false;
};

struct HCIRUNTIME_API FHCIAgentPlanValidationResult
{
	bool bValid = false;
	FString ErrorCode;
	FString Field;
	FString Reason;

	FString RequestId;
	FString Intent;
	int32 PlanVersion = 0;
	int32 StepCount = 0;
	int32 ValidatedStepCount = 0;
	int32 WriteLikeStepCount = 0;
	int32 TotalTargetModifyCount = 0;
	FString MaxRiskLevel;

	int32 FailedStepIndex = INDEX_NONE; // 0-based
	FString FailedStepId;
	FString FailedToolName;
};

class HCIRUNTIME_API FHCIAgentPlanValidator
{
public:
	static bool ValidatePlan(
		const FHCIAgentPlan& Plan,
		const FHCIToolRegistry& ToolRegistry,
		FHCIAgentPlanValidationResult& OutResult);

	static bool ValidatePlan(
		const FHCIAgentPlan& Plan,
		const FHCIToolRegistry& ToolRegistry,
		const FHCIAgentPlanValidationContext& Context,
		FHCIAgentPlanValidationResult& OutResult);
};


