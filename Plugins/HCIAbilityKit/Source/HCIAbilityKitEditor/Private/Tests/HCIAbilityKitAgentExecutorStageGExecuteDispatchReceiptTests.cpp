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
#include "Agent/HCIAbilityKitAgentExecutorStageGExecutePermitTicketBridge.h"
#include "Agent/HCIAbilityKitAgentExecutorStageGExecuteDispatchRequestBridge.h"
#include "Agent/HCIAbilityKitAgentExecutorStageGExecuteDispatchReceiptBridge.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteArchiveBundle.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteFinalReport.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteHandoffEnvelope.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteReceipt.h"
#include "Agent/HCIAbilityKitAgentStageGExecuteIntent.h"
#include "Agent/HCIAbilityKitAgentStageGWriteEnableRequest.h"
#include "Agent/HCIAbilityKitAgentStageGExecutePermitTicket.h"
#include "Agent/HCIAbilityKitAgentStageGExecuteDispatchRequest.h"
#include "Agent/HCIAbilityKitAgentStageGExecuteDispatchReceipt.h"
#include "Agent/HCIAbilityKitAgentStageGExecuteDispatchReceiptJsonSerializer.h"
#include "Agent/HCIAbilityKitDryRunDiff.h"
#include "Misc/AutomationTest.h"

