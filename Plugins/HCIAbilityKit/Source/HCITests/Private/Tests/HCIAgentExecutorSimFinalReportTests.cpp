#if WITH_DEV_AUTOMATION_TESTS

#include "Agent/Contracts/StageF/HCIAgentApplyConfirmRequest.h"
#include "Agent/Contracts/StageF/HCIAgentApplyRequest.h"
#include "Agent/Contracts/StageF/HCIAgentExecuteTicket.h"
#include "Agent/Bridges/HCIAgentExecutorApplyConfirmBridge.h"
#include "Agent/Bridges/HCIAgentExecutorApplyRequestBridge.h"
#include "Agent/Bridges/HCIAgentExecutorExecuteTicketBridge.h"
#include "Agent/Bridges/HCIAgentExecutorSimulateExecuteFinalReportBridge.h"
#include "Agent/Bridges/HCIAgentExecutorSimulateExecuteReceiptBridge.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteFinalReport.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteFinalReportJsonSerializer.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteReceipt.h"
#include "Agent/Executor/HCIDryRunDiff.h"
#include "Misc/AutomationTest.h"

namespace
{
static FHCIDryRunDiffReport MakeF13SelectedReviewReport()
{
	FHCIDryRunDiffReport Report;
	Report.RequestId = TEXT("req_f13_review");

	FHCIDryRunDiffItem& Item = Report.DiffItems.AddDefaulted_GetRef();
	Item.AssetPath = TEXT("/Game/Levels/Demo/SM_F13_A.SM_F13_A");
	Item.Field = TEXT("step:s1");
	Item.ToolName = TEXT("ScanLevelMeshRisks");
	Item.Risk = EHCIDryRunRisk::ReadOnly;
	Item.ObjectType = EHCIDryRunObjectType::Actor;
	Item.LocateStrategy = EHCIDryRunLocateStrategy::CameraFocus;
	Item.EvidenceKey = TEXT("actor_path");
	Item.ActorPath = TEXT("/Game/Maps/Demo.PersistentLevel.SM_F13_A_01");

	FHCIDryRunDiff::NormalizeAndFinalize(Report);
	return Report;
}

static FHCIAgentApplyRequest MakeF13ApplyRequest(const FHCIDryRunDiffReport& Review)
{
	FHCIAgentApplyRequest ApplyRequest;
	check(FHCIAgentExecutorApplyRequestBridge::BuildApplyRequest(Review, ApplyRequest));
	return ApplyRequest;
}

static FHCIAgentApplyConfirmRequest MakeF13ConfirmRequest(
	const FHCIAgentApplyRequest& ApplyRequest,
	const FHCIDryRunDiffReport& Review,
	const bool bUserConfirmed)
{
	FHCIAgentApplyConfirmRequest ConfirmRequest;
	check(FHCIAgentExecutorApplyConfirmBridge::BuildConfirmRequest(ApplyRequest, Review, bUserConfirmed, ConfirmRequest));
	return ConfirmRequest;
}

static FHCIAgentExecuteTicket MakeF13ExecuteTicket(
	const FHCIAgentApplyConfirmRequest& ConfirmRequest,
	const FHCIAgentApplyRequest& ApplyRequest,
	const FHCIDryRunDiffReport& Review)
{
	FHCIAgentExecuteTicket ExecuteTicket;
	check(FHCIAgentExecutorExecuteTicketBridge::BuildExecuteTicket(ConfirmRequest, ApplyRequest, Review, ExecuteTicket));
	return ExecuteTicket;
}

static FHCIAgentSimulateExecuteReceipt MakeF13Receipt(
	const FHCIAgentExecuteTicket& ExecuteTicket,
	const FHCIAgentApplyConfirmRequest& ConfirmRequest,
	const FHCIAgentApplyRequest& ApplyRequest,
	const FHCIDryRunDiffReport& Review)
{
	FHCIAgentSimulateExecuteReceipt Receipt;
	check(FHCIAgentExecutorSimulateExecuteReceiptBridge::BuildSimulateExecuteReceipt(
		ExecuteTicket, ConfirmRequest, ApplyRequest, Review, Receipt));
	return Receipt;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorSimFinalReportReadyTest,
	"HCI.Editor.AgentExecutorSimFinalReport.ReadyWhenReceiptAcceptedAndStateMatches",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorSimFinalReportReadyTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeF13SelectedReviewReport();
	const FHCIAgentApplyRequest ApplyRequest = MakeF13ApplyRequest(Review);
	const FHCIAgentApplyConfirmRequest ConfirmRequest = MakeF13ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAgentExecuteTicket ExecuteTicket = MakeF13ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteReceipt Receipt = MakeF13Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);

