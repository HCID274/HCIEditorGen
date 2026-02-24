#include "Commands/HCIAbilityKitAgentDemoConsoleCommands.h"

#include "Commands/HCIAbilityKitAgentCommandHandlers.h"

void FHCIAbilityKitAgentDemoConsoleCommands::StartupLlmCommands()
{
	if (!AgentPlanWithLLMDemoCommand.IsValid())
	{
		AgentPlanWithLLMDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentPlanWithLLMDemo"),
			TEXT("H1 NL->Plan JSON demo (LLM preferred + validator). Usage: HCIAbilityKit.AgentPlanWithLLMDemo [natural language text...]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentPlanWithLLMDemoCommand));
	}

	if (!AgentPlanWithRealLLMDemoCommand.IsValid())
	{
		AgentPlanWithRealLLMDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentPlanWithRealLLMDemo"),
			TEXT("H3 NL->Plan JSON demo (real HTTP LLM provider). Usage: HCIAbilityKit.AgentPlanWithRealLLMDemo [normal|config_missing|http_fail] [natural language text...]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentPlanWithRealLLMDemoCommand));
	}

	if (!AgentPlanWithRealLLMProbeCommand.IsValid())
	{
		AgentPlanWithRealLLMProbeCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentPlanWithRealLLMProbe"),
			TEXT("H3 HTTP probe (raw response). Usage: HCIAbilityKit.AgentPlanWithRealLLMProbe [prompt_text]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentPlanWithRealLLMProbeCommand));
	}

	if (!AgentPlanWithLLMFallbackDemoCommand.IsValid())
	{
		AgentPlanWithLLMFallbackDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentPlanWithLLMFallbackDemo"),
			TEXT("H1 NL->Plan JSON fallback demo. Usage: HCIAbilityKit.AgentPlanWithLLMFallbackDemo [timeout|invalid_json|empty|contract_invalid] [natural language text...]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentPlanWithLLMFallbackDemoCommand));
	}

	if (!AgentPlanWithLLMStabilityDemoCommand.IsValid())
	{
		AgentPlanWithLLMStabilityDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentPlanWithLLMStabilityDemo"),
			TEXT("H2 LLM stability demo (retry/circuit/metrics). Usage: HCIAbilityKit.AgentPlanWithLLMStabilityDemo [all|retry_success|retry_fallback|circuit_open|reset_metrics] [natural language text...]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentPlanWithLLMStabilityDemoCommand));
	}

	if (!AgentPlanWithLLMMetricsDumpCommand.IsValid())
	{
		AgentPlanWithLLMMetricsDumpCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentPlanWithLLMMetricsDump"),
			TEXT("H2 dump planner stability metrics snapshot."),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentPlanWithLLMMetricsDumpCommand));
	}
}

void FHCIAbilityKitAgentDemoConsoleCommands::ShutdownLlmCommands()
{
	AgentPlanWithLLMMetricsDumpCommand.Reset();
	AgentPlanWithLLMStabilityDemoCommand.Reset();
	AgentPlanWithLLMFallbackDemoCommand.Reset();
	AgentPlanWithRealLLMProbeCommand.Reset();
	AgentPlanWithRealLLMDemoCommand.Reset();
	AgentPlanWithLLMDemoCommand.Reset();
}
