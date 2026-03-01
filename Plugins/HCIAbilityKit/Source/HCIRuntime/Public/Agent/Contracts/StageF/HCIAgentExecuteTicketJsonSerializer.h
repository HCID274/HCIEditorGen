#pragma once

#include "CoreMinimal.h"

struct FHCIAgentExecuteTicket;

class HCIRUNTIME_API FHCIAgentExecuteTicketJsonSerializer
{
public:
	static bool SerializeToJsonString(
		const FHCIAgentExecuteTicket& Request,
		FString& OutJsonText);
};



