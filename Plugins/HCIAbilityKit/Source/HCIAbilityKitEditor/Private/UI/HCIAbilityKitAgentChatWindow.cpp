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
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"
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

enum class EHCIAbilityKitChatBubbleRole : uint8
{
	User,
	Assistant
};

enum class EHCIAbilityKitChatBubbleKind : uint8
{
	Text,
	Thinking,
	ReviewCard,
	LocateTargets
};

struct FHCIAbilityKitChatBubbleMessage
{
	EHCIAbilityKitChatBubbleRole Role = EHCIAbilityKitChatBubbleRole::Assistant;
	EHCIAbilityKitChatBubbleKind Kind = EHCIAbilityKitChatBubbleKind::Text;
	FString Text;
	FString SubText;
	bool bShowThrobber = false;
	bool bShowReviewActions = false;
	bool bShowPreviewButton = false;
	TArray<FString> ReviewLines;
	TArray<FHCIAbilityKitAgentUiLocateTarget> LocateTargets;
};

static bool HCI_ParseChatHistoryLine(const FString& Line, EHCIAbilityKitChatBubbleRole& OutRole, FString& OutText)
{
	if (Line.StartsWith(TEXT("你：")))
	{
		OutRole = EHCIAbilityKitChatBubbleRole::User;
		OutText = Line.RightChop(2);
		return true;
	}
	if (Line.StartsWith(TEXT("系统：")))
	{
		OutRole = EHCIAbilityKitChatBubbleRole::Assistant;
		OutText = Line.RightChop(3);
		return true;
	}

	OutRole = EHCIAbilityKitChatBubbleRole::Assistant;
	OutText = Line;
	return true;
}

static FString HCI_FormatHistoryLine(const EHCIAbilityKitChatBubbleRole Role, const FString& Text)
{
	return (Role == EHCIAbilityKitChatBubbleRole::User)
		? FString::Printf(TEXT("你：%s"), *Text)
		: FString::Printf(TEXT("系统：%s"), *Text);
}

