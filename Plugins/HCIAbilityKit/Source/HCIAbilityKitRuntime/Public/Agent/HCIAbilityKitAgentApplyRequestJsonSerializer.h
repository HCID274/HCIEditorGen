#pragma once

#include "CoreMinimal.h"

struct FHCIAbilityKitAgentApplyRequest;

class HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentApplyRequestJsonSerializer
{
public:
	static bool SerializeToJsonString(const FHCIAbilityKitAgentApplyRequest& Request, FString& OutJsonText);
};

