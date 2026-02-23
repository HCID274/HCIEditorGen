#include "Agent/HCIAbilityKitAgentExecutorStageGExecuteIntentBridge.h"

#include "Agent/HCIAbilityKitAgentApplyConfirmRequest.h"
#include "Agent/HCIAbilityKitAgentApplyRequest.h"
#include "Agent/HCIAbilityKitAgentExecuteTicket.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteArchiveBundle.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteFinalReport.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteHandoffEnvelope.h"
#include "Agent/HCIAbilityKitAgentStageGExecuteIntent.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteReceipt.h"
#include "Agent/HCIAbilityKitDryRunDiff.h"
#include "Common/HCIAbilityKitTimeFormat.h"
#include "Misc/Crc.h"
#include "Misc/Guid.h"

namespace
{
static FString HCI_BuildSelectionDigestFromReviewReport_G1(const FHCIAbilityKitDryRunDiffReport& Report)
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

	const uint32 Crc = FCrc::StrCrc32(*Canonical);
	return FString::Printf(TEXT("crc32_%08X"), Crc);
}

static FString HCI_BuildStageGExecuteIntentDigest_G1(const FHCIAbilityKitAgentStageGExecuteIntent& Intent)
{
	FString Canonical;
	Canonical += FString::Printf(
		TEXT("%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s\n"),
		*Intent.SimHandoffEnvelopeId,
		*Intent.SimArchiveBundleId,
		*Intent.SimFinalReportId,
		*Intent.SimExecuteReceiptId,
		*Intent.ExecuteTicketId,
		*Intent.ConfirmRequestId,
		*Intent.ApplyRequestId,
		*Intent.ReviewRequestId,
		*Intent.SelectionDigest,
		*Intent.ArchiveDigest,
		*Intent.HandoffDigest,
		*Intent.TerminalStatus,
		*Intent.ArchiveStatus,
		*Intent.HandoffStatus,
		*Intent.StageGStatus,
		Intent.bUserConfirmed ? TEXT("1") : TEXT("0"),
		Intent.bReadyToSimulateExecute ? TEXT("1") : TEXT("0"),
		Intent.bSimulatedDispatchAccepted ? TEXT("1") : TEXT("0"),
		Intent.bSimulationCompleted ? TEXT("1") : TEXT("0"),
		Intent.bArchiveReady ? TEXT("1") : TEXT("0"));

	Canonical += FString::Printf(
		TEXT("%s|%s|%s|%s|%s\n"),
		Intent.bHandoffReady ? TEXT("1") : TEXT("0"),
		Intent.bWriteEnabled ? TEXT("1") : TEXT("0"),
		Intent.bReadyForStageGEntry ? TEXT("1") : TEXT("0"),
		*Intent.HandoffTarget,
		*Intent.ExecuteTarget);

	for (const FHCIAbilityKitAgentApplyRequestItem& Item : Intent.Items)
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

	const uint32 Crc = FCrc::StrCrc32(*Canonical);
	return FString::Printf(TEXT("crc32_%08X"), Crc);
}