static bool HCI_ShouldSuppressSystemChatLine(const FString& Text)
{
	const FString Trimmed = Text.TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		return true;
	}

	if (Trimmed.StartsWith(TEXT("已发送，等待规划结果")) ||
		Trimmed.StartsWith(TEXT("计划卡片已更新")) ||
		Trimmed.StartsWith(TEXT("检测到只读计划")) ||
		Trimmed.StartsWith(TEXT("计划包含写操作")))
	{
		return true;
	}

	if (Trimmed.StartsWith(TEXT("DryRun:")) || Trimmed.StartsWith(TEXT("Commit:")))
	{
		return true;
	}

	if (Trimmed.Contains(TEXT("可定位结果项，可在结果面板点击定位")))
	{
		return true;
	}

	return false;
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
				[ SNew(STextBlock).Text(FText::FromString(TEXT("HCIAbilityKit Agent Chat"))) ]
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(0.0f, 8.0f, 0.0f, 8.0f)
				[
					SAssignNew(ChatScrollBox, SScrollBox)
					+ SScrollBox::Slot()
					[
						SAssignNew(ChatMessagesBox, SVerticalBox)
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 8.0f)
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
						.HintText(FText::FromString(TEXT("输入自然语言指令，例如：检查一下 /Game/HCI 目录下的模型面数")))
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
			AppendPersistentTextBubble(EHCIAbilityKitChatBubbleRole::Assistant, TEXT("聊天入口已就绪。你可以直接问问题、扫描目录、检查风险；只读操作会在后台自动完成。"));
		}
		else if (!QuickCommandsLoadError.IsEmpty())
		{
			AppendPersistentTextBubble(EHCIAbilityKitChatBubbleRole::Assistant, FString::Printf(TEXT("快捷指令加载失败：%s"), *QuickCommandsLoadError));
		}

		if (!InitialInput.TrimStartAndEnd().IsEmpty() && InputTextBox.IsValid())
		{
			InputTextBox->SetText(FText::FromString(InitialInput));
		}

		RebuildChatListWidgets();
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
			SummaryHandle = AgentSubsystem->OnSummaryReceived.AddSP(this, &SHCIAbilityKitAgentChatWindow::HandleSubsystemSummaryReceived);
			PlanReadyHandle = AgentSubsystem->OnPlanReady.AddSP(this, &SHCIAbilityKitAgentChatWindow::HandleSubsystemPlanReady);
			SessionStateHandle = AgentSubsystem->OnSessionStateChanged.AddSP(this, &SHCIAbilityKitAgentChatWindow::HandleSubsystemSessionStateChanged);
			ProgressStateHandle = AgentSubsystem->OnProgressStateChanged.AddSP(this, &SHCIAbilityKitAgentChatWindow::HandleSubsystemProgressStateChanged);
			LocateTargetsChangedHandle = AgentSubsystem->OnLocateTargetsChanged.AddSP(this, &SHCIAbilityKitAgentChatWindow::HandleSubsystemLocateTargetsChanged);
			ActivityHintHandle = AgentSubsystem->OnActivityHintChanged.AddSP(this, &SHCIAbilityKitAgentChatWindow::HandleSubsystemActivityHintChanged);
		}
	}

	void UnbindSubsystemDelegates()
	{
		if (UHCIAbilityKitAgentSubsystem* AgentSubsystem = GetAgentSubsystem())
		{
			if (ChatLineHandle.IsValid()) { AgentSubsystem->OnChatLine.Remove(ChatLineHandle); ChatLineHandle.Reset(); }
			if (StatusChangedHandle.IsValid()) { AgentSubsystem->OnStatusChanged.Remove(StatusChangedHandle); StatusChangedHandle.Reset(); }
			if (SummaryHandle.IsValid()) { AgentSubsystem->OnSummaryReceived.Remove(SummaryHandle); SummaryHandle.Reset(); }
			if (PlanReadyHandle.IsValid()) { AgentSubsystem->OnPlanReady.Remove(PlanReadyHandle); PlanReadyHandle.Reset(); }
			if (SessionStateHandle.IsValid()) { AgentSubsystem->OnSessionStateChanged.Remove(SessionStateHandle); SessionStateHandle.Reset(); }
			if (ProgressStateHandle.IsValid()) { AgentSubsystem->OnProgressStateChanged.Remove(ProgressStateHandle); ProgressStateHandle.Reset(); }
			if (LocateTargetsChangedHandle.IsValid()) { AgentSubsystem->OnLocateTargetsChanged.Remove(LocateTargetsChangedHandle); LocateTargetsChangedHandle.Reset(); }
			if (ActivityHintHandle.IsValid()) { AgentSubsystem->OnActivityHintChanged.Remove(ActivityHintHandle); ActivityHintHandle.Reset(); }
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
		return SubmitUserInput(InputTextBox->GetText().ToString().TrimStartAndEnd());
	}

	FReply SubmitUserInput(const FString& UserInput)
	{
		const FString Trimmed = UserInput.TrimStartAndEnd();
		if (InputTextBox.IsValid())
		{
			InputTextBox->SetText(FText::GetEmpty());
		}
		if (Trimmed.IsEmpty())
		{
			return FReply::Handled();
		}

		BeginNewRequestUiSession();
		AppendPersistentTextBubble(EHCIAbilityKitChatBubbleRole::User, Trimmed);
		ActiveThinkingBubbleIndex = AppendThinkingBubble(TEXT("思考中..."), TEXT(""));

		bool bSubmitted = false;
		if (UHCIAbilityKitAgentSubsystem* AgentSubsystem = GetAgentSubsystem())
		{
			bSubmitted = AgentSubsystem->SubmitChatInput(Trimmed, TEXT("AgentChatUI"));
		}
		if (!bSubmitted)
		{
			FinalizeThinkingBubbleWithText(TEXT("发送失败：AgentSubsystem 不可用或当前忙碌。"));
		}

		return FReply::Handled();
	}

	FReply HandleClearClicked()
	{
		HistoryLines.Reset();
		ChatMessages.Reset();
		ActiveThinkingBubbleIndex = INDEX_NONE;
		CurrentRequestReviewBubbleIndex = INDEX_NONE;
		CurrentRequestLocateBubbleIndex = INDEX_NONE;
		CachedSummaryText.Reset();
		CachedStatusText.Reset();
		SaveHistoryToDisk();
		AppendPersistentTextBubble(EHCIAbilityKitChatBubbleRole::Assistant, TEXT("聊天历史已清空。"));
		return FReply::Handled();
	}

	void HandleInputCommitted(const FText&, const ETextCommit::Type CommitType)
	{
		if (CommitType == ETextCommit::OnEnter)
		{
			HandleSendClicked();
		}
	}

	void HandleSubsystemChatLine(const FString& RawLine)
	{
		EHCIAbilityKitChatBubbleRole Role = EHCIAbilityKitChatBubbleRole::Assistant;
		FString Text;
		if (!HCI_ParseChatHistoryLine(RawLine, Role, Text))
		{
			return;
		}

		Text.TrimStartAndEndInline();
		if (Role == EHCIAbilityKitChatBubbleRole::User)
		{
			return; // UI 已自行插入用户气泡
		}

		if (HCI_ShouldSuppressSystemChatLine(Text))
		{
			return;
		}

		if (CachedSummaryText == Text)
		{
			return; // 避免和 OnSummaryReceived 重复展示
		}

		if (ChatMessages.IsValidIndex(ActiveThinkingBubbleIndex))
		{
			BufferedAssistantLineForActiveRequest = Text;
			return;
		}

		AppendPersistentTextBubble(EHCIAbilityKitChatBubbleRole::Assistant, Text);
	}

	void HandleSubsystemStatusChanged(const FString& StatusText)
	{
		CachedStatusText = StatusText;
		RefreshThinkingBubbleFromSubsystem();
	}

	void HandleSubsystemSummaryReceived(const FString& SummaryText)
	{
		CachedSummaryText = SummaryText.TrimStartAndEnd();
		TryFinalizeThinkingBubbleIfReady();
	}

	void HandleSubsystemPlanReady()
	{
		RefreshThinkingBubbleFromSubsystem();
	}

	void HandleSubsystemSessionStateChanged(EHCIAbilityKitAgentSessionState)
	{
		RefreshThinkingBubbleFromSubsystem();
		RefreshReviewCardBubbleFromSubsystem();
		TryFinalizeThinkingBubbleIfReady();
	}

	void HandleSubsystemProgressStateChanged(const FHCIAbilityKitAgentUiProgressState&)
	{
		RefreshThinkingBubbleFromSubsystem();
	}

	void HandleSubsystemLocateTargetsChanged()
	{
		RefreshLocateTargetsBubbleFromSubsystem();
	}

	void HandleSubsystemActivityHintChanged(const FString&)
	{
		RefreshThinkingBubbleFromSubsystem();
	}

	void BeginNewRequestUiSession()
	{
		ActiveThinkingBubbleIndex = INDEX_NONE;
		CurrentRequestReviewBubbleIndex = INDEX_NONE;
		CurrentRequestLocateBubbleIndex = INDEX_NONE;
		CachedSummaryText.Reset();
		CachedStatusText.Reset();
		BufferedAssistantLineForActiveRequest.Reset();
	}

	int32 AppendThinkingBubble(const FString& Text, const FString& SubText)
	{
		FHCIAbilityKitChatBubbleMessage Msg;
		Msg.Role = EHCIAbilityKitChatBubbleRole::Assistant;
		Msg.Kind = EHCIAbilityKitChatBubbleKind::Thinking;
		Msg.Text = Text;
		Msg.SubText = SubText;
		Msg.bShowThrobber = true;
		ChatMessages.Add(MoveTemp(Msg));
		RebuildChatListWidgets();
		return ChatMessages.Num() - 1;
	}

	int32 AppendPersistentTextBubble(const EHCIAbilityKitChatBubbleRole Role, const FString& Text)
	{
		FHCIAbilityKitChatBubbleMessage Msg;
		Msg.Role = Role;
		Msg.Kind = EHCIAbilityKitChatBubbleKind::Text;
		Msg.Text = Text;
		ChatMessages.Add(MoveTemp(Msg));

		HistoryLines.Add(HCI_FormatHistoryLine(Role, Text));
		const int32 MaxLines = 120;
		if (HistoryLines.Num() > MaxLines)
		{
			HistoryLines.RemoveAt(0, HistoryLines.Num() - MaxLines);
		}
		SaveHistoryToDisk();
		RebuildChatListWidgets();
		return ChatMessages.Num() - 1;
	}

	void FinalizeThinkingBubbleWithText(const FString& Text)
	{
		if (!ChatMessages.IsValidIndex(ActiveThinkingBubbleIndex))
		{
			AppendPersistentTextBubble(EHCIAbilityKitChatBubbleRole::Assistant, Text);
			return;
		}

		FHCIAbilityKitChatBubbleMessage& Msg = ChatMessages[ActiveThinkingBubbleIndex];
		Msg.Kind = EHCIAbilityKitChatBubbleKind::Text;
		Msg.Role = EHCIAbilityKitChatBubbleRole::Assistant;
		Msg.Text = Text;
		Msg.SubText.Reset();
		Msg.bShowThrobber = false;
		Msg.bShowReviewActions = false;
		Msg.bShowPreviewButton = false;
		Msg.ReviewLines.Reset();
		Msg.LocateTargets.Reset();
		ActiveThinkingBubbleIndex = INDEX_NONE;

		HistoryLines.Add(HCI_FormatHistoryLine(EHCIAbilityKitChatBubbleRole::Assistant, Text));
		const int32 MaxLines = 120;
		if (HistoryLines.Num() > MaxLines)
		{
			HistoryLines.RemoveAt(0, HistoryLines.Num() - MaxLines);
		}
		SaveHistoryToDisk();
		RebuildChatListWidgets();
	}

	void RefreshThinkingBubbleFromSubsystem()
	{
		if (!ChatMessages.IsValidIndex(ActiveThinkingBubbleIndex))
		{
			return;
		}

		FString ActivityHint;
		FString ProgressLabel;
		EHCIAbilityKitAgentSessionState State = EHCIAbilityKitAgentSessionState::Idle;
		bool bHasSubsystem = false;
		if (UHCIAbilityKitAgentSubsystem* AgentSubsystem = GetAgentSubsystem())
		{
			bHasSubsystem = true;
			AgentSubsystem->GetCurrentActivityHint(ActivityHint);
			FHCIAbilityKitAgentUiProgressState ProgressState;
			AgentSubsystem->GetCurrentProgressState(ProgressState);
			ProgressLabel = ProgressState.Label;
			State = AgentSubsystem->GetCurrentState();
		}

		FHCIAbilityKitChatBubbleMessage& Msg = ChatMessages[ActiveThinkingBubbleIndex];
		if (Msg.Kind != EHCIAbilityKitChatBubbleKind::Thinking && Msg.Kind != EHCIAbilityKitChatBubbleKind::Text)
		{
			return;
		}

		const FString NewMainText = ActivityHint.IsEmpty() ? TEXT("思考中...") : ActivityHint;
		Msg.Text = NewMainText;
		Msg.SubText = ProgressLabel;

		const bool bActiveThinkingState = bHasSubsystem && (
			State == EHCIAbilityKitAgentSessionState::Thinking ||
			State == EHCIAbilityKitAgentSessionState::PlanReady ||
			State == EHCIAbilityKitAgentSessionState::AutoExecuteReadOnly ||
			State == EHCIAbilityKitAgentSessionState::Executing ||
			State == EHCIAbilityKitAgentSessionState::Summarizing);

		Msg.Kind = EHCIAbilityKitChatBubbleKind::Thinking;
		Msg.bShowThrobber = bActiveThinkingState;
		RebuildChatListWidgets();
	}

	void TryFinalizeThinkingBubbleIfReady()
	{
		if (!ChatMessages.IsValidIndex(ActiveThinkingBubbleIndex))
		{
			return;
		}
		if (CachedSummaryText.IsEmpty())
		{
			if (UHCIAbilityKitAgentSubsystem* AgentSubsystem = GetAgentSubsystem())
			{
				const EHCIAbilityKitAgentSessionState State = AgentSubsystem->GetCurrentState();
				if ((State == EHCIAbilityKitAgentSessionState::Failed || State == EHCIAbilityKitAgentSessionState::Cancelled) &&
					!BufferedAssistantLineForActiveRequest.IsEmpty())
				{
					FinalizeThinkingBubbleWithText(BufferedAssistantLineForActiveRequest);
					BufferedAssistantLineForActiveRequest.Reset();
				}
			}
			return;
		}

		bool bReadyToFinalize = false;
		if (UHCIAbilityKitAgentSubsystem* AgentSubsystem = GetAgentSubsystem())
		{
			const EHCIAbilityKitAgentSessionState State = AgentSubsystem->GetCurrentState();
			const bool bHasPlan = AgentSubsystem->HasLastPlan();
			bReadyToFinalize = !bHasPlan ||
				State == EHCIAbilityKitAgentSessionState::AwaitUserConfirm ||
				State == EHCIAbilityKitAgentSessionState::Completed ||
				State == EHCIAbilityKitAgentSessionState::Failed ||
				State == EHCIAbilityKitAgentSessionState::Cancelled;
		}
		else
		{
			bReadyToFinalize = true;
		}

		if (!bReadyToFinalize)
		{
			return;
		}

		FinalizeThinkingBubbleWithText(CachedSummaryText);
		CachedSummaryText.Reset();
		BufferedAssistantLineForActiveRequest.Reset();
	}

	void RefreshReviewCardBubbleFromSubsystem()
	{
		UHCIAbilityKitAgentSubsystem* AgentSubsystem = GetAgentSubsystem();
		if (!AgentSubsystem)
		{
			return;
		}

		if (!AgentSubsystem->CanCommitLastPlanFromChat() && !AgentSubsystem->CanCancelPendingPlanFromChat())
		{
			return;
		}

		TArray<FString> CardLines;
		if (!AgentSubsystem->BuildLastPlanCardLines(CardLines) || CardLines.Num() <= 0)
		{
			return;
		}

		FHCIAbilityKitChatBubbleMessage* TargetMsg = nullptr;
		if (ChatMessages.IsValidIndex(CurrentRequestReviewBubbleIndex))
		{
			TargetMsg = &ChatMessages[CurrentRequestReviewBubbleIndex];
		}
		else
		{
			FHCIAbilityKitChatBubbleMessage Msg;
			Msg.Role = EHCIAbilityKitChatBubbleRole::Assistant;
			Msg.Kind = EHCIAbilityKitChatBubbleKind::ReviewCard;
			ChatMessages.Add(MoveTemp(Msg));
			CurrentRequestReviewBubbleIndex = ChatMessages.Num() - 1;
			TargetMsg = &ChatMessages[CurrentRequestReviewBubbleIndex];
		}

		TargetMsg->Role = EHCIAbilityKitChatBubbleRole::Assistant;
		TargetMsg->Kind = EHCIAbilityKitChatBubbleKind::ReviewCard;
		TargetMsg->Text = TEXT("待确认执行（写操作）");
		TargetMsg->ReviewLines = CardLines;
		TargetMsg->bShowReviewActions = true;
		TargetMsg->bShowPreviewButton = true;
		RebuildChatListWidgets();
	}

	void RefreshLocateTargetsBubbleFromSubsystem()
	{
		UHCIAbilityKitAgentSubsystem* AgentSubsystem = GetAgentSubsystem();
		if (!AgentSubsystem)
		{
			return;
		}

		TArray<FHCIAbilityKitAgentUiLocateTarget> Targets;
		AgentSubsystem->GetLastExecutionLocateTargets(Targets);
		if (Targets.Num() <= 0)
		{
			return;
		}

		FHCIAbilityKitChatBubbleMessage* TargetMsg = nullptr;
		if (ChatMessages.IsValidIndex(CurrentRequestLocateBubbleIndex))
		{
			TargetMsg = &ChatMessages[CurrentRequestLocateBubbleIndex];
		}
		else
		{
			FHCIAbilityKitChatBubbleMessage Msg;
			Msg.Role = EHCIAbilityKitChatBubbleRole::Assistant;
			Msg.Kind = EHCIAbilityKitChatBubbleKind::LocateTargets;
			ChatMessages.Add(MoveTemp(Msg));
			CurrentRequestLocateBubbleIndex = ChatMessages.Num() - 1;
			TargetMsg = &ChatMessages[CurrentRequestLocateBubbleIndex];
		}

		TargetMsg->Text = FString::Printf(TEXT("结果定位（%d 项，可点击跳转）"), Targets.Num());
		TargetMsg->LocateTargets = MoveTemp(Targets);
		RebuildChatListWidgets();
	}

	TSharedRef<SWidget> BuildBubbleWidget(const int32 MessageIndex)
	{
		const FHCIAbilityKitChatBubbleMessage& Msg = ChatMessages[MessageIndex];
		const bool bIsUser = Msg.Role == EHCIAbilityKitChatBubbleRole::User;
		const FLinearColor BubbleColor = bIsUser
			? FLinearColor(0.14f, 0.38f, 0.93f, 1.0f)
			: FLinearColor(0.17f, 0.17f, 0.19f, 1.0f);

		TSharedRef<SVerticalBox> ContentBox = SNew(SVerticalBox);

		if (Msg.Kind == EHCIAbilityKitChatBubbleKind::Thinking)
		{
			ContentBox->AddSlot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SThrobber)
					.Visibility(Msg.bShowThrobber ? EVisibility::Visible : EVisibility::Collapsed)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Msg.Text))
					.AutoWrapText(true)
					.WrapTextAt(560.0f)
					.ColorAndOpacity(FSlateColor(FLinearColor::White))
				]
			];

			if (!Msg.SubText.IsEmpty())
			{
				ContentBox->AddSlot()
				.AutoHeight()
				.Padding(0.0f, 6.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Msg.SubText))
					.AutoWrapText(true)
					.WrapTextAt(560.0f)
					.ColorAndOpacity(FSlateColor(FLinearColor(0.82f, 0.82f, 0.86f, 1.0f)))
				];
			}
		}
		else if (Msg.Kind == EHCIAbilityKitChatBubbleKind::ReviewCard)
		{
			ContentBox->AddSlot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(Msg.Text))
				.AutoWrapText(true)
				.WrapTextAt(560.0f)
				.ColorAndOpacity(FSlateColor(FLinearColor::White))
			];

			for (int32 LineIndex = 0; LineIndex < Msg.ReviewLines.Num(); ++LineIndex)
			{
				ContentBox->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Msg.ReviewLines[LineIndex]))
					.AutoWrapText(true)
					.WrapTextAt(560.0f)
					.ColorAndOpacity(FSlateColor(FLinearColor(0.90f, 0.90f, 0.92f, 1.0f)))
				];
			}

			if (Msg.bShowReviewActions)
			{
				ContentBox->AddSlot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.Text(FText::FromString(TEXT("取消")))
						.OnClicked(this, &SHCIAbilityKitAgentChatWindow::HandleCancelPendingPlanClicked)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.Text(FText::FromString(TEXT("确认执行")))
						.OnClicked(this, &SHCIAbilityKitAgentChatWindow::HandleCommitLastPlanClicked)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.Visibility(Msg.bShowPreviewButton ? EVisibility::Visible : EVisibility::Collapsed)
						.Text(FText::FromString(TEXT("调试预览")))
						.OnClicked(this, &SHCIAbilityKitAgentChatWindow::HandleOpenPreviewClicked)
					]
				];
			}
		}
		else if (Msg.Kind == EHCIAbilityKitChatBubbleKind::LocateTargets)
		{
			ContentBox->AddSlot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(Msg.Text))
				.AutoWrapText(true)
				.WrapTextAt(560.0f)
				.ColorAndOpacity(FSlateColor(FLinearColor::White))
			];

			const int32 MaxButtons = FMath::Min(Msg.LocateTargets.Num(), 12);
			for (int32 Index = 0; Index < MaxButtons; ++Index)
			{
				const FHCIAbilityKitAgentUiLocateTarget& Target = Msg.LocateTargets[Index];
				ContentBox->AddSlot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.Text(FText::FromString(Target.DisplayLabel))
					.ToolTipText(FText::FromString(Target.TargetPath))
					.OnClicked_Lambda([this, Index]()
					{
						return HandleLocateResultTargetClicked(Index);
					})
				];
			}

			if (Msg.LocateTargets.Num() > MaxButtons)
			{
				ContentBox->AddSlot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(TEXT("仅显示前 %d 项（总计 %d 项）"), MaxButtons, Msg.LocateTargets.Num())))
					.AutoWrapText(true)
					.WrapTextAt(560.0f)
					.ColorAndOpacity(FSlateColor(FLinearColor(0.82f, 0.82f, 0.86f, 1.0f)))
				];
			}
		}
		else
		{
			ContentBox->AddSlot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(Msg.Text))
				.AutoWrapText(true)
				.WrapTextAt(560.0f)
				.ColorAndOpacity(FSlateColor(FLinearColor::White))
			];

			if (!Msg.SubText.IsEmpty())
			{
				ContentBox->AddSlot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Msg.SubText))
					.AutoWrapText(true)
					.WrapTextAt(560.0f)
					.ColorAndOpacity(FSlateColor(FLinearColor(0.82f, 0.82f, 0.86f, 1.0f)))
				];
			}
		}

		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(bIsUser ? HAlign_Right : HAlign_Left)
			.FillWidth(1.0f)
			[
				SNew(SBox)
				.MaxDesiredWidth(620.0f)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
					.BorderBackgroundColor(BubbleColor)
					.Padding(FMargin(12.0f, 10.0f))
					[
						ContentBox
					]
				]
			];
	}

	void RebuildChatListWidgets()
	{
		if (!ChatMessagesBox.IsValid())
		{
			return;
		}

		ChatMessagesBox->ClearChildren();
		for (int32 Index = 0; Index < ChatMessages.Num(); ++Index)
		{
			ChatMessagesBox->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				BuildBubbleWidget(Index)
			];
		}

		if (ChatScrollBox.IsValid())
		{
			ChatScrollBox->ScrollToEnd();
		}
	}

	int32 LoadHistoryFromDisk()
	{
		FString Error;
		if (!HCI_LoadAgentChatHistory(HistoryLines, Error))
		{
			HistoryLines.Reset();
			AppendPersistentTextBubble(EHCIAbilityKitChatBubbleRole::Assistant, FString::Printf(TEXT("历史加载失败：%s"), *Error));
			return -1;
		}

		for (const FString& Line : HistoryLines)
		{
			EHCIAbilityKitChatBubbleRole Role;
			FString Text;
			HCI_ParseChatHistoryLine(Line, Role, Text);
			FHCIAbilityKitChatBubbleMessage Msg;
			Msg.Role = Role;
			Msg.Kind = EHCIAbilityKitChatBubbleKind::Text;
			Msg.Text = Text;
			ChatMessages.Add(MoveTemp(Msg));
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
			QuickCommandsBox->AddSlot().AutoWidth()
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("未配置")))
			];
			return;
		}

		for (const FHCIAbilityKitAgentQuickCommand& Command : QuickCommands)
		{
			const FString PromptText = Command.Prompt;
			QuickCommandsBox->AddSlot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SButton)
				.Text(FText::FromString(Command.Label))
				.OnClicked_Lambda([this, PromptText]()
				{
					return SubmitUserInput(PromptText);
				})
			];
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

