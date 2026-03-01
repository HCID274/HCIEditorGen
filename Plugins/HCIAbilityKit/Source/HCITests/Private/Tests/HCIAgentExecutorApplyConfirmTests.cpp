#if WITH_DEV_AUTOMATION_TESTS

#include "Agent/Contracts/StageF/HCIAgentApplyConfirmRequest.h"
#include "Agent/Contracts/StageF/HCIAgentApplyConfirmRequestJsonSerializer.h"
#include "Agent/Contracts/StageF/HCIAgentApplyRequest.h"
#include "Agent/Bridges/HCIAgentExecutorApplyConfirmBridge.h"
#include "Agent/Bridges/HCIAgentExecutorApplyRequestBridge.h"
#include "Agent/Executor/HCIDryRunDiff.h"
#include "Misc/AutomationTest.h"

namespace
{
static FHCIDryRunDiffReport MakeF10SelectedReviewReport(const bool bBlocked)
{
	FHCIDryRunDiffReport Report;
	Report.RequestId = TEXT("req_f10_review");

	FHCIDryRunDiffItem& First = Report.DiffItems.AddDefaulted_GetRef();
	First.AssetPath = TEXT("/Game/Art/T_F10_A.T_F10_A");
	First.Field = TEXT("step:s1");
	First.ToolName = TEXT("SetTextureMaxSize");
	First.Risk = EHCIDryRunRisk::Write;
	First.ObjectType = EHCIDryRunObjectType::Asset;
	First.LocateStrategy = EHCIDryRunLocateStrategy::SyncBrowser;
	First.EvidenceKey = TEXT("asset_path");
	if (bBlocked)
	{
		First.SkipReason = TEXT("error_code=E4005;failure_phase=preflight;preflight_gate=confirm_gate;reason=user_not_confirmed");
	}

	FHCIDryRunDiff::NormalizeAndFinalize(Report);
	return Report;
}

static FHCIAgentApplyRequest MakeF10ApplyRequestFromReview(const FHCIDryRunDiffReport& Report)
{
	FHCIAgentApplyRequest ApplyRequest;
	const bool bOk = FHCIAgentExecutorApplyRequestBridge::BuildApplyRequest(Report, ApplyRequest);
	check(bOk);
	return ApplyRequest;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorApplyConfirmReadyTest,
	"HCI.Editor.AgentExecutorApplyConfirm.ReadyWhenConfirmedAndMatchingState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorApplyConfirmReadyTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeF10SelectedReviewReport(false);
	const FHCIAgentApplyRequest ApplyRequest = MakeF10ApplyRequestFromReview(Review);

	FHCIAgentApplyConfirmRequest ConfirmRequest;
	TestTrue(TEXT("Build confirm request"), FHCIAgentExecutorApplyConfirmBridge::BuildConfirmRequest(ApplyRequest, Review, true, ConfirmRequest));
	TestTrue(TEXT("Should be ready"), ConfirmRequest.bReadyToExecute);
	TestEqual(TEXT("ErrorCode should be -"), ConfirmRequest.ErrorCode, FString(TEXT("-")));
	TestEqual(TEXT("Reason should be confirm_request_ready"), ConfirmRequest.Reason, FString(TEXT("confirm_request_ready")));
	TestEqual(TEXT("ApplyRequestId should be carried"), ConfirmRequest.ApplyRequestId, ApplyRequest.RequestId);
	TestEqual(TEXT("SelectionDigest should match"), ConfirmRequest.SelectionDigest, ApplyRequest.SelectionDigest);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorApplyConfirmUnconfirmedBlockedTest,
	"HCI.Editor.AgentExecutorApplyConfirm.UnconfirmedReturnsE4005",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorApplyConfirmUnconfirmedBlockedTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeF10SelectedReviewReport(false);
	const FHCIAgentApplyRequest ApplyRequest = MakeF10ApplyRequestFromReview(Review);

	FHCIAgentApplyConfirmRequest ConfirmRequest;
	TestTrue(TEXT("Build confirm request"), FHCIAgentExecutorApplyConfirmBridge::BuildConfirmRequest(ApplyRequest, Review, false, ConfirmRequest));
	TestFalse(TEXT("Should not be ready"), ConfirmRequest.bReadyToExecute);
	TestEqual(TEXT("ErrorCode"), ConfirmRequest.ErrorCode, FString(TEXT("E4005")));
	TestEqual(TEXT("Reason"), ConfirmRequest.Reason, FString(TEXT("user_not_confirmed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorApplyConfirmNotReadyApplyBlockedTest,
	"HCI.Editor.AgentExecutorApplyConfirm.NotReadyApplyReturnsE4203",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorApplyConfirmNotReadyApplyBlockedTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeF10SelectedReviewReport(true);
	const FHCIAgentApplyRequest ApplyRequest = MakeF10ApplyRequestFromReview(Review);

	FHCIAgentApplyConfirmRequest ConfirmRequest;
	TestTrue(TEXT("Build confirm request"), FHCIAgentExecutorApplyConfirmBridge::BuildConfirmRequest(ApplyRequest, Review, true, ConfirmRequest));
	TestFalse(TEXT("Should not be ready"), ConfirmRequest.bReadyToExecute);
	TestEqual(TEXT("ErrorCode"), ConfirmRequest.ErrorCode, FString(TEXT("E4203")));
	TestEqual(TEXT("Reason"), ConfirmRequest.Reason, FString(TEXT("apply_request_not_ready_blocked_rows_present")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorApplyConfirmDigestMismatchTest,
	"HCI.Editor.AgentExecutorApplyConfirm.DigestMismatchReturnsE4202",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorApplyConfirmDigestMismatchTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeF10SelectedReviewReport(false);
	FHCIAgentApplyRequest ApplyRequest = MakeF10ApplyRequestFromReview(Review);
	ApplyRequest.SelectionDigest = TEXT("crc32_DEADBEEF");

	FHCIAgentApplyConfirmRequest ConfirmRequest;
	TestTrue(TEXT("Build confirm request"), FHCIAgentExecutorApplyConfirmBridge::BuildConfirmRequest(ApplyRequest, Review, true, ConfirmRequest));
	TestFalse(TEXT("Should not be ready"), ConfirmRequest.bReadyToExecute);
	TestEqual(TEXT("ErrorCode"), ConfirmRequest.ErrorCode, FString(TEXT("E4202")));
	TestEqual(TEXT("Reason"), ConfirmRequest.Reason, FString(TEXT("selection_digest_mismatch")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorApplyConfirmJsonFieldsTest,
	"HCI.Editor.AgentExecutorApplyConfirm.JsonIncludesReadyAndValidationFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorApplyConfirmJsonFieldsTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeF10SelectedReviewReport(false);
	const FHCIAgentApplyRequest ApplyRequest = MakeF10ApplyRequestFromReview(Review);

	FHCIAgentApplyConfirmRequest ConfirmRequest;
	TestTrue(TEXT("Build confirm request"), FHCIAgentExecutorApplyConfirmBridge::BuildConfirmRequest(ApplyRequest, Review, true, ConfirmRequest));

	FString JsonText;
	TestTrue(TEXT("Serialize confirm request json"), FHCIAgentApplyConfirmRequestJsonSerializer::SerializeToJsonString(ConfirmRequest, JsonText));
	TestTrue(TEXT("JSON includes apply_request_id"), JsonText.Contains(TEXT("\"apply_request_id\"")));
	TestTrue(TEXT("JSON includes ready_to_execute"), JsonText.Contains(TEXT("\"ready_to_execute\"")));
	TestTrue(TEXT("JSON includes user_confirmed"), JsonText.Contains(TEXT("\"user_confirmed\"")));
	TestTrue(TEXT("JSON includes error_code"), JsonText.Contains(TEXT("\"error_code\"")));
	TestTrue(TEXT("JSON includes locate_strategy"), JsonText.Contains(TEXT("\"locate_strategy\"")));
	return true;
}

#endif

