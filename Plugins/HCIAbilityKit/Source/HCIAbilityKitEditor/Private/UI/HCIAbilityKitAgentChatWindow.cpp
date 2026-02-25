#include "UI/HCIAbilityKitAgentChatWindow.h"

#include "Agent/HCIAbilityKitAgentLlmClient.h"
#include "Agent/HCIAbilityKitAgentPlan.h"
#include "Agent/HCIAbilityKitAgentPlanJsonSerializer.h"
#include "Agent/HCIAbilityKitAgentPlanner.h"
#include "Agent/HCIAbilityKitAgentPromptBuilder.h"
#include "Async/Async.h"
#include "Commands/HCIAbilityKitAgentCommandHandlers.h"
#include "Dom/JsonObject.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
struct FHCIAbilityKitChatQuickCommand
{
	FString Label;
	FString Prompt;
};

static FString HCI_GetAgentChatHistoryPath()
{
	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("HCIAbilityKit"),
		TEXT("Config"),
		TEXT("agent_chat_history.local.json"));
}

static bool HCI_LoadAgentChatHistory(TArray<FString>& OutLines, FString& OutError)
{
	OutLines.Reset();
	OutError.Reset();

	const FString HistoryPath = HCI_GetAgentChatHistoryPath();
	if (!FPaths::FileExists(HistoryPath))
	{
		return true;
	}

	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *HistoryPath))
	{
		OutError = TEXT("load_file_failed");
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError = TEXT("invalid_json");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Lines = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("history_lines"), Lines) || Lines == nullptr)
	{
		return true;
	}

	OutLines.Reserve(Lines->Num());
	for (const TSharedPtr<FJsonValue>& Value : *Lines)
	{
		FString Line;
		if (Value.IsValid() && Value->TryGetString(Line))
		{
			OutLines.Add(Line);
		}
	}
	return true;
}

static bool HCI_SaveAgentChatHistory(const TArray<FString>& InLines, FString& OutError)
{
	OutError.Reset();
	const FString HistoryPath = HCI_GetAgentChatHistoryPath();
	const FString HistoryDir = FPaths::GetPath(HistoryPath);
	IFileManager::Get().MakeDirectory(*HistoryDir, true);

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Lines;
	Lines.Reserve(InLines.Num());
	for (const FString& Line : InLines)
	{
		Lines.Add(MakeShared<FJsonValueString>(Line));
	}
	RootObject->SetArrayField(TEXT("history_lines"), Lines);

	FString JsonText;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonText);
	if (!FJsonSerializer::Serialize(RootObject, Writer))
	{
		OutError = TEXT("serialize_failed");
		return false;
	}

	if (!FFileHelper::SaveStringToFile(JsonText, *HistoryPath))
	{
		OutError = TEXT("save_file_failed");
		return false;
	}
	return true;
}

static FHCIAbilityKitAgentPromptBundleOptions HCI_MakeChatSummaryPromptBundleOptions()
{
	FHCIAbilityKitAgentPromptBundleOptions Options;
	Options.SkillBundleRelativeDir = TEXT("Source/HCIEditorGen/文档/提示词/Skills/I7_AgentChatSummary");
	Options.PromptTemplateFileName = TEXT("prompt.md");
	Options.ToolsSchemaFileName = TEXT("tools_schema.json");
	return Options;
}

static bool HCI_LoadChatQuickCommandsFromBundle(TArray<FHCIAbilityKitChatQuickCommand>& OutCommands, FString& OutError)
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

		FHCIAbilityKitChatQuickCommand Command;
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

static FString HCI_SerializePlanContextForSummary(
	const FString& UserText,
	const FHCIAbilityKitAgentPlan& Plan,
	const FString& RouteReason,
	const FHCIAbilityKitAgentPlannerResultMetadata& PlannerMetadata)
{
	FString PlanJson;
	if (!FHCIAbilityKitAgentPlanJsonSerializer::SerializeToJsonString(Plan, PlanJson))
	{
		PlanJson = TEXT("{}");
	}

	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("user_text"), UserText);
	Root->SetStringField(TEXT("route_reason"), RouteReason);
	Root->SetStringField(TEXT("planner_provider"), PlannerMetadata.PlannerProvider);
	Root->SetStringField(TEXT("provider_mode"), PlannerMetadata.ProviderMode);
	Root->SetBoolField(TEXT("fallback_used"), PlannerMetadata.bFallbackUsed);
	Root->SetStringField(TEXT("fallback_reason"), PlannerMetadata.FallbackReason);
	Root->SetStringField(TEXT("error_code"), PlannerMetadata.ErrorCode);
	Root->SetStringField(TEXT("plan_json"), PlanJson);

	FString Out;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
	FJsonSerializer::Serialize(Root, Writer);
	return Out;
}

