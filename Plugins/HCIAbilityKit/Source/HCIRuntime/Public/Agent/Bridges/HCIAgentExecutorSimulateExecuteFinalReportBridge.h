#pragma once

#include "CoreMinimal.h"

struct FHCIAgentApplyConfirmRequest;
struct FHCIAgentApplyRequest;
struct FHCIAgentExecuteTicket;
struct FHCIAgentSimulateExecuteFinalReport;
struct FHCIAgentSimulateExecuteReceipt;
struct FHCIDryRunDiffReport;

class HCIRUNTIME_API FHCIAgentExecutorSimulateExecuteFinalReportBridge
{
public:
	static bool BuildSimulateExecuteFinalReport(
		const FHCIAgentSimulateExecuteReceipt& SimExecuteReceipt,
		const FHCIAgentExecuteTicket& CurrentExecuteTicket,
		const FHCIAgentApplyConfirmRequest& CurrentConfirmRequest,
		const FHCIAgentApplyRequest& CurrentApplyRequest,
		const FHCIDryRunDiffReport& CurrentReviewReport,
		FHCIAgentSimulateExecuteFinalReport& OutReport);
};


