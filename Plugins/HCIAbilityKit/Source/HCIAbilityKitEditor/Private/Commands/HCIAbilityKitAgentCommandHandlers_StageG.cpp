#include "Commands/HCIAbilityKitAgentCommandHandlers.h"
#include "Commands/HCIAbilityKitAgentDemoState.h"

#include "Agent/Contracts/StageF/HCIAbilityKitAgentApplyConfirmRequest.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentApplyRequest.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentExecuteTicket.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentSimulateExecuteArchiveBundle.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentSimulateExecuteFinalReport.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentSimulateExecuteHandoffEnvelope.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentSimulateExecuteReceipt.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGExecuteIntent.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGExecuteIntentJsonSerializer.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGWriteEnableRequest.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGWriteEnableRequestJsonSerializer.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGExecutePermitTicket.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGExecutePermitTicketJsonSerializer.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGExecuteDispatchRequest.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGExecuteDispatchRequestJsonSerializer.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGExecuteDispatchReceipt.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGExecuteDispatchReceiptJsonSerializer.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGExecuteCommitRequest.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGExecuteCommitRequestJsonSerializer.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGExecuteCommitReceipt.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGExecuteCommitReceiptJsonSerializer.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGExecuteFinalReport.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGExecuteFinalReportJsonSerializer.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGExecuteArchiveBundle.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGExecuteArchiveBundleJsonSerializer.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGExecutionReadinessReport.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGExecutionReadinessReportJsonSerializer.h"
#include "Agent/Bridges/HCIAbilityKitAgentExecutorStageGExecuteIntentBridge.h"
#include "Agent/Bridges/HCIAbilityKitAgentExecutorStageGWriteEnableRequestBridge.h"
#include "Agent/Bridges/HCIAbilityKitAgentExecutorStageGExecutePermitTicketBridge.h"
#include "Agent/Bridges/HCIAbilityKitAgentExecutorStageGExecuteDispatchRequestBridge.h"
#include "Agent/Bridges/HCIAbilityKitAgentExecutorStageGExecuteDispatchReceiptBridge.h"
#include "Agent/Bridges/HCIAbilityKitAgentExecutorStageGExecuteCommitRequestBridge.h"
#include "Agent/Bridges/HCIAbilityKitAgentExecutorStageGExecuteCommitReceiptBridge.h"
#include "Agent/Bridges/HCIAbilityKitAgentExecutorStageGExecuteFinalReportBridge.h"
#include "Agent/Bridges/HCIAbilityKitAgentExecutorStageGExecuteArchiveBundleBridge.h"
#include "Agent/Bridges/HCIAbilityKitAgentExecutorStageGExecutionReadinessReportBridge.h"
#include "Agent/Executor/HCIAbilityKitDryRunDiff.h"

DEFINE_LOG_CATEGORY_STATIC(LogHCIAbilityKitAgentDemoStageG, Log, All);
#ifdef LogHCIAbilityKitAgentDemo
#undef LogHCIAbilityKitAgentDemo
#endif
#define LogHCIAbilityKitAgentDemo LogHCIAbilityKitAgentDemoStageG

static FHCIAbilityKitAgentDemoState& HCI_StageG_State()
{
    return HCI_GetAgentDemoState();
}

// Helpers defined in HCIAbilityKitAgentDemoConsoleCommands.cpp
bool HCI_TryParseBool01Arg(const FString& InValue, bool& OutValue);
bool HCI_TryBuildAgentExecutorApplyRequestFromLatestReview(FHCIAbilityKitAgentApplyRequest& OutApplyRequest);
bool HCI_TryBuildAgentExecutorConfirmRequestFromLatestPreview(
    bool bUserConfirmed,
    const FString& TamperMode,
    FHCIAbilityKitAgentApplyConfirmRequest& OutConfirmRequest);
bool HCI_TryBuildAgentExecutorExecuteTicketFromLatestPreview(
    const FString& TamperMode,
    FHCIAbilityKitAgentExecuteTicket& OutExecuteTicket);
bool HCI_TryBuildAgentExecutorSimExecuteReceiptFromLatestPreview(
    const FString& TamperMode,
    FHCIAbilityKitAgentSimulateExecuteReceipt& OutReceipt);
bool HCI_TryBuildAgentExecutorSimFinalReportFromLatestPreview(
    const FString& TamperMode,
    FHCIAbilityKitAgentSimulateExecuteFinalReport& OutReport);
bool HCI_TryBuildAgentExecutorSimArchiveBundleFromLatestPreview(
    const FString& TamperMode,
    FHCIAbilityKitAgentSimulateExecuteArchiveBundle& OutBundle);
bool HCI_TryBuildAgentExecutorSimHandoffEnvelopeFromLatestPreview(
    const FString& TamperMode,
    FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope& OutEnvelope);

static bool HCI_TryParseStageGExecuteIntentTamperModeArg(const FString& InValue, FString& OutTamperMode)
{
	const FString Normalized = InValue.TrimStartAndEnd().ToLower();
	if (Normalized == TEXT("none") || Normalized == TEXT("digest") || Normalized == TEXT("apply") ||
		Normalized == TEXT("review") || Normalized == TEXT("confirm") || Normalized == TEXT("receipt") ||
		Normalized == TEXT("final") || Normalized == TEXT("archive") || Normalized == TEXT("handoff") ||
		Normalized == TEXT("ready"))
	{
		OutTamperMode = Normalized;
		return true;
	}
	return false;
}

static void HCI_LogAgentExecutorStageGExecuteIntentSummary(const FHCIAbilityKitAgentStageGExecuteIntent& Intent)
{
	const FString SummaryLine = FString::Printf(
		TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteIntent] summary request_id=%s sim_handoff_envelope_id=%s sim_archive_bundle_id=%s sim_final_report_id=%s sim_execute_receipt_id=%s execute_ticket_id=%s confirm_request_id=%s apply_request_id=%s review_request_id=%s selection_digest=%s archive_digest=%s handoff_digest=%s execute_intent_digest=%s execute_target=%s handoff_target=%s user_confirmed=%s ready_to_simulate_execute=%s simulated_dispatch_accepted=%s simulation_completed=%s terminal_status=%s archive_status=%s handoff_status=%s stage_g_status=%s archive_ready=%s handoff_ready=%s write_enabled=%s ready_for_stage_g_entry=%s error_code=%s reason=%s validation=ok"),
		Intent.RequestId.IsEmpty() ? TEXT("-") : *Intent.RequestId,
		Intent.SimHandoffEnvelopeId.IsEmpty() ? TEXT("-") : *Intent.SimHandoffEnvelopeId,
		Intent.SimArchiveBundleId.IsEmpty() ? TEXT("-") : *Intent.SimArchiveBundleId,
		Intent.SimFinalReportId.IsEmpty() ? TEXT("-") : *Intent.SimFinalReportId,
		Intent.SimExecuteReceiptId.IsEmpty() ? TEXT("-") : *Intent.SimExecuteReceiptId,
		Intent.ExecuteTicketId.IsEmpty() ? TEXT("-") : *Intent.ExecuteTicketId,
		Intent.ConfirmRequestId.IsEmpty() ? TEXT("-") : *Intent.ConfirmRequestId,
		Intent.ApplyRequestId.IsEmpty() ? TEXT("-") : *Intent.ApplyRequestId,
		Intent.ReviewRequestId.IsEmpty() ? TEXT("-") : *Intent.ReviewRequestId,
		Intent.SelectionDigest.IsEmpty() ? TEXT("-") : *Intent.SelectionDigest,
		Intent.ArchiveDigest.IsEmpty() ? TEXT("-") : *Intent.ArchiveDigest,
		Intent.HandoffDigest.IsEmpty() ? TEXT("-") : *Intent.HandoffDigest,
		Intent.ExecuteIntentDigest.IsEmpty() ? TEXT("-") : *Intent.ExecuteIntentDigest,
		Intent.ExecuteTarget.IsEmpty() ? TEXT("-") : *Intent.ExecuteTarget,
		Intent.HandoffTarget.IsEmpty() ? TEXT("-") : *Intent.HandoffTarget,
		Intent.bUserConfirmed ? TEXT("true") : TEXT("false"),
		Intent.bReadyToSimulateExecute ? TEXT("true") : TEXT("false"),
		Intent.bSimulatedDispatchAccepted ? TEXT("true") : TEXT("false"),
		Intent.bSimulationCompleted ? TEXT("true") : TEXT("false"),
		Intent.TerminalStatus.IsEmpty() ? TEXT("-") : *Intent.TerminalStatus,
		Intent.ArchiveStatus.IsEmpty() ? TEXT("-") : *Intent.ArchiveStatus,
		Intent.HandoffStatus.IsEmpty() ? TEXT("-") : *Intent.HandoffStatus,
		Intent.StageGStatus.IsEmpty() ? TEXT("-") : *Intent.StageGStatus,
		Intent.bArchiveReady ? TEXT("true") : TEXT("false"),
		Intent.bHandoffReady ? TEXT("true") : TEXT("false"),
		Intent.bWriteEnabled ? TEXT("true") : TEXT("false"),
		Intent.bReadyForStageGEntry ? TEXT("true") : TEXT("false"),
		Intent.ErrorCode.IsEmpty() ? TEXT("-") : *Intent.ErrorCode,
		Intent.Reason.IsEmpty() ? TEXT("-") : *Intent.Reason);
	if (Intent.bReadyForStageGEntry)
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("%s"), *SummaryLine);
		return;
	}
	UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("%s"), *SummaryLine);
}

static void HCI_LogAgentExecutorStageGExecuteIntentRows(const FHCIAbilityKitAgentStageGExecuteIntent& Intent)
{
	for (const FHCIAbilityKitAgentApplyRequestItem& Item : Intent.Items)
	{
		if (Item.bBlocked)
		{
			UE_LOG(
				LogHCIAbilityKitAgentDemo,
				Warning,
				TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteIntent] row=%d tool_name=%s risk=%s blocked=%s skip_reason=%s object_type=%s locate_strategy=%s asset_path=%s field=%s evidence_key=%s actor_path=%s"),
				Item.RowIndex,
				Item.ToolName.IsEmpty() ? TEXT("-") : *Item.ToolName,
				*FHCIAbilityKitDryRunDiff::RiskToString(Item.Risk),
				TEXT("true"),
				Item.SkipReason.IsEmpty() ? TEXT("-") : *Item.SkipReason,
				*FHCIAbilityKitDryRunDiff::ObjectTypeToString(Item.ObjectType),
				*FHCIAbilityKitDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
				Item.AssetPath.IsEmpty() ? TEXT("-") : *Item.AssetPath,
				Item.Field.IsEmpty() ? TEXT("-") : *Item.Field,
				Item.EvidenceKey.IsEmpty() ? TEXT("-") : *Item.EvidenceKey,
				Item.ActorPath.IsEmpty() ? TEXT("-") : *Item.ActorPath);
			continue;
		}

		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteIntent] row=%d tool_name=%s risk=%s blocked=%s skip_reason=%s object_type=%s locate_strategy=%s asset_path=%s field=%s evidence_key=%s actor_path=%s"),
			Item.RowIndex,
			Item.ToolName.IsEmpty() ? TEXT("-") : *Item.ToolName,
			*FHCIAbilityKitDryRunDiff::RiskToString(Item.Risk),
			TEXT("false"),
			Item.SkipReason.IsEmpty() ? TEXT("-") : *Item.SkipReason,
			*FHCIAbilityKitDryRunDiff::ObjectTypeToString(Item.ObjectType),
			*FHCIAbilityKitDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
			Item.AssetPath.IsEmpty() ? TEXT("-") : *Item.AssetPath,
			Item.Field.IsEmpty() ? TEXT("-") : *Item.Field,
			Item.EvidenceKey.IsEmpty() ? TEXT("-") : *Item.EvidenceKey,
			Item.ActorPath.IsEmpty() ? TEXT("-") : *Item.ActorPath);
	}
}

static bool HCI_TryBuildAgentExecutorStageGExecuteIntentFromLatestPreview(const FString& TamperMode, FHCIAbilityKitAgentStageGExecuteIntent& OutIntent)
{
	if (HCI_StageG_State().AgentSimulateExecuteHandoffEnvelopePreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteIntent] prepare=unavailable reason=no_sim_handoff_envelope_preview_state"));
		UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteIntent] suggestion=先运行 HCIAbilityKit.AgentExecutePlanReviewPrepareSimHandoffEnvelope"));
		return false;
	}
	if (HCI_StageG_State().AgentSimulateExecuteArchiveBundlePreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteIntent] prepare=unavailable reason=no_sim_archive_bundle_preview_state"));
		UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteIntent] suggestion=先运行 HCIAbilityKit.AgentExecutePlanReviewPrepareSimArchiveBundle"));
		return false;
	}
	if (HCI_StageG_State().AgentSimulateExecuteFinalReportPreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteIntent] prepare=unavailable reason=no_sim_final_report_preview_state"));
		UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteIntent] suggestion=先运行 HCIAbilityKit.AgentExecutePlanReviewPrepareSimFinalReport"));
		return false;
	}
	if (HCI_StageG_State().AgentSimulateExecuteReceiptPreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteIntent] prepare=unavailable reason=no_sim_execute_receipt_preview_state"));
		UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteIntent] suggestion=先运行 HCIAbilityKit.AgentExecutePlanReviewPrepareSimExecuteReceipt"));
		return false;
	}
	if (HCI_StageG_State().AgentExecuteTicketPreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteIntent] prepare=unavailable reason=no_execute_ticket_preview_state"));
		UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteIntent] suggestion=先运行 HCIAbilityKit.AgentExecutePlanReviewPrepareExecuteTicket"));
		return false;
	}
	if (HCI_StageG_State().AgentApplyConfirmRequestPreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteIntent] prepare=unavailable reason=no_confirm_request_preview_state"));
		UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteIntent] suggestion=先运行 HCIAbilityKit.AgentExecutePlanReviewPrepareConfirm"));
		return false;
	}
	if (HCI_StageG_State().AgentApplyRequestPreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteIntent] prepare=unavailable reason=no_apply_request_preview_state"));
		UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteIntent] suggestion=先运行 HCIAbilityKit.AgentExecutePlanReviewPrepareApply"));
		return false;
	}
	if (HCI_StageG_State().AgentExecutorReviewDiffPreviewState.DiffItems.Num() <= 0)
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteIntent] prepare=unavailable reason=no_review_preview_state"));
		UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteIntent] suggestion=先运行 HCIAbilityKit.AgentExecutePlanReviewDemo 或 HCIAbilityKit.AgentExecutePlanReviewSelect"));
		return false;
	}

	FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope WorkingHandoff = HCI_StageG_State().AgentSimulateExecuteHandoffEnvelopePreviewState;
	if (TamperMode == TEXT("digest")) { WorkingHandoff.SelectionDigest += TEXT("_tampered"); }
	else if (TamperMode == TEXT("apply")) { WorkingHandoff.ApplyRequestId += TEXT("_stale"); }
	else if (TamperMode == TEXT("review")) { WorkingHandoff.ReviewRequestId += TEXT("_stale"); }
	else if (TamperMode == TEXT("confirm")) { WorkingHandoff.ConfirmRequestId += TEXT("_stale"); }
	else if (TamperMode == TEXT("receipt")) { WorkingHandoff.bSimulatedDispatchAccepted = false; }
	else if (TamperMode == TEXT("final")) { WorkingHandoff.bSimulationCompleted = false; WorkingHandoff.TerminalStatus = TEXT("blocked"); }
	else if (TamperMode == TEXT("archive")) { WorkingHandoff.bArchiveReady = false; WorkingHandoff.ArchiveStatus = TEXT("blocked"); }
	else if (TamperMode == TEXT("handoff")) { WorkingHandoff.bHandoffReady = false; WorkingHandoff.HandoffStatus = TEXT("blocked"); }
	else if (TamperMode == TEXT("ready")) { WorkingHandoff.bReadyToSimulateExecute = false; }

	if (!FHCIAbilityKitAgentExecutorStageGExecuteIntentBridge::BuildStageGExecuteIntent(
		WorkingHandoff,
		HCI_StageG_State().AgentSimulateExecuteArchiveBundlePreviewState,
		HCI_StageG_State().AgentSimulateExecuteFinalReportPreviewState,
		HCI_StageG_State().AgentSimulateExecuteReceiptPreviewState,
		HCI_StageG_State().AgentExecuteTicketPreviewState,
		HCI_StageG_State().AgentApplyConfirmRequestPreviewState,
		HCI_StageG_State().AgentApplyRequestPreviewState,
		HCI_StageG_State().AgentExecutorReviewDiffPreviewState,
		OutIntent))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteIntent] prepare_failed reason=bridge_failed"));
		return false;
	}

	HCI_StageG_State().AgentStageGExecuteIntentPreviewState = OutIntent;
	return true;
}

static bool HCI_TryParseStageGExecuteIntentCommandArgs(const TArray<FString>& Args, FString& OutTamperMode)
{
	OutTamperMode = TEXT("none");
	if (Args.Num() == 0)
	{
		return true;
	}
	if (Args.Num() != 1)
	{
		return false;
	}
	return HCI_TryParseStageGExecuteIntentTamperModeArg(Args[0], OutTamperMode);
}

void HCI_RunAbilityKitAgentExecutePlanReviewPrepareStageGExecuteIntentCommand(const TArray<FString>& Args)
{
	FString TamperMode;
	if (!HCI_TryParseStageGExecuteIntentCommandArgs(Args, TamperMode))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteIntent] invalid_args usage=HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteIntent [tamper=none|digest|apply|review|confirm|receipt|final|archive|handoff|ready]"));
		return;
	}

	FHCIAbilityKitAgentStageGExecuteIntent Intent;
	if (!HCI_TryBuildAgentExecutorStageGExecuteIntentFromLatestPreview(TamperMode, Intent))
	{
		return;
	}

	HCI_LogAgentExecutorStageGExecuteIntentSummary(Intent);
	HCI_LogAgentExecutorStageGExecuteIntentRows(Intent);
	UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteIntent] hint=也可运行 HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteIntentJson [tamper] 输出 StageGExecuteIntent JSON"));
}

void HCI_RunAbilityKitAgentExecutePlanReviewPrepareStageGExecuteIntentJsonCommand(const TArray<FString>& Args)
{
	FString TamperMode;
	if (!HCI_TryParseStageGExecuteIntentCommandArgs(Args, TamperMode))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteIntent] invalid_args usage=HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteIntentJson [tamper=none|digest|apply|review|confirm|receipt|final|archive|handoff|ready]"));
		return;
	}

	FHCIAbilityKitAgentStageGExecuteIntent Intent;
	if (!HCI_TryBuildAgentExecutorStageGExecuteIntentFromLatestPreview(TamperMode, Intent))
	{
		return;
	}

	HCI_LogAgentExecutorStageGExecuteIntentSummary(Intent);
	HCI_LogAgentExecutorStageGExecuteIntentRows(Intent);

	FString JsonText;
	if (!FHCIAbilityKitAgentStageGExecuteIntentJsonSerializer::SerializeToJsonString(Intent, JsonText))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteIntent] json_failed request_id=%s"), Intent.RequestId.IsEmpty() ? TEXT("-") : *Intent.RequestId);
		return;
	}

	UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteIntent] json request_id=%s bytes=%d"), Intent.RequestId.IsEmpty() ? TEXT("-") : *Intent.RequestId, JsonText.Len());
	UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("%s"), *JsonText);
}

static bool HCI_TryParseStageGWriteEnableTamperModeArg(const FString& InValue, FString& OutTamperMode)
{
	const FString V = InValue.TrimStartAndEnd();
	if (V == TEXT("none") || V == TEXT("digest") || V == TEXT("intent") || V == TEXT("handoff") || V == TEXT("ready"))
	{
		OutTamperMode = V;
		return true;
	}
	return false;
}

