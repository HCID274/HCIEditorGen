#pragma once

#include "CoreMinimal.h"

#include "Agent/HCIAbilityKitAgentApplyRequest.h"

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentSimulateExecuteArchiveBundle
{
	FString RequestId;
	FString SimFinalReportId;
	FString SimExecuteReceiptId;
	FString ExecuteTicketId;
	FString ConfirmRequestId;
	FString ApplyRequestId;
	FString ReviewRequestId;
	FString SelectionDigest;
	FString ArchiveDigest;
	FString GeneratedUtc;
	FString ExecutionMode;
	FString TransactionMode;
	FString TerminationPolicy;
	FString TerminalStatus;
	FString ArchiveStatus;
	bool bUserConfirmed = false;
	bool bReadyToSimulateExecute = false;
	bool bSimulatedDispatchAccepted = false;
	bool bSimulationCompleted = false;
	bool bArchiveReady = false;
	FString ErrorCode;
	FString Reason;
	FHCIAbilityKitAgentApplyRequestSummary Summary;
	TArray<FHCIAbilityKitAgentApplyRequestItem> Items;
};
