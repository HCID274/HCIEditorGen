#if WITH_DEV_AUTOMATION_TESTS

#include "Agent/Contracts/StageF/HCIAbilityKitAgentApplyConfirmRequest.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentApplyRequest.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentExecuteTicket.h"
#include "Agent/Bridges/HCIAbilityKitAgentExecutorApplyConfirmBridge.h"
#include "Agent/Bridges/HCIAbilityKitAgentExecutorApplyRequestBridge.h"
#include "Agent/Bridges/HCIAbilityKitAgentExecutorExecuteTicketBridge.h"
#include "Agent/Bridges/HCIAbilityKitAgentExecutorSimulateExecuteArchiveBundleBridge.h"
#include "Agent/Bridges/HCIAbilityKitAgentExecutorSimulateExecuteFinalReportBridge.h"
#include "Agent/Bridges/HCIAbilityKitAgentExecutorSimulateExecuteHandoffEnvelopeBridge.h"
#include "Agent/Bridges/HCIAbilityKitAgentExecutorSimulateExecuteReceiptBridge.h"
#include "Agent/Bridges/HCIAbilityKitAgentExecutorStageGExecuteIntentBridge.h"
#include "Agent/Bridges/HCIAbilityKitAgentExecutorStageGWriteEnableRequestBridge.h"
#include "Agent/Bridges/HCIAbilityKitAgentExecutorStageGExecutePermitTicketBridge.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentSimulateExecuteArchiveBundle.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentSimulateExecuteFinalReport.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentSimulateExecuteHandoffEnvelope.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentSimulateExecuteReceipt.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGExecuteIntent.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGWriteEnableRequest.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGExecutePermitTicket.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGExecutePermitTicketJsonSerializer.h"
#include "Agent/Executor/HCIAbilityKitDryRunDiff.h"
#include "Misc/AutomationTest.h"

