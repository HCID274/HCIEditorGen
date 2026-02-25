#include "Agent/Bridges/HCIAbilityKitAgentExecutorStageGExecuteCommitRequestBridge.h"

#include "Agent/Contracts/StageF/HCIAbilityKitAgentApplyConfirmRequest.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentApplyRequest.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentExecuteTicket.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentSimulateExecuteArchiveBundle.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentSimulateExecuteFinalReport.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentSimulateExecuteHandoffEnvelope.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentSimulateExecuteReceipt.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGExecuteDispatchRequest.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGExecuteDispatchReceipt.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGExecuteCommitRequest.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGExecuteIntent.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGExecutePermitTicket.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGWriteEnableRequest.h"
#include "Agent/Executor/HCIAbilityKitDryRunDiff.h"
#include "Common/HCIAbilityKitTimeFormat.h"
#include "Misc/Crc.h"
#include "Misc/Guid.h"

namespace
{
static FString HCI_BuildSelectionDigestFromReviewReport_G6(const FHCIAbilityKitDryRunDiffReport& Report)
{
	FString Canonical;
	Canonical.Reserve(Report.DiffItems.Num() * 96);
	for (int32 RowIndex = 0; RowIndex < Report.DiffItems.Num(); ++RowIndex)
	{
		const FHCIAbilityKitDryRunDiffItem& Item = Report.DiffItems[RowIndex];
		Canonical += FString::Printf(
			TEXT("%d|%s|%s|%s|%s|%s|%s|%s\n"),
			RowIndex,
			*Item.ToolName,
			*Item.AssetPath,
			*Item.Field,
			*FHCIAbilityKitDryRunDiff::RiskToString(Item.Risk),
			*Item.SkipReason,
			*FHCIAbilityKitDryRunDiff::ObjectTypeToString(Item.ObjectType),
			*FHCIAbilityKitDryRunDiff::LocateStrategyToString(Item.LocateStrategy));
	}
	return FString::Printf(TEXT("crc32_%08X"), FCrc::StrCrc32(*Canonical));
}

static FString HCI_BuildStageGExecuteCommitRequestDigest_G6(const FHCIAbilityKitAgentStageGExecuteCommitRequest& Request)
{
	FString Canonical;
	Canonical += FString::Printf(
		TEXT("%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s\n"),
		*Request.StageGExecuteDispatchReceiptId,
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
		*Request.StageGExecuteDispatchReceiptDigest,
		*Request.StageGExecuteDispatchReceiptStatus,
		*Request.StageGExecuteCommitRequestStatus);

	Canonical += FString::Printf(
		TEXT("%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s\n"),
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
		Request.bStageGExecuteDispatchReceiptReady ? TEXT("1") : TEXT("0"),
		Request.bExecuteCommitConfirmed ? TEXT("1") : TEXT("0"),
		Request.bStageGExecuteCommitRequestReady ? TEXT("1") : TEXT("0"));

	Canonical += FString::Printf(TEXT("%s|%s|%s\n"), *Request.ExecuteTarget, *Request.HandoffTarget, *Request.Reason);

	for (const FHCIAbilityKitAgentApplyRequestItem& Item : Request.Items)
	{
		Canonical += FString::Printf(
			TEXT("%d|%s|%s|%s|%s|%s|%s|%s|%s|%s\n"),
			Item.RowIndex,
			*Item.ToolName,
			*Item.AssetPath,
			*Item.Field,
			*Item.SkipReason,
			Item.bBlocked ? TEXT("1") : TEXT("0"),
			*FHCIAbilityKitDryRunDiff::RiskToString(Item.Risk),
			*FHCIAbilityKitDryRunDiff::ObjectTypeToString(Item.ObjectType),
			*FHCIAbilityKitDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
			*Item.EvidenceKey);
	}

	return FString::Printf(TEXT("crc32_%08X"), FCrc::StrCrc32(*Canonical));
}

static void HCI_CopyDispatchReceiptToCommitRequest_G6(
	const FHCIAbilityKitAgentStageGExecuteDispatchReceipt& InReceipt,
	FHCIAbilityKitAgentStageGExecuteCommitRequest& OutRequest)
{
	OutRequest = FHCIAbilityKitAgentStageGExecuteCommitRequest();
	OutRequest.RequestId = FString::Printf(TEXT("stagegcommitreq_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	OutRequest.StageGExecuteDispatchReceiptId = InReceipt.RequestId;
	OutRequest.StageGExecuteDispatchRequestId = InReceipt.StageGExecuteDispatchRequestId;
	OutRequest.StageGExecutePermitTicketId = InReceipt.StageGExecutePermitTicketId;
	OutRequest.StageGWriteEnableRequestId = InReceipt.StageGWriteEnableRequestId;
	OutRequest.StageGExecuteIntentId = InReceipt.StageGExecuteIntentId;
	OutRequest.SimHandoffEnvelopeId = InReceipt.SimHandoffEnvelopeId;
	OutRequest.SimArchiveBundleId = InReceipt.SimArchiveBundleId;
	OutRequest.SimFinalReportId = InReceipt.SimFinalReportId;
	OutRequest.SimExecuteReceiptId = InReceipt.SimExecuteReceiptId;
	OutRequest.ExecuteTicketId = InReceipt.ExecuteTicketId;
	OutRequest.ConfirmRequestId = InReceipt.ConfirmRequestId;
	OutRequest.ApplyRequestId = InReceipt.ApplyRequestId;
	OutRequest.ReviewRequestId = InReceipt.ReviewRequestId;
	OutRequest.SelectionDigest = InReceipt.SelectionDigest;
	OutRequest.ArchiveDigest = InReceipt.ArchiveDigest;
	OutRequest.HandoffDigest = InReceipt.HandoffDigest;
	OutRequest.ExecuteIntentDigest = InReceipt.ExecuteIntentDigest;
	OutRequest.StageGWriteEnableDigest = InReceipt.StageGWriteEnableDigest;
	OutRequest.StageGExecutePermitDigest = InReceipt.StageGExecutePermitDigest;
	OutRequest.StageGExecuteDispatchDigest = InReceipt.StageGExecuteDispatchDigest;
	OutRequest.StageGExecuteDispatchReceiptDigest = InReceipt.StageGExecuteDispatchReceiptDigest;
	OutRequest.GeneratedUtc = FHCIAbilityKitTimeFormat::FormatNowBeijingIso8601();
	OutRequest.ExecutionMode = TEXT("stage_g_execute_commit_request_dry_run");
	OutRequest.ExecuteTarget = InReceipt.ExecuteTarget;
	OutRequest.HandoffTarget = InReceipt.HandoffTarget;
	OutRequest.TransactionMode = InReceipt.TransactionMode;
	OutRequest.TerminationPolicy = InReceipt.TerminationPolicy;
	OutRequest.TerminalStatus = InReceipt.TerminalStatus;
	OutRequest.ArchiveStatus = InReceipt.ArchiveStatus;
	OutRequest.HandoffStatus = InReceipt.HandoffStatus;
	OutRequest.StageGStatus = InReceipt.StageGStatus;
	OutRequest.StageGWriteStatus = InReceipt.StageGWriteStatus;
	OutRequest.StageGExecutePermitStatus = InReceipt.StageGExecutePermitStatus;
	OutRequest.StageGExecuteDispatchStatus = InReceipt.StageGExecuteDispatchStatus;
	OutRequest.StageGExecuteDispatchReceiptStatus = InReceipt.StageGExecuteDispatchReceiptStatus;
	OutRequest.StageGExecuteCommitRequestStatus = TEXT("blocked");
	OutRequest.bUserConfirmed = InReceipt.bUserConfirmed;
	OutRequest.bReadyToSimulateExecute = InReceipt.bReadyToSimulateExecute;
	OutRequest.bSimulatedDispatchAccepted = InReceipt.bSimulatedDispatchAccepted;
	OutRequest.bSimulationCompleted = InReceipt.bSimulationCompleted;
	OutRequest.bArchiveReady = InReceipt.bArchiveReady;
	OutRequest.bHandoffReady = InReceipt.bHandoffReady;
	OutRequest.bWriteEnabled = InReceipt.bWriteEnabled;
	OutRequest.bReadyForStageGEntry = InReceipt.bReadyForStageGEntry;
	OutRequest.bWriteEnableConfirmed = InReceipt.bWriteEnableConfirmed;
	OutRequest.bReadyForStageGExecute = InReceipt.bReadyForStageGExecute;
	OutRequest.bStageGExecutePermitReady = InReceipt.bStageGExecutePermitReady;
	OutRequest.bExecuteDispatchConfirmed = InReceipt.bExecuteDispatchConfirmed;
	OutRequest.bStageGExecuteDispatchReady = InReceipt.bStageGExecuteDispatchReady;
	OutRequest.bStageGExecuteDispatchAccepted = InReceipt.bStageGExecuteDispatchAccepted;
	OutRequest.bStageGExecuteDispatchReceiptReady = InReceipt.bStageGExecuteDispatchReceiptReady;
	OutRequest.bExecuteCommitConfirmed = false;
	OutRequest.bStageGExecuteCommitRequestReady = false;
	OutRequest.ErrorCode = TEXT("-");
	OutRequest.Reason = TEXT("stage_g_execute_commit_request_ready");
	OutRequest.Summary = InReceipt.Summary;
	OutRequest.Items = InReceipt.Items;
}
} // namespace

bool FHCIAbilityKitAgentExecutorStageGExecuteCommitRequestBridge::BuildStageGExecuteCommitRequest(
	const FHCIAbilityKitAgentStageGExecuteDispatchReceipt& StageGExecuteDispatchReceipt,
	const FString& ExpectedStageGExecuteDispatchReceiptId,
	const FHCIAbilityKitAgentStageGExecuteDispatchRequest& StageGExecuteDispatchRequest,
	const FHCIAbilityKitAgentStageGExecutePermitTicket& StageGExecutePermitTicket,
	const FHCIAbilityKitAgentStageGWriteEnableRequest& StageGWriteEnableRequest,
	const FHCIAbilityKitAgentStageGExecuteIntent& StageGExecuteIntent,
	const FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope& SimHandoffEnvelope,
	const FHCIAbilityKitAgentSimulateExecuteArchiveBundle& SimArchiveBundle,
	const FHCIAbilityKitAgentSimulateExecuteFinalReport& SimFinalReport,
	const FHCIAbilityKitAgentSimulateExecuteReceipt& SimExecuteReceipt,
	const FHCIAbilityKitAgentExecuteTicket& CurrentExecuteTicket,
	const FHCIAbilityKitAgentApplyConfirmRequest& CurrentConfirmRequest,
	const FHCIAbilityKitAgentApplyRequest& CurrentApplyRequest,
	const FHCIAbilityKitDryRunDiffReport& CurrentReviewReport,
	bool bExecuteCommitConfirmed,
	FHCIAbilityKitAgentStageGExecuteCommitRequest& OutRequest)
{
	HCI_CopyDispatchReceiptToCommitRequest_G6(StageGExecuteDispatchReceipt, OutRequest);
	OutRequest.bExecuteCommitConfirmed = bExecuteCommitConfirmed;

	auto FinalizeAndReturn = [&OutRequest]() -> bool
	{
		OutRequest.StageGExecuteCommitRequestDigest = HCI_BuildStageGExecuteCommitRequestDigest_G6(OutRequest);
		return true;
	};

	if (!ExpectedStageGExecuteDispatchReceiptId.IsEmpty() && StageGExecuteDispatchReceipt.RequestId != ExpectedStageGExecuteDispatchReceiptId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("stage_g_execute_dispatch_receipt_id_mismatch");
		return FinalizeAndReturn();
	}

	if (!StageGExecuteDispatchReceipt.bUserConfirmed)
	{
		OutRequest.ErrorCode = TEXT("E4005");
		OutRequest.Reason = TEXT("user_not_confirmed");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteDispatchReceipt.bWriteEnableConfirmed)
	{
		OutRequest.ErrorCode = TEXT("E4005");
		OutRequest.Reason = TEXT("stage_g_write_enable_not_confirmed");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteDispatchReceipt.bExecuteDispatchConfirmed)
	{
		OutRequest.ErrorCode = TEXT("E4005");
		OutRequest.Reason = TEXT("stage_g_execute_dispatch_not_confirmed");
		return FinalizeAndReturn();
	}
	if (!bExecuteCommitConfirmed)
	{
		OutRequest.ErrorCode = TEXT("E4005");
		OutRequest.Reason = TEXT("stage_g_execute_commit_not_confirmed");
		return FinalizeAndReturn();
	}

	if (!StageGExecuteDispatchReceipt.bStageGExecuteDispatchReady ||
		!StageGExecuteDispatchReceipt.bReadyForStageGExecute ||
		!StageGExecuteDispatchReceipt.bWriteEnabled ||
		!StageGExecuteDispatchReceipt.bStageGExecuteDispatchAccepted ||
		!StageGExecuteDispatchReceipt.bStageGExecuteDispatchReceiptReady ||
		!StageGExecuteDispatchReceipt.StageGExecuteDispatchStatus.Equals(TEXT("ready"), ESearchCase::IgnoreCase) ||
		!StageGExecuteDispatchReceipt.StageGExecuteDispatchReceiptStatus.Equals(TEXT("accepted"), ESearchCase::IgnoreCase))
	{
		OutRequest.ErrorCode = TEXT("E4214");
		OutRequest.Reason = TEXT("stage_g_execute_dispatch_receipt_not_ready");
		return FinalizeAndReturn();
	}

	if (!StageGExecuteDispatchReceipt.bReadyForStageGEntry ||
		!StageGExecuteDispatchReceipt.StageGStatus.Equals(TEXT("ready"), ESearchCase::IgnoreCase))
	{
		OutRequest.ErrorCode = TEXT("E4210");
		OutRequest.Reason = TEXT("stage_g_execute_intent_not_ready");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteDispatchReceipt.bReadyToSimulateExecute)
	{
		OutRequest.ErrorCode = TEXT("E4205");
		OutRequest.Reason = TEXT("execute_ticket_not_ready");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteDispatchReceipt.bSimulatedDispatchAccepted)
	{
		OutRequest.ErrorCode = TEXT("E4206");
		OutRequest.Reason = TEXT("simulate_execute_receipt_not_accepted");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteDispatchReceipt.bSimulationCompleted || !StageGExecuteDispatchReceipt.TerminalStatus.Equals(TEXT("completed"), ESearchCase::IgnoreCase))
	{
		OutRequest.ErrorCode = TEXT("E4207");
		OutRequest.Reason = TEXT("simulate_final_report_not_completed");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteDispatchReceipt.bArchiveReady || !StageGExecuteDispatchReceipt.ArchiveStatus.Equals(TEXT("ready"), ESearchCase::IgnoreCase))
	{
		OutRequest.ErrorCode = TEXT("E4208");
		OutRequest.Reason = TEXT("sim_archive_bundle_not_ready");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteDispatchReceipt.bHandoffReady || !StageGExecuteDispatchReceipt.HandoffStatus.Equals(TEXT("ready"), ESearchCase::IgnoreCase))
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

	if (StageGExecuteDispatchReceipt.StageGExecuteDispatchRequestId != StageGExecuteDispatchRequest.RequestId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("stage_g_execute_dispatch_request_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteDispatchReceipt.StageGExecutePermitTicketId != StageGExecutePermitTicket.RequestId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("stage_g_execute_permit_ticket_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteDispatchReceipt.StageGWriteEnableRequestId != StageGWriteEnableRequest.RequestId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("stage_g_write_enable_request_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteDispatchReceipt.StageGExecuteIntentId != StageGExecuteIntent.RequestId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("stage_g_execute_intent_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteDispatchReceipt.SimHandoffEnvelopeId != SimHandoffEnvelope.RequestId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("sim_handoff_envelope_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteDispatchReceipt.SimArchiveBundleId != SimArchiveBundle.RequestId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("sim_archive_bundle_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteDispatchReceipt.SimFinalReportId != SimFinalReport.RequestId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("sim_final_report_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteDispatchReceipt.SimExecuteReceiptId != SimExecuteReceipt.RequestId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("sim_execute_receipt_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteDispatchReceipt.ExecuteTicketId != CurrentExecuteTicket.RequestId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("execute_ticket_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteDispatchReceipt.ConfirmRequestId != CurrentConfirmRequest.RequestId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("confirm_request_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteDispatchReceipt.ApplyRequestId != CurrentApplyRequest.RequestId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("apply_request_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteDispatchReceipt.ReviewRequestId != CurrentReviewReport.RequestId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("review_request_id_mismatch");
		return FinalizeAndReturn();
	}

	if (StageGExecuteDispatchReceipt.SelectionDigest != StageGWriteEnableRequest.SelectionDigest ||
		StageGExecuteDispatchReceipt.SelectionDigest != StageGExecuteIntent.SelectionDigest ||
		StageGExecuteDispatchReceipt.SelectionDigest != SimHandoffEnvelope.SelectionDigest ||
		StageGExecuteDispatchReceipt.SelectionDigest != SimArchiveBundle.SelectionDigest ||
		StageGExecuteDispatchReceipt.SelectionDigest != SimFinalReport.SelectionDigest ||
		StageGExecuteDispatchReceipt.SelectionDigest != SimExecuteReceipt.SelectionDigest ||
		StageGExecuteDispatchReceipt.SelectionDigest != CurrentExecuteTicket.SelectionDigest ||
		StageGExecuteDispatchReceipt.SelectionDigest != CurrentConfirmRequest.SelectionDigest ||
		StageGExecuteDispatchReceipt.SelectionDigest != CurrentApplyRequest.SelectionDigest)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("selection_digest_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteDispatchReceipt.SelectionDigest != HCI_BuildSelectionDigestFromReviewReport_G6(CurrentReviewReport))
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("selection_digest_mismatch");
		return FinalizeAndReturn();
	}

	if (StageGExecuteDispatchReceipt.ArchiveDigest.IsEmpty() || StageGExecuteDispatchReceipt.ArchiveDigest != SimArchiveBundle.ArchiveDigest)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("archive_digest_mismatch_or_missing");
		return FinalizeAndReturn();
	}
	if (StageGExecuteDispatchReceipt.HandoffDigest.IsEmpty() || StageGExecuteDispatchReceipt.HandoffDigest != SimHandoffEnvelope.HandoffDigest)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("handoff_digest_mismatch_or_missing");
		return FinalizeAndReturn();
	}
	if (StageGExecuteDispatchReceipt.ExecuteIntentDigest.IsEmpty() || StageGExecuteDispatchReceipt.ExecuteIntentDigest != StageGExecuteIntent.ExecuteIntentDigest)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("execute_intent_digest_mismatch_or_missing");
		return FinalizeAndReturn();
	}
	if (StageGExecuteDispatchReceipt.StageGWriteEnableDigest.IsEmpty() || StageGExecuteDispatchReceipt.StageGWriteEnableDigest != StageGWriteEnableRequest.StageGWriteEnableDigest)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("stage_g_write_enable_digest_mismatch_or_missing");
		return FinalizeAndReturn();
	}
	if (StageGExecuteDispatchReceipt.StageGExecutePermitDigest.IsEmpty() || StageGExecuteDispatchReceipt.StageGExecutePermitDigest != StageGExecutePermitTicket.StageGExecutePermitDigest)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("stage_g_execute_permit_digest_mismatch_or_missing");
		return FinalizeAndReturn();
	}
	if (StageGExecuteDispatchReceipt.StageGExecuteDispatchDigest.IsEmpty() ||
		StageGExecuteDispatchReceipt.StageGExecuteDispatchDigest != StageGExecuteDispatchRequest.StageGExecuteDispatchDigest)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("stage_g_execute_dispatch_digest_mismatch_or_missing");
		return FinalizeAndReturn();
	}
	if (StageGExecuteDispatchReceipt.StageGExecuteDispatchReceiptDigest.IsEmpty())
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("stage_g_execute_dispatch_receipt_digest_missing");
		return FinalizeAndReturn();
	}

	if (!StageGExecuteDispatchReceipt.ExecuteTarget.Equals(TEXT("stage_g_execute_runtime"), ESearchCase::IgnoreCase))
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("execute_target_mismatch");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteDispatchReceipt.HandoffTarget.Equals(TEXT("stage_g_execute"), ESearchCase::IgnoreCase))
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("handoff_target_mismatch");
		return FinalizeAndReturn();
	}

	OutRequest.StageGExecuteCommitRequestStatus = TEXT("ready");
	OutRequest.bWriteEnabled = true;
	OutRequest.bReadyForStageGExecute = true;
	OutRequest.bStageGExecutePermitReady = true;
	OutRequest.bStageGExecuteDispatchReady = true;
	OutRequest.bStageGExecuteDispatchAccepted = true;
	OutRequest.bStageGExecuteDispatchReceiptReady = true;
	OutRequest.bStageGExecuteCommitRequestReady = true;
	OutRequest.ErrorCode = TEXT("-");
	OutRequest.Reason = TEXT("stage_g_execute_commit_request_ready");
	return FinalizeAndReturn();
}



