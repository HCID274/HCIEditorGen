#pragma once

#include "CoreMinimal.h"

struct FHCIAbilityKitAgentApplyConfirmRequest;
struct FHCIAbilityKitAgentApplyRequest;
struct FHCIAbilityKitDryRunDiffReport;

class HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentExecutorApplyConfirmBridge
{
public:
	static bool BuildConfirmRequest(
		const FHCIAbilityKitAgentApplyRequest& ApplyRequest,
		const FHCIAbilityKitDryRunDiffReport& CurrentReviewReport,
		bool bUserConfirmed,
		FHCIAbilityKitAgentApplyConfirmRequest& OutConfirmRequest);
};

