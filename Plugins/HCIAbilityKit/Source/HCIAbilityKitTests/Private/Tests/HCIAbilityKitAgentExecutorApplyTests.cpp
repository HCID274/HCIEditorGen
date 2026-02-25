#if WITH_DEV_AUTOMATION_TESTS

#include "Agent/Contracts/StageF/HCIAbilityKitAgentApplyRequest.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentApplyRequestJsonSerializer.h"
#include "Agent/Bridges/HCIAbilityKitAgentExecutorApplyRequestBridge.h"
#include "Agent/Executor/HCIAbilityKitDryRunDiff.h"
#include "Misc/AutomationTest.h"

namespace
{
static FHCIAbilityKitDryRunDiffReport MakeF9SelectedReviewReport(const bool bIncludeBlockedRow)
{
	FHCIAbilityKitDryRunDiffReport Report;
	Report.RequestId = TEXT("req_f9_review");

	FHCIAbilityKitDryRunDiffItem& First = Report.DiffItems.AddDefaulted_GetRef();
	First.AssetPath = TEXT("/Game/Art/T_F9_A.T_F9_A");
	First.Field = TEXT("step:s1");
	First.ToolName = TEXT("SetTextureMaxSize");
	First.Risk = EHCIAbilityKitDryRunRisk::Write;
	First.ObjectType = EHCIAbilityKitDryRunObjectType::Asset;
	First.LocateStrategy = EHCIAbilityKitDryRunLocateStrategy::SyncBrowser;
	First.EvidenceKey = TEXT("asset_path");
	if (bIncludeBlockedRow)
	{
		First.SkipReason = TEXT("error_code=E4005;failure_phase=preflight;preflight_gate=confirm_gate;reason=user_not_confirmed");
	}

	FHCIAbilityKitDryRunDiffItem& Second = Report.DiffItems.AddDefaulted_GetRef();
	Second.AssetPath = TEXT("PersistentLevel.Actor_F9");
	Second.ActorPath = TEXT("/Game/Maps/UEDPIE_0_Map.Map:PersistentLevel.Actor_F9");
	Second.Field = TEXT("step:s2");
	Second.ToolName = TEXT("ScanLevelMeshRisks");
	Second.Risk = EHCIAbilityKitDryRunRisk::ReadOnly;
	Second.ObjectType = EHCIAbilityKitDryRunObjectType::Actor;
	Second.LocateStrategy = EHCIAbilityKitDryRunLocateStrategy::CameraFocus;
	Second.EvidenceKey = TEXT("actor_path");

	FHCIAbilityKitDryRunDiff::NormalizeAndFinalize(Report);
	return Report;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorApplyReadyOnUnblockedSelectionTest,
	"HCIAbilityKit.Editor.AgentExecutorApply.ReadyWhenNoBlockedRows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorApplyReadyOnUnblockedSelectionTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport ReviewReport = MakeF9SelectedReviewReport(false);

	FHCIAbilityKitAgentApplyRequest ApplyRequest;
	TestTrue(TEXT("Bridge should succeed"), FHCIAbilityKitAgentExecutorApplyRequestBridge::BuildApplyRequest(ReviewReport, ApplyRequest));
	TestEqual(TEXT("ReviewRequestId should carry through"), ApplyRequest.ReviewRequestId, FString(TEXT("req_f9_review")));
	TestTrue(TEXT("ApplyRequest should be ready"), ApplyRequest.bReady);
	TestEqual(TEXT("BlockedRows should be 0"), ApplyRequest.Summary.BlockedRows, 0);
	TestEqual(TEXT("ModifiableRows should be 2"), ApplyRequest.Summary.ModifiableRows, 2);
	TestEqual(TEXT("WriteRows should be 1"), ApplyRequest.Summary.WriteRows, 1);
	TestEqual(TEXT("ReadOnlyRows should be 1"), ApplyRequest.Summary.ReadOnlyRows, 1);
	TestTrue(TEXT("Selection digest should not be empty"), !ApplyRequest.SelectionDigest.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorApplyBlockedRowsTest,
	"HCIAbilityKit.Editor.AgentExecutorApply.BlockedRowsMakeApplyRequestNotReady",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorApplyBlockedRowsTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport ReviewReport = MakeF9SelectedReviewReport(true);

	FHCIAbilityKitAgentApplyRequest ApplyRequest;
	TestTrue(TEXT("Bridge should succeed"), FHCIAbilityKitAgentExecutorApplyRequestBridge::BuildApplyRequest(ReviewReport, ApplyRequest));
	TestFalse(TEXT("ApplyRequest should not be ready"), ApplyRequest.bReady);
	TestEqual(TEXT("BlockedRows should be 1"), ApplyRequest.Summary.BlockedRows, 1);
	TestEqual(TEXT("ModifiableRows should be 1"), ApplyRequest.Summary.ModifiableRows, 1);
	TestEqual(TEXT("First row should be blocked"), ApplyRequest.Items[0].bBlocked, true);
	TestTrue(TEXT("SkipReason should preserve E4005"), ApplyRequest.Items[0].SkipReason.Contains(TEXT("E4005")));
	TestEqual(TEXT("Second row should keep actor locate"), FHCIAbilityKitDryRunDiff::LocateStrategyToString(ApplyRequest.Items[1].LocateStrategy), FString(TEXT("camera_focus")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorApplyJsonFieldsTest,
	"HCIAbilityKit.Editor.AgentExecutorApply.JsonIncludesDigestReadyAndBlockedFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorApplyJsonFieldsTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport ReviewReport = MakeF9SelectedReviewReport(true);

	FHCIAbilityKitAgentApplyRequest ApplyRequest;
	TestTrue(TEXT("Bridge should succeed"), FHCIAbilityKitAgentExecutorApplyRequestBridge::BuildApplyRequest(ReviewReport, ApplyRequest));

	FString JsonText;
	TestTrue(TEXT("Serialize ApplyRequest JSON"), FHCIAbilityKitAgentApplyRequestJsonSerializer::SerializeToJsonString(ApplyRequest, JsonText));
	TestTrue(TEXT("JSON includes selection_digest"), JsonText.Contains(TEXT("\"selection_digest\"")));
	TestTrue(TEXT("JSON includes ready"), JsonText.Contains(TEXT("\"ready\"")));
	TestTrue(TEXT("JSON includes blocked"), JsonText.Contains(TEXT("\"blocked\"")));
	TestTrue(TEXT("JSON includes skip_reason"), JsonText.Contains(TEXT("\"skip_reason\"")));
	TestTrue(TEXT("JSON includes locate_strategy"), JsonText.Contains(TEXT("\"locate_strategy\"")));
	return true;
}

#endif
