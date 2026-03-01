#pragma once

#include "CoreMinimal.h"
#include "Agent/Contracts/StageF/HCIAgentApplyConfirmRequest.h"
#include "Agent/Contracts/StageF/HCIAgentApplyRequest.h"
#include "Agent/Contracts/StageF/HCIAgentExecuteTicket.h"
#include "Agent/Planner/HCIAgentPlan.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteArchiveBundle.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteFinalReport.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteHandoffEnvelope.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteReceipt.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecuteArchiveBundle.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecuteCommitReceipt.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecuteCommitRequest.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecuteDispatchReceipt.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecuteDispatchRequest.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecuteFinalReport.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecuteIntent.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecutePermitTicket.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecutionReadinessReport.h"
#include "Agent/Contracts/StageG/HCIAgentStageGWriteEnableRequest.h"
#include "Agent/Executor/HCIDryRunDiff.h"
#include "Interfaces/IHttpRequest.h"
#include "Templates/Atomic.h"

struct FHCIAgentDemoState
{
	FHCIAgentPlan AgentPlanPreviewState;
	FHCIDryRunDiffReport AgentExecutorReviewDiffPreviewState;
	FHCIAgentApplyRequest AgentApplyRequestPreviewState;
	FHCIAgentApplyConfirmRequest AgentApplyConfirmRequestPreviewState;
	FHCIAgentExecuteTicket AgentExecuteTicketPreviewState;
	FHCIAgentSimulateExecuteReceipt AgentSimulateExecuteReceiptPreviewState;
	FHCIAgentSimulateExecuteFinalReport AgentSimulateExecuteFinalReportPreviewState;
	FHCIAgentSimulateExecuteArchiveBundle AgentSimulateExecuteArchiveBundlePreviewState;
	FHCIAgentSimulateExecuteHandoffEnvelope AgentSimulateExecuteHandoffEnvelopePreviewState;
	FHCIAgentStageGExecuteIntent AgentStageGExecuteIntentPreviewState;
	FHCIAgentStageGWriteEnableRequest AgentStageGWriteEnableRequestPreviewState;
	FHCIAgentStageGExecutePermitTicket AgentStageGExecutePermitTicketPreviewState;
	FHCIAgentStageGExecuteDispatchRequest AgentStageGExecuteDispatchRequestPreviewState;
	FHCIAgentStageGExecuteDispatchReceipt AgentStageGExecuteDispatchReceiptPreviewState;
	FHCIAgentStageGExecuteCommitRequest AgentStageGExecuteCommitRequestPreviewState;
	FHCIAgentStageGExecuteCommitReceipt AgentStageGExecuteCommitReceiptPreviewState;
	FHCIAgentStageGExecuteFinalReport AgentStageGExecuteFinalReportPreviewState;
	FHCIAgentStageGExecuteArchiveBundle AgentStageGExecuteArchiveBundlePreviewState;
	FHCIAgentStageGExecutionReadinessReport AgentStageGExecutionReadinessReportPreviewState;
	TAtomic<bool> bRealLlmPlanCommandInFlight{false};
	TArray<TSharedPtr<IHttpRequest, ESPMode::ThreadSafe>> RealLlmProbeRequests;

	void ResetPreviewStates();
};

FHCIAgentDemoState& HCI_GetAgentDemoState();