static void HCI_LogAgentExecutorStageGWriteEnableRequestSummary(const FHCIAbilityKitAgentStageGWriteEnableRequest& Request)
{
	UE_LOG(
		LogHCIAbilityKitAgentDemo,
		Display,
		TEXT("[HCIAbilityKit][AgentExecutorStageGWriteEnableRequest] summary request_id=%s stage_g_execute_intent_id=%s sim_handoff_envelope_id=%s sim_archive_bundle_id=%s sim_final_report_id=%s sim_execute_receipt_id=%s execute_ticket_id=%s confirm_request_id=%s apply_request_id=%s review_request_id=%s selection_digest=%s archive_digest=%s handoff_digest=%s execute_intent_digest=%s stage_g_write_enable_digest=%s execute_target=%s handoff_target=%s user_confirmed=%s write_enable_confirmed=%s ready_to_simulate_execute=%s simulated_dispatch_accepted=%s simulation_completed=%s terminal_status=%s archive_status=%s handoff_status=%s stage_g_status=%s stage_g_write_status=%s archive_ready=%s handoff_ready=%s write_enabled=%s ready_for_stage_g_entry=%s ready_for_stage_g_execute=%s error_code=%s reason=%s validation=ok"),
		Request.RequestId.IsEmpty() ? TEXT("-") : *Request.RequestId,
		Request.StageGExecuteIntentId.IsEmpty() ? TEXT("-") : *Request.StageGExecuteIntentId,
		Request.SimHandoffEnvelopeId.IsEmpty() ? TEXT("-") : *Request.SimHandoffEnvelopeId,
		Request.SimArchiveBundleId.IsEmpty() ? TEXT("-") : *Request.SimArchiveBundleId,
		Request.SimFinalReportId.IsEmpty() ? TEXT("-") : *Request.SimFinalReportId,
		Request.SimExecuteReceiptId.IsEmpty() ? TEXT("-") : *Request.SimExecuteReceiptId,
		Request.ExecuteTicketId.IsEmpty() ? TEXT("-") : *Request.ExecuteTicketId,
		Request.ConfirmRequestId.IsEmpty() ? TEXT("-") : *Request.ConfirmRequestId,
		Request.ApplyRequestId.IsEmpty() ? TEXT("-") : *Request.ApplyRequestId,
		Request.ReviewRequestId.IsEmpty() ? TEXT("-") : *Request.ReviewRequestId,
		Request.SelectionDigest.IsEmpty() ? TEXT("-") : *Request.SelectionDigest,
		Request.ArchiveDigest.IsEmpty() ? TEXT("-") : *Request.ArchiveDigest,
		Request.HandoffDigest.IsEmpty() ? TEXT("-") : *Request.HandoffDigest,
		Request.ExecuteIntentDigest.IsEmpty() ? TEXT("-") : *Request.ExecuteIntentDigest,
		Request.StageGWriteEnableDigest.IsEmpty() ? TEXT("-") : *Request.StageGWriteEnableDigest,
		Request.ExecuteTarget.IsEmpty() ? TEXT("-") : *Request.ExecuteTarget,
		Request.HandoffTarget.IsEmpty() ? TEXT("-") : *Request.HandoffTarget,
		Request.bUserConfirmed ? TEXT("true") : TEXT("false"),
		Request.bWriteEnableConfirmed ? TEXT("true") : TEXT("false"),
		Request.bReadyToSimulateExecute ? TEXT("true") : TEXT("false"),
		Request.bSimulatedDispatchAccepted ? TEXT("true") : TEXT("false"),
		Request.bSimulationCompleted ? TEXT("true") : TEXT("false"),
		Request.TerminalStatus.IsEmpty() ? TEXT("-") : *Request.TerminalStatus,
		Request.ArchiveStatus.IsEmpty() ? TEXT("-") : *Request.ArchiveStatus,
		Request.HandoffStatus.IsEmpty() ? TEXT("-") : *Request.HandoffStatus,
		Request.StageGStatus.IsEmpty() ? TEXT("-") : *Request.StageGStatus,
		Request.StageGWriteStatus.IsEmpty() ? TEXT("-") : *Request.StageGWriteStatus,
		Request.bArchiveReady ? TEXT("true") : TEXT("false"),
		Request.bHandoffReady ? TEXT("true") : TEXT("false"),
		Request.bWriteEnabled ? TEXT("true") : TEXT("false"),
		Request.bReadyForStageGEntry ? TEXT("true") : TEXT("false"),
		Request.bReadyForStageGExecute ? TEXT("true") : TEXT("false"),
		Request.ErrorCode.IsEmpty() ? TEXT("-") : *Request.ErrorCode,
		Request.Reason.IsEmpty() ? TEXT("-") : *Request.Reason);
}

static void HCI_LogAgentExecutorStageGWriteEnableRequestRows(const FHCIAbilityKitAgentStageGWriteEnableRequest& Request)
{
	for (int32 Index = 0; Index < Request.Items.Num(); ++Index)
	{
		const FHCIAbilityKitAgentApplyRequestItem& Item = Request.Items[Index];
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentExecutorStageGWriteEnableRequest] row=%d tool_name=%s risk=%s blocked=%s skip_reason=%s object_type=%s locate_strategy=%s asset_path=%s field=%s evidence_key=%s actor_path=%s"),
			Index,
			Item.ToolName.IsEmpty() ? TEXT("-") : *Item.ToolName,
			*FHCIAbilityKitDryRunDiff::RiskToString(Item.Risk),
			Item.bBlocked ? TEXT("true") : TEXT("false"),
			Item.SkipReason.IsEmpty() ? TEXT("-") : *Item.SkipReason,
			*FHCIAbilityKitDryRunDiff::ObjectTypeToString(Item.ObjectType),
			*FHCIAbilityKitDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
			Item.AssetPath.IsEmpty() ? TEXT("-") : *Item.AssetPath,
			Item.Field.IsEmpty() ? TEXT("-") : *Item.Field,
			Item.EvidenceKey.IsEmpty() ? TEXT("-") : *Item.EvidenceKey,
			Item.ActorPath.IsEmpty() ? TEXT("-") : *Item.ActorPath);
	}
}

static bool HCI_TryBuildAgentExecutorStageGWriteEnableRequestFromLatestPreview(
	const bool bWriteEnableConfirmed,
	const FString& TamperMode,
	FHCIAbilityKitAgentStageGWriteEnableRequest& OutRequest)
{
	if (HCI_StageG_State().AgentStageGExecuteIntentPreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorStageGWriteEnableRequest] prepare=unavailable reason=no_stage_g_execute_intent_preview_state"));
		UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGWriteEnableRequest] suggestion=先运行 HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteIntent"));
		return false;
	}
	if (HCI_StageG_State().AgentSimulateExecuteHandoffEnvelopePreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorStageGWriteEnableRequest] prepare=unavailable reason=no_sim_handoff_envelope_preview_state"));
		UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGWriteEnableRequest] suggestion=先运行 HCIAbilityKit.AgentExecutePlanReviewPrepareSimHandoffEnvelope"));
		return false;
	}
	if (HCI_StageG_State().AgentSimulateExecuteArchiveBundlePreviewState.Items.Num() <= 0 ||
		HCI_StageG_State().AgentSimulateExecuteFinalReportPreviewState.Items.Num() <= 0 ||
		HCI_StageG_State().AgentSimulateExecuteReceiptPreviewState.Items.Num() <= 0 ||
		HCI_StageG_State().AgentExecuteTicketPreviewState.Items.Num() <= 0 ||
		HCI_StageG_State().AgentApplyConfirmRequestPreviewState.Items.Num() <= 0 ||
		HCI_StageG_State().AgentApplyRequestPreviewState.Items.Num() <= 0 ||
		HCI_StageG_State().AgentExecutorReviewDiffPreviewState.DiffItems.Num() <= 0)
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorStageGWriteEnableRequest] prepare=unavailable reason=missing_upstream_preview_state"));
		UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGWriteEnableRequest] suggestion=先按 F15/G1 链路生成 Review/Apply/Confirm/Ticket/Receipt/Final/Archive/Handoff/StageGIntent 预览"));
		return false;
	}

	FHCIAbilityKitAgentStageGExecuteIntent WorkingIntent = HCI_StageG_State().AgentStageGExecuteIntentPreviewState;
	if (TamperMode == TEXT("digest")) { WorkingIntent.SelectionDigest += TEXT("_tampered"); }
	else if (TamperMode == TEXT("intent")) { WorkingIntent.RequestId += TEXT("_stale"); }
	else if (TamperMode == TEXT("handoff")) { WorkingIntent.SimHandoffEnvelopeId += TEXT("_stale"); }
	else if (TamperMode == TEXT("ready")) { WorkingIntent.bReadyForStageGEntry = false; WorkingIntent.StageGStatus = TEXT("blocked"); }

	if (!FHCIAbilityKitAgentExecutorStageGWriteEnableRequestBridge::BuildStageGWriteEnableRequest(
		WorkingIntent,
		HCI_StageG_State().AgentStageGExecuteIntentPreviewState.RequestId,
		HCI_StageG_State().AgentSimulateExecuteHandoffEnvelopePreviewState,
		HCI_StageG_State().AgentSimulateExecuteArchiveBundlePreviewState,
		HCI_StageG_State().AgentSimulateExecuteFinalReportPreviewState,
		HCI_StageG_State().AgentSimulateExecuteReceiptPreviewState,
		HCI_StageG_State().AgentExecuteTicketPreviewState,
		HCI_StageG_State().AgentApplyConfirmRequestPreviewState,
		HCI_StageG_State().AgentApplyRequestPreviewState,
		HCI_StageG_State().AgentExecutorReviewDiffPreviewState,
		bWriteEnableConfirmed,
		OutRequest))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorStageGWriteEnableRequest] prepare_failed reason=bridge_failed"));
		return false;
	}

	HCI_StageG_State().AgentStageGWriteEnableRequestPreviewState = OutRequest;
	return true;
}

static bool HCI_TryParseStageGWriteEnableRequestCommandArgs(const TArray<FString>& Args, bool& OutWriteEnableConfirmed, FString& OutTamperMode)
{
	OutWriteEnableConfirmed = true;
	OutTamperMode = TEXT("none");
	if (Args.Num() == 0)
	{
		return true;
	}
	if (Args.Num() >= 1)
	{
		if (!HCI_TryParseBool01Arg(Args[0], OutWriteEnableConfirmed))
		{
			return false;
		}
	}
	if (Args.Num() >= 2)
	{
		if (!HCI_TryParseStageGWriteEnableTamperModeArg(Args[1], OutTamperMode))
		{
			return false;
		}
	}
	return Args.Num() <= 2;
}

void HCI_RunAbilityKitAgentExecutePlanReviewPrepareStageGWriteEnableRequestCommand(const TArray<FString>& Args)
{
	bool bWriteEnableConfirmed = true;
	FString TamperMode;
	if (!HCI_TryParseStageGWriteEnableRequestCommandArgs(Args, bWriteEnableConfirmed, TamperMode))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorStageGWriteEnableRequest] invalid_args usage=HCIAbilityKit.AgentExecutePlanReviewPrepareStageGWriteEnableRequest [write_enable_confirmed=0|1] [tamper=none|digest|intent|handoff|ready]"));
		return;
	}

	FHCIAbilityKitAgentStageGWriteEnableRequest Request;
	if (!HCI_TryBuildAgentExecutorStageGWriteEnableRequestFromLatestPreview(bWriteEnableConfirmed, TamperMode, Request))
	{
		return;
	}

	HCI_LogAgentExecutorStageGWriteEnableRequestSummary(Request);
	HCI_LogAgentExecutorStageGWriteEnableRequestRows(Request);
	UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGWriteEnableRequest] hint=也可运行 HCIAbilityKit.AgentExecutePlanReviewPrepareStageGWriteEnableRequestJson [write_enable_confirmed] [tamper] 输出 StageGWriteEnableRequest JSON"));
}

void HCI_RunAbilityKitAgentExecutePlanReviewPrepareStageGWriteEnableRequestJsonCommand(const TArray<FString>& Args)
{
	bool bWriteEnableConfirmed = true;
	FString TamperMode;
	if (!HCI_TryParseStageGWriteEnableRequestCommandArgs(Args, bWriteEnableConfirmed, TamperMode))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorStageGWriteEnableRequest] invalid_args usage=HCIAbilityKit.AgentExecutePlanReviewPrepareStageGWriteEnableRequestJson [write_enable_confirmed=0|1] [tamper=none|digest|intent|handoff|ready]"));
		return;
	}

	FHCIAbilityKitAgentStageGWriteEnableRequest Request;
	if (!HCI_TryBuildAgentExecutorStageGWriteEnableRequestFromLatestPreview(bWriteEnableConfirmed, TamperMode, Request))
	{
		return;
	}

	HCI_LogAgentExecutorStageGWriteEnableRequestSummary(Request);
	HCI_LogAgentExecutorStageGWriteEnableRequestRows(Request);

	FString JsonText;
	if (!FHCIAbilityKitAgentStageGWriteEnableRequestJsonSerializer::SerializeToJsonString(Request, JsonText))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorStageGWriteEnableRequest] json_failed request_id=%s"), Request.RequestId.IsEmpty() ? TEXT("-") : *Request.RequestId);
		return;
	}

	UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGWriteEnableRequest] json request_id=%s bytes=%d"), Request.RequestId.IsEmpty() ? TEXT("-") : *Request.RequestId, JsonText.Len());
	UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("%s"), *JsonText);
}

static bool HCI_TryParseStageGExecutePermitTamperModeArg(const FString& InValue, FString& OutTamperMode)
{
	const FString V = InValue.TrimStartAndEnd();
	if (V == TEXT("none") || V == TEXT("digest") || V == TEXT("intent") || V == TEXT("handoff") || V == TEXT("permit") || V == TEXT("ready"))
	{
		OutTamperMode = V;
		return true;
	}
	return false;
}

static void HCI_LogAgentExecutorStageGExecutePermitTicketSummary(const FHCIAbilityKitAgentStageGExecutePermitTicket& Request)
{
	UE_LOG(
		LogHCIAbilityKitAgentDemo,
		Display,
		TEXT("[HCIAbilityKit][AgentExecutorStageGExecutePermitTicket] summary request_id=%s stage_g_write_enable_request_id=%s stage_g_execute_intent_id=%s sim_handoff_envelope_id=%s sim_archive_bundle_id=%s sim_final_report_id=%s sim_execute_receipt_id=%s execute_ticket_id=%s confirm_request_id=%s apply_request_id=%s review_request_id=%s selection_digest=%s archive_digest=%s handoff_digest=%s execute_intent_digest=%s stage_g_write_enable_digest=%s stage_g_execute_permit_digest=%s execute_target=%s handoff_target=%s user_confirmed=%s write_enable_confirmed=%s ready_to_simulate_execute=%s simulated_dispatch_accepted=%s simulation_completed=%s terminal_status=%s archive_status=%s handoff_status=%s stage_g_status=%s stage_g_write_status=%s stage_g_execute_permit_status=%s archive_ready=%s handoff_ready=%s write_enabled=%s ready_for_stage_g_entry=%s ready_for_stage_g_execute=%s stage_g_execute_permit_ready=%s error_code=%s reason=%s validation=ok"),
		Request.RequestId.IsEmpty() ? TEXT("-") : *Request.RequestId,
		Request.StageGWriteEnableRequestId.IsEmpty() ? TEXT("-") : *Request.StageGWriteEnableRequestId,
		Request.StageGExecuteIntentId.IsEmpty() ? TEXT("-") : *Request.StageGExecuteIntentId,
		Request.SimHandoffEnvelopeId.IsEmpty() ? TEXT("-") : *Request.SimHandoffEnvelopeId,
		Request.SimArchiveBundleId.IsEmpty() ? TEXT("-") : *Request.SimArchiveBundleId,
		Request.SimFinalReportId.IsEmpty() ? TEXT("-") : *Request.SimFinalReportId,
		Request.SimExecuteReceiptId.IsEmpty() ? TEXT("-") : *Request.SimExecuteReceiptId,
		Request.ExecuteTicketId.IsEmpty() ? TEXT("-") : *Request.ExecuteTicketId,
		Request.ConfirmRequestId.IsEmpty() ? TEXT("-") : *Request.ConfirmRequestId,
		Request.ApplyRequestId.IsEmpty() ? TEXT("-") : *Request.ApplyRequestId,
		Request.ReviewRequestId.IsEmpty() ? TEXT("-") : *Request.ReviewRequestId,
		Request.SelectionDigest.IsEmpty() ? TEXT("-") : *Request.SelectionDigest,
		Request.ArchiveDigest.IsEmpty() ? TEXT("-") : *Request.ArchiveDigest,
		Request.HandoffDigest.IsEmpty() ? TEXT("-") : *Request.HandoffDigest,
		Request.ExecuteIntentDigest.IsEmpty() ? TEXT("-") : *Request.ExecuteIntentDigest,
		Request.StageGWriteEnableDigest.IsEmpty() ? TEXT("-") : *Request.StageGWriteEnableDigest,
		Request.StageGExecutePermitDigest.IsEmpty() ? TEXT("-") : *Request.StageGExecutePermitDigest,
		Request.ExecuteTarget.IsEmpty() ? TEXT("-") : *Request.ExecuteTarget,
		Request.HandoffTarget.IsEmpty() ? TEXT("-") : *Request.HandoffTarget,
		Request.bUserConfirmed ? TEXT("true") : TEXT("false"),
		Request.bWriteEnableConfirmed ? TEXT("true") : TEXT("false"),
		Request.bReadyToSimulateExecute ? TEXT("true") : TEXT("false"),
		Request.bSimulatedDispatchAccepted ? TEXT("true") : TEXT("false"),
		Request.bSimulationCompleted ? TEXT("true") : TEXT("false"),
		Request.TerminalStatus.IsEmpty() ? TEXT("-") : *Request.TerminalStatus,
		Request.ArchiveStatus.IsEmpty() ? TEXT("-") : *Request.ArchiveStatus,
		Request.HandoffStatus.IsEmpty() ? TEXT("-") : *Request.HandoffStatus,
		Request.StageGStatus.IsEmpty() ? TEXT("-") : *Request.StageGStatus,
		Request.StageGWriteStatus.IsEmpty() ? TEXT("-") : *Request.StageGWriteStatus,
		Request.StageGExecutePermitStatus.IsEmpty() ? TEXT("-") : *Request.StageGExecutePermitStatus,
		Request.bArchiveReady ? TEXT("true") : TEXT("false"),
		Request.bHandoffReady ? TEXT("true") : TEXT("false"),
		Request.bWriteEnabled ? TEXT("true") : TEXT("false"),
		Request.bReadyForStageGEntry ? TEXT("true") : TEXT("false"),
		Request.bReadyForStageGExecute ? TEXT("true") : TEXT("false"),
		Request.bStageGExecutePermitReady ? TEXT("true") : TEXT("false"),
		Request.ErrorCode.IsEmpty() ? TEXT("-") : *Request.ErrorCode,
		Request.Reason.IsEmpty() ? TEXT("-") : *Request.Reason);
}

static void HCI_LogAgentExecutorStageGExecutePermitTicketRows(const FHCIAbilityKitAgentStageGExecutePermitTicket& Request)
{
	for (int32 Index = 0; Index < Request.Items.Num(); ++Index)
	{
		const FHCIAbilityKitAgentApplyRequestItem& Item = Request.Items[Index];
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentExecutorStageGExecutePermitTicket] row=%d tool_name=%s risk=%s blocked=%s skip_reason=%s object_type=%s locate_strategy=%s asset_path=%s field=%s evidence_key=%s actor_path=%s"),
			Index,
			Item.ToolName.IsEmpty() ? TEXT("-") : *Item.ToolName,
			*FHCIAbilityKitDryRunDiff::RiskToString(Item.Risk),
			Item.bBlocked ? TEXT("true") : TEXT("false"),
			Item.SkipReason.IsEmpty() ? TEXT("-") : *Item.SkipReason,
			*FHCIAbilityKitDryRunDiff::ObjectTypeToString(Item.ObjectType),
			*FHCIAbilityKitDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
			Item.AssetPath.IsEmpty() ? TEXT("-") : *Item.AssetPath,
			Item.Field.IsEmpty() ? TEXT("-") : *Item.Field,
			Item.EvidenceKey.IsEmpty() ? TEXT("-") : *Item.EvidenceKey,
			Item.ActorPath.IsEmpty() ? TEXT("-") : *Item.ActorPath);
	}
}

