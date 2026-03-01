#include "Agent/Bridges/HCIAgentExecutorStageGExecuteCommitReceiptBridge.h"

#include "Agent/Contracts/StageF/HCIAgentApplyConfirmRequest.h"
#include "Agent/Contracts/StageF/HCIAgentApplyRequest.h"
#include "Agent/Contracts/StageF/HCIAgentExecuteTicket.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteArchiveBundle.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteFinalReport.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteHandoffEnvelope.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteReceipt.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecuteCommitReceipt.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecuteCommitRequest.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecuteDispatchRequest.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecuteDispatchReceipt.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecuteIntent.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecutePermitTicket.h"
#include "Agent/Contracts/StageG/HCIAgentStageGWriteEnableRequest.h"
#include "Agent/Executor/HCIDryRunDiff.h"
#include "Common/HCITimeFormat.h"
#include "Misc/Crc.h"
#include "Misc/Guid.h"

namespace
{
static FString HCI_BuildSelectionDigestFromReviewReport_G7(const FHCIDryRunDiffReport& Report)
{
	FString Canonical;
	Canonical.Reserve(Report.DiffItems.Num() * 96);
	for (int32 RowIndex = 0; RowIndex < Report.DiffItems.Num(); ++RowIndex)
	{
		const FHCIDryRunDiffItem& Item = Report.DiffItems[RowIndex];
		Canonical += FString::Printf(
			TEXT("%d|%s|%s|%s|%s|%s|%s|%s\n"),
			RowIndex,
			*Item.ToolName,
			*Item.AssetPath,
			*Item.Field,
			*FHCIDryRunDiff::RiskToString(Item.Risk),
			*Item.SkipReason,
			*FHCIDryRunDiff::ObjectTypeToString(Item.ObjectType),
			*FHCIDryRunDiff::LocateStrategyToString(Item.LocateStrategy));
	}
	return FString::Printf(TEXT("crc32_%08X"), FCrc::StrCrc32(*Canonical));
}

static FString HCI_BuildStageGExecuteCommitReceiptDigest_G7(const FHCIAgentStageGExecuteCommitReceipt& Receipt)
{
	FString Canonical;
	Canonical += FString::Printf(
		TEXT("%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s\n"),
		*Receipt.StageGExecuteCommitRequestId,
		*Receipt.StageGExecuteDispatchReceiptId,
		*Receipt.StageGExecuteDispatchRequestId,
		*Receipt.StageGExecutePermitTicketId,
		*Receipt.StageGWriteEnableRequestId,
		*Receipt.StageGExecuteIntentId,
		*Receipt.SimHandoffEnvelopeId,
		*Receipt.SimArchiveBundleId,
		*Receipt.SimFinalReportId,
		*Receipt.SimExecuteReceiptId,
		*Receipt.ExecuteTicketId,
		*Receipt.ConfirmRequestId,
		*Receipt.ApplyRequestId,
		*Receipt.ReviewRequestId,
		*Receipt.SelectionDigest,
		*Receipt.ArchiveDigest,
		*Receipt.HandoffDigest,
		*Receipt.ExecuteIntentDigest,
		*Receipt.StageGWriteEnableDigest,
		*Receipt.StageGExecutePermitDigest,
		*Receipt.StageGExecuteDispatchDigest,
		*Receipt.StageGExecuteDispatchReceiptDigest,
		*Receipt.StageGExecuteCommitRequestDigest,
		*Receipt.TerminalStatus,
		*Receipt.ArchiveStatus,
		*Receipt.HandoffStatus,
		*Receipt.StageGStatus,
		*Receipt.StageGWriteStatus,
		*Receipt.StageGExecuteCommitRequestStatus,
		*Receipt.StageGExecuteCommitReceiptStatus);

	Canonical += FString::Printf(
		TEXT("%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s\n"),
		Receipt.bUserConfirmed ? TEXT("1") : TEXT("0"),
		Receipt.bReadyToSimulateExecute ? TEXT("1") : TEXT("0"),
		Receipt.bSimulatedDispatchAccepted ? TEXT("1") : TEXT("0"),
		Receipt.bSimulationCompleted ? TEXT("1") : TEXT("0"),
		Receipt.bArchiveReady ? TEXT("1") : TEXT("0"),
		Receipt.bHandoffReady ? TEXT("1") : TEXT("0"),
		Receipt.bWriteEnabled ? TEXT("1") : TEXT("0"),
		Receipt.bReadyForStageGEntry ? TEXT("1") : TEXT("0"),
		Receipt.bWriteEnableConfirmed ? TEXT("1") : TEXT("0"),
		Receipt.bReadyForStageGExecute ? TEXT("1") : TEXT("0"),
		Receipt.bStageGExecutePermitReady ? TEXT("1") : TEXT("0"),
		Receipt.bExecuteDispatchConfirmed ? TEXT("1") : TEXT("0"),
		Receipt.bStageGExecuteDispatchReady ? TEXT("1") : TEXT("0"),
		Receipt.bStageGExecuteDispatchAccepted ? TEXT("1") : TEXT("0"),
		Receipt.bStageGExecuteDispatchReceiptReady ? TEXT("1") : TEXT("0"),
		Receipt.bExecuteCommitConfirmed ? TEXT("1") : TEXT("0"),
		Receipt.bStageGExecuteCommitRequestReady ? TEXT("1") : TEXT("0"),
		Receipt.bStageGExecuteCommitAccepted ? TEXT("1") : TEXT("0"),
		Receipt.bStageGExecuteCommitReceiptReady ? TEXT("1") : TEXT("0"));

	Canonical += FString::Printf(TEXT("%s|%s|%s\n"), *Receipt.ExecuteTarget, *Receipt.HandoffTarget, *Receipt.Reason);

	for (const FHCIAgentApplyRequestItem& Item : Receipt.Items)
	{
		Canonical += FString::Printf(
			TEXT("%d|%s|%s|%s|%s|%s|%s|%s|%s|%s\n"),
			Item.RowIndex,
			*Item.ToolName,
			*Item.AssetPath,
			*Item.Field,
			*Item.SkipReason,
			Item.bBlocked ? TEXT("1") : TEXT("0"),
			*FHCIDryRunDiff::RiskToString(Item.Risk),
			*FHCIDryRunDiff::ObjectTypeToString(Item.ObjectType),
			*FHCIDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
			*Item.EvidenceKey);
	}

	return FString::Printf(TEXT("crc32_%08X"), FCrc::StrCrc32(*Canonical));
}

static void HCI_CopyCommitRequestToCommitReceipt_G7(
	const FHCIAgentStageGExecuteCommitRequest& InRequest,
	FHCIAgentStageGExecuteCommitReceipt& OutReceipt)
{
	OutReceipt = FHCIAgentStageGExecuteCommitReceipt();
	OutReceipt.RequestId = FString::Printf(TEXT("stagegcommitrcpt_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	OutReceipt.StageGExecuteCommitRequestId = InRequest.RequestId;
	OutReceipt.StageGExecuteDispatchReceiptId = InRequest.StageGExecuteDispatchReceiptId;
	OutReceipt.StageGExecuteDispatchRequestId = InRequest.StageGExecuteDispatchRequestId;
	OutReceipt.StageGExecutePermitTicketId = InRequest.StageGExecutePermitTicketId;
	OutReceipt.StageGWriteEnableRequestId = InRequest.StageGWriteEnableRequestId;
	OutReceipt.StageGExecuteIntentId = InRequest.StageGExecuteIntentId;
	OutReceipt.SimHandoffEnvelopeId = InRequest.SimHandoffEnvelopeId;
	OutReceipt.SimArchiveBundleId = InRequest.SimArchiveBundleId;
	OutReceipt.SimFinalReportId = InRequest.SimFinalReportId;
	OutReceipt.SimExecuteReceiptId = InRequest.SimExecuteReceiptId;
	OutReceipt.ExecuteTicketId = InRequest.ExecuteTicketId;
	OutReceipt.ConfirmRequestId = InRequest.ConfirmRequestId;
	OutReceipt.ApplyRequestId = InRequest.ApplyRequestId;
	OutReceipt.ReviewRequestId = InRequest.ReviewRequestId;
	OutReceipt.SelectionDigest = InRequest.SelectionDigest;
	OutReceipt.ArchiveDigest = InRequest.ArchiveDigest;
	OutReceipt.HandoffDigest = InRequest.HandoffDigest;
	OutReceipt.ExecuteIntentDigest = InRequest.ExecuteIntentDigest;
	OutReceipt.StageGWriteEnableDigest = InRequest.StageGWriteEnableDigest;
	OutReceipt.StageGExecutePermitDigest = InRequest.StageGExecutePermitDigest;
	OutReceipt.StageGExecuteDispatchDigest = InRequest.StageGExecuteDispatchDigest;
	OutReceipt.StageGExecuteDispatchReceiptDigest = InRequest.StageGExecuteDispatchReceiptDigest;
	OutReceipt.StageGExecuteCommitRequestDigest = InRequest.StageGExecuteCommitRequestDigest;
	OutReceipt.GeneratedUtc = FHCITimeFormat::FormatNowBeijingIso8601();
	OutReceipt.ExecutionMode = TEXT("stage_g_execute_commit_receipt_dry_run");
	OutReceipt.ExecuteTarget = InRequest.ExecuteTarget;
	OutReceipt.HandoffTarget = InRequest.HandoffTarget;
	OutReceipt.TransactionMode = InRequest.TransactionMode;
	OutReceipt.TerminationPolicy = InRequest.TerminationPolicy;
	OutReceipt.TerminalStatus = InRequest.TerminalStatus;
	OutReceipt.ArchiveStatus = InRequest.ArchiveStatus;
	OutReceipt.HandoffStatus = InRequest.HandoffStatus;
	OutReceipt.StageGStatus = InRequest.StageGStatus;
	OutReceipt.StageGWriteStatus = InRequest.StageGWriteStatus;
	OutReceipt.StageGExecutePermitStatus = InRequest.StageGExecutePermitStatus;
	OutReceipt.StageGExecuteDispatchStatus = InRequest.StageGExecuteDispatchStatus;
	OutReceipt.StageGExecuteDispatchReceiptStatus = InRequest.StageGExecuteDispatchReceiptStatus;
	OutReceipt.StageGExecuteCommitRequestStatus = InRequest.StageGExecuteCommitRequestStatus;
	OutReceipt.StageGExecuteCommitReceiptStatus = TEXT("blocked");
	OutReceipt.bUserConfirmed = InRequest.bUserConfirmed;
	OutReceipt.bReadyToSimulateExecute = InRequest.bReadyToSimulateExecute;
	OutReceipt.bSimulatedDispatchAccepted = InRequest.bSimulatedDispatchAccepted;
	OutReceipt.bSimulationCompleted = InRequest.bSimulationCompleted;
	OutReceipt.bArchiveReady = InRequest.bArchiveReady;
	OutReceipt.bHandoffReady = InRequest.bHandoffReady;
	OutReceipt.bWriteEnabled = InRequest.bWriteEnabled;
	OutReceipt.bReadyForStageGEntry = InRequest.bReadyForStageGEntry;
	OutReceipt.bWriteEnableConfirmed = InRequest.bWriteEnableConfirmed;
	OutReceipt.bReadyForStageGExecute = InRequest.bReadyForStageGExecute;
	OutReceipt.bStageGExecutePermitReady = InRequest.bStageGExecutePermitReady;
	OutReceipt.bExecuteDispatchConfirmed = InRequest.bExecuteDispatchConfirmed;
	OutReceipt.bStageGExecuteDispatchReady = InRequest.bStageGExecuteDispatchReady;
	OutReceipt.bStageGExecuteDispatchAccepted = InRequest.bStageGExecuteDispatchAccepted;
	OutReceipt.bStageGExecuteDispatchReceiptReady = InRequest.bStageGExecuteDispatchReceiptReady;
	OutReceipt.bExecuteCommitConfirmed = InRequest.bExecuteCommitConfirmed;
	OutReceipt.bStageGExecuteCommitRequestReady = InRequest.bStageGExecuteCommitRequestReady;
	OutReceipt.bStageGExecuteCommitAccepted = false;
	OutReceipt.bStageGExecuteCommitReceiptReady = false;
	OutReceipt.ErrorCode = TEXT("-");
	OutReceipt.Reason = TEXT("stage_g_execute_commit_receipt_ready");
	OutReceipt.Summary = InRequest.Summary;
	OutReceipt.Items = InRequest.Items;
}
} // namespace

bool FHCIAgentExecutorStageGExecuteCommitReceiptBridge::BuildStageGExecuteCommitReceipt(
	const FHCIAgentStageGExecuteCommitRequest& StageGExecuteCommitRequest,
	const FString& ExpectedStageGExecuteCommitRequestId,
	const FHCIAgentStageGExecuteDispatchReceipt& StageGExecuteDispatchReceipt,
	const FHCIAgentStageGExecuteDispatchRequest& StageGExecuteDispatchRequest,
	const FHCIAgentStageGExecutePermitTicket& StageGExecutePermitTicket,
	const FHCIAgentStageGWriteEnableRequest& StageGWriteEnableRequest,
	const FHCIAgentStageGExecuteIntent& StageGExecuteIntent,
	const FHCIAgentSimulateExecuteHandoffEnvelope& SimHandoffEnvelope,
	const FHCIAgentSimulateExecuteArchiveBundle& SimArchiveBundle,
	const FHCIAgentSimulateExecuteFinalReport& SimFinalReport,
	const FHCIAgentSimulateExecuteReceipt& SimExecuteReceipt,
	const FHCIAgentExecuteTicket& CurrentExecuteTicket,
	const FHCIAgentApplyConfirmRequest& CurrentConfirmRequest,
	const FHCIAgentApplyRequest& CurrentApplyRequest,
	const FHCIDryRunDiffReport& CurrentReviewReport,
	FHCIAgentStageGExecuteCommitReceipt& OutReceipt)
{
	HCI_CopyCommitRequestToCommitReceipt_G7(StageGExecuteCommitRequest, OutReceipt);

	auto FinalizeAndReturn = [&OutReceipt]() -> bool
	{
		OutReceipt.StageGExecuteCommitReceiptDigest = HCI_BuildStageGExecuteCommitReceiptDigest_G7(OutReceipt);
		return true;
	};

	if (!ExpectedStageGExecuteCommitRequestId.IsEmpty() && StageGExecuteCommitRequest.RequestId != ExpectedStageGExecuteCommitRequestId)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("stage_g_execute_commit_request_id_mismatch");
		return FinalizeAndReturn();
	}

	if (!StageGExecuteCommitRequest.bUserConfirmed)
	{
		OutReceipt.ErrorCode = TEXT("E4005");
		OutReceipt.Reason = TEXT("user_not_confirmed");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteCommitRequest.bWriteEnableConfirmed)
	{
		OutReceipt.ErrorCode = TEXT("E4005");
		OutReceipt.Reason = TEXT("stage_g_write_enable_not_confirmed");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteCommitRequest.bExecuteDispatchConfirmed)
	{
		OutReceipt.ErrorCode = TEXT("E4005");
		OutReceipt.Reason = TEXT("stage_g_execute_dispatch_not_confirmed");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteCommitRequest.bExecuteCommitConfirmed)
	{
		OutReceipt.ErrorCode = TEXT("E4005");
		OutReceipt.Reason = TEXT("stage_g_execute_commit_not_confirmed");
		return FinalizeAndReturn();
	}

	if (!StageGExecuteCommitRequest.bStageGExecuteCommitRequestReady ||
		!StageGExecuteCommitRequest.bWriteEnabled ||
		!StageGExecuteCommitRequest.bReadyForStageGExecute ||
		!StageGExecuteCommitRequest.bStageGExecuteDispatchAccepted ||
		!StageGExecuteCommitRequest.bStageGExecuteDispatchReceiptReady ||
		!StageGExecuteCommitRequest.StageGExecuteCommitRequestStatus.Equals(TEXT("ready"), ESearchCase::IgnoreCase))
	{
		OutReceipt.ErrorCode = TEXT("E4215");
		OutReceipt.Reason = TEXT("stage_g_execute_commit_request_not_ready");
		return FinalizeAndReturn();
	}

	if (!StageGExecuteCommitRequest.bReadyForStageGEntry || !StageGExecuteCommitRequest.StageGStatus.Equals(TEXT("ready"), ESearchCase::IgnoreCase))
	{
		OutReceipt.ErrorCode = TEXT("E4210");
		OutReceipt.Reason = TEXT("stage_g_execute_intent_not_ready");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteCommitRequest.bReadyToSimulateExecute)
	{
		OutReceipt.ErrorCode = TEXT("E4205");
		OutReceipt.Reason = TEXT("execute_ticket_not_ready");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteCommitRequest.bSimulatedDispatchAccepted)
	{
		OutReceipt.ErrorCode = TEXT("E4206");
		OutReceipt.Reason = TEXT("simulate_execute_receipt_not_accepted");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteCommitRequest.bSimulationCompleted || !StageGExecuteCommitRequest.TerminalStatus.Equals(TEXT("completed"), ESearchCase::IgnoreCase))
	{
		OutReceipt.ErrorCode = TEXT("E4207");
		OutReceipt.Reason = TEXT("simulate_final_report_not_completed");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteCommitRequest.bArchiveReady || !StageGExecuteCommitRequest.ArchiveStatus.Equals(TEXT("ready"), ESearchCase::IgnoreCase))
	{
		OutReceipt.ErrorCode = TEXT("E4208");
		OutReceipt.Reason = TEXT("sim_archive_bundle_not_ready");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteCommitRequest.bHandoffReady || !StageGExecuteCommitRequest.HandoffStatus.Equals(TEXT("ready"), ESearchCase::IgnoreCase))
	{
		OutReceipt.ErrorCode = TEXT("E4209");
		OutReceipt.Reason = TEXT("sim_handoff_envelope_not_ready");
		return FinalizeAndReturn();
	}
	if (!CurrentConfirmRequest.bReadyToExecute)
	{
		OutReceipt.ErrorCode = TEXT("E4204");
		OutReceipt.Reason = TEXT("confirm_request_not_ready");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteDispatchReceipt.bStageGExecuteDispatchReceiptReady || !StageGExecuteDispatchReceipt.bStageGExecuteDispatchAccepted)
	{
		OutReceipt.ErrorCode = TEXT("E4214");
		OutReceipt.Reason = TEXT("stage_g_execute_dispatch_receipt_not_ready");
		return FinalizeAndReturn();
	}

	if (StageGExecuteCommitRequest.StageGExecuteDispatchReceiptId != StageGExecuteDispatchReceipt.RequestId)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("stage_g_execute_dispatch_receipt_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteCommitRequest.StageGExecuteDispatchRequestId != StageGExecuteDispatchRequest.RequestId)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("stage_g_execute_dispatch_request_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteCommitRequest.StageGExecutePermitTicketId != StageGExecutePermitTicket.RequestId)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("stage_g_execute_permit_ticket_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteCommitRequest.StageGWriteEnableRequestId != StageGWriteEnableRequest.RequestId)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("stage_g_write_enable_request_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteCommitRequest.StageGExecuteIntentId != StageGExecuteIntent.RequestId)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("stage_g_execute_intent_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteCommitRequest.SimHandoffEnvelopeId != SimHandoffEnvelope.RequestId)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("sim_handoff_envelope_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteCommitRequest.SimArchiveBundleId != SimArchiveBundle.RequestId)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("sim_archive_bundle_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteCommitRequest.SimFinalReportId != SimFinalReport.RequestId)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("sim_final_report_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteCommitRequest.SimExecuteReceiptId != SimExecuteReceipt.RequestId)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("sim_execute_receipt_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteCommitRequest.ExecuteTicketId != CurrentExecuteTicket.RequestId)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("execute_ticket_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteCommitRequest.ConfirmRequestId != CurrentConfirmRequest.RequestId)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("confirm_request_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteCommitRequest.ApplyRequestId != CurrentApplyRequest.RequestId)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("apply_request_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteCommitRequest.ReviewRequestId != CurrentReviewReport.RequestId)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("review_request_id_mismatch");
		return FinalizeAndReturn();
	}

