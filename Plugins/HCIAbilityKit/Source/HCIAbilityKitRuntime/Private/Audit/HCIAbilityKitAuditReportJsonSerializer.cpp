#include "Audit/HCIAbilityKitAuditReportJsonSerializer.h"

#include "Audit/HCIAbilityKitAuditReport.h"
#include "Common/HCIAbilityKitTimeFormat.h"
#include "Dom/JsonObject.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

bool FHCIAbilityKitAuditReportJsonSerializer::SerializeToJsonString(const FHCIAbilityKitAuditReport& Report, FString& OutJsonText)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("run_id"), Report.RunId);
	Root->SetStringField(TEXT("generated_utc"), FHCIAbilityKitTimeFormat::FormatUtcAsBeijingIso8601(Report.GeneratedUtc));
	Root->SetStringField(TEXT("source"), Report.Source);

	TArray<TSharedPtr<FJsonValue>> ResultValues;
	ResultValues.Reserve(Report.Results.Num());
	for (const FHCIAbilityKitAuditResultEntry& Result : Report.Results)
	{
		TSharedRef<FJsonObject> ResultObject = MakeShared<FJsonObject>();
		ResultObject->SetStringField(TEXT("asset_path"), Result.AssetPath);
		ResultObject->SetStringField(TEXT("asset_name"), Result.AssetName);
		ResultObject->SetStringField(TEXT("asset_class"), Result.AssetClass);
		ResultObject->SetStringField(TEXT("rule_id"), Result.RuleId);
		ResultObject->SetStringField(TEXT("severity"), Result.SeverityText);
		ResultObject->SetStringField(TEXT("reason"), Result.Reason);
		ResultObject->SetStringField(TEXT("hint"), Result.Hint);
		ResultObject->SetStringField(TEXT("triangle_source"), Result.TriangleSource);
		ResultObject->SetStringField(TEXT("scan_state"), Result.ScanState);
		if (!Result.SkipReason.IsEmpty())
		{
			ResultObject->SetStringField(TEXT("skip_reason"), Result.SkipReason);
		}

		TSharedRef<FJsonObject> EvidenceObject = MakeShared<FJsonObject>();
		for (const TPair<FString, FString>& Pair : Result.Evidence)
		{
			EvidenceObject->SetStringField(Pair.Key, Pair.Value);
		}
		ResultObject->SetObjectField(TEXT("evidence"), EvidenceObject);

		ResultValues.Emplace(MakeShared<FJsonValueObject>(ResultObject));
	}
	Root->SetArrayField(TEXT("results"), ResultValues);

	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutJsonText);
	return FJsonSerializer::Serialize(Root, Writer);
}
