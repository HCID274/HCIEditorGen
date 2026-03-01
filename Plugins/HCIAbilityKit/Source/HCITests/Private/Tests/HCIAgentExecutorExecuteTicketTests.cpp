#if WITH_DEV_AUTOMATION_TESTS

#include "Agent/Contracts/StageF/HCIAgentApplyConfirmRequest.h"
#include "Agent/Contracts/StageF/HCIAgentApplyRequest.h"
#include "Agent/Contracts/StageF/HCIAgentExecuteTicket.h"
#include "Agent/Contracts/StageF/HCIAgentExecuteTicketJsonSerializer.h"
#include "Agent/Bridges/HCIAgentExecutorApplyConfirmBridge.h"
#include "Agent/Bridges/HCIAgentExecutorApplyRequestBridge.h"
#include "Agent/Bridges/HCIAgentExecutorExecuteTicketBridge.h"
#include "Agent/Executor/HCIDryRunDiff.h"
#include "Misc/AutomationTest.h"

namespace
{
static FHCIDryRunDiffReport MakeF11SelectedReviewReport(const bool bBlocked)
{
	FHCIDryRunDiffReport Report;
	Report.RequestId = TEXT("req_f11_review");

	FHCIDryRunDiffItem& Item = Report.DiffItems.AddDefaulted_GetRef();
	Item.AssetPath = TEXT("/Game/Levels/Demo/SM_Box.SM_Box");
	Item.Field = TEXT("step:s1");
	Item.ToolName = TEXT("ScanLevelMeshRisks");
	Item.Risk = EHCIDryRunRisk::ReadOnly;
	Item.ObjectType = EHCIDryRunObjectType::Actor;
	Item.LocateStrategy = EHCIDryRunLocateStrategy::CameraFocus;
	Item.EvidenceKey = TEXT("actor_path");
	Item.ActorPath = TEXT("/Game/Maps/Demo.PersistentLevel.SM_Box_01");
	if (bBlocked)
	{
		Item.SkipReason = TEXT("error_code=E4005;failure_phase=preflight;preflight_gate=confirm_gate;reason=user_not_confirmed");
	}

	FHCIDryRunDiff::NormalizeAndFinalize(Report);
	return Report;
}

static FHCIAgentApplyRequest MakeF11ApplyRequest(const FHCIDryRunDiffReport& Review)
{
	FHCIAgentApplyRequest ApplyRequest;
	check(FHCIAgentExecutorApplyRequestBridge::BuildApplyRequest(Review, ApplyRequest));
	return ApplyRequest;
}

static FHCIAgentApplyConfirmRequest MakeF11ConfirmRequest(
	const FHCIAgentApplyRequest& ApplyRequest,
	const FHCIDryRunDiffReport& Review,
	const bool bUserConfirmed)
{
	FHCIAgentApplyConfirmRequest ConfirmRequest;
	check(FHCIAgentExecutorApplyConfirmBridge::BuildConfirmRequest(ApplyRequest, Review, bUserConfirmed, ConfirmRequest));
	return ConfirmRequest;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorExecuteTicketReadyTest,
	"HCI.Editor.AgentExecutorExecuteTicket.ReadyWhenConfirmRequestReadyAndMatchingState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorExecuteTicketReadyTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeF11SelectedReviewReport(false);
	const FHCIAgentApplyRequest ApplyRequest = MakeF11ApplyRequest(Review);
	const FHCIAgentApplyConfirmRequest ConfirmRequest = MakeF11ConfirmRequest(ApplyRequest, Review, true);

	FHCIAgentExecuteTicket ExecuteTicket;
	TestTrue(TEXT("Build execute ticket"), FHCIAgentExecutorExecuteTicketBridge::BuildExecuteTicket(ConfirmRequest, ApplyRequest, Review, ExecuteTicket));
	TestTrue(TEXT("Should be ready"), ExecuteTicket.bReadyToSimulateExecute);
	TestEqual(TEXT("ErrorCode should be -"), ExecuteTicket.ErrorCode, FString(TEXT("-")));
	TestEqual(TEXT("Reason should be execute_ticket_ready"), ExecuteTicket.Reason, FString(TEXT("execute_ticket_ready")));
	TestEqual(TEXT("ConfirmRequestId should be carried"), ExecuteTicket.ConfirmRequestId, ConfirmRequest.RequestId);
	TestEqual(TEXT("ApplyRequestId should match"), ExecuteTicket.ApplyRequestId, ApplyRequest.RequestId);
	TestEqual(TEXT("SelectionDigest should match"), ExecuteTicket.SelectionDigest, ApplyRequest.SelectionDigest);
	TestEqual(TEXT("TransactionMode"), ExecuteTicket.TransactionMode, FString(TEXT("all_or_nothing")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorExecuteTicketConfirmNotReadyTest,
	"HCI.Editor.AgentExecutorExecuteTicket.ConfirmNotReadyReturnsE4204",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorExecuteTicketConfirmNotReadyTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeF11SelectedReviewReport(true);
	const FHCIAgentApplyRequest ApplyRequest = MakeF11ApplyRequest(Review);
	const FHCIAgentApplyConfirmRequest ConfirmRequest = MakeF11ConfirmRequest(ApplyRequest, Review, true);

	FHCIAgentExecuteTicket ExecuteTicket;
	TestTrue(TEXT("Build execute ticket"), FHCIAgentExecutorExecuteTicketBridge::BuildExecuteTicket(ConfirmRequest, ApplyRequest, Review, ExecuteTicket));
	TestFalse(TEXT("Should not be ready"), ExecuteTicket.bReadyToSimulateExecute);
	TestEqual(TEXT("ErrorCode"), ExecuteTicket.ErrorCode, FString(TEXT("E4204")));
	TestEqual(TEXT("Reason"), ExecuteTicket.Reason, FString(TEXT("confirm_request_not_ready")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorExecuteTicketDigestMismatchTest,
	"HCI.Editor.AgentExecutorExecuteTicket.DigestMismatchReturnsE4202",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorExecuteTicketDigestMismatchTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeF11SelectedReviewReport(false);
	const FHCIAgentApplyRequest ApplyRequest = MakeF11ApplyRequest(Review);
	FHCIAgentApplyConfirmRequest ConfirmRequest = MakeF11ConfirmRequest(ApplyRequest, Review, true);
	ConfirmRequest.SelectionDigest = TEXT("crc32_BAD0C0DE");

	FHCIAgentExecuteTicket ExecuteTicket;
	TestTrue(TEXT("Build execute ticket"), FHCIAgentExecutorExecuteTicketBridge::BuildExecuteTicket(ConfirmRequest, ApplyRequest, Review, ExecuteTicket));
	TestFalse(TEXT("Should not be ready"), ExecuteTicket.bReadyToSimulateExecute);
	TestEqual(TEXT("ErrorCode"), ExecuteTicket.ErrorCode, FString(TEXT("E4202")));
	TestEqual(TEXT("Reason"), ExecuteTicket.Reason, FString(TEXT("selection_digest_mismatch")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorExecuteTicketJsonFieldsTest,
	"HCI.Editor.AgentExecutorExecuteTicket.JsonIncludesReadyAndPolicyFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorExecuteTicketJsonFieldsTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeF11SelectedReviewReport(false);
	const FHCIAgentApplyRequest ApplyRequest = MakeF11ApplyRequest(Review);
	const FHCIAgentApplyConfirmRequest ConfirmRequest = MakeF11ConfirmRequest(ApplyRequest, Review, true);

	FHCIAgentExecuteTicket ExecuteTicket;
	TestTrue(TEXT("Build execute ticket"), FHCIAgentExecutorExecuteTicketBridge::BuildExecuteTicket(ConfirmRequest, ApplyRequest, Review, ExecuteTicket));

	FString JsonText;
	TestTrue(TEXT("Serialize execute ticket json"), FHCIAgentExecuteTicketJsonSerializer::SerializeToJsonString(ExecuteTicket, JsonText));
	TestTrue(TEXT("JSON includes confirm_request_id"), JsonText.Contains(TEXT("\"confirm_request_id\"")));
	TestTrue(TEXT("JSON includes ready_to_simulate_execute"), JsonText.Contains(TEXT("\"ready_to_simulate_execute\"")));
	TestTrue(TEXT("JSON includes transaction_mode"), JsonText.Contains(TEXT("\"transaction_mode\"")));
	TestTrue(TEXT("JSON includes termination_policy"), JsonText.Contains(TEXT("\"termination_policy\"")));
	TestTrue(TEXT("JSON includes locate_strategy"), JsonText.Contains(TEXT("\"locate_strategy\"")));
	return true;
}

#endif

