#pragma once

#include "CoreMinimal.h"

struct FHCIAgentSimulateExecuteArchiveBundle;

class HCIRUNTIME_API FHCIAgentSimulateExecuteArchiveBundleJsonSerializer
{
public:
	static bool SerializeToJsonString(
		const FHCIAgentSimulateExecuteArchiveBundle& Bundle,
		FString& OutJsonText);
};


