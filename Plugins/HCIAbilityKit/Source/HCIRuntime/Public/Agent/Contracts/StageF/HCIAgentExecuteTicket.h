#pragma once

#include "CoreMinimal.h"

#include "Agent/Contracts/StageF/HCIAgentApplyRequest.h"

struct HCIRUNTIME_API FHCIAgentExecuteTicket
{
	FString RequestId;
	FString ConfirmRequestId;
	FString ApplyRequestId;
	FString ReviewRequestId;
	FString SelectionDigest;
	FString GeneratedUtc;
	FString ExecutionMode;
	FString TransactionMode;
	FString TerminationPolicy;
	bool bUserConfirmed = false;
	bool bReadyToSimulateExecute = false;
	FString ErrorCode;
	FString Reason;
	FHCIAgentApplyRequestSummary Summary;
	TArray<FHCIAgentApplyRequestItem> Items;
};


