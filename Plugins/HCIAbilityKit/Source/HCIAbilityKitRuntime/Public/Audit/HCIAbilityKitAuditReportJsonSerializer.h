#pragma once

#include "CoreMinimal.h"

struct FHCIAbilityKitAuditReport;

class HCIABILITYKITRUNTIME_API FHCIAbilityKitAuditReportJsonSerializer
{
public:
	static bool SerializeToJsonString(const FHCIAbilityKitAuditReport& Report, FString& OutJsonText);
};

