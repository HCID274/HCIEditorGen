#pragma once

#include "CoreMinimal.h"

struct FHCIAgentStageGExecuteIntent;

class HCIRUNTIME_API FHCIAgentStageGExecuteIntentJsonSerializer
{
public:
	static bool SerializeToJsonString(
		const FHCIAgentStageGExecuteIntent& Intent,
		FString& OutJsonText);
};




