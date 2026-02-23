#include "Agent/HCIAbilityKitAgentStageGExecuteIntentJsonSerializer.h"

#include "Agent/HCIAbilityKitAgentApplyRequest.h"
#include "Agent/HCIAbilityKitAgentStageGExecuteIntent.h"
#include "Agent/HCIAbilityKitDryRunDiff.h"
#include "Dom/JsonObject.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

bool FHCIAbilityKitAgentStageGExecuteIntentJsonSerializer::SerializeToJsonString(
	const FHCIAbilityKitAgentStageGExecuteIntent& Intent,
	FString& OutJsonText)
{
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("request_id"), Intent.RequestId);
	Root->SetStringField(TEXT("sim_handoff_envelope_id"), Intent.SimHandoffEnvelopeId);
	Root->SetStringField(TEXT("sim_archive_bundle_id"), Intent.SimArchiveBundleId);
	Root->SetStringField(TEXT("sim_final_report_id"), Intent.SimFinalReportId);
	Root->SetStringField(TEXT("sim_execute_receipt_id"), Intent.SimExecuteReceiptId);
	Root->SetStringField(TEXT("execute_ticket_id"), Intent.ExecuteTicketId);
	Root->SetStringField(TEXT("confirm_request_id"), Intent.ConfirmRequestId);
	Root->SetStringField(TEXT("apply_request_id"), Intent.ApplyRequestId);
	Root->SetStringField(TEXT("review_request_id"), Intent.ReviewRequestId);
	Root->SetStringField(TEXT("selection_digest"), Intent.SelectionDigest);
	Root->SetStringField(TEXT("archive_digest"), Intent.ArchiveDigest);
	Root->SetStringField(TEXT("handoff_digest"), Intent.HandoffDigest);
	Root->SetStringField(TEXT("execute_intent_digest"), Intent.ExecuteIntentDigest);
	Root->SetStringField(TEXT("generated_utc"), Intent.GeneratedUtc);
	Root->SetStringField(TEXT("execution_mode"), Intent.ExecutionMode);
	Root->SetStringField(TEXT("execute_target"), Intent.ExecuteTarget);
	Root->SetStringField(TEXT("handoff_target"), Intent.HandoffTarget);
	Root->SetStringField(TEXT("transaction_mode"), Intent.TransactionMode);
	Root->SetStringField(TEXT("termination_policy"), Intent.TerminationPolicy);
	Root->SetStringField(TEXT("terminal_status"), Intent.TerminalStatus);
	Root->SetStringField(TEXT("archive_status"), Intent.ArchiveStatus);
	Root->SetStringField(TEXT("handoff_status"), Intent.HandoffStatus);
	Root->SetStringField(TEXT("stage_g_status"), Intent.StageGStatus);
	Root->SetBoolField(TEXT("user_confirmed"), Intent.bUserConfirmed);
	Root->SetBoolField(TEXT("ready_to_simulate_execute"), Intent.bReadyToSimulateExecute);
	Root->SetBoolField(TEXT("simulated_dispatch_accepted"), Intent.bSimulatedDispatchAccepted);
	Root->SetBoolField(TEXT("simulation_completed"), Intent.bSimulationCompleted);
	Root->SetBoolField(TEXT("archive_ready"), Intent.bArchiveReady);
	Root->SetBoolField(TEXT("handoff_ready"), Intent.bHandoffReady);
	Root->SetBoolField(TEXT("write_enabled"), Intent.bWriteEnabled);
	Root->SetBoolField(TEXT("ready_for_stage_g_entry"), Intent.bReadyForStageGEntry);
	Root->SetStringField(TEXT("error_code"), Intent.ErrorCode);
	Root->SetStringField(TEXT("reason"), Intent.Reason);

	const TSharedRef<FJsonObject> Summary = MakeShared<FJsonObject>();
	Summary->SetNumberField(TEXT("total_rows"), Intent.Summary.TotalRows);
	Summary->SetNumberField(TEXT("selected_rows"), Intent.Summary.SelectedRows);
	Summary->SetNumberField(TEXT("modifiable_rows"), Intent.Summary.ModifiableRows);
	Summary->SetNumberField(TEXT("blocked_rows"), Intent.Summary.BlockedRows);
	Summary->SetNumberField(TEXT("read_only_rows"), Intent.Summary.ReadOnlyRows);
	Summary->SetNumberField(TEXT("write_rows"), Intent.Summary.WriteRows);
	Summary->SetNumberField(TEXT("destructive_rows"), Intent.Summary.DestructiveRows);
	Root->SetObjectField(TEXT("summary"), Summary);

	TArray<TSharedPtr<FJsonValue>> ItemsJson;
	ItemsJson.Reserve(Intent.Items.Num());
	for (const FHCIAbilityKitAgentApplyRequestItem& Item : Intent.Items)
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