static bool HCI_TryFormatSummaryTextFromLlmOutput(const FString& RawOutput, FString& OutSummary)
{
	OutSummary.Reset();
	const FString Trimmed = RawOutput.TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Trimmed);
	if (FJsonSerializer::Deserialize(Reader, RootObject) && RootObject.IsValid())
	{
		FString Summary;
		FString RiskNote;
		FString NextAction;
		RootObject->TryGetStringField(TEXT("summary"), Summary);
		RootObject->TryGetStringField(TEXT("risk_note"), RiskNote);
		RootObject->TryGetStringField(TEXT("next_action"), NextAction);

		Summary.TrimStartAndEndInline();
		RiskNote.TrimStartAndEndInline();
		NextAction.TrimStartAndEndInline();
		if (!Summary.IsEmpty())
		{
			OutSummary = FString::Printf(TEXT("摘要：%s"), *Summary);
			if (!RiskNote.IsEmpty())
			{
				OutSummary += FString::Printf(TEXT(" | 风险：%s"), *RiskNote);
			}
			if (!NextAction.IsEmpty())
			{
				OutSummary += FString::Printf(TEXT(" | 建议：%s"), *NextAction);
			}
			return true;
		}
	}

	OutSummary = Trimmed;
	return true;
}

static void HCI_RequestChatSummaryFromPromptAsync(
	const FString& UserText,
	const FHCIAbilityKitAgentPlan& Plan,
	const FString& RouteReason,
	const FHCIAbilityKitAgentPlannerResultMetadata& PlannerMetadata,
	TFunction<void(bool, const FString&)>&& OnComplete)
{
	auto Complete = [OnComplete = MoveTemp(OnComplete)](const bool bOk, const FString& Message) mutable
	{
		if (OnComplete)
		{
			OnComplete(bOk, Message);
		}
	};

	const FString ContextJson = HCI_SerializePlanContextForSummary(UserText, Plan, RouteReason, PlannerMetadata);

	const FHCIAbilityKitAgentPromptBundleOptions BundleOptions = HCI_MakeChatSummaryPromptBundleOptions();
	FString SystemPrompt;
	FString PromptError;
	if (!FHCIAbilityKitAgentPromptBuilder::BuildSystemPromptFromBundleWithEnvContext(
		ContextJson,
		TEXT("source=AgentChatUI"),
		BundleOptions,
		SystemPrompt,
		PromptError))
	{
		Complete(false, FString::Printf(TEXT("summary_prompt_build_failed:%s"), *PromptError));
		return;
	}

	FHCIAbilityKitAgentLlmProviderConfig ProviderConfig;
	FString ConfigError;
	if (!FHCIAbilityKitAgentLlmClient::LoadProviderConfigFromJsonFile(
		TEXT("Saved/HCIAbilityKit/Config/llm_provider.local.json"),
		TEXT("https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions"),
		TEXT("qwen3.5-plus"),
		ProviderConfig,
		ConfigError))
	{
		Complete(false, FString::Printf(TEXT("summary_config_error:%s"), *ConfigError));
		return;
	}

	FString RequestBody;
	FString RequestBodyError;
	if (!FHCIAbilityKitAgentLlmClient::BuildChatCompletionsRequestBody(
		SystemPrompt,
		ContextJson,
		ProviderConfig,
		RequestBody,
		RequestBodyError))
	{
		Complete(false, FString::Printf(TEXT("summary_request_body_error:%s"), *RequestBodyError));
		return;
	}

	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request =
		FHCIAbilityKitAgentLlmClient::CreateChatCompletionsHttpRequest(ProviderConfig, RequestBody);
	if (!Request.IsValid())
	{
		Complete(false, TEXT("summary_create_request_failed"));
		return;
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("[HCIAbilityKit][AgentChatUI][Summary] dispatched model=%s bundle=%s"),
		*ProviderConfig.Model,
		*BundleOptions.SkillBundleRelativeDir);

	Request->OnProcessRequestComplete().BindLambda(
		[Complete = MoveTemp(Complete)](
			const FHttpRequestPtr,
			const FHttpResponsePtr HttpResponse,
			const bool bRequestSuccess) mutable
		{
			if (!bRequestSuccess || !HttpResponse.IsValid())
			{
				Complete(false, TEXT("summary_http_failed"));
				return;
			}

			if (HttpResponse->GetResponseCode() < 200 || HttpResponse->GetResponseCode() >= 300)
			{
				UE_LOG(
					LogTemp,
					Warning,
					TEXT("[HCIAbilityKit][AgentChatUI][Summary] done success=false status=%d"),
					HttpResponse->GetResponseCode());
				Complete(false, FString::Printf(TEXT("summary_http_status_%d"), HttpResponse->GetResponseCode()));
				return;
			}

			FString Content;
			FString LlmErrorCode;
			FString LlmError;
			if (!FHCIAbilityKitAgentLlmClient::TryExtractMessageContentFromResponse(
				HttpResponse->GetContentAsString(),
				Content,
				LlmErrorCode,
				LlmError))
			{
				UE_LOG(
					LogTemp,
					Warning,
					TEXT("[HCIAbilityKit][AgentChatUI][Summary] done success=false reason=extract_failed error_code=%s"),
					*LlmErrorCode);
				Complete(false, FString::Printf(TEXT("summary_extract_failed:%s:%s"), *LlmErrorCode, *LlmError));
				return;
			}

			FString SummaryText;
			if (!HCI_TryFormatSummaryTextFromLlmOutput(Content, SummaryText))
			{
				UE_LOG(
					LogTemp,
					Warning,
					TEXT("[HCIAbilityKit][AgentChatUI][Summary] done success=false reason=output_empty"));
				Complete(false, TEXT("summary_output_empty"));
				return;
			}

			UE_LOG(LogTemp, Display, TEXT("[HCIAbilityKit][AgentChatUI][Summary] done success=true status=200"));
			Complete(true, SummaryText);
		});

	if (!Request->ProcessRequest())
	{
		Complete(false, TEXT("summary_process_request_failed"));
	}
}

