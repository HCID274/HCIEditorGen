#include "Commands/HCIAbilityKitAgentDemoConsoleCommands.h"

#include "Commands/HCIAbilityKitAgentCommandHandlers.h"

void FHCIAbilityKitAgentDemoConsoleCommands::StartupCoreCommands()
{
	if (!AgentConfirmGateDemoCommand.IsValid())
	{
		AgentConfirmGateDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentConfirmGateDemo"),
			TEXT("E3 confirm-gate demo. Usage: HCIAbilityKit.AgentConfirmGateDemo [tool_name] [requires_confirm 0|1] [user_confirmed 0|1]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentConfirmGateDemoCommand));
	}

	if (!AgentBlastRadiusDemoCommand.IsValid())
	{
		AgentBlastRadiusDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentBlastRadiusDemo"),
			TEXT("E4 blast-radius demo. Usage: HCIAbilityKit.AgentBlastRadiusDemo [tool_name] [target_modify_count>=0]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentBlastRadiusDemoCommand));
	}

	if (!AgentTransactionDemoCommand.IsValid())
	{
		AgentTransactionDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentTransactionDemo"),
			TEXT("E5 all-or-nothing transaction demo. Usage: HCIAbilityKit.AgentTransactionDemo [total_steps>=1] [fail_step_index>=0]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentTransactionDemoCommand));
	}

	if (!AgentSourceControlDemoCommand.IsValid())
	{
		AgentSourceControlDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentSourceControlDemo"),
			TEXT("E6 source-control fail-fast/offline demo. Usage: HCIAbilityKit.AgentSourceControlDemo [tool_name] [source_control_enabled 0|1] [checkout_succeeded 0|1]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentSourceControlDemoCommand));
	}

	if (!AgentRbacDemoCommand.IsValid())
	{
		AgentRbacDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentRbacDemo"),
			TEXT("E7 local mock RBAC + local audit log demo. Usage: HCIAbilityKit.AgentRbacDemo [user_name] [tool_name] [asset_count>=0]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentRbacDemoCommand));
	}

	if (!AgentLodSafetyDemoCommand.IsValid())
	{
		AgentLodSafetyDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentLodSafetyDemo"),
			TEXT("E8 LOD tool safety boundary demo. Usage: HCIAbilityKit.AgentLodSafetyDemo [tool_name] [target_object_class] [nanite_enabled 0|1]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentLodSafetyDemoCommand));
	}

	if (!AgentPlanDemoCommand.IsValid())
	{
		AgentPlanDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentPlanDemo"),
			TEXT("F1 NL->Plan JSON demo (structured plan preview). Usage: HCIAbilityKit.AgentPlanDemo [natural language text...]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentPlanDemoCommand));
	}

	if (!AgentPlanDemoJsonCommand.IsValid())
	{
		AgentPlanDemoJsonCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentPlanDemoJson"),
			TEXT("F1 NL->Plan JSON demo (print contract JSON). Usage: HCIAbilityKit.AgentPlanDemoJson [natural language text...]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentPlanDemoJsonCommand));
	}

	if (!AgentPlanPreviewUiCommand.IsValid())
	{
		AgentPlanPreviewUiCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentPlanPreviewUI"),
			TEXT("Stage I1 draft plan preview UI. Usage: HCIAbilityKit.AgentPlanPreviewUI [natural language text...]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentPlanPreviewUiCommand));
	}

	if (!AgentPlanValidateDemoCommand.IsValid())
	{
		AgentPlanValidateDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentPlanValidateDemo"),
			TEXT("F2 Plan validator demo. Usage: HCIAbilityKit.AgentPlanValidateDemo [case_key]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentPlanValidateDemoCommand));
	}

	if (!AgentExecutePlanDemoCommand.IsValid())
	{
		AgentExecutePlanDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanDemo"),
			TEXT("F3 Executor skeleton demo (validate + simulate execute + convergence logs). Usage: HCIAbilityKit.AgentExecutePlanDemo [natural language text...]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanDemoCommand));
	}

	if (!AgentExecutePlanFailDemoCommand.IsValid())
	{
		AgentExecutePlanFailDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanFailDemo"),
			TEXT("F4 Executor failure convergence demo (step-level error + termination policy). Usage: HCIAbilityKit.AgentExecutePlanFailDemo [ok|fail_stop|fail_continue]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanFailDemoCommand));
	}

	if (!AgentExecutePlanPreflightDemoCommand.IsValid())
	{
		AgentExecutePlanPreflightDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanPreflightDemo"),
			TEXT("F5 Executor preflight gate-chain demo (Confirm/BlastRadius/RBAC/SourceControl/LOD Safety). Usage: HCIAbilityKit.AgentExecutePlanPreflightDemo [ok|fail_confirm|fail_blast|fail_rbac|fail_sc|fail_lod]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanPreflightDemoCommand));
	}
}

void FHCIAbilityKitAgentDemoConsoleCommands::ShutdownCoreCommands()
{
	AgentExecutePlanPreflightDemoCommand.Reset();
	AgentExecutePlanFailDemoCommand.Reset();
	AgentExecutePlanDemoCommand.Reset();
	AgentPlanValidateDemoCommand.Reset();
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
