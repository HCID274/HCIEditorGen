#pragma once

#include "CoreMinimal.h"

struct FHCIAbilityKitAgentApplyRequest;
struct FHCIAbilityKitDryRunDiffReport;

class HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentExecutorApplyRequestBridge
{
public:
	// Build an apply-request package from the latest selected ExecutorReview dry-run diff.
	static bool BuildApplyRequest(
		const FHCIAbilityKitDryRunDiffReport& SelectedReviewReport,
		FHCIAbilityKitAgentApplyRequest& OutApplyRequest);
};

