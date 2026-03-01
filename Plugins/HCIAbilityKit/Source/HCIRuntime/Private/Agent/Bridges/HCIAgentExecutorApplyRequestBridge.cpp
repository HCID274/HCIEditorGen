#include "Agent/Bridges/HCIAgentExecutorApplyRequestBridge.h"

#include "Agent/Contracts/StageF/HCIAgentApplyRequest.h"
#include "Agent/Executor/HCIDryRunDiff.h"
#include "Common/HCITimeFormat.h"
#include "Misc/Crc.h"
#include "Misc/Guid.h"

namespace
{
static FString HCI_BuildSelectionDigest(const FHCIDryRunDiffReport& Report)
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
} // namespace

bool FHCIAgentExecutorApplyRequestBridge::BuildApplyRequest(
	const FHCIDryRunDiffReport& SelectedReviewReport,
	FHCIAgentApplyRequest& OutApplyRequest)
{
	OutApplyRequest = FHCIAgentApplyRequest();
	OutApplyRequest.RequestId = FString::Printf(TEXT("apply_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	OutApplyRequest.ReviewRequestId = SelectedReviewReport.RequestId;
	OutApplyRequest.SelectionDigest = HCI_BuildSelectionDigest(SelectedReviewReport);
	OutApplyRequest.GeneratedUtc = FHCITimeFormat::FormatNowBeijingIso8601();
	OutApplyRequest.ExecutionMode = TEXT("simulate_dry_run_apply_request");
	OutApplyRequest.Items.Reserve(SelectedReviewReport.DiffItems.Num());

	for (int32 RowIndex = 0; RowIndex < SelectedReviewReport.DiffItems.Num(); ++RowIndex)
	{
		const FHCIDryRunDiffItem& SourceItem = SelectedReviewReport.DiffItems[RowIndex];
		FHCIAgentApplyRequestItem& Item = OutApplyRequest.Items.AddDefaulted_GetRef();
		Item.RowIndex = RowIndex;
		Item.ToolName = SourceItem.ToolName;
		Item.Risk = SourceItem.Risk;
		Item.AssetPath = SourceItem.AssetPath;
		Item.Field = SourceItem.Field;
		Item.SkipReason = SourceItem.SkipReason;
		Item.bBlocked = !SourceItem.SkipReason.IsEmpty();
		Item.ObjectType = SourceItem.ObjectType;
		Item.LocateStrategy = SourceItem.LocateStrategy;
		Item.EvidenceKey = SourceItem.EvidenceKey;
		Item.ActorPath = SourceItem.ActorPath;

		++OutApplyRequest.Summary.TotalRows;
		++OutApplyRequest.Summary.SelectedRows;
		if (Item.bBlocked)
		{
			++OutApplyRequest.Summary.BlockedRows;
		}
		else
		{
			++OutApplyRequest.Summary.ModifiableRows;
		}

		switch (Item.Risk)
		{
		case EHCIDryRunRisk::ReadOnly:
			++OutApplyRequest.Summary.ReadOnlyRows;
			break;
		case EHCIDryRunRisk::Write:
			++OutApplyRequest.Summary.WriteRows;
			break;
		case EHCIDryRunRisk::Destructive:
			++OutApplyRequest.Summary.DestructiveRows;
			break;
		default:
			break;
		}
	}

	OutApplyRequest.bReady = (OutApplyRequest.Summary.BlockedRows == 0);
	return true;
}

