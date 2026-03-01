#pragma once

#include "CoreMinimal.h"

struct FHCIAuditReport;

class HCIRUNTIME_API FHCIAuditReportJsonSerializer
{
public:
	static bool SerializeToJsonString(const FHCIAuditReport& Report, FString& OutJsonText);
};



