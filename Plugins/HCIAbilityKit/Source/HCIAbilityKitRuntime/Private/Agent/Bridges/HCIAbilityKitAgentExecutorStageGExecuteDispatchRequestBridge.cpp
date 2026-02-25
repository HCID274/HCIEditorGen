#include "Agent/Bridges/HCIAbilityKitAgentExecutorStageGExecuteDispatchRequestBridge.h"

#include "Agent/Contracts/StageF/HCIAbilityKitAgentApplyConfirmRequest.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentApplyRequest.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentExecuteTicket.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentSimulateExecuteArchiveBundle.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentSimulateExecuteFinalReport.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentSimulateExecuteHandoffEnvelope.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentSimulateExecuteReceipt.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGExecuteDispatchRequest.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGExecuteIntent.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGExecutePermitTicket.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGWriteEnableRequest.h"
#include "Agent/Executor/HCIAbilityKitDryRunDiff.h"
#include "Common/HCIAbilityKitTimeFormat.h"
#include "Misc/Crc.h"
#include "Misc/Guid.h"

namespace
{
static FString HCI_BuildSelectionDigestFromReviewReport_G4(const FHCIAbilityKitDryRunDiffReport& Report)
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

static FString HCI_BuildStageGExecuteDispatchDigest_G4(const FHCIAbilityKitAgentStageGExecuteDispatchRequest& Request)
{
	FString Canonical;
	Canonical += FString::Printf(
		TEXT("%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s\n"),
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
		*Request.StageGExecuteDispatchStatus);

	Canonical += FString::Printf(
		TEXT("%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s\n"),
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
		Request.bStageGExecuteDispatchReady ? TEXT("1") : TEXT("0"));

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

static void HCI_CopyPermitTicketToDispatchRequest_G4(
	const FHCIAbilityKitAgentStageGExecutePermitTicket& InTicket,
	FHCIAbilityKitAgentStageGExecuteDispatchRequest& OutRequest)
{
	OutRequest = FHCIAbilityKitAgentStageGExecuteDispatchRequest();
	OutRequest.RequestId = FString::Printf(TEXT("stagegdispatch_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	OutRequest.StageGExecutePermitTicketId = InTicket.RequestId;
	OutRequest.StageGWriteEnableRequestId = InTicket.StageGWriteEnableRequestId;
	OutRequest.StageGExecuteIntentId = InTicket.StageGExecuteIntentId;
	OutRequest.SimHandoffEnvelopeId = InTicket.SimHandoffEnvelopeId;
	OutRequest.SimArchiveBundleId = InTicket.SimArchiveBundleId;
	OutRequest.SimFinalReportId = InTicket.SimFinalReportId;
	OutRequest.SimExecuteReceiptId = InTicket.SimExecuteReceiptId;
	OutRequest.ExecuteTicketId = InTicket.ExecuteTicketId;
	OutRequest.ConfirmRequestId = InTicket.ConfirmRequestId;
	OutRequest.ApplyRequestId = InTicket.ApplyRequestId;
	OutRequest.ReviewRequestId = InTicket.ReviewRequestId;
	OutRequest.SelectionDigest = InTicket.SelectionDigest;
	OutRequest.ArchiveDigest = InTicket.ArchiveDigest;
	OutRequest.HandoffDigest = InTicket.HandoffDigest;
	OutRequest.ExecuteIntentDigest = InTicket.ExecuteIntentDigest;
	OutRequest.StageGWriteEnableDigest = InTicket.StageGWriteEnableDigest;
	OutRequest.StageGExecutePermitDigest = InTicket.StageGExecutePermitDigest;
	OutRequest.GeneratedUtc = FHCIAbilityKitTimeFormat::FormatNowBeijingIso8601();
	OutRequest.ExecutionMode = TEXT("stage_g_execute_dispatch_request_dry_run");
	OutRequest.ExecuteTarget = InTicket.ExecuteTarget;
	OutRequest.HandoffTarget = InTicket.HandoffTarget;
	OutRequest.TransactionMode = InTicket.TransactionMode;
	OutRequest.TerminationPolicy = InTicket.TerminationPolicy;
	OutRequest.TerminalStatus = InTicket.TerminalStatus;
	OutRequest.ArchiveStatus = InTicket.ArchiveStatus;
	OutRequest.HandoffStatus = InTicket.HandoffStatus;
	OutRequest.StageGStatus = InTicket.StageGStatus;
	OutRequest.StageGWriteStatus = InTicket.StageGWriteStatus;
	OutRequest.StageGExecutePermitStatus = InTicket.StageGExecutePermitStatus;
	OutRequest.StageGExecuteDispatchStatus = TEXT("blocked");
	OutRequest.bUserConfirmed = InTicket.bUserConfirmed;
	OutRequest.bReadyToSimulateExecute = InTicket.bReadyToSimulateExecute;
	OutRequest.bSimulatedDispatchAccepted = InTicket.bSimulatedDispatchAccepted;
	OutRequest.bSimulationCompleted = InTicket.bSimulationCompleted;
	OutRequest.bArchiveReady = InTicket.bArchiveReady;
	OutRequest.bHandoffReady = InTicket.bHandoffReady;
	OutRequest.bWriteEnabled = InTicket.bWriteEnabled;
	OutRequest.bReadyForStageGEntry = InTicket.bReadyForStageGEntry;
	OutRequest.bWriteEnableConfirmed = InTicket.bWriteEnableConfirmed;
	OutRequest.bReadyForStageGExecute = InTicket.bReadyForStageGExecute;
	OutRequest.bStageGExecutePermitReady = InTicket.bStageGExecutePermitReady;
	OutRequest.bExecuteDispatchConfirmed = false;
	OutRequest.bStageGExecuteDispatchReady = false;
	OutRequest.ErrorCode = TEXT("-");
	OutRequest.Reason = TEXT("stage_g_execute_dispatch_request_ready");
	OutRequest.Summary = InTicket.Summary;
	OutRequest.Items = InTicket.Items;
}
} // namespace

bool FHCIAbilityKitAgentExecutorStageGExecuteDispatchRequestBridge::BuildStageGExecuteDispatchRequest(
	const FHCIAbilityKitAgentStageGExecutePermitTicket& StageGExecutePermitTicket,
	const FString& ExpectedStageGExecutePermitTicketId,
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
	bool bExecuteDispatchConfirmed,
	FHCIAbilityKitAgentStageGExecuteDispatchRequest& OutRequest)
{
	HCI_CopyPermitTicketToDispatchRequest_G4(StageGExecutePermitTicket, OutRequest);
	OutRequest.bExecuteDispatchConfirmed = bExecuteDispatchConfirmed;

	auto FinalizeAndReturn = [&OutRequest]() -> bool
	{
		OutRequest.StageGExecuteDispatchDigest = HCI_BuildStageGExecuteDispatchDigest_G4(OutRequest);
		return true;
	};

	if (!ExpectedStageGExecutePermitTicketId.IsEmpty() && StageGExecutePermitTicket.RequestId != ExpectedStageGExecutePermitTicketId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("stage_g_execute_permit_ticket_id_mismatch");
		return FinalizeAndReturn();
	}

	if (!StageGExecutePermitTicket.bUserConfirmed)
	{
		OutRequest.ErrorCode = TEXT("E4005");
		OutRequest.Reason = TEXT("user_not_confirmed");
		return FinalizeAndReturn();
	}
	if (!StageGExecutePermitTicket.bWriteEnableConfirmed)
	{
		OutRequest.ErrorCode = TEXT("E4005");
		OutRequest.Reason = TEXT("stage_g_write_enable_not_confirmed");
		return FinalizeAndReturn();
	}
	if (!bExecuteDispatchConfirmed)
	{
		OutRequest.ErrorCode = TEXT("E4005");
		OutRequest.Reason = TEXT("stage_g_execute_dispatch_not_confirmed");
		return FinalizeAndReturn();
	}

	if (!StageGExecutePermitTicket.bStageGExecutePermitReady ||
		!StageGExecutePermitTicket.bReadyForStageGExecute ||
		!StageGExecutePermitTicket.bWriteEnabled ||
		!StageGExecutePermitTicket.StageGExecutePermitStatus.Equals(TEXT("ready"), ESearchCase::IgnoreCase))
	{
		OutRequest.ErrorCode = TEXT("E4212");
		OutRequest.Reason = TEXT("stage_g_execute_permit_ticket_not_ready");
		return FinalizeAndReturn();
	}

	if (!StageGExecutePermitTicket.bReadyForStageGEntry ||
		!StageGExecutePermitTicket.StageGStatus.Equals(TEXT("ready"), ESearchCase::IgnoreCase))
	{
		OutRequest.ErrorCode = TEXT("E4210");
		OutRequest.Reason = TEXT("stage_g_execute_intent_not_ready");
		return FinalizeAndReturn();
	}
	if (!StageGExecutePermitTicket.bReadyToSimulateExecute)
	{
		OutRequest.ErrorCode = TEXT("E4205");
		OutRequest.Reason = TEXT("execute_ticket_not_ready");
		return FinalizeAndReturn();
	}
	if (!StageGExecutePermitTicket.bSimulatedDispatchAccepted)
	{
		OutRequest.ErrorCode = TEXT("E4206");
		OutRequest.Reason = TEXT("simulate_execute_receipt_not_accepted");
		return FinalizeAndReturn();
	}
	if (!StageGExecutePermitTicket.bSimulationCompleted || !StageGExecutePermitTicket.TerminalStatus.Equals(TEXT("completed"), ESearchCase::IgnoreCase))
	{
		OutRequest.ErrorCode = TEXT("E4207");
		OutRequest.Reason = TEXT("simulate_final_report_not_completed");
		return FinalizeAndReturn();
	}
	if (!StageGExecutePermitTicket.bArchiveReady || !StageGExecutePermitTicket.ArchiveStatus.Equals(TEXT("ready"), ESearchCase::IgnoreCase))
	{
		OutRequest.ErrorCode = TEXT("E4208");
		OutRequest.Reason = TEXT("sim_archive_bundle_not_ready");
		return FinalizeAndReturn();
	}
	if (!StageGExecutePermitTicket.bHandoffReady || !StageGExecutePermitTicket.HandoffStatus.Equals(TEXT("ready"), ESearchCase::IgnoreCase))
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

	if (StageGExecutePermitTicket.StageGWriteEnableRequestId != StageGWriteEnableRequest.RequestId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("stage_g_write_enable_request_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecutePermitTicket.StageGExecuteIntentId != StageGExecuteIntent.RequestId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("stage_g_execute_intent_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecutePermitTicket.SimHandoffEnvelopeId != SimHandoffEnvelope.RequestId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("sim_handoff_envelope_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecutePermitTicket.SimArchiveBundleId != SimArchiveBundle.RequestId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("sim_archive_bundle_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecutePermitTicket.SimFinalReportId != SimFinalReport.RequestId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("sim_final_report_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecutePermitTicket.SimExecuteReceiptId != SimExecuteReceipt.RequestId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("sim_execute_receipt_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecutePermitTicket.ExecuteTicketId != CurrentExecuteTicket.RequestId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("execute_ticket_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecutePermitTicket.ConfirmRequestId != CurrentConfirmRequest.RequestId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("confirm_request_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecutePermitTicket.ApplyRequestId != CurrentApplyRequest.RequestId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("apply_request_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecutePermitTicket.ReviewRequestId != CurrentReviewReport.RequestId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("review_request_id_mismatch");
		return FinalizeAndReturn();
	}

	if (StageGExecutePermitTicket.SelectionDigest != StageGWriteEnableRequest.SelectionDigest ||
		StageGExecutePermitTicket.SelectionDigest != StageGExecuteIntent.SelectionDigest ||
		StageGExecutePermitTicket.SelectionDigest != SimHandoffEnvelope.SelectionDigest ||
		StageGExecutePermitTicket.SelectionDigest != SimArchiveBundle.SelectionDigest ||
		StageGExecutePermitTicket.SelectionDigest != SimFinalReport.SelectionDigest ||
		StageGExecutePermitTicket.SelectionDigest != SimExecuteReceipt.SelectionDigest ||
		StageGExecutePermitTicket.SelectionDigest != CurrentExecuteTicket.SelectionDigest ||
		StageGExecutePermitTicket.SelectionDigest != CurrentConfirmRequest.SelectionDigest ||
		StageGExecutePermitTicket.SelectionDigest != CurrentApplyRequest.SelectionDigest)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("selection_digest_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecutePermitTicket.SelectionDigest != HCI_BuildSelectionDigestFromReviewReport_G4(CurrentReviewReport))
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("selection_digest_mismatch");
		return FinalizeAndReturn();
	}

	if (StageGExecutePermitTicket.ArchiveDigest.IsEmpty() || StageGExecutePermitTicket.ArchiveDigest != SimArchiveBundle.ArchiveDigest)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("archive_digest_mismatch_or_missing");
		return FinalizeAndReturn();
	}
	if (StageGExecutePermitTicket.HandoffDigest.IsEmpty() || StageGExecutePermitTicket.HandoffDigest != SimHandoffEnvelope.HandoffDigest)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("handoff_digest_mismatch_or_missing");
		return FinalizeAndReturn();
	}
	if (StageGExecutePermitTicket.ExecuteIntentDigest.IsEmpty() || StageGExecutePermitTicket.ExecuteIntentDigest != StageGExecuteIntent.ExecuteIntentDigest)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("execute_intent_digest_mismatch_or_missing");
		return FinalizeAndReturn();
	}
	if (StageGExecutePermitTicket.StageGWriteEnableDigest.IsEmpty() || StageGExecutePermitTicket.StageGWriteEnableDigest != StageGWriteEnableRequest.StageGWriteEnableDigest)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("stage_g_write_enable_digest_mismatch_or_missing");
		return FinalizeAndReturn();
	}
	if (StageGExecutePermitTicket.StageGExecutePermitDigest.IsEmpty())
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("stage_g_execute_permit_digest_missing");
		return FinalizeAndReturn();
	}

	if (!StageGExecutePermitTicket.ExecuteTarget.Equals(TEXT("stage_g_execute_runtime"), ESearchCase::IgnoreCase))
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("execute_target_mismatch");
		return FinalizeAndReturn();
	}
	if (!StageGExecutePermitTicket.HandoffTarget.Equals(TEXT("stage_g_execute"), ESearchCase::IgnoreCase))
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("handoff_target_mismatch");
		return FinalizeAndReturn();
	}

	OutRequest.StageGExecuteDispatchStatus = TEXT("ready");
	OutRequest.bWriteEnabled = true;
	OutRequest.bReadyForStageGExecute = true;
	OutRequest.bStageGExecutePermitReady = true;
	OutRequest.bStageGExecuteDispatchReady = true;
	OutRequest.ErrorCode = TEXT("-");
	OutRequest.Reason = TEXT("stage_g_execute_dispatch_request_ready");
	return FinalizeAndReturn();
}