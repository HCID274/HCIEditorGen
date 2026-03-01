#pragma once

#include "CoreMinimal.h"

struct FHCIAgentApplyConfirmRequest;
struct FHCIAgentApplyRequest;
struct FHCIAgentExecuteTicket;
struct FHCIAgentSimulateExecuteHandoffEnvelope;
struct FHCIAgentSimulateExecuteArchiveBundle;
struct FHCIAgentSimulateExecuteFinalReport;
struct FHCIAgentSimulateExecuteReceipt;
struct FHCIAgentStageGExecuteIntent;
struct FHCIAgentStageGWriteEnableRequest;
struct FHCIAgentStageGExecutePermitTicket;
struct FHCIAgentStageGExecuteDispatchRequest;
struct FHCIAgentStageGExecuteDispatchReceipt;
struct FHCIDryRunDiffReport;

class HCIRUNTIME_API FHCIAgentExecutorStageGExecuteDispatchReceiptBridge
{
public:
	static bool BuildStageGExecuteDispatchReceipt(
		const FHCIAgentStageGExecuteDispatchRequest& StageGExecuteDispatchRequest,
		const FString& ExpectedStageGExecuteDispatchRequestId,
		const FHCIAgentStageGExecutePermitTicket& StageGExecutePermitTicket,
		const FHCIAgentStageGWriteEnableRequest& StageGWriteEnableRequest,
		const FHCIAgentStageGExecuteIntent& StageGExecuteIntent,
		const FHCIAgentSimulateExecuteHandoffEnvelope& SimHandoffEnvelope,
		const FHCIAgentSimulateExecuteArchiveBundle& SimArchiveBundle,
		const FHCIAgentSimulateExecuteFinalReport& SimFinalReport,
		const FHCIAgentSimulateExecuteReceipt& SimExecuteReceipt,
		const FHCIAgentExecuteTicket& CurrentExecuteTicket,
		const FHCIAgentApplyConfirmRequest& CurrentConfirmRequest,
		const FHCIAgentApplyRequest& CurrentApplyRequest,
		const FHCIDryRunDiffReport& CurrentReviewReport,
		FHCIAgentStageGExecuteDispatchReceipt& OutRequest);
};





