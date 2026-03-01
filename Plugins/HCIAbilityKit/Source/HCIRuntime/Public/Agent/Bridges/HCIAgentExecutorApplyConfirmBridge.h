#pragma once

#include "CoreMinimal.h"

struct FHCIAgentApplyConfirmRequest;
struct FHCIAgentApplyRequest;
struct FHCIDryRunDiffReport;

class HCIRUNTIME_API FHCIAgentExecutorApplyConfirmBridge
{
public:
	static bool BuildConfirmRequest(
		const FHCIAgentApplyRequest& ApplyRequest,
		const FHCIDryRunDiffReport& CurrentReviewReport,
		bool bUserConfirmed,
		FHCIAgentApplyConfirmRequest& OutConfirmRequest);
};



