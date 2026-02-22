#pragma once

#include "CoreMinimal.h"

struct FHCIAbilityKitDryRunDiffReport;

class HCIABILITYKITRUNTIME_API FHCIAbilityKitDryRunDiffJsonSerializer
{
public:
	static bool SerializeToJsonString(const FHCIAbilityKitDryRunDiffReport& Report, FString& OutJsonText);
};

