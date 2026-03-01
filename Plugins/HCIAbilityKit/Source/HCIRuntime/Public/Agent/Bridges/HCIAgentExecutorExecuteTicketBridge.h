#pragma once

#include "CoreMinimal.h"

struct FHCIAgentApplyConfirmRequest;
struct FHCIAgentApplyRequest;
struct FHCIAgentExecuteTicket;
struct FHCIDryRunDiffReport;

class HCIRUNTIME_API FHCIAgentExecutorExecuteTicketBridge
{
public:
	static bool BuildExecuteTicket(
		const FHCIAgentApplyConfirmRequest& ConfirmRequest,
		const FHCIAgentApplyRequest& CurrentApplyRequest,
		const FHCIDryRunDiffReport& CurrentReviewReport,
		FHCIAgentExecuteTicket& OutExecuteTicket);
};



