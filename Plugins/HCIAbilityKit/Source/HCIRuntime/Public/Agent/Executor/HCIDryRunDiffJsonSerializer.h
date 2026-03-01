#pragma once

#include "CoreMinimal.h"

struct FHCIDryRunDiffReport;

class HCIRUNTIME_API FHCIDryRunDiffJsonSerializer
{
public:
	static bool SerializeToJsonString(const FHCIDryRunDiffReport& Report, FString& OutJsonText);
};



