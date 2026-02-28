#pragma once

#include "CoreMinimal.h"

// Legacy snapshot (Global Step 2 refactor): kept for reference only; do not include from production ToolActions.

#include "AgentActions/Support/HCIAbilityKitAssetNamingRules.h"
#include "AgentActions/Support/HCIAbilityKitAssetPathUtils.h"

#include "AssetRegistry/AssetData.h"
#include "Dom/JsonObject.h"

namespace
{
static bool HCI_TrySplitObjectPath(
	const FString& ObjectPath,
	FString& OutPackagePath,
	FString& OutAssetName)
{
	return HCIAbilityKitAssetPathUtils::TrySplitObjectPath(ObjectPath, OutPackagePath, OutAssetName);
}

static FString HCI_ToObjectPath(const FString& PackagePath, const FString& AssetName)
{
	return HCIAbilityKitAssetPathUtils::ToObjectPath(PackagePath, AssetName);
}

static void HCI_NormalizeAssetPathVariants(
	const FString& InPath,
	FString& OutAssetPath,
	FString& OutObjectPath)
{
	HCIAbilityKitAssetPathUtils::NormalizeAssetPathVariants(InPath, OutAssetPath, OutObjectPath);
}

static FString HCI_GetDirectoryFromPackagePath(const FString& PackagePath)
{
	return HCIAbilityKitAssetPathUtils::GetDirectoryFromPackagePath(PackagePath);
}

static FString HCI_TrimTrailingSlash(const FString& InPath)
{
	return HCIAbilityKitAssetPathUtils::TrimTrailingSlash(InPath);
}

static FString HCI_SanitizeIdentifier(const FString& InText)
{
	return HCIAbilityKitAssetNamingRules::SanitizeIdentifier(InText);
}

static FString HCI_RemoveKnownPrefixToken(const FString& InName)
{
	return HCIAbilityKitAssetNamingRules::RemoveKnownPrefixToken(InName);
}

static FString HCI_DeriveClassPrefix(const UObject* Asset)
{
	return HCIAbilityKitAssetNamingRules::DeriveClassPrefix(Asset);
}

static FString HCI_DeriveClassPrefixFromAssetData(const FAssetData& AssetData)
{
	return HCIAbilityKitAssetNamingRules::DeriveClassPrefixFromAssetData(AssetData);
}

static bool HCI_TryReadRequiredStringArg(
	const TSharedPtr<FJsonObject>& Args,
	const TCHAR* Field,
	FString& OutValue)
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

static bool HCI_TryReadRequiredStringArrayArg(
	const TSharedPtr<FJsonObject>& Args,
	const TCHAR* Field,
	TArray<FString>& OutValue)
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

static bool HCI_TryReadOptionalStringArrayArg(
	const TSharedPtr<FJsonObject>& Args,
	const TCHAR* Field,
	TArray<FString>& OutValue)
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

static bool HCI_TryReadRequiredIntArg(
	const TSharedPtr<FJsonObject>& Args,
	const TCHAR* Field,
	int32& OutValue)
{
	if (!Args.IsValid())
	{
		return false;
	}
	return Args->TryGetNumberField(Field, OutValue);
}
}
