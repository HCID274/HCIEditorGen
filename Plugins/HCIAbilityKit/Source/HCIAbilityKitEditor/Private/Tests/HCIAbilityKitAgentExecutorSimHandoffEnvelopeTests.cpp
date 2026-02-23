#if WITH_DEV_AUTOMATION_TESTS

#include "Agent/HCIAbilityKitAgentApplyConfirmRequest.h"
#include "Agent/HCIAbilityKitAgentApplyRequest.h"
#include "Agent/HCIAbilityKitAgentExecuteTicket.h"
#include "Agent/HCIAbilityKitAgentExecutorApplyConfirmBridge.h"
#include "Agent/HCIAbilityKitAgentExecutorApplyRequestBridge.h"
#include "Agent/HCIAbilityKitAgentExecutorExecuteTicketBridge.h"
#include "Agent/HCIAbilityKitAgentExecutorSimulateExecuteArchiveBundleBridge.h"
#include "Agent/HCIAbilityKitAgentExecutorSimulateExecuteFinalReportBridge.h"
#include "Agent/HCIAbilityKitAgentExecutorSimulateExecuteHandoffEnvelopeBridge.h"
#include "Agent/HCIAbilityKitAgentExecutorSimulateExecuteReceiptBridge.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteArchiveBundle.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteFinalReport.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteHandoffEnvelope.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteHandoffEnvelopeJsonSerializer.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteReceipt.h"
#include "Agent/HCIAbilityKitDryRunDiff.h"
#include "Misc/AutomationTest.h"

namespace
{
static FHCIAbilityKitDryRunDiffReport MakeF15SelectedReviewReport()
{
	FHCIAbilityKitDryRunDiffReport Report;
	Report.RequestId = TEXT("req_f15_review");

	FHCIAbilityKitDryRunDiffItem& Item = Report.DiffItems.AddDefaulted_GetRef();
	Item.AssetPath = TEXT("/Game/Levels/Demo/SM_F15_A.SM_F15_A");
	Item.Field = TEXT("step:s1");
	Item.ToolName = TEXT("ScanLevelMeshRisks");
	Item.Risk = EHCIAbilityKitDryRunRisk::ReadOnly;
	Item.ObjectType = EHCIAbilityKitDryRunObjectType::Actor;
	Item.LocateStrategy = EHCIAbilityKitDryRunLocateStrategy::CameraFocus;
	Item.EvidenceKey = TEXT("actor_path");
	Item.ActorPath = TEXT("/Game/Maps/Demo.PersistentLevel.SM_F15_A_01");

	FHCIAbilityKitDryRunDiff::NormalizeAndFinalize(Report);
	return Report;
}

static FHCIAbilityKitAgentApplyRequest MakeF15ApplyRequest(const FHCIAbilityKitDryRunDiffReport& Review)
{
	FHCIAbilityKitAgentApplyRequest ApplyRequest;
	check(FHCIAbilityKitAgentExecutorApplyRequestBridge::BuildApplyRequest(Review, ApplyRequest));
	return ApplyRequest;
}

static FHCIAbilityKitAgentApplyConfirmRequest MakeF15ConfirmRequest(const FHCIAbilityKitAgentApplyRequest& ApplyRequest, const FHCIAbilityKitDryRunDiffReport& Review, bool bUserConfirmed)
{
	FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest;
	check(FHCIAbilityKitAgentExecutorApplyConfirmBridge::BuildConfirmRequest(ApplyRequest, Review, bUserConfirmed, ConfirmRequest));
	return ConfirmRequest;
}

static FHCIAbilityKitAgentExecuteTicket MakeF15ExecuteTicket(const FHCIAbilityKitAgentApplyConfirmRequest& ConfirmRequest, const FHCIAbilityKitAgentApplyRequest& ApplyRequest, const FHCIAbilityKitDryRunDiffReport& Review)
{
	FHCIAbilityKitAgentExecuteTicket ExecuteTicket;
	check(FHCIAbilityKitAgentExecutorExecuteTicketBridge::BuildExecuteTicket(ConfirmRequest, ApplyRequest, Review, ExecuteTicket));
	return ExecuteTicket;
}

static FHCIAbilityKitAgentSimulateExecuteReceipt MakeF15Receipt(const FHCIAbilityKitAgentExecuteTicket& ExecuteTicket, const FHCIAbilityKitAgentApplyConfirmRequest& ConfirmRequest, const FHCIAbilityKitAgentApplyRequest& ApplyRequest, const FHCIAbilityKitDryRunDiffReport& Review)
{
	FHCIAbilityKitAgentSimulateExecuteReceipt Receipt;
	check(FHCIAbilityKitAgentExecutorSimulateExecuteReceiptBridge::BuildSimulateExecuteReceipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review, Receipt));
	return Receipt;
}

