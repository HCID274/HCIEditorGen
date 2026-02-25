#pragma once

#include "CoreMinimal.h"

struct FHCIAbilityKitAgentSimulateExecuteReceipt;

class HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentSimulateExecuteReceiptJsonSerializer
{
public:
	static bool SerializeToJsonString(
		const FHCIAbilityKitAgentSimulateExecuteReceipt& Request,
		FString& OutJsonText);
};
