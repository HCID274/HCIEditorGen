#include "Agent/Bridges/HCIAbilityKitAgentExecutorStageGExecutePermitTicketBridge.h"

#include "Agent/Contracts/StageF/HCIAbilityKitAgentApplyConfirmRequest.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentApplyRequest.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentExecuteTicket.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentSimulateExecuteArchiveBundle.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentSimulateExecuteFinalReport.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentSimulateExecuteHandoffEnvelope.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentSimulateExecuteReceipt.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGExecuteIntent.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGWriteEnableRequest.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGExecutePermitTicket.h"
#include "Agent/Executor/HCIAbilityKitDryRunDiff.h"
#include "Common/HCIAbilityKitTimeFormat.h"
#include "Misc/Crc.h"
#include "Misc/Guid.h"

namespace
{
static FString HCI_BuildSelectionDigestFromReviewReport_G3(const FHCIAbilityKitDryRunDiffReport& Report)
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

static FString HCI_BuildStageGExecutePermitDigest_G3(const FHCIAbilityKitAgentStageGExecutePermitTicket& Ticket)
{
	FString Canonical;
	Canonical += FString::Printf(
		TEXT("%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s\n"),
		*Ticket.StageGWriteEnableRequestId,
		*Ticket.StageGExecuteIntentId,
		*Ticket.SimHandoffEnvelopeId,
		*Ticket.SimArchiveBundleId,
		*Ticket.SimFinalReportId,
		*Ticket.SimExecuteReceiptId,
		*Ticket.ExecuteTicketId,
		*Ticket.ConfirmRequestId,
		*Ticket.ApplyRequestId,
		*Ticket.ReviewRequestId,
		*Ticket.SelectionDigest,
		*Ticket.ArchiveDigest,
		*Ticket.HandoffDigest,
		*Ticket.ExecuteIntentDigest,
		*Ticket.StageGWriteEnableDigest,
		*Ticket.TerminalStatus,
		*Ticket.ArchiveStatus,
		*Ticket.HandoffStatus,
		*Ticket.StageGStatus,
		*Ticket.StageGWriteStatus,
		*Ticket.StageGExecutePermitStatus);

	Canonical += FString::Printf(
		TEXT("%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s\n"),
		Ticket.bUserConfirmed ? TEXT("1") : TEXT("0"),
		Ticket.bReadyToSimulateExecute ? TEXT("1") : TEXT("0"),
		Ticket.bSimulatedDispatchAccepted ? TEXT("1") : TEXT("0"),
		Ticket.bSimulationCompleted ? TEXT("1") : TEXT("0"),
		Ticket.bArchiveReady ? TEXT("1") : TEXT("0"),
		Ticket.bHandoffReady ? TEXT("1") : TEXT("0"),
		Ticket.bWriteEnabled ? TEXT("1") : TEXT("0"),
		Ticket.bReadyForStageGEntry ? TEXT("1") : TEXT("0"),
		Ticket.bWriteEnableConfirmed ? TEXT("1") : TEXT("0"),
		Ticket.bReadyForStageGExecute ? TEXT("1") : TEXT("0"),
		Ticket.bStageGExecutePermitReady ? TEXT("1") : TEXT("0"));

	Canonical += FString::Printf(TEXT("%s|%s|%s\n"), *Ticket.ExecuteTarget, *Ticket.HandoffTarget, *Ticket.Reason);

	for (const FHCIAbilityKitAgentApplyRequestItem& Item : Ticket.Items)
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

static void HCI_CopyStageGWriteEnableRequestToExecutePermitTicket_G3(
	const FHCIAbilityKitAgentStageGWriteEnableRequest& InRequest,
	FHCIAbilityKitAgentStageGExecutePermitTicket& OutTicket)
{
	OutTicket = FHCIAbilityKitAgentStageGExecutePermitTicket();
	OutTicket.RequestId = FString::Printf(TEXT("stagegpermit_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	OutTicket.StageGWriteEnableRequestId = InRequest.RequestId;
	OutTicket.StageGExecuteIntentId = InRequest.StageGExecuteIntentId;
	OutTicket.SimHandoffEnvelopeId = InRequest.SimHandoffEnvelopeId;
	OutTicket.SimArchiveBundleId = InRequest.SimArchiveBundleId;
	OutTicket.SimFinalReportId = InRequest.SimFinalReportId;
	OutTicket.SimExecuteReceiptId = InRequest.SimExecuteReceiptId;
	OutTicket.ExecuteTicketId = InRequest.ExecuteTicketId;
	OutTicket.ConfirmRequestId = InRequest.ConfirmRequestId;
	OutTicket.ApplyRequestId = InRequest.ApplyRequestId;
	OutTicket.ReviewRequestId = InRequest.ReviewRequestId;
	OutTicket.SelectionDigest = InRequest.SelectionDigest;
	OutTicket.ArchiveDigest = InRequest.ArchiveDigest;
	OutTicket.HandoffDigest = InRequest.HandoffDigest;
	OutTicket.ExecuteIntentDigest = InRequest.ExecuteIntentDigest;
	OutTicket.StageGWriteEnableDigest = InRequest.StageGWriteEnableDigest;
	OutTicket.GeneratedUtc = FHCIAbilityKitTimeFormat::FormatNowBeijingIso8601();
	OutTicket.ExecutionMode = TEXT("stage_g_execute_permit_ticket_dry_run");
	OutTicket.ExecuteTarget = InRequest.ExecuteTarget;
	OutTicket.HandoffTarget = InRequest.HandoffTarget;
	OutTicket.TransactionMode = InRequest.TransactionMode;
	OutTicket.TerminationPolicy = InRequest.TerminationPolicy;
	OutTicket.TerminalStatus = InRequest.TerminalStatus;
	OutTicket.ArchiveStatus = InRequest.ArchiveStatus;
	OutTicket.HandoffStatus = InRequest.HandoffStatus;
	OutTicket.StageGStatus = InRequest.StageGStatus;
	OutTicket.StageGWriteStatus = InRequest.StageGWriteStatus;
	OutTicket.StageGExecutePermitStatus = TEXT("blocked");
	OutTicket.bUserConfirmed = InRequest.bUserConfirmed;
	OutTicket.bReadyToSimulateExecute = InRequest.bReadyToSimulateExecute;
	OutTicket.bSimulatedDispatchAccepted = InRequest.bSimulatedDispatchAccepted;
	OutTicket.bSimulationCompleted = InRequest.bSimulationCompleted;
	OutTicket.bArchiveReady = InRequest.bArchiveReady;
	OutTicket.bHandoffReady = InRequest.bHandoffReady;
	OutTicket.bWriteEnabled = InRequest.bWriteEnabled;
	OutTicket.bReadyForStageGEntry = InRequest.bReadyForStageGEntry;
	OutTicket.bWriteEnableConfirmed = InRequest.bWriteEnableConfirmed;
	OutTicket.bReadyForStageGExecute = InRequest.bReadyForStageGExecute;
	OutTicket.bStageGExecutePermitReady = false;
	OutTicket.ErrorCode = TEXT("-");
	OutTicket.Reason = TEXT("stage_g_execute_permit_ticket_ready");
	OutTicket.Summary = InRequest.Summary;
	OutTicket.Items = InRequest.Items;
}
} // namespace

bool FHCIAbilityKitAgentExecutorStageGExecutePermitTicketBridge::BuildStageGExecutePermitTicket(
	const FHCIAbilityKitAgentStageGWriteEnableRequest& StageGWriteEnableRequest,
	const FString& ExpectedStageGWriteEnableRequestId,
	const FHCIAbilityKitAgentStageGExecuteIntent& StageGExecuteIntent,
	const FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope& SimHandoffEnvelope,
	const FHCIAbilityKitAgentSimulateExecuteArchiveBundle& SimArchiveBundle,
	const FHCIAbilityKitAgentSimulateExecuteFinalReport& SimFinalReport,
	const FHCIAbilityKitAgentSimulateExecuteReceipt& SimExecuteReceipt,
	const FHCIAbilityKitAgentExecuteTicket& CurrentExecuteTicket,
	const FHCIAbilityKitAgentApplyConfirmRequest& CurrentConfirmRequest,
	const FHCIAbilityKitAgentApplyRequest& CurrentApplyRequest,
	const FHCIAbilityKitDryRunDiffReport& CurrentReviewReport,
	FHCIAbilityKitAgentStageGExecutePermitTicket& OutTicket)
{
	HCI_CopyStageGWriteEnableRequestToExecutePermitTicket_G3(StageGWriteEnableRequest, OutTicket);

	auto FinalizeAndReturn = [&OutTicket]() -> bool
	{
		OutTicket.StageGExecutePermitDigest = HCI_BuildStageGExecutePermitDigest_G3(OutTicket);
		return true;
	};

	if (!ExpectedStageGWriteEnableRequestId.IsEmpty() && StageGWriteEnableRequest.RequestId != ExpectedStageGWriteEnableRequestId)
	{
		OutTicket.ErrorCode = TEXT("E4202");
		OutTicket.Reason = TEXT("stage_g_write_enable_request_id_mismatch");
		return FinalizeAndReturn();
	}

	if (!StageGWriteEnableRequest.bUserConfirmed)
	{
		OutTicket.ErrorCode = TEXT("E4005");
		OutTicket.Reason = TEXT("user_not_confirmed");
		return FinalizeAndReturn();
	}

	if (!StageGWriteEnableRequest.bWriteEnableConfirmed)
	{
		OutTicket.ErrorCode = TEXT("E4005");
		OutTicket.Reason = TEXT("stage_g_write_enable_not_confirmed");
		return FinalizeAndReturn();
	}

	if (!StageGWriteEnableRequest.bReadyForStageGExecute || !StageGWriteEnableRequest.bWriteEnabled ||
		!StageGWriteEnableRequest.StageGWriteStatus.Equals(TEXT("ready"), ESearchCase::IgnoreCase))
	{
		OutTicket.ErrorCode = TEXT("E4211");
		OutTicket.Reason = TEXT("stage_g_write_enable_request_not_ready");
		return FinalizeAndReturn();
	}

	if (!StageGWriteEnableRequest.bReadyForStageGEntry ||
		!StageGWriteEnableRequest.StageGStatus.Equals(TEXT("ready"), ESearchCase::IgnoreCase))
	{
		OutTicket.ErrorCode = TEXT("E4210");
		OutTicket.Reason = TEXT("stage_g_execute_intent_not_ready");
		return FinalizeAndReturn();
	}

	if (!StageGWriteEnableRequest.bReadyToSimulateExecute)
	{
		OutTicket.ErrorCode = TEXT("E4205");
		OutTicket.Reason = TEXT("execute_ticket_not_ready");
		return FinalizeAndReturn();
	}
	if (!StageGWriteEnableRequest.bSimulatedDispatchAccepted)
	{
		OutTicket.ErrorCode = TEXT("E4206");
		OutTicket.Reason = TEXT("simulate_execute_receipt_not_accepted");
		return FinalizeAndReturn();
	}
	if (!StageGWriteEnableRequest.bSimulationCompleted || !StageGWriteEnableRequest.TerminalStatus.Equals(TEXT("completed"), ESearchCase::IgnoreCase))
	{
		OutTicket.ErrorCode = TEXT("E4207");
		OutTicket.Reason = TEXT("simulate_final_report_not_completed");
		return FinalizeAndReturn();
	}
	if (!StageGWriteEnableRequest.bArchiveReady || !StageGWriteEnableRequest.ArchiveStatus.Equals(TEXT("ready"), ESearchCase::IgnoreCase))
	{
		OutTicket.ErrorCode = TEXT("E4208");
		OutTicket.Reason = TEXT("sim_archive_bundle_not_ready");
		return FinalizeAndReturn();
	}
	if (!StageGWriteEnableRequest.bHandoffReady || !StageGWriteEnableRequest.HandoffStatus.Equals(TEXT("ready"), ESearchCase::IgnoreCase))
	{
		OutTicket.ErrorCode = TEXT("E4209");
		OutTicket.Reason = TEXT("sim_handoff_envelope_not_ready");
		return FinalizeAndReturn();
	}

	if (!CurrentExecuteTicket.bReadyToSimulateExecute)
	{
		OutTicket.ErrorCode = TEXT("E4205");
		OutTicket.Reason = TEXT("execute_ticket_not_ready");
		return FinalizeAndReturn();
	}
	if (!CurrentConfirmRequest.bReadyToExecute)
	{
		OutTicket.ErrorCode = TEXT("E4204");
		OutTicket.Reason = TEXT("confirm_request_not_ready");
		return FinalizeAndReturn();
	}

	if (StageGWriteEnableRequest.StageGExecuteIntentId != StageGExecuteIntent.RequestId)
	{
		OutTicket.ErrorCode = TEXT("E4202");
		OutTicket.Reason = TEXT("stage_g_execute_intent_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGWriteEnableRequest.SimHandoffEnvelopeId != SimHandoffEnvelope.RequestId)
	{
		OutTicket.ErrorCode = TEXT("E4202");
		OutTicket.Reason = TEXT("sim_handoff_envelope_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGWriteEnableRequest.SimArchiveBundleId != SimArchiveBundle.RequestId)
	{
		OutTicket.ErrorCode = TEXT("E4202");
		OutTicket.Reason = TEXT("sim_archive_bundle_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGWriteEnableRequest.SimFinalReportId != SimFinalReport.RequestId)
	{
		OutTicket.ErrorCode = TEXT("E4202");
		OutTicket.Reason = TEXT("sim_final_report_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGWriteEnableRequest.SimExecuteReceiptId != SimExecuteReceipt.RequestId)
	{
		OutTicket.ErrorCode = TEXT("E4202");
		OutTicket.Reason = TEXT("sim_execute_receipt_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGWriteEnableRequest.ExecuteTicketId != CurrentExecuteTicket.RequestId)
	{
		OutTicket.ErrorCode = TEXT("E4202");
		OutTicket.Reason = TEXT("execute_ticket_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGWriteEnableRequest.ConfirmRequestId != CurrentConfirmRequest.RequestId)
	{
		OutTicket.ErrorCode = TEXT("E4202");
		OutTicket.Reason = TEXT("confirm_request_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGWriteEnableRequest.ApplyRequestId != CurrentApplyRequest.RequestId)
	{
		OutTicket.ErrorCode = TEXT("E4202");
		OutTicket.Reason = TEXT("apply_request_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGWriteEnableRequest.ReviewRequestId != CurrentReviewReport.RequestId)
	{
		OutTicket.ErrorCode = TEXT("E4202");
		OutTicket.Reason = TEXT("review_request_id_mismatch");
		return FinalizeAndReturn();
	}

	if (StageGWriteEnableRequest.SelectionDigest != StageGExecuteIntent.SelectionDigest ||
		StageGWriteEnableRequest.SelectionDigest != SimHandoffEnvelope.SelectionDigest ||
		StageGWriteEnableRequest.SelectionDigest != SimArchiveBundle.SelectionDigest ||
		StageGWriteEnableRequest.SelectionDigest != SimFinalReport.SelectionDigest ||
		StageGWriteEnableRequest.SelectionDigest != SimExecuteReceipt.SelectionDigest ||
		StageGWriteEnableRequest.SelectionDigest != CurrentExecuteTicket.SelectionDigest ||
		StageGWriteEnableRequest.SelectionDigest != CurrentConfirmRequest.SelectionDigest ||
		StageGWriteEnableRequest.SelectionDigest != CurrentApplyRequest.SelectionDigest)
	{
		OutTicket.ErrorCode = TEXT("E4202");
		OutTicket.Reason = TEXT("selection_digest_mismatch");
		return FinalizeAndReturn();
	}

	if (StageGWriteEnableRequest.SelectionDigest != HCI_BuildSelectionDigestFromReviewReport_G3(CurrentReviewReport))
	{
		OutTicket.ErrorCode = TEXT("E4202");
		OutTicket.Reason = TEXT("selection_digest_mismatch");
		return FinalizeAndReturn();
	}

	if (StageGWriteEnableRequest.ArchiveDigest.IsEmpty() || StageGWriteEnableRequest.ArchiveDigest != SimArchiveBundle.ArchiveDigest)
	{
		OutTicket.ErrorCode = TEXT("E4202");
		OutTicket.Reason = TEXT("archive_digest_mismatch_or_missing");
		return FinalizeAndReturn();
	}
	if (StageGWriteEnableRequest.HandoffDigest.IsEmpty() || StageGWriteEnableRequest.HandoffDigest != SimHandoffEnvelope.HandoffDigest)
	{
		OutTicket.ErrorCode = TEXT("E4202");
		OutTicket.Reason = TEXT("handoff_digest_mismatch_or_missing");
		return FinalizeAndReturn();
	}
	if (StageGWriteEnableRequest.ExecuteIntentDigest.IsEmpty() || StageGWriteEnableRequest.ExecuteIntentDigest != StageGExecuteIntent.ExecuteIntentDigest)
	{
		OutTicket.ErrorCode = TEXT("E4202");
		OutTicket.Reason = TEXT("execute_intent_digest_mismatch_or_missing");
		return FinalizeAndReturn();
	}
	if (StageGWriteEnableRequest.StageGWriteEnableDigest.IsEmpty())
	{
		OutTicket.ErrorCode = TEXT("E4202");
		OutTicket.Reason = TEXT("stage_g_write_enable_digest_missing");
		return FinalizeAndReturn();
	}

	if (!StageGWriteEnableRequest.ExecuteTarget.Equals(TEXT("stage_g_execute_runtime"), ESearchCase::IgnoreCase))
	{
		OutTicket.ErrorCode = TEXT("E4202");
		OutTicket.Reason = TEXT("execute_target_mismatch");
		return FinalizeAndReturn();
	}
	if (!StageGWriteEnableRequest.HandoffTarget.Equals(TEXT("stage_g_execute"), ESearchCase::IgnoreCase))
	{
		OutTicket.ErrorCode = TEXT("E4202");
		OutTicket.Reason = TEXT("handoff_target_mismatch");
		return FinalizeAndReturn();
	}

	OutTicket.StageGExecutePermitStatus = TEXT("ready");
	OutTicket.bWriteEnabled = true;
	OutTicket.bReadyForStageGExecute = true;
	OutTicket.bStageGExecutePermitReady = true;
	OutTicket.ErrorCode = TEXT("-");
	OutTicket.Reason = TEXT("stage_g_execute_permit_ticket_ready");
	return FinalizeAndReturn();
}