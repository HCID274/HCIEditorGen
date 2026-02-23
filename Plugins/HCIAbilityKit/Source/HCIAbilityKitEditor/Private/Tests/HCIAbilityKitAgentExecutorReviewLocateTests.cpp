#if WITH_DEV_AUTOMATION_TESTS

#include "Agent/HCIAbilityKitDryRunDiff.h"
#include "Commands/HCIAbilityKitAgentExecutorReviewLocateUtils.h"
#include "Misc/AutomationTest.h"

namespace
{
static FHCIAbilityKitDryRunDiffReport MakeF7ReviewReport()
{
	FHCIAbilityKitDryRunDiffReport Report;
	Report.RequestId = TEXT("req_f7_review");

	FHCIAbilityKitDryRunDiffItem& ActorItem = Report.DiffItems.AddDefaulted_GetRef();
	ActorItem.AssetPath = TEXT("PersistentLevel.Actor_A");
	ActorItem.ActorPath = TEXT("/Game/Maps/UEDPIE_0_Map.Map:PersistentLevel.Actor_A");
	ActorItem.ToolName = TEXT("ScanLevelMeshRisks");
	ActorItem.ObjectType = EHCIAbilityKitDryRunObjectType::Actor;
	ActorItem.LocateStrategy = EHCIAbilityKitDryRunLocateStrategy::CameraFocus;
	ActorItem.EvidenceKey = TEXT("actor_path");

	FHCIAbilityKitDryRunDiffItem& AssetItem = Report.DiffItems.AddDefaulted_GetRef();
	AssetItem.AssetPath = TEXT("/Game/Art/T_F7.T_F7");
	AssetItem.ToolName = TEXT("SetTextureMaxSize");
	AssetItem.ObjectType = EHCIAbilityKitDryRunObjectType::Asset;
	AssetItem.LocateStrategy = EHCIAbilityKitDryRunLocateStrategy::SyncBrowser;
	AssetItem.EvidenceKey = TEXT("asset_path");

	return Report;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorReviewLocateResolveActorRowTest,
	"HCIAbilityKit.Editor.AgentExecutorReviewLocate.ResolveActorRowKeepsCameraFocusStrategy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorReviewLocateResolveActorRowTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Report = MakeF7ReviewReport();

	FHCIAbilityKitAgentExecutorReviewLocateResolvedRow Resolved;
	FString Reason;
	TestTrue(TEXT("Row 0 should resolve"), FHCIAbilityKitAgentExecutorReviewLocateUtils::TryResolveRow(Report, 0, Resolved, Reason));
	TestEqual(TEXT("Reason should be ok"), Reason, FString(TEXT("ok")));
	TestEqual(TEXT("RowIndex should be 0"), Resolved.RowIndex, 0);
	TestEqual(TEXT("ObjectType should be actor"), FHCIAbilityKitDryRunDiff::ObjectTypeToString(Resolved.ObjectType), FString(TEXT("actor")));
	TestEqual(TEXT("LocateStrategy should be camera_focus"), FHCIAbilityKitDryRunDiff::LocateStrategyToString(Resolved.LocateStrategy), FString(TEXT("camera_focus")));
	TestEqual(TEXT("ActorPath should carry through"), Resolved.ActorPath, FString(TEXT("/Game/Maps/UEDPIE_0_Map.Map:PersistentLevel.Actor_A")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorReviewLocateRejectsOutOfRangeRowTest,
	"HCIAbilityKit.Editor.AgentExecutorReviewLocate.ResolveRejectsOutOfRangeRow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorReviewLocateRejectsOutOfRangeRowTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Report = MakeF7ReviewReport();

	FHCIAbilityKitAgentExecutorReviewLocateResolvedRow Resolved;
	FString Reason;
	TestFalse(TEXT("Out-of-range row should fail"), FHCIAbilityKitAgentExecutorReviewLocateUtils::TryResolveRow(Report, 9, Resolved, Reason));
	TestEqual(TEXT("Reason should be row_index_out_of_range"), Reason, FString(TEXT("row_index_out_of_range")));
	return true;
}

#endif
