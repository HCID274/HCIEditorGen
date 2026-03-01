#if WITH_DEV_AUTOMATION_TESTS

#include "Agent/Contracts/StageF/HCIAgentApplyConfirmRequest.h"
#include "Agent/Contracts/StageF/HCIAgentApplyRequest.h"
#include "Agent/Contracts/StageF/HCIAgentExecuteTicket.h"
#include "Agent/Bridges/HCIAgentExecutorApplyConfirmBridge.h"
#include "Agent/Bridges/HCIAgentExecutorApplyRequestBridge.h"
#include "Agent/Bridges/HCIAgentExecutorExecuteTicketBridge.h"
#include "Agent/Bridges/HCIAgentExecutorSimulateExecuteReceiptBridge.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteReceipt.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteReceiptJsonSerializer.h"
#include "Agent/Executor/HCIDryRunDiff.h"
#include "Misc/AutomationTest.h"

namespace
{
static FHCIDryRunDiffReport MakeF12SelectedReviewReport()
{
	FHCIDryRunDiffReport Report;
	Report.RequestId = TEXT("req_f12_review");

	FHCIDryRunDiffItem& Item = Report.DiffItems.AddDefaulted_GetRef();
	Item.AssetPath = TEXT("/Game/Levels/Demo/SM_F12_A.SM_F12_A");
	Item.Field = TEXT("step:s1");
	Item.ToolName = TEXT("ScanLevelMeshRisks");
	Item.Risk = EHCIDryRunRisk::ReadOnly;
	Item.ObjectType = EHCIDryRunObjectType::Actor;
	Item.LocateStrategy = EHCIDryRunLocateStrategy::CameraFocus;
	Item.EvidenceKey = TEXT("actor_path");
	Item.ActorPath = TEXT("/Game/Maps/Demo.PersistentLevel.SM_F12_A_01");

	FHCIDryRunDiff::NormalizeAndFinalize(Report);
	return Report;
}

static FHCIAgentApplyRequest MakeF12ApplyRequest(const FHCIDryRunDiffReport& Review)
{
	FHCIAgentApplyRequest ApplyRequest;
	check(FHCIAgentExecutorApplyRequestBridge::BuildApplyRequest(Review, ApplyRequest));
	return ApplyRequest;
}

static FHCIAgentApplyConfirmRequest MakeF12ConfirmRequest(
	const FHCIAgentApplyRequest& ApplyRequest,
	const FHCIDryRunDiffReport& Review,
	const bool bUserConfirmed)
{
	FHCIAgentApplyConfirmRequest ConfirmRequest;
	check(FHCIAgentExecutorApplyConfirmBridge::BuildConfirmRequest(ApplyRequest, Review, bUserConfirmed, ConfirmRequest));
	return ConfirmRequest;
}

static FHCIAgentExecuteTicket MakeF12ExecuteTicket(
	const FHCIAgentApplyConfirmRequest& ConfirmRequest,
	const FHCIAgentApplyRequest& ApplyRequest,
	const FHCIDryRunDiffReport& Review)
{
	FHCIAgentExecuteTicket ExecuteTicket;
	check(FHCIAgentExecutorExecuteTicketBridge::BuildExecuteTicket(ConfirmRequest, ApplyRequest, Review, ExecuteTicket));
	return ExecuteTicket;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorSimExecuteReceiptReadyTest,
	"HCI.Editor.AgentExecutorSimExecuteReceipt.ReadyWhenExecuteTicketMatchesCurrentState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorSimExecuteReceiptReadyTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeF12SelectedReviewReport();
	const FHCIAgentApplyRequest ApplyRequest = MakeF12ApplyRequest(Review);
	const FHCIAgentApplyConfirmRequest ConfirmRequest = MakeF12ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAgentExecuteTicket ExecuteTicket = MakeF12ExecuteTicket(ConfirmRequest, ApplyRequest, Review);

	FHCIAgentSimulateExecuteReceipt Receipt;
	TestTrue(TEXT("Build simulate execute receipt"), FHCIAgentExecutorSimulateExecuteReceiptBridge::BuildSimulateExecuteReceipt(
		ExecuteTicket, ConfirmRequest, ApplyRequest, Review, Receipt));
	TestTrue(TEXT("Simulated dispatch accepted"), Receipt.bSimulatedDispatchAccepted);
	TestEqual(TEXT("ErrorCode"), Receipt.ErrorCode, FString(TEXT("-")));
	TestEqual(TEXT("Reason"), Receipt.Reason, FString(TEXT("simulate_execute_receipt_ready")));
	TestEqual(TEXT("ExecuteTicketId"), Receipt.ExecuteTicketId, ExecuteTicket.RequestId);
	TestEqual(TEXT("ConfirmRequestId"), Receipt.ConfirmRequestId, ConfirmRequest.RequestId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorSimExecuteReceiptTicketNotReadyTest,
	"HCI.Editor.AgentExecutorSimExecuteReceipt.ExecuteTicketNotReadyReturnsE4205",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorSimExecuteReceiptTicketNotReadyTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeF12SelectedReviewReport();
	const FHCIAgentApplyRequest ApplyRequest = MakeF12ApplyRequest(Review);
	const FHCIAgentApplyConfirmRequest ConfirmRequest = MakeF12ConfirmRequest(ApplyRequest, Review, true);
	FHCIAgentExecuteTicket ExecuteTicket = MakeF12ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	ExecuteTicket.bReadyToSimulateExecute = false;

	FHCIAgentSimulateExecuteReceipt Receipt;
	TestTrue(TEXT("Build simulate execute receipt"), FHCIAgentExecutorSimulateExecuteReceiptBridge::BuildSimulateExecuteReceipt(
		ExecuteTicket, ConfirmRequest, ApplyRequest, Review, Receipt));
	TestFalse(TEXT("Simulated dispatch accepted"), Receipt.bSimulatedDispatchAccepted);
	TestEqual(TEXT("ErrorCode"), Receipt.ErrorCode, FString(TEXT("E4205")));
	TestEqual(TEXT("Reason"), Receipt.Reason, FString(TEXT("execute_ticket_not_ready")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorSimExecuteReceiptDigestMismatchTest,
	"HCI.Editor.AgentExecutorSimExecuteReceipt.DigestMismatchReturnsE4202",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorSimExecuteReceiptDigestMismatchTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeF12SelectedReviewReport();
	const FHCIAgentApplyRequest ApplyRequest = MakeF12ApplyRequest(Review);
	const FHCIAgentApplyConfirmRequest ConfirmRequest = MakeF12ConfirmRequest(ApplyRequest, Review, true);
	FHCIAgentExecuteTicket ExecuteTicket = MakeF12ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	ExecuteTicket.SelectionDigest = TEXT("crc32_1234ABCD");

	FHCIAgentSimulateExecuteReceipt Receipt;
	TestTrue(TEXT("Build simulate execute receipt"), FHCIAgentExecutorSimulateExecuteReceiptBridge::BuildSimulateExecuteReceipt(
		ExecuteTicket, ConfirmRequest, ApplyRequest, Review, Receipt));
	TestFalse(TEXT("Simulated dispatch accepted"), Receipt.bSimulatedDispatchAccepted);
	TestEqual(TEXT("ErrorCode"), Receipt.ErrorCode, FString(TEXT("E4202")));
	TestEqual(TEXT("Reason"), Receipt.Reason, FString(TEXT("selection_digest_mismatch")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorSimExecuteReceiptJsonFieldsTest,
	"HCI.Editor.AgentExecutorSimExecuteReceipt.JsonIncludesDispatchAcceptedAndPolicyFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorSimExecuteReceiptJsonFieldsTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeF12SelectedReviewReport();
	const FHCIAgentApplyRequest ApplyRequest = MakeF12ApplyRequest(Review);
	const FHCIAgentApplyConfirmRequest ConfirmRequest = MakeF12ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAgentExecuteTicket ExecuteTicket = MakeF12ExecuteTicket(ConfirmRequest, ApplyRequest, Review);

	FHCIAgentSimulateExecuteReceipt Receipt;
	TestTrue(TEXT("Build simulate execute receipt"), FHCIAgentExecutorSimulateExecuteReceiptBridge::BuildSimulateExecuteReceipt(
		ExecuteTicket, ConfirmRequest, ApplyRequest, Review, Receipt));

	FString JsonText;
	TestTrue(TEXT("Serialize simulate execute receipt json"), FHCIAgentSimulateExecuteReceiptJsonSerializer::SerializeToJsonString(Receipt, JsonText));
	TestTrue(TEXT("JSON includes execute_ticket_id"), JsonText.Contains(TEXT("\"execute_ticket_id\"")));
	TestTrue(TEXT("JSON includes simulated_dispatch_accepted"), JsonText.Contains(TEXT("\"simulated_dispatch_accepted\"")));
	TestTrue(TEXT("JSON includes transaction_mode"), JsonText.Contains(TEXT("\"transaction_mode\"")));
	TestTrue(TEXT("JSON includes termination_policy"), JsonText.Contains(TEXT("\"termination_policy\"")));
	TestTrue(TEXT("JSON includes locate_strategy"), JsonText.Contains(TEXT("\"locate_strategy\"")));
	return true;
}

#endif
