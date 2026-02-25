#pragma once

#include "CoreMinimal.h"

struct FHCIAbilityKitAgentStageGExecutionReadinessReport;

class HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentStageGExecutionReadinessReportJsonSerializer
{
public:
	static bool SerializeToJsonString(
		const FHCIAbilityKitAgentStageGExecutionReadinessReport& Report,
		FString& OutJsonText);
};

