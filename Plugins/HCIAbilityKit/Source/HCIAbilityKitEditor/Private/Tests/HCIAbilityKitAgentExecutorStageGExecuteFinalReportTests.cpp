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
#include "Agent/HCIAbilityKitAgentExecutorStageGExecuteCommitRequestBridge.h"
#include "Agent/HCIAbilityKitAgentExecutorStageGExecuteCommitReceiptBridge.h"
#include "Agent/HCIAbilityKitAgentExecutorStageGExecuteFinalReportBridge.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteArchiveBundle.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteFinalReport.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteHandoffEnvelope.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteReceipt.h"
#include "Agent/HCIAbilityKitAgentStageGExecuteIntent.h"
#include "Agent/HCIAbilityKitAgentStageGWriteEnableRequest.h"
#include "Agent/HCIAbilityKitAgentStageGExecutePermitTicket.h"
#include "Agent/HCIAbilityKitAgentStageGExecuteDispatchRequest.h"
#include "Agent/HCIAbilityKitAgentStageGExecuteDispatchReceipt.h"
#include "Agent/HCIAbilityKitAgentStageGExecuteCommitRequest.h"
#include "Agent/HCIAbilityKitAgentStageGExecuteCommitReceipt.h"
#include "Agent/HCIAbilityKitAgentStageGExecuteFinalReport.h"
#include "Agent/HCIAbilityKitAgentStageGExecuteFinalReportJsonSerializer.h"
#include "Agent/HCIAbilityKitDryRunDiff.h"
#include "Misc/AutomationTest.h"

