#include "Commands/HCIAbilityKitAgentDemoConsoleCommands.h"

#include "Commands/HCIAbilityKitAgentCommandHandlers.h"

void FHCIAbilityKitAgentDemoConsoleCommands::StartupReviewCommands()
{
	if (!AgentExecutePlanReviewDemoCommand.IsValid())
	{
		AgentExecutePlanReviewDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewDemo"),
			TEXT("F6 Executor -> Dry-Run Diff review bridge demo. Usage: HCIAbilityKit.AgentExecutePlanReviewDemo [ok_naming|ok_level_risk|fail_confirm|fail_lod]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewDemoCommand));
	}

	if (!AgentExecutePlanReviewJsonCommand.IsValid())
	{
		AgentExecutePlanReviewJsonCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewJson"),
			TEXT("F6 Executor -> Dry-Run Diff review bridge JSON preview. Usage: HCIAbilityKit.AgentExecutePlanReviewJson [ok_naming|ok_level_risk|fail_confirm|fail_lod]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewJsonCommand));
	}

	if (!AgentExecutePlanReviewLocateCommand.IsValid())
	{
		AgentExecutePlanReviewLocateCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewLocate"),
			TEXT("F7 Locate a row from the latest ExecutorReview dry-run diff preview. Usage: HCIAbilityKit.AgentExecutePlanReviewLocate [row_index]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewLocateCommand));
	}

	if (!AgentExecutePlanReviewSelectCommand.IsValid())
	{
		AgentExecutePlanReviewSelectCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewSelect"),
			TEXT("F8 Select review rows (dedupe/filter) from latest ExecutorReview preview. Usage: HCIAbilityKit.AgentExecutePlanReviewSelect [row_indices_csv]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewSelectCommand));
	}

	if (!AgentExecutePlanReviewSelectJsonCommand.IsValid())
	{
		AgentExecutePlanReviewSelectJsonCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewSelectJson"),
			TEXT("F8 Print selected review rows as JSON. Usage: HCIAbilityKit.AgentExecutePlanReviewSelectJson [row_indices_csv]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewSelectJsonCommand));
	}

	if (!AgentExecutePlanReviewPrepareApplyCommand.IsValid())
	{
		AgentExecutePlanReviewPrepareApplyCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewPrepareApply"),
			TEXT("F9 Validate selected review subset and build ApplyRequest (still dry-run only). Usage: HCIAbilityKit.AgentExecutePlanReviewPrepareApply [tamper=none|digest|review]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewPrepareApplyCommand));
	}

	if (!AgentExecutePlanReviewPrepareApplyJsonCommand.IsValid())
	{
		AgentExecutePlanReviewPrepareApplyJsonCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewPrepareApplyJson"),
			TEXT("F9 Build ApplyRequest and print JSON. Usage: HCIAbilityKit.AgentExecutePlanReviewPrepareApplyJson [tamper=none|digest|review]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewPrepareApplyJsonCommand));
	}

	if (!AgentExecutePlanReviewPrepareConfirmCommand.IsValid())
	{
		AgentExecutePlanReviewPrepareConfirmCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewPrepareConfirm"),
			TEXT("F10 Validate latest ApplyRequest and build ConfirmRequest. Usage: HCIAbilityKit.AgentExecutePlanReviewPrepareConfirm [user_confirmed=0|1] [tamper=none|digest|apply|review]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewPrepareConfirmCommand));
	}

	if (!AgentExecutePlanReviewPrepareConfirmJsonCommand.IsValid())
	{
		AgentExecutePlanReviewPrepareConfirmJsonCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewPrepareConfirmJson"),
			TEXT("F10 Build ConfirmRequest and print JSON. Usage: HCIAbilityKit.AgentExecutePlanReviewPrepareConfirmJson [user_confirmed=0|1] [tamper=none|digest|apply|review]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewPrepareConfirmJsonCommand));
	}

	if (!AgentExecutePlanReviewPrepareExecuteTicketCommand.IsValid())
	{
		AgentExecutePlanReviewPrepareExecuteTicketCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewPrepareExecuteTicket"),
			TEXT("F11 Validate latest ConfirmRequest against latest Apply/Review preview and build ExecuteTicket. Usage: HCIAbilityKit.AgentExecutePlanReviewPrepareExecuteTicket [tamper=none|digest|apply|review]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewPrepareExecuteTicketCommand));
	}

	if (!AgentExecutePlanReviewPrepareExecuteTicketJsonCommand.IsValid())
	{
		AgentExecutePlanReviewPrepareExecuteTicketJsonCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewPrepareExecuteTicketJson"),
			TEXT("F11 Build ExecuteTicket and print JSON. Usage: HCIAbilityKit.AgentExecutePlanReviewPrepareExecuteTicketJson [tamper=none|digest|apply|review]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewPrepareExecuteTicketJsonCommand));
	}

	if (!AgentExecutePlanReviewPrepareSimExecuteReceiptCommand.IsValid())
	{
		AgentExecutePlanReviewPrepareSimExecuteReceiptCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewPrepareSimExecuteReceipt"),
			TEXT("F12 Validate latest ExecuteTicket against latest Confirm/Apply/Review preview and build SimExecuteReceipt. Usage: HCIAbilityKit.AgentExecutePlanReviewPrepareSimExecuteReceipt [tamper=none|digest|apply|review|confirm|ready]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewPrepareSimExecuteReceiptCommand));
	}

	if (!AgentExecutePlanReviewPrepareSimExecuteReceiptJsonCommand.IsValid())
	{
		AgentExecutePlanReviewPrepareSimExecuteReceiptJsonCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewPrepareSimExecuteReceiptJson"),
			TEXT("F12 Build SimExecuteReceipt and print JSON. Usage: HCIAbilityKit.AgentExecutePlanReviewPrepareSimExecuteReceiptJson [tamper=none|digest|apply|review|confirm|ready]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewPrepareSimExecuteReceiptJsonCommand));
	}

	if (!AgentExecutePlanReviewPrepareSimFinalReportCommand.IsValid())
	{
		AgentExecutePlanReviewPrepareSimFinalReportCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewPrepareSimFinalReport"),
			TEXT("F13 Validate latest SimExecuteReceipt against latest ExecuteTicket/Confirm/Apply/Review preview and build final simulation report. Usage: HCIAbilityKit.AgentExecutePlanReviewPrepareSimFinalReport [tamper=none|digest|apply|review|confirm|receipt|ready]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewPrepareSimFinalReportCommand));
	}

	if (!AgentExecutePlanReviewPrepareSimFinalReportJsonCommand.IsValid())
	{
		AgentExecutePlanReviewPrepareSimFinalReportJsonCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewPrepareSimFinalReportJson"),
			TEXT("F13 Build final simulation report and print JSON. Usage: HCIAbilityKit.AgentExecutePlanReviewPrepareSimFinalReportJson [tamper=none|digest|apply|review|confirm|receipt|ready]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewPrepareSimFinalReportJsonCommand));
	}

	if (!AgentExecutePlanReviewPrepareSimArchiveBundleCommand.IsValid())
	{
		AgentExecutePlanReviewPrepareSimArchiveBundleCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewPrepareSimArchiveBundle"),
			TEXT("F14 Validate latest SimFinalReport against latest SimExecuteReceipt/ExecuteTicket/Confirm/Apply/Review preview and build final simulation archive bundle. Usage: HCIAbilityKit.AgentExecutePlanReviewPrepareSimArchiveBundle [tamper=none|digest|apply|review|confirm|receipt|final|ready]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewPrepareSimArchiveBundleCommand));
	}

	if (!AgentExecutePlanReviewPrepareSimArchiveBundleJsonCommand.IsValid())
	{
		AgentExecutePlanReviewPrepareSimArchiveBundleJsonCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewPrepareSimArchiveBundleJson"),
			TEXT("F14 Build final simulation archive bundle and print JSON. Usage: HCIAbilityKit.AgentExecutePlanReviewPrepareSimArchiveBundleJson [tamper=none|digest|apply|review|confirm|receipt|final|ready]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewPrepareSimArchiveBundleJsonCommand));
	}

	if (!AgentExecutePlanReviewPrepareSimHandoffEnvelopeCommand.IsValid())
	{
		AgentExecutePlanReviewPrepareSimHandoffEnvelopeCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewPrepareSimHandoffEnvelope"),
			TEXT("F15 Validate latest SimArchiveBundle against latest SimFinalReport/Receipt/ExecuteTicket/Confirm/Apply/Review preview and build Stage-G handoff envelope. Usage: HCIAbilityKit.AgentExecutePlanReviewPrepareSimHandoffEnvelope [tamper=none|digest|apply|review|confirm|receipt|final|archive|ready]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewPrepareSimHandoffEnvelopeCommand));
	}

	if (!AgentExecutePlanReviewPrepareSimHandoffEnvelopeJsonCommand.IsValid())
	{
		AgentExecutePlanReviewPrepareSimHandoffEnvelopeJsonCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewPrepareSimHandoffEnvelopeJson"),
			TEXT("F15 Build Stage-G handoff envelope and print JSON. Usage: HCIAbilityKit.AgentExecutePlanReviewPrepareSimHandoffEnvelopeJson [tamper=none|digest|apply|review|confirm|receipt|final|archive|ready]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewPrepareSimHandoffEnvelopeJsonCommand));
	}

	if (!AgentExecutePlanReviewPrepareStageGExecuteIntentCommand.IsValid())
	{
		AgentExecutePlanReviewPrepareStageGExecuteIntentCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteIntent"),
			TEXT("G1 Validate latest SimHandoffEnvelope and build Stage-G execute intent (validate-only)."),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewPrepareStageGExecuteIntentCommand));
	}

	if (!AgentExecutePlanReviewPrepareStageGExecuteIntentJsonCommand.IsValid())
	{
		AgentExecutePlanReviewPrepareStageGExecuteIntentJsonCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteIntentJson"),
			TEXT("G1 Build Stage-G execute intent (validate-only) and print JSON."),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewPrepareStageGExecuteIntentJsonCommand));
	}

	if (!AgentExecutePlanReviewPrepareStageGWriteEnableRequestCommand.IsValid())
	{
		AgentExecutePlanReviewPrepareStageGWriteEnableRequestCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewPrepareStageGWriteEnableRequest"),
			TEXT("G2 Validate latest StageGExecuteIntent and build Stage-G write-enable request."),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewPrepareStageGWriteEnableRequestCommand));
	}

	if (!AgentExecutePlanReviewPrepareStageGWriteEnableRequestJsonCommand.IsValid())
	{
		AgentExecutePlanReviewPrepareStageGWriteEnableRequestJsonCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewPrepareStageGWriteEnableRequestJson"),
			TEXT("G2 Build Stage-G write-enable request and print JSON."),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewPrepareStageGWriteEnableRequestJsonCommand));
	}

	if (!AgentExecutePlanReviewPrepareStageGExecutePermitTicketCommand.IsValid())
	{
		AgentExecutePlanReviewPrepareStageGExecutePermitTicketCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecutePermitTicket"),
			TEXT("G3 Validate latest StageGWriteEnableRequest and build Stage-G execute permit ticket."),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewPrepareStageGExecutePermitTicketCommand));
	}

	if (!AgentExecutePlanReviewPrepareStageGExecutePermitTicketJsonCommand.IsValid())
	{
		AgentExecutePlanReviewPrepareStageGExecutePermitTicketJsonCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecutePermitTicketJson"),
			TEXT("G3 Build Stage-G execute permit ticket and print JSON."),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewPrepareStageGExecutePermitTicketJsonCommand));
	}

	if (!AgentExecutePlanReviewPrepareStageGExecuteDispatchRequestCommand.IsValid())
	{
		AgentExecutePlanReviewPrepareStageGExecuteDispatchRequestCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteDispatchRequest"),
			TEXT("G4 Validate latest StageGExecutePermitTicket and build Stage-G execute dispatch request."),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewPrepareStageGExecuteDispatchRequestCommand));
	}

	if (!AgentExecutePlanReviewPrepareStageGExecuteDispatchRequestJsonCommand.IsValid())
	{
		AgentExecutePlanReviewPrepareStageGExecuteDispatchRequestJsonCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteDispatchRequestJson"),
			TEXT("G4 Build Stage-G execute dispatch request and print JSON."),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewPrepareStageGExecuteDispatchRequestJsonCommand));
	}

	if (!AgentExecutePlanReviewPrepareStageGExecuteDispatchReceiptCommand.IsValid())
	{
		AgentExecutePlanReviewPrepareStageGExecuteDispatchReceiptCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteDispatchReceipt"),
			TEXT("G5 Validate latest StageGExecuteDispatchRequest and build Stage-G execute dispatch receipt."),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewPrepareStageGExecuteDispatchReceiptCommand));
	}

	if (!AgentExecutePlanReviewPrepareStageGExecuteDispatchReceiptJsonCommand.IsValid())
	{
		AgentExecutePlanReviewPrepareStageGExecuteDispatchReceiptJsonCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteDispatchReceiptJson"),
			TEXT("G5 Build Stage-G execute dispatch receipt and print JSON."),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewPrepareStageGExecuteDispatchReceiptJsonCommand));
	}

	if (!AgentExecutePlanReviewPrepareStageGExecuteCommitRequestCommand.IsValid())
	{
		AgentExecutePlanReviewPrepareStageGExecuteCommitRequestCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteCommitRequest"),
			TEXT("G6 Validate latest StageGExecuteDispatchReceipt and build Stage-G execute commit request."),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewPrepareStageGExecuteCommitRequestCommand));
	}

	if (!AgentExecutePlanReviewPrepareStageGExecuteCommitRequestJsonCommand.IsValid())
	{
		AgentExecutePlanReviewPrepareStageGExecuteCommitRequestJsonCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteCommitRequestJson"),
			TEXT("G6 Build Stage-G execute commit request and print JSON."),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewPrepareStageGExecuteCommitRequestJsonCommand));
	}

	if (!AgentExecutePlanReviewPrepareStageGExecuteCommitReceiptCommand.IsValid())
	{
		AgentExecutePlanReviewPrepareStageGExecuteCommitReceiptCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteCommitReceipt"),
			TEXT("G7 Validate latest StageGExecuteCommitRequest and build Stage-G execute commit receipt."),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewPrepareStageGExecuteCommitReceiptCommand));
	}

	if (!AgentExecutePlanReviewPrepareStageGExecuteCommitReceiptJsonCommand.IsValid())
	{
		AgentExecutePlanReviewPrepareStageGExecuteCommitReceiptJsonCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteCommitReceiptJson"),
			TEXT("G7 Build Stage-G execute commit receipt and print JSON."),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewPrepareStageGExecuteCommitReceiptJsonCommand));
	}

	if (!AgentExecutePlanReviewPrepareStageGExecuteFinalReportCommand.IsValid())
	{
		AgentExecutePlanReviewPrepareStageGExecuteFinalReportCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteFinalReport"),
			TEXT("G8 Validate latest StageGExecuteCommitReceipt and build Stage-G execute final report."),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewPrepareStageGExecuteFinalReportCommand));
	}

	if (!AgentExecutePlanReviewPrepareStageGExecuteFinalReportJsonCommand.IsValid())
	{
		AgentExecutePlanReviewPrepareStageGExecuteFinalReportJsonCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteFinalReportJson"),
			TEXT("G8 Build Stage-G execute final report and print JSON."),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewPrepareStageGExecuteFinalReportJsonCommand));
	}

	if (!AgentExecutePlanReviewPrepareStageGExecuteArchiveBundleCommand.IsValid())
	{
		AgentExecutePlanReviewPrepareStageGExecuteArchiveBundleCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteArchiveBundle"),
			TEXT("G9 Validate latest StageGExecuteFinalReport and build Stage-G execute archive bundle."),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewPrepareStageGExecuteArchiveBundleCommand));
	}

	if (!AgentExecutePlanReviewPrepareStageGExecuteArchiveBundleJsonCommand.IsValid())
	{
		AgentExecutePlanReviewPrepareStageGExecuteArchiveBundleJsonCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteArchiveBundleJson"),
			TEXT("G9 Build Stage-G execute archive bundle and print JSON."),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewPrepareStageGExecuteArchiveBundleJsonCommand));
	}

	if (!AgentExecutePlanReviewPrepareStageGExecutionReadinessCommand.IsValid())
	{
		AgentExecutePlanReviewPrepareStageGExecutionReadinessCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecutionReadiness"),
			TEXT("G10 Validate latest StageGExecuteArchiveBundle and build Stage-G execution readiness report."),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewPrepareStageGExecutionReadinessCommand));
	}

	if (!AgentExecutePlanReviewPrepareStageGExecutionReadinessJsonCommand.IsValid())
	{
		AgentExecutePlanReviewPrepareStageGExecutionReadinessJsonCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecutionReadinessJson"),
			TEXT("G10 Build Stage-G execution readiness report and print JSON."),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewPrepareStageGExecutionReadinessJsonCommand));
	}
}

