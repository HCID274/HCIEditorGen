#include "Agent/HCIAbilityKitAgentExecutorSimulateExecuteReceiptBridge.h"

#include "Agent/HCIAbilityKitAgentApplyConfirmRequest.h"
#include "Agent/HCIAbilityKitAgentApplyRequest.h"
#include "Agent/HCIAbilityKitAgentExecuteTicket.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteReceipt.h"
#include "Agent/HCIAbilityKitDryRunDiff.h"
#include "Common/HCIAbilityKitTimeFormat.h"
#include "Misc/Crc.h"
#include "Misc/Guid.h"

namespace
{
static FString HCI_BuildSelectionDigestFromReviewReport_F12(const FHCIAbilityKitDryRunDiffReport& Report)
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

static void HCI_CopyExecuteTicketToReceipt(
	const FHCIAbilityKitAgentExecuteTicket& ExecuteTicket,
	FHCIAbilityKitAgentSimulateExecuteReceipt& OutReceipt)
{
	OutReceipt = FHCIAbilityKitAgentSimulateExecuteReceipt();
	OutReceipt.RequestId = FString::Printf(TEXT("receipt_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	OutReceipt.ExecuteTicketId = ExecuteTicket.RequestId;
	OutReceipt.ConfirmRequestId = ExecuteTicket.ConfirmRequestId;
	OutReceipt.ApplyRequestId = ExecuteTicket.ApplyRequestId;
	OutReceipt.ReviewRequestId = ExecuteTicket.ReviewRequestId;
	OutReceipt.SelectionDigest = ExecuteTicket.SelectionDigest;
	OutReceipt.GeneratedUtc = FHCIAbilityKitTimeFormat::FormatNowBeijingIso8601();
	OutReceipt.ExecutionMode = TEXT("simulate_dry_run_execute_receipt");
	OutReceipt.TransactionMode = ExecuteTicket.TransactionMode;
	OutReceipt.TerminationPolicy = ExecuteTicket.TerminationPolicy;
	OutReceipt.bUserConfirmed = ExecuteTicket.bUserConfirmed;
	OutReceipt.bReadyToSimulateExecute = ExecuteTicket.bReadyToSimulateExecute;
	OutReceipt.bSimulatedDispatchAccepted = false;
	OutReceipt.ErrorCode = TEXT("-");
	OutReceipt.Reason = TEXT("simulate_execute_receipt_ready");
	OutReceipt.Summary = ExecuteTicket.Summary;
	OutReceipt.Items = ExecuteTicket.Items;
}
} // namespace

bool FHCIAbilityKitAgentExecutorSimulateExecuteReceiptBridge::BuildSimulateExecuteReceipt(
	const FHCIAbilityKitAgentExecuteTicket& ExecuteTicket,
	const FHCIAbilityKitAgentApplyConfirmRequest& CurrentConfirmRequest,
	const FHCIAbilityKitAgentApplyRequest& CurrentApplyRequest,
	const FHCIAbilityKitDryRunDiffReport& CurrentReviewReport,
	FHCIAbilityKitAgentSimulateExecuteReceipt& OutReceipt)
{
	HCI_CopyExecuteTicketToReceipt(ExecuteTicket, OutReceipt);

	if (!ExecuteTicket.bUserConfirmed)
	{
		OutReceipt.bSimulatedDispatchAccepted = false;
		OutReceipt.ErrorCode = TEXT("E4005");
		OutReceipt.Reason = TEXT("user_not_confirmed");
		return true;
	}

	if (!ExecuteTicket.bReadyToSimulateExecute)
	{
		OutReceipt.bSimulatedDispatchAccepted = false;
		OutReceipt.ErrorCode = TEXT("E4205");
		OutReceipt.Reason = TEXT("execute_ticket_not_ready");
		return true;
	}

	if (!CurrentConfirmRequest.bReadyToExecute)
	{
		OutReceipt.bSimulatedDispatchAccepted = false;
		OutReceipt.ErrorCode = TEXT("E4204");
		OutReceipt.Reason = TEXT("confirm_request_not_ready");
		return true;
	}

	if (ExecuteTicket.ConfirmRequestId != CurrentConfirmRequest.RequestId)
	{
		OutReceipt.bSimulatedDispatchAccepted = false;
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("confirm_request_id_mismatch");
		return true;
	}

	if (ExecuteTicket.ApplyRequestId != CurrentApplyRequest.RequestId)
	{
		OutReceipt.bSimulatedDispatchAccepted = false;
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("apply_request_id_mismatch");
		return true;
	}

	if (ExecuteTicket.ReviewRequestId != CurrentReviewReport.RequestId)
	{
		OutReceipt.bSimulatedDispatchAccepted = false;
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("review_request_id_mismatch");
		return true;
	}

	if (ExecuteTicket.SelectionDigest != CurrentConfirmRequest.SelectionDigest ||
		ExecuteTicket.SelectionDigest != CurrentApplyRequest.SelectionDigest)
	{
		OutReceipt.bSimulatedDispatchAccepted = false;
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("selection_digest_mismatch");
		return true;
	}

	const FString CurrentSelectionDigest = HCI_BuildSelectionDigestFromReviewReport_F12(CurrentReviewReport);
	if (ExecuteTicket.SelectionDigest != CurrentSelectionDigest)
	{
		OutReceipt.bSimulatedDispatchAccepted = false;
		OutReceipt.ErrorCode = TEXT("E4202");
		OutReceipt.Reason = TEXT("selection_digest_mismatch");
		return true;
	}

	OutReceipt.bSimulatedDispatchAccepted = true;
	OutReceipt.ErrorCode = TEXT("-");
	OutReceipt.Reason = TEXT("simulate_execute_receipt_ready");
	return true;
}
