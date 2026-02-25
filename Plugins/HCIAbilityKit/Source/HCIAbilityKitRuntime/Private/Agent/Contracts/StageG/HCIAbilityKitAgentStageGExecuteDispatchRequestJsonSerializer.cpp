#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGExecuteDispatchRequestJsonSerializer.h"

#include "Agent/Contracts/StageF/HCIAbilityKitAgentApplyRequest.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGExecuteDispatchRequest.h"
#include "Agent/Executor/HCIAbilityKitDryRunDiff.h"
#include "Dom/JsonObject.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

bool FHCIAbilityKitAgentStageGExecuteDispatchRequestJsonSerializer::SerializeToJsonString(
	const FHCIAbilityKitAgentStageGExecuteDispatchRequest& Request,
	FString& OutJsonText)
{
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("request_id"), Request.RequestId);
	Root->SetStringField(TEXT("stage_g_execute_permit_ticket_id"), Request.StageGExecutePermitTicketId);
	Root->SetStringField(TEXT("stage_g_write_enable_request_id"), Request.StageGWriteEnableRequestId);
	Root->SetStringField(TEXT("stage_g_execute_intent_id"), Request.StageGExecuteIntentId);
	Root->SetStringField(TEXT("sim_handoff_envelope_id"), Request.SimHandoffEnvelopeId);
	Root->SetStringField(TEXT("sim_archive_bundle_id"), Request.SimArchiveBundleId);
	Root->SetStringField(TEXT("sim_final_report_id"), Request.SimFinalReportId);
	Root->SetStringField(TEXT("sim_execute_receipt_id"), Request.SimExecuteReceiptId);
	Root->SetStringField(TEXT("execute_ticket_id"), Request.ExecuteTicketId);
	Root->SetStringField(TEXT("confirm_request_id"), Request.ConfirmRequestId);
	Root->SetStringField(TEXT("apply_request_id"), Request.ApplyRequestId);
	Root->SetStringField(TEXT("review_request_id"), Request.ReviewRequestId);
	Root->SetStringField(TEXT("selection_digest"), Request.SelectionDigest);
	Root->SetStringField(TEXT("archive_digest"), Request.ArchiveDigest);
	Root->SetStringField(TEXT("handoff_digest"), Request.HandoffDigest);
	Root->SetStringField(TEXT("execute_intent_digest"), Request.ExecuteIntentDigest);
	Root->SetStringField(TEXT("stage_g_write_enable_digest"), Request.StageGWriteEnableDigest);
	Root->SetStringField(TEXT("stage_g_execute_permit_digest"), Request.StageGExecutePermitDigest);
	Root->SetStringField(TEXT("stage_g_execute_dispatch_digest"), Request.StageGExecuteDispatchDigest);
	Root->SetStringField(TEXT("generated_utc"), Request.GeneratedUtc);
	Root->SetStringField(TEXT("execution_mode"), Request.ExecutionMode);
	Root->SetStringField(TEXT("execute_target"), Request.ExecuteTarget);
	Root->SetStringField(TEXT("handoff_target"), Request.HandoffTarget);
	Root->SetStringField(TEXT("transaction_mode"), Request.TransactionMode);
	Root->SetStringField(TEXT("termination_policy"), Request.TerminationPolicy);
	Root->SetStringField(TEXT("terminal_status"), Request.TerminalStatus);
	Root->SetStringField(TEXT("archive_status"), Request.ArchiveStatus);
	Root->SetStringField(TEXT("handoff_status"), Request.HandoffStatus);
	Root->SetStringField(TEXT("stage_g_status"), Request.StageGStatus);
	Root->SetStringField(TEXT("stage_g_write_status"), Request.StageGWriteStatus);
	Root->SetStringField(TEXT("stage_g_execute_permit_status"), Request.StageGExecutePermitStatus);
	Root->SetStringField(TEXT("stage_g_execute_dispatch_status"), Request.StageGExecuteDispatchStatus);
	Root->SetBoolField(TEXT("user_confirmed"), Request.bUserConfirmed);
	Root->SetBoolField(TEXT("ready_to_simulate_execute"), Request.bReadyToSimulateExecute);
	Root->SetBoolField(TEXT("simulated_dispatch_accepted"), Request.bSimulatedDispatchAccepted);
	Root->SetBoolField(TEXT("simulation_completed"), Request.bSimulationCompleted);
	Root->SetBoolField(TEXT("archive_ready"), Request.bArchiveReady);
	Root->SetBoolField(TEXT("handoff_ready"), Request.bHandoffReady);
	Root->SetBoolField(TEXT("write_enabled"), Request.bWriteEnabled);
	Root->SetBoolField(TEXT("ready_for_stage_g_entry"), Request.bReadyForStageGEntry);
	Root->SetBoolField(TEXT("write_enable_confirmed"), Request.bWriteEnableConfirmed);
	Root->SetBoolField(TEXT("ready_for_stage_g_execute"), Request.bReadyForStageGExecute);
	Root->SetBoolField(TEXT("stage_g_execute_permit_ready"), Request.bStageGExecutePermitReady);
	Root->SetBoolField(TEXT("execute_dispatch_confirmed"), Request.bExecuteDispatchConfirmed);
	Root->SetBoolField(TEXT("stage_g_execute_dispatch_ready"), Request.bStageGExecuteDispatchReady);
	Root->SetStringField(TEXT("error_code"), Request.ErrorCode);
	Root->SetStringField(TEXT("reason"), Request.Reason);

	const TSharedRef<FJsonObject> Summary = MakeShared<FJsonObject>();
	Summary->SetNumberField(TEXT("total_rows"), Request.Summary.TotalRows);
	Summary->SetNumberField(TEXT("selected_rows"), Request.Summary.SelectedRows);
	Summary->SetNumberField(TEXT("modifiable_rows"), Request.Summary.ModifiableRows);
	Summary->SetNumberField(TEXT("blocked_rows"), Request.Summary.BlockedRows);
	Summary->SetNumberField(TEXT("read_only_rows"), Request.Summary.ReadOnlyRows);
	Summary->SetNumberField(TEXT("write_rows"), Request.Summary.WriteRows);
	Summary->SetNumberField(TEXT("destructive_rows"), Request.Summary.DestructiveRows);
	Root->SetObjectField(TEXT("summary"), Summary);

	TArray<TSharedPtr<FJsonValue>> ItemsJson;
	ItemsJson.Reserve(Request.Items.Num());
	for (const FHCIAbilityKitAgentApplyRequestItem& Item : Request.Items)
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