class SHCIAbilityKitAgentChatWindow final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHCIAbilityKitAgentChatWindow)
		: _InitialInput()
	{
	}
		SLATE_ARGUMENT(FString, InitialInput)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		InitialInput = InArgs._InitialInput;
		LoadQuickCommandsFromBundle();

		ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(10.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(StatusTextBlock, STextBlock)
					.Text(FText::FromString(TEXT("状态：空闲")))
				]
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(0.0f, 8.0f, 0.0f, 8.0f)
				[
					SAssignNew(HistoryTextBox, SMultiLineEditableTextBox)
					.IsReadOnly(true)
					.AlwaysShowScrollbars(true)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("快捷指令：")))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.Padding(8.0f, 0.0f, 0.0f, 0.0f)
					[
						SAssignNew(QuickCommandsBox, SHorizontalBox)
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SAssignNew(InputTextBox, SEditableTextBox)
						.HintText(FText::FromString(TEXT("输入自然语言指令，例如：整理临时目录资产并归档")))
						.OnTextCommitted(this, &SHCIAbilityKitAgentChatWindow::HandleInputCommitted)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(8.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.Text(FText::FromString(TEXT("发送")))
						.OnClicked(this, &SHCIAbilityKitAgentChatWindow::HandleSendClicked)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(8.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.Text(FText::FromString(TEXT("清空")))
						.OnClicked(this, &SHCIAbilityKitAgentChatWindow::HandleClearClicked)
					]
				]
			]
		];

		RebuildQuickCommandButtons();

		const int32 RestoredCount = LoadHistoryFromDisk();
		if (RestoredCount <= 0)
		{
			AppendAssistantLine(TEXT("聊天入口已就绪。发送后将走真实 LLM 规划链路，并自动弹出 PlanPreview。"));
		}
		else
		{
			RefreshHistoryText();
			AppendAssistantLine(FString::Printf(TEXT("已恢复历史消息 %d 条。"), RestoredCount));
		}

		if (!QuickCommandsLoadError.IsEmpty())
		{
			AppendAssistantLine(FString::Printf(TEXT("快捷指令加载失败：%s"), *QuickCommandsLoadError));
		}

		if (HCI_IsAgentPlanPreviewRequestInFlight())
		{
			SetStatus(TEXT("状态：忙碌（已有请求执行中）"));
		}

		if (!InitialInput.TrimStartAndEnd().IsEmpty() && InputTextBox.IsValid())
		{
			InputTextBox->SetText(FText::FromString(InitialInput));
		}
	}

