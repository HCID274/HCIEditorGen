#if WITH_DEV_AUTOMATION_TESTS

#include "Agent/Contracts/StageF/HCIAgentApplyRequest.h"
#include "Agent/Contracts/StageF/HCIAgentApplyRequestJsonSerializer.h"
#include "Agent/Bridges/HCIAgentExecutorApplyRequestBridge.h"
#include "Agent/Executor/HCIDryRunDiff.h"
#include "Misc/AutomationTest.h"

namespace
{
static FHCIDryRunDiffReport MakeF9SelectedReviewReport(const bool bIncludeBlockedRow)
{
	FHCIDryRunDiffReport Report;
	Report.RequestId = TEXT("req_f9_review");

	FHCIDryRunDiffItem& First = Report.DiffItems.AddDefaulted_GetRef();
	First.AssetPath = TEXT("/Game/Art/T_F9_A.T_F9_A");
	First.Field = TEXT("step:s1");
	First.ToolName = TEXT("SetTextureMaxSize");
	First.Risk = EHCIDryRunRisk::Write;
	First.ObjectType = EHCIDryRunObjectType::Asset;
	First.LocateStrategy = EHCIDryRunLocateStrategy::SyncBrowser;
	First.EvidenceKey = TEXT("asset_path");
	if (bIncludeBlockedRow)
	{
		First.SkipReason = TEXT("error_code=E4005;failure_phase=preflight;preflight_gate=confirm_gate;reason=user_not_confirmed");
	}

	FHCIDryRunDiffItem& Second = Report.DiffItems.AddDefaulted_GetRef();
	Second.AssetPath = TEXT("PersistentLevel.Actor_F9");
	Second.ActorPath = TEXT("/Game/Maps/UEDPIE_0_Map.Map:PersistentLevel.Actor_F9");
	Second.Field = TEXT("step:s2");
	Second.ToolName = TEXT("ScanLevelMeshRisks");
	Second.Risk = EHCIDryRunRisk::ReadOnly;
	Second.ObjectType = EHCIDryRunObjectType::Actor;
	Second.LocateStrategy = EHCIDryRunLocateStrategy::CameraFocus;
	Second.EvidenceKey = TEXT("actor_path");

	FHCIDryRunDiff::NormalizeAndFinalize(Report);
	return Report;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorApplyReadyOnUnblockedSelectionTest,
	"HCI.Editor.AgentExecutorApply.ReadyWhenNoBlockedRows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorApplyReadyOnUnblockedSelectionTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport ReviewReport = MakeF9SelectedReviewReport(false);

	FHCIAgentApplyRequest ApplyRequest;
	TestTrue(TEXT("Bridge should succeed"), FHCIAgentExecutorApplyRequestBridge::BuildApplyRequest(ReviewReport, ApplyRequest));
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
	FHCIAgentExecutorApplyBlockedRowsTest,
	"HCI.Editor.AgentExecutorApply.BlockedRowsMakeApplyRequestNotReady",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorApplyBlockedRowsTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport ReviewReport = MakeF9SelectedReviewReport(true);

	FHCIAgentApplyRequest ApplyRequest;
	TestTrue(TEXT("Bridge should succeed"), FHCIAgentExecutorApplyRequestBridge::BuildApplyRequest(ReviewReport, ApplyRequest));
	TestFalse(TEXT("ApplyRequest should not be ready"), ApplyRequest.bReady);
	TestEqual(TEXT("BlockedRows should be 1"), ApplyRequest.Summary.BlockedRows, 1);
	TestEqual(TEXT("ModifiableRows should be 1"), ApplyRequest.Summary.ModifiableRows, 1);
	TestEqual(TEXT("First row should be blocked"), ApplyRequest.Items[0].bBlocked, true);
	TestTrue(TEXT("SkipReason should preserve E4005"), ApplyRequest.Items[0].SkipReason.Contains(TEXT("E4005")));
	TestEqual(TEXT("Second row should keep actor locate"), FHCIDryRunDiff::LocateStrategyToString(ApplyRequest.Items[1].LocateStrategy), FString(TEXT("camera_focus")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorApplyJsonFieldsTest,
	"HCI.Editor.AgentExecutorApply.JsonIncludesDigestReadyAndBlockedFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorApplyJsonFieldsTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport ReviewReport = MakeF9SelectedReviewReport(true);

	FHCIAgentApplyRequest ApplyRequest;
	TestTrue(TEXT("Bridge should succeed"), FHCIAgentExecutorApplyRequestBridge::BuildApplyRequest(ReviewReport, ApplyRequest));

	FString JsonText;
	TestTrue(TEXT("Serialize ApplyRequest JSON"), FHCIAgentApplyRequestJsonSerializer::SerializeToJsonString(ApplyRequest, JsonText));
	TestTrue(TEXT("JSON includes selection_digest"), JsonText.Contains(TEXT("\"selection_digest\"")));
	TestTrue(TEXT("JSON includes ready"), JsonText.Contains(TEXT("\"ready\"")));
	TestTrue(TEXT("JSON includes blocked"), JsonText.Contains(TEXT("\"blocked\"")));
	TestTrue(TEXT("JSON includes skip_reason"), JsonText.Contains(TEXT("\"skip_reason\"")));
	TestTrue(TEXT("JSON includes locate_strategy"), JsonText.Contains(TEXT("\"locate_strategy\"")));
	return true;
}

#endif

