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
struct FHCIAgentStageGExecuteFinalReport;
struct FHCIAgentStageGExecuteArchiveBundle;
struct FHCIDryRunDiffReport;

class HCIRUNTIME_API FHCIAgentExecutorStageGExecuteArchiveBundleBridge
{
public:
	static bool BuildStageGExecuteArchiveBundle(
		const FHCIAgentStageGExecuteFinalReport& StageGExecuteFinalReport,
		const FString& ExpectedStageGExecuteFinalReportId,
		const FString& ExpectedStageGExecuteCommitReceiptId,
		const FString& ExpectedStageGExecuteCommitReceiptDigest,
		const FHCIAgentStageGExecuteCommitReceipt& StageGExecuteCommitReceipt,
		const FHCIAgentStageGExecuteCommitRequest& StageGExecuteCommitRequest,
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
		FHCIAgentStageGExecuteArchiveBundle& OutReceipt);
};


