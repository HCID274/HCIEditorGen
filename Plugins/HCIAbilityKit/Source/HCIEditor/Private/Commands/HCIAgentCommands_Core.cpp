#include "Commands/HCIAgentDemoConsoleCommands.h"

#include "Commands/HCIAgentCommandHandlers.h"

void FHCIAgentDemoConsoleCommands::StartupCoreCommands()
{
	if (!AgentConfirmGateDemoCommand.IsValid())
	{
		AgentConfirmGateDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCI.AgentConfirmGateDemo"),
			TEXT("E3 confirm-gate demo. Usage: HCI.AgentConfirmGateDemo [tool_name] [requires_confirm 0|1] [user_confirmed 0|1]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentConfirmGateDemoCommand));
	}

	if (!AgentBlastRadiusDemoCommand.IsValid())
	{
		AgentBlastRadiusDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCI.AgentBlastRadiusDemo"),
			TEXT("E4 blast-radius demo. Usage: HCI.AgentBlastRadiusDemo [tool_name] [target_modify_count>=0]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentBlastRadiusDemoCommand));
	}

	if (!AgentTransactionDemoCommand.IsValid())
	{
		AgentTransactionDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCI.AgentTransactionDemo"),
			TEXT("E5 all-or-nothing transaction demo. Usage: HCI.AgentTransactionDemo [total_steps>=1] [fail_step_index>=0]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentTransactionDemoCommand));
	}

	if (!AgentSourceControlDemoCommand.IsValid())
	{
		AgentSourceControlDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCI.AgentSourceControlDemo"),
			TEXT("E6 source-control fail-fast/offline demo. Usage: HCI.AgentSourceControlDemo [tool_name] [source_control_enabled 0|1] [checkout_succeeded 0|1]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentSourceControlDemoCommand));
	}

	if (!AgentRbacDemoCommand.IsValid())
	{
		AgentRbacDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCI.AgentRbacDemo"),
			TEXT("E7 local mock RBAC + local audit log demo. Usage: HCI.AgentRbacDemo [user_name] [tool_name] [asset_count>=0]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentRbacDemoCommand));
	}

	if (!AgentLodSafetyDemoCommand.IsValid())
	{
		AgentLodSafetyDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCI.AgentLodSafetyDemo"),
			TEXT("E8 LOD tool safety boundary demo. Usage: HCI.AgentLodSafetyDemo [tool_name] [target_object_class] [nanite_enabled 0|1]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentLodSafetyDemoCommand));
	}

	if (!AgentPlanDemoCommand.IsValid())
	{
		AgentPlanDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCI.AgentPlanDemo"),
			TEXT("F1 NL->Plan JSON demo (structured plan preview). Usage: HCI.AgentPlanDemo [natural language text...]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentPlanDemoCommand));
	}

	if (!AgentPlanDemoJsonCommand.IsValid())
	{
		AgentPlanDemoJsonCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCI.AgentPlanDemoJson"),
			TEXT("F1 NL->Plan JSON demo (print contract JSON). Usage: HCI.AgentPlanDemoJson [natural language text...]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentPlanDemoJsonCommand));
	}

	if (!AgentPlanPreviewUiCommand.IsValid())
	{
		AgentPlanPreviewUiCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCI.AgentPlanPreviewUI"),
			TEXT("Stage I1 draft plan preview UI. Usage: HCI.AgentPlanPreviewUI [natural language text...]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentPlanPreviewUiCommand));
	}

		if (!AgentChatUiCommand.IsValid())
		{
			AgentChatUiCommand = MakeUnique<FAutoConsoleCommand>(
				TEXT("HCI.AgentChatUI"),
				TEXT("Stage I7 chat-like NL entry (history + shortcuts + summary). Usage: HCI.AgentChatUI [optional initial text]"),
				FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentChatUiCommand));
		}

	if (!AgentPlanValidateDemoCommand.IsValid())
	{
		AgentPlanValidateDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCI.AgentPlanValidateDemo"),
			TEXT("F2 Plan validator demo. Usage: HCI.AgentPlanValidateDemo [case_key]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentPlanValidateDemoCommand));
	}

	if (!AgentExecutePlanDemoCommand.IsValid())
	{
		AgentExecutePlanDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCI.AgentExecutePlanDemo"),
			TEXT("F3 Executor skeleton demo (validate + simulate execute + convergence logs). Usage: HCI.AgentExecutePlanDemo [natural language text...]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanDemoCommand));
	}

	if (!AgentExecutePlanFailDemoCommand.IsValid())
	{
		AgentExecutePlanFailDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCI.AgentExecutePlanFailDemo"),
			TEXT("F4 Executor failure convergence demo (step-level error + termination policy). Usage: HCI.AgentExecutePlanFailDemo [ok|fail_stop|fail_continue]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanFailDemoCommand));
	}

	if (!AgentExecutePlanPreflightDemoCommand.IsValid())
	{
		AgentExecutePlanPreflightDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCI.AgentExecutePlanPreflightDemo"),
			TEXT("F5 Executor preflight gate-chain demo (Confirm/BlastRadius/RBAC/SourceControl/LOD Safety). Usage: HCI.AgentExecutePlanPreflightDemo [ok|fail_confirm|fail_blast|fail_rbac|fail_sc|fail_lod]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanPreflightDemoCommand));
	}
}

void FHCIAgentDemoConsoleCommands::ShutdownCoreCommands()
{
	AgentExecutePlanPreflightDemoCommand.Reset();
	AgentExecutePlanFailDemoCommand.Reset();
	AgentExecutePlanDemoCommand.Reset();
	AgentPlanValidateDemoCommand.Reset();
	AgentChatUiCommand.Reset();
	AgentPlanPreviewUiCommand.Reset();
	AgentPlanDemoJsonCommand.Reset();
	AgentPlanDemoCommand.Reset();
	AgentLodSafetyDemoCommand.Reset();
	AgentRbacDemoCommand.Reset();
	AgentSourceControlDemoCommand.Reset();
	AgentTransactionDemoCommand.Reset();
	AgentBlastRadiusDemoCommand.Reset();
	AgentConfirmGateDemoCommand.Reset();
}

