#include "UI/HCIAbilityKitAgentChatWindow.h"

#include "Editor.h"
#include "Dom/JsonObject.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Subsystems/HCIAbilityKitAgentSubsystem.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
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

class SHCIAbilityKitAgentChatWindow final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHCIAbilityKitAgentChatWindow)
		: _InitialInput()
	{
	}
		SLATE_ARGUMENT(FString, InitialInput)
	SLATE_END_ARGS()

	~SHCIAbilityKitAgentChatWindow() override
	{
		UnbindSubsystemDelegates();
	}

	void Construct(const FArguments& InArgs)
	{
		InitialInput = InArgs._InitialInput;
		BindSubsystemDelegates();
		RefreshQuickCommandsFromSubsystem();

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
				.AutoHeight()
				.Padding(0.0f, 6.0f, 0.0f, 0.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SAssignNew(ProgressThrobber, SThrobber)
						.Visibility(EVisibility::Collapsed)
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.Padding(8.0f, 0.0f, 0.0f, 0.0f)
					.VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SAssignNew(ProgressTextBlock, STextBlock)
							.Text(FText::FromString(TEXT("进度：未开始")))
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 4.0f, 0.0f, 0.0f)
						[
							SAssignNew(ProgressBar, SProgressBar)
							.Visibility(EVisibility::Collapsed)
						]
					]
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
				.Padding(0.0f, 0.0f, 0.0f, 8.0f)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
					.Padding(8.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("计划 / 审查卡")))
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 4.0f, 0.0f, 4.0f)
						[
							SAssignNew(PlanCardTextBox, SMultiLineEditableTextBox)
							.IsReadOnly(true)
							.AutoWrapText(true)
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SAssignNew(OpenPreviewButton, SButton)
								.Text(FText::FromString(TEXT("调试预览")))
								.IsEnabled(false)
								.OnClicked(this, &SHCIAbilityKitAgentChatWindow::HandleOpenPreviewClicked)
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(8.0f, 0.0f, 0.0f, 0.0f)
							[
								SAssignNew(CancelPlanButton, SButton)
								.Text(FText::FromString(TEXT("取消")))
								.IsEnabled(false)
								.OnClicked(this, &SHCIAbilityKitAgentChatWindow::HandleCancelPendingPlanClicked)
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(8.0f, 0.0f, 0.0f, 0.0f)
							[
								SAssignNew(CommitPlanButton, SButton)
								.Text(FText::FromString(TEXT("确认执行")))
								.IsEnabled(false)
								.OnClicked(this, &SHCIAbilityKitAgentChatWindow::HandleCommitLastPlanClicked)
							]
						]
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 8.0f)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
					.Padding(8.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("结果定位（点击跳转）")))
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 4.0f, 0.0f, 0.0f)
						[
							SAssignNew(ResultTargetsHintText, STextBlock)
							.Text(FText::FromString(TEXT("暂无可定位结果。执行扫描后会在这里显示 Actor/Asset。")))
						]
						+ SVerticalBox::Slot()
						.MaxHeight(140.0f)
						.Padding(0.0f, 4.0f, 0.0f, 0.0f)
						[
							SNew(SScrollBox)
							+ SScrollBox::Slot()
							[
								SAssignNew(ResultTargetsBox, SVerticalBox)
							]
						]
					]
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
			AppendAssistantLine(TEXT("聊天入口已就绪。发送后将走真实 LLM 规划链路，由状态机自动分支执行。"));
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

		if (UHCIAbilityKitAgentSubsystem* AgentSubsystem = GetAgentSubsystem())
		{
			if (AgentSubsystem->IsBusy())
			{
				SetStatus(TEXT("状态：忙碌（已有请求执行中）"));
			}
		}
		RefreshPlanCardFromSubsystem();
		RefreshProgressFromSubsystem();
		RefreshLocateTargetsFromSubsystem();

		if (!InitialInput.TrimStartAndEnd().IsEmpty() && InputTextBox.IsValid())
		{
			InputTextBox->SetText(FText::FromString(InitialInput));
		}
	}