static bool HCI_TryBuildAgentExecutorStageGExecutePermitTicketFromLatestPreview(
	const bool bWriteEnableConfirmed,
	const FString& TamperMode,
	FHCIAbilityKitAgentStageGExecutePermitTicket& OutRequest)
{
	if (HCI_StageG_State().AgentStageGWriteEnableRequestPreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorStageGExecutePermitTicket] prepare=unavailable reason=no_stage_g_write_enable_request_preview_state"));
		UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecutePermitTicket] suggestion=先运行 HCIAbilityKit.AgentExecutePlanReviewPrepareStageGWriteEnableRequest"));
		return false;
	}
	if (HCI_StageG_State().AgentStageGExecuteIntentPreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorStageGExecutePermitTicket] prepare=unavailable reason=no_stage_g_execute_intent_preview_state"));
		UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecutePermitTicket] suggestion=先运行 HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteIntent"));
		return false;
	}
	if (HCI_StageG_State().AgentSimulateExecuteHandoffEnvelopePreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorStageGExecutePermitTicket] prepare=unavailable reason=no_sim_handoff_envelope_preview_state"));
		UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecutePermitTicket] suggestion=先运行 HCIAbilityKit.AgentExecutePlanReviewPrepareSimHandoffEnvelope"));
		return false;
	}
	if (HCI_StageG_State().AgentSimulateExecuteArchiveBundlePreviewState.Items.Num() <= 0 ||
		HCI_StageG_State().AgentSimulateExecuteFinalReportPreviewState.Items.Num() <= 0 ||
		HCI_StageG_State().AgentSimulateExecuteReceiptPreviewState.Items.Num() <= 0 ||
		HCI_StageG_State().AgentExecuteTicketPreviewState.Items.Num() <= 0 ||
		HCI_StageG_State().AgentApplyConfirmRequestPreviewState.Items.Num() <= 0 ||
		HCI_StageG_State().AgentApplyRequestPreviewState.Items.Num() <= 0 ||
		HCI_StageG_State().AgentExecutorReviewDiffPreviewState.DiffItems.Num() <= 0)
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorStageGExecutePermitTicket] prepare=unavailable reason=missing_upstream_preview_state"));
		UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecutePermitTicket] suggestion=先按 F15/G1/G2 链路生成 Review/Apply/Confirm/Ticket/Receipt/Final/Archive/Handoff/StageGIntent/StageGWriteEnableRequest 预览"));
		return false;
	}

	FHCIAbilityKitAgentStageGWriteEnableRequest WorkingRequest = HCI_StageG_State().AgentStageGWriteEnableRequestPreviewState;
	WorkingRequest.bWriteEnableConfirmed = bWriteEnableConfirmed;
	if (TamperMode == TEXT("digest")) { WorkingRequest.SelectionDigest += TEXT("_tampered"); }
	else if (TamperMode == TEXT("intent")) { WorkingRequest.StageGExecuteIntentId += TEXT("_stale"); }
	else if (TamperMode == TEXT("handoff")) { WorkingRequest.SimHandoffEnvelopeId += TEXT("_stale"); }
	else if (TamperMode == TEXT("write")) { WorkingRequest.bWriteEnabled = false; WorkingRequest.StageGWriteStatus = TEXT("blocked"); }
	else if (TamperMode == TEXT("ready")) { WorkingRequest.bReadyForStageGExecute = false; WorkingRequest.StageGWriteStatus = TEXT("blocked"); }

	if (!FHCIAbilityKitAgentExecutorStageGExecutePermitTicketBridge::BuildStageGExecutePermitTicket(
		WorkingRequest,
		HCI_StageG_State().AgentStageGWriteEnableRequestPreviewState.RequestId,
		HCI_StageG_State().AgentStageGExecuteIntentPreviewState,
		HCI_StageG_State().AgentSimulateExecuteHandoffEnvelopePreviewState,
		HCI_StageG_State().AgentSimulateExecuteArchiveBundlePreviewState,
		HCI_StageG_State().AgentSimulateExecuteFinalReportPreviewState,
		HCI_StageG_State().AgentSimulateExecuteReceiptPreviewState,
		HCI_StageG_State().AgentExecuteTicketPreviewState,
		HCI_StageG_State().AgentApplyConfirmRequestPreviewState,
		HCI_StageG_State().AgentApplyRequestPreviewState,
		HCI_StageG_State().AgentExecutorReviewDiffPreviewState,
		OutRequest))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorStageGExecutePermitTicket] prepare_failed reason=bridge_failed"));
		return false;
	}

	HCI_StageG_State().AgentStageGExecutePermitTicketPreviewState = OutRequest;
	return true;
}

static bool HCI_TryParseStageGExecutePermitTicketCommandArgs(const TArray<FString>& Args, bool& OutWriteEnableConfirmed, FString& OutTamperMode)
{
	OutWriteEnableConfirmed = true;
	OutTamperMode = TEXT("none");
	if (Args.Num() == 0)
	{
		return true;
	}
	if (Args.Num() >= 1)
	{
		if (!HCI_TryParseBool01Arg(Args[0], OutWriteEnableConfirmed))
		{
			return false;
		}
	}
	if (Args.Num() >= 2)
	{
		if (!HCI_TryParseStageGExecutePermitTamperModeArg(Args[1], OutTamperMode))
		{
			return false;
		}
	}
	return Args.Num() <= 2;
}

void HCI_RunAbilityKitAgentExecutePlanReviewPrepareStageGExecutePermitTicketCommand(const TArray<FString>& Args)
{
	bool bWriteEnableConfirmed = true;
	FString TamperMode;
	if (!HCI_TryParseStageGExecutePermitTicketCommandArgs(Args, bWriteEnableConfirmed, TamperMode))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorStageGExecutePermitTicket] invalid_args usage=HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecutePermitTicket [write_enable_confirmed=0|1] [tamper=none|digest|intent|handoff|write|ready]"));
		return;
	}

	FHCIAbilityKitAgentStageGExecutePermitTicket Request;
	if (!HCI_TryBuildAgentExecutorStageGExecutePermitTicketFromLatestPreview(bWriteEnableConfirmed, TamperMode, Request))
	{
		return;
	}

	HCI_LogAgentExecutorStageGExecutePermitTicketSummary(Request);
	HCI_LogAgentExecutorStageGExecutePermitTicketRows(Request);
	UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecutePermitTicket] hint=也可运行 HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecutePermitTicketJson [write_enable_confirmed] [tamper] 输出 StageGExecutePermitTicket JSON"));
}

void HCI_RunAbilityKitAgentExecutePlanReviewPrepareStageGExecutePermitTicketJsonCommand(const TArray<FString>& Args)
{
	bool bWriteEnableConfirmed = true;
	FString TamperMode;
	if (!HCI_TryParseStageGExecutePermitTicketCommandArgs(Args, bWriteEnableConfirmed, TamperMode))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorStageGExecutePermitTicket] invalid_args usage=HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecutePermitTicketJson [write_enable_confirmed=0|1] [tamper=none|digest|intent|handoff|write|ready]"));
		return;
	}

	FHCIAbilityKitAgentStageGExecutePermitTicket Request;
	if (!HCI_TryBuildAgentExecutorStageGExecutePermitTicketFromLatestPreview(bWriteEnableConfirmed, TamperMode, Request))
	{
		return;
	}

	HCI_LogAgentExecutorStageGExecutePermitTicketSummary(Request);
	HCI_LogAgentExecutorStageGExecutePermitTicketRows(Request);

	FString JsonText;
	if (!FHCIAbilityKitAgentStageGExecutePermitTicketJsonSerializer::SerializeToJsonString(Request, JsonText))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorStageGExecutePermitTicket] json_failed request_id=%s"), Request.RequestId.IsEmpty() ? TEXT("-") : *Request.RequestId);
		return;
	}

	UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecutePermitTicket] json request_id=%s bytes=%d"), Request.RequestId.IsEmpty() ? TEXT("-") : *Request.RequestId, JsonText.Len());
	UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("%s"), *JsonText);
}


static bool HCI_TryParseStageGExecuteDispatchTamperModeArg(const FString& InValue, FString& OutTamperMode)
{
	const FString V = InValue.TrimStartAndEnd();
	if (V == TEXT("none") || V == TEXT("digest") || V == TEXT("intent") || V == TEXT("handoff") || V == TEXT("write") || V == TEXT("ready"))
	{
		OutTamperMode = V;
		return true;
	}
	return false;
}

static void HCI_LogAgentExecutorStageGExecuteDispatchRequestSummary(const FHCIAbilityKitAgentStageGExecuteDispatchRequest& Request)
{
	UE_LOG(
		LogHCIAbilityKitAgentDemo,
		Display,
		TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteDispatchRequest] summary request_id=%s stage_g_execute_permit_ticket_id=%s stage_g_write_enable_request_id=%s stage_g_execute_intent_id=%s sim_handoff_envelope_id=%s sim_archive_bundle_id=%s sim_final_report_id=%s sim_execute_receipt_id=%s execute_ticket_id=%s confirm_request_id=%s apply_request_id=%s review_request_id=%s selection_digest=%s archive_digest=%s handoff_digest=%s execute_intent_digest=%s stage_g_write_enable_digest=%s stage_g_execute_permit_digest=%s stage_g_execute_dispatch_digest=%s execute_target=%s handoff_target=%s user_confirmed=%s execute_dispatch_confirmed=%s ready_to_simulate_execute=%s simulated_dispatch_accepted=%s simulation_completed=%s terminal_status=%s archive_status=%s handoff_status=%s stage_g_status=%s stage_g_write_status=%s stage_g_execute_permit_status=%s stage_g_execute_dispatch_status=%s archive_ready=%s handoff_ready=%s write_enabled=%s ready_for_stage_g_entry=%s ready_for_stage_g_execute=%s stage_g_execute_permit_ready=%s stage_g_execute_dispatch_ready=%s error_code=%s reason=%s validation=ok"),
		Request.RequestId.IsEmpty() ? TEXT("-") : *Request.RequestId,
		Request.StageGExecutePermitTicketId.IsEmpty() ? TEXT("-") : *Request.StageGExecutePermitTicketId,
		Request.StageGWriteEnableRequestId.IsEmpty() ? TEXT("-") : *Request.StageGWriteEnableRequestId,
		Request.StageGExecuteIntentId.IsEmpty() ? TEXT("-") : *Request.StageGExecuteIntentId,
		Request.SimHandoffEnvelopeId.IsEmpty() ? TEXT("-") : *Request.SimHandoffEnvelopeId,
		Request.SimArchiveBundleId.IsEmpty() ? TEXT("-") : *Request.SimArchiveBundleId,
		Request.SimFinalReportId.IsEmpty() ? TEXT("-") : *Request.SimFinalReportId,
		Request.SimExecuteReceiptId.IsEmpty() ? TEXT("-") : *Request.SimExecuteReceiptId,
		Request.ExecuteTicketId.IsEmpty() ? TEXT("-") : *Request.ExecuteTicketId,
		Request.ConfirmRequestId.IsEmpty() ? TEXT("-") : *Request.ConfirmRequestId,
		Request.ApplyRequestId.IsEmpty() ? TEXT("-") : *Request.ApplyRequestId,
		Request.ReviewRequestId.IsEmpty() ? TEXT("-") : *Request.ReviewRequestId,
		Request.SelectionDigest.IsEmpty() ? TEXT("-") : *Request.SelectionDigest,
		Request.ArchiveDigest.IsEmpty() ? TEXT("-") : *Request.ArchiveDigest,
		Request.HandoffDigest.IsEmpty() ? TEXT("-") : *Request.HandoffDigest,
		Request.ExecuteIntentDigest.IsEmpty() ? TEXT("-") : *Request.ExecuteIntentDigest,
		Request.StageGWriteEnableDigest.IsEmpty() ? TEXT("-") : *Request.StageGWriteEnableDigest,
		Request.StageGExecutePermitDigest.IsEmpty() ? TEXT("-") : *Request.StageGExecutePermitDigest,
		Request.StageGExecuteDispatchDigest.IsEmpty() ? TEXT("-") : *Request.StageGExecuteDispatchDigest,
		Request.ExecuteTarget.IsEmpty() ? TEXT("-") : *Request.ExecuteTarget,
		Request.HandoffTarget.IsEmpty() ? TEXT("-") : *Request.HandoffTarget,
		Request.bUserConfirmed ? TEXT("true") : TEXT("false"),
		Request.bExecuteDispatchConfirmed ? TEXT("true") : TEXT("false"),
		Request.bReadyToSimulateExecute ? TEXT("true") : TEXT("false"),
		Request.bSimulatedDispatchAccepted ? TEXT("true") : TEXT("false"),
		Request.bSimulationCompleted ? TEXT("true") : TEXT("false"),
		Request.TerminalStatus.IsEmpty() ? TEXT("-") : *Request.TerminalStatus,
		Request.ArchiveStatus.IsEmpty() ? TEXT("-") : *Request.ArchiveStatus,
		Request.HandoffStatus.IsEmpty() ? TEXT("-") : *Request.HandoffStatus,
		Request.StageGStatus.IsEmpty() ? TEXT("-") : *Request.StageGStatus,
		Request.StageGWriteStatus.IsEmpty() ? TEXT("-") : *Request.StageGWriteStatus,
		Request.StageGExecutePermitStatus.IsEmpty() ? TEXT("-") : *Request.StageGExecutePermitStatus,
		Request.StageGExecuteDispatchStatus.IsEmpty() ? TEXT("-") : *Request.StageGExecuteDispatchStatus,
		Request.bArchiveReady ? TEXT("true") : TEXT("false"),
		Request.bHandoffReady ? TEXT("true") : TEXT("false"),
		Request.bWriteEnabled ? TEXT("true") : TEXT("false"),
		Request.bReadyForStageGEntry ? TEXT("true") : TEXT("false"),
		Request.bReadyForStageGExecute ? TEXT("true") : TEXT("false"),
		Request.bStageGExecutePermitReady ? TEXT("true") : TEXT("false"),
		Request.bStageGExecuteDispatchReady ? TEXT("true") : TEXT("false"),
		Request.ErrorCode.IsEmpty() ? TEXT("-") : *Request.ErrorCode,
		Request.Reason.IsEmpty() ? TEXT("-") : *Request.Reason);
}

static void HCI_LogAgentExecutorStageGExecuteDispatchRequestRows(const FHCIAbilityKitAgentStageGExecuteDispatchRequest& Request)
{
	for (int32 Index = 0; Index < Request.Items.Num(); ++Index)
	{
		const FHCIAbilityKitAgentApplyRequestItem& Item = Request.Items[Index];
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteDispatchRequest] row=%d tool_name=%s risk=%s blocked=%s skip_reason=%s object_type=%s locate_strategy=%s asset_path=%s field=%s evidence_key=%s actor_path=%s"),
			Index,
			Item.ToolName.IsEmpty() ? TEXT("-") : *Item.ToolName,
			*FHCIAbilityKitDryRunDiff::RiskToString(Item.Risk),
			Item.bBlocked ? TEXT("true") : TEXT("false"),
			Item.SkipReason.IsEmpty() ? TEXT("-") : *Item.SkipReason,
			*FHCIAbilityKitDryRunDiff::ObjectTypeToString(Item.ObjectType),
			*FHCIAbilityKitDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
			Item.AssetPath.IsEmpty() ? TEXT("-") : *Item.AssetPath,
			Item.Field.IsEmpty() ? TEXT("-") : *Item.Field,
			Item.EvidenceKey.IsEmpty() ? TEXT("-") : *Item.EvidenceKey,
			Item.ActorPath.IsEmpty() ? TEXT("-") : *Item.ActorPath);
	}
}

static bool HCI_TryBuildAgentExecutorStageGExecuteDispatchRequestFromLatestPreview(
	const bool bExecuteDispatchConfirmed,
	const FString& TamperMode,
	FHCIAbilityKitAgentStageGExecuteDispatchRequest& OutRequest)
{
	if (HCI_StageG_State().AgentStageGExecutePermitTicketPreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteDispatchRequest] prepare=unavailable reason=no_stage_g_execute_permit_ticket_preview_state"));
		UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteDispatchRequest] suggestion=先运行 HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecutePermitTicket"));
		return false;
	}
	if (HCI_StageG_State().AgentStageGWriteEnableRequestPreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteDispatchRequest] prepare=unavailable reason=no_stage_g_write_enable_request_preview_state"));
		UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteDispatchRequest] suggestion=先运行 HCIAbilityKit.AgentExecutePlanReviewPrepareStageGWriteEnableRequest"));
		return false;
	}
	if (HCI_StageG_State().AgentStageGExecuteIntentPreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteDispatchRequest] prepare=unavailable reason=no_stage_g_execute_intent_preview_state"));
		UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteDispatchRequest] suggestion=先运行 HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteIntent"));
		return false;
	}
	if (HCI_StageG_State().AgentSimulateExecuteHandoffEnvelopePreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteDispatchRequest] prepare=unavailable reason=no_sim_handoff_envelope_preview_state"));
		UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteDispatchRequest] suggestion=先运行 HCIAbilityKit.AgentExecutePlanReviewPrepareSimHandoffEnvelope"));
		return false;
	}
	if (HCI_StageG_State().AgentSimulateExecuteArchiveBundlePreviewState.Items.Num() <= 0 ||
		HCI_StageG_State().AgentSimulateExecuteFinalReportPreviewState.Items.Num() <= 0 ||
		HCI_StageG_State().AgentSimulateExecuteReceiptPreviewState.Items.Num() <= 0 ||
		HCI_StageG_State().AgentExecuteTicketPreviewState.Items.Num() <= 0 ||
		HCI_StageG_State().AgentApplyConfirmRequestPreviewState.Items.Num() <= 0 ||
		HCI_StageG_State().AgentApplyRequestPreviewState.Items.Num() <= 0 ||
		HCI_StageG_State().AgentExecutorReviewDiffPreviewState.DiffItems.Num() <= 0)
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteDispatchRequest] prepare=unavailable reason=missing_upstream_preview_state"));
		UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteDispatchRequest] suggestion=先按 F15/G1/G2/G3 链路生成 ... -> StageGExecutePermitTicket 预览"));
		return false;
	}

	FHCIAbilityKitAgentStageGExecutePermitTicket WorkingTicket = HCI_StageG_State().AgentStageGExecutePermitTicketPreviewState;
	if (TamperMode == TEXT("digest")) { WorkingTicket.SelectionDigest += TEXT("_tampered"); }
	else if (TamperMode == TEXT("intent")) { WorkingTicket.StageGExecuteIntentId += TEXT("_stale"); }
	else if (TamperMode == TEXT("handoff")) { WorkingTicket.SimHandoffEnvelopeId += TEXT("_stale"); }
	else if (TamperMode == TEXT("permit")) { WorkingTicket.bStageGExecutePermitReady = false; WorkingTicket.StageGExecutePermitStatus = TEXT("blocked"); }
	else if (TamperMode == TEXT("ready")) { WorkingTicket.bReadyForStageGExecute = false; WorkingTicket.StageGExecutePermitStatus = TEXT("blocked"); }

	if (!FHCIAbilityKitAgentExecutorStageGExecuteDispatchRequestBridge::BuildStageGExecuteDispatchRequest(
		WorkingTicket,
		HCI_StageG_State().AgentStageGExecutePermitTicketPreviewState.RequestId,
		HCI_StageG_State().AgentStageGWriteEnableRequestPreviewState,
		HCI_StageG_State().AgentStageGExecuteIntentPreviewState,
		HCI_StageG_State().AgentSimulateExecuteHandoffEnvelopePreviewState,
		HCI_StageG_State().AgentSimulateExecuteArchiveBundlePreviewState,
		HCI_StageG_State().AgentSimulateExecuteFinalReportPreviewState,
		HCI_StageG_State().AgentSimulateExecuteReceiptPreviewState,
		HCI_StageG_State().AgentExecuteTicketPreviewState,
		HCI_StageG_State().AgentApplyConfirmRequestPreviewState,
		HCI_StageG_State().AgentApplyRequestPreviewState,
		HCI_StageG_State().AgentExecutorReviewDiffPreviewState,
		bExecuteDispatchConfirmed,
		OutRequest))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteDispatchRequest] prepare_failed reason=bridge_failed"));
		return false;
	}

	HCI_StageG_State().AgentStageGExecuteDispatchRequestPreviewState = OutRequest;
	return true;
}

