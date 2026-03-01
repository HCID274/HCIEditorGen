#pragma once

#include "CoreMinimal.h"

struct FHCIAgentStageGExecuteCommitReceipt;

class HCIRUNTIME_API FHCIAgentStageGExecuteCommitReceiptJsonSerializer
{
public:
	static bool SerializeToJsonString(
		const FHCIAgentStageGExecuteCommitReceipt& Receipt,
		FString& OutJsonText);
};



