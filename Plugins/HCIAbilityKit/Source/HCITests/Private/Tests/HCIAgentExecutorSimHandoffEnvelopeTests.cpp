#if WITH_DEV_AUTOMATION_TESTS

#include "Agent/Contracts/StageF/HCIAgentApplyConfirmRequest.h"
#include "Agent/Contracts/StageF/HCIAgentApplyRequest.h"
#include "Agent/Contracts/StageF/HCIAgentExecuteTicket.h"
#include "Agent/Bridges/HCIAgentExecutorApplyConfirmBridge.h"
#include "Agent/Bridges/HCIAgentExecutorApplyRequestBridge.h"
#include "Agent/Bridges/HCIAgentExecutorExecuteTicketBridge.h"
#include "Agent/Bridges/HCIAgentExecutorSimulateExecuteArchiveBundleBridge.h"
#include "Agent/Bridges/HCIAgentExecutorSimulateExecuteFinalReportBridge.h"
#include "Agent/Bridges/HCIAgentExecutorSimulateExecuteHandoffEnvelopeBridge.h"
#include "Agent/Bridges/HCIAgentExecutorSimulateExecuteReceiptBridge.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteArchiveBundle.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteFinalReport.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteHandoffEnvelope.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteHandoffEnvelopeJsonSerializer.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteReceipt.h"
#include "Agent/Executor/HCIDryRunDiff.h"
#include "Misc/AutomationTest.h"

namespace
{
static FHCIDryRunDiffReport MakeF15SelectedReviewReport()
{
	FHCIDryRunDiffReport Report;
	Report.RequestId = TEXT("req_f15_review");

	FHCIDryRunDiffItem& Item = Report.DiffItems.AddDefaulted_GetRef();
	Item.AssetPath = TEXT("/Game/Levels/Demo/SM_F15_A.SM_F15_A");
	Item.Field = TEXT("step:s1");
	Item.ToolName = TEXT("ScanLevelMeshRisks");
	Item.Risk = EHCIDryRunRisk::ReadOnly;
	Item.ObjectType = EHCIDryRunObjectType::Actor;
	Item.LocateStrategy = EHCIDryRunLocateStrategy::CameraFocus;
	Item.EvidenceKey = TEXT("actor_path");
	Item.ActorPath = TEXT("/Game/Maps/Demo.PersistentLevel.SM_F15_A_01");

	FHCIDryRunDiff::NormalizeAndFinalize(Report);
	return Report;
}

static FHCIAgentApplyRequest MakeF15ApplyRequest(const FHCIDryRunDiffReport& Review)
{
	FHCIAgentApplyRequest ApplyRequest;
	check(FHCIAgentExecutorApplyRequestBridge::BuildApplyRequest(Review, ApplyRequest));
	return ApplyRequest;
}

static FHCIAgentApplyConfirmRequest MakeF15ConfirmRequest(const FHCIAgentApplyRequest& ApplyRequest, const FHCIDryRunDiffReport& Review, bool bUserConfirmed)
{
	FHCIAgentApplyConfirmRequest ConfirmRequest;
	check(FHCIAgentExecutorApplyConfirmBridge::BuildConfirmRequest(ApplyRequest, Review, bUserConfirmed, ConfirmRequest));
	return ConfirmRequest;
}

static FHCIAgentExecuteTicket MakeF15ExecuteTicket(const FHCIAgentApplyConfirmRequest& ConfirmRequest, const FHCIAgentApplyRequest& ApplyRequest, const FHCIDryRunDiffReport& Review)
{
	FHCIAgentExecuteTicket ExecuteTicket;
	check(FHCIAgentExecutorExecuteTicketBridge::BuildExecuteTicket(ConfirmRequest, ApplyRequest, Review, ExecuteTicket));
	return ExecuteTicket;
}

static FHCIAgentSimulateExecuteReceipt MakeF15Receipt(const FHCIAgentExecuteTicket& ExecuteTicket, const FHCIAgentApplyConfirmRequest& ConfirmRequest, const FHCIAgentApplyRequest& ApplyRequest, const FHCIDryRunDiffReport& Review)
{
	FHCIAgentSimulateExecuteReceipt Receipt;
	check(FHCIAgentExecutorSimulateExecuteReceiptBridge::BuildSimulateExecuteReceipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review, Receipt));
	return Receipt;
}

static FHCIAgentSimulateExecuteFinalReport MakeF15FinalReport(const FHCIAgentSimulateExecuteReceipt& Receipt, const FHCIAgentExecuteTicket& ExecuteTicket, const FHCIAgentApplyConfirmRequest& ConfirmRequest, const FHCIAgentApplyRequest& ApplyRequest, const FHCIDryRunDiffReport& Review)
{
	FHCIAgentSimulateExecuteFinalReport FinalReport;
	check(FHCIAgentExecutorSimulateExecuteFinalReportBridge::BuildSimulateExecuteFinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, FinalReport));
	return FinalReport;
}