static bool HCI_TryParseStageGExecuteDispatchRequestCommandArgs(const TArray<FString>& Args, bool& OutExecuteDispatchConfirmed, FString& OutTamperMode)
{
	OutExecuteDispatchConfirmed = true;
	OutTamperMode = TEXT("none");
	if (Args.Num() == 0)
	{
		return true;
	}
	if (Args.Num() >= 1)
	{
		if (!HCI_TryParseBool01Arg(Args[0], OutExecuteDispatchConfirmed))
		{
			return false;
		}
	}
	if (Args.Num() >= 2)
	{
		if (!HCI_TryParseStageGExecuteDispatchTamperModeArg(Args[1], OutTamperMode))
		{
			return false;
		}
	}
	return Args.Num() <= 2;
}

void HCI_RunAbilityKitAgentExecutePlanReviewPrepareStageGExecuteDispatchRequestCommand(const TArray<FString>& Args)
{
	bool bExecuteDispatchConfirmed = true;
	FString TamperMode;
	if (!HCI_TryParseStageGExecuteDispatchRequestCommandArgs(Args, bExecuteDispatchConfirmed, TamperMode))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteDispatchRequest] invalid_args usage=HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteDispatchRequest [execute_dispatch_confirmed=0|1] [tamper=none|digest|intent|handoff|permit|ready]"));
		return;
	}

	FHCIAbilityKitAgentStageGExecuteDispatchRequest Request;
	if (!HCI_TryBuildAgentExecutorStageGExecuteDispatchRequestFromLatestPreview(bExecuteDispatchConfirmed, TamperMode, Request))
	{
		return;
	}

	HCI_LogAgentExecutorStageGExecuteDispatchRequestSummary(Request);
	HCI_LogAgentExecutorStageGExecuteDispatchRequestRows(Request);
	UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteDispatchRequest] hint=也可运行 HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteDispatchRequestJson [execute_dispatch_confirmed] [tamper] 输出 StageGExecuteDispatchRequest JSON"));
}

void HCI_RunAbilityKitAgentExecutePlanReviewPrepareStageGExecuteDispatchRequestJsonCommand(const TArray<FString>& Args)
{
	bool bExecuteDispatchConfirmed = true;
	FString TamperMode;
	if (!HCI_TryParseStageGExecuteDispatchRequestCommandArgs(Args, bExecuteDispatchConfirmed, TamperMode))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteDispatchRequest] invalid_args usage=HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteDispatchRequestJson [execute_dispatch_confirmed=0|1] [tamper=none|digest|intent|handoff|permit|ready]"));
		return;
	}

	FHCIAbilityKitAgentStageGExecuteDispatchRequest Request;
	if (!HCI_TryBuildAgentExecutorStageGExecuteDispatchRequestFromLatestPreview(bExecuteDispatchConfirmed, TamperMode, Request))
	{
		return;
	}

	HCI_LogAgentExecutorStageGExecuteDispatchRequestSummary(Request);
	HCI_LogAgentExecutorStageGExecuteDispatchRequestRows(Request);

	FString JsonText;
	if (!FHCIAbilityKitAgentStageGExecuteDispatchRequestJsonSerializer::SerializeToJsonString(Request, JsonText))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteDispatchRequest] json_failed request_id=%s"), Request.RequestId.IsEmpty() ? TEXT("-") : *Request.RequestId);
		return;
	}

	UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteDispatchRequest] json request_id=%s bytes=%d"), Request.RequestId.IsEmpty() ? TEXT("-") : *Request.RequestId, JsonText.Len());
	UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("%s"), *JsonText);
}



static bool HCI_TryParseStageGExecuteDispatchReceiptTamperModeArg(const FString& InValue, FString& OutTamperMode)
{
	const FString V = InValue.TrimStartAndEnd();
	if (V == TEXT("none") || V == TEXT("digest") || V == TEXT("intent") || V == TEXT("handoff") || V == TEXT("dispatch") || V == TEXT("ready"))
	{
		OutTamperMode = V;
		return true;
	}
	return false;
}

static void HCI_LogAgentExecutorStageGExecuteDispatchReceiptSummary(const FHCIAbilityKitAgentStageGExecuteDispatchReceipt& Request)
{
	UE_LOG(
		LogHCIAbilityKitAgentDemo,
		Display,
		TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteDispatchReceipt] summary request_id=%s stage_g_execute_dispatch_request_id=%s stage_g_execute_permit_ticket_id=%s stage_g_write_enable_request_id=%s stage_g_execute_intent_id=%s sim_handoff_envelope_id=%s sim_archive_bundle_id=%s sim_final_report_id=%s sim_execute_receipt_id=%s execute_ticket_id=%s confirm_request_id=%s apply_request_id=%s review_request_id=%s selection_digest=%s archive_digest=%s handoff_digest=%s execute_intent_digest=%s stage_g_write_enable_digest=%s stage_g_execute_permit_digest=%s stage_g_execute_dispatch_digest=%s stage_g_execute_dispatch_receipt_digest=%s execute_target=%s handoff_target=%s user_confirmed=%s execute_dispatch_confirmed=%s ready_to_simulate_execute=%s simulated_dispatch_accepted=%s simulation_completed=%s terminal_status=%s archive_status=%s handoff_status=%s stage_g_status=%s stage_g_write_status=%s stage_g_execute_permit_status=%s stage_g_execute_dispatch_status=%s stage_g_execute_dispatch_receipt_status=%s archive_ready=%s handoff_ready=%s write_enabled=%s ready_for_stage_g_entry=%s ready_for_stage_g_execute=%s stage_g_execute_permit_ready=%s stage_g_execute_dispatch_ready=%s stage_g_execute_dispatch_accepted=%s stage_g_execute_dispatch_receipt_ready=%s error_code=%s reason=%s validation=ok"),
		Request.RequestId.IsEmpty() ? TEXT("-") : *Request.RequestId,
		Request.StageGExecuteDispatchRequestId.IsEmpty() ? TEXT("-") : *Request.StageGExecuteDispatchRequestId,
		Request.StageGExecutePermitTicketId.IsEmpty() ? TEXT("-") : *Request.StageGExecutePermitTicketId,
		Request.StageGWriteEnableRequestId.IsEmpty() ? TEXT("-") : *Request.StageGWriteEnableRequestId,
		Request.StageGExecuteIntentId.IsEmpty() ? TEXT("-") : *Request.StageGExecuteIntentId,
		Request.SimHandoffEnvelopeId.IsEmpty() ? TEXT("-") : *Request.SimHandoffEnvelopeId,
		Request.SimArchiveBundleId.IsEmpty() ? TEXT("-") : *Request.SimArchiveBundleId,
		Request.SimFinalReportId.IsEmpty() ? TEXT("-") : *Request.SimFinalReportId,
		Request.SimExecuteReceiptId.IsEmpty() ? TEXT("-") : *Request.SimExecuteReceiptId,
		Request.ExecuteTicketId.IsEmpty() ? TEXT("-") : *Request.ExecuteTicketId,
		Request.ConfirmRequestId.IsEmpty() ? TEXT("-") : *Request.ConfirmRequestId,
		Request.ApplyRequestId.IsEmpty() ? TEXT("-") : *Request.ApplyRequestId,
		Request.ReviewRequestId.IsEmpty() ? TEXT("-") : *Request.ReviewRequestId,
		Request.SelectionDigest.IsEmpty() ? TEXT("-") : *Request.SelectionDigest,
		Request.ArchiveDigest.IsEmpty() ? TEXT("-") : *Request.ArchiveDigest,
		Request.HandoffDigest.IsEmpty() ? TEXT("-") : *Request.HandoffDigest,
		Request.ExecuteIntentDigest.IsEmpty() ? TEXT("-") : *Request.ExecuteIntentDigest,
		Request.StageGWriteEnableDigest.IsEmpty() ? TEXT("-") : *Request.StageGWriteEnableDigest,
		Request.StageGExecutePermitDigest.IsEmpty() ? TEXT("-") : *Request.StageGExecutePermitDigest,
		Request.StageGExecuteDispatchDigest.IsEmpty() ? TEXT("-") : *Request.StageGExecuteDispatchDigest,
		Request.StageGExecuteDispatchReceiptDigest.IsEmpty() ? TEXT("-") : *Request.StageGExecuteDispatchReceiptDigest,
		Request.ExecuteTarget.IsEmpty() ? TEXT("-") : *Request.ExecuteTarget,
		Request.HandoffTarget.IsEmpty() ? TEXT("-") : *Request.HandoffTarget,
		Request.bUserConfirmed ? TEXT("true") : TEXT("false"),
		Request.bExecuteDispatchConfirmed ? TEXT("true") : TEXT("false"),
		Request.bReadyToSimulateExecute ? TEXT("true") : TEXT("false"),
		Request.bSimulatedDispatchAccepted ? TEXT("true") : TEXT("false"),
		Request.bSimulationCompleted ? TEXT("true") : TEXT("false"),
		Request.TerminalStatus.IsEmpty() ? TEXT("-") : *Request.TerminalStatus,
		Request.ArchiveStatus.IsEmpty() ? TEXT("-") : *Request.ArchiveStatus,
		Request.HandoffStatus.IsEmpty() ? TEXT("-") : *Request.HandoffStatus,
		Request.StageGStatus.IsEmpty() ? TEXT("-") : *Request.StageGStatus,
		Request.StageGWriteStatus.IsEmpty() ? TEXT("-") : *Request.StageGWriteStatus,
		Request.StageGExecutePermitStatus.IsEmpty() ? TEXT("-") : *Request.StageGExecutePermitStatus,
		Request.StageGExecuteDispatchStatus.IsEmpty() ? TEXT("-") : *Request.StageGExecuteDispatchStatus,
		Request.StageGExecuteDispatchReceiptStatus.IsEmpty() ? TEXT("-") : *Request.StageGExecuteDispatchReceiptStatus,
		Request.bArchiveReady ? TEXT("true") : TEXT("false"),
		Request.bHandoffReady ? TEXT("true") : TEXT("false"),
		Request.bWriteEnabled ? TEXT("true") : TEXT("false"),
		Request.bReadyForStageGEntry ? TEXT("true") : TEXT("false"),
		Request.bReadyForStageGExecute ? TEXT("true") : TEXT("false"),
		Request.bStageGExecutePermitReady ? TEXT("true") : TEXT("false"),
		Request.bStageGExecuteDispatchReady ? TEXT("true") : TEXT("false"),
		Request.bStageGExecuteDispatchAccepted ? TEXT("true") : TEXT("false"),
		Request.bStageGExecuteDispatchReceiptReady ? TEXT("true") : TEXT("false"),
		Request.ErrorCode.IsEmpty() ? TEXT("-") : *Request.ErrorCode,
		Request.Reason.IsEmpty() ? TEXT("-") : *Request.Reason);
}

static void HCI_LogAgentExecutorStageGExecuteDispatchReceiptRows(const FHCIAbilityKitAgentStageGExecuteDispatchReceipt& Request)
{
	for (int32 Index = 0; Index < Request.Items.Num(); ++Index)
	{
		const FHCIAbilityKitAgentApplyRequestItem& Item = Request.Items[Index];
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteDispatchReceipt] row=%d tool_name=%s risk=%s blocked=%s skip_reason=%s object_type=%s locate_strategy=%s asset_path=%s field=%s evidence_key=%s actor_path=%s"),
			Index,
			Item.ToolName.IsEmpty() ? TEXT("-") : *Item.ToolName,
			*FHCIAbilityKitDryRunDiff::RiskToString(Item.Risk),
			Item.bBlocked ? TEXT("true") : TEXT("false"),
			Item.SkipReason.IsEmpty() ? TEXT("-") : *Item.SkipReason,
			*FHCIAbilityKitDryRunDiff::ObjectTypeToString(Item.ObjectType),
			*FHCIAbilityKitDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
			Item.AssetPath.IsEmpty() ? TEXT("-") : *Item.AssetPath,
			Item.Field.IsEmpty() ? TEXT("-") : *Item.Field,
			Item.EvidenceKey.IsEmpty() ? TEXT("-") : *Item.EvidenceKey,
			Item.ActorPath.IsEmpty() ? TEXT("-") : *Item.ActorPath);
	}
}

static bool HCI_TryBuildAgentExecutorStageGExecuteDispatchReceiptFromLatestPreview(
	const FString& TamperMode,
	FHCIAbilityKitAgentStageGExecuteDispatchReceipt& OutRequest)
{
	if (HCI_StageG_State().AgentStageGExecuteDispatchRequestPreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteDispatchReceipt] prepare=unavailable reason=no_stage_g_execute_dispatch_request_preview_state"));
		UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteDispatchReceipt] suggestion=先运行 HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteDispatchRequest"));
		return false;
	}
	if (HCI_StageG_State().AgentStageGExecutePermitTicketPreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteDispatchReceipt] prepare=unavailable reason=no_stage_g_execute_permit_ticket_preview_state"));
		UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteDispatchReceipt] suggestion=先运行 HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecutePermitTicket"));
		return false;
	}
	if (HCI_StageG_State().AgentStageGWriteEnableRequestPreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteDispatchReceipt] prepare=unavailable reason=no_stage_g_write_enable_request_preview_state"));
		UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteDispatchReceipt] suggestion=先运行 HCIAbilityKit.AgentExecutePlanReviewPrepareStageGWriteEnableRequest"));
		return false;
	}
	if (HCI_StageG_State().AgentStageGExecuteIntentPreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteDispatchReceipt] prepare=unavailable reason=no_stage_g_execute_intent_preview_state"));
		UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteDispatchReceipt] suggestion=先运行 HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteIntent"));
		return false;
	}
	if (HCI_StageG_State().AgentSimulateExecuteHandoffEnvelopePreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteDispatchReceipt] prepare=unavailable reason=no_sim_handoff_envelope_preview_state"));
		UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteDispatchReceipt] suggestion=先运行 HCIAbilityKit.AgentExecutePlanReviewPrepareSimHandoffEnvelope"));
		return false;
	}
	if (HCI_StageG_State().AgentSimulateExecuteArchiveBundlePreviewState.Items.Num() <= 0 ||
		HCI_StageG_State().AgentSimulateExecuteFinalReportPreviewState.Items.Num() <= 0 ||
		HCI_StageG_State().AgentSimulateExecuteReceiptPreviewState.Items.Num() <= 0 ||
		HCI_StageG_State().AgentExecuteTicketPreviewState.Items.Num() <= 0 ||
		HCI_StageG_State().AgentApplyConfirmRequestPreviewState.Items.Num() <= 0 ||
		HCI_StageG_State().AgentApplyRequestPreviewState.Items.Num() <= 0 ||
		HCI_StageG_State().AgentExecutorReviewDiffPreviewState.DiffItems.Num() <= 0)
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteDispatchReceipt] prepare=unavailable reason=missing_upstream_preview_state"));
		UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteDispatchReceipt] suggestion=先按 F15/G1/G2/G3/G4 链路生成 ... -> StageGExecuteDispatchRequest 预览"));
		return false;
	}

	FHCIAbilityKitAgentStageGExecuteDispatchRequest WorkingRequest = HCI_StageG_State().AgentStageGExecuteDispatchRequestPreviewState;
	if (TamperMode == TEXT("digest")) { WorkingRequest.SelectionDigest += TEXT("_tampered"); }
	else if (TamperMode == TEXT("intent")) { WorkingRequest.StageGExecuteIntentId += TEXT("_stale"); }
	else if (TamperMode == TEXT("handoff")) { WorkingRequest.SimHandoffEnvelopeId += TEXT("_stale"); }
	else if (TamperMode == TEXT("dispatch")) { WorkingRequest.RequestId += TEXT("_stale"); }
	else if (TamperMode == TEXT("ready")) { WorkingRequest.bStageGExecuteDispatchReady = false; WorkingRequest.StageGExecuteDispatchStatus = TEXT("blocked"); }

	if (!FHCIAbilityKitAgentExecutorStageGExecuteDispatchReceiptBridge::BuildStageGExecuteDispatchReceipt(
		WorkingRequest,
		HCI_StageG_State().AgentStageGExecuteDispatchRequestPreviewState.RequestId,
		HCI_StageG_State().AgentStageGExecutePermitTicketPreviewState,
		HCI_StageG_State().AgentStageGWriteEnableRequestPreviewState,
		HCI_StageG_State().AgentStageGExecuteIntentPreviewState,
		HCI_StageG_State().AgentSimulateExecuteHandoffEnvelopePreviewState,
		HCI_StageG_State().AgentSimulateExecuteArchiveBundlePreviewState,
		HCI_StageG_State().AgentSimulateExecuteFinalReportPreviewState,
		HCI_StageG_State().AgentSimulateExecuteReceiptPreviewState,
		HCI_StageG_State().AgentExecuteTicketPreviewState,
		HCI_StageG_State().AgentApplyConfirmRequestPreviewState,
		HCI_StageG_State().AgentApplyRequestPreviewState,
		HCI_StageG_State().AgentExecutorReviewDiffPreviewState,
		OutRequest))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteDispatchReceipt] prepare_failed reason=bridge_failed"));
		return false;
	}

	HCI_StageG_State().AgentStageGExecuteDispatchReceiptPreviewState = OutRequest;
	return true;
}

static bool HCI_TryParseStageGExecuteDispatchReceiptCommandArgs(const TArray<FString>& Args, FString& OutTamperMode)
{
	OutTamperMode = TEXT("none");
	if (Args.Num() == 0)
	{
		return true;
	}
	if (Args.Num() >= 1)
	{
		if (!HCI_TryParseStageGExecuteDispatchReceiptTamperModeArg(Args[0], OutTamperMode))
		{
			return false;
		}
	}
	return Args.Num() <= 1;
}

void HCI_RunAbilityKitAgentExecutePlanReviewPrepareStageGExecuteDispatchReceiptCommand(const TArray<FString>& Args)
{
	FString TamperMode;
	if (!HCI_TryParseStageGExecuteDispatchReceiptCommandArgs(Args, TamperMode))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteDispatchReceipt] invalid_args usage=HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteDispatchReceipt [tamper=none|digest|intent|handoff|dispatch|ready]"));
		return;
	}

	FHCIAbilityKitAgentStageGExecuteDispatchReceipt Request;
	if (!HCI_TryBuildAgentExecutorStageGExecuteDispatchReceiptFromLatestPreview(TamperMode, Request))
	{
		return;
	}

	HCI_LogAgentExecutorStageGExecuteDispatchReceiptSummary(Request);
	HCI_LogAgentExecutorStageGExecuteDispatchReceiptRows(Request);
	UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteDispatchReceipt] hint=也可运行 HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteDispatchReceiptJson [tamper] 输出 StageGExecuteDispatchReceipt JSON"));
}

