#pragma once

#include "CoreMinimal.h"

#include "Agent/Executor/HCIAbilityKitDryRunDiff.h"

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitDryRunDiffSelectionResult
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
	FHCIAbilityKitDryRunDiffReport SelectedReport;
};

class HCIABILITYKITRUNTIME_API FHCIAbilityKitDryRunDiffSelection
{
public:
	static bool SelectRows(
		const FHCIAbilityKitDryRunDiffReport& SourceReport,
		const TArray<int32>& RequestedRowIndices,
		FHCIAbilityKitDryRunDiffSelectionResult& OutResult);
};
