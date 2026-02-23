#pragma once

#include "CoreMinimal.h"

struct FHCIAbilityKitAgentExecutorRunResult;
struct FHCIAbilityKitDryRunDiffReport;

class HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentExecutorDryRunBridge
{
public:
	// Convert executor step-level convergence rows into the Stage E2 Dry-Run Diff review contract.
	static bool BuildDryRunDiffReport(
		const FHCIAbilityKitAgentExecutorRunResult& RunResult,
		FHCIAbilityKitDryRunDiffReport& OutReport);
};

