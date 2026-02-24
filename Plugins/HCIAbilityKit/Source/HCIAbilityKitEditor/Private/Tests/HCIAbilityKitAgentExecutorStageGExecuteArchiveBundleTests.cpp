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
#include "Agent/HCIAbilityKitAgentExecutorStageGExecuteArchiveBundleBridge.h"
#include "Agent/HCIAbilityKitAgentExecutorStageGExecutionReadinessReportBridge.h"
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
#include "Agent/HCIAbilityKitAgentStageGExecuteArchiveBundle.h"
#include "Agent/HCIAbilityKitAgentStageGExecuteArchiveBundleJsonSerializer.h"
#include "Agent/HCIAbilityKitAgentStageGExecutionReadinessReport.h"
#include "Agent/HCIAbilityKitAgentStageGExecutionReadinessReportJsonSerializer.h"
#include "Agent/HCIAbilityKitDryRunDiff.h"
#include "Misc/AutomationTest.h"

namespace
{
static FHCIAbilityKitDryRunDiffReport MakeG9SelectedReviewReport()
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

static FHCIAbilityKitAgentApplyRequest MakeG9ApplyRequest(const FHCIAbilityKitDryRunDiffReport& Review)
{
	FHCIAbilityKitAgentApplyRequest ApplyRequest;
	check(FHCIAbilityKitAgentExecutorApplyRequestBridge::BuildApplyRequest(Review, ApplyRequest));
	return ApplyRequest;
}

static FHCIAbilityKitAgentApplyConfirmRequest MakeG9ConfirmRequest(
	const FHCIAbilityKitAgentApplyRequest& ApplyRequest,
	const FHCIAbilityKitDryRunDiffReport& Review,
	const bool bUserConfirmed)
{
	FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest;
	check(FHCIAbilityKitAgentExecutorApplyConfirmBridge::BuildConfirmRequest(ApplyRequest, Review, bUserConfirmed, ConfirmRequest));
	return ConfirmRequest;
}

static FHCIAbilityKitAgentExecuteTicket MakeG9ExecuteTicket(
	const FHCIAbilityKitAgentApplyConfirmRequest& ConfirmRequest,
	const FHCIAbilityKitAgentApplyRequest& ApplyRequest,
	const FHCIAbilityKitDryRunDiffReport& Review)
{
	FHCIAbilityKitAgentExecuteTicket ExecuteTicket;
	check(FHCIAbilityKitAgentExecutorExecuteTicketBridge::BuildExecuteTicket(ConfirmRequest, ApplyRequest, Review, ExecuteTicket));
	return ExecuteTicket;
}

static FHCIAbilityKitAgentSimulateExecuteReceipt MakeG9Receipt(
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

static FHCIAbilityKitAgentSimulateExecuteFinalReport MakeG9FinalReport(
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

static FHCIAbilityKitAgentSimulateExecuteArchiveBundle MakeG9ArchiveBundle(
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

static FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope MakeG9HandoffEnvelope(
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

static FHCIAbilityKitAgentStageGExecuteIntent MakeG9StageGIntent(
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

static FHCIAbilityKitAgentStageGWriteEnableRequest MakeG9WriteEnableRequest(
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

static FHCIAbilityKitAgentStageGExecutePermitTicket MakeG9PermitTicket(
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

static FHCIAbilityKitAgentStageGExecuteDispatchRequest MakeG9DispatchRequest(
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

static FHCIAbilityKitAgentStageGExecuteDispatchReceipt MakeG9DispatchReceipt(
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

static FHCIAbilityKitAgentStageGExecuteCommitRequest MakeG9CommitRequest(
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

static FHCIAbilityKitAgentStageGExecuteCommitReceipt MakeG9CommitReceipt(
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

static FHCIAbilityKitAgentStageGExecuteFinalReport MakeG9StageGExecuteFinalReport(
	const FHCIAbilityKitAgentStageGExecuteCommitReceipt& CommitReceipt,
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
	FHCIAbilityKitAgentStageGExecuteFinalReport StageGFinalReport;
	check(FHCIAbilityKitAgentExecutorStageGExecuteFinalReportBridge::BuildStageGExecuteFinalReport(
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

static FHCIAbilityKitAgentStageGExecuteArchiveBundle MakeReadyG9StageGExecuteArchiveBundle(const FHCIAbilityKitDryRunDiffReport& Review)
{
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeG9ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeG9ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAbilityKitAgentExecuteTicket ExecuteTicket = MakeG9ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteReceipt Receipt = MakeG9Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteFinalReport FinalReport = MakeG9FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeG9ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = MakeG9HandoffEnvelope(ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGExecuteIntent Intent = MakeG9StageGIntent(HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGWriteEnableRequest WriteEnableRequest = MakeG9WriteEnableRequest(Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAbilityKitAgentStageGExecutePermitTicket PermitTicket = MakeG9PermitTicket(WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGExecuteDispatchRequest DispatchRequest = MakeG9DispatchRequest(PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAbilityKitAgentStageGExecuteDispatchReceipt DispatchReceipt = MakeG9DispatchReceipt(DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGExecuteCommitRequest CommitRequest = MakeG9CommitRequest(
		DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAbilityKitAgentStageGExecuteCommitReceipt CommitReceipt = MakeG9CommitReceipt(
		CommitRequest, DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGExecuteFinalReport StageGFinalReport = MakeG9StageGExecuteFinalReport(
		CommitReceipt, CommitRequest, DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);

	FHCIAbilityKitAgentStageGExecuteArchiveBundle Request;
	check(FHCIAbilityKitAgentExecutorStageGExecuteArchiveBundleBridge::BuildStageGExecuteArchiveBundle(
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
	FHCIAbilityKitAgentExecutorStageGExecuteArchiveBundleReadyTest,
	"HCIAbilityKit.Editor.AgentExecutorStageGExecuteArchiveBundle.ReadyWhenStageGExecuteCommitReceiptReady",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorStageGExecuteArchiveBundleReadyTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeG9SelectedReviewReport();
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeG9ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeG9ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAbilityKitAgentExecuteTicket ExecuteTicket = MakeG9ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteReceipt Receipt = MakeG9Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteFinalReport FinalReport = MakeG9FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeG9ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = MakeG9HandoffEnvelope(ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGExecuteIntent Intent = MakeG9StageGIntent(HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGWriteEnableRequest WriteEnableRequest = MakeG9WriteEnableRequest(Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAbilityKitAgentStageGExecutePermitTicket PermitTicket = MakeG9PermitTicket(WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGExecuteDispatchRequest DispatchRequest = MakeG9DispatchRequest(PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAbilityKitAgentStageGExecuteDispatchReceipt DispatchReceipt = MakeG9DispatchReceipt(DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGExecuteCommitRequest CommitRequest = MakeG9CommitRequest(
		DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAbilityKitAgentStageGExecuteCommitReceipt CommitReceipt = MakeG9CommitReceipt(
		CommitRequest, DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGExecuteFinalReport StageGFinalReport = MakeG9StageGExecuteFinalReport(
		CommitReceipt, CommitRequest, DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);

	FHCIAbilityKitAgentStageGExecuteArchiveBundle Request;
	TestTrue(TEXT("Build stage g execute archive bundle"), FHCIAbilityKitAgentExecutorStageGExecuteArchiveBundleBridge::BuildStageGExecuteArchiveBundle(
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
	FHCIAbilityKitAgentExecutorStageGExecuteArchiveBundleWriteEnableNotReadyTest,
	"HCIAbilityKit.Editor.AgentExecutorStageGExecuteArchiveBundle.StageGExecuteCommitReceiptNotReadyReturnsE4217",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorStageGExecuteArchiveBundleWriteEnableNotReadyTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeG9SelectedReviewReport();
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeG9ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeG9ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAbilityKitAgentExecuteTicket ExecuteTicket = MakeG9ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteReceipt Receipt = MakeG9Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteFinalReport FinalReport = MakeG9FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeG9ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = MakeG9HandoffEnvelope(ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGExecuteIntent Intent = MakeG9StageGIntent(HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGWriteEnableRequest WriteEnableRequest = MakeG9WriteEnableRequest(Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAbilityKitAgentStageGExecutePermitTicket PermitTicket = MakeG9PermitTicket(WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGExecuteDispatchRequest DispatchRequest = MakeG9DispatchRequest(PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAbilityKitAgentStageGExecuteDispatchReceipt DispatchReceipt = MakeG9DispatchReceipt(DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	FHCIAbilityKitAgentStageGExecuteCommitRequest CommitRequest = MakeG9CommitRequest(
		DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	FHCIAbilityKitAgentStageGExecuteCommitReceipt CommitReceipt = MakeG9CommitReceipt(
		CommitRequest, DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	FHCIAbilityKitAgentStageGExecuteFinalReport StageGFinalReport = MakeG9StageGExecuteFinalReport(
		CommitReceipt, CommitRequest, DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	StageGFinalReport.bWriteEnabled = false;
	StageGFinalReport.bReadyForStageGExecute = false;
	StageGFinalReport.bStageGExecuteFinalized = false;
	StageGFinalReport.bStageGExecuteFinalReportReady = false;
	StageGFinalReport.StageGExecuteFinalReportStatus = TEXT("blocked");

	FHCIAbilityKitAgentStageGExecuteArchiveBundle Request;
	TestTrue(TEXT("Build stage g execute archive bundle"), FHCIAbilityKitAgentExecutorStageGExecuteArchiveBundleBridge::BuildStageGExecuteArchiveBundle(
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
	FHCIAbilityKitAgentExecutorStageGExecuteArchiveBundleDigestMismatchTest,
	"HCIAbilityKit.Editor.AgentExecutorStageGExecuteArchiveBundle.DigestMismatchReturnsE4202",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorStageGExecuteArchiveBundleDigestMismatchTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeG9SelectedReviewReport();
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeG9ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeG9ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAbilityKitAgentExecuteTicket ExecuteTicket = MakeG9ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteReceipt Receipt = MakeG9Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteFinalReport FinalReport = MakeG9FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeG9ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = MakeG9HandoffEnvelope(ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGExecuteIntent Intent = MakeG9StageGIntent(HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGWriteEnableRequest WriteEnableRequest = MakeG9WriteEnableRequest(Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAbilityKitAgentStageGExecutePermitTicket PermitTicket = MakeG9PermitTicket(WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGExecuteDispatchRequest DispatchRequest = MakeG9DispatchRequest(PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAbilityKitAgentStageGExecuteDispatchReceipt DispatchReceipt = MakeG9DispatchReceipt(DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	FHCIAbilityKitAgentStageGExecuteCommitRequest CommitRequest = MakeG9CommitRequest(
		DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	FHCIAbilityKitAgentStageGExecuteCommitReceipt CommitReceipt = MakeG9CommitReceipt(
		CommitRequest, DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	FHCIAbilityKitAgentStageGExecuteFinalReport StageGFinalReport = MakeG9StageGExecuteFinalReport(
		CommitReceipt, CommitRequest, DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	StageGFinalReport.SelectionDigest = TEXT("crc32_BAD0C0DE");

	FHCIAbilityKitAgentStageGExecuteArchiveBundle Request;
	TestTrue(TEXT("Build stage g execute archive bundle"), FHCIAbilityKitAgentExecutorStageGExecuteArchiveBundleBridge::BuildStageGExecuteArchiveBundle(
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
	FHCIAbilityKitAgentExecutorStageGExecuteArchiveBundleJsonFieldsTest,
	"HCIAbilityKit.Editor.AgentExecutorStageGExecuteArchiveBundle.JsonIncludesFinalReportFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorStageGExecuteArchiveBundleJsonFieldsTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeG9SelectedReviewReport();
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = MakeG9ApplyRequest(Review);
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = MakeG9ConfirmRequest(ApplyRequest, Review, true);
	const FHCIAbilityKitAgentExecuteTicket ExecuteTicket = MakeG9ExecuteTicket(ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteReceipt Receipt = MakeG9Receipt(ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteFinalReport FinalReport = MakeG9FinalReport(Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteArchiveBundle ArchiveBundle = MakeG9ArchiveBundle(FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = MakeG9HandoffEnvelope(ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGExecuteIntent Intent = MakeG9StageGIntent(HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGWriteEnableRequest WriteEnableRequest = MakeG9WriteEnableRequest(Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAbilityKitAgentStageGExecutePermitTicket PermitTicket = MakeG9PermitTicket(WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGExecuteDispatchRequest DispatchRequest = MakeG9DispatchRequest(PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAbilityKitAgentStageGExecuteDispatchReceipt DispatchReceipt = MakeG9DispatchReceipt(DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGExecuteCommitRequest CommitRequest = MakeG9CommitRequest(
		DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review, true);
	const FHCIAbilityKitAgentStageGExecuteCommitReceipt CommitReceipt = MakeG9CommitReceipt(
		CommitRequest, DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);
	const FHCIAbilityKitAgentStageGExecuteFinalReport StageGFinalReport = MakeG9StageGExecuteFinalReport(
		CommitReceipt, CommitRequest, DispatchReceipt, DispatchRequest, PermitTicket, WriteEnableRequest, Intent, HandoffEnvelope, ArchiveBundle, FinalReport, Receipt, ExecuteTicket, ConfirmRequest, ApplyRequest, Review);

	FHCIAbilityKitAgentStageGExecuteArchiveBundle Request;
	TestTrue(TEXT("Build stage g execute archive bundle"), FHCIAbilityKitAgentExecutorStageGExecuteArchiveBundleBridge::BuildStageGExecuteArchiveBundle(
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
	TestTrue(TEXT("Serialize stage g execute archive bundle json"), FHCIAbilityKitAgentStageGExecuteArchiveBundleJsonSerializer::SerializeToJsonString(Request, JsonText));
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
	FHCIAbilityKitAgentExecutorStageGExecutionReadinessReadyTest,
	"HCIAbilityKit.Editor.AgentExecutorStageGExecutionReadiness.ReadyWhenArchiveBundleReady",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorStageGExecutionReadinessReadyTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeG9SelectedReviewReport();
	const FHCIAbilityKitAgentStageGExecuteArchiveBundle ArchiveBundle = MakeReadyG9StageGExecuteArchiveBundle(Review);

	FHCIAbilityKitAgentStageGExecutionReadinessReport Report;
	TestTrue(TEXT("Build stage g execution readiness"), FHCIAbilityKitAgentExecutorStageGExecutionReadinessReportBridge::BuildStageGExecutionReadinessReport(
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
	FHCIAbilityKitAgentExecutorStageGExecutionReadinessArchiveNotReadyTest,
	"HCIAbilityKit.Editor.AgentExecutorStageGExecutionReadiness.ArchiveNotReadyReturnsE4218",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorStageGExecutionReadinessArchiveNotReadyTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeG9SelectedReviewReport();
	FHCIAbilityKitAgentStageGExecuteArchiveBundle ArchiveBundle = MakeReadyG9StageGExecuteArchiveBundle(Review);
	ArchiveBundle.bStageGExecuteArchiveBundleReady = false;
	ArchiveBundle.bStageGExecuteArchived = false;
	ArchiveBundle.StageGExecuteArchiveBundleStatus = TEXT("blocked");

	FHCIAbilityKitAgentStageGExecutionReadinessReport Report;
	TestTrue(TEXT("Build stage g execution readiness"), FHCIAbilityKitAgentExecutorStageGExecutionReadinessReportBridge::BuildStageGExecutionReadinessReport(
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
	FHCIAbilityKitAgentExecutorStageGExecutionReadinessModeBlockedTest,
	"HCIAbilityKit.Editor.AgentExecutorStageGExecutionReadiness.WriteModeBlockedReturnsE4219",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorStageGExecutionReadinessModeBlockedTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeG9SelectedReviewReport();
	FHCIAbilityKitAgentStageGExecuteArchiveBundle ArchiveBundle = MakeReadyG9StageGExecuteArchiveBundle(Review);
	ArchiveBundle.ExecutionMode = TEXT("write_execute");

	FHCIAbilityKitAgentStageGExecutionReadinessReport Report;
	TestTrue(TEXT("Build stage g execution readiness"), FHCIAbilityKitAgentExecutorStageGExecutionReadinessReportBridge::BuildStageGExecutionReadinessReport(
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
	FHCIAbilityKitAgentExecutorStageGExecutionReadinessUnconfirmedTest,
	"HCIAbilityKit.Editor.AgentExecutorStageGExecutionReadiness.UnconfirmedReturnsE4005",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorStageGExecutionReadinessUnconfirmedTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeG9SelectedReviewReport();
	const FHCIAbilityKitAgentStageGExecuteArchiveBundle ArchiveBundle = MakeReadyG9StageGExecuteArchiveBundle(Review);

	FHCIAbilityKitAgentStageGExecutionReadinessReport Report;
	TestTrue(TEXT("Build stage g execution readiness"), FHCIAbilityKitAgentExecutorStageGExecutionReadinessReportBridge::BuildStageGExecutionReadinessReport(
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
	FHCIAbilityKitAgentExecutorStageGExecutionReadinessJsonFieldsTest,
	"HCIAbilityKit.Editor.AgentExecutorStageGExecutionReadiness.JsonIncludesReadinessFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorStageGExecutionReadinessJsonFieldsTest::RunTest(const FString& Parameters)
{
	const FHCIAbilityKitDryRunDiffReport Review = MakeG9SelectedReviewReport();
	const FHCIAbilityKitAgentStageGExecuteArchiveBundle ArchiveBundle = MakeReadyG9StageGExecuteArchiveBundle(Review);

	FHCIAbilityKitAgentStageGExecutionReadinessReport Report;
	TestTrue(TEXT("Build stage g execution readiness"), FHCIAbilityKitAgentExecutorStageGExecutionReadinessReportBridge::BuildStageGExecutionReadinessReport(
		ArchiveBundle,
		ArchiveBundle.RequestId,
		Review,
		true,
		Report));

	FString JsonText;
	TestTrue(TEXT("Serialize stage g execution readiness json"), FHCIAbilityKitAgentStageGExecutionReadinessReportJsonSerializer::SerializeToJsonString(Report, JsonText));
	TestTrue(TEXT("JSON includes stage_g_execute_archive_bundle_id"), JsonText.Contains(TEXT("\"stage_g_execute_archive_bundle_id\"")));
	TestTrue(TEXT("JSON includes stage_g_execution_readiness_digest"), JsonText.Contains(TEXT("\"stage_g_execution_readiness_digest\"")));
	TestTrue(TEXT("JSON includes stage_g_execution_readiness_status"), JsonText.Contains(TEXT("\"stage_g_execution_readiness_status\"")));
	TestTrue(TEXT("JSON includes ready_for_h1_planner_integration"), JsonText.Contains(TEXT("\"ready_for_h1_planner_integration\"")));
	TestTrue(TEXT("JSON includes execution_mode"), JsonText.Contains(TEXT("\"execution_mode\"")));
	TestTrue(TEXT("JSON includes locate_strategy"), JsonText.Contains(TEXT("\"locate_strategy\"")));
	return true;
}

#endif





