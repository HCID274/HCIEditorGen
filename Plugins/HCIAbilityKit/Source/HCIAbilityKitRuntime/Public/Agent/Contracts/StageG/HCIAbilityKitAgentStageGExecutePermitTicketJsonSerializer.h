#pragma once

#include "CoreMinimal.h"

struct FHCIAbilityKitAgentStageGExecutePermitTicket;

class HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentStageGExecutePermitTicketJsonSerializer
{
public:
	static bool SerializeToJsonString(
		const FHCIAbilityKitAgentStageGExecutePermitTicket& Request,
		FString& OutJsonText);
};