namespace
{
static FHCIAbilityKitDryRunDiffReport MakeG3SelectedReviewReport()
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

static FHCIAbilityKitAgentApplyRequest MakeG3ApplyRequest(const FHCIAbilityKitDryRunDiffReport& Review)
{
	FHCIAbilityKitAgentApplyRequest ApplyRequest;
	check(FHCIAbilityKitAgentExecutorApplyRequestBridge::BuildApplyRequest(Review, ApplyRequest));
	return ApplyRequest;
}

static FHCIAbilityKitAgentApplyConfirmRequest MakeG3ConfirmRequest(
	const FHCIAbilityKitAgentApplyRequest& ApplyRequest,
	const FHCIAbilityKitDryRunDiffReport& Review,
	const bool bUserConfirmed)
{
	FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest;
	check(FHCIAbilityKitAgentExecutorApplyConfirmBridge::BuildConfirmRequest(ApplyRequest, Review, bUserConfirmed, ConfirmRequest));
	return ConfirmRequest;
}

static FHCIAbilityKitAgentExecuteTicket MakeG3ExecuteTicket(
	const FHCIAbilityKitAgentApplyConfirmRequest& ConfirmRequest,
	const FHCIAbilityKitAgentApplyRequest& ApplyRequest,
	const FHCIAbilityKitDryRunDiffReport& Review)
{
	FHCIAbilityKitAgentExecuteTicket ExecuteTicket;
	check(FHCIAbilityKitAgentExecutorExecuteTicketBridge::BuildExecuteTicket(ConfirmRequest, ApplyRequest, Review, ExecuteTicket));
	return ExecuteTicket;
}

static FHCIAbilityKitAgentSimulateExecuteReceipt MakeG3Receipt(
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

static FHCIAbilityKitAgentSimulateExecuteFinalReport MakeG3FinalReport(
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

static FHCIAbilityKitAgentSimulateExecuteArchiveBundle MakeG3ArchiveBundle(
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

static FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope MakeG3HandoffEnvelope(
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

static FHCIAbilityKitAgentStageGExecuteIntent MakeG3StageGIntent(
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

static FHCIAbilityKitAgentStageGWriteEnableRequest MakeG3WriteEnableRequest(
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
	FHCIAbilityKitAgentExecutorStageGExecutePermitTicketReadyTest,
	"HCIAbilityKit.Editor.AgentExecutorStageGExecutePermitTicket.ReadyWhenStageGWriteEnableRequestReady",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorStageGExecutePermitTicketReadyTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeG3SelectedReviewReport();
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeG3ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeG3ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAbilityKitAgentExecuteTicket ExecuteTicket = MakeG3ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteReceipt Receipt = MakeG3Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteFinalReport FinalReport = MakeG3FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeG3ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = MakeG3HandoffEnvelope(ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGExecuteIntent Intent = MakeG3StageGIntent(HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGWriteEnableRequest WriteEnableRequest = MakeG3WriteEnableRequest(Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);

	FHCIAbilityKitAgentStageGExecutePermitTicket Ticket;
	TestTrue(TEXT("Build stage g execute permit ticket"), FHCIAbilityKitAgentExecutorStageGExecutePermitTicketBridge::BuildStageGExecutePermitTicket(
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
	TestEqual(TEXT("Reason"), Ticket.Reason, FString(TEXT("stage_g_execute_permit_ticket_ready")));
	TestFalse(TEXT("PermitDigest present"), Ticket.StageGExecutePermitDigest.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorStageGExecutePermitTicketWriteEnableNotReadyTest,
	"HCIAbilityKit.Editor.AgentExecutorStageGExecutePermitTicket.StageGWriteEnableRequestNotReadyReturnsE4211",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorStageGExecutePermitTicketWriteEnableNotReadyTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeG3SelectedReviewReport();
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeG3ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeG3ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAbilityKitAgentExecuteTicket ExecuteTicket = MakeG3ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteReceipt Receipt = MakeG3Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteFinalReport FinalReport = MakeG3FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeG3ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = MakeG3HandoffEnvelope(ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGExecuteIntent Intent = MakeG3StageGIntent(HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	FHCIAbilityKitAgentStageGWriteEnableRequest WriteEnableRequest = MakeG3WriteEnableRequest(Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	WriteEnableRequest.bWriteEnabled = false;
	WriteEnableRequest.bReadyForStageGExecute = false;
	WriteEnableRequest.StageGWriteStatus = TEXT("blocked");

	FHCIAbilityKitAgentStageGExecutePermitTicket Ticket;
	TestTrue(TEXT("Build stage g execute permit ticket"), FHCIAbilityKitAgentExecutorStageGExecutePermitTicketBridge::BuildStageGExecutePermitTicket(
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
	TestEqual(TEXT("ErrorCode"), Ticket.ErrorCode, FString(TEXT("E4211")));
	TestEqual(TEXT("Reason"), Ticket.Reason, FString(TEXT("stage_g_write_enable_request_not_ready")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorStageGExecutePermitTicketDigestMismatchTest,
	"HCIAbilityKit.Editor.AgentExecutorStageGExecutePermitTicket.DigestMismatchReturnsE4202",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorStageGExecutePermitTicketDigestMismatchTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeG3SelectedReviewReport();
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeG3ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeG3ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAbilityKitAgentExecuteTicket ExecuteTicket = MakeG3ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteReceipt Receipt = MakeG3Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteFinalReport FinalReport = MakeG3FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeG3ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = MakeG3HandoffEnvelope(ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGExecuteIntent Intent = MakeG3StageGIntent(HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	FHCIAbilityKitAgentStageGWriteEnableRequest WriteEnableRequest = MakeG3WriteEnableRequest(Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	WriteEnableRequest.SelectionDigest = TEXT("crc32_BAD0C0DE");

	FHCIAbilityKitAgentStageGExecutePermitTicket Ticket;
	TestTrue(TEXT("Build stage g execute permit ticket"), FHCIAbilityKitAgentExecutorStageGExecutePermitTicketBridge::BuildStageGExecutePermitTicket(
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
	FHCIAbilityKitAgentExecutorStageGExecutePermitTicketJsonFieldsTest,
	"HCIAbilityKit.Editor.AgentExecutorStageGExecutePermitTicket.JsonIncludesPermitFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorStageGExecutePermitTicketJsonFieldsTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeG3SelectedReviewReport();
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeG3ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeG3ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAbilityKitAgentExecuteTicket ExecuteTicket = MakeG3ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteReceipt Receipt = MakeG3Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteFinalReport FinalReport = MakeG3FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeG3ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = MakeG3HandoffEnvelope(ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGExecuteIntent Intent = MakeG3StageGIntent(HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGWriteEnableRequest WriteEnableRequest = MakeG3WriteEnableRequest(Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);

	FHCIAbilityKitAgentStageGExecutePermitTicket Ticket;
	TestTrue(TEXT("Build stage g execute permit ticket"), FHCIAbilityKitAgentExecutorStageGExecutePermitTicketBridge::BuildStageGExecutePermitTicket(
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
	TestTrue(TEXT("Serialize stage g execute permit ticket json"), FHCIAbilityKitAgentStageGExecutePermitTicketJsonSerializer::SerializeToJsonString(Ticket, JsonText));
	TestTrue(TEXT("JSON includes stage_g_write_enable_request_id"), JsonText.Contains(TEXT("\"stage_g_write_enable_request_id\"")));
	TestTrue(TEXT("JSON includes stage_g_write_enable_digest"), JsonText.Contains(TEXT("\"stage_g_write_enable_digest\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_permit_digest"), JsonText.Contains(TEXT("\"stage_g_execute_permit_digest\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_permit_status"), JsonText.Contains(TEXT("\"stage_g_execute_permit_status\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_permit_ready"), JsonText.Contains(TEXT("\"stage_g_execute_permit_ready\"")));
	TestTrue(TEXT("JSON includes locate_strategy"), JsonText.Contains(TEXT("\"locate_strategy\"")));
	return true;
}

#endif