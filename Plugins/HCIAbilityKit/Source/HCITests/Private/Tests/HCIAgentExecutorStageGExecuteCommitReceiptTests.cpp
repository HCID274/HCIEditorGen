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
#include "Agent/Contracts/StageG/HCIAgentStageGExecuteCommitReceiptJsonSerializer.h"
#include "Agent/Executor/HCIDryRunDiff.h"
#include "Misc/AutomationTest.h"

namespace
{
static FHCIDryRunDiffReport MakeG7SelectedReviewReport()
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

static FHCIAgentApplyRequest MakeG7ApplyRequest(const FHCIDryRunDiffReport& Review)
{
	FHCIAgentApplyRequest ApplyRequest;
	check(FHCIAgentExecutorApplyRequestBridge::BuildApplyRequest(Review, ApplyRequest));
	return ApplyRequest;
}

static FHCIAgentApplyConfirmRequest MakeG7ConfirmRequest(
	const FHCIAgentApplyRequest& ApplyRequest,
	const FHCIDryRunDiffReport& Review,
	const bool bUserConfirmed)
{
	FHCIAgentApplyConfirmRequest ConfirmRequest;
	check(FHCIAgentExecutorApplyConfirmBridge::BuildConfirmRequest(ApplyRequest, Review, bUserConfirmed, ConfirmRequest));
	return ConfirmRequest;
}

static FHCIAgentExecuteTicket MakeG7ExecuteTicket(
	const FHCIAgentApplyConfirmRequest& ConfirmRequest,
	const FHCIAgentApplyRequest& ApplyRequest,
	const FHCIDryRunDiffReport& Review)
{
	FHCIAgentExecuteTicket ExecuteTicket;
	check(FHCIAgentExecutorExecuteTicketBridge::BuildExecuteTicket(ConfirmRequest, ApplyRequest, Review, ExecuteTicket));
	return ExecuteTicket;
}

static FHCIAgentSimulateExecuteReceipt MakeG7Receipt(
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

static FHCIAgentSimulateExecuteFinalReport MakeG7FinalReport(
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

static FHCIAgentSimulateExecuteArchiveBundle MakeG7ArchiveBundle(
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

static FHCIAgentSimulateExecuteHandoffEnvelope MakeG7HandoffEnvelope(
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

static FHCIAgentStageGExecuteIntent MakeG7StageGIntent(
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

static FHCIAgentStageGWriteEnableRequest MakeG7WriteEnableRequest(
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

static FHCIAgentStageGExecutePermitTicket MakeG7PermitTicket(
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

static FHCIAgentStageGExecuteDispatchRequest MakeG7DispatchRequest(
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

static FHCIAgentStageGExecuteDispatchReceipt MakeG7DispatchReceipt(
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

static FHCIAgentStageGExecuteCommitRequest MakeG7CommitRequest(
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
} // namespace

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorStageGExecuteCommitReceiptReadyTest,
	"HCI.Editor.AgentExecutorStageGExecuteCommitReceipt.ReadyWhenStageGExecuteDispatchReceiptReady",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorStageGExecuteCommitReceiptReadyTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeG7SelectedReviewReport();
	const FHCIAgentApplyRequest ApplyRequest = MakeG7ApplyRequest(Review);
	const FHCIAgentApplyConfirmRequest ConfirmRequest = MakeG7ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAgentExecuteTicket ExecuteTicket = MakeG7ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteReceipt Receipt = MakeG7Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteFinalReport FinalReport = MakeG7FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeG7ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = MakeG7HandoffEnvelope(ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGExecuteIntent Intent = MakeG7StageGIntent(HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGWriteEnableRequest WriteEnableRequest = MakeG7WriteEnableRequest(Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAgentStageGExecutePermitTicket PermitTicket = MakeG7PermitTicket(WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGExecuteDispatchRequest DispatchRequest = MakeG7DispatchRequest(PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAgentStageGExecuteDispatchReceipt DispatchReceipt = MakeG7DispatchReceipt(DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGExecuteCommitRequest CommitRequest = MakeG7CommitRequest(
		DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);

	FHCIAgentStageGExecuteCommitReceipt Request;
	TestTrue(TEXT("Build stage g execute commit receipt"), FHCIAgentExecutorStageGExecuteCommitReceiptBridge::BuildStageGExecuteCommitReceipt(
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
		Request));
	TestTrue(TEXT("CommitReady"), Request.bStageGExecuteCommitReceiptReady);
	TestTrue(TEXT("CommitAccepted"), Request.bStageGExecuteCommitAccepted);
	TestTrue(TEXT("CommitConfirmed"), Request.bExecuteCommitConfirmed);
	TestTrue(TEXT("WriteEnabled"), Request.bWriteEnabled);
	TestEqual(TEXT("CommitStatus"), Request.StageGExecuteCommitReceiptStatus, FString(TEXT("accepted")));
	TestEqual(TEXT("ErrorCode"), Request.ErrorCode, FString(TEXT("-")));
	TestEqual(TEXT("Reason"), Request.Reason, FString(TEXT("stage_g_execute_commit_receipt_ready")));
	TestFalse(TEXT("CommitDigest present"), Request.StageGExecuteCommitReceiptDigest.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorStageGExecuteCommitReceiptWriteEnableNotReadyTest,
	"HCI.Editor.AgentExecutorStageGExecuteCommitReceipt.StageGExecuteDispatchReceiptNotReadyReturnsE4214",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorStageGExecuteCommitReceiptWriteEnableNotReadyTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeG7SelectedReviewReport();
	const FHCIAgentApplyRequest ApplyRequest = MakeG7ApplyRequest(Review);
	const FHCIAgentApplyConfirmRequest ConfirmRequest = MakeG7ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAgentExecuteTicket ExecuteTicket = MakeG7ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteReceipt Receipt = MakeG7Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteFinalReport FinalReport = MakeG7FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeG7ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = MakeG7HandoffEnvelope(ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGExecuteIntent Intent = MakeG7StageGIntent(HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGWriteEnableRequest WriteEnableRequest = MakeG7WriteEnableRequest(Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAgentStageGExecutePermitTicket PermitTicket = MakeG7PermitTicket(WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGExecuteDispatchRequest DispatchRequest = MakeG7DispatchRequest(PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAgentStageGExecuteDispatchReceipt DispatchReceipt = MakeG7DispatchReceipt(DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	FHCIAgentStageGExecuteCommitRequest CommitRequest = MakeG7CommitRequest(
		DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	CommitRequest.bWriteEnabled = false;
	CommitRequest.bReadyForStageGExecute = false;
	CommitRequest.bStageGExecuteCommitRequestReady = false;
	CommitRequest.StageGExecuteCommitRequestStatus = TEXT("blocked");

	FHCIAgentStageGExecuteCommitReceipt Request;
	TestTrue(TEXT("Build stage g execute commit receipt"), FHCIAgentExecutorStageGExecuteCommitReceiptBridge::BuildStageGExecuteCommitReceipt(
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
		Request));
	TestFalse(TEXT("CommitReady"), Request.bStageGExecuteCommitReceiptReady);
	TestEqual(TEXT("CommitStatus"), Request.StageGExecuteCommitReceiptStatus, FString(TEXT("blocked")));
	TestEqual(TEXT("ErrorCode"), Request.ErrorCode, FString(TEXT("E4215")));
	TestEqual(TEXT("Reason"), Request.Reason, FString(TEXT("stage_g_execute_commit_request_not_ready")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorStageGExecuteCommitReceiptDigestMismatchTest,
	"HCI.Editor.AgentExecutorStageGExecuteCommitReceipt.DigestMismatchReturnsE4202",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorStageGExecuteCommitReceiptDigestMismatchTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeG7SelectedReviewReport();
	const FHCIAgentApplyRequest ApplyRequest = MakeG7ApplyRequest(Review);
	const FHCIAgentApplyConfirmRequest ConfirmRequest = MakeG7ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAgentExecuteTicket ExecuteTicket = MakeG7ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteReceipt Receipt = MakeG7Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteFinalReport FinalReport = MakeG7FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeG7ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = MakeG7HandoffEnvelope(ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGExecuteIntent Intent = MakeG7StageGIntent(HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGWriteEnableRequest WriteEnableRequest = MakeG7WriteEnableRequest(Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAgentStageGExecutePermitTicket PermitTicket = MakeG7PermitTicket(WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGExecuteDispatchRequest DispatchRequest = MakeG7DispatchRequest(PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAgentStageGExecuteDispatchReceipt DispatchReceipt = MakeG7DispatchReceipt(DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	FHCIAgentStageGExecuteCommitRequest CommitRequest = MakeG7CommitRequest(
		DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	CommitRequest.SelectionDigest = TEXT("crc32_BAD0C0DE");

	FHCIAgentStageGExecuteCommitReceipt Request;
	TestTrue(TEXT("Build stage g execute commit receipt"), FHCIAgentExecutorStageGExecuteCommitReceiptBridge::BuildStageGExecuteCommitReceipt(
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
		Request));
	TestFalse(TEXT("CommitReady"), Request.bStageGExecuteCommitReceiptReady);
	TestEqual(TEXT("ErrorCode"), Request.ErrorCode, FString(TEXT("E4202")));
	TestEqual(TEXT("Reason"), Request.Reason, FString(TEXT("selection_digest_mismatch")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorStageGExecuteCommitReceiptJsonFieldsTest,
	"HCI.Editor.AgentExecutorStageGExecuteCommitReceipt.JsonIncludesCommitFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorStageGExecuteCommitReceiptJsonFieldsTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeG7SelectedReviewReport();
	const FHCIAgentApplyRequest ApplyRequest = MakeG7ApplyRequest(Review);
	const FHCIAgentApplyConfirmRequest ConfirmRequest = MakeG7ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAgentExecuteTicket ExecuteTicket = MakeG7ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteReceipt Receipt = MakeG7Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteFinalReport FinalReport = MakeG7FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeG7ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = MakeG7HandoffEnvelope(ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGExecuteIntent Intent = MakeG7StageGIntent(HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGWriteEnableRequest WriteEnableRequest = MakeG7WriteEnableRequest(Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAgentStageGExecutePermitTicket PermitTicket = MakeG7PermitTicket(WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGExecuteDispatchRequest DispatchRequest = MakeG7DispatchRequest(PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAgentStageGExecuteDispatchReceipt DispatchReceipt = MakeG7DispatchReceipt(DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGExecuteCommitRequest CommitRequest = MakeG7CommitRequest(
		DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);

	FHCIAgentStageGExecuteCommitReceipt Request;
	TestTrue(TEXT("Build stage g execute commit receipt"), FHCIAgentExecutorStageGExecuteCommitReceiptBridge::BuildStageGExecuteCommitReceipt(
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
		Request));

	FString JsonText;
	TestTrue(TEXT("Serialize stage g execute commit receipt json"), FHCIAgentStageGExecuteCommitReceiptJsonSerializer::SerializeToJsonString(Request, JsonText));
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
	TestTrue(TEXT("JSON includes stage_g_execute_commit_receipt_digest"), JsonText.Contains(TEXT("\"stage_g_execute_commit_receipt_digest\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_commit_receipt_status"), JsonText.Contains(TEXT("\"stage_g_execute_commit_receipt_status\"")));
	TestTrue(TEXT("JSON includes execute_commit_confirmed"), JsonText.Contains(TEXT("\"execute_commit_confirmed\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_commit_request_ready"), JsonText.Contains(TEXT("\"stage_g_execute_commit_request_ready\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_commit_accepted"), JsonText.Contains(TEXT("\"stage_g_execute_commit_accepted\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_commit_receipt_ready"), JsonText.Contains(TEXT("\"stage_g_execute_commit_receipt_ready\"")));
	TestTrue(TEXT("JSON includes locate_strategy"), JsonText.Contains(TEXT("\"locate_strategy\"")));
	return true;
}

#endif





