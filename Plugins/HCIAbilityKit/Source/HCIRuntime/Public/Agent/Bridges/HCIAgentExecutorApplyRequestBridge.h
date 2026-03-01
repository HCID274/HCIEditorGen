#pragma once

#include "CoreMinimal.h"

struct FHCIAgentApplyRequest;
struct FHCIDryRunDiffReport;

class HCIRUNTIME_API FHCIAgentExecutorApplyRequestBridge
{
public:
	// Build an apply-request package from the latest selected ExecutorReview dry-run diff.
	static bool BuildApplyRequest(
		const FHCIDryRunDiffReport& SelectedReviewReport,
		FHCIAgentApplyRequest& OutApplyRequest);
};



