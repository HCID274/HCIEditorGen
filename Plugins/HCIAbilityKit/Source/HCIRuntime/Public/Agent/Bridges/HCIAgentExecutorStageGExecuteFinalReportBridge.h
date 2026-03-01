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
struct FHCIAgentStageGExecuteCommitReceipt;
struct FHCIAgentStageGExecuteFinalReport;
struct FHCIDryRunDiffReport;

class HCIRUNTIME_API FHCIAgentExecutorStageGExecuteFinalReportBridge
{
public:
	static bool BuildStageGExecuteFinalReport(
		const FHCIAgentStageGExecuteCommitReceipt& StageGExecuteCommitReceipt,
		const FString& ExpectedStageGExecuteCommitReceiptId,
		const FString& ExpectedStageGExecuteCommitRequestId,
		const FString& ExpectedStageGExecuteCommitRequestDigest,
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
		FHCIAgentStageGExecuteFinalReport& OutReceipt);
};


