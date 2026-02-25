#pragma once

#include "CoreMinimal.h"

#include "Agent/Contracts/StageF/HCIAbilityKitAgentApplyRequest.h"

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentStageGExecuteCommitRequest
{
	FString RequestId;
	FString StageGExecuteDispatchReceiptId;
	FString StageGExecuteDispatchRequestId;
	FString StageGExecutePermitTicketId;
	FString StageGWriteEnableRequestId;
	FString StageGExecuteIntentId;
	FString SimHandoffEnvelopeId;
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
	FString ExecuteIntentDigest;
	FString StageGWriteEnableDigest;
	FString StageGExecutePermitDigest;
	FString StageGExecuteDispatchDigest;
	FString StageGExecuteDispatchReceiptDigest;
	FString StageGExecuteCommitRequestDigest;
	FString GeneratedUtc;
	FString ExecutionMode;
	FString ExecuteTarget;
	FString HandoffTarget;
	FString TransactionMode;
	FString TerminationPolicy;
	FString TerminalStatus;
	FString ArchiveStatus;
	FString HandoffStatus;
	FString StageGStatus;
	FString StageGWriteStatus;
	FString StageGExecutePermitStatus;
	FString StageGExecuteDispatchStatus;
	FString StageGExecuteDispatchReceiptStatus;
	FString StageGExecuteCommitRequestStatus;
	bool bUserConfirmed = false;
	bool bReadyToSimulateExecute = false;
	bool bSimulatedDispatchAccepted = false;
	bool bSimulationCompleted = false;
	bool bArchiveReady = false;
	bool bHandoffReady = false;
	bool bWriteEnabled = false;
	bool bReadyForStageGEntry = false;
	bool bWriteEnableConfirmed = false;
	bool bReadyForStageGExecute = false;
	bool bStageGExecutePermitReady = false;
	bool bExecuteDispatchConfirmed = false;
	bool bStageGExecuteDispatchReady = false;
	bool bStageGExecuteDispatchAccepted = false;
	bool bStageGExecuteDispatchReceiptReady = false;
	bool bExecuteCommitConfirmed = false;
	bool bStageGExecuteCommitRequestReady = false;
	FString ErrorCode;
	FString Reason;
	FHCIAbilityKitAgentApplyRequestSummary Summary;
	TArray<FHCIAbilityKitAgentApplyRequestItem> Items;
};



