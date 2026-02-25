#include "Agent/Bridges/HCIAbilityKitAgentExecutorStageGExecuteArchiveBundleBridge.h"

#include "Agent/Contracts/StageF/HCIAbilityKitAgentApplyConfirmRequest.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentApplyRequest.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentExecuteTicket.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentSimulateExecuteArchiveBundle.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentSimulateExecuteFinalReport.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentSimulateExecuteHandoffEnvelope.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentSimulateExecuteReceipt.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGExecuteArchiveBundle.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGExecuteCommitRequest.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGExecuteCommitReceipt.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGExecuteDispatchRequest.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGExecuteDispatchReceipt.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGExecuteFinalReport.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGExecuteIntent.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGExecutePermitTicket.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGWriteEnableRequest.h"
#include "Agent/Executor/HCIAbilityKitDryRunDiff.h"
#include "Common/HCIAbilityKitTimeFormat.h"
#include "Misc/Crc.h"
#include "Misc/Guid.h"

namespace
{
static FString HCI_BuildSelectionDigestFromReviewReport_G9(const FHCIAbilityKitDryRunDiffReport& Report)
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

static FString HCI_BuildStageGExecuteArchiveBundleDigest_G9(const FHCIAbilityKitAgentStageGExecuteArchiveBundle& Bundle)
{
	FString Canonical;
	Canonical += FString::Printf(
		TEXT("%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s\n"),
		*Bundle.StageGExecuteFinalReportId,
		*Bundle.StageGExecuteCommitReceiptId,
		*Bundle.StageGExecuteCommitRequestId,
		*Bundle.StageGExecuteDispatchReceiptId,
		*Bundle.StageGExecuteDispatchRequestId,
		*Bundle.StageGExecutePermitTicketId,
		*Bundle.StageGWriteEnableRequestId,
		*Bundle.StageGExecuteIntentId,
		*Bundle.SimHandoffEnvelopeId,
		*Bundle.SimArchiveBundleId,
		*Bundle.SimFinalReportId,
		*Bundle.SimExecuteReceiptId,
		*Bundle.ExecuteTicketId,
		*Bundle.ConfirmRequestId,
		*Bundle.ApplyRequestId,
		*Bundle.ReviewRequestId,
		*Bundle.SelectionDigest,
		*Bundle.ArchiveDigest,
		*Bundle.HandoffDigest,
		*Bundle.ExecuteIntentDigest,
		*Bundle.StageGWriteEnableDigest,
		*Bundle.StageGExecutePermitDigest,
		*Bundle.StageGExecuteDispatchDigest,
		*Bundle.StageGExecuteDispatchReceiptDigest,
		*Bundle.StageGExecuteCommitRequestDigest,
		*Bundle.StageGExecuteCommitReceiptDigest,
		*Bundle.StageGExecuteFinalReportDigest,
		*Bundle.TerminalStatus,
		*Bundle.ArchiveStatus,
		*Bundle.HandoffStatus,
		*Bundle.StageGStatus,
		*Bundle.StageGWriteStatus,
		*Bundle.StageGExecuteFinalReportStatus,
		*Bundle.StageGExecuteArchiveBundleStatus);

	Canonical += FString::Printf(
		TEXT("%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s\n"),
		Bundle.bUserConfirmed ? TEXT("1") : TEXT("0"),
		Bundle.bReadyToSimulateExecute ? TEXT("1") : TEXT("0"),
		Bundle.bSimulatedDispatchAccepted ? TEXT("1") : TEXT("0"),
		Bundle.bSimulationCompleted ? TEXT("1") : TEXT("0"),
		Bundle.bArchiveReady ? TEXT("1") : TEXT("0"),
		Bundle.bHandoffReady ? TEXT("1") : TEXT("0"),
		Bundle.bWriteEnabled ? TEXT("1") : TEXT("0"),
		Bundle.bReadyForStageGEntry ? TEXT("1") : TEXT("0"),
		Bundle.bWriteEnableConfirmed ? TEXT("1") : TEXT("0"),
		Bundle.bReadyForStageGExecute ? TEXT("1") : TEXT("0"),
		Bundle.bStageGExecutePermitReady ? TEXT("1") : TEXT("0"),
		Bundle.bExecuteDispatchConfirmed ? TEXT("1") : TEXT("0"),
		Bundle.bStageGExecuteDispatchReady ? TEXT("1") : TEXT("0"),
		Bundle.bStageGExecuteDispatchAccepted ? TEXT("1") : TEXT("0"),
		Bundle.bStageGExecuteDispatchReceiptReady ? TEXT("1") : TEXT("0"),
		Bundle.bExecuteCommitConfirmed ? TEXT("1") : TEXT("0"),
		Bundle.bStageGExecuteCommitRequestReady ? TEXT("1") : TEXT("0"),
		Bundle.bStageGExecuteCommitAccepted ? TEXT("1") : TEXT("0"),
		Bundle.bStageGExecuteCommitReceiptReady ? TEXT("1") : TEXT("0"),
		Bundle.bStageGExecuteFinalized ? TEXT("1") : TEXT("0"),
		Bundle.bStageGExecuteFinalReportReady ? TEXT("1") : TEXT("0"),
		Bundle.bStageGExecuteArchived ? TEXT("1") : TEXT("0"),
		Bundle.bStageGExecuteArchiveBundleReady ? TEXT("1") : TEXT("0"));

	Canonical += FString::Printf(TEXT("%s|%s|%s\n"), *Bundle.ExecuteTarget, *Bundle.HandoffTarget, *Bundle.Reason);

	for (const FHCIAbilityKitAgentApplyRequestItem& Item : Bundle.Items)
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

static void HCI_CopyFinalReportToArchiveBundle_G9(
	const FHCIAbilityKitAgentStageGExecuteFinalReport& InReport,
	FHCIAbilityKitAgentStageGExecuteArchiveBundle& OutBundle)
{
	OutBundle = FHCIAbilityKitAgentStageGExecuteArchiveBundle();
	OutBundle.RequestId = FString::Printf(TEXT("stagegarchive_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	OutBundle.StageGExecuteFinalReportId = InReport.RequestId;
	OutBundle.StageGExecuteCommitReceiptId = InReport.StageGExecuteCommitReceiptId;
	OutBundle.StageGExecuteCommitRequestId = InReport.StageGExecuteCommitRequestId;
	OutBundle.StageGExecuteDispatchReceiptId = InReport.StageGExecuteDispatchReceiptId;
	OutBundle.StageGExecuteDispatchRequestId = InReport.StageGExecuteDispatchRequestId;
	OutBundle.StageGExecutePermitTicketId = InReport.StageGExecutePermitTicketId;
	OutBundle.StageGWriteEnableRequestId = InReport.StageGWriteEnableRequestId;
	OutBundle.StageGExecuteIntentId = InReport.StageGExecuteIntentId;
	OutBundle.SimHandoffEnvelopeId = InReport.SimHandoffEnvelopeId;
	OutBundle.SimArchiveBundleId = InReport.SimArchiveBundleId;
	OutBundle.SimFinalReportId = InReport.SimFinalReportId;
	OutBundle.SimExecuteReceiptId = InReport.SimExecuteReceiptId;
	OutBundle.ExecuteTicketId = InReport.ExecuteTicketId;
	OutBundle.ConfirmRequestId = InReport.ConfirmRequestId;
	OutBundle.ApplyRequestId = InReport.ApplyRequestId;
	OutBundle.ReviewRequestId = InReport.ReviewRequestId;
	OutBundle.SelectionDigest = InReport.SelectionDigest;
	OutBundle.ArchiveDigest = InReport.ArchiveDigest;
	OutBundle.HandoffDigest = InReport.HandoffDigest;
	OutBundle.ExecuteIntentDigest = InReport.ExecuteIntentDigest;
	OutBundle.StageGWriteEnableDigest = InReport.StageGWriteEnableDigest;
	OutBundle.StageGExecutePermitDigest = InReport.StageGExecutePermitDigest;
	OutBundle.StageGExecuteDispatchDigest = InReport.StageGExecuteDispatchDigest;
	OutBundle.StageGExecuteDispatchReceiptDigest = InReport.StageGExecuteDispatchReceiptDigest;
	OutBundle.StageGExecuteCommitRequestDigest = InReport.StageGExecuteCommitRequestDigest;
	OutBundle.StageGExecuteCommitReceiptDigest = InReport.StageGExecuteCommitReceiptDigest;
	OutBundle.StageGExecuteFinalReportDigest = InReport.StageGExecuteFinalReportDigest;
	OutBundle.GeneratedUtc = FHCIAbilityKitTimeFormat::FormatNowBeijingIso8601();
	OutBundle.ExecutionMode = TEXT("stage_g_execute_archive_bundle_dry_run");
	OutBundle.ExecuteTarget = InReport.ExecuteTarget;
	OutBundle.HandoffTarget = InReport.HandoffTarget;
	OutBundle.TransactionMode = InReport.TransactionMode;
	OutBundle.TerminationPolicy = InReport.TerminationPolicy;
	OutBundle.TerminalStatus = InReport.TerminalStatus;
	OutBundle.ArchiveStatus = InReport.ArchiveStatus;
	OutBundle.HandoffStatus = InReport.HandoffStatus;
	OutBundle.StageGStatus = InReport.StageGStatus;
	OutBundle.StageGWriteStatus = InReport.StageGWriteStatus;
	OutBundle.StageGExecutePermitStatus = InReport.StageGExecutePermitStatus;
	OutBundle.StageGExecuteDispatchStatus = InReport.StageGExecuteDispatchStatus;
	OutBundle.StageGExecuteDispatchReceiptStatus = InReport.StageGExecuteDispatchReceiptStatus;
	OutBundle.StageGExecuteCommitRequestStatus = InReport.StageGExecuteCommitRequestStatus;
	OutBundle.StageGExecuteCommitReceiptStatus = InReport.StageGExecuteCommitReceiptStatus;
	OutBundle.StageGExecuteFinalReportStatus = InReport.StageGExecuteFinalReportStatus;
	OutBundle.StageGExecuteArchiveBundleStatus = TEXT("blocked");
	OutBundle.bUserConfirmed = InReport.bUserConfirmed;
	OutBundle.bReadyToSimulateExecute = InReport.bReadyToSimulateExecute;
	OutBundle.bSimulatedDispatchAccepted = InReport.bSimulatedDispatchAccepted;
	OutBundle.bSimulationCompleted = InReport.bSimulationCompleted;
	OutBundle.bArchiveReady = InReport.bArchiveReady;
	OutBundle.bHandoffReady = InReport.bHandoffReady;
	OutBundle.bWriteEnabled = InReport.bWriteEnabled;
	OutBundle.bReadyForStageGEntry = InReport.bReadyForStageGEntry;
	OutBundle.bWriteEnableConfirmed = InReport.bWriteEnableConfirmed;
	OutBundle.bReadyForStageGExecute = InReport.bReadyForStageGExecute;
	OutBundle.bStageGExecutePermitReady = InReport.bStageGExecutePermitReady;
	OutBundle.bExecuteDispatchConfirmed = InReport.bExecuteDispatchConfirmed;
	OutBundle.bStageGExecuteDispatchReady = InReport.bStageGExecuteDispatchReady;
	OutBundle.bStageGExecuteDispatchAccepted = InReport.bStageGExecuteDispatchAccepted;
	OutBundle.bStageGExecuteDispatchReceiptReady = InReport.bStageGExecuteDispatchReceiptReady;
	OutBundle.bExecuteCommitConfirmed = InReport.bExecuteCommitConfirmed;
	OutBundle.bStageGExecuteCommitRequestReady = InReport.bStageGExecuteCommitRequestReady;
	OutBundle.bStageGExecuteCommitAccepted = InReport.bStageGExecuteCommitAccepted;
	OutBundle.bStageGExecuteCommitReceiptReady = InReport.bStageGExecuteCommitReceiptReady;
	OutBundle.bStageGExecuteFinalized = InReport.bStageGExecuteFinalized;
	OutBundle.bStageGExecuteFinalReportReady = InReport.bStageGExecuteFinalReportReady;
	OutBundle.bStageGExecuteArchived = false;
	OutBundle.bStageGExecuteArchiveBundleReady = false;
	OutBundle.ErrorCode = TEXT("-");
	OutBundle.Reason = TEXT("stage_g_execute_archive_bundle_ready");
	OutBundle.Summary = InReport.Summary;
	OutBundle.Items = InReport.Items;
}
} // namespace

bool FHCIAbilityKitAgentExecutorStageGExecuteArchiveBundleBridge::BuildStageGExecuteArchiveBundle(
	const FHCIAbilityKitAgentStageGExecuteFinalReport& StageGExecuteFinalReport,
	const FString& ExpectedStageGExecuteFinalReportId,
	const FString& ExpectedStageGExecuteCommitReceiptId,
	const FString& ExpectedStageGExecuteCommitReceiptDigest,
	const FHCIAbilityKitAgentStageGExecuteCommitReceipt& StageGExecuteCommitReceipt,
	const FHCIAbilityKitAgentStageGExecuteCommitRequest& StageGExecuteCommitRequest,
	const FHCIAbilityKitAgentStageGExecuteDispatchReceipt& StageGExecuteDispatchReceipt,
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
	FHCIAbilityKitAgentStageGExecuteArchiveBundle& OutBundle)
{
	HCI_CopyFinalReportToArchiveBundle_G9(StageGExecuteFinalReport, OutBundle);

	auto FinalizeAndReturn = [&OutBundle]() -> bool
	{
		OutBundle.StageGExecuteArchiveBundleDigest = HCI_BuildStageGExecuteArchiveBundleDigest_G9(OutBundle);
		return true;
	};

	if (!ExpectedStageGExecuteFinalReportId.IsEmpty() && StageGExecuteFinalReport.RequestId != ExpectedStageGExecuteFinalReportId)
	{
		OutBundle.ErrorCode = TEXT("E4202");
		OutBundle.Reason = TEXT("stage_g_execute_final_report_id_mismatch");
		return FinalizeAndReturn();
	}
	if (!ExpectedStageGExecuteCommitReceiptId.IsEmpty() && StageGExecuteFinalReport.StageGExecuteCommitReceiptId != ExpectedStageGExecuteCommitReceiptId)
	{
		OutBundle.ErrorCode = TEXT("E4202");
		OutBundle.Reason = TEXT("stage_g_execute_commit_receipt_id_mismatch");
		return FinalizeAndReturn();
	}

	if (!StageGExecuteFinalReport.bUserConfirmed)
	{
		OutBundle.ErrorCode = TEXT("E4005");
		OutBundle.Reason = TEXT("user_not_confirmed");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteFinalReport.bWriteEnableConfirmed)
	{
		OutBundle.ErrorCode = TEXT("E4005");
		OutBundle.Reason = TEXT("stage_g_write_enable_not_confirmed");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteFinalReport.bExecuteDispatchConfirmed)
	{
		OutBundle.ErrorCode = TEXT("E4005");
		OutBundle.Reason = TEXT("stage_g_execute_dispatch_not_confirmed");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteFinalReport.bExecuteCommitConfirmed)
	{
		OutBundle.ErrorCode = TEXT("E4005");
		OutBundle.Reason = TEXT("stage_g_execute_commit_not_confirmed");
		return FinalizeAndReturn();
	}

	if (!StageGExecuteFinalReport.bStageGExecuteFinalReportReady ||
		!StageGExecuteFinalReport.bStageGExecuteFinalized ||
		!StageGExecuteFinalReport.bWriteEnabled ||
		!StageGExecuteFinalReport.bReadyForStageGExecute ||
		!StageGExecuteFinalReport.StageGExecuteFinalReportStatus.Equals(TEXT("completed"), ESearchCase::IgnoreCase))
	{
		OutBundle.ErrorCode = TEXT("E4217");
		OutBundle.Reason = TEXT("stage_g_execute_final_report_not_completed");
		return FinalizeAndReturn();
	}

	if (!StageGExecuteFinalReport.bReadyForStageGEntry || !StageGExecuteFinalReport.StageGStatus.Equals(TEXT("ready"), ESearchCase::IgnoreCase))
	{
		OutBundle.ErrorCode = TEXT("E4210");
		OutBundle.Reason = TEXT("stage_g_execute_intent_not_ready");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteFinalReport.bReadyToSimulateExecute)
	{
		OutBundle.ErrorCode = TEXT("E4205");
		OutBundle.Reason = TEXT("execute_ticket_not_ready");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteFinalReport.bSimulatedDispatchAccepted)
	{
		OutBundle.ErrorCode = TEXT("E4206");
		OutBundle.Reason = TEXT("simulate_execute_receipt_not_accepted");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteFinalReport.bSimulationCompleted || !StageGExecuteFinalReport.TerminalStatus.Equals(TEXT("completed"), ESearchCase::IgnoreCase))
	{
		OutBundle.ErrorCode = TEXT("E4207");
		OutBundle.Reason = TEXT("simulate_final_report_not_completed");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteFinalReport.bArchiveReady || !StageGExecuteFinalReport.ArchiveStatus.Equals(TEXT("ready"), ESearchCase::IgnoreCase))
	{
		OutBundle.ErrorCode = TEXT("E4208");
		OutBundle.Reason = TEXT("sim_archive_bundle_not_ready");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteFinalReport.bHandoffReady || !StageGExecuteFinalReport.HandoffStatus.Equals(TEXT("ready"), ESearchCase::IgnoreCase))
	{
		OutBundle.ErrorCode = TEXT("E4209");
		OutBundle.Reason = TEXT("sim_handoff_envelope_not_ready");
		return FinalizeAndReturn();
	}
	if (!CurrentConfirmRequest.bReadyToExecute)
	{
		OutBundle.ErrorCode = TEXT("E4204");
		OutBundle.Reason = TEXT("confirm_request_not_ready");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteCommitReceipt.bStageGExecuteCommitReceiptReady || !StageGExecuteCommitReceipt.bStageGExecuteCommitAccepted)
	{
		OutBundle.ErrorCode = TEXT("E4216");
		OutBundle.Reason = TEXT("stage_g_execute_commit_receipt_not_ready");
		return FinalizeAndReturn();
	}

	if (StageGExecuteFinalReport.StageGExecuteCommitReceiptId != StageGExecuteCommitReceipt.RequestId)
	{
		OutBundle.ErrorCode = TEXT("E4202");
		OutBundle.Reason = TEXT("stage_g_execute_commit_receipt_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteFinalReport.StageGExecuteCommitRequestId != StageGExecuteCommitRequest.RequestId)
	{
		OutBundle.ErrorCode = TEXT("E4202");
		OutBundle.Reason = TEXT("stage_g_execute_commit_request_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteFinalReport.StageGExecuteDispatchReceiptId != StageGExecuteDispatchReceipt.RequestId)
	{
		OutBundle.ErrorCode = TEXT("E4202");
		OutBundle.Reason = TEXT("stage_g_execute_dispatch_receipt_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteFinalReport.StageGExecuteDispatchRequestId != StageGExecuteDispatchRequest.RequestId)
	{
		OutBundle.ErrorCode = TEXT("E4202");
		OutBundle.Reason = TEXT("stage_g_execute_dispatch_request_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteFinalReport.StageGExecutePermitTicketId != StageGExecutePermitTicket.RequestId)
	{
		OutBundle.ErrorCode = TEXT("E4202");
		OutBundle.Reason = TEXT("stage_g_execute_permit_ticket_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteFinalReport.StageGWriteEnableRequestId != StageGWriteEnableRequest.RequestId)
	{
		OutBundle.ErrorCode = TEXT("E4202");
		OutBundle.Reason = TEXT("stage_g_write_enable_request_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteFinalReport.StageGExecuteIntentId != StageGExecuteIntent.RequestId)
	{
		OutBundle.ErrorCode = TEXT("E4202");
		OutBundle.Reason = TEXT("stage_g_execute_intent_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteFinalReport.SimHandoffEnvelopeId != SimHandoffEnvelope.RequestId)
	{
		OutBundle.ErrorCode = TEXT("E4202");
		OutBundle.Reason = TEXT("sim_handoff_envelope_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteFinalReport.SimArchiveBundleId != SimArchiveBundle.RequestId)
	{
		OutBundle.ErrorCode = TEXT("E4202");
		OutBundle.Reason = TEXT("sim_archive_bundle_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteFinalReport.SimFinalReportId != SimFinalReport.RequestId)
	{
		OutBundle.ErrorCode = TEXT("E4202");
		OutBundle.Reason = TEXT("sim_final_report_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteFinalReport.SimExecuteReceiptId != SimExecuteReceipt.RequestId)
	{
		OutBundle.ErrorCode = TEXT("E4202");
		OutBundle.Reason = TEXT("sim_execute_receipt_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteFinalReport.ExecuteTicketId != CurrentExecuteTicket.RequestId)
	{
		OutBundle.ErrorCode = TEXT("E4202");
		OutBundle.Reason = TEXT("execute_ticket_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteFinalReport.ConfirmRequestId != CurrentConfirmRequest.RequestId)
	{
		OutBundle.ErrorCode = TEXT("E4202");
		OutBundle.Reason = TEXT("confirm_request_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteFinalReport.ApplyRequestId != CurrentApplyRequest.RequestId)
	{
		OutBundle.ErrorCode = TEXT("E4202");
		OutBundle.Reason = TEXT("apply_request_id_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteFinalReport.ReviewRequestId != CurrentReviewReport.RequestId)
	{
		OutBundle.ErrorCode = TEXT("E4202");
		OutBundle.Reason = TEXT("review_request_id_mismatch");
		return FinalizeAndReturn();
	}

	if (StageGExecuteFinalReport.SelectionDigest != StageGExecuteCommitReceipt.SelectionDigest ||
		StageGExecuteFinalReport.SelectionDigest != StageGExecuteDispatchReceipt.SelectionDigest ||
		StageGExecuteFinalReport.SelectionDigest != StageGWriteEnableRequest.SelectionDigest ||
		StageGExecuteFinalReport.SelectionDigest != StageGExecuteIntent.SelectionDigest ||
		StageGExecuteFinalReport.SelectionDigest != SimHandoffEnvelope.SelectionDigest ||
		StageGExecuteFinalReport.SelectionDigest != SimArchiveBundle.SelectionDigest ||
		StageGExecuteFinalReport.SelectionDigest != SimFinalReport.SelectionDigest ||
		StageGExecuteFinalReport.SelectionDigest != SimExecuteReceipt.SelectionDigest ||
		StageGExecuteFinalReport.SelectionDigest != CurrentExecuteTicket.SelectionDigest ||
		StageGExecuteFinalReport.SelectionDigest != CurrentConfirmRequest.SelectionDigest ||
		StageGExecuteFinalReport.SelectionDigest != CurrentApplyRequest.SelectionDigest)
	{
		OutBundle.ErrorCode = TEXT("E4202");
		OutBundle.Reason = TEXT("selection_digest_mismatch");
		return FinalizeAndReturn();
	}
	if (StageGExecuteFinalReport.SelectionDigest != HCI_BuildSelectionDigestFromReviewReport_G9(CurrentReviewReport))
	{
		OutBundle.ErrorCode = TEXT("E4202");
		OutBundle.Reason = TEXT("selection_digest_mismatch");
		return FinalizeAndReturn();
	}

	if (!ExpectedStageGExecuteCommitReceiptDigest.IsEmpty() && StageGExecuteFinalReport.StageGExecuteCommitReceiptDigest != ExpectedStageGExecuteCommitReceiptDigest)
	{
		OutBundle.ErrorCode = TEXT("E4202");
		OutBundle.Reason = TEXT("stage_g_execute_commit_receipt_digest_mismatch_or_missing");
		return FinalizeAndReturn();
	}
	if (StageGExecuteFinalReport.StageGExecuteCommitReceiptDigest.IsEmpty() ||
		StageGExecuteFinalReport.StageGExecuteCommitReceiptDigest != StageGExecuteCommitReceipt.StageGExecuteCommitReceiptDigest)
	{
		OutBundle.ErrorCode = TEXT("E4202");
		OutBundle.Reason = TEXT("stage_g_execute_commit_receipt_digest_mismatch_or_missing");
		return FinalizeAndReturn();
	}
	if (StageGExecuteFinalReport.StageGExecuteCommitRequestDigest.IsEmpty() ||
		StageGExecuteFinalReport.StageGExecuteCommitRequestDigest != StageGExecuteCommitRequest.StageGExecuteCommitRequestDigest)
	{
		OutBundle.ErrorCode = TEXT("E4202");
		OutBundle.Reason = TEXT("stage_g_execute_commit_request_digest_mismatch_or_missing");
		return FinalizeAndReturn();
	}
	if (StageGExecuteFinalReport.StageGExecuteDispatchReceiptDigest.IsEmpty() ||
		StageGExecuteFinalReport.StageGExecuteDispatchReceiptDigest != StageGExecuteDispatchReceipt.StageGExecuteDispatchReceiptDigest)
	{
		OutBundle.ErrorCode = TEXT("E4202");
		OutBundle.Reason = TEXT("stage_g_execute_dispatch_receipt_digest_mismatch_or_missing");
		return FinalizeAndReturn();
	}
	if (StageGExecuteFinalReport.StageGExecuteDispatchDigest.IsEmpty() ||
		StageGExecuteFinalReport.StageGExecuteDispatchDigest != StageGExecuteDispatchRequest.StageGExecuteDispatchDigest)
	{
		OutBundle.ErrorCode = TEXT("E4202");
		OutBundle.Reason = TEXT("stage_g_execute_dispatch_digest_mismatch_or_missing");
		return FinalizeAndReturn();
	}
	if (StageGExecuteFinalReport.StageGExecutePermitDigest.IsEmpty() ||
		StageGExecuteFinalReport.StageGExecutePermitDigest != StageGExecutePermitTicket.StageGExecutePermitDigest)
	{
		OutBundle.ErrorCode = TEXT("E4202");
		OutBundle.Reason = TEXT("stage_g_execute_permit_digest_mismatch_or_missing");
		return FinalizeAndReturn();
	}
	if (StageGExecuteFinalReport.StageGWriteEnableDigest.IsEmpty() ||
		StageGExecuteFinalReport.StageGWriteEnableDigest != StageGWriteEnableRequest.StageGWriteEnableDigest)
	{
		OutBundle.ErrorCode = TEXT("E4202");
		OutBundle.Reason = TEXT("stage_g_write_enable_digest_mismatch_or_missing");
		return FinalizeAndReturn();
	}
	if (StageGExecuteFinalReport.ExecuteIntentDigest.IsEmpty() ||
		StageGExecuteFinalReport.ExecuteIntentDigest != StageGExecuteIntent.ExecuteIntentDigest)
	{
		OutBundle.ErrorCode = TEXT("E4202");
		OutBundle.Reason = TEXT("execute_intent_digest_mismatch_or_missing");
		return FinalizeAndReturn();
	}
	if (StageGExecuteFinalReport.HandoffDigest.IsEmpty() || StageGExecuteFinalReport.HandoffDigest != SimHandoffEnvelope.HandoffDigest)
	{
		OutBundle.ErrorCode = TEXT("E4202");
		OutBundle.Reason = TEXT("handoff_digest_mismatch_or_missing");
		return FinalizeAndReturn();
	}
	if (StageGExecuteFinalReport.ArchiveDigest.IsEmpty() || StageGExecuteFinalReport.ArchiveDigest != SimArchiveBundle.ArchiveDigest)
	{
		OutBundle.ErrorCode = TEXT("E4202");
		OutBundle.Reason = TEXT("archive_digest_mismatch_or_missing");
		return FinalizeAndReturn();
	}
	if (StageGExecuteFinalReport.StageGExecuteFinalReportDigest.IsEmpty())
	{
		OutBundle.ErrorCode = TEXT("E4202");
		OutBundle.Reason = TEXT("stage_g_execute_final_report_digest_missing");
		return FinalizeAndReturn();
	}

	if (!StageGExecuteFinalReport.ExecuteTarget.Equals(TEXT("stage_g_execute_runtime"), ESearchCase::IgnoreCase))
	{
		OutBundle.ErrorCode = TEXT("E4202");
		OutBundle.Reason = TEXT("execute_target_mismatch");
		return FinalizeAndReturn();
	}
	if (!StageGExecuteFinalReport.HandoffTarget.Equals(TEXT("stage_g_execute"), ESearchCase::IgnoreCase))
	{
		OutBundle.ErrorCode = TEXT("E4202");
		OutBundle.Reason = TEXT("handoff_target_mismatch");
		return FinalizeAndReturn();
	}

	OutBundle.StageGExecuteArchiveBundleStatus = TEXT("ready");
	OutBundle.bWriteEnabled = true;
	OutBundle.bReadyForStageGExecute = true;
	OutBundle.bStageGExecuteFinalized = true;
	OutBundle.bStageGExecuteFinalReportReady = true;
	OutBundle.bStageGExecuteArchived = true;
	OutBundle.bStageGExecuteArchiveBundleReady = true;
	OutBundle.ErrorCode = TEXT("-");
	OutBundle.Reason = TEXT("stage_g_execute_archive_bundle_ready");
	return FinalizeAndReturn();
}