static void HCI_CopyHandoffEnvelopeToStageGIntent(
	const FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope& HandoffEnvelope,
	FHCIAbilityKitAgentStageGExecuteIntent& OutIntent)
{
	OutIntent = FHCIAbilityKitAgentStageGExecuteIntent();
	OutIntent.RequestId = FString::Printf(TEXT("stagegintent_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	OutIntent.SimHandoffEnvelopeId = HandoffEnvelope.RequestId;
	OutIntent.SimArchiveBundleId = HandoffEnvelope.SimArchiveBundleId;
	OutIntent.SimFinalReportId = HandoffEnvelope.SimFinalReportId;
	OutIntent.SimExecuteReceiptId = HandoffEnvelope.SimExecuteReceiptId;
	OutIntent.ExecuteTicketId = HandoffEnvelope.ExecuteTicketId;
	OutIntent.ConfirmRequestId = HandoffEnvelope.ConfirmRequestId;
	OutIntent.ApplyRequestId = HandoffEnvelope.ApplyRequestId;
	OutIntent.ReviewRequestId = HandoffEnvelope.ReviewRequestId;
	OutIntent.SelectionDigest = HandoffEnvelope.SelectionDigest;
	OutIntent.ArchiveDigest = HandoffEnvelope.ArchiveDigest;
	OutIntent.HandoffDigest = HandoffEnvelope.HandoffDigest;
	OutIntent.GeneratedUtc = FHCIAbilityKitTimeFormat::FormatNowBeijingIso8601();
	OutIntent.ExecutionMode = TEXT("stage_g_validate_only");
	OutIntent.ExecuteTarget = TEXT("stage_g_execute_runtime");
	OutIntent.HandoffTarget = HandoffEnvelope.HandoffTarget;
	OutIntent.TransactionMode = HandoffEnvelope.TransactionMode;
	OutIntent.TerminationPolicy = HandoffEnvelope.TerminationPolicy;
	OutIntent.TerminalStatus = HandoffEnvelope.TerminalStatus;
	OutIntent.ArchiveStatus = HandoffEnvelope.ArchiveStatus;
	OutIntent.HandoffStatus = HandoffEnvelope.HandoffStatus;
	OutIntent.StageGStatus = TEXT("blocked");
	OutIntent.bUserConfirmed = HandoffEnvelope.bUserConfirmed;
	OutIntent.bReadyToSimulateExecute = HandoffEnvelope.bReadyToSimulateExecute;
	OutIntent.bSimulatedDispatchAccepted = HandoffEnvelope.bSimulatedDispatchAccepted;
	OutIntent.bSimulationCompleted = HandoffEnvelope.bSimulationCompleted;
	OutIntent.bArchiveReady = HandoffEnvelope.bArchiveReady;
	OutIntent.bHandoffReady = HandoffEnvelope.bHandoffReady;
	OutIntent.bWriteEnabled = false;
	OutIntent.bReadyForStageGEntry = false;
	OutIntent.ErrorCode = TEXT("-");
	OutIntent.Reason = TEXT("stage_g_execute_intent_ready_validate_only");
	OutIntent.Summary = HandoffEnvelope.Summary;
	OutIntent.Items = HandoffEnvelope.Items;
}
} // namespace

bool FHCIAbilityKitAgentExecutorStageGExecuteIntentBridge::BuildStageGExecuteIntent(
	const FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope& SimHandoffEnvelope,
	const FHCIAbilityKitAgentSimulateExecuteArchiveBundle& SimArchiveBundle,
	const FHCIAbilityKitAgentSimulateExecuteFinalReport& SimFinalReport,
	const FHCIAbilityKitAgentSimulateExecuteReceipt& SimExecuteReceipt,
	const FHCIAbilityKitAgentExecuteTicket& CurrentExecuteTicket,
	const FHCIAbilityKitAgentApplyConfirmRequest& CurrentConfirmRequest,
	const FHCIAbilityKitAgentApplyRequest& CurrentApplyRequest,
	const FHCIAbilityKitDryRunDiffReport& CurrentReviewReport,
	FHCIAbilityKitAgentStageGExecuteIntent& OutIntent)
{
	HCI_CopyHandoffEnvelopeToStageGIntent(SimHandoffEnvelope, OutIntent);

	if (!SimHandoffEnvelope.bUserConfirmed)
	{
		OutIntent.ErrorCode = TEXT("E4005");
		OutIntent.Reason = TEXT("user_not_confirmed");
		OutIntent.ExecuteIntentDigest = HCI_BuildStageGExecuteIntentDigest_G1(OutIntent);
		return true;
	}

	if (!SimHandoffEnvelope.bReadyToSimulateExecute)
	{
		OutIntent.ErrorCode = TEXT("E4205");
		OutIntent.Reason = TEXT("execute_ticket_not_ready");
		OutIntent.ExecuteIntentDigest = HCI_BuildStageGExecuteIntentDigest_G1(OutIntent);
		return true;
	}

	if (!SimHandoffEnvelope.bSimulatedDispatchAccepted)
	{
		OutIntent.ErrorCode = TEXT("E4206");
		OutIntent.Reason = TEXT("simulate_execute_receipt_not_accepted");
		OutIntent.ExecuteIntentDigest = HCI_BuildStageGExecuteIntentDigest_G1(OutIntent);
		return true;
	}

	if (!SimHandoffEnvelope.bSimulationCompleted || !SimHandoffEnvelope.TerminalStatus.Equals(TEXT("completed"), ESearchCase::IgnoreCase))
	{
		OutIntent.ErrorCode = TEXT("E4207");
		OutIntent.Reason = TEXT("simulate_final_report_not_completed");
		OutIntent.ExecuteIntentDigest = HCI_BuildStageGExecuteIntentDigest_G1(OutIntent);
		return true;
	}

	if (!SimHandoffEnvelope.bArchiveReady || !SimHandoffEnvelope.ArchiveStatus.Equals(TEXT("ready"), ESearchCase::IgnoreCase))
	{
		OutIntent.ErrorCode = TEXT("E4208");
		OutIntent.Reason = TEXT("sim_archive_bundle_not_ready");
		OutIntent.ExecuteIntentDigest = HCI_BuildStageGExecuteIntentDigest_G1(OutIntent);
		return true;
	}

	if (!SimHandoffEnvelope.bHandoffReady || !SimHandoffEnvelope.HandoffStatus.Equals(TEXT("ready"), ESearchCase::IgnoreCase))
	{
		OutIntent.ErrorCode = TEXT("E4209");
		OutIntent.Reason = TEXT("sim_handoff_envelope_not_ready");
		OutIntent.ExecuteIntentDigest = HCI_BuildStageGExecuteIntentDigest_G1(OutIntent);
		return true;
	}

	if (!CurrentExecuteTicket.bReadyToSimulateExecute)
	{
		OutIntent.ErrorCode = TEXT("E4205");
		OutIntent.Reason = TEXT("execute_ticket_not_ready");
		OutIntent.ExecuteIntentDigest = HCI_BuildStageGExecuteIntentDigest_G1(OutIntent);
		return true;
	}

	if (!CurrentConfirmRequest.bReadyToExecute)
	{
		OutIntent.ErrorCode = TEXT("E4204");
		OutIntent.Reason = TEXT("confirm_request_not_ready");
		OutIntent.ExecuteIntentDigest = HCI_BuildStageGExecuteIntentDigest_G1(OutIntent);
		return true;
	}

	if (SimHandoffEnvelope.SimArchiveBundleId != SimArchiveBundle.RequestId)
	{
		OutIntent.ErrorCode = TEXT("E4202");
		OutIntent.Reason = TEXT("sim_archive_bundle_id_mismatch");
		OutIntent.ExecuteIntentDigest = HCI_BuildStageGExecuteIntentDigest_G1(OutIntent);
		return true;
	}

	if (SimHandoffEnvelope.SimFinalReportId != SimFinalReport.RequestId)
	{
		OutIntent.ErrorCode = TEXT("E4202");
		OutIntent.Reason = TEXT("sim_final_report_id_mismatch");
		OutIntent.ExecuteIntentDigest = HCI_BuildStageGExecuteIntentDigest_G1(OutIntent);
		return true;
	}

	if (SimHandoffEnvelope.SimExecuteReceiptId != SimExecuteReceipt.RequestId)
	{
		OutIntent.ErrorCode = TEXT("E4202");
		OutIntent.Reason = TEXT("sim_execute_receipt_id_mismatch");
		OutIntent.ExecuteIntentDigest = HCI_BuildStageGExecuteIntentDigest_G1(OutIntent);
		return true;
	}

	if (SimHandoffEnvelope.ExecuteTicketId != CurrentExecuteTicket.RequestId)
	{
		OutIntent.ErrorCode = TEXT("E4202");
		OutIntent.Reason = TEXT("execute_ticket_id_mismatch");
		OutIntent.ExecuteIntentDigest = HCI_BuildStageGExecuteIntentDigest_G1(OutIntent);
		return true;
	}

	if (SimHandoffEnvelope.ConfirmRequestId != CurrentConfirmRequest.RequestId)
	{
		OutIntent.ErrorCode = TEXT("E4202");
		OutIntent.Reason = TEXT("confirm_request_id_mismatch");
		OutIntent.ExecuteIntentDigest = HCI_BuildStageGExecuteIntentDigest_G1(OutIntent);
		return true;
	}

	if (SimHandoffEnvelope.ApplyRequestId != CurrentApplyRequest.RequestId)
	{
		OutIntent.ErrorCode = TEXT("E4202");
		OutIntent.Reason = TEXT("apply_request_id_mismatch");
		OutIntent.ExecuteIntentDigest = HCI_BuildStageGExecuteIntentDigest_G1(OutIntent);
		return true;
	}

	if (SimHandoffEnvelope.ReviewRequestId != CurrentReviewReport.RequestId)
	{
		OutIntent.ErrorCode = TEXT("E4202");
		OutIntent.Reason = TEXT("review_request_id_mismatch");
		OutIntent.ExecuteIntentDigest = HCI_BuildStageGExecuteIntentDigest_G1(OutIntent);
		return true;
	}

	if (SimHandoffEnvelope.SelectionDigest != SimArchiveBundle.SelectionDigest ||
		SimHandoffEnvelope.SelectionDigest != SimFinalReport.SelectionDigest ||
		SimHandoffEnvelope.SelectionDigest != SimExecuteReceipt.SelectionDigest ||
		SimHandoffEnvelope.SelectionDigest != CurrentExecuteTicket.SelectionDigest ||
		SimHandoffEnvelope.SelectionDigest != CurrentConfirmRequest.SelectionDigest ||
		SimHandoffEnvelope.SelectionDigest != CurrentApplyRequest.SelectionDigest)
	{
		OutIntent.ErrorCode = TEXT("E4202");
		OutIntent.Reason = TEXT("selection_digest_mismatch");
		OutIntent.ExecuteIntentDigest = HCI_BuildStageGExecuteIntentDigest_G1(OutIntent);
		return true;
	}

	const FString CurrentSelectionDigest = HCI_BuildSelectionDigestFromReviewReport_G1(CurrentReviewReport);
	if (SimHandoffEnvelope.SelectionDigest != CurrentSelectionDigest)
	{
		OutIntent.ErrorCode = TEXT("E4202");
		OutIntent.Reason = TEXT("selection_digest_mismatch");
		OutIntent.ExecuteIntentDigest = HCI_BuildStageGExecuteIntentDigest_G1(OutIntent);
		return true;
	}

	if (SimHandoffEnvelope.ArchiveDigest.IsEmpty() || SimHandoffEnvelope.ArchiveDigest != SimArchiveBundle.ArchiveDigest)
	{
		OutIntent.ErrorCode = TEXT("E4202");
		OutIntent.Reason = TEXT("archive_digest_mismatch_or_missing");
		OutIntent.ExecuteIntentDigest = HCI_BuildStageGExecuteIntentDigest_G1(OutIntent);
		return true;
	}

	if (SimHandoffEnvelope.HandoffDigest.IsEmpty())
	{
		OutIntent.ErrorCode = TEXT("E4202");
		OutIntent.Reason = TEXT("handoff_digest_missing");
		OutIntent.ExecuteIntentDigest = HCI_BuildStageGExecuteIntentDigest_G1(OutIntent);
		return true;
	}

	if (!SimHandoffEnvelope.HandoffTarget.Equals(TEXT("stage_g_execute"), ESearchCase::IgnoreCase))
	{
		OutIntent.ErrorCode = TEXT("E4202");
		OutIntent.Reason = TEXT("handoff_target_mismatch");
		OutIntent.ExecuteIntentDigest = HCI_BuildStageGExecuteIntentDigest_G1(OutIntent);
		return true;
	}

	OutIntent.StageGStatus = TEXT("ready");
	OutIntent.bReadyForStageGEntry = true;
	OutIntent.bWriteEnabled = false;
	OutIntent.ErrorCode = TEXT("-");
	OutIntent.Reason = TEXT("stage_g_execute_intent_ready_validate_only");
	OutIntent.ExecuteIntentDigest = HCI_BuildStageGExecuteIntentDigest_G1(OutIntent);
	return true;
}

