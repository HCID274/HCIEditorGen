#include "Agent/HCIAbilityKitAgentStageGExecuteArchiveBundleJsonSerializer.h"

#include "Agent/HCIAbilityKitAgentApplyRequest.h"
#include "Agent/HCIAbilityKitAgentStageGExecuteArchiveBundle.h"
#include "Agent/HCIAbilityKitDryRunDiff.h"
#include "Dom/JsonObject.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

bool FHCIAbilityKitAgentStageGExecuteArchiveBundleJsonSerializer::SerializeToJsonString(
	const FHCIAbilityKitAgentStageGExecuteArchiveBundle& Receipt,
	FString& OutJsonText)
{
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("request_id"), Receipt.RequestId);
	Root->SetStringField(TEXT("stage_g_execute_final_report_id"), Receipt.StageGExecuteFinalReportId);
	Root->SetStringField(TEXT("stage_g_execute_commit_receipt_id"), Receipt.StageGExecuteCommitReceiptId);
	Root->SetStringField(TEXT("stage_g_execute_commit_request_id"), Receipt.StageGExecuteCommitRequestId);
	Root->SetStringField(TEXT("stage_g_execute_dispatch_receipt_id"), Receipt.StageGExecuteDispatchReceiptId);
	Root->SetStringField(TEXT("stage_g_execute_dispatch_request_id"), Receipt.StageGExecuteDispatchRequestId);
	Root->SetStringField(TEXT("stage_g_execute_permit_ticket_id"), Receipt.StageGExecutePermitTicketId);
	Root->SetStringField(TEXT("stage_g_write_enable_request_id"), Receipt.StageGWriteEnableRequestId);
	Root->SetStringField(TEXT("stage_g_execute_intent_id"), Receipt.StageGExecuteIntentId);
	Root->SetStringField(TEXT("sim_handoff_envelope_id"), Receipt.SimHandoffEnvelopeId);
	Root->SetStringField(TEXT("sim_archive_bundle_id"), Receipt.SimArchiveBundleId);
	Root->SetStringField(TEXT("sim_final_report_id"), Receipt.SimFinalReportId);
	Root->SetStringField(TEXT("sim_execute_receipt_id"), Receipt.SimExecuteReceiptId);
	Root->SetStringField(TEXT("execute_ticket_id"), Receipt.ExecuteTicketId);
	Root->SetStringField(TEXT("confirm_request_id"), Receipt.ConfirmRequestId);
	Root->SetStringField(TEXT("apply_request_id"), Receipt.ApplyRequestId);
	Root->SetStringField(TEXT("review_request_id"), Receipt.ReviewRequestId);
	Root->SetStringField(TEXT("selection_digest"), Receipt.SelectionDigest);
	Root->SetStringField(TEXT("archive_digest"), Receipt.ArchiveDigest);
	Root->SetStringField(TEXT("handoff_digest"), Receipt.HandoffDigest);
	Root->SetStringField(TEXT("execute_intent_digest"), Receipt.ExecuteIntentDigest);
	Root->SetStringField(TEXT("stage_g_write_enable_digest"), Receipt.StageGWriteEnableDigest);
	Root->SetStringField(TEXT("stage_g_execute_permit_digest"), Receipt.StageGExecutePermitDigest);
	Root->SetStringField(TEXT("stage_g_execute_dispatch_digest"), Receipt.StageGExecuteDispatchDigest);
	Root->SetStringField(TEXT("stage_g_execute_dispatch_receipt_digest"), Receipt.StageGExecuteDispatchReceiptDigest);
	Root->SetStringField(TEXT("stage_g_execute_commit_request_digest"), Receipt.StageGExecuteCommitRequestDigest);
	Root->SetStringField(TEXT("stage_g_execute_commit_receipt_digest"), Receipt.StageGExecuteCommitReceiptDigest);
	Root->SetStringField(TEXT("stage_g_execute_final_report_digest"), Receipt.StageGExecuteFinalReportDigest);
	Root->SetStringField(TEXT("stage_g_execute_archive_bundle_digest"), Receipt.StageGExecuteArchiveBundleDigest);
	Root->SetStringField(TEXT("generated_utc"), Receipt.GeneratedUtc);
	Root->SetStringField(TEXT("execution_mode"), Receipt.ExecutionMode);
	Root->SetStringField(TEXT("execute_target"), Receipt.ExecuteTarget);
	Root->SetStringField(TEXT("handoff_target"), Receipt.HandoffTarget);
	Root->SetStringField(TEXT("transaction_mode"), Receipt.TransactionMode);
	Root->SetStringField(TEXT("termination_policy"), Receipt.TerminationPolicy);
	Root->SetStringField(TEXT("terminal_status"), Receipt.TerminalStatus);
	Root->SetStringField(TEXT("archive_status"), Receipt.ArchiveStatus);
	Root->SetStringField(TEXT("handoff_status"), Receipt.HandoffStatus);
	Root->SetStringField(TEXT("stage_g_status"), Receipt.StageGStatus);
	Root->SetStringField(TEXT("stage_g_write_status"), Receipt.StageGWriteStatus);
	Root->SetStringField(TEXT("stage_g_execute_permit_status"), Receipt.StageGExecutePermitStatus);
	Root->SetStringField(TEXT("stage_g_execute_dispatch_status"), Receipt.StageGExecuteDispatchStatus);
	Root->SetStringField(TEXT("stage_g_execute_dispatch_receipt_status"), Receipt.StageGExecuteDispatchReceiptStatus);
	Root->SetStringField(TEXT("stage_g_execute_commit_request_status"), Receipt.StageGExecuteCommitRequestStatus);
	Root->SetStringField(TEXT("stage_g_execute_commit_receipt_status"), Receipt.StageGExecuteCommitReceiptStatus);
	Root->SetStringField(TEXT("stage_g_execute_final_report_status"), Receipt.StageGExecuteFinalReportStatus);
	Root->SetStringField(TEXT("stage_g_execute_archive_bundle_status"), Receipt.StageGExecuteArchiveBundleStatus);
	Root->SetBoolField(TEXT("user_confirmed"), Receipt.bUserConfirmed);
	Root->SetBoolField(TEXT("ready_to_simulate_execute"), Receipt.bReadyToSimulateExecute);
	Root->SetBoolField(TEXT("simulated_dispatch_accepted"), Receipt.bSimulatedDispatchAccepted);
	Root->SetBoolField(TEXT("simulation_completed"), Receipt.bSimulationCompleted);
	Root->SetBoolField(TEXT("archive_ready"), Receipt.bArchiveReady);
	Root->SetBoolField(TEXT("handoff_ready"), Receipt.bHandoffReady);
	Root->SetBoolField(TEXT("write_enabled"), Receipt.bWriteEnabled);
	Root->SetBoolField(TEXT("ready_for_stage_g_entry"), Receipt.bReadyForStageGEntry);
	Root->SetBoolField(TEXT("write_enable_confirmed"), Receipt.bWriteEnableConfirmed);
	Root->SetBoolField(TEXT("ready_for_stage_g_execute"), Receipt.bReadyForStageGExecute);
	Root->SetBoolField(TEXT("stage_g_execute_permit_ready"), Receipt.bStageGExecutePermitReady);
	Root->SetBoolField(TEXT("execute_dispatch_confirmed"), Receipt.bExecuteDispatchConfirmed);
	Root->SetBoolField(TEXT("stage_g_execute_dispatch_ready"), Receipt.bStageGExecuteDispatchReady);
	Root->SetBoolField(TEXT("stage_g_execute_dispatch_accepted"), Receipt.bStageGExecuteDispatchAccepted);
	Root->SetBoolField(TEXT("stage_g_execute_dispatch_receipt_ready"), Receipt.bStageGExecuteDispatchReceiptReady);
	Root->SetBoolField(TEXT("execute_commit_confirmed"), Receipt.bExecuteCommitConfirmed);
	Root->SetBoolField(TEXT("stage_g_execute_commit_request_ready"), Receipt.bStageGExecuteCommitRequestReady);
	Root->SetBoolField(TEXT("stage_g_execute_commit_accepted"), Receipt.bStageGExecuteCommitAccepted);
	Root->SetBoolField(TEXT("stage_g_execute_commit_receipt_ready"), Receipt.bStageGExecuteCommitReceiptReady);
	Root->SetBoolField(TEXT("stage_g_execute_finalized"), Receipt.bStageGExecuteFinalized);
	Root->SetBoolField(TEXT("stage_g_execute_final_report_ready"), Receipt.bStageGExecuteFinalReportReady);
	Root->SetBoolField(TEXT("stage_g_execute_archived"), Receipt.bStageGExecuteArchived);
	Root->SetBoolField(TEXT("stage_g_execute_archive_bundle_ready"), Receipt.bStageGExecuteArchiveBundleReady);
	Root->SetStringField(TEXT("error_code"), Receipt.ErrorCode);
	Root->SetStringField(TEXT("reason"), Receipt.Reason);

	const TSharedRef<FJsonObject> Summary = MakeShared<FJsonObject>();
	Summary->SetNumberField(TEXT("total_rows"), Receipt.Summary.TotalRows);
	Summary->SetNumberField(TEXT("selected_rows"), Receipt.Summary.SelectedRows);
	Summary->SetNumberField(TEXT("modifiable_rows"), Receipt.Summary.ModifiableRows);
	Summary->SetNumberField(TEXT("blocked_rows"), Receipt.Summary.BlockedRows);
	Summary->SetNumberField(TEXT("read_only_rows"), Receipt.Summary.ReadOnlyRows);
	Summary->SetNumberField(TEXT("write_rows"), Receipt.Summary.WriteRows);
	Summary->SetNumberField(TEXT("destructive_rows"), Receipt.Summary.DestructiveRows);
	Root->SetObjectField(TEXT("summary"), Summary);

	TArray<TSharedPtr<FJsonValue>> ItemsJson;
	ItemsJson.Reserve(Receipt.Items.Num());
	for (const FHCIAbilityKitAgentApplyRequestItem& Item : Receipt.Items)
	{
		const TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetNumberField(TEXT("row_index"), Item.RowIndex);
		Obj->SetStringField(TEXT("tool_name"), Item.ToolName);
		Obj->SetStringField(TEXT("risk"), FHCIAbilityKitDryRunDiff::RiskToString(Item.Risk));
		Obj->SetStringField(TEXT("asset_path"), Item.AssetPath);
		Obj->SetStringField(TEXT("field"), Item.Field);
		Obj->SetStringField(TEXT("skip_reason"), Item.SkipReason);
		Obj->SetBoolField(TEXT("blocked"), Item.bBlocked);
		Obj->SetStringField(TEXT("object_type"), FHCIAbilityKitDryRunDiff::ObjectTypeToString(Item.ObjectType));
		Obj->SetStringField(TEXT("locate_strategy"), FHCIAbilityKitDryRunDiff::LocateStrategyToString(Item.LocateStrategy));
		Obj->SetStringField(TEXT("evidence_key"), Item.EvidenceKey);
		Obj->SetStringField(TEXT("actor_path"), Item.ActorPath);
		ItemsJson.Add(MakeShared<FJsonValueObject>(Obj));
	}
	Root->SetArrayField(TEXT("items"), ItemsJson);

	OutJsonText.Reset();
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutJsonText);
	return FJsonSerializer::Serialize(Root, Writer);
}



