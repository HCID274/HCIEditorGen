#include "Agent/HCIAbilityKitAgentExecutorApplyConfirmBridge.h"

#include "Agent/HCIAbilityKitAgentApplyConfirmRequest.h"
#include "Agent/HCIAbilityKitAgentApplyRequest.h"
#include "Agent/HCIAbilityKitDryRunDiff.h"
#include "Common/HCIAbilityKitTimeFormat.h"
#include "Misc/Crc.h"
#include "Misc/Guid.h"

namespace
{
static FString HCI_BuildSelectionDigestFromReviewReport(const FHCIAbilityKitDryRunDiffReport& Report)
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

static void HCI_CopyApplyRequestToConfirmRequest(
	const FHCIAbilityKitAgentApplyRequest& ApplyRequest,
	const bool bUserConfirmed,
	FHCIAbilityKitAgentApplyConfirmRequest& OutConfirmRequest)
{
	OutConfirmRequest = FHCIAbilityKitAgentApplyConfirmRequest();
	OutConfirmRequest.RequestId = FString::Printf(TEXT("confirm_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	OutConfirmRequest.ApplyRequestId = ApplyRequest.RequestId;
	OutConfirmRequest.ReviewRequestId = ApplyRequest.ReviewRequestId;
	OutConfirmRequest.SelectionDigest = ApplyRequest.SelectionDigest;
	OutConfirmRequest.GeneratedUtc = FHCIAbilityKitTimeFormat::FormatNowBeijingIso8601();
	OutConfirmRequest.ExecutionMode = TEXT("simulate_dry_run_apply_confirm_request");
	OutConfirmRequest.bUserConfirmed = bUserConfirmed;
	OutConfirmRequest.Summary = ApplyRequest.Summary;
	OutConfirmRequest.Items = ApplyRequest.Items;
	OutConfirmRequest.bReadyToExecute = false;
	OutConfirmRequest.ErrorCode = TEXT("-");
	OutConfirmRequest.Reason = TEXT("confirm_request_ready");
}
} // namespace

bool FHCIAbilityKitAgentExecutorApplyConfirmBridge::BuildConfirmRequest(
	const FHCIAbilityKitAgentApplyRequest& ApplyRequest,
	const FHCIAbilityKitDryRunDiffReport& CurrentReviewReport,
	const bool bUserConfirmed,
	FHCIAbilityKitAgentApplyConfirmRequest& OutConfirmRequest)
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

