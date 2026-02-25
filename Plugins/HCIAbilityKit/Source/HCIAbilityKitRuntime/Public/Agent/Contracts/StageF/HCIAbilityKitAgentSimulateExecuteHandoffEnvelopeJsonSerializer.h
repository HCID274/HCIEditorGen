#pragma once

#include "CoreMinimal.h"

struct FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope;

class HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentSimulateExecuteHandoffEnvelopeJsonSerializer
{
public:
	static bool SerializeToJsonString(
		const FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope& Bundle,
		FString& OutJsonText);
};

