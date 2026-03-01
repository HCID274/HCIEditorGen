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
struct FHCIAgentStageGExecuteCommitReceipt;
struct FHCIDryRunDiffReport;

class HCIRUNTIME_API FHCIAgentExecutorStageGExecuteCommitReceiptBridge
{
public:
	static bool BuildStageGExecuteCommitReceipt(
		const FHCIAgentStageGExecuteCommitRequest& StageGExecuteCommitRequest,
		const FString& ExpectedStageGExecuteCommitRequestId,
		const FHCIAgentStageGExecuteDispatchReceipt& StageGExecuteDispatchReceipt,
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
		FHCIAgentStageGExecuteCommitReceipt& OutReceipt);
};