private:
	FString InitialInput;
	FString QuickCommandsLoadError;
	FString CachedSummaryText;
	FString CachedStatusText;
	FString BufferedAssistantLineForActiveRequest;
	TArray<FString> HistoryLines;
	TArray<FHCIAbilityKitAgentQuickCommand> QuickCommands;
	TArray<FHCIAbilityKitChatBubbleMessage> ChatMessages;
	int32 ActiveThinkingBubbleIndex = INDEX_NONE;
	int32 CurrentRequestReviewBubbleIndex = INDEX_NONE;
	int32 CurrentRequestLocateBubbleIndex = INDEX_NONE;

	TSharedPtr<SScrollBox> ChatScrollBox;
	TSharedPtr<SVerticalBox> ChatMessagesBox;
	TSharedPtr<SEditableTextBox> InputTextBox;
	TSharedPtr<SHorizontalBox> QuickCommandsBox;

	FDelegateHandle ChatLineHandle;
	FDelegateHandle StatusChangedHandle;
	FDelegateHandle SummaryHandle;
	FDelegateHandle PlanReadyHandle;
	FDelegateHandle SessionStateHandle;
	FDelegateHandle ProgressStateHandle;
	FDelegateHandle LocateTargetsChangedHandle;
	FDelegateHandle ActivityHintHandle;
};
}

void FHCIAbilityKitAgentChatWindow::OpenWindow(const FString& InitialInput)
{
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(FText::FromString(TEXT("HCIAbilityKit Agent Chat")))
		.ClientSize(FVector2D(920.0f, 700.0f))
		.SupportsMaximize(true)
		.SupportsMinimize(true);

	Window->SetContent(
		SNew(SHCIAbilityKitAgentChatWindow)
		.InitialInput(InitialInput));

	FSlateApplication::Get().AddWindow(Window);
}
