#if WITH_DEV_AUTOMATION_TESTS

#include "Agent/HCIAbilityKitAgentApplyConfirmRequest.h"
#include "Agent/HCIAbilityKitAgentApplyRequest.h"
#include "Agent/HCIAbilityKitAgentExecuteTicket.h"
#include "Agent/HCIAbilityKitAgentExecutorApplyConfirmBridge.h"
#include "Agent/HCIAbilityKitAgentExecutorApplyRequestBridge.h"
#include "Agent/HCIAbilityKitAgentExecutorExecuteTicketBridge.h"
#include "Agent/HCIAbilityKitAgentExecutorSimulateExecuteReceiptBridge.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteReceipt.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteReceiptJsonSerializer.h"
#include "Agent/HCIAbilityKitDryRunDiff.h"
#include "Misc/AutomationTest.h"

namespace
{
static FHCIAbilityKitDryRunDiffReport MakeF12SelectedReviewReport()
{
	FHCIAbilityKitDryRunDiffReport Report;
	Report.RequestId = TEXT("req_f12_review");

	FHCIAbilityKitDryRunDiffItem& Item = Report.DiffItems.AddDefaulted_GetRef();
	Item.AssetPath = TEXT("/Game/Levels/Demo/SM_F12_A.SM_F12_A");
	Item.Field = TEXT("step:s1");
	Item.ToolName = TEXT("ScanLevelMeshRisks");
	Item.Risk = EHCIAbilityKitDryRunRisk::ReadOnly;
	Item.ObjectType = EHCIAbilityKitDryRunObjectType::Actor;
	Item.LocateStrategy = EHCIAbilityKitDryRunLocateStrategy::CameraFocus;
	Item.EvidenceKey = TEXT("actor_path");
	Item.ActorPath = TEXT("/Game/Maps/Demo.PersistentLevel.SM_F12_A_01");

	FHCIAbilityKitDryRunDiff::NormalizeAndFinalize(Report);
	return Report;
}

static FHCIAbilityKitAgentApplyRequest MakeF12ApplyRequest(const FHCIAbilityKitDryRunDiffReport& Review)
{
	FHCIAbilityKitAgentApplyRequest ApplyRequest;
	check(FHCIAbilityKitAgentExecutorApplyRequestBridge::BuildApplyRequest(Review, ApplyRequest));
	return ApplyRequest;
}

static FHCIAbilityKitAgentApplyConfirmRequest MakeF12ConfirmRequest(
	const FHCIAbilityKitAgentApplyRequest& ApplyRequest,
	const FHCIAbilityKitDryRunDiffReport& Review,
	const bool bUserConfirmed)
{
	FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest;
	check(FHCIAbilityKitAgentExecutorApplyConfirmBridge::BuildConfirmRequest(ApplyRequest, Review, bUserConfirmed, ConfirmRequest));
	return ConfirmRequest;
}

static FHCIAbilityKitAgentExecuteTicket MakeF12ExecuteTicket(
	const FHCIAbilityKitAgentApplyConfirmRequest& ConfirmRequest,
	const FHCIAbilityKitAgentApplyRequest& ApplyRequest,
	const FHCIAbilityKitDryRunDiffReport& Review)
{
	FHCIAbilityKitAgentExecuteTicket ExecuteTicket;
	check(FHCIAbilityKitAgentExecutorExecuteTicketBridge::BuildExecuteTicket(ConfirmRequest, ApplyRequest, Review, ExecuteTicket));
	return ExecuteTicket;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorSimExecuteReceiptReadyTest,
	"HCIAbilityKit.Editor.AgentExecutorSimExecuteReceipt.ReadyWhenExecuteTicketMatchesCurrentState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorSimExecuteReceiptReadyTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeF12SelectedReviewReport();
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeF12ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeF12ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAbilityKitAgentExecuteTicket ExecuteTicket = MakeF12ExecuteTicket(ConfirmRequest, ApplyRequest, Review);

	FHCIAbilityKitAgentSimulateExecuteReceipt Receipt;
	TestTrue(TEXT("Build simulate execute receipt"), FHCIAbilityKitAgentExecutorSimulateExecuteReceiptBridge::BuildSimulateExecuteReceipt(
		ExecuteTicket, ConfirmRequest, ApplyRequest, Review, Receipt));
	TestTrue(TEXT("Simulated dispatch accepted"), Receipt.bSimulatedDispatchAccepted);
	TestEqual(TEXT("ErrorCode"), Receipt.ErrorCode, FString(TEXT("-")));
	TestEqual(TEXT("Reason"), Receipt.Reason, FString(TEXT("simulate_execute_receipt_ready")));
	TestEqual(TEXT("ExecuteTicketId"), Receipt.ExecuteTicketId, ExecuteTicket.RequestId);
	TestEqual(TEXT("ConfirmRequestId"), Receipt.ConfirmRequestId, ConfirmRequest.RequestId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorSimExecuteReceiptTicketNotReadyTest,
	"HCIAbilityKit.Editor.AgentExecutorSimExecuteReceipt.ExecuteTicketNotReadyReturnsE4205",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorSimExecuteReceiptTicketNotReadyTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeF12SelectedReviewReport();
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeF12ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeF12ConfirmRequest(ApplyRequest, Review, true);
	FHCIAbilityKitAgentExecuteTicket ExecuteTicket = MakeF12ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	ExecuteTicket.bReadyToSimulateExecute = false;

	FHCIAbilityKitAgentSimulateExecuteReceipt Receipt;
	TestTrue(TEXT("Build simulate execute receipt"), FHCIAbilityKitAgentExecutorSimulateExecuteReceiptBridge::BuildSimulateExecuteReceipt(
		ExecuteTicket, ConfirmRequest, ApplyRequest, Review, Receipt));
	TestFalse(TEXT("Simulated dispatch accepted"), Receipt.bSimulatedDispatchAccepted);
	TestEqual(TEXT("ErrorCode"), Receipt.ErrorCode, FString(TEXT("E4205")));
	TestEqual(TEXT("Reason"), Receipt.Reason, FString(TEXT("execute_ticket_not_ready")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorSimExecuteReceiptDigestMismatchTest,
	"HCIAbilityKit.Editor.AgentExecutorSimExecuteReceipt.DigestMismatchReturnsE4202",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorSimExecuteReceiptDigestMismatchTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeF12SelectedReviewReport();
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeF12ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeF12ConfirmRequest(ApplyRequest, Review, true);
	FHCIAbilityKitAgentExecuteTicket ExecuteTicket = MakeF12ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	ExecuteTicket.SelectionDigest = TEXT("crc32_1234ABCD");

	FHCIAbilityKitAgentSimulateExecuteReceipt Receipt;
	TestTrue(TEXT("Build simulate execute receipt"), FHCIAbilityKitAgentExecutorSimulateExecuteReceiptBridge::BuildSimulateExecuteReceipt(
		ExecuteTicket, ConfirmRequest, ApplyRequest, Review, Receipt));
	TestFalse(TEXT("Simulated dispatch accepted"), Receipt.bSimulatedDispatchAccepted);
	TestEqual(TEXT("ErrorCode"), Receipt.ErrorCode, FString(TEXT("E4202")));
	TestEqual(TEXT("Reason"), Receipt.Reason, FString(TEXT("selection_digest_mismatch")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorSimExecuteReceiptJsonFieldsTest,
	"HCIAbilityKit.Editor.AgentExecutorSimExecuteReceipt.JsonIncludesDispatchAcceptedAndPolicyFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorSimExecuteReceiptJsonFieldsTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeF12SelectedReviewReport();
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeF12ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeF12ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAbilityKitAgentExecuteTicket ExecuteTicket = MakeF12ExecuteTicket(ConfirmRequest, ApplyRequest, Review);

	FHCIAbilityKitAgentSimulateExecuteReceipt Receipt;
	TestTrue(TEXT("Build simulate execute receipt"), FHCIAbilityKitAgentExecutorSimulateExecuteReceiptBridge::BuildSimulateExecuteReceipt(
		ExecuteTicket, ConfirmRequest, ApplyRequest, Review, Receipt));

	FString JsonText;
	TestTrue(TEXT("Serialize simulate execute receipt json"), FHCIAbilityKitAgentSimulateExecuteReceiptJsonSerializer::SerializeToJsonString(Receipt, JsonText));
	TestTrue(TEXT("JSON includes execute_ticket_id"), JsonText.Contains(TEXT("\"execute_ticket_id\"")));
	TestTrue(TEXT("JSON includes simulated_dispatch_accepted"), JsonText.Contains(TEXT("\"simulated_dispatch_accepted\"")));
	TestTrue(TEXT("JSON includes transaction_mode"), JsonText.Contains(TEXT("\"transaction_mode\"")));
	TestTrue(TEXT("JSON includes termination_policy"), JsonText.Contains(TEXT("\"termination_policy\"")));
	TestTrue(TEXT("JSON includes locate_strategy"), JsonText.Contains(TEXT("\"locate_strategy\"")));
	return true;
}

#endif
