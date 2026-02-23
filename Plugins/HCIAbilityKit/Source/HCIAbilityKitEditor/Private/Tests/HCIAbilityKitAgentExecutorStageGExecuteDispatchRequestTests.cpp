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
#include "Agent/HCIAbilityKitAgentExecutorStageGExecuteDispatchRequestBridge.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteArchiveBundle.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteFinalReport.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteHandoffEnvelope.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteReceipt.h"
#include "Agent/HCIAbilityKitAgentStageGExecuteIntent.h"
#include "Agent/HCIAbilityKitAgentStageGWriteEnableRequest.h"
#include "Agent/HCIAbilityKitAgentStageGExecuteDispatchRequest.h"
#include "Agent/HCIAbilityKitAgentStageGExecuteDispatchRequestJsonSerializer.h"
#include "Agent/HCIAbilityKitDryRunDiff.h"
#include "Misc/AutomationTest.h"

namespace
{
static FHCIAbilityKitDryRunDiffReport MakeG4SelectedReviewReport()
{
	FHCIAbilityKitDryRunDiffReport Report;
	Report.RequestId = TEXT("req_g3_review");

	FHCIAbilityKitDryRunDiffItem& Item = Report.DiffItems.AddDefaulted_GetRef();
	Item.AssetPath = TEXT("/Game/Levels/Demo/SM_G3_A.SM_G3_A");
	Item.Field = TEXT("step:s1");
	Item.ToolName = TEXT("ScanLevelMeshRisks");
	Item.Risk = EHCIAbilityKitDryRunRisk::ReadOnly;
	Item.ObjectType = EHCIAbilityKitDryRunObjectType::Actor;
	Item.LocateStrategy = EHCIAbilityKitDryRunLocateStrategy::CameraFocus;
	Item.EvidenceKey = TEXT("actor_path");
	Item.ActorPath = TEXT("/Game/Maps/Demo.PersistentLevel.SM_G3_A_01");

	FHCIAbilityKitDryRunDiff::NormalizeAndFinalize(Report);
	return Report;
}

static FHCIAbilityKitAgentApplyRequest MakeG4ApplyRequest(const FHCIAbilityKitDryRunDiffReport& Review)
{
	FHCIAbilityKitAgentApplyRequest ApplyRequest;
	check(FHCIAbilityKitAgentExecutorApplyRequestBridge::BuildApplyRequest(Review, ApplyRequest));
	return ApplyRequest;
}

static FHCIAbilityKitAgentApplyConfirmRequest MakeG4ConfirmRequest(
	const FHCIAbilityKitAgentApplyRequest& ApplyRequest,
	const FHCIAbilityKitDryRunDiffReport& Review,
	const bool bUserConfirmed)
{
	FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest;
	check(FHCIAbilityKitAgentExecutorApplyConfirmBridge::BuildConfirmRequest(ApplyRequest, Review, bUserConfirmed, ConfirmRequest));
	return ConfirmRequest;
}

static FHCIAbilityKitAgentExecuteTicket MakeG4ExecuteTicket(
	const FHCIAbilityKitAgentApplyConfirmRequest& ConfirmRequest,
	const FHCIAbilityKitAgentApplyRequest& ApplyRequest,
	const FHCIAbilityKitDryRunDiffReport& Review)
{
	FHCIAbilityKitAgentExecuteTicket ExecuteTicket;
	check(FHCIAbilityKitAgentExecutorExecuteTicketBridge::BuildExecuteTicket(ConfirmRequest, ApplyRequest, Review, ExecuteTicket));
	return ExecuteTicket;
}

static FHCIAbilityKitAgentSimulateExecuteReceipt MakeG4Receipt(
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

static FHCIAbilityKitAgentSimulateExecuteFinalReport MakeG4FinalReport(
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

static FHCIAbilityKitAgentSimulateExecuteArchiveBundle MakeG4ArchiveBundle(
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

static FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope MakeG4HandoffEnvelope(
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

static FHCIAbilityKitAgentStageGExecuteIntent MakeG4StageGIntent(
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

static FHCIAbilityKitAgentStageGWriteEnableRequest MakeG4WriteEnableRequest(
	const FHCIAbilityKitAgentStageGExecuteIntent& Intent,
	const FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope& HandoffEnvelope,
	const FHCIAbilityKitAgentSimulateExecuteArchiveBundle& ArchiveBundle,
	const FHCIAbilityKitAgentSimulateExecuteFinalReport& FinalReport,
	const FHCIAbilityKitAgentSimulateExecuteReceipt& Receipt,
	const FHCIAbilityKitAgentExecuteTicket& ExecuteTicket,
	const FHCIAbilityKitAgentApplyConfirmRequest& ConfirmRequest,
	const FHCIAbilityKitAgentApplyRequest& ApplyRequest,
	const FHCIAbilityKitDryRunDiffReport& Review,
	const bool bWriteEnableConfirmed)
{
	FHCIAbilityKitAgentStageGWriteEnableRequest Request;
	check(FHCIAbilityKitAgentExecutorStageGWriteEnableRequestBridge::BuildStageGWriteEnableRequest(
		Intent,
		Intent.RequestId,
		HandoffEnvelope,
		ArchiveBundle,
		FinalReport,
		Receipt,
		ExecuteTicket,
		ConfirmRequest,
		ApplyRequest,
		Review,
		bWriteEnableConfirmed,
		Request));
	return Request;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorStageGExecuteDispatchRequestReadyTest,
	"HCIAbilityKit.Editor.AgentExecutorStageGExecuteDispatchRequest.ReadyWhenStageGWriteEnableRequestReady",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorStageGExecuteDispatchRequestReadyTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeG4SelectedReviewReport();
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeG4ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeG4ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAbilityKitAgentExecuteTicket ExecuteTicket = MakeG4ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteReceipt Receipt = MakeG4Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteFinalReport FinalReport = MakeG4FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeG4ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = MakeG4HandoffEnvelope(ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGExecuteIntent Intent = MakeG4StageGIntent(HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGWriteEnableRequest WriteEnableRequest = MakeG4WriteEnableRequest(Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);

	FHCIAbilityKitAgentStageGExecuteDispatchRequest Ticket;
	TestTrue(TEXT("Build stage g execute dispatch request"), FHCIAbilityKitAgentExecutorStageGExecuteDispatchRequestBridge::BuildStageGExecuteDispatchRequest(
		WriteEnableRequest,
		WriteEnableRequest.RequestId,
		Intent,
		HandoffEnvelope,
		ArchiveBundle,
		FinalReport,
		Receipt,
		ExecuteTicket,
		ConfirmRequest,
		ApplyRequest,
		Review,
		Ticket));
	TestTrue(TEXT("PermitReady"), Ticket.bStageGExecutePermitReady);
	TestTrue(TEXT("WriteEnabled"), Ticket.bWriteEnabled);
	TestEqual(TEXT("PermitStatus"), Ticket.StageGExecutePermitStatus, FString(TEXT("ready")));
	TestEqual(TEXT("ErrorCode"), Ticket.ErrorCode, FString(TEXT("-")));
	TestEqual(TEXT("Reason"), Ticket.Reason, FString(TEXT("stage_g_execute_dispatch_ticket_ready")));
	TestFalse(TEXT("PermitDigest present"), Ticket.StageGExecutePermitDigest.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorStageGExecuteDispatchRequestWriteEnableNotReadyTest,
	"HCIAbilityKit.Editor.AgentExecutorStageGExecuteDispatchRequest.StageGWriteEnableRequestNotReadyReturnsE4212",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorStageGExecuteDispatchRequestWriteEnableNotReadyTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeG4SelectedReviewReport();
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeG4ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeG4ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAbilityKitAgentExecuteTicket ExecuteTicket = MakeG4ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteReceipt Receipt = MakeG4Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteFinalReport FinalReport = MakeG4FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeG4ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = MakeG4HandoffEnvelope(ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGExecuteIntent Intent = MakeG4StageGIntent(HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	FHCIAbilityKitAgentStageGWriteEnableRequest WriteEnableRequest = MakeG4WriteEnableRequest(Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	WriteEnableRequest.bWriteEnabled = false;
	WriteEnableRequest.bReadyForStageGExecute = false;
	WriteEnableRequest.StageGWriteStatus = TEXT("blocked");

	FHCIAbilityKitAgentStageGExecuteDispatchRequest Ticket;
	TestTrue(TEXT("Build stage g execute dispatch request"), FHCIAbilityKitAgentExecutorStageGExecuteDispatchRequestBridge::BuildStageGExecuteDispatchRequest(
		WriteEnableRequest,
		WriteEnableRequest.RequestId,
		Intent,
		HandoffEnvelope,
		ArchiveBundle,
		FinalReport,
		Receipt,
		ExecuteTicket,
		ConfirmRequest,
		ApplyRequest,
		Review,
		Ticket));
	TestFalse(TEXT("PermitReady"), Ticket.bStageGExecutePermitReady);
	TestEqual(TEXT("PermitStatus"), Ticket.StageGExecutePermitStatus, FString(TEXT("blocked")));
	TestEqual(TEXT("ErrorCode"), Ticket.ErrorCode, FString(TEXT("E4212")));
	TestEqual(TEXT("Reason"), Ticket.Reason, FString(TEXT("stage_g_execute_permit_ticket_not_ready")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorStageGExecuteDispatchRequestDigestMismatchTest,
	"HCIAbilityKit.Editor.AgentExecutorStageGExecuteDispatchRequest.DigestMismatchReturnsE4202",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorStageGExecuteDispatchRequestDigestMismatchTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeG4SelectedReviewReport();
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeG4ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeG4ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAbilityKitAgentExecuteTicket ExecuteTicket = MakeG4ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteReceipt Receipt = MakeG4Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteFinalReport FinalReport = MakeG4FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeG4ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = MakeG4HandoffEnvelope(ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGExecuteIntent Intent = MakeG4StageGIntent(HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	FHCIAbilityKitAgentStageGWriteEnableRequest WriteEnableRequest = MakeG4WriteEnableRequest(Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	WriteEnableRequest.SelectionDigest = TEXT("crc32_BAD0C0DE");

	FHCIAbilityKitAgentStageGExecuteDispatchRequest Ticket;
	TestTrue(TEXT("Build stage g execute dispatch request"), FHCIAbilityKitAgentExecutorStageGExecuteDispatchRequestBridge::BuildStageGExecuteDispatchRequest(
		WriteEnableRequest,
		WriteEnableRequest.RequestId,
		Intent,
		HandoffEnvelope,
		ArchiveBundle,
		FinalReport,
		Receipt,
		ExecuteTicket,
		ConfirmRequest,
		ApplyRequest,
		Review,
		Ticket));
	TestFalse(TEXT("PermitReady"), Ticket.bStageGExecutePermitReady);
	TestEqual(TEXT("ErrorCode"), Ticket.ErrorCode, FString(TEXT("E4202")));
	TestEqual(TEXT("Reason"), Ticket.Reason, FString(TEXT("selection_digest_mismatch")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorStageGExecuteDispatchRequestJsonFieldsTest,
	"HCIAbilityKit.Editor.AgentExecutorStageGExecuteDispatchRequest.JsonIncludesPermitFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorStageGExecuteDispatchRequestJsonFieldsTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeG4SelectedReviewReport();
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeG4ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeG4ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAbilityKitAgentExecuteTicket ExecuteTicket = MakeG4ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteReceipt Receipt = MakeG4Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteFinalReport FinalReport = MakeG4FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeG4ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = MakeG4HandoffEnvelope(ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGExecuteIntent Intent = MakeG4StageGIntent(HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGWriteEnableRequest WriteEnableRequest = MakeG4WriteEnableRequest(Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);

	FHCIAbilityKitAgentStageGExecuteDispatchRequest Ticket;
	TestTrue(TEXT("Build stage g execute dispatch request"), FHCIAbilityKitAgentExecutorStageGExecuteDispatchRequestBridge::BuildStageGExecuteDispatchRequest(
		WriteEnableRequest,
		WriteEnableRequest.RequestId,
		Intent,
		HandoffEnvelope,
		ArchiveBundle,
		FinalReport,
		Receipt,
		ExecuteTicket,
		ConfirmRequest,
		ApplyRequest,
		Review,
		Ticket));

	FString JsonText;
	TestTrue(TEXT("Serialize stage g execute dispatch request json"), FHCIAbilityKitAgentStageGExecuteDispatchRequestJsonSerializer::SerializeToJsonString(Ticket, JsonText));
	TestTrue(TEXT("JSON includes stage_g_write_enable_request_id"), JsonText.Contains(TEXT("\"stage_g_write_enable_request_id\"")));
	TestTrue(TEXT("JSON includes stage_g_write_enable_digest"), JsonText.Contains(TEXT("\"stage_g_write_enable_digest\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_dispatch_digest"), JsonText.Contains(TEXT("\"stage_g_execute_dispatch_digest\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_dispatch_status"), JsonText.Contains(TEXT("\"stage_g_execute_dispatch_status\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_dispatch_ready"), JsonText.Contains(TEXT("\"stage_g_execute_dispatch_ready\"")));
	TestTrue(TEXT("JSON includes locate_strategy"), JsonText.Contains(TEXT("\"locate_strategy\"")));
	return true;
}

#endif