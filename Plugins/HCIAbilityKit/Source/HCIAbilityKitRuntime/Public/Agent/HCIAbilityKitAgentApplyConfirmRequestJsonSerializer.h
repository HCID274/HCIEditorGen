#pragma once

#include "CoreMinimal.h"

struct FHCIAbilityKitAgentApplyConfirmRequest;

class HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentApplyConfirmRequestJsonSerializer
{
public:
	static bool SerializeToJsonString(const FHCIAbilityKitAgentApplyConfirmRequest& Request, FString& OutJsonText);
};

