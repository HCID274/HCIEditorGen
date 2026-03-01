#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteFinalReportJsonSerializer.h"

#include "Agent/Contracts/StageF/HCIAgentApplyRequest.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteFinalReport.h"
#include "Agent/Executor/HCIDryRunDiff.h"
#include "Dom/JsonObject.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

bool FHCIAgentSimulateExecuteFinalReportJsonSerializer::SerializeToJsonString(
	const FHCIAgentSimulateExecuteFinalReport& Report,
	FString& OutJsonText)
{
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("request_id"), Report.RequestId);
	Root->SetStringField(TEXT("sim_execute_receipt_id"), Report.SimExecuteReceiptId);
	Root->SetStringField(TEXT("execute_ticket_id"), Report.ExecuteTicketId);
	Root->SetStringField(TEXT("confirm_request_id"), Report.ConfirmRequestId);
	Root->SetStringField(TEXT("apply_request_id"), Report.ApplyRequestId);
	Root->SetStringField(TEXT("review_request_id"), Report.ReviewRequestId);
	Root->SetStringField(TEXT("selection_digest"), Report.SelectionDigest);
	Root->SetStringField(TEXT("generated_utc"), Report.GeneratedUtc);
	Root->SetStringField(TEXT("execution_mode"), Report.ExecutionMode);
	Root->SetStringField(TEXT("transaction_mode"), Report.TransactionMode);
	Root->SetStringField(TEXT("termination_policy"), Report.TerminationPolicy);
	Root->SetStringField(TEXT("terminal_status"), Report.TerminalStatus);
	Root->SetBoolField(TEXT("user_confirmed"), Report.bUserConfirmed);
	Root->SetBoolField(TEXT("ready_to_simulate_execute"), Report.bReadyToSimulateExecute);
	Root->SetBoolField(TEXT("simulated_dispatch_accepted"), Report.bSimulatedDispatchAccepted);
	Root->SetBoolField(TEXT("simulation_completed"), Report.bSimulationCompleted);
	Root->SetStringField(TEXT("error_code"), Report.ErrorCode);
	Root->SetStringField(TEXT("reason"), Report.Reason);

	const TSharedRef<FJsonObject> Summary = MakeShared<FJsonObject>();
	Summary->SetNumberField(TEXT("total_rows"), Report.Summary.TotalRows);
	Summary->SetNumberField(TEXT("selected_rows"), Report.Summary.SelectedRows);
	Summary->SetNumberField(TEXT("modifiable_rows"), Report.Summary.ModifiableRows);
	Summary->SetNumberField(TEXT("blocked_rows"), Report.Summary.BlockedRows);
	Summary->SetNumberField(TEXT("read_only_rows"), Report.Summary.ReadOnlyRows);
	Summary->SetNumberField(TEXT("write_rows"), Report.Summary.WriteRows);
	Summary->SetNumberField(TEXT("destructive_rows"), Report.Summary.DestructiveRows);
	Root->SetObjectField(TEXT("summary"), Summary);

	TArray<TSharedPtr<FJsonValue>> ItemsJson;
	ItemsJson.Reserve(Report.Items.Num());
	for (const FHCIAgentApplyRequestItem& Item : Report.Items)
	{
		const TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetNumberField(TEXT("row_index"), Item.RowIndex);
		Obj->SetStringField(TEXT("tool_name"), Item.ToolName);
		Obj->SetStringField(TEXT("risk"), FHCIDryRunDiff::RiskToString(Item.Risk));
		Obj->SetStringField(TEXT("asset_path"), Item.AssetPath);
		Obj->SetStringField(TEXT("field"), Item.Field);
		Obj->SetStringField(TEXT("skip_reason"), Item.SkipReason);
		Obj->SetBoolField(TEXT("blocked"), Item.bBlocked);
		Obj->SetStringField(TEXT("object_type"), FHCIDryRunDiff::ObjectTypeToString(Item.ObjectType));
		Obj->SetStringField(TEXT("locate_strategy"), FHCIDryRunDiff::LocateStrategyToString(Item.LocateStrategy));
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
