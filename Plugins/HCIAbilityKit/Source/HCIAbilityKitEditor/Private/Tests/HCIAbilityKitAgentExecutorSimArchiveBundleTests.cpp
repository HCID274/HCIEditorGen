#if WITH_DEV_AUTOMATION_TESTS

#include "Agent/HCIAbilityKitAgentApplyConfirmRequest.h"
#include "Agent/HCIAbilityKitAgentApplyRequest.h"
#include "Agent/HCIAbilityKitAgentExecuteTicket.h"
#include "Agent/HCIAbilityKitAgentExecutorApplyConfirmBridge.h"
#include "Agent/HCIAbilityKitAgentExecutorApplyRequestBridge.h"
#include "Agent/HCIAbilityKitAgentExecutorExecuteTicketBridge.h"
#include "Agent/HCIAbilityKitAgentExecutorSimulateExecuteArchiveBundleBridge.h"
#include "Agent/HCIAbilityKitAgentExecutorSimulateExecuteFinalReportBridge.h"
#include "Agent/HCIAbilityKitAgentExecutorSimulateExecuteReceiptBridge.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteArchiveBundle.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteArchiveBundleJsonSerializer.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteFinalReport.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteReceipt.h"
#include "Agent/HCIAbilityKitDryRunDiff.h"
#include "Misc/AutomationTest.h"

