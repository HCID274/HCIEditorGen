#pragma once

#include "CoreMinimal.h"

struct FHCIAgentApplyConfirmRequest;
struct FHCIAgentApplyRequest;
struct FHCIAgentExecuteTicket;
struct FHCIAgentSimulateExecuteReceipt;
struct FHCIDryRunDiffReport;

class HCIRUNTIME_API FHCIAgentExecutorSimulateExecuteReceiptBridge
{
public:
	static bool BuildSimulateExecuteReceipt(
		const FHCIAgentExecuteTicket& ExecuteTicket,
		const FHCIAgentApplyConfirmRequest& CurrentConfirmRequest,
		const FHCIAgentApplyRequest& CurrentApplyRequest,
		const FHCIDryRunDiffReport& CurrentReviewReport,
		FHCIAgentSimulateExecuteReceipt& OutReceipt);
};


