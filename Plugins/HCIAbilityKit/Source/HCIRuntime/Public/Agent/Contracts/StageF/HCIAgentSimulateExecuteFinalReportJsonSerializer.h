#pragma once

#include "CoreMinimal.h"

struct FHCIAgentSimulateExecuteFinalReport;

class HCIRUNTIME_API FHCIAgentSimulateExecuteFinalReportJsonSerializer
{
public:
	static bool SerializeToJsonString(
		const FHCIAgentSimulateExecuteFinalReport& Report,
		FString& OutJsonText);
};


