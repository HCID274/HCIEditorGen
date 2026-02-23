#if WITH_DEV_AUTOMATION_TESTS

#include "Agent/HCIAbilityKitAgentApplyConfirmRequest.h"
#include "Agent/HCIAbilityKitAgentApplyRequest.h"
#include "Agent/HCIAbilityKitAgentExecuteTicket.h"
#include "Agent/HCIAbilityKitAgentExecuteTicketJsonSerializer.h"
#include "Agent/HCIAbilityKitAgentExecutorApplyConfirmBridge.h"
#include "Agent/HCIAbilityKitAgentExecutorApplyRequestBridge.h"
#include "Agent/HCIAbilityKitAgentExecutorExecuteTicketBridge.h"
#include "Agent/HCIAbilityKitDryRunDiff.h"
#include "Misc/AutomationTest.h"

namespace
{
static FHCIAbilityKitDryRunDiffReport MakeF11SelectedReviewReport(const bool bBlocked)
{
	FHCIAbilityKitDryRunDiffReport Report;
	Report.RequestId = TEXT("req_f11_review");

	FHCIAbilityKitDryRunDiffItem& Item = Report.DiffItems.AddDefaulted_GetRef();
	Item.AssetPath = TEXT("/Game/Levels/Demo/SM_Box.SM_Box");
	Item.Field = TEXT("step:s1");
	Item.ToolName = TEXT("ScanLevelMeshRisks");
	Item.Risk = EHCIAbilityKitDryRunRisk::ReadOnly;
	Item.ObjectType = EHCIAbilityKitDryRunObjectType::Actor;
	Item.LocateStrategy = EHCIAbilityKitDryRunLocateStrategy::CameraFocus;
	Item.EvidenceKey = TEXT("actor_path");
	Item.ActorPath = TEXT("/Game/Maps/Demo.PersistentLevel.SM_Box_01");
	if (bBlocked)
	{
		Item.SkipReason = TEXT("error_code=E4005;failure_phase=preflight;preflight_gate=confirm_gate;reason=user_not_confirmed");
	}

	FHCIAbilityKitDryRunDiff::NormalizeAndFinalize(Report);
	return Report;
}

static FHCIAbilityKitAgentApplyRequest MakeF11ApplyRequest(const FHCIAbilityKitDryRunDiffReport& Review)
{
	FHCIAbilityKitAgentApplyRequest ApplyRequest;
	check(FHCIAbilityKitAgentExecutorApplyRequestBridge::BuildApplyRequest(Review, ApplyRequest));
	return ApplyRequest;
}

static FHCIAbilityKitAgentApplyConfirmRequest MakeF11ConfirmRequest(
	const FHCIAbilityKitAgentApplyRequest& ApplyRequest,
	const FHCIAbilityKitDryRunDiffReport& Review,
	const bool bUserConfirmed)
{
	FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest;
	check(FHCIAbilityKitAgentExecutorApplyConfirmBridge::BuildConfirmRequest(ApplyRequest, Review, bUserConfirmed, ConfirmRequest));
	return ConfirmRequest;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorExecuteTicketReadyTest,
	"HCIAbilityKit.Editor.AgentExecutorExecuteTicket.ReadyWhenConfirmRequestReadyAndMatchingState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorExecuteTicketReadyTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeF11SelectedReviewReport(false);
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeF11ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeF11ConfirmRequest(ApplyRequest, Review, true);

	FHCIAbilityKitAgentExecuteTicket ExecuteTicket;
	TestTrue(TEXT("Build execute ticket"), FHCIAbilityKitAgentExecutorExecuteTicketBridge::BuildExecuteTicket(ConfirmRequest, ApplyRequest, Review, ExecuteTicket));
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
	FHCIAbilityKitAgentExecutorExecuteTicketConfirmNotReadyTest,
	"HCIAbilityKit.Editor.AgentExecutorExecuteTicket.ConfirmNotReadyReturnsE4204",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorExecuteTicketConfirmNotReadyTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeF11SelectedReviewReport(true);
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeF11ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeF11ConfirmRequest(ApplyRequest, Review, true);

	FHCIAbilityKitAgentExecuteTicket ExecuteTicket;
	TestTrue(TEXT("Build execute ticket"), FHCIAbilityKitAgentExecutorExecuteTicketBridge::BuildExecuteTicket(ConfirmRequest, ApplyRequest, Review, ExecuteTicket));
	TestFalse(TEXT("Should not be ready"), ExecuteTicket.bReadyToSimulateExecute);
	TestEqual(TEXT("ErrorCode"), ExecuteTicket.ErrorCode, FString(TEXT("E4204")));
	TestEqual(TEXT("Reason"), ExecuteTicket.Reason, FString(TEXT("confirm_request_not_ready")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorExecuteTicketDigestMismatchTest,
	"HCIAbilityKit.Editor.AgentExecutorExecuteTicket.DigestMismatchReturnsE4202",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorExecuteTicketDigestMismatchTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeF11SelectedReviewReport(false);
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeF11ApplyRequest(Review);
	FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeF11ConfirmRequest(ApplyRequest, Review, true);
	ConfirmRequest.SelectionDigest = TEXT("crc32_BAD0C0DE");

	FHCIAbilityKitAgentExecuteTicket ExecuteTicket;
	TestTrue(TEXT("Build execute ticket"), FHCIAbilityKitAgentExecutorExecuteTicketBridge::BuildExecuteTicket(ConfirmRequest, ApplyRequest, Review, ExecuteTicket));
	TestFalse(TEXT("Should not be ready"), ExecuteTicket.bReadyToSimulateExecute);
	TestEqual(TEXT("ErrorCode"), ExecuteTicket.ErrorCode, FString(TEXT("E4202")));
	TestEqual(TEXT("Reason"), ExecuteTicket.Reason, FString(TEXT("selection_digest_mismatch")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorExecuteTicketJsonFieldsTest,
	"HCIAbilityKit.Editor.AgentExecutorExecuteTicket.JsonIncludesReadyAndPolicyFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorExecuteTicketJsonFieldsTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeF11SelectedReviewReport(false);
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeF11ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeF11ConfirmRequest(ApplyRequest, Review, true);

	FHCIAbilityKitAgentExecuteTicket ExecuteTicket;
	TestTrue(TEXT("Build execute ticket"), FHCIAbilityKitAgentExecutorExecuteTicketBridge::BuildExecuteTicket(ConfirmRequest, ApplyRequest, Review, ExecuteTicket));

	FString JsonText;
	TestTrue(TEXT("Serialize execute ticket json"), FHCIAbilityKitAgentExecuteTicketJsonSerializer::SerializeToJsonString(ExecuteTicket, JsonText));
	TestTrue(TEXT("JSON includes confirm_request_id"), JsonText.Contains(TEXT("\"confirm_request_id\"")));
	TestTrue(TEXT("JSON includes ready_to_simulate_execute"), JsonText.Contains(TEXT("\"ready_to_simulate_execute\"")));
	TestTrue(TEXT("JSON includes transaction_mode"), JsonText.Contains(TEXT("\"transaction_mode\"")));
	TestTrue(TEXT("JSON includes termination_policy"), JsonText.Contains(TEXT("\"termination_policy\"")));
	TestTrue(TEXT("JSON includes locate_strategy"), JsonText.Contains(TEXT("\"locate_strategy\"")));
	return true;
}

#endif

