#if WITH_DEV_AUTOMATION_TESTS

#include "Agent/HCIAbilityKitAgentApplyConfirmRequest.h"
#include "Agent/HCIAbilityKitAgentApplyRequest.h"
#include "Agent/HCIAbilityKitAgentExecuteTicket.h"
#include "Agent/HCIAbilityKitAgentExecutorApplyConfirmBridge.h"
#include "Agent/HCIAbilityKitAgentExecutorApplyRequestBridge.h"
#include "Agent/HCIAbilityKitAgentExecutorExecuteTicketBridge.h"
#include "Agent/HCIAbilityKitAgentExecutorSimulateExecuteFinalReportBridge.h"
#include "Agent/HCIAbilityKitAgentExecutorSimulateExecuteReceiptBridge.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteFinalReport.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteFinalReportJsonSerializer.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteReceipt.h"
#include "Agent/HCIAbilityKitDryRunDiff.h"
#include "Misc/AutomationTest.h"

namespace
{
static FHCIAbilityKitDryRunDiffReport MakeF13SelectedReviewReport()
{
	FHCIAbilityKitDryRunDiffReport Report;
	Report.RequestId = TEXT("req_f13_review");

	FHCIAbilityKitDryRunDiffItem& Item = Report.DiffItems.AddDefaulted_GetRef();
	Item.AssetPath = TEXT("/Game/Levels/Demo/SM_F13_A.SM_F13_A");
	Item.Field = TEXT("step:s1");
	Item.ToolName = TEXT("ScanLevelMeshRisks");
	Item.Risk = EHCIAbilityKitDryRunRisk::ReadOnly;
	Item.ObjectType = EHCIAbilityKitDryRunObjectType::Actor;
	Item.LocateStrategy = EHCIAbilityKitDryRunLocateStrategy::CameraFocus;
	Item.EvidenceKey = TEXT("actor_path");
	Item.ActorPath = TEXT("/Game/Maps/Demo.PersistentLevel.SM_F13_A_01");

	FHCIAbilityKitDryRunDiff::NormalizeAndFinalize(Report);
	return Report;
}

static FHCIAbilityKitAgentApplyRequest MakeF13ApplyRequest(const FHCIAbilityKitDryRunDiffReport& Review)
{
	FHCIAbilityKitAgentApplyRequest ApplyRequest;
	check(FHCIAbilityKitAgentExecutorApplyRequestBridge::BuildApplyRequest(Review, ApplyRequest));
	return ApplyRequest;
}

static FHCIAbilityKitAgentApplyConfirmRequest MakeF13ConfirmRequest(
	const FHCIAbilityKitAgentApplyRequest& ApplyRequest,
	const FHCIAbilityKitDryRunDiffReport& Review,
	const bool bUserConfirmed)
{
	FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest;
	check(FHCIAbilityKitAgentExecutorApplyConfirmBridge::BuildConfirmRequest(ApplyRequest, Review, bUserConfirmed, ConfirmRequest));
	return ConfirmRequest;
}

static FHCIAbilityKitAgentExecuteTicket MakeF13ExecuteTicket(
	const FHCIAbilityKitAgentApplyConfirmRequest& ConfirmRequest,
	const FHCIAbilityKitAgentApplyRequest& ApplyRequest,
	const FHCIAbilityKitDryRunDiffReport& Review)
{
	FHCIAbilityKitAgentExecuteTicket ExecuteTicket;
	check(FHCIAbilityKitAgentExecutorExecuteTicketBridge::BuildExecuteTicket(ConfirmRequest, ApplyRequest, Review, ExecuteTicket));
	return ExecuteTicket;
}

static FHCIAbilityKitAgentSimulateExecuteReceipt MakeF13Receipt(
	const FHCIAbilityKitAgentExecuteTicket& ExecuteTicket,
	const FHCIAbilityKitAgentApplyConfirmRequest& ConfirmRequest,
	const FHCIAbilityKitAgentApplyRequest& ApplyRequest,
	const FHCIAbilityKitDryRunDiffReport& Review)
{
	FHCIAbilityKitAgentSimulateExecuteReceipt Receipt;
	check(FHCIAbilityKitAgentExecutorSimulateExecuteReceiptBridge::BuildSimulateExecuteReceipt(
		ExecuteTicket, ConfirmRequest, ApplyRequest, Review, Receipt));
	return Receipt;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorSimFinalReportReadyTest,
	"HCIAbilityKit.Editor.AgentExecutorSimFinalReport.ReadyWhenReceiptAcceptedAndStateMatches",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorSimFinalReportReadyTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeF13SelectedReviewReport();
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeF13ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeF13ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAbilityKitAgentExecuteTicket ExecuteTicket = MakeF13ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteReceipt Receipt = MakeF13Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);

