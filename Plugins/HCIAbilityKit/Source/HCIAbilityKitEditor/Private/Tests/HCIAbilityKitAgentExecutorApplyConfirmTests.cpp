#if WITH_DEV_AUTOMATION_TESTS

#include "Agent/HCIAbilityKitAgentApplyConfirmRequest.h"
#include "Agent/HCIAbilityKitAgentApplyConfirmRequestJsonSerializer.h"
#include "Agent/HCIAbilityKitAgentApplyRequest.h"
#include "Agent/HCIAbilityKitAgentExecutorApplyConfirmBridge.h"
#include "Agent/HCIAbilityKitAgentExecutorApplyRequestBridge.h"
#include "Agent/HCIAbilityKitDryRunDiff.h"
#include "Misc/AutomationTest.h"

namespace
{
static FHCIAbilityKitDryRunDiffReport MakeF10SelectedReviewReport(const bool bBlocked)
{
	FHCIAbilityKitDryRunDiffReport Report;
	Report.RequestId = TEXT("req_f10_review");

	FHCIAbilityKitDryRunDiffItem& First = Report.DiffItems.AddDefaulted_GetRef();
	First.AssetPath = TEXT("/Game/Art/T_F10_A.T_F10_A");
	First.Field = TEXT("step:s1");
	First.ToolName = TEXT("SetTextureMaxSize");
	First.Risk = EHCIAbilityKitDryRunRisk::Write;
	First.ObjectType = EHCIAbilityKitDryRunObjectType::Asset;
	First.LocateStrategy = EHCIAbilityKitDryRunLocateStrategy::SyncBrowser;
	First.EvidenceKey = TEXT("asset_path");
	if (bBlocked)
	{
		First.SkipReason = TEXT("error_code=E4005;failure_phase=preflight;preflight_gate=confirm_gate;reason=user_not_confirmed");
	}

	FHCIAbilityKitDryRunDiff::NormalizeAndFinalize(Report);
	return Report;
}

static FHCIAbilityKitAgentApplyRequest MakeF10ApplyRequestFromReview(const FHCIAbilityKitDryRunDiffReport& Report)
{
	FHCIAbilityKitAgentApplyRequest ApplyRequest;
	const bool bOk = FHCIAbilityKitAgentExecutorApplyRequestBridge::BuildApplyRequest(Report, ApplyRequest);
	check(bOk);
	return ApplyRequest;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorApplyConfirmReadyTest,
	"HCIAbilityKit.Editor.AgentExecutorApplyConfirm.ReadyWhenConfirmedAndMatchingState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorApplyConfirmReadyTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeF10SelectedReviewReport(false);
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeF10ApplyRequestFromReview(Review);

	FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest;
	TestTrue(TEXT("Build confirm request"), FHCIAbilityKitAgentExecutorApplyConfirmBridge::BuildConfirmRequest(ApplyRequest, Review, true, ConfirmRequest));
	TestTrue(TEXT("Should be ready"), ConfirmRequest.bReadyToExecute);
	TestEqual(TEXT("ErrorCode should be -"), ConfirmRequest.ErrorCode, FString(TEXT("-")));
	TestEqual(TEXT("Reason should be confirm_request_ready"), ConfirmRequest.Reason, FString(TEXT("confirm_request_ready")));
	TestEqual(TEXT("ApplyRequestId should be carried"), ConfirmRequest.ApplyRequestId, ApplyRequest.RequestId);
	TestEqual(TEXT("SelectionDigest should match"), ConfirmRequest.SelectionDigest, ApplyRequest.SelectionDigest);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorApplyConfirmUnconfirmedBlockedTest,
	"HCIAbilityKit.Editor.AgentExecutorApplyConfirm.UnconfirmedReturnsE4005",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorApplyConfirmUnconfirmedBlockedTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeF10SelectedReviewReport(false);
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeF10ApplyRequestFromReview(Review);

	FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest;
	TestTrue(TEXT("Build confirm request"), FHCIAbilityKitAgentExecutorApplyConfirmBridge::BuildConfirmRequest(ApplyRequest, Review, false, ConfirmRequest));
	TestFalse(TEXT("Should not be ready"), ConfirmRequest.bReadyToExecute);
	TestEqual(TEXT("ErrorCode"), ConfirmRequest.ErrorCode, FString(TEXT("E4005")));
	TestEqual(TEXT("Reason"), ConfirmRequest.Reason, FString(TEXT("user_not_confirmed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorApplyConfirmNotReadyApplyBlockedTest,
	"HCIAbilityKit.Editor.AgentExecutorApplyConfirm.NotReadyApplyReturnsE4203",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorApplyConfirmNotReadyApplyBlockedTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeF10SelectedReviewReport(true);
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeF10ApplyRequestFromReview(Review);

	FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest;
	TestTrue(TEXT("Build confirm request"), FHCIAbilityKitAgentExecutorApplyConfirmBridge::BuildConfirmRequest(ApplyRequest, Review, true, ConfirmRequest));
	TestFalse(TEXT("Should not be ready"), ConfirmRequest.bReadyToExecute);
	TestEqual(TEXT("ErrorCode"), ConfirmRequest.ErrorCode, FString(TEXT("E4203")));
	TestEqual(TEXT("Reason"), ConfirmRequest.Reason, FString(TEXT("apply_request_not_ready_blocked_rows_present")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorApplyConfirmDigestMismatchTest,
	"HCIAbilityKit.Editor.AgentExecutorApplyConfirm.DigestMismatchReturnsE4202",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorApplyConfirmDigestMismatchTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeF10SelectedReviewReport(false);
	FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeF10ApplyRequestFromReview(Review);
	ApplyRequest.SelectionDigest = TEXT("crc32_DEADBEEF");

	FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest;
	TestTrue(TEXT("Build confirm request"), FHCIAbilityKitAgentExecutorApplyConfirmBridge::BuildConfirmRequest(ApplyRequest, Review, true, ConfirmRequest));
	TestFalse(TEXT("Should not be ready"), ConfirmRequest.bReadyToExecute);
	TestEqual(TEXT("ErrorCode"), ConfirmRequest.ErrorCode, FString(TEXT("E4202")));
	TestEqual(TEXT("Reason"), ConfirmRequest.Reason, FString(TEXT("selection_digest_mismatch")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorApplyConfirmJsonFieldsTest,
	"HCIAbilityKit.Editor.AgentExecutorApplyConfirm.JsonIncludesReadyAndValidationFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorApplyConfirmJsonFieldsTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeF10SelectedReviewReport(false);
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeF10ApplyRequestFromReview(Review);

	FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest;
	TestTrue(TEXT("Build confirm request"), FHCIAbilityKitAgentExecutorApplyConfirmBridge::BuildConfirmRequest(ApplyRequest, Review, true, ConfirmRequest));

	FString JsonText;
	TestTrue(TEXT("Serialize confirm request json"), FHCIAbilityKitAgentApplyConfirmRequestJsonSerializer::SerializeToJsonString(ConfirmRequest, JsonText));
	TestTrue(TEXT("JSON includes apply_request_id"), JsonText.Contains(TEXT("\"apply_request_id\"")));
	TestTrue(TEXT("JSON includes ready_to_execute"), JsonText.Contains(TEXT("\"ready_to_execute\"")));
	TestTrue(TEXT("JSON includes user_confirmed"), JsonText.Contains(TEXT("\"user_confirmed\"")));
	TestTrue(TEXT("JSON includes error_code"), JsonText.Contains(TEXT("\"error_code\"")));
	TestTrue(TEXT("JSON includes locate_strategy"), JsonText.Contains(TEXT("\"locate_strategy\"")));
	return true;
}

#endif

