#include "AgentActions/Support/HCIAbilityKitToolActionParamParser.h"

#include "Dom/JsonObject.h"

FHCIAbilityKitToolActionParamParser::FHCIAbilityKitToolActionParamParser(const TSharedPtr<FJsonObject>& InArgs)
	: Args(InArgs)
{
}

bool FHCIAbilityKitToolActionParamParser::TryGetRequiredString(const TCHAR* Field, FString& OutValue) const
{
	OutValue.Reset();
	if (!Args.IsValid())
	{
		return false;
	}
	if (!Args->TryGetStringField(Field, OutValue))
	{
		return false;
	}
	OutValue.TrimStartAndEndInline();
	return !OutValue.IsEmpty();
}

bool FHCIAbilityKitToolActionParamParser::TryGetRequiredInt(const TCHAR* Field, int32& OutValue) const
{
	if (!Args.IsValid())
	{
		return false;
	}
	return Args->TryGetNumberField(Field, OutValue);
}

bool FHCIAbilityKitToolActionParamParser::TryGetRequiredStringArray(const TCHAR* Field, TArray<FString>& OutValue) const
{
	OutValue.Reset();
	if (!Args.IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* JsonArray = nullptr;
	if (!Args->TryGetArrayField(Field, JsonArray))
	{
		return false;
	}

	for (const TSharedPtr<FJsonValue>& Val : *JsonArray)
	{
		if (Val.IsValid() && Val->Type == EJson::String)
		{
			OutValue.Add(Val->AsString());
		}
	}
	return OutValue.Num() > 0;
}

bool FHCIAbilityKitToolActionParamParser::TryGetOptionalStringArray(const TCHAR* Field, TArray<FString>& OutValue) const
{
	OutValue.Reset();
	if (!Args.IsValid() || !Args->HasField(Field))
	{
		return true;
	}

	const TArray<TSharedPtr<FJsonValue>>* JsonArray = nullptr;
	if (!Args->TryGetArrayField(Field, JsonArray))
	{
		return false;
	}

	for (const TSharedPtr<FJsonValue>& Val : *JsonArray)
	{
		if (!Val.IsValid() || Val->Type != EJson::String)
		{
			return false;
		}
		FString Item = Val->AsString();
		Item.TrimStartAndEndInline();
		if (!Item.IsEmpty())
		{
			OutValue.Add(MoveTemp(Item));
		}
	}
	return true;
}

bool FHCIAbilityKitToolActionParamParser::TryGetOptionalStringFieldRaw(const TCHAR* Field, FString& OutValue) const
{
	OutValue.Reset();
	if (!Args.IsValid() || !Args->HasField(Field))
	{
		return false;
	}
	return Args->TryGetStringField(Field, OutValue);
}