static FHCIAbilityKitAgentSimulateExecuteFinalReport MakeF15FinalReport(const FHCIAbilityKitAgentSimulateExecuteReceipt& Receipt, const FHCIAbilityKitAgentExecuteTicket& ExecuteTicket, const FHCIAbilityKitAgentApplyConfirmRequest& ConfirmRequest, const FHCIAbilityKitAgentApplyRequest& ApplyRequest, const FHCIAbilityKitDryRunDiffReport& Review)
{
	FHCIAbilityKitAgentSimulateExecuteFinalReport FinalReport;
	check(FHCIAbilityKitAgentExecutorSimulateExecuteFinalReportBridge::BuildSimulateExecuteFinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, FinalReport));
	return FinalReport;
}

static FHCIAbilityKitAgentSimulateExecuteArchiveBundle MakeF15ArchiveBundle(const FHCIAbilityKitAgentSimulateExecuteFinalReport& FinalReport, const FHCIAbilityKitAgentSimulateExecuteReceipt& Receipt, const FHCIAbilityKitAgentExecuteTicket& ExecuteTicket, const FHCIAbilityKitAgentApplyConfirmRequest& ConfirmRequest, const FHCIAbilityKitAgentApplyRequest& ApplyRequest, const FHCIAbilityKitDryRunDiffReport& Review)
{
	FHCIAbilityKitAgentSimulateExecuteArchiveBundle ArchiveBundle;
	check(FHCIAbilityKitAgentExecutorSimulateExecuteArchiveBundleBridge::BuildSimulateExecuteArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, ArchiveBundle));
	return ArchiveBundle;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorSimHandoffEnvelopeReadyTest,
	"HCIAbilityKit.Editor.AgentExecutorSimHandoffEnvelope.ReadyWhenArchiveBundleCompletedAndStateMatches",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorSimHandoffEnvelopeReadyTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeF15SelectedReviewReport();
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeF15ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeF15ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAbilityKitAgentExecuteTicket ExecuteTicket = MakeF15ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteReceipt Receipt = MakeF15Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteFinalReport FinalReport = MakeF15FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeF15ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);

	FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope Envelope;
	TestTrue(TEXT("Build handoff envelope"), FHCIAbilityKitAgentExecutorSimulateExecuteHandoffEnvelopeBridge::BuildSimulateExecuteHandoffEnvelope(
		ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, Envelope));
	TestTrue(TEXT("Handoff ready"), Envelope.bHandoffReady);
	TestEqual(TEXT("HandoffStatus"), Envelope.HandoffStatus, FString(TEXT("ready")));
	TestEqual(TEXT("ErrorCode"), Envelope.ErrorCode, FString(TEXT("-")));
	TestEqual(TEXT("Reason"), Envelope.Reason, FString(TEXT("simulate_handoff_envelope_ready")));
	TestEqual(TEXT("HandoffTarget"), Envelope.HandoffTarget, FString(TEXT("stage_g_execute")));
	TestFalse(TEXT("HandoffDigest present"), Envelope.HandoffDigest.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorSimHandoffEnvelopeArchiveNotReadyTest,
	"HCIAbilityKit.Editor.AgentExecutorSimHandoffEnvelope.ArchiveNotReadyReturnsE4208",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorSimHandoffEnvelopeArchiveNotReadyTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeF15SelectedReviewReport();
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeF15ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeF15ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAbilityKitAgentExecuteTicket ExecuteTicket = MakeF15ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteReceipt Receipt = MakeF15Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteFinalReport FinalReport = MakeF15FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	FHCIAbilityKitAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeF15ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	ArchiveBundle.bArchiveReady = false;
	ArchiveBundle.ArchiveStatus = TEXT("blocked");

	FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope Envelope;
	TestTrue(TEXT("Build handoff envelope"), FHCIAbilityKitAgentExecutorSimulateExecuteHandoffEnvelopeBridge::BuildSimulateExecuteHandoffEnvelope(
		ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, Envelope));
	TestFalse(TEXT("Handoff ready"), Envelope.bHandoffReady);
	TestEqual(TEXT("HandoffStatus"), Envelope.HandoffStatus, FString(TEXT("blocked")));
	TestEqual(TEXT("ErrorCode"), Envelope.ErrorCode, FString(TEXT("E4208")));
	TestEqual(TEXT("Reason"), Envelope.Reason, FString(TEXT("sim_archive_bundle_not_ready")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorSimHandoffEnvelopeDigestMismatchTest,
	"HCIAbilityKit.Editor.AgentExecutorSimHandoffEnvelope.DigestMismatchReturnsE4202",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorSimHandoffEnvelopeDigestMismatchTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeF15SelectedReviewReport();
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeF15ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeF15ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAbilityKitAgentExecuteTicket ExecuteTicket = MakeF15ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteReceipt Receipt = MakeF15Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteFinalReport FinalReport = MakeF15FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	FHCIAbilityKitAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeF15ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	ArchiveBundle.SelectionDigest = TEXT("crc32_BAD0C0DE");

	FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope Envelope;
	TestTrue(TEXT("Build handoff envelope"), FHCIAbilityKitAgentExecutorSimulateExecuteHandoffEnvelopeBridge::BuildSimulateExecuteHandoffEnvelope(
		ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, Envelope));
	TestFalse(TEXT("Handoff ready"), Envelope.bHandoffReady);
	TestEqual(TEXT("ErrorCode"), Envelope.ErrorCode, FString(TEXT("E4202")));
	TestEqual(TEXT("Reason"), Envelope.Reason, FString(TEXT("selection_digest_mismatch")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorSimHandoffEnvelopeJsonFieldsTest,
	"HCIAbilityKit.Editor.AgentExecutorSimHandoffEnvelope.JsonIncludesArchiveAndHandoffFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorSimHandoffEnvelopeJsonFieldsTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeF15SelectedReviewReport();
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeF15ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeF15ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAbilityKitAgentExecuteTicket ExecuteTicket = MakeF15ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteReceipt Receipt = MakeF15Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteFinalReport FinalReport = MakeF15FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeF15ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);

	FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope Envelope;
	TestTrue(TEXT("Build handoff envelope"), FHCIAbilityKitAgentExecutorSimulateExecuteHandoffEnvelopeBridge::BuildSimulateExecuteHandoffEnvelope(
		ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, Envelope));

	FString JsonText;
	TestTrue(TEXT("Serialize handoff envelope json"), FHCIAbilityKitAgentSimulateExecuteHandoffEnvelopeJsonSerializer::SerializeToJsonString(Envelope, JsonText));
	TestTrue(TEXT("JSON includes sim_archive_bundle_id"), JsonText.Contains(TEXT("\"sim_archive_bundle_id\"")));
	TestTrue(TEXT("JSON includes archive_digest"), JsonText.Contains(TEXT("\"archive_digest\"")));
	TestTrue(TEXT("JSON includes handoff_digest"), JsonText.Contains(TEXT("\"handoff_digest\"")));
	TestTrue(TEXT("JSON includes archive_status"), JsonText.Contains(TEXT("\"archive_status\"")));
	TestTrue(TEXT("JSON includes handoff_status"), JsonText.Contains(TEXT("\"handoff_status\"")));
	TestTrue(TEXT("JSON includes handoff_ready"), JsonText.Contains(TEXT("\"handoff_ready\"")));
	TestTrue(TEXT("JSON includes handoff_target"), JsonText.Contains(TEXT("\"handoff_target\"")));
	TestTrue(TEXT("JSON includes locate_strategy"), JsonText.Contains(TEXT("\"locate_strategy\"")));
	return true;
}

#endif
