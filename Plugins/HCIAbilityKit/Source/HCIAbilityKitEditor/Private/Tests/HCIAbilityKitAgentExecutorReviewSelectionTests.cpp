#if WITH_DEV_AUTOMATION_TESTS

#include "Agent/HCIAbilityKitDryRunDiff.h"
#include "Agent/HCIAbilityKitDryRunDiffJsonSerializer.h"
#include "Agent/HCIAbilityKitDryRunDiffSelection.h"
#include "Misc/AutomationTest.h"

namespace
{
static FHCIAbilityKitDryRunDiffReport MakeF8ReviewReport()
{
	FHCIAbilityKitDryRunDiffReport Report;
	Report.RequestId = TEXT("req_f8_review");

	FHCIAbilityKitDryRunDiffItem& ActorItem = Report.DiffItems.AddDefaulted_GetRef();
	ActorItem.AssetPath = TEXT("PersistentLevel.Actor_A");
	ActorItem.ActorPath = TEXT("/Game/Maps/UEDPIE_0_Map.Map:PersistentLevel.Actor_A");
	ActorItem.Field = TEXT("step:s1");
	ActorItem.ToolName = TEXT("ScanLevelMeshRisks");
	ActorItem.ObjectType = EHCIAbilityKitDryRunObjectType::Actor;
	ActorItem.LocateStrategy = EHCIAbilityKitDryRunLocateStrategy::CameraFocus;
	ActorItem.EvidenceKey = TEXT("actor_path");

	FHCIAbilityKitDryRunDiffItem& AssetItem = Report.DiffItems.AddDefaulted_GetRef();
	AssetItem.AssetPath = TEXT("/Game/Art/T_F8.T_F8");
	AssetItem.Field = TEXT("step:s2");
	AssetItem.ToolName = TEXT("SetTextureMaxSize");
	AssetItem.ObjectType = EHCIAbilityKitDryRunObjectType::Asset;
	AssetItem.LocateStrategy = EHCIAbilityKitDryRunLocateStrategy::SyncBrowser;
	AssetItem.EvidenceKey = TEXT("asset_path");
	AssetItem.SkipReason = TEXT("error_code=E4005;preflight_gate=confirm_gate");

	FHCIAbilityKitDryRunDiff::NormalizeAndFinalize(Report);
	return Report;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorReviewSelectionDedupTest,
	"HCIAbilityKit.Editor.AgentExecutorReviewSelect.SelectRowsDedupPreservesOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorReviewSelectionDedupTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Report = MakeF8ReviewReport();
	const TArray<int32> RequestedRows = {1, 1, 0};

	FHCIAbilityKitDryRunDiffSelectionResult Result;
	TestTrue(TEXT("Selection should succeed"), FHCIAbilityKitDryRunDiffSelection::SelectRows(Report, RequestedRows, Result));
	TestTrue(TEXT("Result success flag"), Result.bSuccess);
	TestEqual(TEXT("Reason should be ok"), Result.Reason, FString(TEXT("ok")));
	TestEqual(TEXT("InputRowCount"), Result.InputRowCount, 3);
	TestEqual(TEXT("DroppedDuplicateRows"), Result.DroppedDuplicateRows, 1);
	TestEqual(TEXT("UniqueRowCount"), Result.UniqueRowCount, 2);
	TestEqual(TEXT("TotalRowsBefore"), Result.TotalRowsBefore, 2);
	TestEqual(TEXT("TotalRowsAfter"), Result.TotalRowsAfter, 2);
	TestEqual(TEXT("Selected first row keeps first appearance order"), Result.AppliedRowIndices[0], 1);
	TestEqual(TEXT("Selected second row keeps first appearance order"), Result.AppliedRowIndices[1], 0);
	TestEqual(TEXT("First selected tool"), Result.SelectedReport.DiffItems[0].ToolName, FString(TEXT("SetTextureMaxSize")));
	TestEqual(TEXT("Second selected tool"), Result.SelectedReport.DiffItems[1].ToolName, FString(TEXT("ScanLevelMeshRisks")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorReviewSelectionOutOfRangeTest,
	"HCIAbilityKit.Editor.AgentExecutorReviewSelect.SelectRowsRejectsOutOfRange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorReviewSelectionOutOfRangeTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Report = MakeF8ReviewReport();

	FHCIAbilityKitDryRunDiffSelectionResult Result;
	TestFalse(TEXT("Selection should fail on out-of-range row"), FHCIAbilityKitDryRunDiffSelection::SelectRows(Report, {9}, Result));
	TestFalse(TEXT("Result success flag"), Result.bSuccess);
	TestEqual(TEXT("ErrorCode"), Result.ErrorCode, FString(TEXT("E4201")));
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("row_index_out_of_range")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorReviewSelectionJsonFieldsTest,
	"HCIAbilityKit.Editor.AgentExecutorReviewSelect.SelectedJsonKeepsLocateAndSkipReasonFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorReviewSelectionJsonFieldsTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Report = MakeF8ReviewReport();

	FHCIAbilityKitDryRunDiffSelectionResult Result;
	TestTrue(TEXT("Selection should succeed"), FHCIAbilityKitDryRunDiffSelection::SelectRows(Report, {1}, Result));

	FString JsonText;
	TestTrue(TEXT("Serialize selected report"), FHCIAbilityKitDryRunDiffJsonSerializer::SerializeToJsonString(Result.SelectedReport, JsonText));
	TestTrue(TEXT("JSON includes skip_reason"), JsonText.Contains(TEXT("\"skip_reason\"")));
	TestTrue(TEXT("JSON includes locate_strategy"), JsonText.Contains(TEXT("\"locate_strategy\"")));
	TestTrue(TEXT("JSON includes sync_browser"), JsonText.Contains(TEXT("sync_browser")));
	return true;
}

#endif

