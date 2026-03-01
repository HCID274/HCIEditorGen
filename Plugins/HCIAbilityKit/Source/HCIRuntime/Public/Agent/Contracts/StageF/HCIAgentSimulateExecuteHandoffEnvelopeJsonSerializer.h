#pragma once

#include "CoreMinimal.h"

struct FHCIAgentSimulateExecuteHandoffEnvelope;

class HCIRUNTIME_API FHCIAgentSimulateExecuteHandoffEnvelopeJsonSerializer
{
public:
	static bool SerializeToJsonString(
		const FHCIAgentSimulateExecuteHandoffEnvelope& Bundle,
		FString& OutJsonText);
};



