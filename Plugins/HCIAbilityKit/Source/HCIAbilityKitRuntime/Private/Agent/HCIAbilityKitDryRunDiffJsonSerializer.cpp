#include "Agent/HCIAbilityKitDryRunDiffJsonSerializer.h"

#include "Agent/HCIAbilityKitDryRunDiff.h"
#include "Dom/JsonObject.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

bool FHCIAbilityKitDryRunDiffJsonSerializer::SerializeToJsonString(
	const FHCIAbilityKitDryRunDiffReport& Report,
	FString& OutJsonText)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("request_id"), Report.RequestId);

	TSharedRef<FJsonObject> SummaryObject = MakeShared<FJsonObject>();
	SummaryObject->SetNumberField(TEXT("total_candidates"), Report.Summary.TotalCandidates);
	SummaryObject->SetNumberField(TEXT("modifiable"), Report.Summary.Modifiable);
	SummaryObject->SetNumberField(TEXT("skipped"), Report.Summary.Skipped);
	Root->SetObjectField(TEXT("summary"), SummaryObject);

	TArray<TSharedPtr<FJsonValue>> DiffItemsJson;
	DiffItemsJson.Reserve(Report.DiffItems.Num());
	for (const FHCIAbilityKitDryRunDiffItem& Item : Report.DiffItems)
	{
		TSharedRef<FJsonObject> ItemObject = MakeShared<FJsonObject>();
		ItemObject->SetStringField(TEXT("asset_path"), Item.AssetPath);
		ItemObject->SetStringField(TEXT("field"), Item.Field);
		ItemObject->SetStringField(TEXT("before"), Item.Before);
		ItemObject->SetStringField(TEXT("after"), Item.After);
		ItemObject->SetStringField(TEXT("tool_name"), Item.ToolName);
		ItemObject->SetStringField(TEXT("risk"), FHCIAbilityKitDryRunDiff::RiskToString(Item.Risk));
		ItemObject->SetStringField(TEXT("skip_reason"), Item.SkipReason);
		ItemObject->SetStringField(TEXT("object_type"), FHCIAbilityKitDryRunDiff::ObjectTypeToString(Item.ObjectType));
		ItemObject->SetStringField(TEXT("locate_strategy"), FHCIAbilityKitDryRunDiff::LocateStrategyToString(Item.LocateStrategy));
		ItemObject->SetStringField(TEXT("evidence_key"), Item.EvidenceKey);
		if (!Item.ActorPath.IsEmpty())
		{
			ItemObject->SetStringField(TEXT("actor_path"), Item.ActorPath);
		}
		DiffItemsJson.Add(MakeShared<FJsonValueObject>(ItemObject));
	}
	Root->SetArrayField(TEXT("diff_items"), DiffItemsJson);

	OutJsonText.Reset();
	const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutJsonText);
	return FJsonSerializer::Serialize(Root, Writer);
}