namespace
{
static FHCIAbilityKitDryRunDiffReport MakeG8SelectedReviewReport()
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

static FHCIAbilityKitAgentApplyRequest MakeG8ApplyRequest(const FHCIAbilityKitDryRunDiffReport& Review)
{
	FHCIAbilityKitAgentApplyRequest ApplyRequest;
	check(FHCIAbilityKitAgentExecutorApplyRequestBridge::BuildApplyRequest(Review, ApplyRequest));
	return ApplyRequest;
}

static FHCIAbilityKitAgentApplyConfirmRequest MakeG8ConfirmRequest(
	const FHCIAbilityKitAgentApplyRequest& ApplyRequest,
	const FHCIAbilityKitDryRunDiffReport& Review,
	const bool bUserConfirmed)
{
	FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest;
	check(FHCIAbilityKitAgentExecutorApplyConfirmBridge::BuildConfirmRequest(ApplyRequest, Review, bUserConfirmed, ConfirmRequest));
	return ConfirmRequest;
}

static FHCIAbilityKitAgentExecuteTicket MakeG8ExecuteTicket(
	const FHCIAbilityKitAgentApplyConfirmRequest& ConfirmRequest,
	const FHCIAbilityKitAgentApplyRequest& ApplyRequest,
	const FHCIAbilityKitDryRunDiffReport& Review)
{
	FHCIAbilityKitAgentExecuteTicket ExecuteTicket;
	check(FHCIAbilityKitAgentExecutorExecuteTicketBridge::BuildExecuteTicket(ConfirmRequest, ApplyRequest, Review, ExecuteTicket));
	return ExecuteTicket;
}

static FHCIAbilityKitAgentSimulateExecuteReceipt MakeG8Receipt(
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

static FHCIAbilityKitAgentSimulateExecuteFinalReport MakeG8FinalReport(
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

static FHCIAbilityKitAgentSimulateExecuteArchiveBundle MakeG8ArchiveBundle(
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

static FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope MakeG8HandoffEnvelope(
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

static FHCIAbilityKitAgentStageGExecuteIntent MakeG8StageGIntent(
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

static FHCIAbilityKitAgentStageGWriteEnableRequest MakeG8WriteEnableRequest(
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

static FHCIAbilityKitAgentStageGExecutePermitTicket MakeG8PermitTicket(
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

static FHCIAbilityKitAgentStageGExecuteDispatchRequest MakeG8DispatchRequest(
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

static FHCIAbilityKitAgentStageGExecuteDispatchReceipt MakeG8DispatchReceipt(
	const FHCIAbilityKitAgentStageGExecuteDispatchRequest& DispatchRequest,
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
	const FHCIAbilityKitDryRunDiffReport& Review)
{
	FHCIAbilityKitAgentStageGExecuteDispatchReceipt Request;
	check(FHCIAbilityKitAgentExecutorStageGExecuteDispatchReceiptBridge::BuildStageGExecuteDispatchReceipt(
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

static FHCIAbilityKitAgentStageGExecuteCommitRequest MakeG8CommitRequest(
	const FHCIAbilityKitAgentStageGExecuteDispatchReceipt& DispatchReceipt,
	const FHCIAbilityKitAgentStageGExecuteDispatchRequest& DispatchRequest,
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
	const bool bExecuteCommitConfirmed)
{
	FHCIAbilityKitAgentStageGExecuteCommitRequest Request;
	check(FHCIAbilityKitAgentExecutorStageGExecuteCommitRequestBridge::BuildStageGExecuteCommitRequest(
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

static FHCIAbilityKitAgentStageGExecuteCommitReceipt MakeG8CommitReceipt(
	const FHCIAbilityKitAgentStageGExecuteCommitRequest& CommitRequest,
	const FHCIAbilityKitAgentStageGExecuteDispatchReceipt& DispatchReceipt,
	const FHCIAbilityKitAgentStageGExecuteDispatchRequest& DispatchRequest,
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
	const FHCIAbilityKitDryRunDiffReport& Review)
{
	FHCIAbilityKitAgentStageGExecuteCommitReceipt CommitReceipt;
	check(FHCIAbilityKitAgentExecutorStageGExecuteCommitReceiptBridge::BuildStageGExecuteCommitReceipt(
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
	FHCIAbilityKitAgentExecutorStageGExecuteFinalReportReadyTest,
	"HCIAbilityKit.Editor.AgentExecutorStageGExecuteFinalReport.ReadyWhenStageGExecuteCommitReceiptReady",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorStageGExecuteFinalReportReadyTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeG8SelectedReviewReport();
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeG8ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeG8ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAbilityKitAgentExecuteTicket ExecuteTicket = MakeG8ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteReceipt Receipt = MakeG8Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteFinalReport FinalReport = MakeG8FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeG8ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = MakeG8HandoffEnvelope(ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGExecuteIntent Intent = MakeG8StageGIntent(HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGWriteEnableRequest WriteEnableRequest = MakeG8WriteEnableRequest(Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAbilityKitAgentStageGExecutePermitTicket PermitTicket = MakeG8PermitTicket(WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGExecuteDispatchRequest DispatchRequest = MakeG8DispatchRequest(PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAbilityKitAgentStageGExecuteDispatchReceipt DispatchReceipt = MakeG8DispatchReceipt(DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGExecuteCommitRequest CommitRequest = MakeG8CommitRequest(
		DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAbilityKitAgentStageGExecuteCommitReceipt CommitReceipt = MakeG8CommitReceipt(
		CommitRequest, DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);

	FHCIAbilityKitAgentStageGExecuteFinalReport Request;
	TestTrue(TEXT("Build stage g execute final report"), FHCIAbilityKitAgentExecutorStageGExecuteFinalReportBridge::BuildStageGExecuteFinalReport(
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
	FHCIAbilityKitAgentExecutorStageGExecuteFinalReportWriteEnableNotReadyTest,
	"HCIAbilityKit.Editor.AgentExecutorStageGExecuteFinalReport.StageGExecuteCommitReceiptNotReadyReturnsE4216",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorStageGExecuteFinalReportWriteEnableNotReadyTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeG8SelectedReviewReport();
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeG8ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeG8ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAbilityKitAgentExecuteTicket ExecuteTicket = MakeG8ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteReceipt Receipt = MakeG8Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteFinalReport FinalReport = MakeG8FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeG8ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = MakeG8HandoffEnvelope(ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGExecuteIntent Intent = MakeG8StageGIntent(HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGWriteEnableRequest WriteEnableRequest = MakeG8WriteEnableRequest(Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAbilityKitAgentStageGExecutePermitTicket PermitTicket = MakeG8PermitTicket(WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGExecuteDispatchRequest DispatchRequest = MakeG8DispatchRequest(PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAbilityKitAgentStageGExecuteDispatchReceipt DispatchReceipt = MakeG8DispatchReceipt(DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	FHCIAbilityKitAgentStageGExecuteCommitRequest CommitRequest = MakeG8CommitRequest(
		DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	FHCIAbilityKitAgentStageGExecuteCommitReceipt CommitReceipt = MakeG8CommitReceipt(
		CommitRequest, DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	CommitReceipt.bWriteEnabled = false;
	CommitReceipt.bReadyForStageGExecute = false;
	CommitReceipt.bStageGExecuteCommitAccepted = false;
	CommitReceipt.bStageGExecuteCommitReceiptReady = false;
	CommitReceipt.StageGExecuteCommitReceiptStatus = TEXT("blocked");

	FHCIAbilityKitAgentStageGExecuteFinalReport Request;
	TestTrue(TEXT("Build stage g execute final report"), FHCIAbilityKitAgentExecutorStageGExecuteFinalReportBridge::BuildStageGExecuteFinalReport(
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
	FHCIAbilityKitAgentExecutorStageGExecuteFinalReportDigestMismatchTest,
	"HCIAbilityKit.Editor.AgentExecutorStageGExecuteFinalReport.DigestMismatchReturnsE4202",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorStageGExecuteFinalReportDigestMismatchTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeG8SelectedReviewReport();
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeG8ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeG8ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAbilityKitAgentExecuteTicket ExecuteTicket = MakeG8ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteReceipt Receipt = MakeG8Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteFinalReport FinalReport = MakeG8FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeG8ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = MakeG8HandoffEnvelope(ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGExecuteIntent Intent = MakeG8StageGIntent(HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGWriteEnableRequest WriteEnableRequest = MakeG8WriteEnableRequest(Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAbilityKitAgentStageGExecutePermitTicket PermitTicket = MakeG8PermitTicket(WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGExecuteDispatchRequest DispatchRequest = MakeG8DispatchRequest(PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAbilityKitAgentStageGExecuteDispatchReceipt DispatchReceipt = MakeG8DispatchReceipt(DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	FHCIAbilityKitAgentStageGExecuteCommitRequest CommitRequest = MakeG8CommitRequest(
		DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	FHCIAbilityKitAgentStageGExecuteCommitReceipt CommitReceipt = MakeG8CommitReceipt(
		CommitRequest, DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	CommitReceipt.SelectionDigest = TEXT("crc32_BAD0C0DE");

	FHCIAbilityKitAgentStageGExecuteFinalReport Request;
	TestTrue(TEXT("Build stage g execute final report"), FHCIAbilityKitAgentExecutorStageGExecuteFinalReportBridge::BuildStageGExecuteFinalReport(
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
	FHCIAbilityKitAgentExecutorStageGExecuteFinalReportJsonFieldsTest,
	"HCIAbilityKit.Editor.AgentExecutorStageGExecuteFinalReport.JsonIncludesFinalReportFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorStageGExecuteFinalReportJsonFieldsTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeG8SelectedReviewReport();
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeG8ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeG8ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAbilityKitAgentExecuteTicket ExecuteTicket = MakeG8ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteReceipt Receipt = MakeG8Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteFinalReport FinalReport = MakeG8FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeG8ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = MakeG8HandoffEnvelope(ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGExecuteIntent Intent = MakeG8StageGIntent(HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGWriteEnableRequest WriteEnableRequest = MakeG8WriteEnableRequest(Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAbilityKitAgentStageGExecutePermitTicket PermitTicket = MakeG8PermitTicket(WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGExecuteDispatchRequest DispatchRequest = MakeG8DispatchRequest(PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAbilityKitAgentStageGExecuteDispatchReceipt DispatchReceipt = MakeG8DispatchReceipt(DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGExecuteCommitRequest CommitRequest = MakeG8CommitRequest(
		DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAbilityKitAgentStageGExecuteCommitReceipt CommitReceipt = MakeG8CommitReceipt(
		CommitRequest, DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);

	FHCIAbilityKitAgentStageGExecuteFinalReport Request;
	TestTrue(TEXT("Build stage g execute final report"), FHCIAbilityKitAgentExecutorStageGExecuteFinalReportBridge::BuildStageGExecuteFinalReport(
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
	TestTrue(TEXT("Serialize stage g execute final report json"), FHCIAbilityKitAgentStageGExecuteFinalReportJsonSerializer::SerializeToJsonString(Request, JsonText));
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





