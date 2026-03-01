#include "Agent/Bridges/HCIAgentExecutorStageGExecuteFinalReportBridge.h"

#include "Agent/Contracts/StageF/HCIAgentApplyConfirmRequest.h"
#include "Agent/Contracts/StageF/HCIAgentApplyRequest.h"
#include "Agent/Contracts/StageF/HCIAgentExecuteTicket.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteArchiveBundle.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteFinalReport.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteHandoffEnvelope.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteReceipt.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecuteFinalReport.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecuteCommitReceipt.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecuteCommitReceipt.h"
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
static FString HCI_BuildSelectionDigestFromReviewReport_G8(const FHCIDryRunDiffReport& Report)
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

static FString HCI_BuildStageGExecuteFinalReportDigest_G7(const FHCIAgentStageGExecuteFinalReport& Receipt)
{
	FString Canonical;
	Canonical += FString::Printf(
		TEXT("%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s\n"),
		*Receipt.StageGExecuteCommitReceiptId,
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
		*Receipt.StageGExecuteCommitReceiptDigest,
		*Receipt.TerminalStatus,
		*Receipt.ArchiveStatus,
		*Receipt.HandoffStatus,
		*Receipt.StageGStatus,
		*Receipt.StageGWriteStatus,
		*Receipt.StageGExecuteCommitRequestStatus,
		*Receipt.StageGExecuteCommitReceiptStatus,
		*Receipt.StageGExecuteFinalReportStatus);

	Canonical += FString::Printf(
		TEXT("%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s\n"),
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
		Receipt.bStageGExecuteCommitReceiptReady ? TEXT("1") : TEXT("0"),
		Receipt.bStageGExecuteFinalized ? TEXT("1") : TEXT("0"),
		Receipt.bStageGExecuteFinalReportReady ? TEXT("1") : TEXT("0"));

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

static void HCI_CopyCommitReceiptToFinalReport_G8(
	const FHCIAgentStageGExecuteCommitReceipt& InReceipt,
	FHCIAgentStageGExecuteFinalReport& OutReceipt)
{
	OutReceipt = FHCIAgentStageGExecuteFinalReport();
	OutReceipt.RequestId = FString::Printf(TEXT("stagegfinal_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	OutReceipt.StageGExecuteCommitReceiptId = InReceipt.RequestId;
	OutReceipt.StageGExecuteCommitRequestId = InReceipt.StageGExecuteCommitRequestId;
	OutReceipt.StageGExecuteDispatchReceiptId = InReceipt.StageGExecuteDispatchReceiptId;
	OutReceipt.StageGExecuteDispatchRequestId = InReceipt.StageGExecuteDispatchRequestId;
	OutReceipt.StageGExecutePermitTicketId = InReceipt.StageGExecutePermitTicketId;
	OutReceipt.StageGWriteEnableRequestId = InReceipt.StageGWriteEnableRequestId;
	OutReceipt.StageGExecuteIntentId = InReceipt.StageGExecuteIntentId;
	OutReceipt.SimHandoffEnvelopeId = InReceipt.SimHandoffEnvelopeId;
	OutReceipt.SimArchiveBundleId = InReceipt.SimArchiveBundleId;
	OutReceipt.SimFinalReportId = InReceipt.SimFinalReportId;
	OutReceipt.SimExecuteReceiptId = InReceipt.SimExecuteReceiptId;
	OutReceipt.ExecuteTicketId = InReceipt.ExecuteTicketId;
	OutReceipt.ConfirmRequestId = InReceipt.ConfirmRequestId;
	OutReceipt.ApplyRequestId = InReceipt.ApplyRequestId;
	OutReceipt.ReviewRequestId = InReceipt.ReviewRequestId;
	OutReceipt.SelectionDigest = InReceipt.SelectionDigest;
	OutReceipt.ArchiveDigest = InReceipt.ArchiveDigest;
	OutReceipt.HandoffDigest = InReceipt.HandoffDigest;
	OutReceipt.ExecuteIntentDigest = InReceipt.ExecuteIntentDigest;
	OutReceipt.StageGWriteEnableDigest = InReceipt.StageGWriteEnableDigest;
	OutReceipt.StageGExecutePermitDigest = InReceipt.StageGExecutePermitDigest;
	OutReceipt.StageGExecuteDispatchDigest = InReceipt.StageGExecuteDispatchDigest;
	OutReceipt.StageGExecuteDispatchReceiptDigest = InReceipt.StageGExecuteDispatchReceiptDigest;
	OutReceipt.StageGExecuteCommitRequestDigest = InReceipt.StageGExecuteCommitRequestDigest;
	OutReceipt.StageGExecuteCommitReceiptDigest = InReceipt.StageGExecuteCommitReceiptDigest;
	OutReceipt.GeneratedUtc = FHCITimeFormat::FormatNowBeijingIso8601();
	OutReceipt.ExecutionMode = TEXT("stage_g_execute_final_report_dry_run");
	OutReceipt.ExecuteTarget = InReceipt.ExecuteTarget;
	OutReceipt.HandoffTarget = InReceipt.HandoffTarget;
	OutReceipt.TransactionMode = InReceipt.TransactionMode;
	OutReceipt.TerminationPolicy = InReceipt.TerminationPolicy;
	OutReceipt.TerminalStatus = InReceipt.TerminalStatus;
	OutReceipt.ArchiveStatus = InReceipt.ArchiveStatus;
	OutReceipt.HandoffStatus = InReceipt.HandoffStatus;
	OutReceipt.StageGStatus = InReceipt.StageGStatus;
	OutReceipt.StageGWriteStatus = InReceipt.StageGWriteStatus;
	OutReceipt.StageGExecutePermitStatus = InReceipt.StageGExecutePermitStatus;
	OutReceipt.StageGExecuteDispatchStatus = InReceipt.StageGExecuteDispatchStatus;
	OutReceipt.StageGExecuteDispatchReceiptStatus = InReceipt.StageGExecuteDispatchReceiptStatus;
	OutReceipt.StageGExecuteCommitRequestStatus = InReceipt.StageGExecuteCommitRequestStatus;
	OutReceipt.StageGExecuteCommitReceiptStatus = InReceipt.StageGExecuteCommitReceiptStatus;
	OutReceipt.StageGExecuteFinalReportStatus = TEXT("blocked");
	OutReceipt.bUserConfirmed = InReceipt.bUserConfirmed;
	OutReceipt.bReadyToSimulateExecute = InReceipt.bReadyToSimulateExecute;
	OutReceipt.bSimulatedDispatchAccepted = InReceipt.bSimulatedDispatchAccepted;
	OutReceipt.bSimulationCompleted = InReceipt.bSimulationCompleted;
	OutReceipt.bArchiveReady = InReceipt.bArchiveReady;
	OutReceipt.bHandoffReady = InReceipt.bHandoffReady;
	OutReceipt.bWriteEnabled = InReceipt.bWriteEnabled;
	OutReceipt.bReadyForStageGEntry = InReceipt.bReadyForStageGEntry;
	OutReceipt.bWriteEnableConfirmed = InReceipt.bWriteEnableConfirmed;
	OutReceipt.bReadyForStageGExecute = InReceipt.bReadyForStageGExecute;
	OutReceipt.bStageGExecutePermitReady = InReceipt.bStageGExecutePermitReady;
	OutReceipt.bExecuteDispatchConfirmed = InReceipt.bExecuteDispatchConfirmed;
	OutReceipt.bStageGExecuteDispatchReady = InReceipt.bStageGExecuteDispatchReady;
	OutReceipt.bStageGExecuteDispatchAccepted = InReceipt.bStageGExecuteDispatchAccepted;
	OutReceipt.bStageGExecuteDispatchReceiptReady = InReceipt.bStageGExecuteDispatchReceiptReady;
	OutReceipt.bExecuteCommitConfirmed = InReceipt.bExecuteCommitConfirmed;
	OutReceipt.bStageGExecuteCommitRequestReady = InReceipt.bStageGExecuteCommitRequestReady;
	OutReceipt.bStageGExecuteCommitAccepted = InReceipt.bStageGExecuteCommitAccepted;
	OutReceipt.bStageGExecuteCommitReceiptReady = InReceipt.bStageGExecuteCommitReceiptReady;
	OutReceipt.bStageGExecuteFinalized = false;
	OutReceipt.bStageGExecuteFinalReportReady = false;
	OutReceipt.ErrorCode = TEXT("-");
	OutReceipt.Reason = TEXT("stage_g_execute_final_report_ready");
	OutReceipt.Summary = InReceipt.Summary;
	OutReceipt.Items = InReceipt.Items;
}
} // namespace

bool FHCIAgentExecutorStageGExecuteFinalReportBridge::BuildStageGExecuteFinalReport(
	const FHCIAgentStageGExecuteCommitReceipt& StageGExecuteCommitReceipt,
	const FString& ExpectedStageGExecuteCommitReceiptId,
	const FString& ExpectedStageGExecuteCommitRequestId,
	const FString& ExpectedStageGExecuteCommitRequestDigest,
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
	FHCIAgentStageGExecuteFinalReport& OutReceipt)
{
	HCI_CopyCommitReceiptToFinalReport_G8(StageGExecuteCommitReceipt, OutReceipt);

	auto FinalizeAndReturn = [&OutReceipt]() -> bool
	{
		OutReceipt.StageGExecuteFinalReportDigest = HCI_BuildStageGExecuteFinalReportDigest_G7(OutReceipt);
		return true;
	};

	if (!ExpectedStageGExecuteCommitReceiptId.IsEmpty() && StageGExecuteCommitReceipt.RequestId != ExpectedStageGExecuteCommitReceiptId)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("stage_g_execute_commit_receipt_id_mismatch");
		return FinalizeAndReturn();
	}

	if (!StageGExecuteCommitReceipt.bUserConfirmed)
	{
		OutReceipt.ErrorCode = TEXT("E4005");
		OutReceipt.Reason = TEXT("user_not_confirmed");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteCommitReceipt.bWriteEnableConfirmed)
	{
		OutReceipt.ErrorCode = TEXT("E4005");
		OutReceipt.Reason = TEXT("stage_g_write_enable_not_confirmed");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteCommitReceipt.bExecuteDispatchConfirmed)
	{
		OutReceipt.ErrorCode = TEXT("E4005");
		OutReceipt.Reason = TEXT("stage_g_execute_dispatch_not_confirmed");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteCommitReceipt.bExecuteCommitConfirmed)
	{
		OutReceipt.ErrorCode = TEXT("E4005");
		OutReceipt.Reason = TEXT("stage_g_execute_commit_not_confirmed");
		return FinalizeAndReturn();
	}

	if (!StageGExecuteCommitReceipt.bStageGExecuteCommitReceiptReady ||
		!StageGExecuteCommitReceipt.bStageGExecuteCommitAccepted ||
		!StageGExecuteCommitReceipt.bWriteEnabled ||
		!StageGExecuteCommitReceipt.bReadyForStageGExecute ||
		!StageGExecuteCommitReceipt.StageGExecuteCommitReceiptStatus.Equals(TEXT("accepted"), ESearchCase::IgnoreCase))
	{
		OutReceipt.ErrorCode = TEXT("E4216");
		OutReceipt.Reason = TEXT("stage_g_execute_commit_receipt_not_ready");
		return FinalizeAndReturn();
	}

	if (!StageGExecuteCommitReceipt.bReadyForStageGEntry || !StageGExecuteCommitReceipt.StageGStatus.Equals(TEXT("ready"), ESearchCase::IgnoreCase))
	{
		OutReceipt.ErrorCode = TEXT("E4210");
		OutReceipt.Reason = TEXT("stage_g_execute_intent_not_ready");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteCommitReceipt.bReadyToSimulateExecute)
	{
		OutReceipt.ErrorCode = TEXT("E4205");
		OutReceipt.Reason = TEXT("execute_ticket_not_ready");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteCommitReceipt.bSimulatedDispatchAccepted)
	{
		OutReceipt.ErrorCode = TEXT("E4206");
		OutReceipt.Reason = TEXT("simulate_execute_receipt_not_accepted");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteCommitReceipt.bSimulationCompleted || !StageGExecuteCommitReceipt.TerminalStatus.Equals(TEXT("completed"), ESearchCase::IgnoreCase))
	{
		OutReceipt.ErrorCode = TEXT("E4207");
		OutReceipt.Reason = TEXT("simulate_final_report_not_completed");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteCommitReceipt.bArchiveReady || !StageGExecuteCommitReceipt.ArchiveStatus.Equals(TEXT("ready"), ESearchCase::IgnoreCase))
	{
		OutReceipt.ErrorCode = TEXT("E4208");
		OutReceipt.Reason = TEXT("sim_archive_bundle_not_ready");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteCommitReceipt.bHandoffReady || !StageGExecuteCommitReceipt.HandoffStatus.Equals(TEXT("ready"), ESearchCase::IgnoreCase))
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

	if (!ExpectedStageGExecuteCommitRequestId.IsEmpty() &&
		StageGExecuteCommitReceipt.StageGExecuteCommitRequestId != ExpectedStageGExecuteCommitRequestId)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("stage_g_execute_commit_request_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteCommitReceipt.StageGExecuteDispatchReceiptId != StageGExecuteDispatchReceipt.RequestId)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("stage_g_execute_dispatch_receipt_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteCommitReceipt.StageGExecuteDispatchRequestId != StageGExecuteDispatchRequest.RequestId)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("stage_g_execute_dispatch_request_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteCommitReceipt.StageGExecutePermitTicketId != StageGExecutePermitTicket.RequestId)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("stage_g_execute_permit_ticket_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteCommitReceipt.StageGWriteEnableRequestId != StageGWriteEnableRequest.RequestId)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("stage_g_write_enable_request_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteCommitReceipt.StageGExecuteIntentId != StageGExecuteIntent.RequestId)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("stage_g_execute_intent_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteCommitReceipt.SimHandoffEnvelopeId != SimHandoffEnvelope.RequestId)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("sim_handoff_envelope_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteCommitReceipt.SimArchiveBundleId != SimArchiveBundle.RequestId)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("sim_archive_bundle_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteCommitReceipt.SimFinalReportId != SimFinalReport.RequestId)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("sim_final_report_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteCommitReceipt.SimExecuteReceiptId != SimExecuteReceipt.RequestId)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("sim_execute_receipt_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteCommitReceipt.ExecuteTicketId != CurrentExecuteTicket.RequestId)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("execute_ticket_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteCommitReceipt.ConfirmRequestId != CurrentConfirmRequest.RequestId)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("confirm_request_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteCommitReceipt.ApplyRequestId != CurrentApplyRequest.RequestId)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("apply_request_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteCommitReceipt.ReviewRequestId != CurrentReviewReport.RequestId)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("review_request_id_mismatch");
		return FinalizeAndReturn();
	}

	if (StageGExecuteCommitReceipt.SelectionDigest != StageGExecuteDispatchReceipt.SelectionDigest ||
		StageGExecuteCommitReceipt.SelectionDigest != StageGWriteEnableRequest.SelectionDigest ||
		StageGExecuteCommitReceipt.SelectionDigest != StageGExecuteIntent.SelectionDigest ||
		StageGExecuteCommitReceipt.SelectionDigest != SimHandoffEnvelope.SelectionDigest ||
		StageGExecuteCommitReceipt.SelectionDigest != SimArchiveBundle.SelectionDigest ||
		StageGExecuteCommitReceipt.SelectionDigest != SimFinalReport.SelectionDigest ||
		StageGExecuteCommitReceipt.SelectionDigest != SimExecuteReceipt.SelectionDigest ||
		StageGExecuteCommitReceipt.SelectionDigest != CurrentExecuteTicket.SelectionDigest ||
		StageGExecuteCommitReceipt.SelectionDigest != CurrentConfirmRequest.SelectionDigest ||
		StageGExecuteCommitReceipt.SelectionDigest != CurrentApplyRequest.SelectionDigest)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("selection_digest_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteCommitReceipt.SelectionDigest != HCI_BuildSelectionDigestFromReviewReport_G8(CurrentReviewReport))
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("selection_digest_mismatch");
		return FinalizeAndReturn();
	}

	if (StageGExecuteCommitReceipt.ArchiveDigest.IsEmpty() || StageGExecuteCommitReceipt.ArchiveDigest != SimArchiveBundle.ArchiveDigest)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("archive_digest_mismatch_or_missing");
		return FinalizeAndReturn();
	}
	if (StageGExecuteCommitReceipt.HandoffDigest.IsEmpty() || StageGExecuteCommitReceipt.HandoffDigest != SimHandoffEnvelope.HandoffDigest)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("handoff_digest_mismatch_or_missing");
		return FinalizeAndReturn();
	}
	if (StageGExecuteCommitReceipt.ExecuteIntentDigest.IsEmpty() || StageGExecuteCommitReceipt.ExecuteIntentDigest != StageGExecuteIntent.ExecuteIntentDigest)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("execute_intent_digest_mismatch_or_missing");
		return FinalizeAndReturn();
	}
	if (StageGExecuteCommitReceipt.StageGWriteEnableDigest.IsEmpty() || StageGExecuteCommitReceipt.StageGWriteEnableDigest != StageGWriteEnableRequest.StageGWriteEnableDigest)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("stage_g_write_enable_digest_mismatch_or_missing");
		return FinalizeAndReturn();
	}
	if (StageGExecuteCommitReceipt.StageGExecutePermitDigest.IsEmpty() || StageGExecuteCommitReceipt.StageGExecutePermitDigest != StageGExecutePermitTicket.StageGExecutePermitDigest)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("stage_g_execute_permit_digest_mismatch_or_missing");
		return FinalizeAndReturn();
	}
	if (StageGExecuteCommitReceipt.StageGExecuteDispatchDigest.IsEmpty() || StageGExecuteCommitReceipt.StageGExecuteDispatchDigest != StageGExecuteDispatchRequest.StageGExecuteDispatchDigest)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("stage_g_execute_dispatch_digest_mismatch_or_missing");
		return FinalizeAndReturn();
	}
	if (StageGExecuteCommitReceipt.StageGExecuteDispatchReceiptDigest.IsEmpty() || StageGExecuteCommitReceipt.StageGExecuteDispatchReceiptDigest != StageGExecuteDispatchReceipt.StageGExecuteDispatchReceiptDigest)
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("stage_g_execute_dispatch_receipt_digest_mismatch_or_missing");
		return FinalizeAndReturn();
	}
	if (StageGExecuteCommitReceipt.StageGExecuteCommitRequestDigest.IsEmpty() ||
		(!ExpectedStageGExecuteCommitRequestDigest.IsEmpty() &&
		 StageGExecuteCommitReceipt.StageGExecuteCommitRequestDigest != ExpectedStageGExecuteCommitRequestDigest))
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("stage_g_execute_commit_request_digest_mismatch_or_missing");
		return FinalizeAndReturn();
	}
	if (StageGExecuteCommitReceipt.StageGExecuteCommitReceiptDigest.IsEmpty())
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("stage_g_execute_commit_receipt_digest_missing");
		return FinalizeAndReturn();
	}

	if (!StageGExecuteCommitReceipt.ExecuteTarget.Equals(TEXT("stage_g_execute_runtime"), ESearchCase::IgnoreCase))
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("execute_target_mismatch");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteCommitReceipt.HandoffTarget.Equals(TEXT("stage_g_execute"), ESearchCase::IgnoreCase))
	{
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("handoff_target_mismatch");
		return FinalizeAndReturn();
	}

	OutReceipt.StageGExecuteFinalReportStatus = TEXT("completed");
	OutReceipt.bWriteEnabled = true;
	OutReceipt.bReadyForStageGExecute = true;
	OutReceipt.bStageGExecuteCommitRequestReady = true;
	OutReceipt.bStageGExecuteCommitAccepted = true;
	OutReceipt.bStageGExecuteCommitReceiptReady = true;
	OutReceipt.bStageGExecuteFinalized = true;
	OutReceipt.bStageGExecuteFinalReportReady = true;
	OutReceipt.ErrorCode = TEXT("-");
	OutReceipt.Reason = TEXT("stage_g_execute_final_report_ready");
	return FinalizeAndReturn();
}
