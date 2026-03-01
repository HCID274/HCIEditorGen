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
#include "Agent/Bridges/HCIAgentExecutorStageGExecuteArchiveBundleBridge.h"
#include "Agent/Bridges/HCIAgentExecutorStageGExecutionReadinessReportBridge.h"
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
#include "Agent/Contracts/StageG/HCIAgentStageGExecuteArchiveBundle.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecuteArchiveBundleJsonSerializer.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecutionReadinessReport.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecutionReadinessReportJsonSerializer.h"
#include "Agent/Executor/HCIDryRunDiff.h"
#include "Misc/AutomationTest.h"

namespace
{
static FHCIDryRunDiffReport MakeG9SelectedReviewReport()
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

static FHCIAgentApplyRequest MakeG9ApplyRequest(const FHCIDryRunDiffReport& Review)
{
	FHCIAgentApplyRequest ApplyRequest;
	check(FHCIAgentExecutorApplyRequestBridge::BuildApplyRequest(Review, ApplyRequest));
	return ApplyRequest;
}

static FHCIAgentApplyConfirmRequest MakeG9ConfirmRequest(
	const FHCIAgentApplyRequest& ApplyRequest,
	const FHCIDryRunDiffReport& Review,
	const bool bUserConfirmed)
{
	FHCIAgentApplyConfirmRequest ConfirmRequest;
	check(FHCIAgentExecutorApplyConfirmBridge::BuildConfirmRequest(ApplyRequest, Review, bUserConfirmed, ConfirmRequest));
	return ConfirmRequest;
}

static FHCIAgentExecuteTicket MakeG9ExecuteTicket(
	const FHCIAgentApplyConfirmRequest& ConfirmRequest,
	const FHCIAgentApplyRequest& ApplyRequest,
	const FHCIDryRunDiffReport& Review)
{
	FHCIAgentExecuteTicket ExecuteTicket;
	check(FHCIAgentExecutorExecuteTicketBridge::BuildExecuteTicket(ConfirmRequest, ApplyRequest, Review, ExecuteTicket));
	return ExecuteTicket;
}

static FHCIAgentSimulateExecuteReceipt MakeG9Receipt(
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

static FHCIAgentSimulateExecuteFinalReport MakeG9FinalReport(
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

static FHCIAgentSimulateExecuteArchiveBundle MakeG9ArchiveBundle(
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

static FHCIAgentSimulateExecuteHandoffEnvelope MakeG9HandoffEnvelope(
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

static FHCIAgentStageGExecuteIntent MakeG9StageGIntent(
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

static FHCIAgentStageGWriteEnableRequest MakeG9WriteEnableRequest(
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

static FHCIAgentStageGExecutePermitTicket MakeG9PermitTicket(
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

static FHCIAgentStageGExecuteDispatchRequest MakeG9DispatchRequest(
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

static FHCIAgentStageGExecuteDispatchReceipt MakeG9DispatchReceipt(
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

static FHCIAgentStageGExecuteCommitRequest MakeG9CommitRequest(
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

static FHCIAgentStageGExecuteCommitReceipt MakeG9CommitReceipt(
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

static FHCIAgentStageGExecuteFinalReport MakeG9StageGExecuteFinalReport(
	const FHCIAgentStageGExecuteCommitReceipt& CommitReceipt,
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
	FHCIAgentStageGExecuteFinalReport StageGFinalReport;
	check(FHCIAgentExecutorStageGExecuteFinalReportBridge::BuildStageGExecuteFinalReport(
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
		StageGFinalReport));
	return StageGFinalReport;
}

static FHCIAgentStageGExecuteArchiveBundle MakeReadyG9StageGExecuteArchiveBundle(const FHCIDryRunDiffReport& Review)
{
	const FHCIAgentApplyRequest ApplyRequest = MakeG9ApplyRequest(Review);
	const FHCIAgentApplyConfirmRequest ConfirmRequest = MakeG9ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAgentExecuteTicket ExecuteTicket = MakeG9ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteReceipt Receipt = MakeG9Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteFinalReport FinalReport = MakeG9FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeG9ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = MakeG9HandoffEnvelope(ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGExecuteIntent Intent = MakeG9StageGIntent(HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGWriteEnableRequest WriteEnableRequest = MakeG9WriteEnableRequest(Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAgentStageGExecutePermitTicket PermitTicket = MakeG9PermitTicket(WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGExecuteDispatchRequest DispatchRequest = MakeG9DispatchRequest(PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAgentStageGExecuteDispatchReceipt DispatchReceipt = MakeG9DispatchReceipt(DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGExecuteCommitRequest CommitRequest = MakeG9CommitRequest(
		DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAgentStageGExecuteCommitReceipt CommitReceipt = MakeG9CommitReceipt(
		CommitRequest, DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGExecuteFinalReport StageGFinalReport = MakeG9StageGExecuteFinalReport(
		CommitReceipt, CommitRequest, DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);

	FHCIAgentStageGExecuteArchiveBundle Request;
	check(FHCIAgentExecutorStageGExecuteArchiveBundleBridge::BuildStageGExecuteArchiveBundle(
		StageGFinalReport,
		StageGFinalReport.RequestId,
		CommitReceipt.RequestId,
		CommitReceipt.StageGExecuteCommitReceiptDigest,
		CommitReceipt,
		CommitRequest,
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
	return Request;
}
} // namespace

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorStageGExecuteArchiveBundleReadyTest,
	"HCI.Editor.AgentExecutorStageGExecuteArchiveBundle.ReadyWhenStageGExecuteCommitReceiptReady",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorStageGExecuteArchiveBundleReadyTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeG9SelectedReviewReport();
	const FHCIAgentApplyRequest ApplyRequest = MakeG9ApplyRequest(Review);
	const FHCIAgentApplyConfirmRequest ConfirmRequest = MakeG9ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAgentExecuteTicket ExecuteTicket = MakeG9ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteReceipt Receipt = MakeG9Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteFinalReport FinalReport = MakeG9FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeG9ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = MakeG9HandoffEnvelope(ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGExecuteIntent Intent = MakeG9StageGIntent(HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGWriteEnableRequest WriteEnableRequest = MakeG9WriteEnableRequest(Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAgentStageGExecutePermitTicket PermitTicket = MakeG9PermitTicket(WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGExecuteDispatchRequest DispatchRequest = MakeG9DispatchRequest(PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAgentStageGExecuteDispatchReceipt DispatchReceipt = MakeG9DispatchReceipt(DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGExecuteCommitRequest CommitRequest = MakeG9CommitRequest(
		DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAgentStageGExecuteCommitReceipt CommitReceipt = MakeG9CommitReceipt(
		CommitRequest, DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGExecuteFinalReport StageGFinalReport = MakeG9StageGExecuteFinalReport(
		CommitReceipt, CommitRequest, DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);

	FHCIAgentStageGExecuteArchiveBundle Request;
	TestTrue(TEXT("Build stage g execute archive bundle"), FHCIAgentExecutorStageGExecuteArchiveBundleBridge::BuildStageGExecuteArchiveBundle(
		StageGFinalReport,
		StageGFinalReport.RequestId,
		CommitReceipt.RequestId,
		CommitReceipt.StageGExecuteCommitReceiptDigest,
		CommitReceipt,
		CommitRequest,
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
	TestTrue(TEXT("ArchiveBundleReady"), Request.bStageGExecuteArchiveBundleReady);
	TestTrue(TEXT("Archived"), Request.bStageGExecuteArchived);
	TestTrue(TEXT("CommitConfirmed"), Request.bExecuteCommitConfirmed);
	TestTrue(TEXT("WriteEnabled"), Request.bWriteEnabled);
	TestEqual(TEXT("ArchiveBundleStatus"), Request.StageGExecuteArchiveBundleStatus, FString(TEXT("ready")));
	TestEqual(TEXT("ErrorCode"), Request.ErrorCode, FString(TEXT("-")));
	TestEqual(TEXT("Reason"), Request.Reason, FString(TEXT("stage_g_execute_archive_bundle_ready")));
	TestFalse(TEXT("CommitDigest present"), Request.StageGExecuteArchiveBundleDigest.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorStageGExecuteArchiveBundleWriteEnableNotReadyTest,
	"HCI.Editor.AgentExecutorStageGExecuteArchiveBundle.StageGExecuteCommitReceiptNotReadyReturnsE4217",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorStageGExecuteArchiveBundleWriteEnableNotReadyTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeG9SelectedReviewReport();
	const FHCIAgentApplyRequest ApplyRequest = MakeG9ApplyRequest(Review);
	const FHCIAgentApplyConfirmRequest ConfirmRequest = MakeG9ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAgentExecuteTicket ExecuteTicket = MakeG9ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteReceipt Receipt = MakeG9Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteFinalReport FinalReport = MakeG9FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeG9ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = MakeG9HandoffEnvelope(ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGExecuteIntent Intent = MakeG9StageGIntent(HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGWriteEnableRequest WriteEnableRequest = MakeG9WriteEnableRequest(Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAgentStageGExecutePermitTicket PermitTicket = MakeG9PermitTicket(WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGExecuteDispatchRequest DispatchRequest = MakeG9DispatchRequest(PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAgentStageGExecuteDispatchReceipt DispatchReceipt = MakeG9DispatchReceipt(DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	FHCIAgentStageGExecuteCommitRequest CommitRequest = MakeG9CommitRequest(
		DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	FHCIAgentStageGExecuteCommitReceipt CommitReceipt = MakeG9CommitReceipt(
		CommitRequest, DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	FHCIAgentStageGExecuteFinalReport StageGFinalReport = MakeG9StageGExecuteFinalReport(
		CommitReceipt, CommitRequest, DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	StageGFinalReport.bWriteEnabled = false;
	StageGFinalReport.bReadyForStageGExecute = false;
	StageGFinalReport.bStageGExecuteFinalized = false;
	StageGFinalReport.bStageGExecuteFinalReportReady = false;
	StageGFinalReport.StageGExecuteFinalReportStatus = TEXT("blocked");

	FHCIAgentStageGExecuteArchiveBundle Request;
	TestTrue(TEXT("Build stage g execute archive bundle"), FHCIAgentExecutorStageGExecuteArchiveBundleBridge::BuildStageGExecuteArchiveBundle(
		StageGFinalReport,
		StageGFinalReport.RequestId,
		CommitReceipt.RequestId,
		CommitReceipt.StageGExecuteCommitReceiptDigest,
		CommitReceipt,
		CommitRequest,
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
	TestFalse(TEXT("ArchiveBundleReady"), Request.bStageGExecuteArchiveBundleReady);
	TestEqual(TEXT("ArchiveBundleStatus"), Request.StageGExecuteArchiveBundleStatus, FString(TEXT("blocked")));
	TestEqual(TEXT("ErrorCode"), Request.ErrorCode, FString(TEXT("E4217")));
	TestEqual(TEXT("Reason"), Request.Reason, FString(TEXT("stage_g_execute_final_report_not_completed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorStageGExecuteArchiveBundleDigestMismatchTest,
	"HCI.Editor.AgentExecutorStageGExecuteArchiveBundle.DigestMismatchReturnsE4202",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorStageGExecuteArchiveBundleDigestMismatchTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeG9SelectedReviewReport();
	const FHCIAgentApplyRequest ApplyRequest = MakeG9ApplyRequest(Review);
	const FHCIAgentApplyConfirmRequest ConfirmRequest = MakeG9ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAgentExecuteTicket ExecuteTicket = MakeG9ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteReceipt Receipt = MakeG9Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteFinalReport FinalReport = MakeG9FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeG9ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = MakeG9HandoffEnvelope(ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGExecuteIntent Intent = MakeG9StageGIntent(HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGWriteEnableRequest WriteEnableRequest = MakeG9WriteEnableRequest(Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAgentStageGExecutePermitTicket PermitTicket = MakeG9PermitTicket(WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGExecuteDispatchRequest DispatchRequest = MakeG9DispatchRequest(PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAgentStageGExecuteDispatchReceipt DispatchReceipt = MakeG9DispatchReceipt(DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	FHCIAgentStageGExecuteCommitRequest CommitRequest = MakeG9CommitRequest(
		DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	FHCIAgentStageGExecuteCommitReceipt CommitReceipt = MakeG9CommitReceipt(
		CommitRequest, DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	FHCIAgentStageGExecuteFinalReport StageGFinalReport = MakeG9StageGExecuteFinalReport(
		CommitReceipt, CommitRequest, DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	StageGFinalReport.SelectionDigest = TEXT("crc32_BAD0C0DE");

	FHCIAgentStageGExecuteArchiveBundle Request;
	TestTrue(TEXT("Build stage g execute archive bundle"), FHCIAgentExecutorStageGExecuteArchiveBundleBridge::BuildStageGExecuteArchiveBundle(
		StageGFinalReport,
		StageGFinalReport.RequestId,
		CommitReceipt.RequestId,
		CommitReceipt.StageGExecuteCommitReceiptDigest,
		CommitReceipt,
		CommitRequest,
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
	TestFalse(TEXT("ArchiveBundleReady"), Request.bStageGExecuteArchiveBundleReady);
	TestEqual(TEXT("ErrorCode"), Request.ErrorCode, FString(TEXT("E4202")));
	TestEqual(TEXT("Reason"), Request.Reason, FString(TEXT("selection_digest_mismatch")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorStageGExecuteArchiveBundleJsonFieldsTest,
	"HCI.Editor.AgentExecutorStageGExecuteArchiveBundle.JsonIncludesFinalReportFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorStageGExecuteArchiveBundleJsonFieldsTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeG9SelectedReviewReport();
	const FHCIAgentApplyRequest ApplyRequest = MakeG9ApplyRequest(Review);
	const FHCIAgentApplyConfirmRequest ConfirmRequest = MakeG9ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAgentExecuteTicket ExecuteTicket = MakeG9ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteReceipt Receipt = MakeG9Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteFinalReport FinalReport = MakeG9FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeG9ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = MakeG9HandoffEnvelope(ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGExecuteIntent Intent = MakeG9StageGIntent(HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGWriteEnableRequest WriteEnableRequest = MakeG9WriteEnableRequest(Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAgentStageGExecutePermitTicket PermitTicket = MakeG9PermitTicket(WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGExecuteDispatchRequest DispatchRequest = MakeG9DispatchRequest(PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAgentStageGExecuteDispatchReceipt DispatchReceipt = MakeG9DispatchReceipt(DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGExecuteCommitRequest CommitRequest = MakeG9CommitRequest(
		DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAgentStageGExecuteCommitReceipt CommitReceipt = MakeG9CommitReceipt(
		CommitRequest, DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAgentStageGExecuteFinalReport StageGFinalReport = MakeG9StageGExecuteFinalReport(
		CommitReceipt, CommitRequest, DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);

	FHCIAgentStageGExecuteArchiveBundle Request;
	TestTrue(TEXT("Build stage g execute archive bundle"), FHCIAgentExecutorStageGExecuteArchiveBundleBridge::BuildStageGExecuteArchiveBundle(
		StageGFinalReport,
		StageGFinalReport.RequestId,
		CommitReceipt.RequestId,
		CommitReceipt.StageGExecuteCommitReceiptDigest,
		CommitReceipt,
		CommitRequest,
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
	TestTrue(TEXT("Serialize stage g execute archive bundle json"), FHCIAgentStageGExecuteArchiveBundleJsonSerializer::SerializeToJsonString(Request, JsonText));
	TestTrue(TEXT("JSON includes stage_g_execute_final_report_id"), JsonText.Contains(TEXT("\"stage_g_execute_final_report_id\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_final_report_digest"), JsonText.Contains(TEXT("\"stage_g_execute_final_report_digest\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_final_report_status"), JsonText.Contains(TEXT("\"stage_g_execute_final_report_status\"")));
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
	TestTrue(TEXT("JSON includes stage_g_execute_archive_bundle_digest"), JsonText.Contains(TEXT("\"stage_g_execute_archive_bundle_digest\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_archive_bundle_status"), JsonText.Contains(TEXT("\"stage_g_execute_archive_bundle_status\"")));
	TestTrue(TEXT("JSON includes execute_commit_confirmed"), JsonText.Contains(TEXT("\"execute_commit_confirmed\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_commit_request_ready"), JsonText.Contains(TEXT("\"stage_g_execute_commit_request_ready\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_commit_accepted"), JsonText.Contains(TEXT("\"stage_g_execute_commit_accepted\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_commit_receipt_ready"), JsonText.Contains(TEXT("\"stage_g_execute_commit_receipt_ready\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_finalized"), JsonText.Contains(TEXT("\"stage_g_execute_finalized\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_final_report_ready"), JsonText.Contains(TEXT("\"stage_g_execute_final_report_ready\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_archived"), JsonText.Contains(TEXT("\"stage_g_execute_archived\"")));
	TestTrue(TEXT("JSON includes stage_g_execute_archive_bundle_ready"), JsonText.Contains(TEXT("\"stage_g_execute_archive_bundle_ready\"")));
	TestTrue(TEXT("JSON includes locate_strategy"), JsonText.Contains(TEXT("\"locate_strategy\"")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorStageGExecutionReadinessReadyTest,
	"HCI.Editor.AgentExecutorStageGExecutionReadiness.ReadyWhenArchiveBundleReady",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorStageGExecutionReadinessReadyTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeG9SelectedReviewReport();
	const FHCIAgentStageGExecuteArchiveBundle ArchiveBundle = MakeReadyG9StageGExecuteArchiveBundle(Review);

	FHCIAgentStageGExecutionReadinessReport Report;
	TestTrue(TEXT("Build stage g execution readiness"), FHCIAgentExecutorStageGExecutionReadinessReportBridge::BuildStageGExecutionReadinessReport(
		ArchiveBundle,
		ArchiveBundle.RequestId,
		Review,
		true,
		Report));
	TestTrue(TEXT("ReadyForH1"), Report.bReadyForH1PlannerIntegration);
	TestEqual(TEXT("ReadinessStatus"), Report.StageGExecutionReadinessStatus, FString(TEXT("ready")));
	TestEqual(TEXT("ExecutionMode"), Report.ExecutionMode, FString(TEXT("simulate_dry_run")));
	TestEqual(TEXT("ErrorCode"), Report.ErrorCode, FString(TEXT("-")));
	TestEqual(TEXT("Reason"), Report.Reason, FString(TEXT("stage_g_execution_readiness_ready")));
	TestFalse(TEXT("ReadinessDigest present"), Report.StageGExecutionReadinessDigest.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorStageGExecutionReadinessArchiveNotReadyTest,
	"HCI.Editor.AgentExecutorStageGExecutionReadiness.ArchiveNotReadyReturnsE4218",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorStageGExecutionReadinessArchiveNotReadyTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeG9SelectedReviewReport();
	FHCIAgentStageGExecuteArchiveBundle ArchiveBundle = MakeReadyG9StageGExecuteArchiveBundle(Review);
	ArchiveBundle.bStageGExecuteArchiveBundleReady = false;
	ArchiveBundle.bStageGExecuteArchived = false;
	ArchiveBundle.StageGExecuteArchiveBundleStatus = TEXT("blocked");

	FHCIAgentStageGExecutionReadinessReport Report;
	TestTrue(TEXT("Build stage g execution readiness"), FHCIAgentExecutorStageGExecutionReadinessReportBridge::BuildStageGExecutionReadinessReport(
		ArchiveBundle,
		ArchiveBundle.RequestId,
		Review,
		true,
		Report));
	TestFalse(TEXT("ReadyForH1"), Report.bReadyForH1PlannerIntegration);
	TestEqual(TEXT("ReadinessStatus"), Report.StageGExecutionReadinessStatus, FString(TEXT("blocked")));
	TestEqual(TEXT("ErrorCode"), Report.ErrorCode, FString(TEXT("E4218")));
	TestEqual(TEXT("Reason"), Report.Reason, FString(TEXT("stage_g_execute_archive_bundle_not_ready")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorStageGExecutionReadinessModeBlockedTest,
	"HCI.Editor.AgentExecutorStageGExecutionReadiness.WriteModeBlockedReturnsE4219",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorStageGExecutionReadinessModeBlockedTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeG9SelectedReviewReport();
	FHCIAgentStageGExecuteArchiveBundle ArchiveBundle = MakeReadyG9StageGExecuteArchiveBundle(Review);
	ArchiveBundle.ExecutionMode = TEXT("write_execute");

	FHCIAgentStageGExecutionReadinessReport Report;
	TestTrue(TEXT("Build stage g execution readiness"), FHCIAgentExecutorStageGExecutionReadinessReportBridge::BuildStageGExecutionReadinessReport(
		ArchiveBundle,
		ArchiveBundle.RequestId,
		Review,
		true,
		Report));
	TestFalse(TEXT("ReadyForH1"), Report.bReadyForH1PlannerIntegration);
	TestEqual(TEXT("ReadinessStatus"), Report.StageGExecutionReadinessStatus, FString(TEXT("blocked")));
	TestEqual(TEXT("ErrorCode"), Report.ErrorCode, FString(TEXT("E4219")));
	TestEqual(TEXT("Reason"), Report.Reason, FString(TEXT("stage_g_execution_mode_write_not_allowed_in_g10")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorStageGExecutionReadinessUnconfirmedTest,
	"HCI.Editor.AgentExecutorStageGExecutionReadiness.UnconfirmedReturnsE4005",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorStageGExecutionReadinessUnconfirmedTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeG9SelectedReviewReport();
	const FHCIAgentStageGExecuteArchiveBundle ArchiveBundle = MakeReadyG9StageGExecuteArchiveBundle(Review);

	FHCIAgentStageGExecutionReadinessReport Report;
	TestTrue(TEXT("Build stage g execution readiness"), FHCIAgentExecutorStageGExecutionReadinessReportBridge::BuildStageGExecutionReadinessReport(
		ArchiveBundle,
		ArchiveBundle.RequestId,
		Review,
		false,
		Report));
	TestFalse(TEXT("ReadyForH1"), Report.bReadyForH1PlannerIntegration);
	TestEqual(TEXT("ReadinessStatus"), Report.StageGExecutionReadinessStatus, FString(TEXT("blocked")));
	TestEqual(TEXT("ErrorCode"), Report.ErrorCode, FString(TEXT("E4005")));
	TestEqual(TEXT("Reason"), Report.Reason, FString(TEXT("user_not_confirmed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorStageGExecutionReadinessJsonFieldsTest,
	"HCI.Editor.AgentExecutorStageGExecutionReadiness.JsonIncludesReadinessFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorStageGExecutionReadinessJsonFieldsTest::RunTest(const FString& Parameters)
{
	const FHCIDryRunDiffReport Review = MakeG9SelectedReviewReport();
	const FHCIAgentStageGExecuteArchiveBundle ArchiveBundle = MakeReadyG9StageGExecuteArchiveBundle(Review);

	FHCIAgentStageGExecutionReadinessReport Report;
	TestTrue(TEXT("Build stage g execution readiness"), FHCIAgentExecutorStageGExecutionReadinessReportBridge::BuildStageGExecutionReadinessReport(
		ArchiveBundle,
		ArchiveBundle.RequestId,
		Review,
		true,
		Report));

	FString JsonText;
	TestTrue(TEXT("Serialize stage g execution readiness json"), FHCIAgentStageGExecutionReadinessReportJsonSerializer::SerializeToJsonString(Report, JsonText));
	TestTrue(TEXT("JSON includes stage_g_execute_archive_bundle_id"), JsonText.Contains(TEXT("\"stage_g_execute_archive_bundle_id\"")));
	TestTrue(TEXT("JSON includes stage_g_execution_readiness_digest"), JsonText.Contains(TEXT("\"stage_g_execution_readiness_digest\"")));
	TestTrue(TEXT("JSON includes stage_g_execution_readiness_status"), JsonText.Contains(TEXT("\"stage_g_execution_readiness_status\"")));
	TestTrue(TEXT("JSON includes ready_for_h1_planner_integration"), JsonText.Contains(TEXT("\"ready_for_h1_planner_integration\"")));
	TestTrue(TEXT("JSON includes execution_mode"), JsonText.Contains(TEXT("\"execution_mode\"")));
	TestTrue(TEXT("JSON includes locate_strategy"), JsonText.Contains(TEXT("\"locate_strategy\"")));
	return true;
}

#endif





