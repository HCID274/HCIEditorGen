#pragma once

#include "CoreMinimal.h"

struct FHCIAgentStageGWriteEnableRequest;

class HCIRUNTIME_API FHCIAgentStageGWriteEnableRequestJsonSerializer
{
public:
	static bool SerializeToJsonString(
		const FHCIAgentStageGWriteEnableRequest& Request,
		FString& OutJsonText);
};




