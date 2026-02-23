#include "Agent/HCIAbilityKitAgentExecutorStageGWriteEnableRequestBridge.h"

#include "Agent/HCIAbilityKitAgentApplyConfirmRequest.h"
#include "Agent/HCIAbilityKitAgentApplyRequest.h"
#include "Agent/HCIAbilityKitAgentExecuteTicket.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteArchiveBundle.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteFinalReport.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteHandoffEnvelope.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteReceipt.h"
#include "Agent/HCIAbilityKitAgentStageGExecuteIntent.h"
#include "Agent/HCIAbilityKitAgentStageGWriteEnableRequest.h"
#include "Agent/HCIAbilityKitDryRunDiff.h"
#include "Common/HCIAbilityKitTimeFormat.h"
#include "Misc/Crc.h"
#include "Misc/Guid.h"

namespace
{
static FString HCI_BuildSelectionDigestFromReviewReport_G2(const FHCIAbilityKitDryRunDiffReport& Report)
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

static FString HCI_BuildStageGWriteEnableDigest_G2(const FHCIAbilityKitAgentStageGWriteEnableRequest& Request)
{
	FString Canonical;
	Canonical += FString::Printf(
		TEXT("%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s\n"),
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
		*Request.TerminalStatus,
		*Request.ArchiveStatus,
		*Request.HandoffStatus,
		*Request.StageGStatus,
		*Request.StageGWriteStatus,
		Request.bUserConfirmed ? TEXT("1") : TEXT("0"),
		Request.bWriteEnableConfirmed ? TEXT("1") : TEXT("0"));

	Canonical += FString::Printf(
		TEXT("%s|%s|%s|%s|%s|%s|%s|%s|%s\n"),
		Request.bReadyToSimulateExecute ? TEXT("1") : TEXT("0"),
		Request.bSimulatedDispatchAccepted ? TEXT("1") : TEXT("0"),
		Request.bSimulationCompleted ? TEXT("1") : TEXT("0"),
		Request.bArchiveReady ? TEXT("1") : TEXT("0"),
		Request.bHandoffReady ? TEXT("1") : TEXT("0"),
		Request.bWriteEnabled ? TEXT("1") : TEXT("0"),
		Request.bReadyForStageGEntry ? TEXT("1") : TEXT("0"),
		Request.bReadyForStageGExecute ? TEXT("1") : TEXT("0"),
		*Request.ExecuteTarget);

	Canonical += FString::Printf(TEXT("%s|%s\n"), *Request.HandoffTarget, *Request.Reason);

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

static void HCI_CopyStageGIntentToWriteEnableRequest_G2(
	const FHCIAbilityKitAgentStageGExecuteIntent& Intent,
	const bool bWriteEnableConfirmed,
	FHCIAbilityKitAgentStageGWriteEnableRequest& OutRequest)
{
	OutRequest = FHCIAbilityKitAgentStageGWriteEnableRequest();
	OutRequest.RequestId = FString::Printf(TEXT("stagegwrite_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	OutRequest.StageGExecuteIntentId = Intent.RequestId;
	OutRequest.SimHandoffEnvelopeId = Intent.SimHandoffEnvelopeId;
	OutRequest.SimArchiveBundleId = Intent.SimArchiveBundleId;
	OutRequest.SimFinalReportId = Intent.SimFinalReportId;
	OutRequest.SimExecuteReceiptId = Intent.SimExecuteReceiptId;
	OutRequest.ExecuteTicketId = Intent.ExecuteTicketId;
	OutRequest.ConfirmRequestId = Intent.ConfirmRequestId;
	OutRequest.ApplyRequestId = Intent.ApplyRequestId;
	OutRequest.ReviewRequestId = Intent.ReviewRequestId;
	OutRequest.SelectionDigest = Intent.SelectionDigest;
	OutRequest.ArchiveDigest = Intent.ArchiveDigest;
	OutRequest.HandoffDigest = Intent.HandoffDigest;
	OutRequest.ExecuteIntentDigest = Intent.ExecuteIntentDigest;
	OutRequest.GeneratedUtc = FHCIAbilityKitTimeFormat::FormatNowBeijingIso8601();
	OutRequest.ExecutionMode = TEXT("stage_g_write_enable_request_dry_run");
	OutRequest.ExecuteTarget = Intent.ExecuteTarget;
	OutRequest.HandoffTarget = Intent.HandoffTarget;
	OutRequest.TransactionMode = Intent.TransactionMode;
	OutRequest.TerminationPolicy = Intent.TerminationPolicy;
	OutRequest.TerminalStatus = Intent.TerminalStatus;
	OutRequest.ArchiveStatus = Intent.ArchiveStatus;
	OutRequest.HandoffStatus = Intent.HandoffStatus;
	OutRequest.StageGStatus = Intent.StageGStatus;
	OutRequest.StageGWriteStatus = TEXT("blocked");
	OutRequest.bUserConfirmed = Intent.bUserConfirmed;
	OutRequest.bReadyToSimulateExecute = Intent.bReadyToSimulateExecute;
	OutRequest.bSimulatedDispatchAccepted = Intent.bSimulatedDispatchAccepted;
	OutRequest.bSimulationCompleted = Intent.bSimulationCompleted;
	OutRequest.bArchiveReady = Intent.bArchiveReady;
	OutRequest.bHandoffReady = Intent.bHandoffReady;
	OutRequest.bWriteEnabled = false;
	OutRequest.bReadyForStageGEntry = Intent.bReadyForStageGEntry;
	OutRequest.bWriteEnableConfirmed = bWriteEnableConfirmed;
	OutRequest.bReadyForStageGExecute = false;
	OutRequest.ErrorCode = TEXT("-");
	OutRequest.Reason = TEXT("stage_g_write_enable_request_ready");
	OutRequest.Summary = Intent.Summary;
	OutRequest.Items = Intent.Items;
}
} // namespace

bool FHCIAbilityKitAgentExecutorStageGWriteEnableRequestBridge::BuildStageGWriteEnableRequest(
	const FHCIAbilityKitAgentStageGExecuteIntent& StageGExecuteIntent,
	const FString& ExpectedStageGExecuteIntentId,
	const FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope& SimHandoffEnvelope,
	const FHCIAbilityKitAgentSimulateExecuteArchiveBundle& SimArchiveBundle,
	const FHCIAbilityKitAgentSimulateExecuteFinalReport& SimFinalReport,
	const FHCIAbilityKitAgentSimulateExecuteReceipt& SimExecuteReceipt,
	const FHCIAbilityKitAgentExecuteTicket& CurrentExecuteTicket,
	const FHCIAbilityKitAgentApplyConfirmRequest& CurrentConfirmRequest,
	const FHCIAbilityKitAgentApplyRequest& CurrentApplyRequest,
	const FHCIAbilityKitDryRunDiffReport& CurrentReviewReport,
	const bool bWriteEnableConfirmed,
	FHCIAbilityKitAgentStageGWriteEnableRequest& OutRequest)
{
	HCI_CopyStageGIntentToWriteEnableRequest_G2(StageGExecuteIntent, bWriteEnableConfirmed, OutRequest);

	if (!ExpectedStageGExecuteIntentId.IsEmpty() && StageGExecuteIntent.RequestId != ExpectedStageGExecuteIntentId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("stage_g_execute_intent_id_mismatch");
		OutRequest.StageGWriteEnableDigest = HCI_BuildStageGWriteEnableDigest_G2(OutRequest);
		return true;
	}

	if (!StageGExecuteIntent.bUserConfirmed)
	{
		OutRequest.ErrorCode = TEXT("E4005");
		OutRequest.Reason = TEXT("user_not_confirmed");
		OutRequest.StageGWriteEnableDigest = HCI_BuildStageGWriteEnableDigest_G2(OutRequest);
		return true;
	}

	if (!bWriteEnableConfirmed)
	{
		OutRequest.ErrorCode = TEXT("E4005");
		OutRequest.Reason = TEXT("stage_g_write_enable_not_confirmed");
		OutRequest.StageGWriteEnableDigest = HCI_BuildStageGWriteEnableDigest_G2(OutRequest);
		return true;
	}

	if (!StageGExecuteIntent.bReadyForStageGEntry || !StageGExecuteIntent.StageGStatus.Equals(TEXT("ready"), ESearchCase::IgnoreCase))
	{
		OutRequest.ErrorCode = TEXT("E4210");
		OutRequest.Reason = TEXT("stage_g_execute_intent_not_ready");
		OutRequest.StageGWriteEnableDigest = HCI_BuildStageGWriteEnableDigest_G2(OutRequest);
		return true;
	}

	if (!StageGExecuteIntent.bReadyToSimulateExecute)
	{
		OutRequest.ErrorCode = TEXT("E4205");
		OutRequest.Reason = TEXT("execute_ticket_not_ready");
		OutRequest.StageGWriteEnableDigest = HCI_BuildStageGWriteEnableDigest_G2(OutRequest);
		return true;
	}

	if (!StageGExecuteIntent.bSimulatedDispatchAccepted)
	{
		OutRequest.ErrorCode = TEXT("E4206");
		OutRequest.Reason = TEXT("simulate_execute_receipt_not_accepted");
		OutRequest.StageGWriteEnableDigest = HCI_BuildStageGWriteEnableDigest_G2(OutRequest);
		return true;
	}

	if (!StageGExecuteIntent.bSimulationCompleted || !StageGExecuteIntent.TerminalStatus.Equals(TEXT("completed"), ESearchCase::IgnoreCase))
	{
		OutRequest.ErrorCode = TEXT("E4207");
		OutRequest.Reason = TEXT("simulate_final_report_not_completed");
		OutRequest.StageGWriteEnableDigest = HCI_BuildStageGWriteEnableDigest_G2(OutRequest);
		return true;
	}

	if (!StageGExecuteIntent.bArchiveReady || !StageGExecuteIntent.ArchiveStatus.Equals(TEXT("ready"), ESearchCase::IgnoreCase))
	{
		OutRequest.ErrorCode = TEXT("E4208");
		OutRequest.Reason = TEXT("sim_archive_bundle_not_ready");
		OutRequest.StageGWriteEnableDigest = HCI_BuildStageGWriteEnableDigest_G2(OutRequest);
		return true;
	}

	if (!StageGExecuteIntent.bHandoffReady || !StageGExecuteIntent.HandoffStatus.Equals(TEXT("ready"), ESearchCase::IgnoreCase))
	{
		OutRequest.ErrorCode = TEXT("E4209");
		OutRequest.Reason = TEXT("sim_handoff_envelope_not_ready");
		OutRequest.StageGWriteEnableDigest = HCI_BuildStageGWriteEnableDigest_G2(OutRequest);
		return true;
	}

	if (!CurrentExecuteTicket.bReadyToSimulateExecute)
	{
		OutRequest.ErrorCode = TEXT("E4205");
		OutRequest.Reason = TEXT("execute_ticket_not_ready");
		OutRequest.StageGWriteEnableDigest = HCI_BuildStageGWriteEnableDigest_G2(OutRequest);
		return true;
	}

	if (!CurrentConfirmRequest.bReadyToExecute)
	{
		OutRequest.ErrorCode = TEXT("E4204");
		OutRequest.Reason = TEXT("confirm_request_not_ready");
		OutRequest.StageGWriteEnableDigest = HCI_BuildStageGWriteEnableDigest_G2(OutRequest);
		return true;
	}

	if (StageGExecuteIntent.SimHandoffEnvelopeId != SimHandoffEnvelope.RequestId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("sim_handoff_envelope_id_mismatch");
		OutRequest.StageGWriteEnableDigest = HCI_BuildStageGWriteEnableDigest_G2(OutRequest);
		return true;
	}
	if (StageGExecuteIntent.SimArchiveBundleId != SimArchiveBundle.RequestId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("sim_archive_bundle_id_mismatch");
		OutRequest.StageGWriteEnableDigest = HCI_BuildStageGWriteEnableDigest_G2(OutRequest);
		return true;
	}
	if (StageGExecuteIntent.SimFinalReportId != SimFinalReport.RequestId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("sim_final_report_id_mismatch");
		OutRequest.StageGWriteEnableDigest = HCI_BuildStageGWriteEnableDigest_G2(OutRequest);
		return true;
	}
	if (StageGExecuteIntent.SimExecuteReceiptId != SimExecuteReceipt.RequestId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("sim_execute_receipt_id_mismatch");
		OutRequest.StageGWriteEnableDigest = HCI_BuildStageGWriteEnableDigest_G2(OutRequest);
		return true;
	}
	if (StageGExecuteIntent.ExecuteTicketId != CurrentExecuteTicket.RequestId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("execute_ticket_id_mismatch");
		OutRequest.StageGWriteEnableDigest = HCI_BuildStageGWriteEnableDigest_G2(OutRequest);
		return true;
	}
	if (StageGExecuteIntent.ConfirmRequestId != CurrentConfirmRequest.RequestId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("confirm_request_id_mismatch");
		OutRequest.StageGWriteEnableDigest = HCI_BuildStageGWriteEnableDigest_G2(OutRequest);
		return true;
	}
	if (StageGExecuteIntent.ApplyRequestId != CurrentApplyRequest.RequestId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("apply_request_id_mismatch");
		OutRequest.StageGWriteEnableDigest = HCI_BuildStageGWriteEnableDigest_G2(OutRequest);
		return true;
	}
	if (StageGExecuteIntent.ReviewRequestId != CurrentReviewReport.RequestId)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("review_request_id_mismatch");
		OutRequest.StageGWriteEnableDigest = HCI_BuildStageGWriteEnableDigest_G2(OutRequest);
		return true;
	}

	if (StageGExecuteIntent.SelectionDigest != SimHandoffEnvelope.SelectionDigest ||
		StageGExecuteIntent.SelectionDigest != SimArchiveBundle.SelectionDigest ||
		StageGExecuteIntent.SelectionDigest != SimFinalReport.SelectionDigest ||
		StageGExecuteIntent.SelectionDigest != SimExecuteReceipt.SelectionDigest ||
		StageGExecuteIntent.SelectionDigest != CurrentExecuteTicket.SelectionDigest ||
		StageGExecuteIntent.SelectionDigest != CurrentConfirmRequest.SelectionDigest ||
		StageGExecuteIntent.SelectionDigest != CurrentApplyRequest.SelectionDigest)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("selection_digest_mismatch");
		OutRequest.StageGWriteEnableDigest = HCI_BuildStageGWriteEnableDigest_G2(OutRequest);
		return true;
	}

	if (StageGExecuteIntent.SelectionDigest != HCI_BuildSelectionDigestFromReviewReport_G2(CurrentReviewReport))
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("selection_digest_mismatch");
		OutRequest.StageGWriteEnableDigest = HCI_BuildStageGWriteEnableDigest_G2(OutRequest);
		return true;
	}

