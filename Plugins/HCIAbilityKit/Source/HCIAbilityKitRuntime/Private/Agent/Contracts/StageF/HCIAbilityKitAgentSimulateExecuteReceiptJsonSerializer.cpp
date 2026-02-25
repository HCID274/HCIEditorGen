#include "Agent/Contracts/StageF/HCIAbilityKitAgentSimulateExecuteReceiptJsonSerializer.h"

#include "Agent/Contracts/StageF/HCIAbilityKitAgentApplyRequest.h"
#include "Agent/Contracts/StageF/HCIAbilityKitAgentSimulateExecuteReceipt.h"
#include "Agent/Executor/HCIAbilityKitDryRunDiff.h"
#include "Dom/JsonObject.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

bool FHCIAbilityKitAgentSimulateExecuteReceiptJsonSerializer::SerializeToJsonString(
	const FHCIAbilityKitAgentSimulateExecuteReceipt& Request,
	FString& OutJsonText)
{
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("request_id"), Request.RequestId);
	Root->SetStringField(TEXT("execute_ticket_id"), Request.ExecuteTicketId);
	Root->SetStringField(TEXT("confirm_request_id"), Request.ConfirmRequestId);
	Root->SetStringField(TEXT("apply_request_id"), Request.ApplyRequestId);
	Root->SetStringField(TEXT("review_request_id"), Request.ReviewRequestId);
	Root->SetStringField(TEXT("selection_digest"), Request.SelectionDigest);
	Root->SetStringField(TEXT("generated_utc"), Request.GeneratedUtc);
	Root->SetStringField(TEXT("execution_mode"), Request.ExecutionMode);
	Root->SetStringField(TEXT("transaction_mode"), Request.TransactionMode);
	Root->SetStringField(TEXT("termination_policy"), Request.TerminationPolicy);
	Root->SetBoolField(TEXT("user_confirmed"), Request.bUserConfirmed);
	Root->SetBoolField(TEXT("ready_to_simulate_execute"), Request.bReadyToSimulateExecute);
	Root->SetBoolField(TEXT("simulated_dispatch_accepted"), Request.bSimulatedDispatchAccepted);
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