void HCI_RunAbilityKitAgentExecutePlanReviewPrepareStageGExecuteDispatchReceiptJsonCommand(const TArray<FString>& Args)
{
	FString TamperMode;
	if (!HCI_TryParseStageGExecuteDispatchReceiptCommandArgs(Args, TamperMode))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteDispatchReceipt] invalid_args usage=HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteDispatchReceiptJson [tamper=none|digest|intent|handoff|dispatch|ready]"));
		return;
	}

	FHCIAbilityKitAgentStageGExecuteDispatchReceipt Request;
	if (!HCI_TryBuildAgentExecutorStageGExecuteDispatchReceiptFromLatestPreview(TamperMode, Request))
	{
		return;
	}

	HCI_LogAgentExecutorStageGExecuteDispatchReceiptSummary(Request);
	HCI_LogAgentExecutorStageGExecuteDispatchReceiptRows(Request);

	FString JsonText;
	if (!FHCIAbilityKitAgentStageGExecuteDispatchReceiptJsonSerializer::SerializeToJsonString(Request, JsonText))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteDispatchReceipt] json_failed request_id=%s"), Request.RequestId.IsEmpty() ? TEXT("-") : *Request.RequestId);
		return;
	}

	UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteDispatchReceipt] json request_id=%s bytes=%d"), Request.RequestId.IsEmpty() ? TEXT("-") : *Request.RequestId, JsonText.Len());
	UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("%s"), *JsonText);
}

static bool HCI_TryParseStageGExecuteCommitRequestTamperModeArg(const FString& InValue, FString& OutTamperMode)
{
	const FString V = InValue.TrimStartAndEnd();
	if (V == TEXT("none") || V == TEXT("digest") || V == TEXT("intent") || V == TEXT("handoff") ||
		V == TEXT("dispatch") || V == TEXT("receipt") || V == TEXT("ready"))
	{
		OutTamperMode = V;
		return true;
	}
	return false;
}

static void HCI_LogAgentExecutorStageGExecuteCommitRequestSummary(const FHCIAbilityKitAgentStageGExecuteCommitRequest& Request)
{
	UE_LOG(
		LogHCIAbilityKitAgentDemo,
		Display,
		TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteCommitRequest] summary request_id=%s stage_g_execute_dispatch_receipt_id=%s stage_g_execute_dispatch_request_id=%s stage_g_execute_permit_ticket_id=%s stage_g_write_enable_request_id=%s stage_g_execute_intent_id=%s sim_handoff_envelope_id=%s sim_archive_bundle_id=%s sim_final_report_id=%s sim_execute_receipt_id=%s execute_ticket_id=%s confirm_request_id=%s apply_request_id=%s review_request_id=%s selection_digest=%s archive_digest=%s handoff_digest=%s execute_intent_digest=%s stage_g_write_enable_digest=%s stage_g_execute_permit_digest=%s stage_g_execute_dispatch_digest=%s stage_g_execute_dispatch_receipt_digest=%s stage_g_execute_commit_request_digest=%s execute_target=%s handoff_target=%s user_confirmed=%s write_enable_confirmed=%s execute_dispatch_confirmed=%s execute_commit_confirmed=%s ready_to_simulate_execute=%s simulated_dispatch_accepted=%s simulation_completed=%s terminal_status=%s archive_status=%s handoff_status=%s stage_g_status=%s stage_g_write_status=%s stage_g_execute_permit_status=%s stage_g_execute_dispatch_status=%s stage_g_execute_dispatch_receipt_status=%s stage_g_execute_commit_request_status=%s archive_ready=%s handoff_ready=%s write_enabled=%s ready_for_stage_g_entry=%s ready_for_stage_g_execute=%s stage_g_execute_permit_ready=%s stage_g_execute_dispatch_ready=%s stage_g_execute_dispatch_accepted=%s stage_g_execute_dispatch_receipt_ready=%s stage_g_execute_commit_request_ready=%s error_code=%s reason=%s validation=ok"),
		Request.RequestId.IsEmpty() ? TEXT("-") : *Request.RequestId,
		Request.StageGExecuteDispatchReceiptId.IsEmpty() ? TEXT("-") : *Request.StageGExecuteDispatchReceiptId,
		Request.StageGExecuteDispatchRequestId.IsEmpty() ? TEXT("-") : *Request.StageGExecuteDispatchRequestId,
		Request.StageGExecutePermitTicketId.IsEmpty() ? TEXT("-") : *Request.StageGExecutePermitTicketId,
		Request.StageGWriteEnableRequestId.IsEmpty() ? TEXT("-") : *Request.StageGWriteEnableRequestId,
		Request.StageGExecuteIntentId.IsEmpty() ? TEXT("-") : *Request.StageGExecuteIntentId,
		Request.SimHandoffEnvelopeId.IsEmpty() ? TEXT("-") : *Request.SimHandoffEnvelopeId,
		Request.SimArchiveBundleId.IsEmpty() ? TEXT("-") : *Request.SimArchiveBundleId,
		Request.SimFinalReportId.IsEmpty() ? TEXT("-") : *Request.SimFinalReportId,
		Request.SimExecuteReceiptId.IsEmpty() ? TEXT("-") : *Request.SimExecuteReceiptId,
		Request.ExecuteTicketId.IsEmpty() ? TEXT("-") : *Request.ExecuteTicketId,
		Request.ConfirmRequestId.IsEmpty() ? TEXT("-") : *Request.ConfirmRequestId,
		Request.ApplyRequestId.IsEmpty() ? TEXT("-") : *Request.ApplyRequestId,
		Request.ReviewRequestId.IsEmpty() ? TEXT("-") : *Request.ReviewRequestId,
		Request.SelectionDigest.IsEmpty() ? TEXT("-") : *Request.SelectionDigest,
		Request.ArchiveDigest.IsEmpty() ? TEXT("-") : *Request.ArchiveDigest,
		Request.HandoffDigest.IsEmpty() ? TEXT("-") : *Request.HandoffDigest,
		Request.ExecuteIntentDigest.IsEmpty() ? TEXT("-") : *Request.ExecuteIntentDigest,
		Request.StageGWriteEnableDigest.IsEmpty() ? TEXT("-") : *Request.StageGWriteEnableDigest,
		Request.StageGExecutePermitDigest.IsEmpty() ? TEXT("-") : *Request.StageGExecutePermitDigest,
		Request.StageGExecuteDispatchDigest.IsEmpty() ? TEXT("-") : *Request.StageGExecuteDispatchDigest,
		Request.StageGExecuteDispatchReceiptDigest.IsEmpty() ? TEXT("-") : *Request.StageGExecuteDispatchReceiptDigest,
		Request.StageGExecuteCommitRequestDigest.IsEmpty() ? TEXT("-") : *Request.StageGExecuteCommitRequestDigest,
		Request.ExecuteTarget.IsEmpty() ? TEXT("-") : *Request.ExecuteTarget,
		Request.HandoffTarget.IsEmpty() ? TEXT("-") : *Request.HandoffTarget,
		Request.bUserConfirmed ? TEXT("true") : TEXT("false"),
		Request.bWriteEnableConfirmed ? TEXT("true") : TEXT("false"),
		Request.bExecuteDispatchConfirmed ? TEXT("true") : TEXT("false"),
		Request.bExecuteCommitConfirmed ? TEXT("true") : TEXT("false"),
		Request.bReadyToSimulateExecute ? TEXT("true") : TEXT("false"),
		Request.bSimulatedDispatchAccepted ? TEXT("true") : TEXT("false"),
		Request.bSimulationCompleted ? TEXT("true") : TEXT("false"),
		Request.TerminalStatus.IsEmpty() ? TEXT("-") : *Request.TerminalStatus,
		Request.ArchiveStatus.IsEmpty() ? TEXT("-") : *Request.ArchiveStatus,
		Request.HandoffStatus.IsEmpty() ? TEXT("-") : *Request.HandoffStatus,
		Request.StageGStatus.IsEmpty() ? TEXT("-") : *Request.StageGStatus,
		Request.StageGWriteStatus.IsEmpty() ? TEXT("-") : *Request.StageGWriteStatus,
		Request.StageGExecutePermitStatus.IsEmpty() ? TEXT("-") : *Request.StageGExecutePermitStatus,
		Request.StageGExecuteDispatchStatus.IsEmpty() ? TEXT("-") : *Request.StageGExecuteDispatchStatus,
		Request.StageGExecuteDispatchReceiptStatus.IsEmpty() ? TEXT("-") : *Request.StageGExecuteDispatchReceiptStatus,
		Request.StageGExecuteCommitRequestStatus.IsEmpty() ? TEXT("-") : *Request.StageGExecuteCommitRequestStatus,
		Request.bArchiveReady ? TEXT("true") : TEXT("false"),
		Request.bHandoffReady ? TEXT("true") : TEXT("false"),
		Request.bWriteEnabled ? TEXT("true") : TEXT("false"),
		Request.bReadyForStageGEntry ? TEXT("true") : TEXT("false"),
		Request.bReadyForStageGExecute ? TEXT("true") : TEXT("false"),
		Request.bStageGExecutePermitReady ? TEXT("true") : TEXT("false"),
		Request.bStageGExecuteDispatchReady ? TEXT("true") : TEXT("false"),
		Request.bStageGExecuteDispatchAccepted ? TEXT("true") : TEXT("false"),
		Request.bStageGExecuteDispatchReceiptReady ? TEXT("true") : TEXT("false"),
		Request.bStageGExecuteCommitRequestReady ? TEXT("true") : TEXT("false"),
		Request.ErrorCode.IsEmpty() ? TEXT("-") : *Request.ErrorCode,
		Request.Reason.IsEmpty() ? TEXT("-") : *Request.Reason);
}

static void HCI_LogAgentExecutorStageGExecuteCommitRequestRows(const FHCIAbilityKitAgentStageGExecuteCommitRequest& Request)
{
	for (int32 Index = 0; Index < Request.Items.Num(); ++Index)
	{
		const FHCIAbilityKitAgentApplyRequestItem& Item = Request.Items[Index];
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteCommitRequest] row=%d tool_name=%s risk=%s blocked=%s skip_reason=%s object_type=%s locate_strategy=%s asset_path=%s field=%s evidence_key=%s actor_path=%s"),
			Index,
			Item.ToolName.IsEmpty() ? TEXT("-") : *Item.ToolName,
			*FHCIAbilityKitDryRunDiff::RiskToString(Item.Risk),
			Item.bBlocked ? TEXT("true") : TEXT("false"),
			Item.SkipReason.IsEmpty() ? TEXT("-") : *Item.SkipReason,
			*FHCIAbilityKitDryRunDiff::ObjectTypeToString(Item.ObjectType),
			*FHCIAbilityKitDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
			Item.AssetPath.IsEmpty() ? TEXT("-") : *Item.AssetPath,
			Item.Field.IsEmpty() ? TEXT("-") : *Item.Field,
			Item.EvidenceKey.IsEmpty() ? TEXT("-") : *Item.EvidenceKey,
			Item.ActorPath.IsEmpty() ? TEXT("-") : *Item.ActorPath);
	}
}

static bool HCI_TryBuildAgentExecutorStageGExecuteCommitRequestFromLatestPreview(
	const bool bExecuteCommitConfirmed,
	const FString& TamperMode,
	FHCIAbilityKitAgentStageGExecuteCommitRequest& OutRequest)
{
	if (HCI_StageG_State().AgentStageGExecuteDispatchReceiptPreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteCommitRequest] prepare=unavailable reason=no_stage_g_execute_dispatch_receipt_preview_state"));
		UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteCommitRequest] suggestion=先运行 HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteDispatchReceipt"));
		return false;
	}
	if (HCI_StageG_State().AgentStageGExecuteDispatchRequestPreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteCommitRequest] prepare=unavailable reason=no_stage_g_execute_dispatch_request_preview_state"));
		UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteCommitRequest] suggestion=先运行 HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteDispatchRequest"));
		return false;
	}
	if (HCI_StageG_State().AgentStageGExecutePermitTicketPreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteCommitRequest] prepare=unavailable reason=no_stage_g_execute_permit_ticket_preview_state"));
		UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteCommitRequest] suggestion=先运行 HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecutePermitTicket"));
		return false;
	}
	if (HCI_StageG_State().AgentStageGWriteEnableRequestPreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteCommitRequest] prepare=unavailable reason=no_stage_g_write_enable_request_preview_state"));
		UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteCommitRequest] suggestion=先运行 HCIAbilityKit.AgentExecutePlanReviewPrepareStageGWriteEnableRequest"));
		return false;
	}
	if (HCI_StageG_State().AgentStageGExecuteIntentPreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteCommitRequest] prepare=unavailable reason=no_stage_g_execute_intent_preview_state"));
		UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteCommitRequest] suggestion=先运行 HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteIntent"));
		return false;
	}
	if (HCI_StageG_State().AgentSimulateExecuteHandoffEnvelopePreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteCommitRequest] prepare=unavailable reason=no_sim_handoff_envelope_preview_state"));
		UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteCommitRequest] suggestion=先运行 HCIAbilityKit.AgentExecutePlanReviewPrepareSimHandoffEnvelope"));
		return false;
	}
	if (HCI_StageG_State().AgentSimulateExecuteArchiveBundlePreviewState.Items.Num() <= 0 ||
		HCI_StageG_State().AgentSimulateExecuteFinalReportPreviewState.Items.Num() <= 0 ||
		HCI_StageG_State().AgentSimulateExecuteReceiptPreviewState.Items.Num() <= 0 ||
		HCI_StageG_State().AgentExecuteTicketPreviewState.Items.Num() <= 0 ||
		HCI_StageG_State().AgentApplyConfirmRequestPreviewState.Items.Num() <= 0 ||
		HCI_StageG_State().AgentApplyRequestPreviewState.Items.Num() <= 0 ||
		HCI_StageG_State().AgentExecutorReviewDiffPreviewState.DiffItems.Num() <= 0)
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteCommitRequest] prepare=unavailable reason=missing_upstream_preview_state"));
		UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteCommitRequest] suggestion=先按 F15/G1/G2/G3/G4/G5 链路生成 ... -> StageGExecuteDispatchReceipt 预览"));
		return false;
	}

	FHCIAbilityKitAgentStageGExecuteDispatchReceipt WorkingReceipt = HCI_StageG_State().AgentStageGExecuteDispatchReceiptPreviewState;
	if (TamperMode == TEXT("digest")) { WorkingReceipt.SelectionDigest += TEXT("_tampered"); }
	else if (TamperMode == TEXT("intent")) { WorkingReceipt.StageGExecuteIntentId += TEXT("_stale"); }
	else if (TamperMode == TEXT("handoff")) { WorkingReceipt.SimHandoffEnvelopeId += TEXT("_stale"); }
	else if (TamperMode == TEXT("dispatch")) { WorkingReceipt.StageGExecuteDispatchRequestId += TEXT("_stale"); }
	else if (TamperMode == TEXT("receipt")) { WorkingReceipt.bStageGExecuteDispatchAccepted = false; WorkingReceipt.bStageGExecuteDispatchReceiptReady = false; WorkingReceipt.StageGExecuteDispatchReceiptStatus = TEXT("blocked"); }
	else if (TamperMode == TEXT("ready")) { WorkingReceipt.bReadyForStageGExecute = false; WorkingReceipt.StageGExecuteDispatchReceiptStatus = TEXT("blocked"); }

	if (!FHCIAbilityKitAgentExecutorStageGExecuteCommitRequestBridge::BuildStageGExecuteCommitRequest(
		WorkingReceipt,
		HCI_StageG_State().AgentStageGExecuteDispatchReceiptPreviewState.RequestId,
		HCI_StageG_State().AgentStageGExecuteDispatchRequestPreviewState,
		HCI_StageG_State().AgentStageGExecutePermitTicketPreviewState,
		HCI_StageG_State().AgentStageGWriteEnableRequestPreviewState,
		HCI_StageG_State().AgentStageGExecuteIntentPreviewState,
		HCI_StageG_State().AgentSimulateExecuteHandoffEnvelopePreviewState,
		HCI_StageG_State().AgentSimulateExecuteArchiveBundlePreviewState,
		HCI_StageG_State().AgentSimulateExecuteFinalReportPreviewState,
		HCI_StageG_State().AgentSimulateExecuteReceiptPreviewState,
		HCI_StageG_State().AgentExecuteTicketPreviewState,
		HCI_StageG_State().AgentApplyConfirmRequestPreviewState,
		HCI_StageG_State().AgentApplyRequestPreviewState,
		HCI_StageG_State().AgentExecutorReviewDiffPreviewState,
		bExecuteCommitConfirmed,
		OutRequest))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteCommitRequest] prepare_failed reason=bridge_failed"));
		return false;
	}

	HCI_StageG_State().AgentStageGExecuteCommitRequestPreviewState = OutRequest;
	return true;
}

static bool HCI_TryParseStageGExecuteCommitRequestCommandArgs(const TArray<FString>& Args, bool& OutExecuteCommitConfirmed, FString& OutTamperMode)
{
	OutExecuteCommitConfirmed = true;
	OutTamperMode = TEXT("none");
	if (Args.Num() == 0)
	{
		return true;
	}
	if (Args.Num() >= 1)
	{
		if (!HCI_TryParseBool01Arg(Args[0], OutExecuteCommitConfirmed))
		{
			return false;
		}
	}
	if (Args.Num() >= 2)
	{
		if (!HCI_TryParseStageGExecuteCommitRequestTamperModeArg(Args[1], OutTamperMode))
		{
			return false;
		}
	}
	return Args.Num() <= 2;
}

void HCI_RunAbilityKitAgentExecutePlanReviewPrepareStageGExecuteCommitRequestCommand(const TArray<FString>& Args)
{
	bool bExecuteCommitConfirmed = true;
	FString TamperMode;
	if (!HCI_TryParseStageGExecuteCommitRequestCommandArgs(Args, bExecuteCommitConfirmed, TamperMode))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteCommitRequest] invalid_args usage=HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteCommitRequest [execute_commit_confirmed=0|1] [tamper=none|digest|intent|handoff|dispatch|receipt|ready]"));
		return;
	}

	FHCIAbilityKitAgentStageGExecuteCommitRequest Request;
	if (!HCI_TryBuildAgentExecutorStageGExecuteCommitRequestFromLatestPreview(bExecuteCommitConfirmed, TamperMode, Request))
	{
		return;
	}

	HCI_LogAgentExecutorStageGExecuteCommitRequestSummary(Request);
	HCI_LogAgentExecutorStageGExecuteCommitRequestRows(Request);
	UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteCommitRequest] hint=也可运行 HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteCommitRequestJson [execute_commit_confirmed] [tamper] 输出 StageGExecuteCommitRequest JSON"));
}

void HCI_RunAbilityKitAgentExecutePlanReviewPrepareStageGExecuteCommitRequestJsonCommand(const TArray<FString>& Args)
{
	bool bExecuteCommitConfirmed = true;
	FString TamperMode;
	if (!HCI_TryParseStageGExecuteCommitRequestCommandArgs(Args, bExecuteCommitConfirmed, TamperMode))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteCommitRequest] invalid_args usage=HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteCommitRequestJson [execute_commit_confirmed=0|1] [tamper=none|digest|intent|handoff|dispatch|receipt|ready]"));
		return;
	}

	FHCIAbilityKitAgentStageGExecuteCommitRequest Request;
	if (!HCI_TryBuildAgentExecutorStageGExecuteCommitRequestFromLatestPreview(bExecuteCommitConfirmed, TamperMode, Request))
	{
		return;
	}

	HCI_LogAgentExecutorStageGExecuteCommitRequestSummary(Request);
	HCI_LogAgentExecutorStageGExecuteCommitRequestRows(Request);

	FString JsonText;
	if (!FHCIAbilityKitAgentStageGExecuteCommitRequestJsonSerializer::SerializeToJsonString(Request, JsonText))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteCommitRequest] json_failed request_id=%s"), Request.RequestId.IsEmpty() ? TEXT("-") : *Request.RequestId);
		return;
	}

	UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteCommitRequest] json request_id=%s bytes=%d"), Request.RequestId.IsEmpty() ? TEXT("-") : *Request.RequestId, JsonText.Len());
	UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("%s"), *JsonText);
}

static bool HCI_TryParseStageGExecuteCommitReceiptTamperModeArg(const FString& InValue, FString& OutTamperMode)
{
	const FString Lower = InValue.TrimStartAndEnd().ToLower();
	if (Lower == TEXT("none") || Lower == TEXT("digest") || Lower == TEXT("intent") || Lower == TEXT("handoff") ||
		Lower == TEXT("dispatch") || Lower == TEXT("receipt") || Lower == TEXT("commit") || Lower == TEXT("ready"))
	{
		OutTamperMode = Lower;
		return true;
	}
	return false;
}

