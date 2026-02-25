#include "Subsystems/HCIAbilityKitAgentSubsystem.h"

#include "Agent/LLM/HCIAbilityKitAgentPromptBuilder.h"
#include "Async/Async.h"
#include "Commands/HCIAbilityKitAgentCommand_ChatPlanAndSummary.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
static FHCIAbilityKitAgentPromptBundleOptions HCI_MakeChatSummaryPromptBundleOptions()
{
	FHCIAbilityKitAgentPromptBundleOptions Options;
	Options.SkillBundleRelativeDir = TEXT("Source/HCIEditorGen/文档/提示词/Skills/I7_AgentChatSummary");
	Options.PromptTemplateFileName = TEXT("prompt.md");
	Options.ToolsSchemaFileName = TEXT("tools_schema.json");
	return Options;
}

static bool HCI_LoadChatQuickCommandsFromBundle(
	TArray<FHCIAbilityKitAgentQuickCommand>& OutCommands,
	FString& OutError)
{
	OutCommands.Reset();
	OutError.Reset();

	const FHCIAbilityKitAgentPromptBundleOptions Options = HCI_MakeChatSummaryPromptBundleOptions();
	FString BundleDir;
	if (!FHCIAbilityKitAgentPromptBuilder::ResolveSkillBundleDirectory(Options, BundleDir, OutError))
	{
		return false;
	}

	const FString QuickCommandsPath = FPaths::Combine(BundleDir, TEXT("quick_commands.json"));
	if (!FPaths::FileExists(QuickCommandsPath))
	{
		OutError = FString::Printf(TEXT("quick_commands_not_found path=%s"), *QuickCommandsPath);
		return false;
	}

	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *QuickCommandsPath))
	{
		OutError = FString::Printf(TEXT("quick_commands_read_failed path=%s"), *QuickCommandsPath);
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError = FString::Printf(TEXT("quick_commands_invalid_json path=%s"), *QuickCommandsPath);
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* CommandArray = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("quick_commands"), CommandArray) || CommandArray == nullptr)
	{
		OutError = TEXT("quick_commands_missing");
		return false;
	}

	for (const TSharedPtr<FJsonValue>& Value : *CommandArray)
	{
		const TSharedPtr<FJsonObject> Item = Value.IsValid() ? Value->AsObject() : nullptr;
		if (!Item.IsValid())
		{
			continue;
		}

		FHCIAbilityKitAgentQuickCommand Command;
		if (!Item->TryGetStringField(TEXT("label"), Command.Label) ||
			!Item->TryGetStringField(TEXT("prompt"), Command.Prompt))
		{
			continue;
		}

		Command.Label.TrimStartAndEndInline();
		Command.Prompt.TrimStartAndEndInline();
		if (!Command.Label.IsEmpty() && !Command.Prompt.IsEmpty())
		{
			OutCommands.Add(MoveTemp(Command));
		}
	}

	if (OutCommands.Num() <= 0)
	{
		OutError = TEXT("quick_commands_empty");
		return false;
	}

	return true;
}
}

void UHCIAbilityKitAgentSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	CommandRegistry.Reset();
	CommandRegistry.Add(TEXT("chat_submit"), UHCIAbilityKitAgentCommand_ChatPlanAndSummary::StaticClass());

	ReloadQuickCommands();
}

void UHCIAbilityKitAgentSubsystem::Deinitialize()
{
	ActiveCommand = nullptr;
	CommandRegistry.Reset();
	QuickCommands.Reset();
	QuickCommandsLoadError.Reset();
	Super::Deinitialize();
}

bool UHCIAbilityKitAgentSubsystem::SubmitChatInput(const FString& UserInput, const FString& SourceTag)
{
	const FString TrimmedInput = UserInput.TrimStartAndEnd();
	if (TrimmedInput.IsEmpty())
	{
		EmitAssistantLine(TEXT("输入为空，未发送。"));
		return false;
	}

	if (ActiveCommand != nullptr)
	{
		EmitStatus(TEXT("状态：忙碌"));
		EmitAssistantLine(TEXT("已有请求执行中，请等待当前请求完成后再发送。"));
		return false;
	}

	EmitUserLine(TrimmedInput);
	EmitAssistantLine(TEXT("已发送，等待规划结果..."));
	EmitStatus(TEXT("状态：发送中"));

	FHCIAbilityKitAgentCommandContext Context;
	Context.InputParam = TrimmedInput;
	Context.SourceTag = SourceTag;
	if (!ExecuteRegisteredCommand(TEXT("chat_submit"), Context))
	{
		EmitStatus(TEXT("状态：空闲（上次失败）"));
		EmitAssistantLine(TEXT("命令未注册：chat_submit"));
		return false;
	}
	return true;
}

bool UHCIAbilityKitAgentSubsystem::IsBusy() const
{
	return ActiveCommand != nullptr;
}

void UHCIAbilityKitAgentSubsystem::ReloadQuickCommands()
{
	FString Error;
	if (!HCI_LoadChatQuickCommandsFromBundle(QuickCommands, Error))
	{
		QuickCommands.Reset();
		QuickCommandsLoadError = Error;
		return;
	}

	QuickCommandsLoadError.Reset();
}

const TArray<FHCIAbilityKitAgentQuickCommand>& UHCIAbilityKitAgentSubsystem::GetQuickCommands() const
{
	return QuickCommands;
}

const FString& UHCIAbilityKitAgentSubsystem::GetQuickCommandsLoadError() const
{
	return QuickCommandsLoadError;
}

void UHCIAbilityKitAgentSubsystem::EmitUserLine(const FString& Text)
{
	OnChatLine.Broadcast(FString::Printf(TEXT("你：%s"), *Text));
}

void UHCIAbilityKitAgentSubsystem::EmitAssistantLine(const FString& Text)
{
	OnChatLine.Broadcast(FString::Printf(TEXT("系统：%s"), *Text));
}

void UHCIAbilityKitAgentSubsystem::EmitStatus(const FString& Text)
{
	OnStatusChanged.Broadcast(Text);
}

void UHCIAbilityKitAgentSubsystem::HandleCommandCompleted(const FHCIAbilityKitAgentCommandResult& Result)
{
	ActiveCommand = nullptr;

	if (Result.bSuccess)
	{
		EmitAssistantLine(Result.Message);
		EmitStatus(TEXT("状态：空闲"));
		OnSummaryReceived.Broadcast(Result.Message);
		return;
	}

	if (Result.bSummaryFailure)
	{
		EmitAssistantLine(FString::Printf(TEXT("摘要生成失败：%s"), *Result.Message));
		EmitStatus(TEXT("状态：空闲（摘要失败）"));
		return;
	}

	EmitAssistantLine(Result.Message);
	EmitStatus(TEXT("状态：空闲（上次失败）"));
}

bool UHCIAbilityKitAgentSubsystem::ExecuteRegisteredCommand(
	const FName& CommandName,
	const FHCIAbilityKitAgentCommandContext& Context)
{
	const TSubclassOf<UHCIAbilityKitAgentCommandBase>* CommandClass = CommandRegistry.Find(CommandName);
	if (CommandClass == nullptr || !CommandClass->Get())
	{
		return false;
	}

	UHCIAbilityKitAgentCommandBase* Command = NewObject<UHCIAbilityKitAgentCommandBase>(this, CommandClass->Get());
	if (Command == nullptr)
	{
		return false;
	}

	ActiveCommand = Command;
	Command->ExecuteAsync(
		Context,
		FHCIAbilityKitAgentCommandComplete::CreateUObject(
			this,
			&UHCIAbilityKitAgentSubsystem::HandleCommandCompleted));
	return true;
}