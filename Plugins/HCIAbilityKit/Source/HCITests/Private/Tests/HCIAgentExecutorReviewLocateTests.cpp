#if WITH_DEV_AUTOMATION_TESTS

#include "Agent/Executor/HCIDryRunDiff.h"
#include "Commands/HCIAgentExecutorReviewLocateUtils.h"
#include "Misc/AutomationTest.h"

namespace
{
static FHCIDryRunDiffReport MakeF7ReviewReport()
{
	FHCIDryRunDiffReport Report;
	Report.RequestId = TEXT("req_f7_review");

	FHCIDryRunDiffItem& ActorItem = Report.DiffItems.AddDefaulted_GetRef();
	ActorItem.AssetPath = TEXT("PersistentLevel.Actor_A");
	ActorItem.ActorPath = TEXT("/Game/Maps/UEDPIE_0_Map.Map:PersistentLevel.Actor_A");
	ActorItem.ToolName = TEXT("ScanLevelMeshRisks");
	ActorItem.ObjectType = EHCIDryRunObjectType::Actor;
	ActorItem.LocateStrategy = EHCIDryRunLocateStrategy::CameraFocus;
	ActorItem.EvidenceKey = TEXT("actor_path");

	FHCIDryRunDiffItem& AssetItem = Report.DiffItems.AddDefaulted_GetRef();
	AssetItem.AssetPath = TEXT("/Game/Art/T_F7.T_F7");
	AssetItem.ToolName = TEXT("SetTextureMaxSize");
	AssetItem.ObjectType = EHCIDryRunObjectType::Asset;
	AssetItem.LocateStrategy = EHCIDryRunLocateStrategy::SyncBrowser;
	AssetItem.EvidenceKey = TEXT("asset_path");

	return Report;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorReviewLocateResolveActorRowTest,
	"HCI.Editor.AgentExecutorReviewLocate.ResolveActorRowKeepsCameraFocusStrategy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorReviewLocateResolveActorRowTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Report = MakeF7ReviewReport();

	FHCIAgentExecutorReviewLocateResolvedRow Resolved;
	FString Reason;
	TestTrue(TEXT("Row 0 should resolve"), FHCIAgentExecutorReviewLocateUtils::TryResolveRow(Report, 0, Resolved, Reason));
	TestEqual(TEXT("Reason should be ok"), Reason, FString(TEXT("ok")));
	TestEqual(TEXT("RowIndex should be 0"), Resolved.RowIndex, 0);
	TestEqual(TEXT("ObjectType should be actor"), FHCIDryRunDiff::ObjectTypeToString(Resolved.ObjectType), FString(TEXT("actor")));
	TestEqual(TEXT("LocateStrategy should be camera_focus"), FHCIDryRunDiff::LocateStrategyToString(Resolved.LocateStrategy), FString(TEXT("camera_focus")));
	TestEqual(TEXT("ActorPath should carry through"), Resolved.ActorPath, FString(TEXT("/Game/Maps/UEDPIE_0_Map.Map:PersistentLevel.Actor_A")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorReviewLocateRejectsOutOfRangeRowTest,
	"HCI.Editor.AgentExecutorReviewLocate.ResolveRejectsOutOfRangeRow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorReviewLocateRejectsOutOfRangeRowTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Report = MakeF7ReviewReport();

	FHCIAgentExecutorReviewLocateResolvedRow Resolved;
	FString Reason;
	TestFalse(TEXT("Out-of-range row should fail"), FHCIAgentExecutorReviewLocateUtils::TryResolveRow(Report, 9, Resolved, Reason));
	TestEqual(TEXT("Reason should be row_index_out_of_range"), Reason, FString(TEXT("row_index_out_of_range")));
	return true;
}

#endif