namespace
{
static FHCIAbilityKitDryRunDiffReport MakeF14SelectedReviewReport()
{
	FHCIAbilityKitDryRunDiffReport Report;
	Report.RequestId = TEXT("req_f14_review");

	FHCIAbilityKitDryRunDiffItem& Item = Report.DiffItems.AddDefaulted_GetRef();
	Item.AssetPath = TEXT("/Game/Levels/Demo/SM_F14_A.SM_F14_A");
	Item.Field = TEXT("step:s1");
	Item.ToolName = TEXT("ScanLevelMeshRisks");
	Item.Risk = EHCIAbilityKitDryRunRisk::ReadOnly;
	Item.ObjectType = EHCIAbilityKitDryRunObjectType::Actor;
	Item.LocateStrategy = EHCIAbilityKitDryRunLocateStrategy::CameraFocus;
	Item.EvidenceKey = TEXT("actor_path");
	Item.ActorPath = TEXT("/Game/Maps/Demo.PersistentLevel.SM_F14_A_01");

	FHCIAbilityKitDryRunDiff::NormalizeAndFinalize(Report);
	return Report;
}

static FHCIAbilityKitAgentApplyRequest MakeF14ApplyRequest(const FHCIAbilityKitDryRunDiffReport& Review)
{
	FHCIAbilityKitAgentApplyRequest ApplyRequest;
	check(FHCIAbilityKitAgentExecutorApplyRequestBridge::BuildApplyRequest(Review, ApplyRequest));
	return ApplyRequest;
}

static FHCIAbilityKitAgentApplyConfirmRequest MakeF14ConfirmRequest(
	const FHCIAbilityKitAgentApplyRequest& ApplyRequest,
	const FHCIAbilityKitDryRunDiffReport& Review,
	const bool bUserConfirmed)
{
	FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest;
	check(FHCIAbilityKitAgentExecutorApplyConfirmBridge::BuildConfirmRequest(ApplyRequest, Review, bUserConfirmed, ConfirmRequest));
	return ConfirmRequest;
}

static FHCIAbilityKitAgentExecuteTicket MakeF14ExecuteTicket(
	const FHCIAbilityKitAgentApplyConfirmRequest& ConfirmRequest,
	const FHCIAbilityKitAgentApplyRequest& ApplyRequest,
	const FHCIAbilityKitDryRunDiffReport& Review)
{
	FHCIAbilityKitAgentExecuteTicket ExecuteTicket;
	check(FHCIAbilityKitAgentExecutorExecuteTicketBridge::BuildExecuteTicket(ConfirmRequest, ApplyRequest, Review, ExecuteTicket));
	return ExecuteTicket;
}

static FHCIAbilityKitAgentSimulateExecuteReceipt MakeF14Receipt(
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

static FHCIAbilityKitAgentSimulateExecuteFinalReport MakeF14FinalReport(
	const FHCIAbilityKitAgentSimulateExecuteReceipt& Receipt,
	const FHCIAbilityKitAgentExecuteTicket& ExecuteTicket,
	const FHCIAbilityKitAgentApplyConfirmRequest& ConfirmRequest,
	const FHCIAbilityKitAgentApplyRequest& ApplyRequest,
	const FHCIAbilityKitDryRunDiffReport& Review)
{
	FHCIAbilityKitAgentSimulateExecuteFinalReport FinalReport;
	check(FHCIAbilityKitAgentExecutorSimulateExecuteFinalReportBridge::BuildSimulateExecuteFinalReport(
		Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, FinalReport));
	return FinalReport;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorSimArchiveBundleReadyTest,
	"HCIAbilityKit.Editor.AgentExecutorSimArchiveBundle.ReadyWhenFinalReportCompletedAndStateMatches",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorSimArchiveBundleReadyTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeF14SelectedReviewReport();
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeF14ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeF14ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAbilityKitAgentExecuteTicket ExecuteTicket = MakeF14ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteReceipt Receipt = MakeF14Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteFinalReport FinalReport = MakeF14FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);

	FHCIAbilityKitAgentSimulateExecuteArchiveBundle Bundle;
	TestTrue(TEXT("Build archive bundle"), FHCIAbilityKitAgentExecutorSimulateExecuteArchiveBundleBridge::BuildSimulateExecuteArchiveBundle(
		FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, Bundle));
	TestTrue(TEXT("Archive ready"), Bundle.bArchiveReady);
	TestEqual(TEXT("ArchiveStatus"), Bundle.ArchiveStatus, FString(TEXT("ready")));
	TestEqual(TEXT("ErrorCode"), Bundle.ErrorCode, FString(TEXT("-")));
	TestEqual(TEXT("Reason"), Bundle.Reason, FString(TEXT("simulate_archive_bundle_ready")));
	TestFalse(TEXT("ArchiveDigest present"), Bundle.ArchiveDigest.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorSimArchiveBundleFinalIncompleteTest,
	"HCIAbilityKit.Editor.AgentExecutorSimArchiveBundle.FinalNotCompletedReturnsE4207",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorSimArchiveBundleFinalIncompleteTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeF14SelectedReviewReport();
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeF14ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeF14ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAbilityKitAgentExecuteTicket ExecuteTicket = MakeF14ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteReceipt Receipt = MakeF14Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	FHCIAbilityKitAgentSimulateExecuteFinalReport FinalReport = MakeF14FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	FinalReport.bSimulationCompleted = false;
	FinalReport.TerminalStatus = TEXT("blocked");

	FHCIAbilityKitAgentSimulateExecuteArchiveBundle Bundle;
	TestTrue(TEXT("Build archive bundle"), FHCIAbilityKitAgentExecutorSimulateExecuteArchiveBundleBridge::BuildSimulateExecuteArchiveBundle(
		FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, Bundle));
	TestFalse(TEXT("Archive ready"), Bundle.bArchiveReady);
	TestEqual(TEXT("ArchiveStatus"), Bundle.ArchiveStatus, FString(TEXT("blocked")));
	TestEqual(TEXT("ErrorCode"), Bundle.ErrorCode, FString(TEXT("E4207")));
	TestEqual(TEXT("Reason"), Bundle.Reason, FString(TEXT("simulate_final_report_not_completed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorSimArchiveBundleDigestMismatchTest,
	"HCIAbilityKit.Editor.AgentExecutorSimArchiveBundle.DigestMismatchReturnsE4202",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorSimArchiveBundleDigestMismatchTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeF14SelectedReviewReport();
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeF14ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeF14ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAbilityKitAgentExecuteTicket ExecuteTicket = MakeF14ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteReceipt Receipt = MakeF14Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	FHCIAbilityKitAgentSimulateExecuteFinalReport FinalReport = MakeF14FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	FinalReport.SelectionDigest = TEXT("crc32_BAD0C0DE");

	FHCIAbilityKitAgentSimulateExecuteArchiveBundle Bundle;
	TestTrue(TEXT("Build archive bundle"), FHCIAbilityKitAgentExecutorSimulateExecuteArchiveBundleBridge::BuildSimulateExecuteArchiveBundle(
		FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, Bundle));
	TestFalse(TEXT("Archive ready"), Bundle.bArchiveReady);
	TestEqual(TEXT("ErrorCode"), Bundle.ErrorCode, FString(TEXT("E4202")));
	TestEqual(TEXT("Reason"), Bundle.Reason, FString(TEXT("selection_digest_mismatch")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorSimArchiveBundleJsonFieldsTest,
	"HCIAbilityKit.Editor.AgentExecutorSimArchiveBundle.JsonIncludesArchiveDigestAndStatusFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorSimArchiveBundleJsonFieldsTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeF14SelectedReviewReport();
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeF14ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeF14ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAbilityKitAgentExecuteTicket ExecuteTicket = MakeF14ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteReceipt Receipt = MakeF14Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteFinalReport FinalReport = MakeF14FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);

	FHCIAbilityKitAgentSimulateExecuteArchiveBundle Bundle;
	TestTrue(TEXT("Build archive bundle"), FHCIAbilityKitAgentExecutorSimulateExecuteArchiveBundleBridge::BuildSimulateExecuteArchiveBundle(
		FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, Bundle));

	FString JsonText;
	TestTrue(TEXT("Serialize archive bundle json"), FHCIAbilityKitAgentSimulateExecuteArchiveBundleJsonSerializer::SerializeToJsonString(Bundle, JsonText));
	TestTrue(TEXT("JSON includes sim_final_report_id"), JsonText.Contains(TEXT("\"sim_final_report_id\"")));
	TestTrue(TEXT("JSON includes archive_digest"), JsonText.Contains(TEXT("\"archive_digest\"")));
	TestTrue(TEXT("JSON includes archive_status"), JsonText.Contains(TEXT("\"archive_status\"")));
	TestTrue(TEXT("JSON includes archive_ready"), JsonText.Contains(TEXT("\"archive_ready\"")));
	TestTrue(TEXT("JSON includes terminal_status"), JsonText.Contains(TEXT("\"terminal_status\"")));
	TestTrue(TEXT("JSON includes locate_strategy"), JsonText.Contains(TEXT("\"locate_strategy\"")));
	return true;
}

#endif
