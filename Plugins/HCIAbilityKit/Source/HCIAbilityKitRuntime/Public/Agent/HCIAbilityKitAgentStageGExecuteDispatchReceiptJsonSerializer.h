#pragma once

#include "CoreMinimal.h"

struct FHCIAbilityKitAgentStageGExecuteDispatchReceipt;

class HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentStageGExecuteDispatchReceiptJsonSerializer
{
public:
	static bool SerializeToJsonString(
		const FHCIAbilityKitAgentStageGExecuteDispatchReceipt& Request,
		FString& OutJsonText);
};



