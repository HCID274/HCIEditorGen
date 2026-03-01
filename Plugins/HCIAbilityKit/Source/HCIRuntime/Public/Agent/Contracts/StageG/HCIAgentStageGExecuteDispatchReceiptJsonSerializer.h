#pragma once

#include "CoreMinimal.h"

struct FHCIAgentStageGExecuteDispatchReceipt;

class HCIRUNTIME_API FHCIAgentStageGExecuteDispatchReceiptJsonSerializer
{
public:
	static bool SerializeToJsonString(
		const FHCIAgentStageGExecuteDispatchReceipt& Request,
		FString& OutJsonText);
};





