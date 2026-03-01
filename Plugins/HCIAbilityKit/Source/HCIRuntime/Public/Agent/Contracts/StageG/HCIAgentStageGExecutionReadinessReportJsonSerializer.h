#pragma once

#include "CoreMinimal.h"

struct FHCIAgentStageGExecutionReadinessReport;

class HCIRUNTIME_API FHCIAgentStageGExecutionReadinessReportJsonSerializer
{
public:
	static bool SerializeToJsonString(
		const FHCIAgentStageGExecutionReadinessReport& Report,
		FString& OutJsonText);
};



