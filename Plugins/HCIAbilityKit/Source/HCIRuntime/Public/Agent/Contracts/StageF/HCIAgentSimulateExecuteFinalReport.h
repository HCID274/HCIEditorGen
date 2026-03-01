#pragma once

#include "CoreMinimal.h"

#include "Agent/Contracts/StageF/HCIAgentApplyRequest.h"

struct HCIRUNTIME_API FHCIAgentSimulateExecuteFinalReport
{
	FString RequestId;
	FString SimExecuteReceiptId;
	FString ExecuteTicketId;
	FString ConfirmRequestId;
	FString ApplyRequestId;
	FString ReviewRequestId;
	FString SelectionDigest;
	FString GeneratedUtc;
	FString ExecutionMode;
	FString TransactionMode;
	FString TerminationPolicy;
	FString TerminalStatus;
	bool bUserConfirmed = false;
	bool bReadyToSimulateExecute = false;
	bool bSimulatedDispatchAccepted = false;
	bool bSimulationCompleted = false;
	FString ErrorCode;
	FString Reason;
	FHCIAgentApplyRequestSummary Summary;
	TArray<FHCIAgentApplyRequestItem> Items;
};

