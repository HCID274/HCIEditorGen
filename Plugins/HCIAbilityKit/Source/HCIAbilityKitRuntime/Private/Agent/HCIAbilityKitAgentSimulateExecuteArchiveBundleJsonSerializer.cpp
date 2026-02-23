#include "Agent/HCIAbilityKitAgentSimulateExecuteArchiveBundleJsonSerializer.h"

#include "Agent/HCIAbilityKitAgentApplyRequest.h"
#include "Agent/HCIAbilityKitAgentSimulateExecuteArchiveBundle.h"
#include "Agent/HCIAbilityKitDryRunDiff.h"
#include "Dom/JsonObject.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

bool FHCIAbilityKitAgentSimulateExecuteArchiveBundleJsonSerializer::SerializeToJsonString(
	const FHCIAbilityKitAgentSimulateExecuteArchiveBundle& Bundle,
	FString& OutJsonText)
{
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("request_id"), Bundle.RequestId);
	Root->SetStringField(TEXT("sim_final_report_id"), Bundle.SimFinalReportId);
	Root->SetStringField(TEXT("sim_execute_receipt_id"), Bundle.SimExecuteReceiptId);
	Root->SetStringField(TEXT("execute_ticket_id"), Bundle.ExecuteTicketId);
	Root->SetStringField(TEXT("confirm_request_id"), Bundle.ConfirmRequestId);
	Root->SetStringField(TEXT("apply_request_id"), Bundle.ApplyRequestId);
	Root->SetStringField(TEXT("review_request_id"), Bundle.ReviewRequestId);
	Root->SetStringField(TEXT("selection_digest"), Bundle.SelectionDigest);
	Root->SetStringField(TEXT("archive_digest"), Bundle.ArchiveDigest);
	Root->SetStringField(TEXT("generated_utc"), Bundle.GeneratedUtc);
	Root->SetStringField(TEXT("execution_mode"), Bundle.ExecutionMode);
	Root->SetStringField(TEXT("transaction_mode"), Bundle.TransactionMode);
	Root->SetStringField(TEXT("termination_policy"), Bundle.TerminationPolicy);
	Root->SetStringField(TEXT("terminal_status"), Bundle.TerminalStatus);
	Root->SetStringField(TEXT("archive_status"), Bundle.ArchiveStatus);
	Root->SetBoolField(TEXT("user_confirmed"), Bundle.bUserConfirmed);
	Root->SetBoolField(TEXT("ready_to_simulate_execute"), Bundle.bReadyToSimulateExecute);
	Root->SetBoolField(TEXT("simulated_dispatch_accepted"), Bundle.bSimulatedDispatchAccepted);
	Root->SetBoolField(TEXT("simulation_completed"), Bundle.bSimulationCompleted);
	Root->SetBoolField(TEXT("archive_ready"), Bundle.bArchiveReady);
	Root->SetStringField(TEXT("error_code"), Bundle.ErrorCode);
	Root->SetStringField(TEXT("reason"), Bundle.Reason);

	const TSharedRef<FJsonObject> Summary = MakeShared<FJsonObject>();
	Summary->SetNumberField(TEXT("total_rows"), Bundle.Summary.TotalRows);
	Summary->SetNumberField(TEXT("selected_rows"), Bundle.Summary.SelectedRows);
	Summary->SetNumberField(TEXT("modifiable_rows"), Bundle.Summary.ModifiableRows);
	Summary->SetNumberField(TEXT("blocked_rows"), Bundle.Summary.BlockedRows);
	Summary->SetNumberField(TEXT("read_only_rows"), Bundle.Summary.ReadOnlyRows);
	Summary->SetNumberField(TEXT("write_rows"), Bundle.Summary.WriteRows);
	Summary->SetNumberField(TEXT("destructive_rows"), Bundle.Summary.DestructiveRows);
	Root->SetObjectField(TEXT("summary"), Summary);

	TArray<TSharedPtr<FJsonValue>> ItemsJson;
	ItemsJson.Reserve(Bundle.Items.Num());
	for (const FHCIAbilityKitAgentApplyRequestItem& Item : Bundle.Items)
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