static void HCI_LogAgentExecutorStageGExecuteCommitReceiptSummary(const FHCIAbilityKitAgentStageGExecuteCommitReceipt& Receipt)
{
	UE_LOG(
		LogHCIAbilityKitAgentDemo,
		Display,
		TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteCommitReceipt] summary request_id=%s stage_g_execute_commit_request_id=%s stage_g_execute_dispatch_receipt_id=%s stage_g_execute_dispatch_request_id=%s stage_g_execute_permit_ticket_id=%s stage_g_write_enable_request_id=%s stage_g_execute_intent_id=%s sim_handoff_envelope_id=%s selection_digest=%s stage_g_execute_commit_request_digest=%s stage_g_execute_commit_receipt_digest=%s execute_commit_confirmed=%s write_enabled=%s ready_for_stage_g_execute=%s stage_g_execute_commit_request_ready=%s stage_g_execute_commit_accepted=%s stage_g_execute_commit_receipt_ready=%s stage_g_execute_commit_request_status=%s stage_g_execute_commit_receipt_status=%s error_code=%s reason=%s validation=ok"),
		Receipt.RequestId.IsEmpty() ? TEXT("-") : *Receipt.RequestId,
		Receipt.StageGExecuteCommitRequestId.IsEmpty() ? TEXT("-") : *Receipt.StageGExecuteCommitRequestId,
		Receipt.StageGExecuteDispatchReceiptId.IsEmpty() ? TEXT("-") : *Receipt.StageGExecuteDispatchReceiptId,
		Receipt.StageGExecuteDispatchRequestId.IsEmpty() ? TEXT("-") : *Receipt.StageGExecuteDispatchRequestId,
		Receipt.StageGExecutePermitTicketId.IsEmpty() ? TEXT("-") : *Receipt.StageGExecutePermitTicketId,
		Receipt.StageGWriteEnableRequestId.IsEmpty() ? TEXT("-") : *Receipt.StageGWriteEnableRequestId,
		Receipt.StageGExecuteIntentId.IsEmpty() ? TEXT("-") : *Receipt.StageGExecuteIntentId,
		Receipt.SimHandoffEnvelopeId.IsEmpty() ? TEXT("-") : *Receipt.SimHandoffEnvelopeId,
		Receipt.SelectionDigest.IsEmpty() ? TEXT("-") : *Receipt.SelectionDigest,
		Receipt.StageGExecuteCommitRequestDigest.IsEmpty() ? TEXT("-") : *Receipt.StageGExecuteCommitRequestDigest,
		Receipt.StageGExecuteCommitReceiptDigest.IsEmpty() ? TEXT("-") : *Receipt.StageGExecuteCommitReceiptDigest,
		Receipt.bExecuteCommitConfirmed ? TEXT("true") : TEXT("false"),
		Receipt.bWriteEnabled ? TEXT("true") : TEXT("false"),
		Receipt.bReadyForStageGExecute ? TEXT("true") : TEXT("false"),
		Receipt.bStageGExecuteCommitRequestReady ? TEXT("true") : TEXT("false"),
		Receipt.bStageGExecuteCommitAccepted ? TEXT("true") : TEXT("false"),
		Receipt.bStageGExecuteCommitReceiptReady ? TEXT("true") : TEXT("false"),
		Receipt.StageGExecuteCommitRequestStatus.IsEmpty() ? TEXT("-") : *Receipt.StageGExecuteCommitRequestStatus,
		Receipt.StageGExecuteCommitReceiptStatus.IsEmpty() ? TEXT("-") : *Receipt.StageGExecuteCommitReceiptStatus,
		Receipt.ErrorCode.IsEmpty() ? TEXT("-") : *Receipt.ErrorCode,
		Receipt.Reason.IsEmpty() ? TEXT("-") : *Receipt.Reason);
}

static void HCI_LogAgentExecutorStageGExecuteCommitReceiptRows(const FHCIAbilityKitAgentStageGExecuteCommitReceipt& Receipt)
{
	for (int32 Index = 0; Index < Receipt.Items.Num(); ++Index)
	{
		const FHCIAbilityKitAgentApplyRequestItem& Item = Receipt.Items[Index];
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteCommitReceipt] row=%d tool_name=%s risk=%s blocked=%s skip_reason=%s object_type=%s locate_strategy=%s asset_path=%s field=%s evidence_key=%s actor_path=%s"),
			Index,
			Item.ToolName.IsEmpty() ? TEXT("-") : *Item.ToolName,
			*FHCIAbilityKitDryRunDiff::RiskToString(Item.Risk),
			Item.bBlocked ? TEXT("true") : TEXT("false"),
			Item.SkipReason.IsEmpty() ? TEXT("-") : *Item.SkipReason,
			*FHCIAbilityKitDryRunDiff::ObjectTypeToString(Item.ObjectType),
			*FHCIAbilityKitDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
			Item.AssetPath.IsEmpty() ? TEXT("-") : *Item.AssetPath,
			Item.Field.IsEmpty() ? TEXT("-") : *Item.Field,
			Item.EvidenceKey.IsEmpty() ? TEXT("-") : *Item.EvidenceKey,
			Item.ActorPath.IsEmpty() ? TEXT("-") : *Item.ActorPath);
	}
}

static bool HCI_TryBuildAgentExecutorStageGExecuteCommitReceiptFromLatestPreview(
	const FString& TamperMode,
	FHCIAbilityKitAgentStageGExecuteCommitReceipt& OutReceipt)
{
	if (HCI_StageG_State().AgentStageGExecuteCommitRequestPreviewState.RequestId.IsEmpty())
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteCommitReceipt] prepare=unavailable reason=no_stage_g_execute_commit_request_preview_state"));
		UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteCommitReceipt] suggestion=先运行 HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteCommitRequest"));
		return false;
	}
	if (HCI_StageG_State().AgentStageGExecuteDispatchReceiptPreviewState.RequestId.IsEmpty())
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteCommitReceipt] prepare=unavailable reason=no_stage_g_execute_dispatch_receipt_preview_state"));
		return false;
	}

	FHCIAbilityKitAgentStageGExecuteCommitRequest CommitRequest = HCI_StageG_State().AgentStageGExecuteCommitRequestPreviewState;
	const FHCIAbilityKitAgentStageGExecuteDispatchReceipt DispatchReceipt = HCI_StageG_State().AgentStageGExecuteDispatchReceiptPreviewState;
	const FHCIAbilityKitAgentStageGExecuteDispatchRequest DispatchRequest = HCI_StageG_State().AgentStageGExecuteDispatchRequestPreviewState;
	const FHCIAbilityKitAgentStageGExecutePermitTicket PermitTicket = HCI_StageG_State().AgentStageGExecutePermitTicketPreviewState;
	const FHCIAbilityKitAgentStageGWriteEnableRequest WriteEnableRequest = HCI_StageG_State().AgentStageGWriteEnableRequestPreviewState;
	const FHCIAbilityKitAgentStageGExecuteIntent Intent = HCI_StageG_State().AgentStageGExecuteIntentPreviewState;
	const FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = HCI_StageG_State().AgentSimulateExecuteHandoffEnvelopePreviewState;
	const FHCIAbilityKitAgentSimulateExecuteArchiveBundle ArchiveBundle = HCI_StageG_State().AgentSimulateExecuteArchiveBundlePreviewState;
	const FHCIAbilityKitAgentSimulateExecuteFinalReport FinalReport = HCI_StageG_State().AgentSimulateExecuteFinalReportPreviewState;
	const FHCIAbilityKitAgentSimulateExecuteReceipt SimReceipt = HCI_StageG_State().AgentSimulateExecuteReceiptPreviewState;
	const FHCIAbilityKitAgentExecuteTicket ExecuteTicket = HCI_StageG_State().AgentExecuteTicketPreviewState;
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = HCI_StageG_State().AgentApplyConfirmRequestPreviewState;
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = HCI_StageG_State().AgentApplyRequestPreviewState;
	const FHCIAbilityKitDryRunDiffReport Review = HCI_StageG_State().AgentExecutorReviewDiffPreviewState;

	FString ExpectedCommitRequestId = CommitRequest.RequestId;
	if (TamperMode == TEXT("digest"))
	{
		CommitRequest.SelectionDigest = TEXT("crc32_BAD0C0DE");
	}
	else if (TamperMode == TEXT("intent"))
	{
		CommitRequest.StageGExecuteIntentId += TEXT("_stale");
	}
	else if (TamperMode == TEXT("handoff"))
	{
		CommitRequest.SimHandoffEnvelopeId += TEXT("_stale");
	}
	else if (TamperMode == TEXT("dispatch"))
	{
		CommitRequest.StageGExecuteDispatchRequestId += TEXT("_stale");
	}
	else if (TamperMode == TEXT("receipt"))
	{
		CommitRequest.StageGExecuteDispatchReceiptId += TEXT("_stale");
	}
	else if (TamperMode == TEXT("commit"))
	{
		ExpectedCommitRequestId += TEXT("_stale");
	}
	else if (TamperMode == TEXT("ready"))
	{
		CommitRequest.bStageGExecuteCommitRequestReady = false;
		CommitRequest.StageGExecuteCommitRequestStatus = TEXT("blocked");
	}

	if (!FHCIAbilityKitAgentExecutorStageGExecuteCommitReceiptBridge::BuildStageGExecuteCommitReceipt(
		CommitRequest,
		ExpectedCommitRequestId,
		DispatchReceipt,
		DispatchRequest,
		PermitTicket,
		WriteEnableRequest,
		Intent,
		HandoffEnvelope,
		ArchiveBundle,
		FinalReport,
		SimReceipt,
		ExecuteTicket,
		ConfirmRequest,
		ApplyRequest,
		Review,
		OutReceipt))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteCommitReceipt] prepare_failed reason=bridge_failed"));
		return false;
	}

	HCI_StageG_State().AgentStageGExecuteCommitReceiptPreviewState = OutReceipt;
	return true;
}

static bool HCI_TryParseStageGExecuteCommitReceiptCommandArgs(const TArray<FString>& Args, FString& OutTamperMode)
{
	OutTamperMode = TEXT("none");
	if (Args.Num() == 0)
	{
		return true;
	}
	if (Args.Num() != 1)
	{
		return false;
	}
	return HCI_TryParseStageGExecuteCommitReceiptTamperModeArg(Args[0], OutTamperMode);
}

void HCI_RunAbilityKitAgentExecutePlanReviewPrepareStageGExecuteCommitReceiptCommand(const TArray<FString>& Args)
{
	FString TamperMode;
	if (!HCI_TryParseStageGExecuteCommitReceiptCommandArgs(Args, TamperMode))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteCommitReceipt] invalid_args usage=HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteCommitReceipt [tamper=none|digest|intent|handoff|dispatch|receipt|commit|ready]"));
		return;
	}

	FHCIAbilityKitAgentStageGExecuteCommitReceipt Receipt;
	if (!HCI_TryBuildAgentExecutorStageGExecuteCommitReceiptFromLatestPreview(TamperMode, Receipt))
	{
		return;
	}

	HCI_LogAgentExecutorStageGExecuteCommitReceiptSummary(Receipt);
	HCI_LogAgentExecutorStageGExecuteCommitReceiptRows(Receipt);
	UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteCommitReceipt] hint=也可运行 HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteCommitReceiptJson [tamper] 输出 StageGExecuteCommitReceipt JSON"));
}

void HCI_RunAbilityKitAgentExecutePlanReviewPrepareStageGExecuteCommitReceiptJsonCommand(const TArray<FString>& Args)
{
	FString TamperMode;
	if (!HCI_TryParseStageGExecuteCommitReceiptCommandArgs(Args, TamperMode))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteCommitReceipt] invalid_args usage=HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteCommitReceiptJson [tamper=none|digest|intent|handoff|dispatch|receipt|commit|ready]"));
		return;
	}

	FHCIAbilityKitAgentStageGExecuteCommitReceipt Receipt;
	if (!HCI_TryBuildAgentExecutorStageGExecuteCommitReceiptFromLatestPreview(TamperMode, Receipt))
	{
		return;
	}

	HCI_LogAgentExecutorStageGExecuteCommitReceiptSummary(Receipt);
	HCI_LogAgentExecutorStageGExecuteCommitReceiptRows(Receipt);

	FString JsonText;
	if (!FHCIAbilityKitAgentStageGExecuteCommitReceiptJsonSerializer::SerializeToJsonString(Receipt, JsonText))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteCommitReceipt] json_failed request_id=%s"), Receipt.RequestId.IsEmpty() ? TEXT("-") : *Receipt.RequestId);
		return;
	}

	UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteCommitReceipt] json request_id=%s bytes=%d"), Receipt.RequestId.IsEmpty() ? TEXT("-") : *Receipt.RequestId, JsonText.Len());
}

static bool HCI_TryParseStageGExecuteFinalReportTamperModeArg(const FString& InValue, FString& OutTamperMode)
{
	const FString Lower = InValue.TrimStartAndEnd().ToLower();
	if (Lower == TEXT("none") || Lower == TEXT("digest") || Lower == TEXT("intent") || Lower == TEXT("handoff") ||
		Lower == TEXT("dispatch") || Lower == TEXT("receipt") || Lower == TEXT("commit") || Lower == TEXT("commitreceipt") || Lower == TEXT("ready"))
	{
		OutTamperMode = Lower;
		return true;
	}
	return false;
}

static void HCI_LogAgentExecutorStageGExecuteFinalReportSummary(const FHCIAbilityKitAgentStageGExecuteFinalReport& Receipt)
{
	UE_LOG(
		LogHCIAbilityKitAgentDemo,
		Display,
		TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteFinalReport] summary request_id=%s stage_g_execute_commit_receipt_id=%s stage_g_execute_commit_request_id=%s stage_g_execute_dispatch_receipt_id=%s stage_g_execute_dispatch_request_id=%s stage_g_execute_permit_ticket_id=%s stage_g_write_enable_request_id=%s stage_g_execute_intent_id=%s sim_handoff_envelope_id=%s selection_digest=%s stage_g_execute_commit_receipt_digest=%s stage_g_execute_final_report_digest=%s execute_commit_confirmed=%s write_enabled=%s ready_for_stage_g_execute=%s stage_g_execute_commit_receipt_ready=%s stage_g_execute_commit_accepted=%s stage_g_execute_finalized=%s stage_g_execute_final_report_ready=%s stage_g_execute_commit_receipt_status=%s stage_g_execute_final_report_status=%s error_code=%s reason=%s validation=ok"),
		Receipt.RequestId.IsEmpty() ? TEXT("-") : *Receipt.RequestId,
		Receipt.StageGExecuteCommitReceiptId.IsEmpty() ? TEXT("-") : *Receipt.StageGExecuteCommitReceiptId,
		Receipt.StageGExecuteCommitRequestId.IsEmpty() ? TEXT("-") : *Receipt.StageGExecuteCommitRequestId,
		Receipt.StageGExecuteDispatchReceiptId.IsEmpty() ? TEXT("-") : *Receipt.StageGExecuteDispatchReceiptId,
		Receipt.StageGExecuteDispatchRequestId.IsEmpty() ? TEXT("-") : *Receipt.StageGExecuteDispatchRequestId,
		Receipt.StageGExecutePermitTicketId.IsEmpty() ? TEXT("-") : *Receipt.StageGExecutePermitTicketId,
		Receipt.StageGWriteEnableRequestId.IsEmpty() ? TEXT("-") : *Receipt.StageGWriteEnableRequestId,
		Receipt.StageGExecuteIntentId.IsEmpty() ? TEXT("-") : *Receipt.StageGExecuteIntentId,
		Receipt.SimHandoffEnvelopeId.IsEmpty() ? TEXT("-") : *Receipt.SimHandoffEnvelopeId,
		Receipt.SelectionDigest.IsEmpty() ? TEXT("-") : *Receipt.SelectionDigest,
		Receipt.StageGExecuteCommitReceiptDigest.IsEmpty() ? TEXT("-") : *Receipt.StageGExecuteCommitReceiptDigest,
		Receipt.StageGExecuteFinalReportDigest.IsEmpty() ? TEXT("-") : *Receipt.StageGExecuteFinalReportDigest,
		Receipt.bExecuteCommitConfirmed ? TEXT("true") : TEXT("false"),
		Receipt.bWriteEnabled ? TEXT("true") : TEXT("false"),
		Receipt.bReadyForStageGExecute ? TEXT("true") : TEXT("false"),
		Receipt.bStageGExecuteCommitReceiptReady ? TEXT("true") : TEXT("false"),
		Receipt.bStageGExecuteCommitAccepted ? TEXT("true") : TEXT("false"),
		Receipt.bStageGExecuteFinalized ? TEXT("true") : TEXT("false"),
		Receipt.bStageGExecuteFinalReportReady ? TEXT("true") : TEXT("false"),
		Receipt.StageGExecuteCommitReceiptStatus.IsEmpty() ? TEXT("-") : *Receipt.StageGExecuteCommitReceiptStatus,
		Receipt.StageGExecuteFinalReportStatus.IsEmpty() ? TEXT("-") : *Receipt.StageGExecuteFinalReportStatus,
		Receipt.ErrorCode.IsEmpty() ? TEXT("-") : *Receipt.ErrorCode,
		Receipt.Reason.IsEmpty() ? TEXT("-") : *Receipt.Reason);
}

static void HCI_LogAgentExecutorStageGExecuteFinalReportRows(const FHCIAbilityKitAgentStageGExecuteFinalReport& Receipt)
{
	for (int32 Index = 0; Index < Receipt.Items.Num(); ++Index)
	{
		const FHCIAbilityKitAgentApplyRequestItem& Item = Receipt.Items[Index];
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteFinalReport] row=%d tool_name=%s risk=%s blocked=%s skip_reason=%s object_type=%s locate_strategy=%s asset_path=%s field=%s evidence_key=%s actor_path=%s"),
			Index,
			Item.ToolName.IsEmpty() ? TEXT("-") : *Item.ToolName,
			*FHCIAbilityKitDryRunDiff::RiskToString(Item.Risk),
			Item.bBlocked ? TEXT("true") : TEXT("false"),
			Item.SkipReason.IsEmpty() ? TEXT("-") : *Item.SkipReason,
			*FHCIAbilityKitDryRunDiff::ObjectTypeToString(Item.ObjectType),
			*FHCIAbilityKitDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
			Item.AssetPath.IsEmpty() ? TEXT("-") : *Item.AssetPath,
			Item.Field.IsEmpty() ? TEXT("-") : *Item.Field,
			Item.EvidenceKey.IsEmpty() ? TEXT("-") : *Item.EvidenceKey,
			Item.ActorPath.IsEmpty() ? TEXT("-") : *Item.ActorPath);
	}
}

