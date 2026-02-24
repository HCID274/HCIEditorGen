#include "Agent/HCIAbilityKitAgentPromptBuilder.h"

#include "Dom/JsonObject.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
static FString HCI_CombineAsFullPath(const FString& RootDir, const FString& RelativePath)
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(RootDir, RelativePath));
}
}

bool FHCIAbilityKitAgentPromptBuilder::ResolveSkillBundleDirectory(
	const FHCIAbilityKitAgentPromptBundleOptions& Options,
	FString& OutBundleDir,
	FString& OutError)
{
	OutBundleDir.Reset();
	OutError.Reset();

	if (Options.SkillBundleRelativeDir.TrimStartAndEnd().IsEmpty())
	{
		OutError = TEXT("prompt_bundle_relative_dir_empty");
		return false;
	}

	TArray<FString> CandidateDirs;
	CandidateDirs.Reserve(4);

	if (FPaths::IsRelative(Options.SkillBundleRelativeDir))
	{
		CandidateDirs.Add(HCI_CombineAsFullPath(FPaths::ProjectDir(), Options.SkillBundleRelativeDir));

		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("HCIAbilityKit"));
		if (Plugin.IsValid())
		{
			CandidateDirs.Add(HCI_CombineAsFullPath(Plugin->GetBaseDir(), Options.SkillBundleRelativeDir));
			CandidateDirs.Add(HCI_CombineAsFullPath(FPaths::Combine(Plugin->GetBaseDir(), TEXT("../../..")), Options.SkillBundleRelativeDir));
		}
	}
	else
	{
		CandidateDirs.Add(FPaths::ConvertRelativePathToFull(Options.SkillBundleRelativeDir));
	}

	for (const FString& CandidateDir : CandidateDirs)
	{
		const FString PromptPath = FPaths::Combine(CandidateDir, Options.PromptTemplateFileName);
		const FString SchemaPath = FPaths::Combine(CandidateDir, Options.ToolsSchemaFileName);
		if (FPaths::FileExists(PromptPath) && FPaths::FileExists(SchemaPath))
		{
			OutBundleDir = CandidateDir;
			return true;
		}
	}

	OutError = FString::Printf(
		TEXT("prompt_bundle_not_found relative=%s candidates=%s"),
		*Options.SkillBundleRelativeDir,
		*FString::Join(CandidateDirs, TEXT("|")));
	return false;
}

bool FHCIAbilityKitAgentPromptBuilder::BuildSystemPromptFromBundle(
	const FString& UserInput,
	const FHCIAbilityKitAgentPromptBundleOptions& Options,
	FString& OutSystemPrompt,
	FString& OutError)
{
	OutSystemPrompt.Reset();
	OutError.Reset();

	FString BundleDir;
	if (!ResolveSkillBundleDirectory(Options, BundleDir, OutError))
	{
		return false;
	}

	const FString PromptTemplatePath = FPaths::Combine(BundleDir, Options.PromptTemplateFileName);
	const FString ToolsSchemaPath = FPaths::Combine(BundleDir, Options.ToolsSchemaFileName);

	FString PromptTemplateText;
	if (!FFileHelper::LoadFileToString(PromptTemplateText, *PromptTemplatePath))
	{
		OutError = FString::Printf(TEXT("prompt_template_read_failed path=%s"), *PromptTemplatePath);
		return false;
	}

	FString ToolsSchemaText;
	if (!FFileHelper::LoadFileToString(ToolsSchemaText, *ToolsSchemaPath))
	{
		OutError = FString::Printf(TEXT("tools_schema_read_failed path=%s"), *ToolsSchemaPath);
		return false;
	}

	TSharedPtr<FJsonObject> ToolsSchemaObject;
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ToolsSchemaText);
		if (!FJsonSerializer::Deserialize(Reader, ToolsSchemaObject) || !ToolsSchemaObject.IsValid())
		{
			OutError = FString::Printf(TEXT("tools_schema_invalid_json path=%s"), *ToolsSchemaPath);
			return false;
		}
	}

	FString SerializedToolsSchema;
	{
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&SerializedToolsSchema);
		FJsonSerializer::Serialize(ToolsSchemaObject.ToSharedRef(), Writer);
	}

	if (!PromptTemplateText.Contains(Options.ToolsSchemaPlaceholder))
	{
		OutError = FString::Printf(
			TEXT("prompt_template_placeholder_missing placeholder=%s path=%s"),
			*Options.ToolsSchemaPlaceholder,
			*PromptTemplatePath);
		return false;
	}
	if (!PromptTemplateText.Contains(Options.UserInputPlaceholder))
	{
		OutError = FString::Printf(
			TEXT("prompt_template_placeholder_missing placeholder=%s path=%s"),
			*Options.UserInputPlaceholder,
			*PromptTemplatePath);
		return false;
	}

	OutSystemPrompt = PromptTemplateText.Replace(
		*Options.ToolsSchemaPlaceholder,
		*SerializedToolsSchema,
		ESearchCase::CaseSensitive);
	OutSystemPrompt = OutSystemPrompt.Replace(
		*Options.UserInputPlaceholder,
		*UserInput,
		ESearchCase::CaseSensitive);
	OutSystemPrompt.TrimStartAndEndInline();
	if (OutSystemPrompt.IsEmpty())
	{
		OutError = TEXT("system_prompt_empty_after_bundle_injection");
		return false;
	}

	return true;
}
