#pragma once

#include "CoreMinimal.h"

namespace HCIRedirectorFixup
{
struct FHCIRedirectorFixupResult
{
	FString Status;
	int32 RedirectorCount = 0;
};

FHCIRedirectorFixupResult FixupRedirectorReferencers(const FString& SourceAssetPath);
}


