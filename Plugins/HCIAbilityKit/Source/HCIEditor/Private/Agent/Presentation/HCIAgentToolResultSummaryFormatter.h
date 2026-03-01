#pragma once

#include "CoreMinimal.h"

struct FHCIAgentExecutorStepResult;

namespace HCIAgentToolResultSummaryFormatter
{
struct FHCIToolResultSummaryOptions
{
	int32 MaxLines = 6;
	int32 MaxItemsPerList = 3;
	int32 MaxCharsPerLine = 220;
};

// Build user-facing, deterministic summary lines from executor step results.
// This is presentation-only formatting: it must not touch tool implementations, UI widgets, or network.
void BuildSummaryLines(
	const TArray<FHCIAgentExecutorStepResult>& StepResults,
	const FHCIToolResultSummaryOptions& Options,
	TArray<FString>& OutLines);
}


