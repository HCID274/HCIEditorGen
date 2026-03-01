#include "Commands/HCIAbilityKitAgentDemoConsoleCommands.h"

void FHCIAbilityKitAgentDemoConsoleCommands::Startup()
{
	StartupCoreCommands();
	StartupIngestCommands();
	StartupLlmCommands();
	StartupReviewCommands();
	StartupFixtureCommands();
}

void FHCIAbilityKitAgentDemoConsoleCommands::Shutdown()
{
	ShutdownFixtureCommands();
	ShutdownReviewCommands();
	ShutdownLlmCommands();
	ShutdownIngestCommands();
	ShutdownCoreCommands();
}

