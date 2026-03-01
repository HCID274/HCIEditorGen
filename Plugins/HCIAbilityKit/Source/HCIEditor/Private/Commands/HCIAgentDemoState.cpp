#include "Commands/HCIAgentDemoState.h"

void FHCIAgentDemoState::ResetPreviewStates()
{
	AgentPlanPreviewState = FHCIAgentPlan();
	AgentExecutorReviewDiffPreviewState = FHCIDryRunDiffReport();
	AgentApplyRequestPreviewState = FHCIAgentApplyRequest();
	AgentApplyConfirmRequestPreviewState = FHCIAgentApplyConfirmRequest();
	AgentExecuteTicketPreviewState = FHCIAgentExecuteTicket();
	AgentSimulateExecuteReceiptPreviewState = FHCIAgentSimulateExecuteReceipt();
	AgentSimulateExecuteFinalReportPreviewState = FHCIAgentSimulateExecuteFinalReport();
	AgentSimulateExecuteArchiveBundlePreviewState = FHCIAgentSimulateExecuteArchiveBundle();
	AgentSimulateExecuteHandoffEnvelopePreviewState = FHCIAgentSimulateExecuteHandoffEnvelope();
	AgentStageGExecuteIntentPreviewState = FHCIAgentStageGExecuteIntent();
	AgentStageGWriteEnableRequestPreviewState = FHCIAgentStageGWriteEnableRequest();
	AgentStageGExecutePermitTicketPreviewState = FHCIAgentStageGExecutePermitTicket();
	AgentStageGExecuteDispatchRequestPreviewState = FHCIAgentStageGExecuteDispatchRequest();
	AgentStageGExecuteDispatchReceiptPreviewState = FHCIAgentStageGExecuteDispatchReceipt();
	AgentStageGExecuteCommitRequestPreviewState = FHCIAgentStageGExecuteCommitRequest();
	AgentStageGExecuteCommitReceiptPreviewState = FHCIAgentStageGExecuteCommitReceipt();
	AgentStageGExecuteFinalReportPreviewState = FHCIAgentStageGExecuteFinalReport();
	AgentStageGExecuteArchiveBundlePreviewState = FHCIAgentStageGExecuteArchiveBundle();
	AgentStageGExecutionReadinessReportPreviewState = FHCIAgentStageGExecutionReadinessReport();
}

FHCIAgentDemoState& HCI_GetAgentDemoState()
{
	static FHCIAgentDemoState State;
	return State;
}

