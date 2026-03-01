#if WITH_DEV_AUTOMATION_TESTS

#include "Agent/Contracts/StageF/HCIAgentApplyConfirmRequest.h"
#include "Agent/Contracts/StageF/HCIAgentApplyRequest.h"
#include "Agent/Contracts/StageF/HCIAgentExecuteTicket.h"
#include "Agent/Bridges/HCIAgentExecutorApplyConfirmBridge.h"
#include "Agent/Bridges/HCIAgentExecutorApplyRequestBridge.h"
#include "Agent/Bridges/HCIAgentExecutorExecuteTicketBridge.h"
#include "Agent/Bridges/HCIAgentExecutorSimulateExecuteArchiveBundleBridge.h"
#include "Agent/Bridges/HCIAgentExecutorSimulateExecuteFinalReportBridge.h"
#include "Agent/Bridges/HCIAgentExecutorSimulateExecuteReceiptBridge.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteArchiveBundle.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteArchiveBundleJsonSerializer.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteFinalReport.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteReceipt.h"
#include "Agent/Executor/HCIDryRunDiff.h"
#include "Misc/AutomationTest.h"

namespace
{
static FHCIDryRunDiffReport MakeF14SelectedReviewReport()
{
	FHCIDryRunDiffReport Report;
	Report.RequestId = TEXT("req_f14_review");

	FHCIDryRunDiffItem& Item = Report.DiffItems.AddDefaulted_GetRef();
	Item.AssetPath = TEXT("/Game/Levels/Demo/SM_F14_A.SM_F14_A");
	Item.Field = TEXT("step:s1");
	Item.ToolName = TEXT("ScanLevelMeshRisks");
	Item.Risk = EHCIDryRunRisk::ReadOnly;
	Item.ObjectType = EHCIDryRunObjectType::Actor;
	Item.LocateStrategy = EHCIDryRunLocateStrategy::CameraFocus;
	Item.EvidenceKey = TEXT("actor_path");
	Item.ActorPath = TEXT("/Game/Maps/Demo.PersistentLevel.SM_F14_A_01");

	FHCIDryRunDiff::NormalizeAndFinalize(Report);
	return Report;
}

static FHCIAgentApplyRequest MakeF14ApplyRequest(const FHCIDryRunDiffReport& Review)
{
	FHCIAgentApplyRequest ApplyRequest;
	check(FHCIAgentExecutorApplyRequestBridge::BuildApplyRequest(Review, ApplyRequest));
	return ApplyRequest;
}

static FHCIAgentApplyConfirmRequest MakeF14ConfirmRequest(
	const FHCIAgentApplyRequest& ApplyRequest,
	const FHCIDryRunDiffReport& Review,
	const bool bUserConfirmed)
{
	FHCIAgentApplyConfirmRequest ConfirmRequest;
	check(FHCIAgentExecutorApplyConfirmBridge::BuildConfirmRequest(ApplyRequest, Review, bUserConfirmed, ConfirmRequest));
	return ConfirmRequest;
}

static FHCIAgentExecuteTicket MakeF14ExecuteTicket(
	const FHCIAgentApplyConfirmRequest& ConfirmRequest,
	const FHCIAgentApplyRequest& ApplyRequest,
	const FHCIDryRunDiffReport& Review)
{
	FHCIAgentExecuteTicket ExecuteTicket;
	check(FHCIAgentExecutorExecuteTicketBridge::BuildExecuteTicket(ConfirmRequest, ApplyRequest, Review, ExecuteTicket));
	return ExecuteTicket;
}

static FHCIAgentSimulateExecuteReceipt MakeF14Receipt(
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

static FHCIAgentSimulateExecuteFinalReport MakeF14FinalReport(
	const FHCIAgentSimulateExecuteReceipt& Receipt,
	const FHCIAgentExecuteTicket& ExecuteTicket,
	const FHCIAgentApplyConfirmRequest& ConfirmRequest,
	const FHCIAgentApplyRequest& ApplyRequest,
	const FHCIDryRunDiffReport& Review)
{
	FHCIAgentSimulateExecuteFinalReport FinalReport;
	check(FHCIAgentExecutorSimulateExecuteFinalReportBridge::BuildSimulateExecuteFinalReport(
		Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, FinalReport));
	return FinalReport;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorSimArchiveBundleReadyTest,
	"HCI.Editor.AgentExecutorSimArchiveBundle.ReadyWhenFinalReportCompletedAndStateMatches",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorSimArchiveBundleReadyTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeF14SelectedReviewReport();
	const FHCIAgentApplyRequest ApplyRequest = MakeF14ApplyRequest(Review);
	const FHCIAgentApplyConfirmRequest ConfirmRequest = MakeF14ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAgentExecuteTicket ExecuteTicket = MakeF14ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteReceipt Receipt = MakeF14Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteFinalReport FinalReport = MakeF14FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);

	FHCIAgentSimulateExecuteArchiveBundle Bundle;
	TestTrue(TEXT("Build archive bundle"), FHCIAgentExecutorSimulateExecuteArchiveBundleBridge::BuildSimulateExecuteArchiveBundle(
		FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, Bundle));
	TestTrue(TEXT("Archive ready"), Bundle.bArchiveReady);
	TestEqual(TEXT("ArchiveStatus"), Bundle.ArchiveStatus, FString(TEXT("ready")));
	TestEqual(TEXT("ErrorCode"), Bundle.ErrorCode, FString(TEXT("-")));
	TestEqual(TEXT("Reason"), Bundle.Reason, FString(TEXT("simulate_archive_bundle_ready")));
	TestFalse(TEXT("ArchiveDigest present"), Bundle.ArchiveDigest.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorSimArchiveBundleFinalIncompleteTest,
	"HCI.Editor.AgentExecutorSimArchiveBundle.FinalNotCompletedReturnsE4207",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorSimArchiveBundleFinalIncompleteTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeF14SelectedReviewReport();
	const FHCIAgentApplyRequest ApplyRequest = MakeF14ApplyRequest(Review);
	const FHCIAgentApplyConfirmRequest ConfirmRequest = MakeF14ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAgentExecuteTicket ExecuteTicket = MakeF14ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteReceipt Receipt = MakeF14Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	FHCIAgentSimulateExecuteFinalReport FinalReport = MakeF14FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	FinalReport.bSimulationCompleted = false;
	FinalReport.TerminalStatus = TEXT("blocked");

	FHCIAgentSimulateExecuteArchiveBundle Bundle;
	TestTrue(TEXT("Build archive bundle"), FHCIAgentExecutorSimulateExecuteArchiveBundleBridge::BuildSimulateExecuteArchiveBundle(
		FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, Bundle));
	TestFalse(TEXT("Archive ready"), Bundle.bArchiveReady);
	TestEqual(TEXT("ArchiveStatus"), Bundle.ArchiveStatus, FString(TEXT("blocked")));
	TestEqual(TEXT("ErrorCode"), Bundle.ErrorCode, FString(TEXT("E4207")));
	TestEqual(TEXT("Reason"), Bundle.Reason, FString(TEXT("simulate_final_report_not_completed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorSimArchiveBundleDigestMismatchTest,
	"HCI.Editor.AgentExecutorSimArchiveBundle.DigestMismatchReturnsE4202",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorSimArchiveBundleDigestMismatchTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeF14SelectedReviewReport();
	const FHCIAgentApplyRequest ApplyRequest = MakeF14ApplyRequest(Review);
	const FHCIAgentApplyConfirmRequest ConfirmRequest = MakeF14ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAgentExecuteTicket ExecuteTicket = MakeF14ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteReceipt Receipt = MakeF14Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	FHCIAgentSimulateExecuteFinalReport FinalReport = MakeF14FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	FinalReport.SelectionDigest = TEXT("crc32_BAD0C0DE");

	FHCIAgentSimulateExecuteArchiveBundle Bundle;
	TestTrue(TEXT("Build archive bundle"), FHCIAgentExecutorSimulateExecuteArchiveBundleBridge::BuildSimulateExecuteArchiveBundle(
		FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, Bundle));
	TestFalse(TEXT("Archive ready"), Bundle.bArchiveReady);
	TestEqual(TEXT("ErrorCode"), Bundle.ErrorCode, FString(TEXT("E4202")));
	TestEqual(TEXT("Reason"), Bundle.Reason, FString(TEXT("selection_digest_mismatch")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorSimArchiveBundleJsonFieldsTest,
	"HCI.Editor.AgentExecutorSimArchiveBundle.JsonIncludesArchiveDigestAndStatusFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorSimArchiveBundleJsonFieldsTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeF14SelectedReviewReport();
	const FHCIAgentApplyRequest ApplyRequest = MakeF14ApplyRequest(Review);
	const FHCIAgentApplyConfirmRequest ConfirmRequest = MakeF14ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAgentExecuteTicket ExecuteTicket = MakeF14ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteReceipt Receipt = MakeF14Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteFinalReport FinalReport = MakeF14FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);

	FHCIAgentSimulateExecuteArchiveBundle Bundle;
	TestTrue(TEXT("Build archive bundle"), FHCIAgentExecutorSimulateExecuteArchiveBundleBridge::BuildSimulateExecuteArchiveBundle(
		FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, Bundle));

	FString JsonText;
	TestTrue(TEXT("Serialize archive bundle json"), FHCIAgentSimulateExecuteArchiveBundleJsonSerializer::SerializeToJsonString(Bundle, JsonText));
	TestTrue(TEXT("JSON includes sim_final_report_id"), JsonText.Contains(TEXT("\"sim_final_report_id\"")));
	TestTrue(TEXT("JSON includes archive_digest"), JsonText.Contains(TEXT("\"archive_digest\"")));
	TestTrue(TEXT("JSON includes archive_status"), JsonText.Contains(TEXT("\"archive_status\"")));
	TestTrue(TEXT("JSON includes archive_ready"), JsonText.Contains(TEXT("\"archive_ready\"")));
	TestTrue(TEXT("JSON includes terminal_status"), JsonText.Contains(TEXT("\"terminal_status\"")));
	TestTrue(TEXT("JSON includes locate_strategy"), JsonText.Contains(TEXT("\"locate_strategy\"")));
	return true;
}

#endif
