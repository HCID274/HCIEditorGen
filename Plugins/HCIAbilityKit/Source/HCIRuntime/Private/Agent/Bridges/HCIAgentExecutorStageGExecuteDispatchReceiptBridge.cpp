#include "Agent/Bridges/HCIAgentExecutorStageGExecuteDispatchReceiptBridge.h"

#include "Agent/Contracts/StageF/HCIAgentApplyConfirmRequest.h"
#include "Agent/Contracts/StageF/HCIAgentApplyRequest.h"
#include "Agent/Contracts/StageF/HCIAgentExecuteTicket.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteArchiveBundle.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteFinalReport.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteHandoffEnvelope.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteReceipt.h"
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
static FString HCI_BuildSelectionDigestFromReviewReport_G5(const FHCIDryRunDiffReport& Report)
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

static FString HCI_BuildStageGExecuteDispatchReceiptDigest_G5(const FHCIAgentStageGExecuteDispatchReceipt& Request)
{
	FString Canonical;
	Canonical += FString::Printf(
		TEXT("%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s\n"),
		*Request.StageGExecuteDispatchRequestId,
		*Request.StageGExecutePermitTicketId,
		*Request.StageGWriteEnableRequestId,
		*Request.StageGExecuteIntentId,
		*Request.SimHandoffEnvelopeId,
		*Request.SimArchiveBundleId,
		*Request.SimFinalReportId,
		*Request.SimExecuteReceiptId,
		*Request.ExecuteTicketId,
		*Request.ConfirmRequestId,
		*Request.ApplyRequestId,
		*Request.ReviewRequestId,
		*Request.SelectionDigest,
		*Request.ArchiveDigest,
		*Request.HandoffDigest,
		*Request.ExecuteIntentDigest,
		*Request.StageGWriteEnableDigest,
		*Request.StageGExecutePermitDigest,
		*Request.TerminalStatus,
		*Request.ArchiveStatus,
		*Request.HandoffStatus,
		*Request.StageGStatus,
		*Request.StageGWriteStatus,
		*Request.StageGExecuteDispatchStatus,
		*Request.StageGExecuteDispatchReceiptStatus);

	Canonical += FString::Printf(
		TEXT("%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s\n"),
		Request.bUserConfirmed ? TEXT("1") : TEXT("0"),
		Request.bReadyToSimulateExecute ? TEXT("1") : TEXT("0"),
		Request.bSimulatedDispatchAccepted ? TEXT("1") : TEXT("0"),
		Request.bSimulationCompleted ? TEXT("1") : TEXT("0"),
		Request.bArchiveReady ? TEXT("1") : TEXT("0"),
		Request.bHandoffReady ? TEXT("1") : TEXT("0"),
		Request.bWriteEnabled ? TEXT("1") : TEXT("0"),
		Request.bReadyForStageGEntry ? TEXT("1") : TEXT("0"),
		Request.bWriteEnableConfirmed ? TEXT("1") : TEXT("0"),
		Request.bReadyForStageGExecute ? TEXT("1") : TEXT("0"),
		Request.bStageGExecutePermitReady ? TEXT("1") : TEXT("0"),
		Request.bExecuteDispatchConfirmed ? TEXT("1") : TEXT("0"),
		Request.bStageGExecuteDispatchReady ? TEXT("1") : TEXT("0"),
		Request.bStageGExecuteDispatchAccepted ? TEXT("1") : TEXT("0"),
		Request.bStageGExecuteDispatchReceiptReady ? TEXT("1") : TEXT("0"));

	Canonical += FString::Printf(TEXT("%s|%s|%s\n"), *Request.ExecuteTarget, *Request.HandoffTarget, *Request.Reason);

	for (const FHCIAgentApplyRequestItem& Item : Request.Items)
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

static void HCI_CopyDispatchRequestToDispatchReceipt_G5(
	const FHCIAgentStageGExecuteDispatchRequest& InRequest,
	FHCIAgentStageGExecuteDispatchReceipt& OutRequest)
{
	OutRequest = FHCIAgentStageGExecuteDispatchReceipt();
	OutRequest.RequestId = FString::Printf(TEXT("stagegdispatchrcpt_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	OutRequest.StageGExecuteDispatchRequestId = InRequest.RequestId;
	OutRequest.StageGExecutePermitTicketId = InRequest.StageGExecutePermitTicketId;
	OutRequest.StageGWriteEnableRequestId = InRequest.StageGWriteEnableRequestId;
	OutRequest.StageGExecuteIntentId = InRequest.StageGExecuteIntentId;
	OutRequest.SimHandoffEnvelopeId = InRequest.SimHandoffEnvelopeId;
	OutRequest.SimArchiveBundleId = InRequest.SimArchiveBundleId;
	OutRequest.SimFinalReportId = InRequest.SimFinalReportId;
	OutRequest.SimExecuteReceiptId = InRequest.SimExecuteReceiptId;
	OutRequest.ExecuteTicketId = InRequest.ExecuteTicketId;
	OutRequest.ConfirmRequestId = InRequest.ConfirmRequestId;
	OutRequest.ApplyRequestId = InRequest.ApplyRequestId;
	OutRequest.ReviewRequestId = InRequest.ReviewRequestId;
	OutRequest.SelectionDigest = InRequest.SelectionDigest;
	OutRequest.ArchiveDigest = InRequest.ArchiveDigest;
	OutRequest.HandoffDigest = InRequest.HandoffDigest;
	OutRequest.ExecuteIntentDigest = InRequest.ExecuteIntentDigest;
	OutRequest.StageGWriteEnableDigest = InRequest.StageGWriteEnableDigest;
	OutRequest.StageGExecutePermitDigest = InRequest.StageGExecutePermitDigest;
	OutRequest.StageGExecuteDispatchDigest = InRequest.StageGExecuteDispatchDigest;
	OutRequest.GeneratedUtc = FHCITimeFormat::FormatNowBeijingIso8601();
	OutRequest.ExecutionMode = TEXT("stage_g_execute_dispatch_receipt_dry_run");
	OutRequest.ExecuteTarget = InRequest.ExecuteTarget;
	OutRequest.HandoffTarget = InRequest.HandoffTarget;
	OutRequest.TransactionMode = InRequest.TransactionMode;
	OutRequest.TerminationPolicy = InRequest.TerminationPolicy;
	OutRequest.TerminalStatus = InRequest.TerminalStatus;
	OutRequest.ArchiveStatus = InRequest.ArchiveStatus;
	OutRequest.HandoffStatus = InRequest.HandoffStatus;
	OutRequest.StageGStatus = InRequest.StageGStatus;
	OutRequest.StageGWriteStatus = InRequest.StageGWriteStatus;
	OutRequest.StageGExecutePermitStatus = InRequest.StageGExecutePermitStatus;
	OutRequest.StageGExecuteDispatchStatus = InRequest.StageGExecuteDispatchStatus;
	OutRequest.StageGExecuteDispatchReceiptStatus = TEXT("blocked");
	OutRequest.bUserConfirmed = InRequest.bUserConfirmed;
	OutRequest.bReadyToSimulateExecute = InRequest.bReadyToSimulateExecute;
	OutRequest.bSimulatedDispatchAccepted = InRequest.bSimulatedDispatchAccepted;
	OutRequest.bSimulationCompleted = InRequest.bSimulationCompleted;
	OutRequest.bArchiveReady = InRequest.bArchiveReady;
	OutRequest.bHandoffReady = InRequest.bHandoffReady;
	OutRequest.bWriteEnabled = InRequest.bWriteEnabled;
	OutRequest.bReadyForStageGEntry = InRequest.bReadyForStageGEntry;
	OutRequest.bWriteEnableConfirmed = InRequest.bWriteEnableConfirmed;
	OutRequest.bReadyForStageGExecute = InRequest.bReadyForStageGExecute;
	OutRequest.bStageGExecutePermitReady = InRequest.bStageGExecutePermitReady;
	OutRequest.bExecuteDispatchConfirmed = InRequest.bExecuteDispatchConfirmed;
	OutRequest.bStageGExecuteDispatchReady = InRequest.bStageGExecuteDispatchReady;
	OutRequest.bStageGExecuteDispatchAccepted = false;
	OutRequest.bStageGExecuteDispatchReceiptReady = false;
	OutRequest.ErrorCode = TEXT("-");
	OutRequest.Reason = TEXT("stage_g_execute_dispatch_receipt_ready");
	OutRequest.Summary = InRequest.Summary;
	OutRequest.Items = InRequest.Items;
}
} // namespace

bool FHCIAgentExecutorStageGExecuteDispatchReceiptBridge::BuildStageGExecuteDispatchReceipt(
	const FHCIAgentStageGExecuteDispatchRequest& StageGExecuteDispatchRequest,
	const FString& ExpectedStageGExecuteDispatchRequestId,
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
	FHCIAgentStageGExecuteDispatchReceipt& OutRequest)
{
	HCI_CopyDispatchRequestToDispatchReceipt_G5(StageGExecuteDispatchRequest, OutRequest);

	auto FinalizeAndReturn = [&OutRequest]() -> bool
	{
		OutRequest.StageGExecuteDispatchReceiptDigest = HCI_BuildStageGExecuteDispatchReceiptDigest_G5(OutRequest);
		return true;
	};

	if (!ExpectedStageGExecuteDispatchRequestId.IsEmpty() && StageGExecuteDispatchRequest.RequestId != ExpectedStageGExecuteDispatchRequestId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("stage_g_execute_dispatch_request_id_mismatch");
		return FinalizeAndReturn();
	}

	if (!StageGExecuteDispatchRequest.bUserConfirmed)
	{
		OutRequest.ErrorCode = TEXT("E4005");
		OutRequest.Reason = TEXT("user_not_confirmed");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteDispatchRequest.bWriteEnableConfirmed)
	{
		OutRequest.ErrorCode = TEXT("E4005");
		OutRequest.Reason = TEXT("stage_g_write_enable_not_confirmed");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteDispatchRequest.bExecuteDispatchConfirmed)
	{
		OutRequest.ErrorCode = TEXT("E4005");
		OutRequest.Reason = TEXT("stage_g_execute_dispatch_not_confirmed");
		return FinalizeAndReturn();
	}

	if (!StageGExecuteDispatchRequest.bStageGExecuteDispatchReady ||
		!StageGExecuteDispatchRequest.bReadyForStageGExecute ||
		!StageGExecuteDispatchRequest.bWriteEnabled ||
		!StageGExecuteDispatchRequest.StageGExecuteDispatchStatus.Equals(TEXT("ready"), ESearchCase::IgnoreCase))
	{
		OutRequest.ErrorCode = TEXT("E4213");
		OutRequest.Reason = TEXT("stage_g_execute_dispatch_request_not_ready");
		return FinalizeAndReturn();
	}

	if (!StageGExecuteDispatchRequest.bReadyForStageGEntry ||
		!StageGExecuteDispatchRequest.StageGStatus.Equals(TEXT("ready"), ESearchCase::IgnoreCase))
	{
		OutRequest.ErrorCode = TEXT("E4210");
		OutRequest.Reason = TEXT("stage_g_execute_intent_not_ready");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteDispatchRequest.bReadyToSimulateExecute)
	{
		OutRequest.ErrorCode = TEXT("E4205");
		OutRequest.Reason = TEXT("execute_ticket_not_ready");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteDispatchRequest.bSimulatedDispatchAccepted)
	{
		OutRequest.ErrorCode = TEXT("E4206");
		OutRequest.Reason = TEXT("simulate_execute_receipt_not_accepted");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteDispatchRequest.bSimulationCompleted || !StageGExecuteDispatchRequest.TerminalStatus.Equals(TEXT("completed"), ESearchCase::IgnoreCase))
	{
		OutRequest.ErrorCode = TEXT("E4207");
		OutRequest.Reason = TEXT("simulate_final_report_not_completed");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteDispatchRequest.bArchiveReady || !StageGExecuteDispatchRequest.ArchiveStatus.Equals(TEXT("ready"), ESearchCase::IgnoreCase))
	{
		OutRequest.ErrorCode = TEXT("E4208");
		OutRequest.Reason = TEXT("sim_archive_bundle_not_ready");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteDispatchRequest.bHandoffReady || !StageGExecuteDispatchRequest.HandoffStatus.Equals(TEXT("ready"), ESearchCase::IgnoreCase))
	{
		OutRequest.ErrorCode = TEXT("E4209");
		OutRequest.Reason = TEXT("sim_handoff_envelope_not_ready");
		return FinalizeAndReturn();
	}

	if (!CurrentExecuteTicket.bReadyToSimulateExecute)
	{
		OutRequest.ErrorCode = TEXT("E4205");
		OutRequest.Reason = TEXT("execute_ticket_not_ready");
		return FinalizeAndReturn();
	}
	if (!CurrentConfirmRequest.bReadyToExecute)
	{
		OutRequest.ErrorCode = TEXT("E4204");
		OutRequest.Reason = TEXT("confirm_request_not_ready");
		return FinalizeAndReturn();
	}

	if (StageGExecuteDispatchRequest.StageGExecutePermitTicketId != StageGExecutePermitTicket.RequestId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("stage_g_execute_permit_ticket_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteDispatchRequest.StageGWriteEnableRequestId != StageGWriteEnableRequest.RequestId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("stage_g_write_enable_request_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteDispatchRequest.StageGExecuteIntentId != StageGExecuteIntent.RequestId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("stage_g_execute_intent_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteDispatchRequest.SimHandoffEnvelopeId != SimHandoffEnvelope.RequestId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("sim_handoff_envelope_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteDispatchRequest.SimArchiveBundleId != SimArchiveBundle.RequestId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("sim_archive_bundle_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteDispatchRequest.SimFinalReportId != SimFinalReport.RequestId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("sim_final_report_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteDispatchRequest.SimExecuteReceiptId != SimExecuteReceipt.RequestId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("sim_execute_receipt_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteDispatchRequest.ExecuteTicketId != CurrentExecuteTicket.RequestId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("execute_ticket_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteDispatchRequest.ConfirmRequestId != CurrentConfirmRequest.RequestId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("confirm_request_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteDispatchRequest.ApplyRequestId != CurrentApplyRequest.RequestId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("apply_request_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteDispatchRequest.ReviewRequestId != CurrentReviewReport.RequestId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("review_request_id_mismatch");
		return FinalizeAndReturn();
	}

	if (StageGExecuteDispatchRequest.SelectionDigest != StageGWriteEnableRequest.SelectionDigest ||
		StageGExecuteDispatchRequest.SelectionDigest != StageGExecuteIntent.SelectionDigest ||
		StageGExecuteDispatchRequest.SelectionDigest != SimHandoffEnvelope.SelectionDigest ||
		StageGExecuteDispatchRequest.SelectionDigest != SimArchiveBundle.SelectionDigest ||
		StageGExecuteDispatchRequest.SelectionDigest != SimFinalReport.SelectionDigest ||
		StageGExecuteDispatchRequest.SelectionDigest != SimExecuteReceipt.SelectionDigest ||
		StageGExecuteDispatchRequest.SelectionDigest != CurrentExecuteTicket.SelectionDigest ||
		StageGExecuteDispatchRequest.SelectionDigest != CurrentConfirmRequest.SelectionDigest ||
		StageGExecuteDispatchRequest.SelectionDigest != CurrentApplyRequest.SelectionDigest)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("selection_digest_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteDispatchRequest.SelectionDigest != HCI_BuildSelectionDigestFromReviewReport_G5(CurrentReviewReport))
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("selection_digest_mismatch");
		return FinalizeAndReturn();
	}

	if (StageGExecuteDispatchRequest.ArchiveDigest.IsEmpty() || StageGExecuteDispatchRequest.ArchiveDigest != SimArchiveBundle.ArchiveDigest)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("archive_digest_mismatch_or_missing");
		return FinalizeAndReturn();
	}
	if (StageGExecuteDispatchRequest.HandoffDigest.IsEmpty() || StageGExecuteDispatchRequest.HandoffDigest != SimHandoffEnvelope.HandoffDigest)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("handoff_digest_mismatch_or_missing");
		return FinalizeAndReturn();
	}
	if (StageGExecuteDispatchRequest.ExecuteIntentDigest.IsEmpty() || StageGExecuteDispatchRequest.ExecuteIntentDigest != StageGExecuteIntent.ExecuteIntentDigest)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("execute_intent_digest_mismatch_or_missing");
		return FinalizeAndReturn();
	}
	if (StageGExecuteDispatchRequest.StageGWriteEnableDigest.IsEmpty() || StageGExecuteDispatchRequest.StageGWriteEnableDigest != StageGWriteEnableRequest.StageGWriteEnableDigest)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("stage_g_write_enable_digest_mismatch_or_missing");
		return FinalizeAndReturn();
	}
	if (StageGExecuteDispatchRequest.StageGExecutePermitDigest.IsEmpty() || StageGExecuteDispatchRequest.StageGExecutePermitDigest != StageGExecutePermitTicket.StageGExecutePermitDigest)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("stage_g_execute_permit_digest_mismatch_or_missing");
		return FinalizeAndReturn();
	}
	if (StageGExecuteDispatchRequest.StageGExecuteDispatchDigest.IsEmpty())
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("stage_g_execute_dispatch_digest_missing");
		return FinalizeAndReturn();
	}

	if (!StageGExecuteDispatchRequest.ExecuteTarget.Equals(TEXT("stage_g_execute_runtime"), ESearchCase::IgnoreCase))
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("execute_target_mismatch");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteDispatchRequest.HandoffTarget.Equals(TEXT("stage_g_execute"), ESearchCase::IgnoreCase))
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("handoff_target_mismatch");
		return FinalizeAndReturn();
	}

	OutRequest.StageGExecuteDispatchReceiptStatus = TEXT("accepted");
	OutRequest.bWriteEnabled = true;
	OutRequest.bReadyForStageGExecute = true;
	OutRequest.bStageGExecutePermitReady = true;
	OutRequest.bStageGExecuteDispatchReady = true;
	OutRequest.bStageGExecuteDispatchAccepted = true;
	OutRequest.bStageGExecuteDispatchReceiptReady = true;
	OutRequest.ErrorCode = TEXT("-");
	OutRequest.Reason = TEXT("stage_g_execute_dispatch_receipt_ready");
	return FinalizeAndReturn();
}