	if (StageGExecuteIntent.ArchiveDigest.IsEmpty() || StageGExecuteIntent.ArchiveDigest != SimArchiveBundle.ArchiveDigest)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("archive_digest_mismatch_or_missing");
		OutRequest.StageGWriteEnableDigest = HCI_BuildStageGWriteEnableDigest_G2(OutRequest);
		return true;
	}

	if (StageGExecuteIntent.HandoffDigest.IsEmpty() || StageGExecuteIntent.HandoffDigest != SimHandoffEnvelope.HandoffDigest)
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("handoff_digest_mismatch_or_missing");
		OutRequest.StageGWriteEnableDigest = HCI_BuildStageGWriteEnableDigest_G2(OutRequest);
		return true;
	}

	if (!StageGExecuteIntent.ExecuteTarget.Equals(TEXT("stage_g_execute_runtime"), ESearchCase::IgnoreCase))
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("execute_target_mismatch");
		OutRequest.StageGWriteEnableDigest = HCI_BuildStageGWriteEnableDigest_G2(OutRequest);
		return true;
	}

	if (!StageGExecuteIntent.HandoffTarget.Equals(TEXT("stage_g_execute"), ESearchCase::IgnoreCase))
	{
		OutRequest.ErrorCode = TEXT("E4202");
		OutRequest.Reason = TEXT("handoff_target_mismatch");
		OutRequest.StageGWriteEnableDigest = HCI_BuildStageGWriteEnableDigest_G2(OutRequest);
		return true;
	}

	OutRequest.StageGWriteStatus = TEXT("ready");
	OutRequest.bWriteEnabled = true;
	OutRequest.bReadyForStageGExecute = true;
	OutRequest.ErrorCode = TEXT("-");
	OutRequest.Reason = TEXT("stage_g_write_enable_request_ready");
	OutRequest.StageGWriteEnableDigest = HCI_BuildStageGWriteEnableDigest_G2(OutRequest);
	return true;
}
