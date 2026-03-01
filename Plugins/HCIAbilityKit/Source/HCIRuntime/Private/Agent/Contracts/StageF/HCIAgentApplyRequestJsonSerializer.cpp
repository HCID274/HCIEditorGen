#include "Agent/Contracts/StageF/HCIAgentApplyRequestJsonSerializer.h"

#include "Agent/Contracts/StageF/HCIAgentApplyRequest.h"
#include "Agent/Executor/HCIDryRunDiff.h"
#include "Dom/JsonObject.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

bool FHCIAgentApplyRequestJsonSerializer::SerializeToJsonString(
	const FHCIAgentApplyRequest& Request,
	FString& OutJsonText)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("request_id"), Request.RequestId);
	Root->SetStringField(TEXT("review_request_id"), Request.ReviewRequestId);
	Root->SetStringField(TEXT("selection_digest"), Request.SelectionDigest);
	Root->SetStringField(TEXT("generated_utc"), Request.GeneratedUtc);
	Root->SetStringField(TEXT("execution_mode"), Request.ExecutionMode);
	Root->SetBoolField(TEXT("ready"), Request.bReady);

	TSharedRef<FJsonObject> SummaryObject = MakeShared<FJsonObject>();
	SummaryObject->SetNumberField(TEXT("total_rows"), Request.Summary.TotalRows);
	SummaryObject->SetNumberField(TEXT("selected_rows"), Request.Summary.SelectedRows);
	SummaryObject->SetNumberField(TEXT("modifiable_rows"), Request.Summary.ModifiableRows);
	SummaryObject->SetNumberField(TEXT("blocked_rows"), Request.Summary.BlockedRows);
	SummaryObject->SetNumberField(TEXT("read_only_rows"), Request.Summary.ReadOnlyRows);
	SummaryObject->SetNumberField(TEXT("write_rows"), Request.Summary.WriteRows);
	SummaryObject->SetNumberField(TEXT("destructive_rows"), Request.Summary.DestructiveRows);
	Root->SetObjectField(TEXT("summary"), SummaryObject);

	TArray<TSharedPtr<FJsonValue>> ItemsJson;
	ItemsJson.Reserve(Request.Items.Num());
	for (const FHCIAgentApplyRequestItem& Item : Request.Items)
	{
		TSharedRef<FJsonObject> ItemObject = MakeShared<FJsonObject>();
		ItemObject->SetNumberField(TEXT("row_index"), Item.RowIndex);
		ItemObject->SetStringField(TEXT("tool_name"), Item.ToolName);
		ItemObject->SetStringField(TEXT("risk"), FHCIDryRunDiff::RiskToString(Item.Risk));
		ItemObject->SetStringField(TEXT("asset_path"), Item.AssetPath);
		ItemObject->SetStringField(TEXT("field"), Item.Field);
		ItemObject->SetStringField(TEXT("skip_reason"), Item.SkipReason);
		ItemObject->SetBoolField(TEXT("blocked"), Item.bBlocked);
		ItemObject->SetStringField(TEXT("object_type"), FHCIDryRunDiff::ObjectTypeToString(Item.ObjectType));
		ItemObject->SetStringField(TEXT("locate_strategy"), FHCIDryRunDiff::LocateStrategyToString(Item.LocateStrategy));
		ItemObject->SetStringField(TEXT("evidence_key"), Item.EvidenceKey);
		if (!Item.ActorPath.IsEmpty())
		{
			ItemObject->SetStringField(TEXT("actor_path"), Item.ActorPath);
		}
		ItemsJson.Add(MakeShared<FJsonValueObject>(ItemObject));
	}
	Root->SetArrayField(TEXT("items"), ItemsJson);

	OutJsonText.Reset();
	const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutJsonText);
	return FJsonSerializer::Serialize(Root, Writer);
}