private:
	FReply HandleSendClicked()
	{
		if (!InputTextBox.IsValid())
		{
			return FReply::Handled();
		}

		const FString UserInput = InputTextBox->GetText().ToString().TrimStartAndEnd();
		return SubmitUserInput(UserInput);
	}

	FReply SubmitUserInput(const FString& UserInput)
	{
		if (UserInput.IsEmpty())
		{
			AppendAssistantLine(TEXT("输入为空，未发送。"));
			return FReply::Handled();
		}

		if (InputTextBox.IsValid())
		{
			InputTextBox->SetText(FText::GetEmpty());
		}
		AppendUserLine(UserInput);
		AppendAssistantLine(TEXT("已发送，等待规划结果..."));
		SetStatus(TEXT("状态：发送中"));

		const TWeakPtr<SHCIAbilityKitAgentChatWindow> WeakWidget(SharedThis(this));
		const bool bAccepted = HCI_RequestAgentPlanPreviewFromUi(
			UserInput,
			TEXT("AgentChatUI"),
			[WeakWidget](
				const bool bSuccess,
				const FString& RequestText,
				const FHCIAbilityKitAgentPlan& Plan,
				const FString& RouteReason,
				const FHCIAbilityKitAgentPlannerResultMetadata& PlannerMetadata,
				const FString& Error)
			{
				AsyncTask(ENamedThreads::GameThread, [WeakWidget, bSuccess, RequestText, Plan, RouteReason, PlannerMetadata, Error]()
				{
					const TSharedPtr<SHCIAbilityKitAgentChatWindow> Pinned = WeakWidget.Pin();
					if (!Pinned.IsValid())
					{
						return;
					}

					if (!bSuccess)
					{
						Pinned->AppendAssistantLine(FString::Printf(
							TEXT("规划失败：input=%s reason=%s provider=%s fallback=%s error_code=%s"),
							RequestText.IsEmpty() ? TEXT("-") : *RequestText,
							Error.IsEmpty() ? TEXT("-") : *Error,
							PlannerMetadata.PlannerProvider.IsEmpty() ? TEXT("-") : *PlannerMetadata.PlannerProvider,
							PlannerMetadata.FallbackReason.IsEmpty() ? TEXT("-") : *PlannerMetadata.FallbackReason,
							PlannerMetadata.ErrorCode.IsEmpty() ? TEXT("-") : *PlannerMetadata.ErrorCode));
						Pinned->SetStatus(TEXT("状态：空闲（上次失败）"));
						return;
					}

					Pinned->AppendAssistantLine(TEXT("规划完成，正在调用提示词生成摘要..."));
					Pinned->SetStatus(TEXT("状态：摘要生成中"));

					HCI_RequestChatSummaryFromPromptAsync(
						RequestText,
						Plan,
						RouteReason,
						PlannerMetadata,
						[WeakWidget](const bool bSummaryOk, const FString& SummaryMessage)
						{
							AsyncTask(ENamedThreads::GameThread, [WeakWidget, bSummaryOk, SummaryMessage]()
							{
								const TSharedPtr<SHCIAbilityKitAgentChatWindow> PinnedInner = WeakWidget.Pin();
								if (!PinnedInner.IsValid())
								{
									return;
								}

								if (bSummaryOk)
								{
									PinnedInner->AppendAssistantLine(SummaryMessage);
									PinnedInner->SetStatus(TEXT("状态：空闲"));
								}
								else
								{
									PinnedInner->AppendAssistantLine(FString::Printf(TEXT("摘要生成失败：%s"), *SummaryMessage));
									PinnedInner->SetStatus(TEXT("状态：空闲（摘要失败）"));
								}
							});
						});
				});
			});

		if (!bAccepted)
		{
			SetStatus(TEXT("状态：忙碌"));
			AppendAssistantLine(TEXT("已有请求执行中，请等待当前请求完成后再发送。"));
		}
		return FReply::Handled();
	}

	FReply HandleClearClicked()
	{
		HistoryLines.Reset();
		if (HistoryTextBox.IsValid())
		{
			HistoryTextBox->SetText(FText::GetEmpty());
		}
		SaveHistoryToDisk();
		AppendAssistantLine(TEXT("历史已清空。"));
		return FReply::Handled();
	}

	void HandleInputCommitted(const FText&, const ETextCommit::Type CommitType)
	{
		if (CommitType == ETextCommit::OnEnter)
		{
			HandleSendClicked();
		}
	}

	void AppendUserLine(const FString& Text)
	{
		AppendHistoryLine(FString::Printf(TEXT("你：%s"), *Text));
	}

	void AppendAssistantLine(const FString& Text)
	{
		AppendHistoryLine(FString::Printf(TEXT("系统：%s"), *Text));
	}

	void AppendHistoryLine(const FString& Line)
	{
		HistoryLines.Add(Line);
		constexpr int32 MaxLines = 120;
		if (HistoryLines.Num() > MaxLines)
		{
			HistoryLines.RemoveAt(0, HistoryLines.Num() - MaxLines);
		}
		RefreshHistoryText();
		SaveHistoryToDisk();
	}

	void RefreshHistoryText()
	{
		if (!HistoryTextBox.IsValid())
		{
			return;
		}

		FString History;
		for (int32 Index = 0; Index < HistoryLines.Num(); ++Index)
		{
			if (Index > 0)
			{
				History += LINE_TERMINATOR;
			}
			History += HistoryLines[Index];
		}
		HistoryTextBox->SetText(FText::FromString(History));
	}

	void SetStatus(const FString& InStatus)
	{
		if (StatusTextBlock.IsValid())
		{
			StatusTextBlock->SetText(FText::FromString(InStatus));
		}
	}

	int32 LoadHistoryFromDisk()
	{
		FString Error;
		if (!HCI_LoadAgentChatHistory(HistoryLines, Error))
		{
			HistoryLines.Reset();
			AppendAssistantLine(FString::Printf(TEXT("历史加载失败：%s"), *Error));
			return -1;
		}
		return HistoryLines.Num();
	}

	void SaveHistoryToDisk() const
	{
		FString Error;
		if (!HCI_SaveAgentChatHistory(HistoryLines, Error))
		{
			UE_LOG(LogTemp, Warning, TEXT("[HCIAbilityKit][AgentChatUI] save_history_failed reason=%s"), *Error);
		}
	}

	void LoadQuickCommandsFromBundle()
	{
		FString Error;
		if (!HCI_LoadChatQuickCommandsFromBundle(QuickCommands, Error))
		{
			QuickCommands.Reset();
			QuickCommandsLoadError = Error;
		}
		else
		{
			QuickCommandsLoadError.Reset();
		}
	}

	void RebuildQuickCommandButtons()
	{
		if (!QuickCommandsBox.IsValid())
		{
			return;
		}

		QuickCommandsBox->ClearChildren();
		if (QuickCommands.Num() <= 0)
		{
			QuickCommandsBox->AddSlot()
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("未配置")))
			];
			return;
		}

		const TWeakPtr<SHCIAbilityKitAgentChatWindow> WeakWidget(SharedThis(this));
		for (int32 Index = 0; Index < QuickCommands.Num(); ++Index)
		{
			const FHCIAbilityKitChatQuickCommand& Command = QuickCommands[Index];
			const FString PromptText = Command.Prompt;
			QuickCommandsBox->AddSlot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SButton)
				.Text(FText::FromString(Command.Label))
				.OnClicked_Lambda([WeakWidget, PromptText]()
				{
					const TSharedPtr<SHCIAbilityKitAgentChatWindow> Pinned = WeakWidget.Pin();
					if (!Pinned.IsValid())
					{
						return FReply::Handled();
					}
					return Pinned->SubmitUserInput(PromptText);
				})
			];
		}
	}

private:
	FString InitialInput;
	FString QuickCommandsLoadError;
	TArray<FString> HistoryLines;
	TArray<FHCIAbilityKitChatQuickCommand> QuickCommands;
	TSharedPtr<SMultiLineEditableTextBox> HistoryTextBox;
	TSharedPtr<SEditableTextBox> InputTextBox;
	TSharedPtr<STextBlock> StatusTextBlock;
	TSharedPtr<SHorizontalBox> QuickCommandsBox;
};
}

void FHCIAbilityKitAgentChatWindow::OpenWindow(const FString& InitialInput)
{
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(FText::FromString(TEXT("HCIAbilityKit Agent Chat (Stage I7 Draft)")))
		.ClientSize(FVector2D(860.0f, 620.0f))
		.SupportsMaximize(true)
		.SupportsMinimize(true);

	Window->SetContent(
		SNew(SHCIAbilityKitAgentChatWindow)
		.InitialInput(InitialInput));

	FSlateApplication::Get().AddWindow(Window);
}
