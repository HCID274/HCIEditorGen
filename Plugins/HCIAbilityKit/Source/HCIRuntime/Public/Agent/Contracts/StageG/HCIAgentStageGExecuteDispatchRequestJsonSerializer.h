#pragma once

#include "CoreMinimal.h"

struct FHCIAgentStageGExecuteDispatchRequest;

class HCIRUNTIME_API FHCIAgentStageGExecuteDispatchRequestJsonSerializer
{
public:
	static bool SerializeToJsonString(
		const FHCIAgentStageGExecuteDispatchRequest& Request,
		FString& OutJsonText);
};




