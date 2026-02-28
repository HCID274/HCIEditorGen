#pragma once

#include "CoreMinimal.h"

class UObject;
struct FAssetData;

namespace HCIAbilityKitAssetNamingRules
{
FString SanitizeIdentifier(const FString& InText);
FString RemoveKnownPrefixToken(const FString& InName);
FString DeriveClassPrefix(const UObject* Asset);
FString DeriveClassPrefixFromAssetData(const FAssetData& AssetData);
}

