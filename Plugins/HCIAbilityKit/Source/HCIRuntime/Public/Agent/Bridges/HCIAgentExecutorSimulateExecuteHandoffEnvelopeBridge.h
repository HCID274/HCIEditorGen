#pragma once

#include "CoreMinimal.h"

struct FHCIAgentApplyConfirmRequest;
struct FHCIAgentApplyRequest;
struct FHCIAgentExecuteTicket;
struct FHCIAgentSimulateExecuteArchiveBundle;
struct FHCIAgentSimulateExecuteHandoffEnvelope;
struct FHCIAgentSimulateExecuteFinalReport;
struct FHCIAgentSimulateExecuteReceipt;
struct FHCIDryRunDiffReport;

class HCIRUNTIME_API FHCIAgentExecutorSimulateExecuteHandoffEnvelopeBridge
{
public:
	static bool BuildSimulateExecuteHandoffEnvelope(
		const FHCIAgentSimulateExecuteArchiveBundle& SimArchiveBundle,
		const FHCIAgentSimulateExecuteFinalReport& SimFinalReport,
		const FHCIAgentSimulateExecuteReceipt& SimExecuteReceipt,
		const FHCIAgentExecuteTicket& CurrentExecuteTicket,
		const FHCIAgentApplyConfirmRequest& CurrentConfirmRequest,
		const FHCIAgentApplyRequest& CurrentApplyRequest,
		const FHCIDryRunDiffReport& CurrentReviewReport,
		FHCIAgentSimulateExecuteHandoffEnvelope& OutBundle);
};



