#pragma once

#include "CoreMinimal.h"

struct FHCIAgentStageGExecuteCommitRequest;

class HCIRUNTIME_API FHCIAgentStageGExecuteCommitRequestJsonSerializer
{
public:
	static bool SerializeToJsonString(
		const FHCIAgentStageGExecuteCommitRequest& Request,
		FString& OutJsonText);
};






