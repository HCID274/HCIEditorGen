#include "Agent/Bridges/HCIAbilityKitAgentExecutorExecuteTicketBridge.h"

#include "Agent/Contracts/StageF/HCIAbilityKitAgentApplyConfirmRequest.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentApplyRequest.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentExecuteTicket.h"
#include "Agent/Executor/HCIAbilityKitDryRunDiff.h"
#include "Common/HCIAbilityKitTimeFormat.h"
#include "Misc/Crc.h"
#include "Misc/Guid.h"

namespace
{
static FString HCI_BuildSelectionDigestFromReviewReport_F11(const FHCIAbilityKitDryRunDiffReport& Report)
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

static void HCI_CopyConfirmRequestToExecuteTicket(
	const FHCIAbilityKitAgentApplyConfirmRequest& ConfirmRequest,
	FHCIAbilityKitAgentExecuteTicket& OutExecuteTicket)
{
	OutExecuteTicket = FHCIAbilityKitAgentExecuteTicket();
	OutExecuteTicket.RequestId = FString::Printf(TEXT("exec_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	OutExecuteTicket.ConfirmRequestId = ConfirmRequest.RequestId;
	OutExecuteTicket.ApplyRequestId = ConfirmRequest.ApplyRequestId;
	OutExecuteTicket.ReviewRequestId = ConfirmRequest.ReviewRequestId;
	OutExecuteTicket.SelectionDigest = ConfirmRequest.SelectionDigest;
	OutExecuteTicket.GeneratedUtc = FHCIAbilityKitTimeFormat::FormatNowBeijingIso8601();
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

bool FHCIAbilityKitAgentExecutorExecuteTicketBridge::BuildExecuteTicket(
	const FHCIAbilityKitAgentApplyConfirmRequest& ConfirmRequest,
	const FHCIAbilityKitAgentApplyRequest& CurrentApplyRequest,
	const FHCIAbilityKitDryRunDiffReport& CurrentReviewReport,
	FHCIAbilityKitAgentExecuteTicket& OutExecuteTicket)
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
