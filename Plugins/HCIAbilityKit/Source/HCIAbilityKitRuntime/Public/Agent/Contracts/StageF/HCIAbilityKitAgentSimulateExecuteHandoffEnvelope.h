#pragma once

#include "CoreMinimal.h"

#include "Agent/Contracts/StageF/HCIAbilityKitAgentApplyRequest.h"

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope
{
	FString RequestId;
	FString SimArchiveBundleId;
	FString SimFinalReportId;
	FString SimExecuteReceiptId;
	FString ExecuteTicketId;
	FString ConfirmRequestId;
	FString ApplyRequestId;
	FString ReviewRequestId;
	FString SelectionDigest;
	FString ArchiveDigest;
	FString HandoffDigest;
	FString GeneratedUtc;
	FString ExecutionMode;
	FString HandoffTarget;
	FString TransactionMode;
	FString TerminationPolicy;
	FString TerminalStatus;
	FString ArchiveStatus;
	FString HandoffStatus;
	bool bUserConfirmed = false;
	bool bReadyToSimulateExecute = false;
	bool bSimulatedDispatchAccepted = false;
	bool bSimulationCompleted = false;
	bool bArchiveReady = false;
	bool bHandoffReady = false;
	FString ErrorCode;
	FString Reason;
	FHCIAbilityKitAgentApplyRequestSummary Summary;
	TArray<FHCIAbilityKitAgentApplyRequestItem> Items;
};