	FHCIAbilityKitAgentSimulateExecuteFinalReport FinalReport;
	TestTrue(TEXT("Build final sim report"), FHCIAbilityKitAgentExecutorSimulateExecuteFinalReportBridge::BuildSimulateExecuteFinalReport(
		Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, FinalReport));
	TestTrue(TEXT("Simulation completed"), FinalReport.bSimulationCompleted);
	TestEqual(TEXT("TerminalStatus"), FinalReport.TerminalStatus, FString(TEXT("completed")));
	TestEqual(TEXT("ErrorCode"), FinalReport.ErrorCode, FString(TEXT("-")));
	TestEqual(TEXT("Reason"), FinalReport.Reason, FString(TEXT("simulate_execute_final_report_ready")));
	TestEqual(TEXT("ReceiptId carried"), FinalReport.SimExecuteReceiptId, Receipt.RequestId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorSimFinalReportReceiptRejectedTest,
	"HCIAbilityKit.Editor.AgentExecutorSimFinalReport.ReceiptRejectedReturnsE4206",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorSimFinalReportReceiptRejectedTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeF13SelectedReviewReport();
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeF13ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeF13ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAbilityKitAgentExecuteTicket ExecuteTicket = MakeF13ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	FHCIAbilityKitAgentSimulateExecuteReceipt Receipt = MakeF13Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	Receipt.bSimulatedDispatchAccepted = false;

	FHCIAbilityKitAgentSimulateExecuteFinalReport FinalReport;
	TestTrue(TEXT("Build final sim report"), FHCIAbilityKitAgentExecutorSimulateExecuteFinalReportBridge::BuildSimulateExecuteFinalReport(
		Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, FinalReport));
	TestFalse(TEXT("Simulation completed"), FinalReport.bSimulationCompleted);
	TestEqual(TEXT("TerminalStatus"), FinalReport.TerminalStatus, FString(TEXT("blocked")));
	TestEqual(TEXT("ErrorCode"), FinalReport.ErrorCode, FString(TEXT("E4206")));
	TestEqual(TEXT("Reason"), FinalReport.Reason, FString(TEXT("simulate_execute_receipt_not_accepted")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorSimFinalReportDigestMismatchTest,
	"HCIAbilityKit.Editor.AgentExecutorSimFinalReport.DigestMismatchReturnsE4202",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorSimFinalReportDigestMismatchTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeF13SelectedReviewReport();
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeF13ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeF13ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAbilityKitAgentExecuteTicket ExecuteTicket = MakeF13ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	FHCIAbilityKitAgentSimulateExecuteReceipt Receipt = MakeF13Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	Receipt.SelectionDigest = TEXT("crc32_BAD0C0DE");

	FHCIAbilityKitAgentSimulateExecuteFinalReport FinalReport;
	TestTrue(TEXT("Build final sim report"), FHCIAbilityKitAgentExecutorSimulateExecuteFinalReportBridge::BuildSimulateExecuteFinalReport(
		Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, FinalReport));
	TestFalse(TEXT("Simulation completed"), FinalReport.bSimulationCompleted);
	TestEqual(TEXT("ErrorCode"), FinalReport.ErrorCode, FString(TEXT("E4202")));
	TestEqual(TEXT("Reason"), FinalReport.Reason, FString(TEXT("selection_digest_mismatch")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorSimFinalReportJsonFieldsTest,
	"HCIAbilityKit.Editor.AgentExecutorSimFinalReport.JsonIncludesTerminalAndReceiptFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorSimFinalReportJsonFieldsTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeF13SelectedReviewReport();
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeF13ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeF13ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAbilityKitAgentExecuteTicket ExecuteTicket = MakeF13ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteReceipt Receipt = MakeF13Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);

	FHCIAbilityKitAgentSimulateExecuteFinalReport FinalReport;
	TestTrue(TEXT("Build final sim report"), FHCIAbilityKitAgentExecutorSimulateExecuteFinalReportBridge::BuildSimulateExecuteFinalReport(
		Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, FinalReport));

	FString JsonText;
	TestTrue(TEXT("Serialize final sim report json"), FHCIAbilityKitAgentSimulateExecuteFinalReportJsonSerializer::SerializeToJsonString(FinalReport, JsonText));
	TestTrue(TEXT("JSON includes sim_execute_receipt_id"), JsonText.Contains(TEXT("\"sim_execute_receipt_id\"")));
	TestTrue(TEXT("JSON includes terminal_status"), JsonText.Contains(TEXT("\"terminal_status\"")));
	TestTrue(TEXT("JSON includes simulation_completed"), JsonText.Contains(TEXT("\"simulation_completed\"")));
	TestTrue(TEXT("JSON includes transaction_mode"), JsonText.Contains(TEXT("\"transaction_mode\"")));
	TestTrue(TEXT("JSON includes locate_strategy"), JsonText.Contains(TEXT("\"locate_strategy\"")));
	return true;
}

#endif