	if (StageGExecuteCommitRequest.SelectionDigest != StageGExecuteDispatchReceipt.SelectionDigest ||
		StageGExecuteCommitRequest.SelectionDigest != StageGWriteEnableRequest.SelectionDigest ||
		StageGExecuteCommitRequest.SelectionDigest != StageGExecuteIntent.SelectionDigest ||
		StageGExecuteCommitRequest.SelectionDigest != SimHandoffEnvelope.SelectionDigest ||
		StageGExecuteCommitRequest.SelectionDigest != SimArchiveBundle.SelectionDigest ||
		StageGExecuteCommitRequest.SelectionDigest != SimFinalReport.SelectionDigest ||
		StageGExecuteCommitRequest.SelectionDigest != SimExecuteReceipt.SelectionDigest ||
		StageGExecuteCommitRequest.SelectionDigest != CurrentExecuteTicket.SelectionDigest ||
		StageGExecuteCommitRequest.SelectionDigest != CurrentConfirmRequest.SelectionDigest ||
		StageGExecuteCommitRequest.SelectionDigest != CurrentApplyRequest.SelectionDigest)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("selection_digest_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteCommitRequest.SelectionDigest != HCI_BuildSelectionDigestFromReviewReport_G7(CurrentReviewReport))
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("selection_digest_mismatch");
		return FinalizeAndReturn();
	}