static bool HCI_TryBuildAgentExecutorStageGExecuteFinalReportFromLatestPreview(
	const FString& TamperMode,
	FHCIAbilityKitAgentStageGExecuteFinalReport& OutReceipt)
{
	if (HCI_StageG_State().AgentStageGExecuteCommitReceiptPreviewState.RequestId.IsEmpty())
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteFinalReport] prepare=unavailable reason=no_stage_g_execute_commit_receipt_preview_state"));
		UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteFinalReport] suggestion=先运行 HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteCommitReceipt"));
		return false;
	}
	if (HCI_StageG_State().AgentStageGExecuteDispatchReceiptPreviewState.RequestId.IsEmpty())
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteFinalReport] prepare=unavailable reason=no_stage_g_execute_dispatch_receipt_preview_state"));
		return false;
	}

	FHCIAbilityKitAgentStageGExecuteCommitReceipt CommitReceipt = HCI_StageG_State().AgentStageGExecuteCommitReceiptPreviewState;
	const FHCIAbilityKitAgentStageGExecuteCommitRequest CommitRequest = HCI_StageG_State().AgentStageGExecuteCommitRequestPreviewState;
	const FHCIAbilityKitAgentStageGExecuteDispatchReceipt DispatchReceipt = HCI_StageG_State().AgentStageGExecuteDispatchReceiptPreviewState;
	const FHCIAbilityKitAgentStageGExecuteDispatchRequest DispatchRequest = HCI_StageG_State().AgentStageGExecuteDispatchRequestPreviewState;
	const FHCIAbilityKitAgentStageGExecutePermitTicket PermitTicket = HCI_StageG_State().AgentStageGExecutePermitTicketPreviewState;
	const FHCIAbilityKitAgentStageGWriteEnableRequest WriteEnableRequest = HCI_StageG_State().AgentStageGWriteEnableRequestPreviewState;
	const FHCIAbilityKitAgentStageGExecuteIntent Intent = HCI_StageG_State().AgentStageGExecuteIntentPreviewState;
	const FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = HCI_StageG_State().AgentSimulateExecuteHandoffEnvelopePreviewState;
	const FHCIAbilityKitAgentSimulateExecuteArchiveBundle ArchiveBundle = HCI_StageG_State().AgentSimulateExecuteArchiveBundlePreviewState;
	const FHCIAbilityKitAgentSimulateExecuteFinalReport FinalReport = HCI_StageG_State().AgentSimulateExecuteFinalReportPreviewState;
	const FHCIAbilityKitAgentSimulateExecuteReceipt SimReceipt = HCI_StageG_State().AgentSimulateExecuteReceiptPreviewState;
	const FHCIAbilityKitAgentExecuteTicket ExecuteTicket = HCI_StageG_State().AgentExecuteTicketPreviewState;
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = HCI_StageG_State().AgentApplyConfirmRequestPreviewState;
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = HCI_StageG_State().AgentApplyRequestPreviewState;
	const FHCIAbilityKitDryRunDiffReport Review = HCI_StageG_State().AgentExecutorReviewDiffPreviewState;

	FString ExpectedCommitReceiptId = CommitReceipt.RequestId;
	FString ExpectedCommitRequestId = CommitRequest.RequestId;
	FString ExpectedCommitRequestDigest = CommitRequest.StageGExecuteCommitRequestDigest;
	if (TamperMode == TEXT("digest"))
	{
		CommitReceipt.SelectionDigest = TEXT("crc32_BAD0C0DE");
	}
	else if (TamperMode == TEXT("intent"))
	{
		CommitReceipt.StageGExecuteIntentId += TEXT("_stale");
	}
	else if (TamperMode == TEXT("handoff"))
	{
		CommitReceipt.SimHandoffEnvelopeId += TEXT("_stale");
	}
	else if (TamperMode == TEXT("dispatch"))
	{
		CommitReceipt.StageGExecuteDispatchRequestId += TEXT("_stale");
	}
	else if (TamperMode == TEXT("receipt"))
	{
		CommitReceipt.StageGExecuteDispatchReceiptId += TEXT("_stale");
	}
	else if (TamperMode == TEXT("commit"))
	{
		ExpectedCommitRequestId += TEXT("_stale");
	}
	else if (TamperMode == TEXT("commitreceipt"))
	{
		ExpectedCommitReceiptId += TEXT("_stale");
	}
	else if (TamperMode == TEXT("ready"))
	{
		CommitReceipt.bStageGExecuteCommitReceiptReady = false;
		CommitReceipt.bStageGExecuteCommitAccepted = false;
		CommitReceipt.StageGExecuteCommitReceiptStatus = TEXT("blocked");
	}

	if (!FHCIAbilityKitAgentExecutorStageGExecuteFinalReportBridge::BuildStageGExecuteFinalReport(
		CommitReceipt,
		ExpectedCommitReceiptId,
		ExpectedCommitRequestId,
		ExpectedCommitRequestDigest,
		DispatchReceipt,
		DispatchRequest,
		PermitTicket,
		WriteEnableRequest,
		Intent,
		HandoffEnvelope,
		ArchiveBundle,
		FinalReport,
		SimReceipt,
		ExecuteTicket,
		ConfirmRequest,
		ApplyRequest,
		Review,
		OutReceipt))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteFinalReport] prepare_failed reason=bridge_failed"));
		return false;
	}

	HCI_StageG_State().AgentStageGExecuteFinalReportPreviewState = OutReceipt;
	return true;
}

static bool HCI_TryParseStageGExecuteFinalReportCommandArgs(const TArray<FString>& Args, FString& OutTamperMode)
{
	OutTamperMode = TEXT("none");
	if (Args.Num() == 0)
	{
		return true;
	}
	if (Args.Num() != 1)
	{
		return false;
	}
	return HCI_TryParseStageGExecuteFinalReportTamperModeArg(Args[0], OutTamperMode);
}

void HCI_RunAbilityKitAgentExecutePlanReviewPrepareStageGExecuteFinalReportCommand(const TArray<FString>& Args)
{
	FString TamperMode;
	if (!HCI_TryParseStageGExecuteFinalReportCommandArgs(Args, TamperMode))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteFinalReport] invalid_args usage=HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteFinalReport [tamper=none|digest|intent|handoff|dispatch|receipt|commit|commitreceipt|ready]"));
		return;
	}

	FHCIAbilityKitAgentStageGExecuteFinalReport Receipt;
	if (!HCI_TryBuildAgentExecutorStageGExecuteFinalReportFromLatestPreview(TamperMode, Receipt))
	{
		return;
	}

	HCI_LogAgentExecutorStageGExecuteFinalReportSummary(Receipt);
	HCI_LogAgentExecutorStageGExecuteFinalReportRows(Receipt);
	UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteFinalReport] hint=也可运行 HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteFinalReportJson [tamper] 输出 StageGExecuteFinalReport JSON"));
}

void HCI_RunAbilityKitAgentExecutePlanReviewPrepareStageGExecuteFinalReportJsonCommand(const TArray<FString>& Args)
{
	FString TamperMode;
	if (!HCI_TryParseStageGExecuteFinalReportCommandArgs(Args, TamperMode))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteFinalReport] invalid_args usage=HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteFinalReportJson [tamper=none|digest|intent|handoff|dispatch|receipt|commit|commitreceipt|ready]"));
		return;
	}

	FHCIAbilityKitAgentStageGExecuteFinalReport Receipt;
	if (!HCI_TryBuildAgentExecutorStageGExecuteFinalReportFromLatestPreview(TamperMode, Receipt))
	{
		return;
	}

	HCI_LogAgentExecutorStageGExecuteFinalReportSummary(Receipt);
	HCI_LogAgentExecutorStageGExecuteFinalReportRows(Receipt);

	FString JsonText;
	if (!FHCIAbilityKitAgentStageGExecuteFinalReportJsonSerializer::SerializeToJsonString(Receipt, JsonText))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteFinalReport] json_failed request_id=%s"), Receipt.RequestId.IsEmpty() ? TEXT("-") : *Receipt.RequestId);
		return;
	}

	UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteFinalReport] json request_id=%s bytes=%d"), Receipt.RequestId.IsEmpty() ? TEXT("-") : *Receipt.RequestId, JsonText.Len());
	UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("%s"), *JsonText);
}





static bool HCI_TryParseStageGExecuteArchiveBundleTamperModeArg(const FString& InValue, FString& OutTamperMode)
{
	const FString Lower = InValue.TrimStartAndEnd().ToLower();
	if (Lower == TEXT("none") || Lower == TEXT("digest") || Lower == TEXT("intent") || Lower == TEXT("handoff") ||
		Lower == TEXT("dispatch") || Lower == TEXT("receipt") || Lower == TEXT("commit") ||
		Lower == TEXT("commitreceipt") || Lower == TEXT("final") || Lower == TEXT("ready"))
	{
		OutTamperMode = Lower;
		return true;
	}
	return false;
}

static void HCI_LogAgentExecutorStageGExecuteArchiveBundleSummary(const FHCIAbilityKitAgentStageGExecuteArchiveBundle& Receipt)
{
	UE_LOG(
		LogHCIAbilityKitAgentDemo,
		Display,
		TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteArchiveBundle] summary request_id=%s stage_g_execute_final_report_id=%s stage_g_execute_commit_receipt_id=%s stage_g_execute_commit_request_id=%s stage_g_execute_dispatch_receipt_id=%s stage_g_execute_dispatch_request_id=%s stage_g_execute_permit_ticket_id=%s stage_g_write_enable_request_id=%s stage_g_execute_intent_id=%s sim_handoff_envelope_id=%s selection_digest=%s stage_g_execute_final_report_digest=%s stage_g_execute_archive_bundle_digest=%s execute_commit_confirmed=%s write_enabled=%s ready_for_stage_g_execute=%s stage_g_execute_final_report_ready=%s stage_g_execute_finalized=%s stage_g_execute_archive_bundle_ready=%s stage_g_execute_final_report_status=%s stage_g_execute_archive_bundle_status=%s error_code=%s reason=%s validation=ok"),
		Receipt.RequestId.IsEmpty() ? TEXT("-") : *Receipt.RequestId,
		Receipt.StageGExecuteFinalReportId.IsEmpty() ? TEXT("-") : *Receipt.StageGExecuteFinalReportId,
		Receipt.StageGExecuteCommitReceiptId.IsEmpty() ? TEXT("-") : *Receipt.StageGExecuteCommitReceiptId,
		Receipt.StageGExecuteCommitRequestId.IsEmpty() ? TEXT("-") : *Receipt.StageGExecuteCommitRequestId,
		Receipt.StageGExecuteDispatchReceiptId.IsEmpty() ? TEXT("-") : *Receipt.StageGExecuteDispatchReceiptId,
		Receipt.StageGExecuteDispatchRequestId.IsEmpty() ? TEXT("-") : *Receipt.StageGExecuteDispatchRequestId,
		Receipt.StageGExecutePermitTicketId.IsEmpty() ? TEXT("-") : *Receipt.StageGExecutePermitTicketId,
		Receipt.StageGWriteEnableRequestId.IsEmpty() ? TEXT("-") : *Receipt.StageGWriteEnableRequestId,
		Receipt.StageGExecuteIntentId.IsEmpty() ? TEXT("-") : *Receipt.StageGExecuteIntentId,
		Receipt.SimHandoffEnvelopeId.IsEmpty() ? TEXT("-") : *Receipt.SimHandoffEnvelopeId,
		Receipt.SelectionDigest.IsEmpty() ? TEXT("-") : *Receipt.SelectionDigest,
		Receipt.StageGExecuteFinalReportDigest.IsEmpty() ? TEXT("-") : *Receipt.StageGExecuteFinalReportDigest,
		Receipt.StageGExecuteArchiveBundleDigest.IsEmpty() ? TEXT("-") : *Receipt.StageGExecuteArchiveBundleDigest,
		Receipt.bExecuteCommitConfirmed ? TEXT("true") : TEXT("false"),
		Receipt.bWriteEnabled ? TEXT("true") : TEXT("false"),
		Receipt.bReadyForStageGExecute ? TEXT("true") : TEXT("false"),
		Receipt.bStageGExecuteFinalReportReady ? TEXT("true") : TEXT("false"),
		Receipt.bStageGExecuteFinalized ? TEXT("true") : TEXT("false"),
		Receipt.bStageGExecuteArchiveBundleReady ? TEXT("true") : TEXT("false"),
		Receipt.StageGExecuteFinalReportStatus.IsEmpty() ? TEXT("-") : *Receipt.StageGExecuteFinalReportStatus,
		Receipt.StageGExecuteArchiveBundleStatus.IsEmpty() ? TEXT("-") : *Receipt.StageGExecuteArchiveBundleStatus,
		Receipt.ErrorCode.IsEmpty() ? TEXT("-") : *Receipt.ErrorCode,
		Receipt.Reason.IsEmpty() ? TEXT("-") : *Receipt.Reason);
}

static void HCI_LogAgentExecutorStageGExecuteArchiveBundleRows(const FHCIAbilityKitAgentStageGExecuteArchiveBundle& Receipt)
{
	for (int32 Index = 0; Index < Receipt.Items.Num(); ++Index)
	{
		const FHCIAbilityKitAgentApplyRequestItem& Item = Receipt.Items[Index];
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteArchiveBundle] row=%d tool_name=%s risk=%s blocked=%s skip_reason=%s object_type=%s locate_strategy=%s asset_path=%s field=%s evidence_key=%s actor_path=%s"),
			Index,
			Item.ToolName.IsEmpty() ? TEXT("-") : *Item.ToolName,
			*FHCIAbilityKitDryRunDiff::RiskToString(Item.Risk),
			Item.bBlocked ? TEXT("true") : TEXT("false"),
			Item.SkipReason.IsEmpty() ? TEXT("-") : *Item.SkipReason,
			*FHCIAbilityKitDryRunDiff::ObjectTypeToString(Item.ObjectType),
			*FHCIAbilityKitDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
			Item.AssetPath.IsEmpty() ? TEXT("-") : *Item.AssetPath,
			Item.Field.IsEmpty() ? TEXT("-") : *Item.Field,
			Item.EvidenceKey.IsEmpty() ? TEXT("-") : *Item.EvidenceKey,
			Item.ActorPath.IsEmpty() ? TEXT("-") : *Item.ActorPath);
	}
}

static bool HCI_TryBuildAgentExecutorStageGExecuteArchiveBundleFromLatestPreview(
	const FString& TamperMode,
	FHCIAbilityKitAgentStageGExecuteArchiveBundle& OutReceipt)
{
	if (HCI_StageG_State().AgentStageGExecuteFinalReportPreviewState.RequestId.IsEmpty())
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteArchiveBundle] prepare=unavailable reason=no_stage_g_execute_final_report_preview_state"));
		UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteArchiveBundle] suggestion=先运行 HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteFinalReport"));
		return false;
	}
	if (HCI_StageG_State().AgentStageGExecuteCommitReceiptPreviewState.RequestId.IsEmpty())
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteArchiveBundle] prepare=unavailable reason=no_stage_g_execute_commit_receipt_preview_state"));
		return false;
	}

	FHCIAbilityKitAgentStageGExecuteFinalReport FinalReportReceipt = HCI_StageG_State().AgentStageGExecuteFinalReportPreviewState;
	const FHCIAbilityKitAgentStageGExecuteCommitReceipt CommitReceipt = HCI_StageG_State().AgentStageGExecuteCommitReceiptPreviewState;
	const FHCIAbilityKitAgentStageGExecuteCommitRequest CommitRequest = HCI_StageG_State().AgentStageGExecuteCommitRequestPreviewState;
	const FHCIAbilityKitAgentStageGExecuteDispatchReceipt DispatchReceipt = HCI_StageG_State().AgentStageGExecuteDispatchReceiptPreviewState;
	const FHCIAbilityKitAgentStageGExecuteDispatchRequest DispatchRequest = HCI_StageG_State().AgentStageGExecuteDispatchRequestPreviewState;
	const FHCIAbilityKitAgentStageGExecutePermitTicket PermitTicket = HCI_StageG_State().AgentStageGExecutePermitTicketPreviewState;
	const FHCIAbilityKitAgentStageGWriteEnableRequest WriteEnableRequest = HCI_StageG_State().AgentStageGWriteEnableRequestPreviewState;
	const FHCIAbilityKitAgentStageGExecuteIntent Intent = HCI_StageG_State().AgentStageGExecuteIntentPreviewState;
	const FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope HandoffEnvelope = HCI_StageG_State().AgentSimulateExecuteHandoffEnvelopePreviewState;
	const FHCIAbilityKitAgentSimulateExecuteArchiveBundle SimArchiveBundle = HCI_StageG_State().AgentSimulateExecuteArchiveBundlePreviewState;
	const FHCIAbilityKitAgentSimulateExecuteFinalReport SimFinalReport = HCI_StageG_State().AgentSimulateExecuteFinalReportPreviewState;
	const FHCIAbilityKitAgentSimulateExecuteReceipt SimReceipt = HCI_StageG_State().AgentSimulateExecuteReceiptPreviewState;
	const FHCIAbilityKitAgentExecuteTicket ExecuteTicket = HCI_StageG_State().AgentExecuteTicketPreviewState;
	const FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest = HCI_StageG_State().AgentApplyConfirmRequestPreviewState;
	const FHCIAbilityKitAgentApplyRequest ApplyRequest = HCI_StageG_State().AgentApplyRequestPreviewState;
	const FHCIAbilityKitDryRunDiffReport Review = HCI_StageG_State().AgentExecutorReviewDiffPreviewState;

	FString ExpectedFinalReportId = FinalReportReceipt.RequestId;
	FString ExpectedCommitReceiptId = CommitReceipt.RequestId;
	FString ExpectedCommitReceiptDigest = CommitReceipt.StageGExecuteCommitReceiptDigest;

	if (TamperMode == TEXT("digest"))
	{
		FinalReportReceipt.SelectionDigest = TEXT("crc32_BAD0C0DE");
	}
	else if (TamperMode == TEXT("intent"))
	{
		FinalReportReceipt.StageGExecuteIntentId += TEXT("_stale");
	}
	else if (TamperMode == TEXT("handoff"))
	{
		FinalReportReceipt.SimHandoffEnvelopeId += TEXT("_stale");
	}
	else if (TamperMode == TEXT("dispatch"))
	{
		FinalReportReceipt.StageGExecuteDispatchRequestId += TEXT("_stale");
	}
	else if (TamperMode == TEXT("receipt"))
	{
		FinalReportReceipt.StageGExecuteDispatchReceiptId += TEXT("_stale");
	}
	else if (TamperMode == TEXT("commit"))
	{
		FinalReportReceipt.StageGExecuteCommitRequestId += TEXT("_stale");
	}
	else if (TamperMode == TEXT("commitreceipt"))
	{
		ExpectedCommitReceiptId += TEXT("_stale");
	}
	else if (TamperMode == TEXT("final"))
	{
		ExpectedFinalReportId += TEXT("_stale");
	}
	else if (TamperMode == TEXT("ready"))
	{
		FinalReportReceipt.bStageGExecuteFinalReportReady = false;
		FinalReportReceipt.bStageGExecuteFinalized = false;
		FinalReportReceipt.StageGExecuteFinalReportStatus = TEXT("blocked");
	}

	if (!FHCIAbilityKitAgentExecutorStageGExecuteArchiveBundleBridge::BuildStageGExecuteArchiveBundle(
		FinalReportReceipt,
		ExpectedFinalReportId,
		ExpectedCommitReceiptId,
		ExpectedCommitReceiptDigest,
		CommitReceipt,
		CommitRequest,
		DispatchReceipt,
		DispatchRequest,
		PermitTicket,
		WriteEnableRequest,
		Intent,
		HandoffEnvelope,
		SimArchiveBundle,
		SimFinalReport,
		SimReceipt,
		ExecuteTicket,
		ConfirmRequest,
		ApplyRequest,
		Review,
		OutReceipt))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteArchiveBundle] prepare_failed reason=bridge_failed"));
		return false;
	}

	HCI_StageG_State().AgentStageGExecuteArchiveBundlePreviewState = OutReceipt;
	return true;
}

static bool HCI_TryParseStageGExecuteArchiveBundleCommandArgs(const TArray<FString>& Args, FString& OutTamperMode)
{
	OutTamperMode = TEXT("none");
	if (Args.Num() == 0)
	{
		return true;
	}
	if (Args.Num() != 1)
	{
		return false;
	}
	return HCI_TryParseStageGExecuteArchiveBundleTamperModeArg(Args[0], OutTamperMode);
}

void HCI_RunAbilityKitAgentExecutePlanReviewPrepareStageGExecuteArchiveBundleCommand(const TArray<FString>& Args)
{
	FString TamperMode;
	if (!HCI_TryParseStageGExecuteArchiveBundleCommandArgs(Args, TamperMode))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteArchiveBundle] invalid_args usage=HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteArchiveBundle [tamper=none|digest|intent|handoff|dispatch|receipt|commit|commitreceipt|final|ready]"));
		return;
	}

	FHCIAbilityKitAgentStageGExecuteArchiveBundle Receipt;
	if (!HCI_TryBuildAgentExecutorStageGExecuteArchiveBundleFromLatestPreview(TamperMode, Receipt))
	{
		return;
	}

	HCI_LogAgentExecutorStageGExecuteArchiveBundleSummary(Receipt);
	HCI_LogAgentExecutorStageGExecuteArchiveBundleRows(Receipt);
	UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteArchiveBundle] hint=也可运行 HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteArchiveBundleJson [tamper] 输出 StageGExecuteArchiveBundle JSON"));
}

