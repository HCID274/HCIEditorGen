#pragma once

#include "CoreMinimal.h"

struct FHCIAbilityKitAgentSimulateExecuteFinalReport;

class HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentSimulateExecuteFinalReportJsonSerializer
{
public:
	static bool SerializeToJsonString(
		const FHCIAbilityKitAgentSimulateExecuteFinalReport& Report,
		FString& OutJsonText);
};
