#pragma once

#include "CoreMinimal.h"

namespace HCIAbilityKitRedirectorFixup
{
struct FHCIAbilityKitRedirectorFixupResult
{
	FString Status;
	int32 RedirectorCount = 0;
};

FHCIAbilityKitRedirectorFixupResult FixupRedirectorReferencers(const FString& SourceAssetPath);
}

