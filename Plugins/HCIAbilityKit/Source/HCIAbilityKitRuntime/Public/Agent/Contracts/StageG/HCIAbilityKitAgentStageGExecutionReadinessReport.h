#pragma once

#include "CoreMinimal.h"

#include "Agent/Contracts/StageF/HCIAbilityKitAgentApplyRequest.h"

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentStageGExecutionReadinessReport
{
	FString RequestId;
	FString StageGExecuteArchiveBundleId;
	FString SelectionDigest;
	FString StageGExecutionReadinessDigest;
	FString StageGExecutionReadinessStatus;
	bool bReadyForH1PlannerIntegration = false;
	FString ExecutionMode;
	FString ErrorCode;
	FString Reason;
	FHCIAbilityKitAgentApplyRequestSummary Summary;
	TArray<FHCIAbilityKitAgentApplyRequestItem> Items;
};
