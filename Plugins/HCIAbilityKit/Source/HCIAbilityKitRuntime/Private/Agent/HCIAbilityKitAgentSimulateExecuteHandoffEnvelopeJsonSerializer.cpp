#include "Agent/HCIAbilityKitAgentSimulateExecuteHandoffEnvelopeJsonSerializer.h"

#include "Agent/HCIAbilityKitAgentApplyRequest.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteHandoffEnvelope.h"
#include "Agent/HCIAbilityKitDryRunDiff.h"
#include "Dom/JsonObject.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

bool FHCIAbilityKitAgentSimulateExecuteHandoffEnvelopeJsonSerializer::SerializeToJsonString(
	const FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope& Envelope,
	FString& OutJsonText)
{
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("request_id"), Envelope.RequestId);
	Root->SetStringField(TEXT("sim_archive_bundle_id"), Envelope.SimArchiveBundleId);
	Root->SetStringField(TEXT("sim_final_report_id"), Envelope.SimFinalReportId);
	Root->SetStringField(TEXT("sim_execute_receipt_id"), Envelope.SimExecuteReceiptId);
	Root->SetStringField(TEXT("execute_ticket_id"), Envelope.ExecuteTicketId);
	Root->SetStringField(TEXT("confirm_request_id"), Envelope.ConfirmRequestId);
	Root->SetStringField(TEXT("apply_request_id"), Envelope.ApplyRequestId);
	Root->SetStringField(TEXT("review_request_id"), Envelope.ReviewRequestId);
	Root->SetStringField(TEXT("selection_digest"), Envelope.SelectionDigest);
	Root->SetStringField(TEXT("archive_digest"), Envelope.ArchiveDigest);
	Root->SetStringField(TEXT("handoff_digest"), Envelope.HandoffDigest);
	Root->SetStringField(TEXT("generated_utc"), Envelope.GeneratedUtc);
	Root->SetStringField(TEXT("execution_mode"), Envelope.ExecutionMode);
	Root->SetStringField(TEXT("handoff_target"), Envelope.HandoffTarget);
	Root->SetStringField(TEXT("transaction_mode"), Envelope.TransactionMode);
	Root->SetStringField(TEXT("termination_policy"), Envelope.TerminationPolicy);
	Root->SetStringField(TEXT("terminal_status"), Envelope.TerminalStatus);
	Root->SetStringField(TEXT("archive_status"), Envelope.ArchiveStatus);
	Root->SetStringField(TEXT("handoff_status"), Envelope.HandoffStatus);
	Root->SetBoolField(TEXT("user_confirmed"), Envelope.bUserConfirmed);
	Root->SetBoolField(TEXT("ready_to_simulate_execute"), Envelope.bReadyToSimulateExecute);
	Root->SetBoolField(TEXT("simulated_dispatch_accepted"), Envelope.bSimulatedDispatchAccepted);
	Root->SetBoolField(TEXT("simulation_completed"), Envelope.bSimulationCompleted);
	Root->SetBoolField(TEXT("archive_ready"), Envelope.bArchiveReady);
	Root->SetBoolField(TEXT("handoff_ready"), Envelope.bHandoffReady);
	Root->SetStringField(TEXT("error_code"), Envelope.ErrorCode);
	Root->SetStringField(TEXT("reason"), Envelope.Reason);

	const TSharedRef<FJsonObject> Summary = MakeShared<FJsonObject>();
	Summary->SetNumberField(TEXT("total_rows"), Envelope.Summary.TotalRows);
	Summary->SetNumberField(TEXT("selected_rows"), Envelope.Summary.SelectedRows);
	Summary->SetNumberField(TEXT("modifiable_rows"), Envelope.Summary.ModifiableRows);
	Summary->SetNumberField(TEXT("blocked_rows"), Envelope.Summary.BlockedRows);
	Summary->SetNumberField(TEXT("read_only_rows"), Envelope.Summary.ReadOnlyRows);
	Summary->SetNumberField(TEXT("write_rows"), Envelope.Summary.WriteRows);
	Summary->SetNumberField(TEXT("destructive_rows"), Envelope.Summary.DestructiveRows);
	Root->SetObjectField(TEXT("summary"), Summary);

	TArray<TSharedPtr<FJsonValue>> ItemsJson;
	ItemsJson.Reserve(Envelope.Items.Num());
	for (const FHCIAbilityKitAgentApplyRequestItem& Item : Envelope.Items)
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