static FHCIAgentSimulateExecuteArchiveBundle MakeF15ArchiveBundle(const FHCIAgentSimulateExecuteFinalReport& FinalReport, const FHCIAgentSimulateExecuteReceipt& Receipt, const FHCIAgentExecuteTicket& ExecuteTicket, const FHCIAgentApplyConfirmRequest& ConfirmRequest, const FHCIAgentApplyRequest& ApplyRequest, const FHCIDryRunDiffReport& Review)
{
	FHCIAgentSimulateExecuteArchiveBundle ArchiveBundle;
	check(FHCIAgentExecutorSimulateExecuteArchiveBundleBridge::BuildSimulateExecuteArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, ArchiveBundle));
	return ArchiveBundle;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorSimHandoffEnvelopeReadyTest,
	"HCI.Editor.AgentExecutorSimHandoffEnvelope.ReadyWhenArchiveBundleCompletedAndStateMatches",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorSimHandoffEnvelopeReadyTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeF15SelectedReviewReport();
	const FHCIAgentApplyRequest ApplyRequest = MakeF15ApplyRequest(Review);
	const FHCIAgentApplyConfirmRequest ConfirmRequest = MakeF15ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAgentExecuteTicket ExecuteTicket = MakeF15ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteReceipt Receipt = MakeF15Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteFinalReport FinalReport = MakeF15FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeF15ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);

	FHCIAgentSimulateExecuteHandoffEnvelope Envelope;
	TestTrue(TEXT("Build handoff envelope"), FHCIAgentExecutorSimulateExecuteHandoffEnvelopeBridge::BuildSimulateExecuteHandoffEnvelope(
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
	FHCIAgentExecutorSimHandoffEnvelopeArchiveNotReadyTest,
	"HCI.Editor.AgentExecutorSimHandoffEnvelope.ArchiveNotReadyReturnsE4208",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorSimHandoffEnvelopeArchiveNotReadyTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeF15SelectedReviewReport();
	const FHCIAgentApplyRequest ApplyRequest = MakeF15ApplyRequest(Review);
	const FHCIAgentApplyConfirmRequest ConfirmRequest = MakeF15ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAgentExecuteTicket ExecuteTicket = MakeF15ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteReceipt Receipt = MakeF15Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteFinalReport FinalReport = MakeF15FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	FHCIAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeF15ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	ArchiveBundle.bArchiveReady = false;
	ArchiveBundle.ArchiveStatus = TEXT("blocked");

	FHCIAgentSimulateExecuteHandoffEnvelope Envelope;
	TestTrue(TEXT("Build handoff envelope"), FHCIAgentExecutorSimulateExecuteHandoffEnvelopeBridge::BuildSimulateExecuteHandoffEnvelope(
		ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, Envelope));
	TestFalse(TEXT("Handoff ready"), Envelope.bHandoffReady);
	TestEqual(TEXT("HandoffStatus"), Envelope.HandoffStatus, FString(TEXT("blocked")));
	TestEqual(TEXT("ErrorCode"), Envelope.ErrorCode, FString(TEXT("E4208")));
	TestEqual(TEXT("Reason"), Envelope.Reason, FString(TEXT("sim_archive_bundle_not_ready")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorSimHandoffEnvelopeDigestMismatchTest,
	"HCI.Editor.AgentExecutorSimHandoffEnvelope.DigestMismatchReturnsE4202",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorSimHandoffEnvelopeDigestMismatchTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeF15SelectedReviewReport();
	const FHCIAgentApplyRequest ApplyRequest = MakeF15ApplyRequest(Review);
	const FHCIAgentApplyConfirmRequest ConfirmRequest = MakeF15ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAgentExecuteTicket ExecuteTicket = MakeF15ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteReceipt Receipt = MakeF15Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteFinalReport FinalReport = MakeF15FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	FHCIAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeF15ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	ArchiveBundle.SelectionDigest = TEXT("crc32_BAD0C0DE");

	FHCIAgentSimulateExecuteHandoffEnvelope Envelope;
	TestTrue(TEXT("Build handoff envelope"), FHCIAgentExecutorSimulateExecuteHandoffEnvelopeBridge::BuildSimulateExecuteHandoffEnvelope(
		ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, Envelope));
	TestFalse(TEXT("Handoff ready"), Envelope.bHandoffReady);
	TestEqual(TEXT("ErrorCode"), Envelope.ErrorCode, FString(TEXT("E4202")));
	TestEqual(TEXT("Reason"), Envelope.Reason, FString(TEXT("selection_digest_mismatch")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorSimHandoffEnvelopeJsonFieldsTest,
	"HCI.Editor.AgentExecutorSimHandoffEnvelope.JsonIncludesArchiveAndHandoffFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorSimHandoffEnvelopeJsonFieldsTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeF15SelectedReviewReport();
	const FHCIAgentApplyRequest ApplyRequest = MakeF15ApplyRequest(Review);
	const FHCIAgentApplyConfirmRequest ConfirmRequest = MakeF15ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAgentExecuteTicket ExecuteTicket = MakeF15ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteReceipt Receipt = MakeF15Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteFinalReport FinalReport = MakeF15FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeF15ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);

	FHCIAgentSimulateExecuteHandoffEnvelope Envelope;
	TestTrue(TEXT("Build handoff envelope"), FHCIAgentExecutorSimulateExecuteHandoffEnvelopeBridge::BuildSimulateExecuteHandoffEnvelope(
		ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, Envelope));

	FString JsonText;
	TestTrue(TEXT("Serialize handoff envelope json"), FHCIAgentSimulateExecuteHandoffEnvelopeJsonSerializer::SerializeToJsonString(Envelope, JsonText));
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
