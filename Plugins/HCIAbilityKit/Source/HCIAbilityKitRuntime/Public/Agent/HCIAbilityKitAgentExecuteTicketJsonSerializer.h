#pragma once

#include "CoreMinimal.h"

struct FHCIAbilityKitAgentExecuteTicket;

class HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentExecuteTicketJsonSerializer
{
public:
	static bool SerializeToJsonString(
		const FHCIAbilityKitAgentExecuteTicket& Request,
		FString& OutJsonText);
};