	if (StageGExecuteCommitRequest.ArchiveDigest.IsEmpty() || StageGExecuteCommitRequest.ArchiveDigest != SimArchiveBundle.ArchiveDigest)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("archive_digest_mismatch_or_missing");
		return FinalizeAndReturn();
	}
	if (StageGExecuteCommitRequest.HandoffDigest.IsEmpty() || StageGExecuteCommitRequest.HandoffDigest != SimHandoffEnvelope.HandoffDigest)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("handoff_digest_mismatch_or_missing");
		return FinalizeAndReturn();
	}
	if (StageGExecuteCommitRequest.ExecuteIntentDigest.IsEmpty() || StageGExecuteCommitRequest.ExecuteIntentDigest != StageGExecuteIntent.ExecuteIntentDigest)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("execute_intent_digest_mismatch_or_missing");
		return FinalizeAndReturn();
	}
	if (StageGExecuteCommitRequest.StageGWriteEnableDigest.IsEmpty() || StageGExecuteCommitRequest.StageGWriteEnableDigest != StageGWriteEnableRequest.StageGWriteEnableDigest)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("stage_g_write_enable_digest_mismatch_or_missing");
		return FinalizeAndReturn();
	}
	if (StageGExecuteCommitRequest.StageGExecutePermitDigest.IsEmpty() || StageGExecuteCommitRequest.StageGExecutePermitDigest != StageGExecutePermitTicket.StageGExecutePermitDigest)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("stage_g_execute_permit_digest_mismatch_or_missing");
		return FinalizeAndReturn();
	}
	if (StageGExecuteCommitRequest.StageGExecuteDispatchDigest.IsEmpty() || StageGExecuteCommitRequest.StageGExecuteDispatchDigest != StageGExecuteDispatchRequest.StageGExecuteDispatchDigest)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("stage_g_execute_dispatch_digest_mismatch_or_missing");
		return FinalizeAndReturn();
	}
	if (StageGExecuteCommitRequest.StageGExecuteDispatchReceiptDigest.IsEmpty() || StageGExecuteCommitRequest.StageGExecuteDispatchReceiptDigest != StageGExecuteDispatchReceipt.StageGExecuteDispatchReceiptDigest)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("stage_g_execute_dispatch_receipt_digest_mismatch_or_missing");
		return FinalizeAndReturn();
	}
	if (StageGExecuteCommitRequest.StageGExecuteCommitRequestDigest.IsEmpty())
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("stage_g_execute_commit_request_digest_missing");
		return FinalizeAndReturn();
	}

	if (!StageGExecuteCommitRequest.ExecuteTarget.Equals(TEXT("stage_g_execute_runtime"), ESearchCase::IgnoreCase))
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("execute_target_mismatch");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteCommitRequest.HandoffTarget.Equals(TEXT("stage_g_execute"), ESearchCase::IgnoreCase))
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("handoff_target_mismatch");
		return FinalizeAndReturn();
	}

	OutReceipt.StageGExecuteCommitReceiptStatus = TEXT("accepted");
	OutReceipt.bWriteEnabled = true;
	OutReceipt.bReadyForStageGExecute = true;
	OutReceipt.bStageGExecuteCommitRequestReady = true;
	OutReceipt.bStageGExecuteCommitAccepted = true;
	OutReceipt.bStageGExecuteCommitReceiptReady = true;
	OutReceipt.ErrorCode = TEXT("-");
	OutReceipt.Reason = TEXT("stage_g_execute_commit_receipt_ready");
	return FinalizeAndReturn();
}

