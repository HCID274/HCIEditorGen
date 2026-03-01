#pragma once

#include "CoreMinimal.h"

struct FHCIAgentApplyRequest;

class HCIRUNTIME_API FHCIAgentApplyRequestJsonSerializer
{
public:
	static bool SerializeToJsonString(const FHCIAgentApplyRequest& Request, FString& OutJsonText);
};



