#include "Agent/Bridges/HCIAgentExecutorStageGExecutionReadinessReportBridge.h"

#include "Agent/Contracts/StageF/HCIAgentApplyRequest.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecuteArchiveBundle.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecutionReadinessReport.h"
#include "Agent/Executor/HCIDryRunDiff.h"
#include "Misc/Crc.h"
#include "Misc/Guid.h"

namespace
{
static FString HCI_BuildSelectionDigestFromReviewReport_G10(const FHCIDryRunDiffReport& Report)
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
	return FString::Printf(TEXT("crc32_%08X"), FCrc::StrCrc32(*Canonical));
}

static FString HCI_BuildStageGExecutionReadinessDigest_G10(const FHCIAgentStageGExecutionReadinessReport& Report)
{
	FString Canonical;
	Canonical += FString::Printf(
		TEXT("%s|%s|%s|%s|%s|%s|%s|%s\n"),
		*Report.StageGExecuteArchiveBundleId,
		*Report.SelectionDigest,
		*Report.StageGExecutionReadinessStatus,
		Report.bReadyForH1PlannerIntegration ? TEXT("1") : TEXT("0"),
		*Report.ExecutionMode,
		*Report.ErrorCode,
		*Report.Reason,
		Report.Items.Num() > 0 ? TEXT("has_items") : TEXT("no_items"));

	for (const FHCIAgentApplyRequestItem& Item : Report.Items)
	{
		Canonical += FString::Printf(
			TEXT("%d|%s|%s|%s|%s|%s|%s|%s|%s|%s\n"),
			Item.RowIndex,
			*Item.ToolName,
			*Item.AssetPath,
			*Item.Field,
			*Item.SkipReason,
			Item.bBlocked ? TEXT("1") : TEXT("0"),
			*FHCIDryRunDiff::RiskToString(Item.Risk),
			*FHCIDryRunDiff::ObjectTypeToString(Item.ObjectType),
			*FHCIDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
			*Item.EvidenceKey);
	}

	return FString::Printf(TEXT("crc32_%08X"), FCrc::StrCrc32(*Canonical));
}
} // namespace

bool FHCIAgentExecutorStageGExecutionReadinessReportBridge::BuildStageGExecutionReadinessReport(
	const FHCIAgentStageGExecuteArchiveBundle& StageGExecuteArchiveBundle,
	const FString& ExpectedStageGExecuteArchiveBundleId,
	const FHCIDryRunDiffReport& CurrentReviewReport,
	const bool bUserConfirmed,
	FHCIAgentStageGExecutionReadinessReport& OutReport)
{
	OutReport = FHCIAgentStageGExecutionReadinessReport();
	OutReport.RequestId = FString::Printf(TEXT("stagegreadiness_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	OutReport.StageGExecuteArchiveBundleId = StageGExecuteArchiveBundle.RequestId;
	OutReport.SelectionDigest = StageGExecuteArchiveBundle.SelectionDigest;
	OutReport.StageGExecutionReadinessStatus = TEXT("blocked");
	OutReport.bReadyForH1PlannerIntegration = false;
	OutReport.ExecutionMode = TEXT("simulate_dry_run");
	OutReport.ErrorCode = TEXT("-");
	OutReport.Reason = TEXT("stage_g_execution_readiness_pending");
	OutReport.Summary = StageGExecuteArchiveBundle.Summary;
	OutReport.Items = StageGExecuteArchiveBundle.Items;

	auto FinalizeAndReturn = [&OutReport]() -> bool
	{
		OutReport.StageGExecutionReadinessDigest = HCI_BuildStageGExecutionReadinessDigest_G10(OutReport);
		return true;
	};

	if (!ExpectedStageGExecuteArchiveBundleId.IsEmpty() && StageGExecuteArchiveBundle.RequestId != ExpectedStageGExecuteArchiveBundleId)
	{
		OutReport.ErrorCode = TEXT("E4202");
		OutReport.Reason = TEXT("stage_g_execute_archive_bundle_id_mismatch");
		return FinalizeAndReturn();
	}

	if (!bUserConfirmed)
	{
		OutReport.ErrorCode = TEXT("E4005");
		OutReport.Reason = TEXT("user_not_confirmed");
		return FinalizeAndReturn();
	}

	const FString ExpectedSelectionDigest = HCI_BuildSelectionDigestFromReviewReport_G10(CurrentReviewReport);
	if (!ExpectedSelectionDigest.IsEmpty() && StageGExecuteArchiveBundle.SelectionDigest != ExpectedSelectionDigest)
	{
		OutReport.ErrorCode = TEXT("E4202");
		OutReport.Reason = TEXT("selection_digest_mismatch");
		return FinalizeAndReturn();
	}

	const bool bDryRunMode =
		StageGExecuteArchiveBundle.ExecutionMode.Equals(TEXT("simulate_dry_run"), ESearchCase::IgnoreCase) ||
		StageGExecuteArchiveBundle.ExecutionMode.Contains(TEXT("dry_run"), ESearchCase::IgnoreCase);
	if (!bDryRunMode)
	{
		OutReport.ErrorCode = TEXT("E4219");
		OutReport.Reason = TEXT("stage_g_execution_mode_write_not_allowed_in_g10");
		return FinalizeAndReturn();
	}

	const bool bArchiveReady =
		StageGExecuteArchiveBundle.bStageGExecuteArchiveBundleReady &&
		StageGExecuteArchiveBundle.bStageGExecuteArchived &&
		StageGExecuteArchiveBundle.StageGExecuteArchiveBundleStatus.Equals(TEXT("ready"), ESearchCase::IgnoreCase);
	if (!bArchiveReady)
	{
		OutReport.ErrorCode = TEXT("E4218");
		OutReport.Reason = TEXT("stage_g_execute_archive_bundle_not_ready");
		return FinalizeAndReturn();
	}

	OutReport.StageGExecutionReadinessStatus = TEXT("ready");
	OutReport.bReadyForH1PlannerIntegration = true;
	OutReport.ErrorCode = TEXT("-");
	OutReport.Reason = TEXT("stage_g_execution_readiness_ready");
	return FinalizeAndReturn();
}

