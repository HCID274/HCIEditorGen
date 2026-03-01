#pragma once

#include "CoreMinimal.h"

struct FHCIAgentStageGExecuteFinalReport;

class HCIRUNTIME_API FHCIAgentStageGExecuteFinalReportJsonSerializer
{
public:
	static bool SerializeToJsonString(
		const FHCIAgentStageGExecuteFinalReport& Receipt,
		FString& OutJsonText);
};



