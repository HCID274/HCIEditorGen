#include "Agent/Bridges/HCIAgentExecutorApplyConfirmBridge.h"

#include "Agent/Contracts/StageF/HCIAgentApplyConfirmRequest.h"
#include "Agent/Contracts/StageF/HCIAgentApplyRequest.h"
#include "Agent/Executor/HCIDryRunDiff.h"
#include "Common/HCITimeFormat.h"
#include "Misc/Crc.h"
#include "Misc/Guid.h"

namespace
{
static FString HCI_BuildSelectionDigestFromReviewReport(const FHCIDryRunDiffReport& Report)
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

static void HCI_CopyApplyRequestToConfirmRequest(
	const FHCIAgentApplyRequest& ApplyRequest,
	const bool bUserConfirmed,
	FHCIAgentApplyConfirmRequest& OutConfirmRequest)
{
	OutConfirmRequest = FHCIAgentApplyConfirmRequest();
	OutConfirmRequest.RequestId = FString::Printf(TEXT("confirm_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	OutConfirmRequest.ApplyRequestId = ApplyRequest.RequestId;
	OutConfirmRequest.ReviewRequestId = ApplyRequest.ReviewRequestId;
	OutConfirmRequest.SelectionDigest = ApplyRequest.SelectionDigest;
	OutConfirmRequest.GeneratedUtc = FHCITimeFormat::FormatNowBeijingIso8601();
	OutConfirmRequest.ExecutionMode = TEXT("simulate_dry_run_apply_confirm_request");
	OutConfirmRequest.bUserConfirmed = bUserConfirmed;
	OutConfirmRequest.Summary = ApplyRequest.Summary;
	OutConfirmRequest.Items = ApplyRequest.Items;
	OutConfirmRequest.bReadyToExecute = false;
	OutConfirmRequest.ErrorCode = TEXT("-");
	OutConfirmRequest.Reason = TEXT("confirm_request_ready");
}
} // namespace

bool FHCIAgentExecutorApplyConfirmBridge::BuildConfirmRequest(
	const FHCIAgentApplyRequest& ApplyRequest,
	const FHCIDryRunDiffReport& CurrentReviewReport,
	const bool bUserConfirmed,
	FHCIAgentApplyConfirmRequest& OutConfirmRequest)
{
	HCI_CopyApplyRequestToConfirmRequest(ApplyRequest, bUserConfirmed, OutConfirmRequest);

	if (!bUserConfirmed)
	{
		OutConfirmRequest.bReadyToExecute = false;
		OutConfirmRequest.ErrorCode = TEXT("E4005");
		OutConfirmRequest.Reason = TEXT("user_not_confirmed");
		return true;
	}

	if (!ApplyRequest.bReady || ApplyRequest.Summary.BlockedRows > 0)
	{
		OutConfirmRequest.bReadyToExecute = false;
		OutConfirmRequest.ErrorCode = TEXT("E4203");
		OutConfirmRequest.Reason = TEXT("apply_request_not_ready_blocked_rows_present");
		return true;
	}

	if (ApplyRequest.ReviewRequestId != CurrentReviewReport.RequestId)
	{
		OutConfirmRequest.bReadyToExecute = false;
		OutConfirmRequest.ErrorCode = TEXT("E4202");
		OutConfirmRequest.Reason = TEXT("review_request_id_mismatch");
		return true;
	}

	const FString CurrentSelectionDigest = HCI_BuildSelectionDigestFromReviewReport(CurrentReviewReport);
	if (ApplyRequest.SelectionDigest != CurrentSelectionDigest)
	{
		OutConfirmRequest.bReadyToExecute = false;
		OutConfirmRequest.ErrorCode = TEXT("E4202");
		OutConfirmRequest.Reason = TEXT("selection_digest_mismatch");
		return true;
	}

	OutConfirmRequest.bReadyToExecute = true;
	OutConfirmRequest.ErrorCode = TEXT("-");
	OutConfirmRequest.Reason = TEXT("confirm_request_ready");
	return true;
}

