#include "Agent/Bridges/HCIAbilityKitAgentExecutorSimulateExecuteArchiveBundleBridge.h"

#include "Agent/Contracts/StageF/HCIAbilityKitAgentApplyConfirmRequest.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentApplyRequest.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentExecuteTicket.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentSimulateExecuteArchiveBundle.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentSimulateExecuteFinalReport.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentSimulateExecuteReceipt.h"
#include "Agent/Executor/HCIAbilityKitDryRunDiff.h"
#include "Common/HCIAbilityKitTimeFormat.h"
#include "Misc/Crc.h"
#include "Misc/Guid.h"

namespace
{
static FString HCI_BuildSelectionDigestFromReviewReport_F14(const FHCIAbilityKitDryRunDiffReport& Report)
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

static FString HCI_BuildArchiveDigest_F14(const FHCIAbilityKitAgentSimulateExecuteArchiveBundle& Bundle)
{
	FString Canonical;
	Canonical += FString::Printf(
		TEXT("%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s\n"),
		*Bundle.SimFinalReportId,
		*Bundle.SimExecuteReceiptId,
		*Bundle.ExecuteTicketId,
		*Bundle.ConfirmRequestId,
		*Bundle.ApplyRequestId,
		*Bundle.ReviewRequestId,
		*Bundle.SelectionDigest,
		*Bundle.TerminalStatus,
		*Bundle.ArchiveStatus,
		Bundle.bUserConfirmed ? TEXT("1") : TEXT("0"),
		Bundle.bReadyToSimulateExecute ? TEXT("1") : TEXT("0"),
		Bundle.bSimulatedDispatchAccepted ? TEXT("1") : TEXT("0"),
		Bundle.bSimulationCompleted ? TEXT("1") : TEXT("0"),
		Bundle.bArchiveReady ? TEXT("1") : TEXT("0"));

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

	const uint32 Crc = FCrc::StrCrc32(*Canonical);
	return FString::Printf(TEXT("crc32_%08X"), Crc);
}

static void HCI_CopyFinalReportToArchiveBundle(
	const FHCIAbilityKitAgentSimulateExecuteFinalReport& SimFinalReport,
	FHCIAbilityKitAgentSimulateExecuteArchiveBundle& OutBundle)
{
	OutBundle = FHCIAbilityKitAgentSimulateExecuteArchiveBundle();
	OutBundle.RequestId = FString::Printf(TEXT("simarchive_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	OutBundle.SimFinalReportId = SimFinalReport.RequestId;
	OutBundle.SimExecuteReceiptId = SimFinalReport.SimExecuteReceiptId;
	OutBundle.ExecuteTicketId = SimFinalReport.ExecuteTicketId;
	OutBundle.ConfirmRequestId = SimFinalReport.ConfirmRequestId;
	OutBundle.ApplyRequestId = SimFinalReport.ApplyRequestId;
	OutBundle.ReviewRequestId = SimFinalReport.ReviewRequestId;
	OutBundle.SelectionDigest = SimFinalReport.SelectionDigest;
	OutBundle.GeneratedUtc = FHCIAbilityKitTimeFormat::FormatNowBeijingIso8601();
	OutBundle.ExecutionMode = TEXT("simulate_dry_run_archive_bundle");
	OutBundle.TransactionMode = SimFinalReport.TransactionMode;
	OutBundle.TerminationPolicy = SimFinalReport.TerminationPolicy;
	OutBundle.TerminalStatus = SimFinalReport.TerminalStatus;
	OutBundle.ArchiveStatus = TEXT("blocked");
	OutBundle.bUserConfirmed = SimFinalReport.bUserConfirmed;
	OutBundle.bReadyToSimulateExecute = SimFinalReport.bReadyToSimulateExecute;
	OutBundle.bSimulatedDispatchAccepted = SimFinalReport.bSimulatedDispatchAccepted;
	OutBundle.bSimulationCompleted = SimFinalReport.bSimulationCompleted;
	OutBundle.bArchiveReady = false;
	OutBundle.ErrorCode = TEXT("-");
	OutBundle.Reason = TEXT("simulate_archive_bundle_ready");
	OutBundle.Summary = SimFinalReport.Summary;
	OutBundle.Items = SimFinalReport.Items;
}
} // namespace

bool FHCIAbilityKitAgentExecutorSimulateExecuteArchiveBundleBridge::BuildSimulateExecuteArchiveBundle(
	const FHCIAbilityKitAgentSimulateExecuteFinalReport& SimFinalReport,
	const FHCIAbilityKitAgentSimulateExecuteReceipt& SimExecuteReceipt,
	const FHCIAbilityKitAgentExecuteTicket& CurrentExecuteTicket,
	const FHCIAbilityKitAgentApplyConfirmRequest& CurrentConfirmRequest,
	const FHCIAbilityKitAgentApplyRequest& CurrentApplyRequest,
	const FHCIAbilityKitDryRunDiffReport& CurrentReviewReport,
	FHCIAbilityKitAgentSimulateExecuteArchiveBundle& OutBundle)
{
	HCI_CopyFinalReportToArchiveBundle(SimFinalReport, OutBundle);

	if (!SimFinalReport.bUserConfirmed)
	{
		OutBundle.ErrorCode = TEXT("E4005");
		OutBundle.Reason = TEXT("user_not_confirmed");
		OutBundle.ArchiveDigest = HCI_BuildArchiveDigest_F14(OutBundle);
		return true;
	}

	if (!SimFinalReport.bReadyToSimulateExecute)
	{
		OutBundle.ErrorCode = TEXT("E4205");
		OutBundle.Reason = TEXT("execute_ticket_not_ready");
		OutBundle.ArchiveDigest = HCI_BuildArchiveDigest_F14(OutBundle);
		return true;
	}

	if (!SimFinalReport.bSimulatedDispatchAccepted)
	{
		OutBundle.ErrorCode = TEXT("E4206");
		OutBundle.Reason = TEXT("simulate_execute_receipt_not_accepted");
		OutBundle.ArchiveDigest = HCI_BuildArchiveDigest_F14(OutBundle);
		return true;
	}

	if (!SimFinalReport.bSimulationCompleted || !SimFinalReport.TerminalStatus.Equals(TEXT("completed"), ESearchCase::IgnoreCase))
	{
		OutBundle.ErrorCode = TEXT("E4207");
		OutBundle.Reason = TEXT("simulate_final_report_not_completed");
		OutBundle.ArchiveDigest = HCI_BuildArchiveDigest_F14(OutBundle);
		return true;
	}

	if (!CurrentExecuteTicket.bReadyToSimulateExecute)
	{
		OutBundle.ErrorCode = TEXT("E4205");
		OutBundle.Reason = TEXT("execute_ticket_not_ready");
		OutBundle.ArchiveDigest = HCI_BuildArchiveDigest_F14(OutBundle);
		return true;
	}

	if (!CurrentConfirmRequest.bReadyToExecute)
	{
		OutBundle.ErrorCode = TEXT("E4204");
		OutBundle.Reason = TEXT("confirm_request_not_ready");
		OutBundle.ArchiveDigest = HCI_BuildArchiveDigest_F14(OutBundle);
		return true;
	}

	if (SimFinalReport.SimExecuteReceiptId != SimExecuteReceipt.RequestId)
	{
		OutBundle.ErrorCode = TEXT("E4202");
		OutBundle.Reason = TEXT("sim_execute_receipt_id_mismatch");
		OutBundle.ArchiveDigest = HCI_BuildArchiveDigest_F14(OutBundle);
		return true;
	}

	if (SimFinalReport.ExecuteTicketId != CurrentExecuteTicket.RequestId)
	{
		OutBundle.ErrorCode = TEXT("E4202");
		OutBundle.Reason = TEXT("execute_ticket_id_mismatch");
		OutBundle.ArchiveDigest = HCI_BuildArchiveDigest_F14(OutBundle);
		return true;
	}

	if (SimFinalReport.ConfirmRequestId != CurrentConfirmRequest.RequestId)
	{
		OutBundle.ErrorCode = TEXT("E4202");
		OutBundle.Reason = TEXT("confirm_request_id_mismatch");
		OutBundle.ArchiveDigest = HCI_BuildArchiveDigest_F14(OutBundle);
		return true;
	}

	if (SimFinalReport.ApplyRequestId != CurrentApplyRequest.RequestId)
	{
		OutBundle.ErrorCode = TEXT("E4202");
		OutBundle.Reason = TEXT("apply_request_id_mismatch");
		OutBundle.ArchiveDigest = HCI_BuildArchiveDigest_F14(OutBundle);
		return true;
	}

	if (SimFinalReport.ReviewRequestId != CurrentReviewReport.RequestId)
	{
		OutBundle.ErrorCode = TEXT("E4202");
		OutBundle.Reason = TEXT("review_request_id_mismatch");
		OutBundle.ArchiveDigest = HCI_BuildArchiveDigest_F14(OutBundle);
		return true;
	}

	if (SimFinalReport.SelectionDigest != SimExecuteReceipt.SelectionDigest ||
		SimFinalReport.SelectionDigest != CurrentExecuteTicket.SelectionDigest ||
		SimFinalReport.SelectionDigest != CurrentConfirmRequest.SelectionDigest ||
		SimFinalReport.SelectionDigest != CurrentApplyRequest.SelectionDigest)
	{
		OutBundle.ErrorCode = TEXT("E4202");
		OutBundle.Reason = TEXT("selection_digest_mismatch");
		OutBundle.ArchiveDigest = HCI_BuildArchiveDigest_F14(OutBundle);
		return true;
	}

	const FString CurrentSelectionDigest = HCI_BuildSelectionDigestFromReviewReport_F14(CurrentReviewReport);
	if (SimFinalReport.SelectionDigest != CurrentSelectionDigest)
	{
		OutBundle.ErrorCode = TEXT("E4202");
		OutBundle.Reason = TEXT("selection_digest_mismatch");
		OutBundle.ArchiveDigest = HCI_BuildArchiveDigest_F14(OutBundle);
		return true;
	}

	OutBundle.ArchiveStatus = TEXT("ready");
	OutBundle.bArchiveReady = true;
	OutBundle.ErrorCode = TEXT("-");
	OutBundle.Reason = TEXT("simulate_archive_bundle_ready");
	OutBundle.ArchiveDigest = HCI_BuildArchiveDigest_F14(OutBundle);
	return true;
}