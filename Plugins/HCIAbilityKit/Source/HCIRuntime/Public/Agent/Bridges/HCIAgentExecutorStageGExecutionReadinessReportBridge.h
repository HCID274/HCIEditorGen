#pragma once

#include "CoreMinimal.h"

struct FHCIAgentStageGExecuteArchiveBundle;
struct FHCIAgentStageGExecutionReadinessReport;
struct FHCIDryRunDiffReport;

class HCIRUNTIME_API FHCIAgentExecutorStageGExecutionReadinessReportBridge
{
public:
	static bool BuildStageGExecutionReadinessReport(
		const FHCIAgentStageGExecuteArchiveBundle& StageGExecuteArchiveBundle,
		const FString& ExpectedStageGExecuteArchiveBundleId,
		const FHCIDryRunDiffReport& CurrentReviewReport,
		const bool bUserConfirmed,
		FHCIAgentStageGExecutionReadinessReport& OutReport);
};



