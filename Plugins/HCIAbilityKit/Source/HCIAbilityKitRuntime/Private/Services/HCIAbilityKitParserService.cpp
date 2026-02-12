#include "Services/HCIAbilityKitParserService.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

bool FHCIAbilityKitParserService::TryParseKitFile(const FString& FullFilename, FHCIAbilityKitParsedData& OutParsed, FString& OutError)
{
	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *FullFilename))
	{
		OutError = FString::Printf(TEXT("Failed to load file: %s"), *FullFilename);
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FileContent);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError = TEXT("Invalid JSON format");
		return false;
	}

	int32 SchemaVersionTmp;
	if (!RootObject->TryGetNumberField(TEXT("schema_version"), SchemaVersionTmp))
	{
		OutError = TEXT("Missing or invalid field: schema_version (must be an integer)");
		return false;
	}
	OutParsed.SchemaVersion = SchemaVersionTmp;

	FString IdTmp;
	if (!RootObject->TryGetStringField(TEXT("id"), IdTmp) || IdTmp.IsEmpty())
	{
		OutError = TEXT("Missing or invalid field: id (must be a non-empty string)");
		return false;
	}
	OutParsed.Id = IdTmp;

	FString DisplayNameTmp;
	if (!RootObject->TryGetStringField(TEXT("display_name"), DisplayNameTmp))
	{
		OutError = TEXT("Missing or invalid field: display_name (must be a string)");
		return false;
	}
	OutParsed.DisplayName = DisplayNameTmp;

	const TSharedPtr<FJsonObject>* ParamsObj = nullptr;
	if (!RootObject->TryGetObjectField(TEXT("params"), ParamsObj) || !ParamsObj || !ParamsObj->IsValid())
	{
		OutError = TEXT("Missing or invalid object: params");
		return false;
	}

	double DamageTmp;
	if (!(*ParamsObj)->TryGetNumberField(TEXT("damage"), DamageTmp))
	{
		OutError = TEXT("Missing or invalid field in params: damage (must be a number)");
		return false;
	}
	OutParsed.Damage = static_cast<float>(DamageTmp);
	return true;
}
