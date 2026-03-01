#pragma once

#include "CoreMinimal.h"

struct FHCIAgentExecutorRunResult;
struct FHCIDryRunDiffReport;

class HCIRUNTIME_API FHCIAgentExecutorDryRunBridge
{
public:
	// Convert executor step-level convergence rows into the Stage E2 Dry-Run Diff review contract.
	static bool BuildDryRunDiffReport(
		const FHCIAgentExecutorRunResult& RunResult,
		FHCIDryRunDiffReport& OutReport);
};



