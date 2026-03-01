#pragma once

#include "CoreMinimal.h"

struct FHCIAgentStageGExecutePermitTicket;

class HCIRUNTIME_API FHCIAgentStageGExecutePermitTicketJsonSerializer
{
public:
	static bool SerializeToJsonString(
		const FHCIAgentStageGExecutePermitTicket& Request,
		FString& OutJsonText);
};




