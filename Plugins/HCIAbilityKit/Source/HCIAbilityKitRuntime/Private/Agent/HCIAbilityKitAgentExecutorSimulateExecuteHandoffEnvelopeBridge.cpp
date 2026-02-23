#include "Agent/HCIAbilityKitAgentExecutorSimulateExecuteHandoffEnvelopeBridge.h"

#include "Agent/HCIAbilityKitAgentApplyConfirmRequest.h"
#include "Agent/HCIAbilityKitAgentApplyRequest.h"
#include "Agent/HCIAbilityKitAgentExecuteTicket.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteArchiveBundle.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteFinalReport.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteHandoffEnvelope.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteReceipt.h"
#include "Agent/HCIAbilityKitDryRunDiff.h"
#include "Common/HCIAbilityKitTimeFormat.h"
#include "Misc/Crc.h"
#include "Misc/Guid.h"

namespace
{
static FString HCI_BuildSelectionDigestFromReviewReport_F15(const FHCIAbilityKitDryRunDiffReport& Report)
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

static FString HCI_BuildHandoffDigest_F15(const FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope& Envelope)
{
	FString Canonical;
	Canonical += FString::Printf(
		TEXT("%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s\n"),
		*Envelope.SimArchiveBundleId,
		*Envelope.SimFinalReportId,
		*Envelope.SimExecuteReceiptId,
		*Envelope.ExecuteTicketId,
		*Envelope.ConfirmRequestId,
		*Envelope.ApplyRequestId,
		*Envelope.ReviewRequestId,
		*Envelope.SelectionDigest,
		*Envelope.ArchiveDigest,
		*Envelope.TerminalStatus,
		*Envelope.ArchiveStatus,
		*Envelope.HandoffStatus,
		Envelope.bUserConfirmed ? TEXT("1") : TEXT("0"),
		Envelope.bReadyToSimulateExecute ? TEXT("1") : TEXT("0"),
		Envelope.bSimulatedDispatchAccepted ? TEXT("1") : TEXT("0"),
		Envelope.bSimulationCompleted ? TEXT("1") : TEXT("0"),
		Envelope.bArchiveReady ? TEXT("1") : TEXT("0"));

	Canonical += FString::Printf(TEXT("%s|%s\n"), Envelope.bHandoffReady ? TEXT("1") : TEXT("0"), *Envelope.HandoffTarget);

	for (const FHCIAbilityKitAgentApplyRequestItem& Item : Envelope.Items)
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

static void HCI_CopyArchiveBundleToHandoffEnvelope(
	const FHCIAbilityKitAgentSimulateExecuteArchiveBundle& SimArchiveBundle,
	FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope& OutEnvelope)
{
	OutEnvelope = FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope();
	OutEnvelope.RequestId = FString::Printf(TEXT("simhandoff_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	OutEnvelope.SimArchiveBundleId = SimArchiveBundle.RequestId;
	OutEnvelope.SimFinalReportId = SimArchiveBundle.SimFinalReportId;
	OutEnvelope.SimExecuteReceiptId = SimArchiveBundle.SimExecuteReceiptId;
	OutEnvelope.ExecuteTicketId = SimArchiveBundle.ExecuteTicketId;
	OutEnvelope.ConfirmRequestId = SimArchiveBundle.ConfirmRequestId;
	OutEnvelope.ApplyRequestId = SimArchiveBundle.ApplyRequestId;
	OutEnvelope.ReviewRequestId = SimArchiveBundle.ReviewRequestId;
	OutEnvelope.SelectionDigest = SimArchiveBundle.SelectionDigest;
	OutEnvelope.ArchiveDigest = SimArchiveBundle.ArchiveDigest;
	OutEnvelope.GeneratedUtc = FHCIAbilityKitTimeFormat::FormatNowBeijingIso8601();
	OutEnvelope.ExecutionMode = TEXT("simulate_dry_run_handoff_envelope");
	OutEnvelope.HandoffTarget = TEXT("stage_g_execute");
	OutEnvelope.TransactionMode = SimArchiveBundle.TransactionMode;
	OutEnvelope.TerminationPolicy = SimArchiveBundle.TerminationPolicy;
	OutEnvelope.TerminalStatus = SimArchiveBundle.TerminalStatus;
	OutEnvelope.ArchiveStatus = SimArchiveBundle.ArchiveStatus;
	OutEnvelope.HandoffStatus = TEXT("blocked");
	OutEnvelope.bUserConfirmed = SimArchiveBundle.bUserConfirmed;
	OutEnvelope.bReadyToSimulateExecute = SimArchiveBundle.bReadyToSimulateExecute;
	OutEnvelope.bSimulatedDispatchAccepted = SimArchiveBundle.bSimulatedDispatchAccepted;
	OutEnvelope.bSimulationCompleted = SimArchiveBundle.bSimulationCompleted;
	OutEnvelope.bArchiveReady = SimArchiveBundle.bArchiveReady;
	OutEnvelope.bHandoffReady = false;
	OutEnvelope.ErrorCode = TEXT("-");
	OutEnvelope.Reason = TEXT("simulate_handoff_envelope_ready");
	OutEnvelope.Summary = SimArchiveBundle.Summary;
	OutEnvelope.Items = SimArchiveBundle.Items;
}
} // namespace

bool FHCIAbilityKitAgentExecutorSimulateExecuteHandoffEnvelopeBridge::BuildSimulateExecuteHandoffEnvelope(
	const FHCIAbilityKitAgentSimulateExecuteArchiveBundle& SimArchiveBundle,
	const FHCIAbilityKitAgentSimulateExecuteFinalReport& SimFinalReport,
	const FHCIAbilityKitAgentSimulateExecuteReceipt& SimExecuteReceipt,
	const FHCIAbilityKitAgentExecuteTicket& CurrentExecuteTicket,
	const FHCIAbilityKitAgentApplyConfirmRequest& CurrentConfirmRequest,
	const FHCIAbilityKitAgentApplyRequest& CurrentApplyRequest,
	const FHCIAbilityKitDryRunDiffReport& CurrentReviewReport,
	FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope& OutEnvelope)
{
	HCI_CopyArchiveBundleToHandoffEnvelope(SimArchiveBundle, OutEnvelope);

	if (!SimArchiveBundle.bUserConfirmed)
	{
		OutEnvelope.ErrorCode = TEXT("E4005");
		OutEnvelope.Reason = TEXT("user_not_confirmed");
		OutEnvelope.HandoffDigest = HCI_BuildHandoffDigest_F15(OutEnvelope);
		return true;
	}

	if (!SimArchiveBundle.bReadyToSimulateExecute)
	{
		OutEnvelope.ErrorCode = TEXT("E4205");
		OutEnvelope.Reason = TEXT("execute_ticket_not_ready");
		OutEnvelope.HandoffDigest = HCI_BuildHandoffDigest_F15(OutEnvelope);
		return true;
	}

	if (!SimArchiveBundle.bSimulatedDispatchAccepted)
	{
		OutEnvelope.ErrorCode = TEXT("E4206");
		OutEnvelope.Reason = TEXT("simulate_execute_receipt_not_accepted");
		OutEnvelope.HandoffDigest = HCI_BuildHandoffDigest_F15(OutEnvelope);
		return true;
	}

	if (!SimArchiveBundle.bSimulationCompleted || !SimArchiveBundle.TerminalStatus.Equals(TEXT("completed"), ESearchCase::IgnoreCase))
	{
		OutEnvelope.ErrorCode = TEXT("E4207");
		OutEnvelope.Reason = TEXT("simulate_final_report_not_completed");
		OutEnvelope.HandoffDigest = HCI_BuildHandoffDigest_F15(OutEnvelope);
		return true;
	}

	if (!SimArchiveBundle.bArchiveReady || !SimArchiveBundle.ArchiveStatus.Equals(TEXT("ready"), ESearchCase::IgnoreCase))
	{
		OutEnvelope.ErrorCode = TEXT("E4208");
		OutEnvelope.Reason = TEXT("sim_archive_bundle_not_ready");
		OutEnvelope.HandoffDigest = HCI_BuildHandoffDigest_F15(OutEnvelope);
		return true;
	}

	if (!CurrentExecuteTicket.bReadyToSimulateExecute)
	{
		OutEnvelope.ErrorCode = TEXT("E4205");
		OutEnvelope.Reason = TEXT("execute_ticket_not_ready");
		OutEnvelope.HandoffDigest = HCI_BuildHandoffDigest_F15(OutEnvelope);
		return true;
	}

	if (!CurrentConfirmRequest.bReadyToExecute)
	{
		OutEnvelope.ErrorCode = TEXT("E4204");
		OutEnvelope.Reason = TEXT("confirm_request_not_ready");
		OutEnvelope.HandoffDigest = HCI_BuildHandoffDigest_F15(OutEnvelope);
		return true;
	}

	if (SimArchiveBundle.SimFinalReportId != SimFinalReport.RequestId)
	{
		OutEnvelope.ErrorCode = TEXT("E4202");
		OutEnvelope.Reason = TEXT("sim_final_report_id_mismatch");
		OutEnvelope.HandoffDigest = HCI_BuildHandoffDigest_F15(OutEnvelope);
		return true;
	}

	if (SimArchiveBundle.SimExecuteReceiptId != SimExecuteReceipt.RequestId)
	{
		OutEnvelope.ErrorCode = TEXT("E4202");
		OutEnvelope.Reason = TEXT("sim_execute_receipt_id_mismatch");
		OutEnvelope.HandoffDigest = HCI_BuildHandoffDigest_F15(OutEnvelope);
		return true;
	}

	if (SimArchiveBundle.ExecuteTicketId != CurrentExecuteTicket.RequestId)
	{
		OutEnvelope.ErrorCode = TEXT("E4202");
		OutEnvelope.Reason = TEXT("execute_ticket_id_mismatch");
		OutEnvelope.HandoffDigest = HCI_BuildHandoffDigest_F15(OutEnvelope);
		return true;
	}

	if (SimArchiveBundle.ConfirmRequestId != CurrentConfirmRequest.RequestId)
	{
		OutEnvelope.ErrorCode = TEXT("E4202");
		OutEnvelope.Reason = TEXT("confirm_request_id_mismatch");
		OutEnvelope.HandoffDigest = HCI_BuildHandoffDigest_F15(OutEnvelope);
		return true;
	}

	if (SimArchiveBundle.ApplyRequestId != CurrentApplyRequest.RequestId)
	{
		OutEnvelope.ErrorCode = TEXT("E4202");
		OutEnvelope.Reason = TEXT("apply_request_id_mismatch");
		OutEnvelope.HandoffDigest = HCI_BuildHandoffDigest_F15(OutEnvelope);
		return true;
	}

	if (SimArchiveBundle.ReviewRequestId != CurrentReviewReport.RequestId)
	{
		OutEnvelope.ErrorCode = TEXT("E4202");
		OutEnvelope.Reason = TEXT("review_request_id_mismatch");
		OutEnvelope.HandoffDigest = HCI_BuildHandoffDigest_F15(OutEnvelope);
		return true;
	}

	if (SimArchiveBundle.SelectionDigest != SimFinalReport.SelectionDigest ||
		SimArchiveBundle.SelectionDigest != SimExecuteReceipt.SelectionDigest ||
		SimArchiveBundle.SelectionDigest != CurrentExecuteTicket.SelectionDigest ||
		SimArchiveBundle.SelectionDigest != CurrentConfirmRequest.SelectionDigest ||
		SimArchiveBundle.SelectionDigest != CurrentApplyRequest.SelectionDigest)
	{
		OutEnvelope.ErrorCode = TEXT("E4202");
		OutEnvelope.Reason = TEXT("selection_digest_mismatch");
		OutEnvelope.HandoffDigest = HCI_BuildHandoffDigest_F15(OutEnvelope);
		return true;
	}

	const FString CurrentSelectionDigest = HCI_BuildSelectionDigestFromReviewReport_F15(CurrentReviewReport);
	if (SimArchiveBundle.SelectionDigest != CurrentSelectionDigest)
	{
		OutEnvelope.ErrorCode = TEXT("E4202");
		OutEnvelope.Reason = TEXT("selection_digest_mismatch");
		OutEnvelope.HandoffDigest = HCI_BuildHandoffDigest_F15(OutEnvelope);
		return true;
	}

	if (SimArchiveBundle.ArchiveDigest.IsEmpty())
	{
		OutEnvelope.ErrorCode = TEXT("E4202");
		OutEnvelope.Reason = TEXT("archive_digest_missing");
		OutEnvelope.HandoffDigest = HCI_BuildHandoffDigest_F15(OutEnvelope);
		return true;
	}

	OutEnvelope.HandoffStatus = TEXT("ready");
	OutEnvelope.bHandoffReady = true;
	OutEnvelope.ErrorCode = TEXT("-");
	OutEnvelope.Reason = TEXT("simulate_handoff_envelope_ready");
	OutEnvelope.HandoffDigest = HCI_BuildHandoffDigest_F15(OutEnvelope);
	return true;
}
