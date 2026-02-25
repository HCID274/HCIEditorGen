#include "Subsystems/HCIAbilityKitAgentSubsystem.h"

#include "Agent/LLM/HCIAbilityKitAgentPromptBuilder.h"
#include "Async/Async.h"
#include "Commands/HCIAbilityKitAgentCommand_ChatPlanAndSummary.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UI/HCIAbilityKitAgentPlanPreviewWindow.h"

namespace
{
static FHCIAbilityKitAgentPromptBundleOptions HCI_MakeChatSummaryPromptBundleOptions_Subsystem()
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

	const FHCIAbilityKitAgentPromptBundleOptions Options = HCI_MakeChatSummaryPromptBundleOptions_Subsystem();
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

static FHCIAbilityKitAgentPlanPreviewContext HCI_MakePreviewContext(
	const FString& RouteReason,
	const FHCIAbilityKitAgentPlannerResultMetadata& PlannerMetadata)
{
	FHCIAbilityKitAgentPlanPreviewContext PreviewContext;
	PreviewContext.RouteReason = RouteReason;
	PreviewContext.PlannerProvider = PlannerMetadata.PlannerProvider.IsEmpty() ? TEXT("-") : PlannerMetadata.PlannerProvider;
	PreviewContext.ProviderMode = PlannerMetadata.ProviderMode.IsEmpty() ? TEXT("-") : PlannerMetadata.ProviderMode;
	PreviewContext.bFallbackUsed = PlannerMetadata.bFallbackUsed;
	PreviewContext.FallbackReason = PlannerMetadata.FallbackReason.IsEmpty() ? TEXT("-") : PlannerMetadata.FallbackReason;
	PreviewContext.EnvScannedAssetCount = PlannerMetadata.bEnvContextInjected ? PlannerMetadata.EnvContextAssetCount : INDEX_NONE;
	return PreviewContext;
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
	bHasLastPlan = false;
	LastPlan = FHCIAbilityKitAgentPlan();
	LastRouteReason.Reset();
	LastPlannerMetadata = FHCIAbilityKitAgentPlannerResultMetadata();
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

bool UHCIAbilityKitAgentSubsystem::HasLastPlan() const
{
	return bHasLastPlan;
}

bool UHCIAbilityKitAgentSubsystem::BuildLastPlanCardLines(TArray<FString>& OutLines) const
{
	OutLines.Reset();
	if (!bHasLastPlan)
	{
		return false;
	}

	OutLines.Add(FString::Printf(
		TEXT("request_id=%s | intent=%s | steps=%d"),
		LastPlan.RequestId.IsEmpty() ? TEXT("-") : *LastPlan.RequestId,
		LastPlan.Intent.IsEmpty() ? TEXT("-") : *LastPlan.Intent,
		LastPlan.Steps.Num()));
	OutLines.Add(FString::Printf(
		TEXT("route_reason=%s | provider=%s | mode=%s | fallback=%s"),
		LastRouteReason.IsEmpty() ? TEXT("-") : *LastRouteReason,
		LastPlannerMetadata.PlannerProvider.IsEmpty() ? TEXT("-") : *LastPlannerMetadata.PlannerProvider,
		LastPlannerMetadata.ProviderMode.IsEmpty() ? TEXT("-") : *LastPlannerMetadata.ProviderMode,
		LastPlannerMetadata.bFallbackUsed ? TEXT("true") : TEXT("false")));

	for (int32 Index = 0; Index < LastPlan.Steps.Num(); ++Index)
	{
		const FHCIAbilityKitAgentPlanStep& Step = LastPlan.Steps[Index];
		OutLines.Add(FString::Printf(
			TEXT("%d. %s | tool=%s | risk=%s"),
			Index + 1,
			Step.StepId.IsEmpty() ? TEXT("-") : *Step.StepId,
			*Step.ToolName.ToString(),
			*FHCIAbilityKitAgentPlanContract::RiskLevelToString(Step.RiskLevel)));
	}

	return true;
}

bool UHCIAbilityKitAgentSubsystem::OpenLastPlanPreview()
{
	if (!bHasLastPlan)
	{
		EmitAssistantLine(TEXT("暂无可预览计划，请先发送一条请求。"));
		return false;
	}

	const FHCIAbilityKitAgentPlanPreviewContext PreviewContext = HCI_MakePreviewContext(LastRouteReason, LastPlannerMetadata);
	FHCIAbilityKitAgentPlanPreviewWindow::OpenWindow(LastPlan, PreviewContext);
	EmitAssistantLine(TEXT("已打开计划预览窗口。"));
	return true;
}

bool UHCIAbilityKitAgentSubsystem::CommitLastPlanFromChat()
{
	if (ActiveCommand != nullptr)
	{
		EmitStatus(TEXT("状态：忙碌"));
		EmitAssistantLine(TEXT("已有请求执行中，请等待当前请求完成后再执行计划。"));
		return false;
	}

	if (!bHasLastPlan)
	{
		EmitAssistantLine(TEXT("暂无可执行计划，请先发送一条请求。"));
		return false;
	}

	const FString ConfirmText = FHCIAbilityKitAgentPlanPreviewWindow::BuildCommitConfirmMessage(LastPlan);
	const EAppReturnType::Type UserDecision = FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(ConfirmText));
	if (UserDecision != EAppReturnType::Yes)
	{
		EmitAssistantLine(TEXT("Commit 已取消（未触发真实写操作）。"));
		EmitStatus(TEXT("状态：空闲"));
		return false;
	}

	EmitStatus(TEXT("状态：执行中"));
	FHCIAbilityKitAgentPlanExecutionReport Report;
	FHCIAbilityKitAgentPlanPreviewWindow::ExecutePlan(LastPlan, false, true, Report);
	EmitAssistantLine(Report.SummaryText);
	EmitAssistantLine(Report.SearchPathEvidenceText);
	EmitStatus(TEXT("状态：空闲"));
	return Report.bRunOk;
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
	if (Result.bHasPlanPayload)
	{
		bHasLastPlan = true;
		LastPlan = Result.Plan;
		LastRouteReason = Result.RouteReason;
		LastPlannerMetadata = Result.PlannerMetadata;
		OnPlanReady.Broadcast();
	}

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
