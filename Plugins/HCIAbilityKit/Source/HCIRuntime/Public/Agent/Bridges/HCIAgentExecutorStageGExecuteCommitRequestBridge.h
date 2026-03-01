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
struct FHCIAgentStageGExecuteCommitRequest;
struct FHCIDryRunDiffReport;

class HCIRUNTIME_API FHCIAgentExecutorStageGExecuteCommitRequestBridge
{
public:
	static bool BuildStageGExecuteCommitRequest(
		const FHCIAgentStageGExecuteDispatchReceipt& StageGExecuteDispatchReceipt,
		const FString& ExpectedStageGExecuteDispatchReceiptId,
		const FHCIAgentStageGExecuteDispatchRequest& StageGExecuteDispatchRequest,
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
		bool bExecuteCommitConfirmed,
		FHCIAgentStageGExecuteCommitRequest& OutRequest);
};






