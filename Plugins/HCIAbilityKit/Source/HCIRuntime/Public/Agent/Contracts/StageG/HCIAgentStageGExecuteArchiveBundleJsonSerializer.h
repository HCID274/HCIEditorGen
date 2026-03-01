#pragma once

#include "CoreMinimal.h"

struct FHCIAgentStageGExecuteArchiveBundle;

class HCIRUNTIME_API FHCIAgentStageGExecuteArchiveBundleJsonSerializer
{
public:
	static bool SerializeToJsonString(
		const FHCIAgentStageGExecuteArchiveBundle& Receipt,
		FString& OutJsonText);
};



