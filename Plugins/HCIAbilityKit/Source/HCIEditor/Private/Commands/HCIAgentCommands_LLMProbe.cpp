#include "Commands/HCIAgentDemoConsoleCommands.h"

#include "Commands/HCIAgentCommandHandlers.h"

void FHCIAgentDemoConsoleCommands::StartupLlmCommands()
{
	if (!AgentPlanWithLLMDemoCommand.IsValid())
	{
		AgentPlanWithLLMDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCI.AgentPlanWithLLMDemo"),
			TEXT("H1 NL->Plan JSON demo (LLM preferred + validator). Usage: HCI.AgentPlanWithLLMDemo [natural language text...]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentPlanWithLLMDemoCommand));
	}

	if (!AgentPlanWithRealLLMDemoCommand.IsValid())
	{
		AgentPlanWithRealLLMDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCI.AgentPlanWithRealLLMDemo"),
			TEXT("H3 NL->Plan JSON demo (real HTTP LLM provider). Usage: HCI.AgentPlanWithRealLLMDemo [normal|config_missing|http_fail] [natural language text...]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentPlanWithRealLLMDemoCommand));
	}

	if (!AgentPlanWithRealLLMProbeCommand.IsValid())
	{
		AgentPlanWithRealLLMProbeCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCI.AgentPlanWithRealLLMProbe"),
			TEXT("H3 HTTP probe (raw response). Usage: HCI.AgentPlanWithRealLLMProbe [prompt_text]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentPlanWithRealLLMProbeCommand));
	}

	if (!AgentPlanWithLLMFallbackDemoCommand.IsValid())
	{
		AgentPlanWithLLMFallbackDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCI.AgentPlanWithLLMFallbackDemo"),
			TEXT("H1 NL->Plan JSON fallback demo. Usage: HCI.AgentPlanWithLLMFallbackDemo [timeout|invalid_json|empty|contract_invalid] [natural language text...]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentPlanWithLLMFallbackDemoCommand));
	}

	if (!AgentPlanWithLLMStabilityDemoCommand.IsValid())
	{
		AgentPlanWithLLMStabilityDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCI.AgentPlanWithLLMStabilityDemo"),
			TEXT("H2 LLM stability demo (retry/circuit/metrics). Usage: HCI.AgentPlanWithLLMStabilityDemo [all|retry_success|retry_fallback|circuit_open|reset_metrics] [natural language text...]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentPlanWithLLMStabilityDemoCommand));
	}

	if (!AgentPlanWithLLMMetricsDumpCommand.IsValid())
	{
		AgentPlanWithLLMMetricsDumpCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCI.AgentPlanWithLLMMetricsDump"),
			TEXT("H2 dump planner stability metrics snapshot."),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentPlanWithLLMMetricsDumpCommand));
	}
}

void FHCIAgentDemoConsoleCommands::ShutdownLlmCommands()
{
	AgentPlanWithLLMMetricsDumpCommand.Reset();
	AgentPlanWithLLMStabilityDemoCommand.Reset();
	AgentPlanWithLLMFallbackDemoCommand.Reset();
	AgentPlanWithRealLLMProbeCommand.Reset();
	AgentPlanWithRealLLMDemoCommand.Reset();
	AgentPlanWithLLMDemoCommand.Reset();
}