void FHCIAbilityKitAgentDemoConsoleCommands::ShutdownReviewCommands()
{
	AgentExecutePlanReviewPrepareStageGExecutionReadinessJsonCommand.Reset();
	AgentExecutePlanReviewPrepareStageGExecutionReadinessCommand.Reset();
	AgentExecutePlanReviewPrepareStageGExecuteArchiveBundleJsonCommand.Reset();
	AgentExecutePlanReviewPrepareStageGExecuteArchiveBundleCommand.Reset();
	AgentExecutePlanReviewPrepareStageGExecuteFinalReportJsonCommand.Reset();
	AgentExecutePlanReviewPrepareStageGExecuteFinalReportCommand.Reset();
	AgentExecutePlanReviewPrepareStageGExecuteCommitReceiptJsonCommand.Reset();
	AgentExecutePlanReviewPrepareStageGExecuteCommitReceiptCommand.Reset();
	AgentExecutePlanReviewPrepareStageGExecuteCommitRequestJsonCommand.Reset();
	AgentExecutePlanReviewPrepareStageGExecuteCommitRequestCommand.Reset();
	AgentExecutePlanReviewPrepareStageGExecuteDispatchReceiptJsonCommand.Reset();
	AgentExecutePlanReviewPrepareStageGExecuteDispatchReceiptCommand.Reset();
	AgentExecutePlanReviewPrepareStageGExecuteDispatchRequestJsonCommand.Reset();
	AgentExecutePlanReviewPrepareStageGExecuteDispatchRequestCommand.Reset();
	AgentExecutePlanReviewPrepareStageGExecutePermitTicketJsonCommand.Reset();
	AgentExecutePlanReviewPrepareStageGExecutePermitTicketCommand.Reset();
	AgentExecutePlanReviewPrepareStageGWriteEnableRequestJsonCommand.Reset();
	AgentExecutePlanReviewPrepareStageGWriteEnableRequestCommand.Reset();
	AgentExecutePlanReviewPrepareStageGExecuteIntentJsonCommand.Reset();
	AgentExecutePlanReviewPrepareStageGExecuteIntentCommand.Reset();
	AgentExecutePlanReviewPrepareSimHandoffEnvelopeJsonCommand.Reset();
	AgentExecutePlanReviewPrepareSimHandoffEnvelopeCommand.Reset();
	AgentExecutePlanReviewPrepareSimArchiveBundleJsonCommand.Reset();
	AgentExecutePlanReviewPrepareSimArchiveBundleCommand.Reset();
	AgentExecutePlanReviewPrepareSimFinalReportJsonCommand.Reset();
	AgentExecutePlanReviewPrepareSimFinalReportCommand.Reset();
	AgentExecutePlanReviewPrepareSimExecuteReceiptJsonCommand.Reset();
	AgentExecutePlanReviewPrepareSimExecuteReceiptCommand.Reset();
	AgentExecutePlanReviewPrepareExecuteTicketJsonCommand.Reset();
	AgentExecutePlanReviewPrepareExecuteTicketCommand.Reset();
	AgentExecutePlanReviewPrepareConfirmJsonCommand.Reset();
	AgentExecutePlanReviewPrepareConfirmCommand.Reset();
	AgentExecutePlanReviewPrepareApplyJsonCommand.Reset();
	AgentExecutePlanReviewPrepareApplyCommand.Reset();
	AgentExecutePlanReviewSelectJsonCommand.Reset();
	AgentExecutePlanReviewSelectCommand.Reset();
	AgentExecutePlanReviewLocateCommand.Reset();
	AgentExecutePlanReviewJsonCommand.Reset();
	AgentExecutePlanReviewDemoCommand.Reset();
}
