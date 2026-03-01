#pragma once

#include "CoreMinimal.h"

#include "Agent/Contracts/StageF/HCIAgentApplyRequest.h"

struct HCIRUNTIME_API FHCIAgentStageGExecutionReadinessReport
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
	FHCIAgentApplyRequestSummary Summary;
	TArray<FHCIAgentApplyRequestItem> Items;
};


