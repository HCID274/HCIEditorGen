#pragma once

#include "CoreMinimal.h"

#include "Agent/Contracts/StageF/HCIAbilityKitAgentApplyRequest.h"

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentStageGExecuteIntent
{
	FString RequestId;
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
	bool bUserConfirmed = false;
	bool bReadyToSimulateExecute = false;
	bool bSimulatedDispatchAccepted = false;
	bool bSimulationCompleted = false;
	bool bArchiveReady = false;
	bool bHandoffReady = false;
	bool bWriteEnabled = false;
	bool bReadyForStageGEntry = false;
	FString ErrorCode;
	FString Reason;
	FHCIAbilityKitAgentApplyRequestSummary Summary;
	TArray<FHCIAbilityKitAgentApplyRequestItem> Items;
};

