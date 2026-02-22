#pragma once

#include "CoreMinimal.h"

#include "Agent/HCIAbilityKitAgentPlan.h"
#include "Agent/HCIAbilityKitToolRegistry.h"

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentPlanValidationContext
{
	// F2 mock seam: caller can mark asset paths as metadata-insufficient to trigger E4012 without touching UE assets.
	TSet<FString> MockMetadataUnavailableAssetPaths;
};

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentPlanValidationResult
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

class HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentPlanValidator
{
public:
	static bool ValidatePlan(
		const FHCIAbilityKitAgentPlan& Plan,
		const FHCIAbilityKitToolRegistry& ToolRegistry,
		FHCIAbilityKitAgentPlanValidationResult& OutResult);

	static bool ValidatePlan(
		const FHCIAbilityKitAgentPlan& Plan,
		const FHCIAbilityKitToolRegistry& ToolRegistry,
		const FHCIAbilityKitAgentPlanValidationContext& Context,
		FHCIAbilityKitAgentPlanValidationResult& OutResult);
};

