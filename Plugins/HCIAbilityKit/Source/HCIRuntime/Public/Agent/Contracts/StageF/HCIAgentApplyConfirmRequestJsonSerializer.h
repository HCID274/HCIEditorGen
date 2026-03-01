#pragma once

#include "CoreMinimal.h"

struct FHCIAgentApplyConfirmRequest;

class HCIRUNTIME_API FHCIAgentApplyConfirmRequestJsonSerializer
{
public:
	static bool SerializeToJsonString(const FHCIAgentApplyConfirmRequest& Request, FString& OutJsonText);
};



