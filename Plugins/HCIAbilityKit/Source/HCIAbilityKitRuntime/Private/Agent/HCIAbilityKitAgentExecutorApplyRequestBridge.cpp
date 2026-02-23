#include "Agent/HCIAbilityKitAgentExecutorApplyRequestBridge.h"

#include "Agent/HCIAbilityKitAgentApplyRequest.h"
#include "Agent/HCIAbilityKitDryRunDiff.h"
#include "Common/HCIAbilityKitTimeFormat.h"
#include "Misc/Crc.h"
#include "Misc/Guid.h"

namespace
{
static FString HCI_BuildSelectionDigest(const FHCIAbilityKitDryRunDiffReport& Report)
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
} // namespace

bool FHCIAbilityKitAgentExecutorApplyRequestBridge::BuildApplyRequest(
	const FHCIAbilityKitDryRunDiffReport& SelectedReviewReport,
	FHCIAbilityKitAgentApplyRequest& OutApplyRequest)
{
	OutApplyRequest = FHCIAbilityKitAgentApplyRequest();
	OutApplyRequest.RequestId = FString::Printf(TEXT("apply_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	OutApplyRequest.ReviewRequestId = SelectedReviewReport.RequestId;
	OutApplyRequest.SelectionDigest = HCI_BuildSelectionDigest(SelectedReviewReport);
	OutApplyRequest.GeneratedUtc = FHCIAbilityKitTimeFormat::FormatNowBeijingIso8601();
	OutApplyRequest.ExecutionMode = TEXT("simulate_dry_run_apply_request");
	OutApplyRequest.Items.Reserve(SelectedReviewReport.DiffItems.Num());

	for (int32 RowIndex = 0; RowIndex < SelectedReviewReport.DiffItems.Num(); ++RowIndex)
	{
		const FHCIAbilityKitDryRunDiffItem& SourceItem = SelectedReviewReport.DiffItems[RowIndex];
		FHCIAbilityKitAgentApplyRequestItem& Item = OutApplyRequest.Items.AddDefaulted_GetRef();
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
		case EHCIAbilityKitDryRunRisk::ReadOnly:
			++OutApplyRequest.Summary.ReadOnlyRows;
			break;
		case EHCIAbilityKitDryRunRisk::Write:
			++OutApplyRequest.Summary.WriteRows;
			break;
		case EHCIAbilityKitDryRunRisk::Destructive:
			++OutApplyRequest.Summary.DestructiveRows;
			break;
		default:
			break;
		}
	}

	OutApplyRequest.bReady = (OutApplyRequest.Summary.BlockedRows == 0);
	return true;
}

