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
#include "Agent/Bridges/HCIAgentExecutorStageGExecuteIntentBridge.h"
#include "Agent/Bridges/HCIAgentExecutorStageGWriteEnableRequestBridge.h"
#include "Agent/Bridges/HCIAgentExecutorStageGExecutePermitTicketBridge.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteArchiveBundle.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteFinalReport.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteHandoffEnvelope.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteReceipt.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecuteIntent.h"
#include "Agent/Contracts/StageG/HCIAgentStageGWriteEnableRequest.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecutePermitTicket.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecutePermitTicketJsonSerializer.h"
#include "Agent/Executor/HCIDryRunDiff.h"
#include "Misc/AutomationTest.h"

namespace
{
static FHCIDryRunDiffReport MakeG3SelectedReviewReport()
{
	FHCIDryRunDiffReport Report;
	Report.RequestId = TEXT("req_g3_review");

	FHCIDryRunDiffItem& Item = Report.DiffItems.AddDefaulted_GetRef();
	Item.AssetPath = TEXT("/Game/Levels/Demo/SM_G3_A.SM_G3_A");
	Item.Field = TEXT("step:s1");
	Item.ToolName = TEXT("ScanLevelMeshRisks");
	Item.Risk = EHCIDryRunRisk::ReadOnly;
	Item.ObjectType = EHCIDryRunObjectType::Actor;
	Item.LocateStrategy = EHCIDryRunLocateStrategy::CameraFocus;
	Item.EvidenceKey = TEXT("actor_path");
	Item.ActorPath = TEXT("/Game/Maps/Demo.PersistentLevel.SM_G3_A_01");

	FHCIDryRunDiff::NormalizeAndFinalize(Report);
	return Report;
}

static FHCIAgentApplyRequest MakeG3ApplyRequest(const FHCIDryRunDiffReport& Review)
{
	FHCIAgentApplyRequest ApplyRequest;
	check(FHCIAgentExecutorApplyRequestBridge::BuildApplyRequest(Review, ApplyRequest));
	return ApplyRequest;
}

static FHCIAgentApplyConfirmRequest MakeG3ConfirmRequest(
	const FHCIAgentApplyRequest& ApplyRequest,
	const FHCIDryRunDiffReport& Review,
	const bool bUserConfirmed)
{
	FHCIAgentApplyConfirmRequest ConfirmRequest;
	check(FHCIAgentExecutorApplyConfirmBridge::BuildConfirmRequest(ApplyRequest, Review, bUserConfirmed, ConfirmRequest));
	return ConfirmRequest;
}

static FHCIAgentExecuteTicket MakeG3ExecuteTicket(
	const FHCIAgentApplyConfirmRequest& ConfirmRequest,
	const FHCIAgentApplyRequest& ApplyRequest,
	const FHCIDryRunDiffReport& Review)
{
	FHCIAgentExecuteTicket ExecuteTicket;
	check(FHCIAgentExecutorExecuteTicketBridge::BuildExecuteTicket(ConfirmRequest, ApplyRequest, Review, ExecuteTicket));
	return ExecuteTicket;
}

static FHCIAgentSimulateExecuteReceipt MakeG3Receipt(
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

static FHCIAgentSimulateExecuteFinalReport MakeG3FinalReport(
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

static FHCIAgentSimulateExecuteArchiveBundle MakeG3ArchiveBundle(
	const FHCIAgentSimulateExecuteFinalReport& FinalReport,
	const FHCIAgentSimulateExecuteReceipt& Receipt,
	const FHCIAgentExecuteTicket& ExecuteTicket,
	const FHCIAgentApplyConfirmRequest& ConfirmRequest,
	const FHCIAgentApplyRequest& ApplyRequest,
	const FHCIDryRunDiffReport& Review)
{
	FHCIAgentSimulateExecuteArchiveBundle ArchiveBundle;
	check(FHCIAgentExecutorSimulateExecuteArchiveBundleBridge::BuildSimulateExecuteArchiveBundle(
		FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, ArchiveBundle));
	return ArchiveBundle;
}

static FHCIAgentSimulateExecuteHandoffEnvelope MakeG3HandoffEnvelope(
	const FHCIAgentSimulateExecuteArchiveBundle& ArchiveBundle,
	const FHCIAgentSimulateExecuteFinalReport& FinalReport,
	const FHCIAgentSimulateExecuteReceipt& Receipt,
	const FHCIAgentExecuteTicket& ExecuteTicket,
	const FHCIAgentApplyConfirmRequest& ConfirmRequest,
	const FHCIAgentApplyRequest& ApplyRequest,
	const FHCIDryRunDiffReport& Review)
{
	FHCIAgentSimulateExecuteHandoffEnvelope HandoffEnvelope;
	check(FHCIAgentExecutorSimulateExecuteHandoffEnvelopeBridge::BuildSimulateExecuteHandoffEnvelope(
		ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, HandoffEnvelope));
	return HandoffEnvelope;
}

static FHCIAgentStageGExecuteIntent MakeG3StageGIntent(
	const FHCIAgentSimulateExecuteHandoffEnvelope& HandoffEnvelope,
	const FHCIAgentSimulateExecuteArchiveBundle& ArchiveBundle,
	const FHCIAgentSimulateExecuteFinalReport& FinalReport,
	const FHCIAgentSimulateExecuteReceipt& Receipt,
	const FHCIAgentExecuteTicket& ExecuteTicket,
	const FHCIAgentApplyConfirmRequest& ConfirmRequest,
	const FHCIAgentApplyRequest& ApplyRequest,
	const FHCIDryRunDiffReport& Review)
{
	FHCIAgentStageGExecuteIntent Intent;
	check(FHCIAgentExecutorStageGExecuteIntentBridge::BuildStageGExecuteIntent(
		HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, Intent));
	return Intent;
}

static FHCIAgentStageGWriteEnableRequest MakeG3WriteEnableRequest(
	const FHCIAgentStageGExecuteIntent& Intent,
	const FHCIAgentSimulateExecuteHandoffEnvelope& HandoffEnvelope,
	const FHCIAgentSimulateExecuteArchiveBundle& ArchiveBundle,
	const FHCIAgentSimulateExecuteFinalReport& FinalReport,
	const FHCIAgentSimulateExecuteReceipt& Receipt,
	const FHCIAgentExecuteTicket& ExecuteTicket,
	const FHCIAgentApplyConfirmRequest& ConfirmRequest,
	const FHCIAgentApplyRequest& ApplyRequest,
	const FHCIDryRunDiffReport& Review,
	const bool bWriteEnableConfirmed)
{
	FHCIAgentStageGWriteEnableRequest Request;
	check(FHCIAgentExecutorStageGWriteEnableRequestBridge::BuildStageGWriteEnableRequest(
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
	FHCIAgentExecutorStageGExecutePermitTicketReadyTest,
	"HCI.Editor.AgentExecutorStageGExecutePermitTicket.ReadyWhenStageGWriteEnableRequestReady",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorStageGExecutePermitTicketReadyTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeG3SelectedReviewReport();
	const FHCIAgentApplyRequest ApplyRequest = MakeG3ApplyRequest(Review);
	const FHCIAgentApplyConfirmRequest ConfirmRequest = MakeG3ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAgentExecuteTicket ExecuteTicket = MakeG3ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteReceipt Receipt = MakeG3Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteFinalReport FinalReport = MakeG3FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeG3ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = MakeG3HandoffEnvelope(ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGExecuteIntent Intent = MakeG3StageGIntent(HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGWriteEnableRequest WriteEnableRequest = MakeG3WriteEnableRequest(Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);

	FHCIAgentStageGExecutePermitTicket Ticket;
	TestTrue(TEXT("Build stage g execute permit ticket"), FHCIAgentExecutorStageGExecutePermitTicketBridge::BuildStageGExecutePermitTicket(
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
	FHCIAgentExecutorStageGExecutePermitTicketWriteEnableNotReadyTest,
	"HCI.Editor.AgentExecutorStageGExecutePermitTicket.StageGWriteEnableRequestNotReadyReturnsE4211",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorStageGExecutePermitTicketWriteEnableNotReadyTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeG3SelectedReviewReport();
	const FHCIAgentApplyRequest ApplyRequest = MakeG3ApplyRequest(Review);
	const FHCIAgentApplyConfirmRequest ConfirmRequest = MakeG3ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAgentExecuteTicket ExecuteTicket = MakeG3ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteReceipt Receipt = MakeG3Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteFinalReport FinalReport = MakeG3FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeG3ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = MakeG3HandoffEnvelope(ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGExecuteIntent Intent = MakeG3StageGIntent(HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	FHCIAgentStageGWriteEnableRequest WriteEnableRequest = MakeG3WriteEnableRequest(Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	WriteEnableRequest.bWriteEnabled = false;
	WriteEnableRequest.bReadyForStageGExecute = false;
	WriteEnableRequest.StageGWriteStatus = TEXT("blocked");

	FHCIAgentStageGExecutePermitTicket Ticket;
	TestTrue(TEXT("Build stage g execute permit ticket"), FHCIAgentExecutorStageGExecutePermitTicketBridge::BuildStageGExecutePermitTicket(
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
	FHCIAgentExecutorStageGExecutePermitTicketDigestMismatchTest,
	"HCI.Editor.AgentExecutorStageGExecutePermitTicket.DigestMismatchReturnsE4202",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorStageGExecutePermitTicketDigestMismatchTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeG3SelectedReviewReport();
	const FHCIAgentApplyRequest ApplyRequest = MakeG3ApplyRequest(Review);
	const FHCIAgentApplyConfirmRequest ConfirmRequest = MakeG3ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAgentExecuteTicket ExecuteTicket = MakeG3ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteReceipt Receipt = MakeG3Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteFinalReport FinalReport = MakeG3FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeG3ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = MakeG3HandoffEnvelope(ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGExecuteIntent Intent = MakeG3StageGIntent(HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	FHCIAgentStageGWriteEnableRequest WriteEnableRequest = MakeG3WriteEnableRequest(Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	WriteEnableRequest.SelectionDigest = TEXT("crc32_BAD0C0DE");

	FHCIAgentStageGExecutePermitTicket Ticket;
	TestTrue(TEXT("Build stage g execute permit ticket"), FHCIAgentExecutorStageGExecutePermitTicketBridge::BuildStageGExecutePermitTicket(
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
	FHCIAgentExecutorStageGExecutePermitTicketJsonFieldsTest,
	"HCI.Editor.AgentExecutorStageGExecutePermitTicket.JsonIncludesPermitFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorStageGExecutePermitTicketJsonFieldsTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeG3SelectedReviewReport();
	const FHCIAgentApplyRequest ApplyRequest = MakeG3ApplyRequest(Review);
	const FHCIAgentApplyConfirmRequest ConfirmRequest = MakeG3ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAgentExecuteTicket ExecuteTicket = MakeG3ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteReceipt Receipt = MakeG3Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteFinalReport FinalReport = MakeG3FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeG3ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = MakeG3HandoffEnvelope(ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGExecuteIntent Intent = MakeG3StageGIntent(HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGWriteEnableRequest WriteEnableRequest = MakeG3WriteEnableRequest(Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);

	FHCIAgentStageGExecutePermitTicket Ticket;
	TestTrue(TEXT("Build stage g execute permit ticket"), FHCIAgentExecutorStageGExecutePermitTicketBridge::BuildStageGExecutePermitTicket(
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
	TestTrue(TEXT("Serialize stage g execute permit ticket json"), FHCIAgentStageGExecutePermitTicketJsonSerializer::SerializeToJsonString(Ticket, JsonText));
	TestTrue(TEXT("JSON includes stage_g_write_enable_request_id"), JsonText.Contains(TEXT("\"stage_g_write_enable_request_id\"")));
	TestTrue(TEXT("JSON includes stage_g_write_enable_digest"), JsonText.Contains(TEXT("\"stage_g_write_enable_digest\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_permit_digest"), JsonText.Contains(TEXT("\"stage_g_execute_permit_digest\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_permit_status"), JsonText.Contains(TEXT("\"stage_g_execute_permit_status\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_permit_ready"), JsonText.Contains(TEXT("\"stage_g_execute_permit_ready\"")));
	TestTrue(TEXT("JSON includes locate_strategy"), JsonText.Contains(TEXT("\"locate_strategy\"")));
	return true;
}

#endif
