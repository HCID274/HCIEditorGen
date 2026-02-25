#pragma once

#include "CoreMinimal.h"

#include "Agent/Contracts/StageF/HCIAbilityKitAgentApplyRequest.h"

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentSimulateExecuteReceipt
{
	FString RequestId;
	FString ExecuteTicketId;
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
	bool bSimulatedDispatchAccepted = false;
	FString ErrorCode;
	FString Reason;
	FHCIAbilityKitAgentApplyRequestSummary Summary;
	TArray<FHCIAbilityKitAgentApplyRequestItem> Items;
};