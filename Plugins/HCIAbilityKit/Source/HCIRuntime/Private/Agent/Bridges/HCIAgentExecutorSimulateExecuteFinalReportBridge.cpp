#include "Agent/Bridges/HCIAgentExecutorSimulateExecuteFinalReportBridge.h"

#include "Agent/Contracts/StageF/HCIAgentApplyConfirmRequest.h"
#include "Agent/Contracts/StageF/HCIAgentApplyRequest.h"
#include "Agent/Contracts/StageF/HCIAgentExecuteTicket.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteFinalReport.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteReceipt.h"
#include "Agent/Executor/HCIDryRunDiff.h"
#include "Common/HCITimeFormat.h"
#include "Misc/Crc.h"
#include "Misc/Guid.h"

namespace
{
static FString HCI_BuildSelectionDigestFromReviewReport_F13(const FHCIDryRunDiffReport& Report)
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

	const uint32 Crc = FCrc::StrCrc32(*Canonical);
	return FString::Printf(TEXT("crc32_%08X"), Crc);
}

static void HCI_CopyReceiptToFinalReport(
	const FHCIAgentSimulateExecuteReceipt& Receipt,
	FHCIAgentSimulateExecuteFinalReport& OutReport)
{
	OutReport = FHCIAgentSimulateExecuteFinalReport();
	OutReport.RequestId = FString::Printf(TEXT("simfinal_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	OutReport.SimExecuteReceiptId = Receipt.RequestId;
	OutReport.ExecuteTicketId = Receipt.ExecuteTicketId;
	OutReport.ConfirmRequestId = Receipt.ConfirmRequestId;
	OutReport.ApplyRequestId = Receipt.ApplyRequestId;
	OutReport.ReviewRequestId = Receipt.ReviewRequestId;
	OutReport.SelectionDigest = Receipt.SelectionDigest;
	OutReport.GeneratedUtc = FHCITimeFormat::FormatNowBeijingIso8601();
	OutReport.ExecutionMode = TEXT("simulate_dry_run_final_report");
	OutReport.TransactionMode = Receipt.TransactionMode;
	OutReport.TerminationPolicy = Receipt.TerminationPolicy;
	OutReport.TerminalStatus = TEXT("blocked");
	OutReport.bUserConfirmed = Receipt.bUserConfirmed;
	OutReport.bReadyToSimulateExecute = Receipt.bReadyToSimulateExecute;
	OutReport.bSimulatedDispatchAccepted = Receipt.bSimulatedDispatchAccepted;
	OutReport.bSimulationCompleted = false;
	OutReport.ErrorCode = TEXT("-");
	OutReport.Reason = TEXT("simulate_execute_final_report_ready");
	OutReport.Summary = Receipt.Summary;
	OutReport.Items = Receipt.Items;
}
} // namespace

bool FHCIAgentExecutorSimulateExecuteFinalReportBridge::BuildSimulateExecuteFinalReport(
	const FHCIAgentSimulateExecuteReceipt& SimExecuteReceipt,
	const FHCIAgentExecuteTicket& CurrentExecuteTicket,
	const FHCIAgentApplyConfirmRequest& CurrentConfirmRequest,
	const FHCIAgentApplyRequest& CurrentApplyRequest,
	const FHCIDryRunDiffReport& CurrentReviewReport,
	FHCIAgentSimulateExecuteFinalReport& OutReport)
{
	HCI_CopyReceiptToFinalReport(SimExecuteReceipt, OutReport);

	if (!SimExecuteReceipt.bUserConfirmed)
	{
		OutReport.ErrorCode = TEXT("E4005");
		OutReport.Reason = TEXT("user_not_confirmed");
		return true;
	}

	if (!SimExecuteReceipt.bReadyToSimulateExecute)
	{
		OutReport.ErrorCode = TEXT("E4205");
		OutReport.Reason = TEXT("execute_ticket_not_ready");
		return true;
	}

	if (!SimExecuteReceipt.bSimulatedDispatchAccepted)
	{
		OutReport.ErrorCode = TEXT("E4206");
		OutReport.Reason = TEXT("simulate_execute_receipt_not_accepted");
		return true;
	}

	if (!CurrentExecuteTicket.bReadyToSimulateExecute)
	{
		OutReport.ErrorCode = TEXT("E4205");
		OutReport.Reason = TEXT("execute_ticket_not_ready");
		return true;
	}

	if (!CurrentConfirmRequest.bReadyToExecute)
	{
		OutReport.ErrorCode = TEXT("E4204");
		OutReport.Reason = TEXT("confirm_request_not_ready");
		return true;
	}

	if (SimExecuteReceipt.ExecuteTicketId != CurrentExecuteTicket.RequestId)
	{
		OutReport.ErrorCode = TEXT("E4202");
		OutReport.Reason = TEXT("execute_ticket_id_mismatch");
		return true;
	}

	if (SimExecuteReceipt.ConfirmRequestId != CurrentConfirmRequest.RequestId)
	{
		OutReport.ErrorCode = TEXT("E4202");
		OutReport.Reason = TEXT("confirm_request_id_mismatch");
		return true;
	}

	if (SimExecuteReceipt.ApplyRequestId != CurrentApplyRequest.RequestId)
	{
		OutReport.ErrorCode = TEXT("E4202");
		OutReport.Reason = TEXT("apply_request_id_mismatch");
		return true;
	}

	if (SimExecuteReceipt.ReviewRequestId != CurrentReviewReport.RequestId)
	{
		OutReport.ErrorCode = TEXT("E4202");
		OutReport.Reason = TEXT("review_request_id_mismatch");
		return true;
	}

	if (SimExecuteReceipt.SelectionDigest != CurrentExecuteTicket.SelectionDigest ||
		SimExecuteReceipt.SelectionDigest != CurrentConfirmRequest.SelectionDigest ||
		SimExecuteReceipt.SelectionDigest != CurrentApplyRequest.SelectionDigest)
	{
		OutReport.ErrorCode = TEXT("E4202");
		OutReport.Reason = TEXT("selection_digest_mismatch");
		return true;
	}

	const FString CurrentSelectionDigest = HCI_BuildSelectionDigestFromReviewReport_F13(CurrentReviewReport);
	if (SimExecuteReceipt.SelectionDigest != CurrentSelectionDigest)
	{
		OutReport.ErrorCode = TEXT("E4202");
		OutReport.Reason = TEXT("selection_digest_mismatch");
		return true;
	}

	OutReport.bSimulationCompleted = true;
	OutReport.TerminalStatus = TEXT("completed");
	OutReport.ErrorCode = TEXT("-");
	OutReport.Reason = TEXT("simulate_execute_final_report_ready");
	return true;
}
