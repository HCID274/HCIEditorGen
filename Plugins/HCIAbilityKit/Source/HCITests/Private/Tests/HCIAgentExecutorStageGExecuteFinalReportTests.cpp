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
#include "Agent/Bridges/HCIAgentExecutorStageGExecuteDispatchRequestBridge.h"
#include "Agent/Bridges/HCIAgentExecutorStageGExecuteDispatchReceiptBridge.h"
#include "Agent/Bridges/HCIAgentExecutorStageGExecuteCommitRequestBridge.h"
#include "Agent/Bridges/HCIAgentExecutorStageGExecuteCommitReceiptBridge.h"
#include "Agent/Bridges/HCIAgentExecutorStageGExecuteFinalReportBridge.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteArchiveBundle.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteFinalReport.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteHandoffEnvelope.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteReceipt.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecuteIntent.h"
#include "Agent/Contracts/StageG/HCIAgentStageGWriteEnableRequest.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecutePermitTicket.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecuteDispatchRequest.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecuteDispatchReceipt.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecuteCommitRequest.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecuteCommitReceipt.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecuteFinalReport.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecuteFinalReportJsonSerializer.h"
#include "Agent/Executor/HCIDryRunDiff.h"
#include "Misc/AutomationTest.h"

namespace
{
static FHCIDryRunDiffReport MakeG8SelectedReviewReport()
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

static FHCIAgentApplyRequest MakeG8ApplyRequest(const FHCIDryRunDiffReport& Review)
{
	FHCIAgentApplyRequest ApplyRequest;
	check(FHCIAgentExecutorApplyRequestBridge::BuildApplyRequest(Review, ApplyRequest));
	return ApplyRequest;
}

static FHCIAgentApplyConfirmRequest MakeG8ConfirmRequest(
	const FHCIAgentApplyRequest& ApplyRequest,
	const FHCIDryRunDiffReport& Review,
	const bool bUserConfirmed)
{
	FHCIAgentApplyConfirmRequest ConfirmRequest;
	check(FHCIAgentExecutorApplyConfirmBridge::BuildConfirmRequest(ApplyRequest, Review, bUserConfirmed, ConfirmRequest));
	return ConfirmRequest;
}

static FHCIAgentExecuteTicket MakeG8ExecuteTicket(
	const FHCIAgentApplyConfirmRequest& ConfirmRequest,
	const FHCIAgentApplyRequest& ApplyRequest,
	const FHCIDryRunDiffReport& Review)
{
	FHCIAgentExecuteTicket ExecuteTicket;
	check(FHCIAgentExecutorExecuteTicketBridge::BuildExecuteTicket(ConfirmRequest, ApplyRequest, Review, ExecuteTicket));
	return ExecuteTicket;
}

static FHCIAgentSimulateExecuteReceipt MakeG8Receipt(
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

static FHCIAgentSimulateExecuteFinalReport MakeG8FinalReport(
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

static FHCIAgentSimulateExecuteArchiveBundle MakeG8ArchiveBundle(
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

static FHCIAgentSimulateExecuteHandoffEnvelope MakeG8HandoffEnvelope(
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

static FHCIAgentStageGExecuteIntent MakeG8StageGIntent(
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

static FHCIAgentStageGWriteEnableRequest MakeG8WriteEnableRequest(
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

static FHCIAgentStageGExecutePermitTicket MakeG8PermitTicket(
	const FHCIAgentStageGWriteEnableRequest& WriteEnableRequest,
	const FHCIAgentStageGExecuteIntent& Intent,
	const FHCIAgentSimulateExecuteHandoffEnvelope& HandoffEnvelope,
	const FHCIAgentSimulateExecuteArchiveBundle& ArchiveBundle,
	const FHCIAgentSimulateExecuteFinalReport& FinalReport,
	const FHCIAgentSimulateExecuteReceipt& Receipt,
	const FHCIAgentExecuteTicket& ExecuteTicket,
	const FHCIAgentApplyConfirmRequest& ConfirmRequest,
	const FHCIAgentApplyRequest& ApplyRequest,
	const FHCIDryRunDiffReport& Review)
{
	FHCIAgentStageGExecutePermitTicket Ticket;
	check(FHCIAgentExecutorStageGExecutePermitTicketBridge::BuildStageGExecutePermitTicket(
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

static FHCIAgentStageGExecuteDispatchRequest MakeG8DispatchRequest(
	const FHCIAgentStageGExecutePermitTicket& PermitTicket,
	const FHCIAgentStageGWriteEnableRequest& WriteEnableRequest,
	const FHCIAgentStageGExecuteIntent& Intent,
	const FHCIAgentSimulateExecuteHandoffEnvelope& HandoffEnvelope,
	const FHCIAgentSimulateExecuteArchiveBundle& ArchiveBundle,
	const FHCIAgentSimulateExecuteFinalReport& FinalReport,
	const FHCIAgentSimulateExecuteReceipt& Receipt,
	const FHCIAgentExecuteTicket& ExecuteTicket,
	const FHCIAgentApplyConfirmRequest& ConfirmRequest,
	const FHCIAgentApplyRequest& ApplyRequest,
	const FHCIDryRunDiffReport& Review,
	const bool bExecuteDispatchConfirmed)
{
	FHCIAgentStageGExecuteDispatchRequest Request;
	check(FHCIAgentExecutorStageGExecuteDispatchRequestBridge::BuildStageGExecuteDispatchRequest(
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

static FHCIAgentStageGExecuteDispatchReceipt MakeG8DispatchReceipt(
	const FHCIAgentStageGExecuteDispatchRequest& DispatchRequest,
	const FHCIAgentStageGExecutePermitTicket& PermitTicket,
	const FHCIAgentStageGWriteEnableRequest& WriteEnableRequest,
	const FHCIAgentStageGExecuteIntent& Intent,
	const FHCIAgentSimulateExecuteHandoffEnvelope& HandoffEnvelope,
	const FHCIAgentSimulateExecuteArchiveBundle& ArchiveBundle,
	const FHCIAgentSimulateExecuteFinalReport& FinalReport,
	const FHCIAgentSimulateExecuteReceipt& Receipt,
	const FHCIAgentExecuteTicket& ExecuteTicket,
	const FHCIAgentApplyConfirmRequest& ConfirmRequest,
	const FHCIAgentApplyRequest& ApplyRequest,
	const FHCIDryRunDiffReport& Review)
{
	FHCIAgentStageGExecuteDispatchReceipt Request;
	check(FHCIAgentExecutorStageGExecuteDispatchReceiptBridge::BuildStageGExecuteDispatchReceipt(
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
	return Request;
}

static FHCIAgentStageGExecuteCommitRequest MakeG8CommitRequest(
	const FHCIAgentStageGExecuteDispatchReceipt& DispatchReceipt,
	const FHCIAgentStageGExecuteDispatchRequest& DispatchRequest,
	const FHCIAgentStageGExecutePermitTicket& PermitTicket,
	const FHCIAgentStageGWriteEnableRequest& WriteEnableRequest,
	const FHCIAgentStageGExecuteIntent& Intent,
	const FHCIAgentSimulateExecuteHandoffEnvelope& HandoffEnvelope,
	const FHCIAgentSimulateExecuteArchiveBundle& ArchiveBundle,
	const FHCIAgentSimulateExecuteFinalReport& FinalReport,
	const FHCIAgentSimulateExecuteReceipt& Receipt,
	const FHCIAgentExecuteTicket& ExecuteTicket,
	const FHCIAgentApplyConfirmRequest& ConfirmRequest,
	const FHCIAgentApplyRequest& ApplyRequest,
	const FHCIDryRunDiffReport& Review,
	const bool bExecuteCommitConfirmed)
{
	FHCIAgentStageGExecuteCommitRequest Request;
	check(FHCIAgentExecutorStageGExecuteCommitRequestBridge::BuildStageGExecuteCommitRequest(
		DispatchReceipt,
		DispatchReceipt.RequestId,
		DispatchRequest,
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
		bExecuteCommitConfirmed,
		Request));
	return Request;
}

static FHCIAgentStageGExecuteCommitReceipt MakeG8CommitReceipt(
	const FHCIAgentStageGExecuteCommitRequest& CommitRequest,
	const FHCIAgentStageGExecuteDispatchReceipt& DispatchReceipt,
	const FHCIAgentStageGExecuteDispatchRequest& DispatchRequest,
	const FHCIAgentStageGExecutePermitTicket& PermitTicket,
	const FHCIAgentStageGWriteEnableRequest& WriteEnableRequest,
	const FHCIAgentStageGExecuteIntent& Intent,
	const FHCIAgentSimulateExecuteHandoffEnvelope& HandoffEnvelope,
	const FHCIAgentSimulateExecuteArchiveBundle& ArchiveBundle,
	const FHCIAgentSimulateExecuteFinalReport& FinalReport,
	const FHCIAgentSimulateExecuteReceipt& Receipt,
	const FHCIAgentExecuteTicket& ExecuteTicket,
	const FHCIAgentApplyConfirmRequest& ConfirmRequest,
	const FHCIAgentApplyRequest& ApplyRequest,
	const FHCIDryRunDiffReport& Review)
{
	FHCIAgentStageGExecuteCommitReceipt CommitReceipt;
	check(FHCIAgentExecutorStageGExecuteCommitReceiptBridge::BuildStageGExecuteCommitReceipt(
		CommitRequest,
		CommitRequest.RequestId,
		DispatchReceipt,
		DispatchRequest,
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
		CommitReceipt));
	return CommitReceipt;
}
} // namespace

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorStageGExecuteFinalReportReadyTest,
	"HCI.Editor.AgentExecutorStageGExecuteFinalReport.ReadyWhenStageGExecuteCommitReceiptReady",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorStageGExecuteFinalReportReadyTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeG8SelectedReviewReport();
	const FHCIAgentApplyRequest ApplyRequest = MakeG8ApplyRequest(Review);
	const FHCIAgentApplyConfirmRequest ConfirmRequest = MakeG8ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAgentExecuteTicket ExecuteTicket = MakeG8ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteReceipt Receipt = MakeG8Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteFinalReport FinalReport = MakeG8FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeG8ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = MakeG8HandoffEnvelope(ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGExecuteIntent Intent = MakeG8StageGIntent(HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGWriteEnableRequest WriteEnableRequest = MakeG8WriteEnableRequest(Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAgentStageGExecutePermitTicket PermitTicket = MakeG8PermitTicket(WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGExecuteDispatchRequest DispatchRequest = MakeG8DispatchRequest(PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAgentStageGExecuteDispatchReceipt DispatchReceipt = MakeG8DispatchReceipt(DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGExecuteCommitRequest CommitRequest = MakeG8CommitRequest(
		DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAgentStageGExecuteCommitReceipt CommitReceipt = MakeG8CommitReceipt(
		CommitRequest, DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);

	FHCIAgentStageGExecuteFinalReport Request;
	TestTrue(TEXT("Build stage g execute final report"), FHCIAgentExecutorStageGExecuteFinalReportBridge::BuildStageGExecuteFinalReport(
		CommitReceipt,
		CommitReceipt.RequestId,
		CommitRequest.RequestId,
		CommitRequest.StageGExecuteCommitRequestDigest,
		DispatchReceipt,
		DispatchRequest,
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
	TestTrue(TEXT("FinalReportReady"), Request.bStageGExecuteFinalReportReady);
	TestTrue(TEXT("Finalized"), Request.bStageGExecuteFinalized);
	TestTrue(TEXT("CommitConfirmed"), Request.bExecuteCommitConfirmed);
	TestTrue(TEXT("WriteEnabled"), Request.bWriteEnabled);
	TestEqual(TEXT("FinalReportStatus"), Request.StageGExecuteFinalReportStatus, FString(TEXT("completed")));
	TestEqual(TEXT("ErrorCode"), Request.ErrorCode, FString(TEXT("-")));
	TestEqual(TEXT("Reason"), Request.Reason, FString(TEXT("stage_g_execute_final_report_ready")));
	TestFalse(TEXT("CommitDigest present"), Request.StageGExecuteFinalReportDigest.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorStageGExecuteFinalReportWriteEnableNotReadyTest,
	"HCI.Editor.AgentExecutorStageGExecuteFinalReport.StageGExecuteCommitReceiptNotReadyReturnsE4216",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorStageGExecuteFinalReportWriteEnableNotReadyTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeG8SelectedReviewReport();
	const FHCIAgentApplyRequest ApplyRequest = MakeG8ApplyRequest(Review);
	const FHCIAgentApplyConfirmRequest ConfirmRequest = MakeG8ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAgentExecuteTicket ExecuteTicket = MakeG8ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteReceipt Receipt = MakeG8Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteFinalReport FinalReport = MakeG8FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeG8ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = MakeG8HandoffEnvelope(ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGExecuteIntent Intent = MakeG8StageGIntent(HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGWriteEnableRequest WriteEnableRequest = MakeG8WriteEnableRequest(Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAgentStageGExecutePermitTicket PermitTicket = MakeG8PermitTicket(WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGExecuteDispatchRequest DispatchRequest = MakeG8DispatchRequest(PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAgentStageGExecuteDispatchReceipt DispatchReceipt = MakeG8DispatchReceipt(DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	FHCIAgentStageGExecuteCommitRequest CommitRequest = MakeG8CommitRequest(
		DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	FHCIAgentStageGExecuteCommitReceipt CommitReceipt = MakeG8CommitReceipt(
		CommitRequest, DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	CommitReceipt.bWriteEnabled = false;
	CommitReceipt.bReadyForStageGExecute = false;
	CommitReceipt.bStageGExecuteCommitAccepted = false;
	CommitReceipt.bStageGExecuteCommitReceiptReady = false;
	CommitReceipt.StageGExecuteCommitReceiptStatus = TEXT("blocked");

	FHCIAgentStageGExecuteFinalReport Request;
	TestTrue(TEXT("Build stage g execute final report"), FHCIAgentExecutorStageGExecuteFinalReportBridge::BuildStageGExecuteFinalReport(
		CommitReceipt,
		CommitReceipt.RequestId,
		CommitRequest.RequestId,
		CommitRequest.StageGExecuteCommitRequestDigest,
		DispatchReceipt,
		DispatchRequest,
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
	TestFalse(TEXT("FinalReportReady"), Request.bStageGExecuteFinalReportReady);
	TestEqual(TEXT("FinalReportStatus"), Request.StageGExecuteFinalReportStatus, FString(TEXT("blocked")));
	TestEqual(TEXT("ErrorCode"), Request.ErrorCode, FString(TEXT("E4216")));
	TestEqual(TEXT("Reason"), Request.Reason, FString(TEXT("stage_g_execute_commit_receipt_not_ready")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorStageGExecuteFinalReportDigestMismatchTest,
	"HCI.Editor.AgentExecutorStageGExecuteFinalReport.DigestMismatchReturnsE4202",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorStageGExecuteFinalReportDigestMismatchTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeG8SelectedReviewReport();
	const FHCIAgentApplyRequest ApplyRequest = MakeG8ApplyRequest(Review);
	const FHCIAgentApplyConfirmRequest ConfirmRequest = MakeG8ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAgentExecuteTicket ExecuteTicket = MakeG8ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteReceipt Receipt = MakeG8Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteFinalReport FinalReport = MakeG8FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeG8ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = MakeG8HandoffEnvelope(ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGExecuteIntent Intent = MakeG8StageGIntent(HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGWriteEnableRequest WriteEnableRequest = MakeG8WriteEnableRequest(Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAgentStageGExecutePermitTicket PermitTicket = MakeG8PermitTicket(WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGExecuteDispatchRequest DispatchRequest = MakeG8DispatchRequest(PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAgentStageGExecuteDispatchReceipt DispatchReceipt = MakeG8DispatchReceipt(DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	FHCIAgentStageGExecuteCommitRequest CommitRequest = MakeG8CommitRequest(
		DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	FHCIAgentStageGExecuteCommitReceipt CommitReceipt = MakeG8CommitReceipt(
		CommitRequest, DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	CommitReceipt.SelectionDigest = TEXT("crc32_BAD0C0DE");

	FHCIAgentStageGExecuteFinalReport Request;
	TestTrue(TEXT("Build stage g execute final report"), FHCIAgentExecutorStageGExecuteFinalReportBridge::BuildStageGExecuteFinalReport(
		CommitReceipt,
		CommitReceipt.RequestId,
		CommitRequest.RequestId,
		CommitRequest.StageGExecuteCommitRequestDigest,
		DispatchReceipt,
		DispatchRequest,
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
	TestFalse(TEXT("FinalReportReady"), Request.bStageGExecuteFinalReportReady);
	TestEqual(TEXT("ErrorCode"), Request.ErrorCode, FString(TEXT("E4202")));
	TestEqual(TEXT("Reason"), Request.Reason, FString(TEXT("selection_digest_mismatch")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorStageGExecuteFinalReportJsonFieldsTest,
	"HCI.Editor.AgentExecutorStageGExecuteFinalReport.JsonIncludesFinalReportFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorStageGExecuteFinalReportJsonFieldsTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeG8SelectedReviewReport();
	const FHCIAgentApplyRequest ApplyRequest = MakeG8ApplyRequest(Review);
	const FHCIAgentApplyConfirmRequest ConfirmRequest = MakeG8ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAgentExecuteTicket ExecuteTicket = MakeG8ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteReceipt Receipt = MakeG8Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteFinalReport FinalReport = MakeG8FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeG8ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = MakeG8HandoffEnvelope(ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGExecuteIntent Intent = MakeG8StageGIntent(HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGWriteEnableRequest WriteEnableRequest = MakeG8WriteEnableRequest(Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAgentStageGExecutePermitTicket PermitTicket = MakeG8PermitTicket(WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGExecuteDispatchRequest DispatchRequest = MakeG8DispatchRequest(PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAgentStageGExecuteDispatchReceipt DispatchReceipt = MakeG8DispatchReceipt(DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGExecuteCommitRequest CommitRequest = MakeG8CommitRequest(
		DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAgentStageGExecuteCommitReceipt CommitReceipt = MakeG8CommitReceipt(
		CommitRequest, DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);

	FHCIAgentStageGExecuteFinalReport Request;
	TestTrue(TEXT("Build stage g execute final report"), FHCIAgentExecutorStageGExecuteFinalReportBridge::BuildStageGExecuteFinalReport(
		CommitReceipt,
		CommitReceipt.RequestId,
		CommitRequest.RequestId,
		CommitRequest.StageGExecuteCommitRequestDigest,
		DispatchReceipt,
		DispatchRequest,
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
	TestTrue(TEXT("Serialize stage g execute final report json"), FHCIAgentStageGExecuteFinalReportJsonSerializer::SerializeToJsonString(Request, JsonText));
	TestTrue(TEXT("JSON includes stage_g_execute_commit_request_id"), JsonText.Contains(TEXT("\"stage_g_execute_commit_request_id\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_dispatch_receipt_id"), JsonText.Contains(TEXT("\"stage_g_execute_dispatch_receipt_id\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_permit_ticket_id"), JsonText.Contains(TEXT("\"stage_g_execute_permit_ticket_id\"")));
	TestTrue(TEXT("JSON includes stage_g_write_enable_request_id"), JsonText.Contains(TEXT("\"stage_g_write_enable_request_id\"")));
	TestTrue(TEXT("JSON includes stage_g_write_enable_digest"), JsonText.Contains(TEXT("\"stage_g_write_enable_digest\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_permit_digest"), JsonText.Contains(TEXT("\"stage_g_execute_permit_digest\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_dispatch_digest"), JsonText.Contains(TEXT("\"stage_g_execute_dispatch_digest\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_dispatch_receipt_digest"), JsonText.Contains(TEXT("\"stage_g_execute_dispatch_receipt_digest\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_commit_request_digest"), JsonText.Contains(TEXT("\"stage_g_execute_commit_request_digest\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_commit_request_status"), JsonText.Contains(TEXT("\"stage_g_execute_commit_request_status\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_commit_receipt_id"), JsonText.Contains(TEXT("\"stage_g_execute_commit_receipt_id\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_commit_receipt_digest"), JsonText.Contains(TEXT("\"stage_g_execute_commit_receipt_digest\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_commit_receipt_status"), JsonText.Contains(TEXT("\"stage_g_execute_commit_receipt_status\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_final_report_digest"), JsonText.Contains(TEXT("\"stage_g_execute_final_report_digest\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_final_report_status"), JsonText.Contains(TEXT("\"stage_g_execute_final_report_status\"")));
	TestTrue(TEXT("JSON includes execute_commit_confirmed"), JsonText.Contains(TEXT("\"execute_commit_confirmed\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_commit_request_ready"), JsonText.Contains(TEXT("\"stage_g_execute_commit_request_ready\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_commit_accepted"), JsonText.Contains(TEXT("\"stage_g_execute_commit_accepted\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_commit_receipt_ready"), JsonText.Contains(TEXT("\"stage_g_execute_commit_receipt_ready\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_finalized"), JsonText.Contains(TEXT("\"stage_g_execute_finalized\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_final_report_ready"), JsonText.Contains(TEXT("\"stage_g_execute_final_report_ready\"")));
	TestTrue(TEXT("JSON includes locate_strategy"), JsonText.Contains(TEXT("\"locate_strategy\"")));
	return true;
}

#endif