namespace
{
static FHCIAbilityKitDryRunDiffReport MakeG5SelectedReviewReport()
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

static FHCIAbilityKitAgentApplyRequest MakeG5ApplyRequest(const FHCIAbilityKitDryRunDiffReport& Review)
{
	FHCIAbilityKitAgentApplyRequest ApplyRequest;
	check(FHCIAbilityKitAgentExecutorApplyRequestBridge::BuildApplyRequest(Review, ApplyRequest));
	return ApplyRequest;
}

static FHCIAbilityKitAgentApplyConfirmRequest MakeG5ConfirmRequest(
	const FHCIAbilityKitAgentApplyRequest& ApplyRequest,
	const FHCIAbilityKitDryRunDiffReport& Review,
	const bool bUserConfirmed)
{
	FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest;
	check(FHCIAbilityKitAgentExecutorApplyConfirmBridge::BuildConfirmRequest(ApplyRequest, Review, bUserConfirmed, ConfirmRequest));
	return ConfirmRequest;
}

static FHCIAbilityKitAgentExecuteTicket MakeG5ExecuteTicket(
	const FHCIAbilityKitAgentApplyConfirmRequest& ConfirmRequest,
	const FHCIAbilityKitAgentApplyRequest& ApplyRequest,
	const FHCIAbilityKitDryRunDiffReport& Review)
{
	FHCIAbilityKitAgentExecuteTicket ExecuteTicket;
	check(FHCIAbilityKitAgentExecutorExecuteTicketBridge::BuildExecuteTicket(ConfirmRequest, ApplyRequest, Review, ExecuteTicket));
	return ExecuteTicket;
}

static FHCIAbilityKitAgentSimulateExecuteReceipt MakeG5Receipt(
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

static FHCIAbilityKitAgentSimulateExecuteFinalReport MakeG5FinalReport(
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

static FHCIAbilityKitAgentSimulateExecuteArchiveBundle MakeG5ArchiveBundle(
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

static FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope MakeG5HandoffEnvelope(
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

static FHCIAbilityKitAgentStageGExecuteIntent MakeG5StageGIntent(
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

static FHCIAbilityKitAgentStageGWriteEnableRequest MakeG5WriteEnableRequest(
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

static FHCIAbilityKitAgentStageGExecutePermitTicket MakeG5PermitTicket(
	const FHCIAbilityKitAgentStageGWriteEnableRequest& WriteEnableRequest,
	const FHCIAbilityKitAgentStageGExecuteIntent& Intent,
	const FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope& HandoffEnvelope,
	const FHCIAbilityKitAgentSimulateExecuteArchiveBundle& ArchiveBundle,
	const FHCIAbilityKitAgentSimulateExecuteFinalReport& FinalReport,
	const FHCIAbilityKitAgentSimulateExecuteReceipt& Receipt,
	const FHCIAbilityKitAgentExecuteTicket& ExecuteTicket,
	const FHCIAbilityKitAgentApplyConfirmRequest& ConfirmRequest,
	const FHCIAbilityKitAgentApplyRequest& ApplyRequest,
	const FHCIAbilityKitDryRunDiffReport& Review)
{
	FHCIAbilityKitAgentStageGExecutePermitTicket Ticket;
	check(FHCIAbilityKitAgentExecutorStageGExecutePermitTicketBridge::BuildStageGExecutePermitTicket(
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
	return Ticket;
}

static FHCIAbilityKitAgentStageGExecuteDispatchRequest MakeG5DispatchRequest(
	const FHCIAbilityKitAgentStageGExecutePermitTicket& PermitTicket,
	const FHCIAbilityKitAgentStageGWriteEnableRequest& WriteEnableRequest,
	const FHCIAbilityKitAgentStageGExecuteIntent& Intent,
	const FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope& HandoffEnvelope,
	const FHCIAbilityKitAgentSimulateExecuteArchiveBundle& ArchiveBundle,
	const FHCIAbilityKitAgentSimulateExecuteFinalReport& FinalReport,
	const FHCIAbilityKitAgentSimulateExecuteReceipt& Receipt,
	const FHCIAbilityKitAgentExecuteTicket& ExecuteTicket,
	const FHCIAbilityKitAgentApplyConfirmRequest& ConfirmRequest,
	const FHCIAbilityKitAgentApplyRequest& ApplyRequest,
	const FHCIAbilityKitDryRunDiffReport& Review,
	const bool bExecuteDispatchConfirmed)
{
	FHCIAbilityKitAgentStageGExecuteDispatchRequest Request;
	check(FHCIAbilityKitAgentExecutorStageGExecuteDispatchRequestBridge::BuildStageGExecuteDispatchRequest(
		PermitTicket,
		PermitTicket.RequestId,
		WriteEnableRequest,
		Intent,
		HandoffEnvelope,
		ArchiveBundle,
		FinalReport,
		Receipt,
		ExecuteTicket,
		ConfirmRequest,
		ApplyRequest,
		Review,
		bExecuteDispatchConfirmed,
		Request));
	return Request;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorStageGExecuteDispatchReceiptReadyTest,
	"HCIAbilityKit.Editor.AgentExecutorStageGExecuteDispatchReceipt.ReadyWhenStageGExecutePermitTicketReady",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorStageGExecuteDispatchReceiptReadyTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeG5SelectedReviewReport();
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeG5ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeG5ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAbilityKitAgentExecuteTicket ExecuteTicket = MakeG5ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteReceipt Receipt = MakeG5Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteFinalReport FinalReport = MakeG5FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeG5ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = MakeG5HandoffEnvelope(ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGExecuteIntent Intent = MakeG5StageGIntent(HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGWriteEnableRequest WriteEnableRequest = MakeG5WriteEnableRequest(Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAbilityKitAgentStageGExecutePermitTicket PermitTicket = MakeG5PermitTicket(WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGExecuteDispatchRequest DispatchRequest = MakeG5DispatchRequest(PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);

	FHCIAbilityKitAgentStageGExecuteDispatchReceipt Request;
	TestTrue(TEXT("Build stage g execute dispatch receipt"), FHCIAbilityKitAgentExecutorStageGExecuteDispatchReceiptBridge::BuildStageGExecuteDispatchReceipt(
		DispatchRequest,
		DispatchRequest.RequestId,
		PermitTicket,
		WriteEnableRequest,
		Intent,
		HandoffEnvelope,
		ArchiveBundle,
		FinalReport,
		Receipt,
		ExecuteTicket,
		ConfirmRequest,
		ApplyRequest,
		Review,
		Request));
	TestTrue(TEXT("DispatchReady"), Request.bStageGExecuteDispatchReceiptReady);
	TestTrue(TEXT("DispatchAccepted"), Request.bStageGExecuteDispatchAccepted);
	TestTrue(TEXT("WriteEnabled"), Request.bWriteEnabled);
	TestEqual(TEXT("DispatchStatus"), Request.StageGExecuteDispatchReceiptStatus, FString(TEXT("accepted")));
	TestEqual(TEXT("ErrorCode"), Request.ErrorCode, FString(TEXT("-")));
	TestEqual(TEXT("Reason"), Request.Reason, FString(TEXT("stage_g_execute_dispatch_receipt_ready")));
	TestFalse(TEXT("DispatchDigest present"), Request.StageGExecuteDispatchReceiptDigest.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorStageGExecuteDispatchReceiptWriteEnableNotReadyTest,
	"HCIAbilityKit.Editor.AgentExecutorStageGExecuteDispatchReceipt.StageGExecuteDispatchRequestNotReadyReturnsE4213",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorStageGExecuteDispatchReceiptWriteEnableNotReadyTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeG5SelectedReviewReport();
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeG5ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeG5ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAbilityKitAgentExecuteTicket ExecuteTicket = MakeG5ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteReceipt Receipt = MakeG5Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteFinalReport FinalReport = MakeG5FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeG5ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = MakeG5HandoffEnvelope(ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGExecuteIntent Intent = MakeG5StageGIntent(HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGWriteEnableRequest WriteEnableRequest = MakeG5WriteEnableRequest(Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAbilityKitAgentStageGExecutePermitTicket PermitTicket = MakeG5PermitTicket(WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	FHCIAbilityKitAgentStageGExecuteDispatchRequest DispatchRequest = MakeG5DispatchRequest(PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	DispatchRequest.bWriteEnabled = false;
	DispatchRequest.bReadyForStageGExecute = false;
	DispatchRequest.bStageGExecuteDispatchReady = false;
	DispatchRequest.StageGExecuteDispatchStatus = TEXT("blocked");

	FHCIAbilityKitAgentStageGExecuteDispatchReceipt Request;
	TestTrue(TEXT("Build stage g execute dispatch receipt"), FHCIAbilityKitAgentExecutorStageGExecuteDispatchReceiptBridge::BuildStageGExecuteDispatchReceipt(
		DispatchRequest,
		DispatchRequest.RequestId,
		PermitTicket,
		WriteEnableRequest,
		Intent,
		HandoffEnvelope,
		ArchiveBundle,
		FinalReport,
		Receipt,
		ExecuteTicket,
		ConfirmRequest,
		ApplyRequest,
		Review,
		Request));
	TestFalse(TEXT("DispatchReady"), Request.bStageGExecuteDispatchReceiptReady);
	TestEqual(TEXT("DispatchStatus"), Request.StageGExecuteDispatchReceiptStatus, FString(TEXT("blocked")));
	TestEqual(TEXT("ErrorCode"), Request.ErrorCode, FString(TEXT("E4213")));
	TestEqual(TEXT("Reason"), Request.Reason, FString(TEXT("stage_g_execute_dispatch_request_not_ready")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorStageGExecuteDispatchReceiptDigestMismatchTest,
	"HCIAbilityKit.Editor.AgentExecutorStageGExecuteDispatchReceipt.DigestMismatchReturnsE4202",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorStageGExecuteDispatchReceiptDigestMismatchTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeG5SelectedReviewReport();
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeG5ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeG5ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAbilityKitAgentExecuteTicket ExecuteTicket = MakeG5ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteReceipt Receipt = MakeG5Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteFinalReport FinalReport = MakeG5FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeG5ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = MakeG5HandoffEnvelope(ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGExecuteIntent Intent = MakeG5StageGIntent(HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGWriteEnableRequest WriteEnableRequest = MakeG5WriteEnableRequest(Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	FHCIAbilityKitAgentStageGExecutePermitTicket PermitTicket = MakeG5PermitTicket(WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	FHCIAbilityKitAgentStageGExecuteDispatchRequest DispatchRequest = MakeG5DispatchRequest(PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	DispatchRequest.SelectionDigest = TEXT("crc32_BAD0C0DE");

	FHCIAbilityKitAgentStageGExecuteDispatchReceipt Request;
	TestTrue(TEXT("Build stage g execute dispatch receipt"), FHCIAbilityKitAgentExecutorStageGExecuteDispatchReceiptBridge::BuildStageGExecuteDispatchReceipt(
		DispatchRequest,
		DispatchRequest.RequestId,
		PermitTicket,
		WriteEnableRequest,
		Intent,
		HandoffEnvelope,
		ArchiveBundle,
		FinalReport,
		Receipt,
		ExecuteTicket,
		ConfirmRequest,
		ApplyRequest,
		Review,
		Request));
	TestFalse(TEXT("DispatchReady"), Request.bStageGExecuteDispatchReceiptReady);
	TestEqual(TEXT("ErrorCode"), Request.ErrorCode, FString(TEXT("E4202")));
	TestEqual(TEXT("Reason"), Request.Reason, FString(TEXT("selection_digest_mismatch")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorStageGExecuteDispatchReceiptJsonFieldsTest,
	"HCIAbilityKit.Editor.AgentExecutorStageGExecuteDispatchReceipt.JsonIncludesDispatchFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorStageGExecuteDispatchReceiptJsonFieldsTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeG5SelectedReviewReport();
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeG5ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeG5ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAbilityKitAgentExecuteTicket ExecuteTicket = MakeG5ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteReceipt Receipt = MakeG5Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteFinalReport FinalReport = MakeG5FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeG5ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = MakeG5HandoffEnvelope(ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGExecuteIntent Intent = MakeG5StageGIntent(HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGWriteEnableRequest WriteEnableRequest = MakeG5WriteEnableRequest(Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAbilityKitAgentStageGExecutePermitTicket PermitTicket = MakeG5PermitTicket(WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGExecuteDispatchRequest DispatchRequest = MakeG5DispatchRequest(PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);

	FHCIAbilityKitAgentStageGExecuteDispatchReceipt Request;
	TestTrue(TEXT("Build stage g execute dispatch receipt"), FHCIAbilityKitAgentExecutorStageGExecuteDispatchReceiptBridge::BuildStageGExecuteDispatchReceipt(
		DispatchRequest,
		DispatchRequest.RequestId,
		PermitTicket,
		WriteEnableRequest,
		Intent,
		HandoffEnvelope,
		ArchiveBundle,
		FinalReport,
		Receipt,
		ExecuteTicket,
		ConfirmRequest,
		ApplyRequest,
		Review,
		Request));

	FString JsonText;
	TestTrue(TEXT("Serialize stage g execute dispatch receipt json"), FHCIAbilityKitAgentStageGExecuteDispatchReceiptJsonSerializer::SerializeToJsonString(Request, JsonText));
	TestTrue(TEXT("JSON includes stage_g_execute_permit_ticket_id"), JsonText.Contains(TEXT("\"stage_g_execute_permit_ticket_id\"")));
	TestTrue(TEXT("JSON includes stage_g_write_enable_request_id"), JsonText.Contains(TEXT("\"stage_g_write_enable_request_id\"")));
	TestTrue(TEXT("JSON includes stage_g_write_enable_digest"), JsonText.Contains(TEXT("\"stage_g_write_enable_digest\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_permit_digest"), JsonText.Contains(TEXT("\"stage_g_execute_permit_digest\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_dispatch_receipt_digest"), JsonText.Contains(TEXT("\"stage_g_execute_dispatch_receipt_digest\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_dispatch_receipt_status"), JsonText.Contains(TEXT("\"stage_g_execute_dispatch_receipt_status\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_dispatch_accepted"), JsonText.Contains(TEXT("\"stage_g_execute_dispatch_accepted\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_dispatch_receipt_ready"), JsonText.Contains(TEXT("\"stage_g_execute_dispatch_receipt_ready\"")));
	TestTrue(TEXT("JSON includes locate_strategy"), JsonText.Contains(TEXT("\"locate_strategy\"")));
	return true;
}

#endif



