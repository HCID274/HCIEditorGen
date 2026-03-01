#include "Agent/Bridges/HCIAgentExecutorExecuteTicketBridge.h"

#include "Agent/Contracts/StageF/HCIAgentApplyConfirmRequest.h"
#include "Agent/Contracts/StageF/HCIAgentApplyRequest.h"
#include "Agent/Contracts/StageF/HCIAgentExecuteTicket.h"
#include "Agent/Executor/HCIDryRunDiff.h"
#include "Common/HCITimeFormat.h"
#include "Misc/Crc.h"
#include "Misc/Guid.h"

namespace
{
static FString HCI_BuildSelectionDigestFromReviewReport_F11(const FHCIDryRunDiffReport& Report)
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

static void HCI_CopyConfirmRequestToExecuteTicket(
	const FHCIAgentApplyConfirmRequest& ConfirmRequest,
	FHCIAgentExecuteTicket& OutExecuteTicket)
{
	OutExecuteTicket = FHCIAgentExecuteTicket();
	OutExecuteTicket.RequestId = FString::Printf(TEXT("exec_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	OutExecuteTicket.ConfirmRequestId = ConfirmRequest.RequestId;
	OutExecuteTicket.ApplyRequestId = ConfirmRequest.ApplyRequestId;
	OutExecuteTicket.ReviewRequestId = ConfirmRequest.ReviewRequestId;
	OutExecuteTicket.SelectionDigest = ConfirmRequest.SelectionDigest;
	OutExecuteTicket.GeneratedUtc = FHCITimeFormat::FormatNowBeijingIso8601();
	OutExecuteTicket.ExecutionMode = TEXT("simulate_dry_run_execute_ticket");
	OutExecuteTicket.TransactionMode = TEXT("all_or_nothing");
	OutExecuteTicket.TerminationPolicy = TEXT("stop_on_first_failure");
	OutExecuteTicket.bUserConfirmed = ConfirmRequest.bUserConfirmed;
	OutExecuteTicket.bReadyToSimulateExecute = false;
	OutExecuteTicket.ErrorCode = TEXT("-");
	OutExecuteTicket.Reason = TEXT("execute_ticket_ready");
	OutExecuteTicket.Summary = ConfirmRequest.Summary;
	OutExecuteTicket.Items = ConfirmRequest.Items;
}
} // namespace

bool FHCIAgentExecutorExecuteTicketBridge::BuildExecuteTicket(
	const FHCIAgentApplyConfirmRequest& ConfirmRequest,
	const FHCIAgentApplyRequest& CurrentApplyRequest,
	const FHCIDryRunDiffReport& CurrentReviewReport,
	FHCIAgentExecuteTicket& OutExecuteTicket)
{
	HCI_CopyConfirmRequestToExecuteTicket(ConfirmRequest, OutExecuteTicket);

	if (!ConfirmRequest.bUserConfirmed)
	{
		OutExecuteTicket.bReadyToSimulateExecute = false;
		OutExecuteTicket.ErrorCode = TEXT("E4005");
		OutExecuteTicket.Reason = TEXT("user_not_confirmed");
		return true;
	}

	if (!ConfirmRequest.bReadyToExecute)
	{
		OutExecuteTicket.bReadyToSimulateExecute = false;
		OutExecuteTicket.ErrorCode = TEXT("E4204");
		OutExecuteTicket.Reason = TEXT("confirm_request_not_ready");
		return true;
	}

	if (ConfirmRequest.ApplyRequestId != CurrentApplyRequest.RequestId)
	{
		OutExecuteTicket.bReadyToSimulateExecute = false;
		OutExecuteTicket.ErrorCode = TEXT("E4202");
		OutExecuteTicket.Reason = TEXT("apply_request_id_mismatch");
		return true;
	}

	if (ConfirmRequest.ReviewRequestId != CurrentReviewReport.RequestId)
	{
		OutExecuteTicket.bReadyToSimulateExecute = false;
		OutExecuteTicket.ErrorCode = TEXT("E4202");
		OutExecuteTicket.Reason = TEXT("review_request_id_mismatch");
		return true;
	}

	if (ConfirmRequest.SelectionDigest != CurrentApplyRequest.SelectionDigest)
	{
		OutExecuteTicket.bReadyToSimulateExecute = false;
		OutExecuteTicket.ErrorCode = TEXT("E4202");
		OutExecuteTicket.Reason = TEXT("selection_digest_mismatch");
		return true;
	}

	const FString CurrentSelectionDigest = HCI_BuildSelectionDigestFromReviewReport_F11(CurrentReviewReport);
	if (ConfirmRequest.SelectionDigest != CurrentSelectionDigest)
	{
		OutExecuteTicket.bReadyToSimulateExecute = false;
		OutExecuteTicket.ErrorCode = TEXT("E4202");
		OutExecuteTicket.Reason = TEXT("selection_digest_mismatch");
		return true;
	}

	OutExecuteTicket.bReadyToSimulateExecute = true;
	OutExecuteTicket.ErrorCode = TEXT("-");
	OutExecuteTicket.Reason = TEXT("execute_ticket_ready");
	return true;
}

