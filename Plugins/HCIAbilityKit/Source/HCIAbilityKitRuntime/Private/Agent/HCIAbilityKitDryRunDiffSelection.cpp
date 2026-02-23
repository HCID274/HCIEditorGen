#include "Agent/HCIAbilityKitDryRunDiffSelection.h"

#include "Containers/Set.h"

bool FHCIAbilityKitDryRunDiffSelection::SelectRows(
	const FHCIAbilityKitDryRunDiffReport& SourceReport,
	const TArray<int32>& RequestedRowIndices,
	FHCIAbilityKitDryRunDiffSelectionResult& OutResult)
{
	OutResult = FHCIAbilityKitDryRunDiffSelectionResult();
	OutResult.InputRowCount = RequestedRowIndices.Num();
	OutResult.TotalRowsBefore = SourceReport.DiffItems.Num();
	OutResult.SelectedReport.RequestId = SourceReport.RequestId;

	if (RequestedRowIndices.Num() == 0)
	{
		OutResult.ErrorCode = TEXT("E4201");
		OutResult.Reason = TEXT("empty_row_selection");
		return false;
	}

	TSet<int32> SeenRows;
	OutResult.AppliedRowIndices.Reserve(RequestedRowIndices.Num());
	OutResult.SelectedReport.DiffItems.Reserve(RequestedRowIndices.Num());

	for (const int32 RowIndex : RequestedRowIndices)
	{
		if (RowIndex < 0 || RowIndex >= SourceReport.DiffItems.Num())
		{
			OutResult.ErrorCode = TEXT("E4201");
			OutResult.Reason = TEXT("row_index_out_of_range");
			return false;
		}

		if (SeenRows.Contains(RowIndex))
		{
			++OutResult.DroppedDuplicateRows;
			continue;
		}

		SeenRows.Add(RowIndex);
		OutResult.AppliedRowIndices.Add(RowIndex);
		OutResult.SelectedReport.DiffItems.Add(SourceReport.DiffItems[RowIndex]);
	}

	OutResult.UniqueRowCount = OutResult.AppliedRowIndices.Num();
	OutResult.TotalRowsAfter = OutResult.SelectedReport.DiffItems.Num();

	if (OutResult.UniqueRowCount == 0)
	{
		OutResult.ErrorCode = TEXT("E4201");
		OutResult.Reason = TEXT("empty_row_selection_after_dedup");
		return false;
	}

	FHCIAbilityKitDryRunDiff::NormalizeAndFinalize(OutResult.SelectedReport);
	OutResult.bSuccess = true;
	OutResult.Reason = TEXT("ok");
	return true;
}

