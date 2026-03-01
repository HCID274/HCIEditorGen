#pragma once

#include "CoreMinimal.h"

struct FHCIAgentSimulateExecuteReceipt;

class HCIRUNTIME_API FHCIAgentSimulateExecuteReceiptJsonSerializer
{
public:
	static bool SerializeToJsonString(
		const FHCIAgentSimulateExecuteReceipt& Request,
		FString& OutJsonText);
};


