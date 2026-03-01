#include "Commands/HCIAgentDemoConsoleCommands.h"

void FHCIAgentDemoConsoleCommands::Startup()
{
	StartupCoreCommands();
	StartupIngestCommands();
	StartupLlmCommands();
	StartupReviewCommands();
	StartupFixtureCommands();
}

void FHCIAgentDemoConsoleCommands::Shutdown()
{
	ShutdownFixtureCommands();
	ShutdownReviewCommands();
	ShutdownLlmCommands();
	ShutdownIngestCommands();
	ShutdownCoreCommands();
}


