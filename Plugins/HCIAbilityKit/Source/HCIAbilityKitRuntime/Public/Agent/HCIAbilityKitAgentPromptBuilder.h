#pragma once

#include "CoreMinimal.h"

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentPromptBundleOptions
{
	FString SkillBundleRelativeDir = TEXT("Source/HCIEditorGen/文档/提示词/Skills/H3_AgentPlanner");
	FString PromptTemplateFileName = TEXT("prompt.md");
	FString ToolsSchemaFileName = TEXT("tools_schema.json");
	FString ToolsSchemaPlaceholder = TEXT("{{TOOLS_SCHEMA}}");
	FString UserInputPlaceholder = TEXT("{{USER_INPUT}}");
};

class HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentPromptBuilder
{
public:
	static bool ResolveSkillBundleDirectory(
		const FHCIAbilityKitAgentPromptBundleOptions& Options,
		FString& OutBundleDir,
		FString& OutError);

	static bool BuildSystemPromptFromBundle(
		const FString& UserInput,
		const FHCIAbilityKitAgentPromptBundleOptions& Options,
		FString& OutSystemPrompt,
		FString& OutError);
};
