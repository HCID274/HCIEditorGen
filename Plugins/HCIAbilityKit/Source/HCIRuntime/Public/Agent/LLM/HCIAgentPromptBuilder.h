#pragma once

#include "CoreMinimal.h"

struct HCIRUNTIME_API FHCIAgentPromptBundleOptions
{
	FString SkillBundleRelativeDir = TEXT("Source/HCIEditorGen/文档/提示词/Skills/H3_AgentPlanner");
	FString PromptTemplateFileName = TEXT("prompt.md");
	FString ToolsSchemaFileName = TEXT("tools_schema.json");
	FString ToolsSchemaPlaceholder = TEXT("{{TOOLS_SCHEMA}}");
	FString EnvContextPlaceholder = TEXT("{{ENV_CONTEXT}}");
	FString UserInputPlaceholder = TEXT("{{USER_INPUT}}");
};

class HCIRUNTIME_API FHCIAgentPromptBuilder
{
public:
	static bool ResolveSkillBundleDirectory(
		const FHCIAgentPromptBundleOptions& Options,
		FString& OutBundleDir,
		FString& OutError);

	static bool BuildSystemPromptFromBundle(
		const FString& UserInput,
		const FHCIAgentPromptBundleOptions& Options,
		FString& OutSystemPrompt,
		FString& OutError);

	static bool BuildSystemPromptFromBundleWithEnvContext(
		const FString& UserInput,
		const FString& EnvContext,
		const FHCIAgentPromptBundleOptions& Options,
		FString& OutSystemPrompt,
		FString& OutError);
};


