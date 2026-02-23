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
#include "Agent/HCIAbilityKitAgentExecutorStageGExecuteIntentBridge.h"
#include "Agent/HCIAbilityKitAgentExecutorStageGWriteEnableRequestBridge.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteArchiveBundle.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteFinalReport.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteHandoffEnvelope.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteReceipt.h"
#include "Agent/HCIAbilityKitAgentStageGExecuteIntent.h"
#include "Agent/HCIAbilityKitAgentStageGWriteEnableRequest.h"
#include "Agent/HCIAbilityKitAgentStageGWriteEnableRequestJsonSerializer.h"
#include "Agent/HCIAbilityKitDryRunDiff.h"
#include "Misc/AutomationTest.h"

namespace
{
static FHCIAbilityKitDryRunDiffReport MakeG2SelectedReviewReport()
{
	FHCIAbilityKitDryRunDiffReport Report;
	Report.RequestId = TEXT("req_g2_review");

	FHCIAbilityKitDryRunDiffItem& Item = Report.DiffItems.AddDefaulted_GetRef();
	Item.AssetPath = TEXT("/Game/Levels/Demo/SM_G2_A.SM_G2_A");
	Item.Field = TEXT("step:s1");
	Item.ToolName = TEXT("ScanLevelMeshRisks");
	Item.Risk = EHCIAbilityKitDryRunRisk::ReadOnly;
	Item.ObjectType = EHCIAbilityKitDryRunObjectType::Actor;
	Item.LocateStrategy = EHCIAbilityKitDryRunLocateStrategy::CameraFocus;
	Item.EvidenceKey = TEXT("actor_path");
	Item.ActorPath = TEXT("/Game/Maps/Demo.PersistentLevel.SM_G2_A_01");

	FHCIAbilityKitDryRunDiff::NormalizeAndFinalize(Report);
	return Report;
}

static FHCIAbilityKitAgentApplyRequest MakeG2ApplyRequest(const FHCIAbilityKitDryRunDiffReport& Review)
{
	FHCIAbilityKitAgentApplyRequest ApplyRequest;
	check(FHCIAbilityKitAgentExecutorApplyRequestBridge::BuildApplyRequest(Review, ApplyRequest));
	return ApplyRequest;
}

static FHCIAbilityKitAgentApplyConfirmRequest MakeG2ConfirmRequest(
	const FHCIAbilityKitAgentApplyRequest& ApplyRequest,
	const FHCIAbilityKitDryRunDiffReport& Review,
	const bool bUserConfirmed)
{
	FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest;
	check(FHCIAbilityKitAgentExecutorApplyConfirmBridge::BuildConfirmRequest(ApplyRequest, Review, bUserConfirmed, ConfirmRequest));
	return ConfirmRequest;
}

static FHCIAbilityKitAgentExecuteTicket MakeG2ExecuteTicket(
	const FHCIAbilityKitAgentApplyConfirmRequest& ConfirmRequest,
	const FHCIAbilityKitAgentApplyRequest& ApplyRequest,
	const FHCIAbilityKitDryRunDiffReport& Review)
{
	FHCIAbilityKitAgentExecuteTicket ExecuteTicket;
	check(FHCIAbilityKitAgentExecutorExecuteTicketBridge::BuildExecuteTicket(ConfirmRequest, ApplyRequest, Review, ExecuteTicket));
	return ExecuteTicket;
}

static FHCIAbilityKitAgentSimulateExecuteReceipt MakeG2Receipt(
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

static FHCIAbilityKitAgentSimulateExecuteFinalReport MakeG2FinalReport(
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

static FHCIAbilityKitAgentSimulateExecuteArchiveBundle MakeG2ArchiveBundle(
	const FHCIAbilityKitAgentSimulateExecuteFinalReport& FinalReport,
	const FHCIAbilityKitAgentSimulateExecuteReceipt& Receipt,
	const FHCIAbilityKitAgentExecuteTicket& ExecuteTicket,
	const FHCIAbilityKitAgentApplyConfirmRequest& ConfirmRequest,
	const FHCIAbilityKitAgentApplyRequest& ApplyRequest,
	const FHCIAbilityKitDryRunDiffReport& Review)
{
	FHCIAbilityKitAgentSimulateExecuteArchiveBundle ArchiveBundle;
	check(FHCIAbilityKitAgentExecutorSimulateExecuteArchiveBundleBridge::BuildSimulateExecuteArchiveBundle(
		FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, ArchiveBundle));
	return ArchiveBundle;
}

static FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope MakeG2HandoffEnvelope(
	const FHCIAbilityKitAgentSimulateExecuteArchiveBundle& ArchiveBundle,
	const FHCIAbilityKitAgentSimulateExecuteFinalReport& FinalReport,
	const FHCIAbilityKitAgentSimulateExecuteReceipt& Receipt,
	const FHCIAbilityKitAgentExecuteTicket& ExecuteTicket,
	const FHCIAbilityKitAgentApplyConfirmRequest& ConfirmRequest,
	const FHCIAbilityKitAgentApplyRequest& ApplyRequest,
	const FHCIAbilityKitDryRunDiffReport& Review)
{
	FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope HandoffEnvelope;
	check(FHCIAbilityKitAgentExecutorSimulateExecuteHandoffEnvelopeBridge::BuildSimulateExecuteHandoffEnvelope(
		ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, HandoffEnvelope));
	return HandoffEnvelope;
}

static FHCIAbilityKitAgentStageGExecuteIntent MakeG2StageGIntent(
	const FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope& HandoffEnvelope,
	const FHCIAbilityKitAgentSimulateExecuteArchiveBundle& ArchiveBundle,
	const FHCIAbilityKitAgentSimulateExecuteFinalReport& FinalReport,
	const FHCIAbilityKitAgentSimulateExecuteReceipt& Receipt,
	const FHCIAbilityKitAgentExecuteTicket& ExecuteTicket,
	const FHCIAbilityKitAgentApplyConfirmRequest& ConfirmRequest,
	const FHCIAbilityKitAgentApplyRequest& ApplyRequest,
	const FHCIAbilityKitDryRunDiffReport& Review)
{
	FHCIAbilityKitAgentStageGExecuteIntent Intent;
	check(FHCIAbilityKitAgentExecutorStageGExecuteIntentBridge::BuildStageGExecuteIntent(
		HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, Intent));
	return Intent;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorStageGWriteEnableRequestReadyTest,
	"HCIAbilityKit.Editor.AgentExecutorStageGWriteEnableRequest.ReadyWhenStageGIntentReadyAndWriteEnableConfirmed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorStageGWriteEnableRequestReadyTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeG2SelectedReviewReport();
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeG2ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeG2ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAbilityKitAgentExecuteTicket ExecuteTicket = MakeG2ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteReceipt Receipt = MakeG2Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteFinalReport FinalReport = MakeG2FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeG2ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = MakeG2HandoffEnvelope(ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGExecuteIntent Intent = MakeG2StageGIntent(HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);

	FHCIAbilityKitAgentStageGWriteEnableRequest Request;
	TestTrue(TEXT("Build stage g write enable request"), FHCIAbilityKitAgentExecutorStageGWriteEnableRequestBridge::BuildStageGWriteEnableRequest(
		Intent, Intent.RequestId, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true, Request));
	TestTrue(TEXT("ReadyForStageGExecute"), Request.bReadyForStageGExecute);
	TestTrue(TEXT("WriteEnabled"), Request.bWriteEnabled);
	TestEqual(TEXT("StageGWriteStatus"), Request.StageGWriteStatus, FString(TEXT("ready")));
	TestEqual(TEXT("ErrorCode"), Request.ErrorCode, FString(TEXT("-")));
	TestEqual(TEXT("Reason"), Request.Reason, FString(TEXT("stage_g_write_enable_request_ready")));
	TestFalse(TEXT("StageGWriteEnableDigest present"), Request.StageGWriteEnableDigest.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorStageGWriteEnableRequestIntentNotReadyTest,
	"HCIAbilityKit.Editor.AgentExecutorStageGWriteEnableRequest.StageGIntentNotReadyReturnsE4210",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorStageGWriteEnableRequestIntentNotReadyTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeG2SelectedReviewReport();
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeG2ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeG2ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAbilityKitAgentExecuteTicket ExecuteTicket = MakeG2ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteReceipt Receipt = MakeG2Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteFinalReport FinalReport = MakeG2FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeG2ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = MakeG2HandoffEnvelope(ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	FHCIAbilityKitAgentStageGExecuteIntent Intent = MakeG2StageGIntent(HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	Intent.bReadyForStageGEntry = false;
	Intent.StageGStatus = TEXT("blocked");

	FHCIAbilityKitAgentStageGWriteEnableRequest Request;
	TestTrue(TEXT("Build stage g write enable request"), FHCIAbilityKitAgentExecutorStageGWriteEnableRequestBridge::BuildStageGWriteEnableRequest(
		Intent, Intent.RequestId, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true, Request));
	TestFalse(TEXT("ReadyForStageGExecute"), Request.bReadyForStageGExecute);
	TestFalse(TEXT("WriteEnabled"), Request.bWriteEnabled);
	TestEqual(TEXT("StageGWriteStatus"), Request.StageGWriteStatus, FString(TEXT("blocked")));
	TestEqual(TEXT("ErrorCode"), Request.ErrorCode, FString(TEXT("E4210")));
	TestEqual(TEXT("Reason"), Request.Reason, FString(TEXT("stage_g_execute_intent_not_ready")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorStageGWriteEnableRequestDigestMismatchTest,
	"HCIAbilityKit.Editor.AgentExecutorStageGWriteEnableRequest.DigestMismatchReturnsE4202",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorStageGWriteEnableRequestDigestMismatchTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeG2SelectedReviewReport();
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeG2ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeG2ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAbilityKitAgentExecuteTicket ExecuteTicket = MakeG2ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteReceipt Receipt = MakeG2Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteFinalReport FinalReport = MakeG2FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeG2ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = MakeG2HandoffEnvelope(ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	FHCIAbilityKitAgentStageGExecuteIntent Intent = MakeG2StageGIntent(HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	Intent.SelectionDigest = TEXT("crc32_BAD0C0DE");

	FHCIAbilityKitAgentStageGWriteEnableRequest Request;
	TestTrue(TEXT("Build stage g write enable request"), FHCIAbilityKitAgentExecutorStageGWriteEnableRequestBridge::BuildStageGWriteEnableRequest(
		Intent, Intent.RequestId, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true, Request));
	TestFalse(TEXT("ReadyForStageGExecute"), Request.bReadyForStageGExecute);
	TestEqual(TEXT("ErrorCode"), Request.ErrorCode, FString(TEXT("E4202")));
	TestEqual(TEXT("Reason"), Request.Reason, FString(TEXT("selection_digest_mismatch")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorStageGWriteEnableRequestIntentIdMismatchTest,
	"HCIAbilityKit.Editor.AgentExecutorStageGWriteEnableRequest.IntentIdMismatchReturnsE4202",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorStageGWriteEnableRequestIntentIdMismatchTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeG2SelectedReviewReport();
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeG2ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeG2ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAbilityKitAgentExecuteTicket ExecuteTicket = MakeG2ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteReceipt Receipt = MakeG2Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteFinalReport FinalReport = MakeG2FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeG2ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = MakeG2HandoffEnvelope(ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	FHCIAbilityKitAgentStageGExecuteIntent Intent = MakeG2StageGIntent(HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FString ExpectedIntentId = Intent.RequestId;
	Intent.RequestId += TEXT("_stale");

	FHCIAbilityKitAgentStageGWriteEnableRequest Request;
	TestTrue(TEXT("Build stage g write enable request"), FHCIAbilityKitAgentExecutorStageGWriteEnableRequestBridge::BuildStageGWriteEnableRequest(
		Intent, ExpectedIntentId, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true, Request));
	TestFalse(TEXT("ReadyForStageGExecute"), Request.bReadyForStageGExecute);
	TestEqual(TEXT("ErrorCode"), Request.ErrorCode, FString(TEXT("E4202")));
	TestEqual(TEXT("Reason"), Request.Reason, FString(TEXT("stage_g_execute_intent_id_mismatch")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorStageGWriteEnableRequestJsonFieldsTest,
	"HCIAbilityKit.Editor.AgentExecutorStageGWriteEnableRequest.JsonIncludesWriteEnableFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorStageGWriteEnableRequestJsonFieldsTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeG2SelectedReviewReport();
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeG2ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeG2ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAbilityKitAgentExecuteTicket ExecuteTicket = MakeG2ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteReceipt Receipt = MakeG2Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteFinalReport FinalReport = MakeG2FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeG2ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = MakeG2HandoffEnvelope(ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGExecuteIntent Intent = MakeG2StageGIntent(HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);

	FHCIAbilityKitAgentStageGWriteEnableRequest Request;
	TestTrue(TEXT("Build stage g write enable request"), FHCIAbilityKitAgentExecutorStageGWriteEnableRequestBridge::BuildStageGWriteEnableRequest(
		Intent, Intent.RequestId, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true, Request));

	FString JsonText;
	TestTrue(TEXT("Serialize stage g write enable request json"), FHCIAbilityKitAgentStageGWriteEnableRequestJsonSerializer::SerializeToJsonString(Request, JsonText));
	TestTrue(TEXT("JSON includes stage_g_execute_intent_id"), JsonText.Contains(TEXT("\"stage_g_execute_intent_id\"")));
	TestTrue(TEXT("JSON includes stage_g_write_enable_digest"), JsonText.Contains(TEXT("\"stage_g_write_enable_digest\"")));
	TestTrue(TEXT("JSON includes stage_g_write_status"), JsonText.Contains(TEXT("\"stage_g_write_status\"")));
	TestTrue(TEXT("JSON includes write_enable_confirmed"), JsonText.Contains(TEXT("\"write_enable_confirmed\"")));
	TestTrue(TEXT("JSON includes ready_for_stage_g_execute"), JsonText.Contains(TEXT("\"ready_for_stage_g_execute\"")));
	TestTrue(TEXT("JSON includes write_enabled"), JsonText.Contains(TEXT("\"write_enabled\"")));
	TestTrue(TEXT("JSON includes locate_strategy"), JsonText.Contains(TEXT("\"locate_strategy\"")));
	return true;
}

#endif
