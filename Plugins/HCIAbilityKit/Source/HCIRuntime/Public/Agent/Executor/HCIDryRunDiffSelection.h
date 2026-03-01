#pragma once

#include "CoreMinimal.h"

#include "Agent/Executor/HCIDryRunDiff.h"

struct HCIRUNTIME_API FHCIDryRunDiffSelectionResult
{
	bool bSuccess = false;
	FString ErrorCode;
	FString Reason;

	int32 InputRowCount = 0;
	int32 UniqueRowCount = 0;
	int32 DroppedDuplicateRows = 0;
	int32 TotalRowsBefore = 0;
	int32 TotalRowsAfter = 0;

	TArray<int32> AppliedRowIndices;
	FHCIDryRunDiffReport SelectedReport;
};

class HCIRUNTIME_API FHCIDryRunDiffSelection
{
public:
	static bool SelectRows(
		const FHCIDryRunDiffReport& SourceReport,
		const TArray<int32>& RequestedRowIndices,
		FHCIDryRunDiffSelectionResult& OutResult);
};


