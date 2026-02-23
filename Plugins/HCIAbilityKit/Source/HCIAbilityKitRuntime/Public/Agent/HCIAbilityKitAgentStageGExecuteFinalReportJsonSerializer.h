#pragma once

#include "CoreMinimal.h"

struct FHCIAbilityKitAgentStageGExecuteFinalReport;

class HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentStageGExecuteFinalReportJsonSerializer
{
public:
	static bool SerializeToJsonString(
		const FHCIAbilityKitAgentStageGExecuteFinalReport& Receipt,
		FString& OutJsonText);
};

