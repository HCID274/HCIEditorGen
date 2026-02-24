#pragma once

#include "CoreMinimal.h"

struct FHCIAbilityKitAgentStageGExecuteArchiveBundle;
struct FHCIAbilityKitAgentStageGExecutionReadinessReport;
struct FHCIAbilityKitDryRunDiffReport;

class HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentExecutorStageGExecutionReadinessReportBridge
{
public:
	static bool BuildStageGExecutionReadinessReport(
		const FHCIAbilityKitAgentStageGExecuteArchiveBundle& StageGExecuteArchiveBundle,
		const FString& ExpectedStageGExecuteArchiveBundleId,
		const FHCIAbilityKitDryRunDiffReport& CurrentReviewReport,
		const bool bUserConfirmed,
		FHCIAbilityKitAgentStageGExecutionReadinessReport& OutReport);
};