	FHCIAgentSimulateExecuteFinalReport FinalReport;
	TestTrue(TEXT("Build final sim report"), FHCIAgentExecutorSimulateExecuteFinalReportBridge::BuildSimulateExecuteFinalReport(
		Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, FinalReport));
	TestTrue(TEXT("Simulation completed"), FinalReport.bSimulationCompleted);
	TestEqual(TEXT("TerminalStatus"), FinalReport.TerminalStatus, FString(TEXT("completed")));
	TestEqual(TEXT("ErrorCode"), FinalReport.ErrorCode, FString(TEXT("-")));
	TestEqual(TEXT("Reason"), FinalReport.Reason, FString(TEXT("simulate_execute_final_report_ready")));
	TestEqual(TEXT("ReceiptId carried"), FinalReport.SimExecuteReceiptId, Receipt.RequestId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorSimFinalReportReceiptRejectedTest,
	"HCI.Editor.AgentExecutorSimFinalReport.ReceiptRejectedReturnsE4206",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorSimFinalReportReceiptRejectedTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeF13SelectedReviewReport();
	const FHCIAgentApplyRequest ApplyRequest = MakeF13ApplyRequest(Review);
	const FHCIAgentApplyConfirmRequest ConfirmRequest = MakeF13ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAgentExecuteTicket ExecuteTicket = MakeF13ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	FHCIAgentSimulateExecuteReceipt Receipt = MakeF13Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	Receipt.bSimulatedDispatchAccepted = false;

	FHCIAgentSimulateExecuteFinalReport FinalReport;
	TestTrue(TEXT("Build final sim report"), FHCIAgentExecutorSimulateExecuteFinalReportBridge::BuildSimulateExecuteFinalReport(
		Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, FinalReport));
	TestFalse(TEXT("Simulation completed"), FinalReport.bSimulationCompleted);
	TestEqual(TEXT("TerminalStatus"), FinalReport.TerminalStatus, FString(TEXT("blocked")));
	TestEqual(TEXT("ErrorCode"), FinalReport.ErrorCode, FString(TEXT("E4206")));
	TestEqual(TEXT("Reason"), FinalReport.Reason, FString(TEXT("simulate_execute_receipt_not_accepted")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorSimFinalReportDigestMismatchTest,
	"HCI.Editor.AgentExecutorSimFinalReport.DigestMismatchReturnsE4202",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorSimFinalReportDigestMismatchTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeF13SelectedReviewReport();
	const FHCIAgentApplyRequest ApplyRequest = MakeF13ApplyRequest(Review);
	const FHCIAgentApplyConfirmRequest ConfirmRequest = MakeF13ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAgentExecuteTicket ExecuteTicket = MakeF13ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	FHCIAgentSimulateExecuteReceipt Receipt = MakeF13Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	Receipt.SelectionDigest = TEXT("crc32_BAD0C0DE");

	FHCIAgentSimulateExecuteFinalReport FinalReport;
	TestTrue(TEXT("Build final sim report"), FHCIAgentExecutorSimulateExecuteFinalReportBridge::BuildSimulateExecuteFinalReport(
		Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, FinalReport));
	TestFalse(TEXT("Simulation completed"), FinalReport.bSimulationCompleted);
	TestEqual(TEXT("ErrorCode"), FinalReport.ErrorCode, FString(TEXT("E4202")));
	TestEqual(TEXT("Reason"), FinalReport.Reason, FString(TEXT("selection_digest_mismatch")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorSimFinalReportJsonFieldsTest,
	"HCI.Editor.AgentExecutorSimFinalReport.JsonIncludesTerminalAndReceiptFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorSimFinalReportJsonFieldsTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeF13SelectedReviewReport();
	const FHCIAgentApplyRequest ApplyRequest = MakeF13ApplyRequest(Review);
	const FHCIAgentApplyConfirmRequest ConfirmRequest = MakeF13ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAgentExecuteTicket ExecuteTicket = MakeF13ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteReceipt Receipt = MakeF13Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);

	FHCIAgentSimulateExecuteFinalReport FinalReport;
	TestTrue(TEXT("Build final sim report"), FHCIAgentExecutorSimulateExecuteFinalReportBridge::BuildSimulateExecuteFinalReport(
		Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, FinalReport));

	FString JsonText;
	TestTrue(TEXT("Serialize final sim report json"), FHCIAgentSimulateExecuteFinalReportJsonSerializer::SerializeToJsonString(FinalReport, JsonText));
	TestTrue(TEXT("JSON includes sim_execute_receipt_id"), JsonText.Contains(TEXT("\"sim_execute_receipt_id\"")));
	TestTrue(TEXT("JSON includes terminal_status"), JsonText.Contains(TEXT("\"terminal_status\"")));
	TestTrue(TEXT("JSON includes simulation_completed"), JsonText.Contains(TEXT("\"simulation_completed\"")));
	TestTrue(TEXT("JSON includes transaction_mode"), JsonText.Contains(TEXT("\"transaction_mode\"")));
	TestTrue(TEXT("JSON includes locate_strategy"), JsonText.Contains(TEXT("\"locate_strategy\"")));
	return true;
}

#endif