private:
	UHCIAbilityKitAgentSubsystem* GetAgentSubsystem() const
	{
		return GEditor ? GEditor->GetEditorSubsystem<UHCIAbilityKitAgentSubsystem>() : nullptr;
	}

	void BindSubsystemDelegates()
	{
		if (UHCIAbilityKitAgentSubsystem* AgentSubsystem = GetAgentSubsystem())
		{
			ChatLineHandle = AgentSubsystem->OnChatLine.AddSP(this, &SHCIAbilityKitAgentChatWindow::HandleSubsystemChatLine);
			StatusChangedHandle = AgentSubsystem->OnStatusChanged.AddSP(this, &SHCIAbilityKitAgentChatWindow::HandleSubsystemStatusChanged);
			PlanReadyHandle = AgentSubsystem->OnPlanReady.AddSP(this, &SHCIAbilityKitAgentChatWindow::HandleSubsystemPlanReady);
			SessionStateHandle = AgentSubsystem->OnSessionStateChanged.AddSP(this, &SHCIAbilityKitAgentChatWindow::HandleSubsystemSessionStateChanged);
			ProgressStateHandle = AgentSubsystem->OnProgressStateChanged.AddSP(this, &SHCIAbilityKitAgentChatWindow::HandleSubsystemProgressStateChanged);
			LocateTargetsChangedHandle = AgentSubsystem->OnLocateTargetsChanged.AddSP(this, &SHCIAbilityKitAgentChatWindow::HandleSubsystemLocateTargetsChanged);
		}
	}

	void UnbindSubsystemDelegates()
	{
		if (UHCIAbilityKitAgentSubsystem* AgentSubsystem = GetAgentSubsystem())
		{
			if (ChatLineHandle.IsValid())
			{
				AgentSubsystem->OnChatLine.Remove(ChatLineHandle);
				ChatLineHandle.Reset();
			}
			if (StatusChangedHandle.IsValid())
			{
				AgentSubsystem->OnStatusChanged.Remove(StatusChangedHandle);
				StatusChangedHandle.Reset();
			}
			if (PlanReadyHandle.IsValid())
			{
				AgentSubsystem->OnPlanReady.Remove(PlanReadyHandle);
				PlanReadyHandle.Reset();
			}
			if (SessionStateHandle.IsValid())
			{
				AgentSubsystem->OnSessionStateChanged.Remove(SessionStateHandle);
				SessionStateHandle.Reset();
			}
			if (ProgressStateHandle.IsValid())
			{
				AgentSubsystem->OnProgressStateChanged.Remove(ProgressStateHandle);
				ProgressStateHandle.Reset();
			}
			if (LocateTargetsChangedHandle.IsValid())
			{
				AgentSubsystem->OnLocateTargetsChanged.Remove(LocateTargetsChangedHandle);
				LocateTargetsChangedHandle.Reset();
			}
		}
	}

	void RefreshQuickCommandsFromSubsystem()
	{
		QuickCommands.Reset();
		QuickCommandsLoadError.Reset();

		if (UHCIAbilityKitAgentSubsystem* AgentSubsystem = GetAgentSubsystem())
		{
			AgentSubsystem->ReloadQuickCommands();
			QuickCommands = AgentSubsystem->GetQuickCommands();
			QuickCommandsLoadError = AgentSubsystem->GetQuickCommandsLoadError();
			return;
		}

		QuickCommandsLoadError = TEXT("agent_subsystem_unavailable");
	}

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
		if (InputTextBox.IsValid())
		{
			InputTextBox->SetText(FText::GetEmpty());
		}

		if (UHCIAbilityKitAgentSubsystem* AgentSubsystem = GetAgentSubsystem())
		{
			AgentSubsystem->SubmitChatInput(UserInput, TEXT("AgentChatUI"));
		}
		else
		{
			AppendAssistantLine(TEXT("AgentSubsystem 不可用，无法发送请求。"));
			SetStatus(TEXT("状态：空闲（上次失败）"));
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

	void HandleSubsystemChatLine(const FString& Line)
	{
		AppendHistoryLine(Line);
	}

	void HandleSubsystemStatusChanged(const FString& StatusText)
	{
		SetStatus(StatusText);
	}

	void HandleSubsystemSessionStateChanged(EHCIAbilityKitAgentSessionState)
	{
		RefreshPlanCardFromSubsystem();
	}

	void HandleSubsystemProgressStateChanged(const FHCIAbilityKitAgentUiProgressState&)
	{
		RefreshProgressFromSubsystem();
	}

	void HandleSubsystemLocateTargetsChanged()
	{
		RefreshLocateTargetsFromSubsystem();
	}

	void HandleSubsystemPlanReady()
	{
		RefreshPlanCardFromSubsystem();
		AppendAssistantLine(TEXT("计划卡片已更新。只读计划自动执行；写计划会在聊天内显示审查卡并等待确认。"));
	}

	void RefreshProgressFromSubsystem()
	{
		FHCIAbilityKitAgentUiProgressState ProgressState;
		if (UHCIAbilityKitAgentSubsystem* AgentSubsystem = GetAgentSubsystem())
		{
			AgentSubsystem->GetCurrentProgressState(ProgressState);
		}

		if (ProgressTextBlock.IsValid())
		{
			ProgressTextBlock->SetText(FText::FromString(
				ProgressState.Label.IsEmpty() ? TEXT("进度：未开始") : ProgressState.Label));
		}

		if (ProgressBar.IsValid())
		{
			ProgressBar->SetVisibility(ProgressState.bVisible ? EVisibility::Visible : EVisibility::Collapsed);
			if (ProgressState.bIndeterminate)
			{
				ProgressBar->SetPercent(TOptional<float>());
			}
			else
			{
				ProgressBar->SetPercent(ProgressState.Percent01);
			}
		}

		if (ProgressThrobber.IsValid())
		{
			ProgressThrobber->SetVisibility(
				(ProgressState.bVisible && ProgressState.bIndeterminate) ? EVisibility::Visible : EVisibility::Collapsed);
		}
	}

	void RefreshLocateTargetsFromSubsystem()
	{
		ResultLocateTargets.Reset();
		if (UHCIAbilityKitAgentSubsystem* AgentSubsystem = GetAgentSubsystem())
		{
			AgentSubsystem->GetLastExecutionLocateTargets(ResultLocateTargets);
		}

		if (ResultTargetsHintText.IsValid())
		{
			if (ResultLocateTargets.Num() <= 0)
			{
				ResultTargetsHintText->SetText(FText::FromString(TEXT("暂无可定位结果。执行扫描后会在这里显示 Actor/Asset。")));
			}
			else
			{
				ResultTargetsHintText->SetText(FText::FromString(FString::Printf(
					TEXT("共 %d 项。点击按钮可在视口或内容浏览器定位。"),
					ResultLocateTargets.Num())));
			}
		}

		if (!ResultTargetsBox.IsValid())
		{
			return;
		}

		ResultTargetsBox->ClearChildren();
		if (ResultLocateTargets.Num() <= 0)
		{
			ResultTargetsBox->AddSlot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("无结果项")))
			];
			return;
		}

		const int32 MaxButtons = FMath::Min(ResultLocateTargets.Num(), 16);
		for (int32 Index = 0; Index < MaxButtons; ++Index)
		{
			const FString Label = ResultLocateTargets[Index].DisplayLabel;
			ResultTargetsBox->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(SButton)
				.Text(FText::FromString(Label))
				.ToolTipText(FText::FromString(ResultLocateTargets[Index].TargetPath))
				.OnClicked_Lambda([WeakThis = TWeakPtr<SHCIAbilityKitAgentChatWindow>(SharedThis(this)), Index]()
				{
					const TSharedPtr<SHCIAbilityKitAgentChatWindow> Pinned = WeakThis.Pin();
					if (!Pinned.IsValid())
					{
						return FReply::Handled();
					}
					return Pinned->HandleLocateResultTargetClicked(Index);
				})
			];
		}

		if (ResultLocateTargets.Num() > MaxButtons)
		{
			ResultTargetsBox->AddSlot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("仅显示前 %d 项（总计 %d 项）"), MaxButtons, ResultLocateTargets.Num())))
			];
		}
	}

	void RefreshPlanCardFromSubsystem()
	{
		TArray<FString> CardLines;
		bool bHasPlan = false;
		if (UHCIAbilityKitAgentSubsystem* AgentSubsystem = GetAgentSubsystem())
		{
			bHasPlan = AgentSubsystem->BuildLastPlanCardLines(CardLines);
		}

		if (PlanCardTextBox.IsValid())
		{
			if (!bHasPlan || CardLines.Num() <= 0)
			{
				PlanCardTextBox->SetText(FText::FromString(TEXT("暂无计划。发送请求后将展示状态、意图和步骤摘要。")));
			}
			else
			{
				FString CardText;
				for (int32 Index = 0; Index < CardLines.Num(); ++Index)
				{
					if (Index > 0)
					{
						CardText += LINE_TERMINATOR;
					}
					CardText += CardLines[Index];
				}
				PlanCardTextBox->SetText(FText::FromString(CardText));
			}
		}

		if (OpenPreviewButton.IsValid())
		{
			OpenPreviewButton->SetEnabled(bHasPlan);
		}
		if (CommitPlanButton.IsValid())
		{
			bool bCanCommit = false;
			bool bCanCancel = false;
			if (UHCIAbilityKitAgentSubsystem* AgentSubsystem = GetAgentSubsystem())
			{
				bCanCommit = AgentSubsystem->CanCommitLastPlanFromChat();
				bCanCancel = AgentSubsystem->CanCancelPendingPlanFromChat();
			}
			CommitPlanButton->SetEnabled(bHasPlan && bCanCommit);
			if (CancelPlanButton.IsValid())
			{
				CancelPlanButton->SetEnabled(bHasPlan && bCanCancel);
			}
		}
		else if (CancelPlanButton.IsValid())
		{
			CancelPlanButton->SetEnabled(false);
		}
	}

	FReply HandleOpenPreviewClicked()
	{
		if (UHCIAbilityKitAgentSubsystem* AgentSubsystem = GetAgentSubsystem())
		{
			AgentSubsystem->OpenLastPlanPreview();
		}
		return FReply::Handled();
	}

	FReply HandleCommitLastPlanClicked()
	{
		if (UHCIAbilityKitAgentSubsystem* AgentSubsystem = GetAgentSubsystem())
		{
			AgentSubsystem->CommitLastPlanFromChat();
		}
		return FReply::Handled();
	}

	FReply HandleCancelPendingPlanClicked()
	{
		if (UHCIAbilityKitAgentSubsystem* AgentSubsystem = GetAgentSubsystem())
		{
			AgentSubsystem->CancelPendingPlanFromChat();
		}
		return FReply::Handled();
	}

	FReply HandleLocateResultTargetClicked(const int32 TargetIndex)
	{
		if (UHCIAbilityKitAgentSubsystem* AgentSubsystem = GetAgentSubsystem())
		{
			AgentSubsystem->TryLocateLastExecutionTargetByIndex(TargetIndex);
		}
		return FReply::Handled();
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
		for (const FHCIAbilityKitAgentQuickCommand& Command : QuickCommands)
		{
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
	TArray<FHCIAbilityKitAgentQuickCommand> QuickCommands;
	TArray<FHCIAbilityKitAgentUiLocateTarget> ResultLocateTargets;
	TSharedPtr<SMultiLineEditableTextBox> HistoryTextBox;
	TSharedPtr<SMultiLineEditableTextBox> PlanCardTextBox;
	TSharedPtr<SEditableTextBox> InputTextBox;
	TSharedPtr<STextBlock> StatusTextBlock;
	TSharedPtr<STextBlock> ProgressTextBlock;
	TSharedPtr<STextBlock> ResultTargetsHintText;
	TSharedPtr<SProgressBar> ProgressBar;
	TSharedPtr<SThrobber> ProgressThrobber;
	TSharedPtr<SVerticalBox> ResultTargetsBox;
	TSharedPtr<SHorizontalBox> QuickCommandsBox;
	TSharedPtr<SButton> OpenPreviewButton;
	TSharedPtr<SButton> CancelPlanButton;
	TSharedPtr<SButton> CommitPlanButton;
	FDelegateHandle ChatLineHandle;
	FDelegateHandle StatusChangedHandle;
	FDelegateHandle PlanReadyHandle;
	FDelegateHandle SessionStateHandle;
	FDelegateHandle ProgressStateHandle;
	FDelegateHandle LocateTargetsChangedHandle;
};
}

void FHCIAbilityKitAgentChatWindow::OpenWindow(const FString& InitialInput)
{
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(FText::FromString(TEXT("HCIAbilityKit Agent Chat (Stage I8 Draft)")))
		.ClientSize(FVector2D(860.0f, 620.0f))
		.SupportsMaximize(true)
		.SupportsMinimize(true);

	Window->SetContent(
		SNew(SHCIAbilityKitAgentChatWindow)
		.InitialInput(InitialInput));

	FSlateApplication::Get().AddWindow(Window);
}