void HCI_RunAbilityKitAgentExecutePlanReviewPrepareStageGExecuteArchiveBundleJsonCommand(const TArray<FString>& Args)
{
	FString TamperMode;
	if (!HCI_TryParseStageGExecuteArchiveBundleCommandArgs(Args, TamperMode))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteArchiveBundle] invalid_args usage=HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteArchiveBundleJson [tamper=none|digest|intent|handoff|dispatch|receipt|commit|commitreceipt|final|ready]"));
		return;
	}

	FHCIAbilityKitAgentStageGExecuteArchiveBundle Receipt;
	if (!HCI_TryBuildAgentExecutorStageGExecuteArchiveBundleFromLatestPreview(TamperMode, Receipt))
	{
		return;
	}

	HCI_LogAgentExecutorStageGExecuteArchiveBundleSummary(Receipt);
	HCI_LogAgentExecutorStageGExecuteArchiveBundleRows(Receipt);

	FString JsonText;
	if (!FHCIAbilityKitAgentStageGExecuteArchiveBundleJsonSerializer::SerializeToJsonString(Receipt, JsonText))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteArchiveBundle] json_failed request_id=%s"), Receipt.RequestId.IsEmpty() ? TEXT("-") : *Receipt.RequestId);
		return;
	}

	UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecuteArchiveBundle] json request_id=%s bytes=%d"), Receipt.RequestId.IsEmpty() ? TEXT("-") : *Receipt.RequestId, JsonText.Len());
	UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("%s"), *JsonText);
}

static bool HCI_TryParseStageGExecutionReadinessTamperModeArg(const FString& InValue, FString& OutTamperMode)
{
	const FString Lower = InValue.TrimStartAndEnd().ToLower();
	if (Lower == TEXT("none") || Lower == TEXT("digest") || Lower == TEXT("archive") || Lower == TEXT("mode"))
	{
		OutTamperMode = Lower;
		return true;
	}
	return false;
}

static void HCI_LogAgentExecutorStageGExecutionReadinessSummary(const FHCIAbilityKitAgentStageGExecutionReadinessReport& Report)
{
	UE_LOG(
		LogHCIAbilityKitAgentDemo,
		Display,
		TEXT("[HCIAbilityKit][AgentExecutorStageGExecutionReadiness] summary request_id=%s stage_g_execute_archive_bundle_id=%s selection_digest=%s stage_g_execution_readiness_digest=%s stage_g_execution_readiness_status=%s ready_for_h1_planner_integration=%s execution_mode=%s error_code=%s reason=%s validation=ok"),
		Report.RequestId.IsEmpty() ? TEXT("-") : *Report.RequestId,
		Report.StageGExecuteArchiveBundleId.IsEmpty() ? TEXT("-") : *Report.StageGExecuteArchiveBundleId,
		Report.SelectionDigest.IsEmpty() ? TEXT("-") : *Report.SelectionDigest,
		Report.StageGExecutionReadinessDigest.IsEmpty() ? TEXT("-") : *Report.StageGExecutionReadinessDigest,
		Report.StageGExecutionReadinessStatus.IsEmpty() ? TEXT("-") : *Report.StageGExecutionReadinessStatus,
		Report.bReadyForH1PlannerIntegration ? TEXT("true") : TEXT("false"),
		Report.ExecutionMode.IsEmpty() ? TEXT("-") : *Report.ExecutionMode,
		Report.ErrorCode.IsEmpty() ? TEXT("-") : *Report.ErrorCode,
		Report.Reason.IsEmpty() ? TEXT("-") : *Report.Reason);
}

static void HCI_LogAgentExecutorStageGExecutionReadinessRows(const FHCIAbilityKitAgentStageGExecutionReadinessReport& Report)
{
	for (int32 Index = 0; Index < Report.Items.Num(); ++Index)
	{
		const FHCIAbilityKitAgentApplyRequestItem& Item = Report.Items[Index];
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentExecutorStageGExecutionReadiness] row=%d tool_name=%s risk=%s blocked=%s skip_reason=%s object_type=%s locate_strategy=%s asset_path=%s field=%s evidence_key=%s actor_path=%s"),
			Index,
			Item.ToolName.IsEmpty() ? TEXT("-") : *Item.ToolName,
			*FHCIAbilityKitDryRunDiff::RiskToString(Item.Risk),
			Item.bBlocked ? TEXT("true") : TEXT("false"),
			Item.SkipReason.IsEmpty() ? TEXT("-") : *Item.SkipReason,
			*FHCIAbilityKitDryRunDiff::ObjectTypeToString(Item.ObjectType),
			*FHCIAbilityKitDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
			Item.AssetPath.IsEmpty() ? TEXT("-") : *Item.AssetPath,
			Item.Field.IsEmpty() ? TEXT("-") : *Item.Field,
			Item.EvidenceKey.IsEmpty() ? TEXT("-") : *Item.EvidenceKey,
			Item.ActorPath.IsEmpty() ? TEXT("-") : *Item.ActorPath);
	}
}

static bool HCI_TryWarmupPreviewStateForStageGExecutionReadiness()
{
	HCI_StageG_State().AgentPlanPreviewState = FHCIAbilityKitAgentPlan();
	HCI_StageG_State().AgentExecutorReviewDiffPreviewState = FHCIAbilityKitDryRunDiffReport();
	HCI_StageG_State().AgentApplyRequestPreviewState = FHCIAbilityKitAgentApplyRequest();
	HCI_StageG_State().AgentApplyConfirmRequestPreviewState = FHCIAbilityKitAgentApplyConfirmRequest();
	HCI_StageG_State().AgentExecuteTicketPreviewState = FHCIAbilityKitAgentExecuteTicket();
	HCI_StageG_State().AgentSimulateExecuteReceiptPreviewState = FHCIAbilityKitAgentSimulateExecuteReceipt();
	HCI_StageG_State().AgentSimulateExecuteFinalReportPreviewState = FHCIAbilityKitAgentSimulateExecuteFinalReport();
	HCI_StageG_State().AgentSimulateExecuteArchiveBundlePreviewState = FHCIAbilityKitAgentSimulateExecuteArchiveBundle();
	HCI_StageG_State().AgentSimulateExecuteHandoffEnvelopePreviewState = FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope();
	HCI_StageG_State().AgentStageGExecuteIntentPreviewState = FHCIAbilityKitAgentStageGExecuteIntent();
	HCI_StageG_State().AgentStageGWriteEnableRequestPreviewState = FHCIAbilityKitAgentStageGWriteEnableRequest();
	HCI_StageG_State().AgentStageGExecutePermitTicketPreviewState = FHCIAbilityKitAgentStageGExecutePermitTicket();
	HCI_StageG_State().AgentStageGExecuteDispatchRequestPreviewState = FHCIAbilityKitAgentStageGExecuteDispatchRequest();
	HCI_StageG_State().AgentStageGExecuteDispatchReceiptPreviewState = FHCIAbilityKitAgentStageGExecuteDispatchReceipt();
	HCI_StageG_State().AgentStageGExecuteCommitRequestPreviewState = FHCIAbilityKitAgentStageGExecuteCommitRequest();
	HCI_StageG_State().AgentStageGExecuteCommitReceiptPreviewState = FHCIAbilityKitAgentStageGExecuteCommitReceipt();
	HCI_StageG_State().AgentStageGExecuteFinalReportPreviewState = FHCIAbilityKitAgentStageGExecuteFinalReport();
	HCI_StageG_State().AgentStageGExecuteArchiveBundlePreviewState = FHCIAbilityKitAgentStageGExecuteArchiveBundle();
	HCI_StageG_State().AgentStageGExecutionReadinessReportPreviewState = FHCIAbilityKitAgentStageGExecutionReadinessReport();

	const TArray<FString> ReviewSeedArgs = { TEXT("ok_naming") };
	HCI_RunAbilityKitAgentExecutePlanReviewDemoCommand(ReviewSeedArgs);
	if (HCI_StageG_State().AgentExecutorReviewDiffPreviewState.DiffItems.Num() <= 0)
	{
		return false;
	}

	FHCIAbilityKitAgentApplyRequest ApplyRequest;
	if (HCI_StageG_State().AgentApplyRequestPreviewState.Items.Num() <= 0 &&
		!HCI_TryBuildAgentExecutorApplyRequestFromLatestReview(ApplyRequest))
	{
		return false;
	}

	FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest;
	if (HCI_StageG_State().AgentApplyConfirmRequestPreviewState.Items.Num() <= 0 &&
		!HCI_TryBuildAgentExecutorConfirmRequestFromLatestPreview(true, TEXT("none"), ConfirmRequest))
	{
		return false;
	}

	FHCIAbilityKitAgentExecuteTicket ExecuteTicket;
	if (HCI_StageG_State().AgentExecuteTicketPreviewState.Items.Num() <= 0 &&
		!HCI_TryBuildAgentExecutorExecuteTicketFromLatestPreview(TEXT("none"), ExecuteTicket))
	{
		return false;
	}

	FHCIAbilityKitAgentSimulateExecuteReceipt SimReceipt;
	if (HCI_StageG_State().AgentSimulateExecuteReceiptPreviewState.Items.Num() <= 0 &&
		!HCI_TryBuildAgentExecutorSimExecuteReceiptFromLatestPreview(TEXT("none"), SimReceipt))
	{
		return false;
	}

	FHCIAbilityKitAgentSimulateExecuteFinalReport SimFinalReport;
	if (HCI_StageG_State().AgentSimulateExecuteFinalReportPreviewState.Items.Num() <= 0 &&
		!HCI_TryBuildAgentExecutorSimFinalReportFromLatestPreview(TEXT("none"), SimFinalReport))
	{
		return false;
	}

	FHCIAbilityKitAgentSimulateExecuteArchiveBundle SimArchiveBundle;
	if (HCI_StageG_State().AgentSimulateExecuteArchiveBundlePreviewState.Items.Num() <= 0 &&
		!HCI_TryBuildAgentExecutorSimArchiveBundleFromLatestPreview(TEXT("none"), SimArchiveBundle))
	{
		return false;
	}

	FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope HandoffEnvelope;
	if (HCI_StageG_State().AgentSimulateExecuteHandoffEnvelopePreviewState.Items.Num() <= 0 &&
		!HCI_TryBuildAgentExecutorSimHandoffEnvelopeFromLatestPreview(TEXT("none"), HandoffEnvelope))
	{
		return false;
	}

	FHCIAbilityKitAgentStageGExecuteIntent StageGIntent;
	if (HCI_StageG_State().AgentStageGExecuteIntentPreviewState.Items.Num() <= 0 &&
		!HCI_TryBuildAgentExecutorStageGExecuteIntentFromLatestPreview(TEXT("none"), StageGIntent))
	{
		return false;
	}

	FHCIAbilityKitAgentStageGWriteEnableRequest StageGWriteEnableRequest;
	if (HCI_StageG_State().AgentStageGWriteEnableRequestPreviewState.Items.Num() <= 0 &&
		!HCI_TryBuildAgentExecutorStageGWriteEnableRequestFromLatestPreview(true, TEXT("none"), StageGWriteEnableRequest))
	{
		return false;
	}

	FHCIAbilityKitAgentStageGExecutePermitTicket StageGPermitTicket;
	if (HCI_StageG_State().AgentStageGExecutePermitTicketPreviewState.Items.Num() <= 0 &&
		!HCI_TryBuildAgentExecutorStageGExecutePermitTicketFromLatestPreview(true, TEXT("none"), StageGPermitTicket))
	{
		return false;
	}

	FHCIAbilityKitAgentStageGExecuteDispatchRequest StageGDispatchRequest;
	if (HCI_StageG_State().AgentStageGExecuteDispatchRequestPreviewState.Items.Num() <= 0 &&
		!HCI_TryBuildAgentExecutorStageGExecuteDispatchRequestFromLatestPreview(true, TEXT("none"), StageGDispatchRequest))
	{
		return false;
	}

	FHCIAbilityKitAgentStageGExecuteDispatchReceipt StageGDispatchReceipt;
	if (HCI_StageG_State().AgentStageGExecuteDispatchReceiptPreviewState.Items.Num() <= 0 &&
		!HCI_TryBuildAgentExecutorStageGExecuteDispatchReceiptFromLatestPreview(TEXT("none"), StageGDispatchReceipt))
	{
		return false;
	}

	FHCIAbilityKitAgentStageGExecuteCommitRequest StageGCommitRequest;
	if (HCI_StageG_State().AgentStageGExecuteCommitRequestPreviewState.Items.Num() <= 0 &&
		!HCI_TryBuildAgentExecutorStageGExecuteCommitRequestFromLatestPreview(true, TEXT("none"), StageGCommitRequest))
	{
		return false;
	}

	FHCIAbilityKitAgentStageGExecuteCommitReceipt StageGCommitReceipt;
	if (HCI_StageG_State().AgentStageGExecuteCommitReceiptPreviewState.Items.Num() <= 0 &&
		!HCI_TryBuildAgentExecutorStageGExecuteCommitReceiptFromLatestPreview(TEXT("none"), StageGCommitReceipt))
	{
		return false;
	}

	FHCIAbilityKitAgentStageGExecuteFinalReport StageGFinalReport;
	if (HCI_StageG_State().AgentStageGExecuteFinalReportPreviewState.Items.Num() <= 0 &&
		!HCI_TryBuildAgentExecutorStageGExecuteFinalReportFromLatestPreview(TEXT("none"), StageGFinalReport))
	{
		return false;
	}

	FHCIAbilityKitAgentStageGExecuteArchiveBundle StageGArchiveBundle;
	if (HCI_StageG_State().AgentStageGExecuteArchiveBundlePreviewState.Items.Num() <= 0 &&
		!HCI_TryBuildAgentExecutorStageGExecuteArchiveBundleFromLatestPreview(TEXT("none"), StageGArchiveBundle))
	{
		return false;
	}

	return HCI_StageG_State().AgentStageGExecuteArchiveBundlePreviewState.Items.Num() > 0;
}

static bool HCI_TryBuildAgentExecutorStageGExecutionReadinessFromLatestPreview(
	const bool bUserConfirmed,
	const FString& TamperMode,
	FHCIAbilityKitAgentStageGExecutionReadinessReport& OutReport)
{
	if (HCI_StageG_State().AgentStageGExecuteArchiveBundlePreviewState.RequestId.IsEmpty())
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecutionReadiness] warmup=start reason=missing_stage_g_execute_archive_bundle_preview_state"));
		if (!HCI_TryWarmupPreviewStateForStageGExecutionReadiness() || HCI_StageG_State().AgentStageGExecuteArchiveBundlePreviewState.RequestId.IsEmpty())
		{
			UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorStageGExecutionReadiness] prepare=unavailable reason=no_stage_g_execute_archive_bundle_preview_state_after_warmup"));
			UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecutionReadiness] suggestion=先运行 HCIAbilityKit.AgentExecutePlanReviewDemo"));
			return false;
		}
		UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecutionReadiness] warmup=done source=auto_bootstrap_chain"));
	}

	FHCIAbilityKitAgentStageGExecuteArchiveBundle WorkingArchive = HCI_StageG_State().AgentStageGExecuteArchiveBundlePreviewState;
	FString ExpectedArchiveBundleId = WorkingArchive.RequestId;

	if (TamperMode == TEXT("digest"))
	{
		WorkingArchive.SelectionDigest = TEXT("crc32_BAD0C0DE");
	}
	else if (TamperMode == TEXT("archive"))
	{
		WorkingArchive.bStageGExecuteArchiveBundleReady = false;
		WorkingArchive.bStageGExecuteArchived = false;
		WorkingArchive.StageGExecuteArchiveBundleStatus = TEXT("blocked");
	}
	else if (TamperMode == TEXT("mode"))
	{
		WorkingArchive.ExecutionMode = TEXT("write_execute");
	}

	if (!FHCIAbilityKitAgentExecutorStageGExecutionReadinessReportBridge::BuildStageGExecutionReadinessReport(
		WorkingArchive,
		ExpectedArchiveBundleId,
		HCI_StageG_State().AgentExecutorReviewDiffPreviewState,
		bUserConfirmed,
		OutReport))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorStageGExecutionReadiness] prepare_failed reason=bridge_failed"));
		return false;
	}

	HCI_StageG_State().AgentStageGExecutionReadinessReportPreviewState = OutReport;
	return true;
}

static bool HCI_TryParseStageGExecutionReadinessCommandArgs(
	const TArray<FString>& Args,
	bool& OutUserConfirmed,
	FString& OutTamperMode)
{
	OutUserConfirmed = true;
	OutTamperMode = TEXT("none");
	if (Args.Num() == 0)
	{
		return true;
	}
	if (Args.Num() == 1)
	{
		return HCI_TryParseBool01Arg(Args[0], OutUserConfirmed);
	}
	if (Args.Num() == 2)
	{
		return HCI_TryParseBool01Arg(Args[0], OutUserConfirmed) &&
			HCI_TryParseStageGExecutionReadinessTamperModeArg(Args[1], OutTamperMode);
	}
	return false;
}

void HCI_RunAbilityKitAgentExecutePlanReviewPrepareStageGExecutionReadinessCommand(const TArray<FString>& Args)
{
	bool bUserConfirmed = true;
	FString TamperMode;
	if (!HCI_TryParseStageGExecutionReadinessCommandArgs(Args, bUserConfirmed, TamperMode))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorStageGExecutionReadiness] invalid_args usage=HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecutionReadiness [user_confirmed=0|1] [tamper=none|digest|archive|mode]"));
		return;
	}

	FHCIAbilityKitAgentStageGExecutionReadinessReport Report;
	if (!HCI_TryBuildAgentExecutorStageGExecutionReadinessFromLatestPreview(bUserConfirmed, TamperMode, Report))
	{
		return;
	}

	HCI_LogAgentExecutorStageGExecutionReadinessSummary(Report);
	HCI_LogAgentExecutorStageGExecutionReadinessRows(Report);
	UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecutionReadiness] hint=也可运行 HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecutionReadinessJson [user_confirmed] [tamper] 输出 StageGExecutionReadinessReport JSON"));
}

void HCI_RunAbilityKitAgentExecutePlanReviewPrepareStageGExecutionReadinessJsonCommand(const TArray<FString>& Args)
{
	bool bUserConfirmed = true;
	FString TamperMode;
	if (!HCI_TryParseStageGExecutionReadinessCommandArgs(Args, bUserConfirmed, TamperMode))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorStageGExecutionReadiness] invalid_args usage=HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecutionReadinessJson [user_confirmed=0|1] [tamper=none|digest|archive|mode]"));
		return;
	}

	FHCIAbilityKitAgentStageGExecutionReadinessReport Report;
	if (!HCI_TryBuildAgentExecutorStageGExecutionReadinessFromLatestPreview(bUserConfirmed, TamperMode, Report))
	{
		return;
	}

	HCI_LogAgentExecutorStageGExecutionReadinessSummary(Report);
	HCI_LogAgentExecutorStageGExecutionReadinessRows(Report);

	FString JsonText;
	if (!FHCIAbilityKitAgentStageGExecutionReadinessReportJsonSerializer::SerializeToJsonString(Report, JsonText))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorStageGExecutionReadiness] json_failed request_id=%s"), Report.RequestId.IsEmpty() ? TEXT("-") : *Report.RequestId);
		return;
	}

	UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("[HCIAbilityKit][AgentExecutorStageGExecutionReadiness] json request_id=%s bytes=%d"), Report.RequestId.IsEmpty() ? TEXT("-") : *Report.RequestId, JsonText.Len());
	UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("%s"), *JsonText);
}

#undef LogHCIAbilityKitAgentDemo
