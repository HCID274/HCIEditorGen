#include "UI/HCIAbilityKitAgentChatWindow.h"

#include "Agent/HCIAbilityKitAgentPlan.h"
#include "Agent/HCIAbilityKitAgentPlanner.h"
#include "Async/Async.h"
#include "Commands/HCIAbilityKitAgentCommandHandlers.h"
#include "Framework/Application/SlateApplication.h"
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

		AppendAssistantLine(TEXT("聊天入口已就绪。发送后将走真实 LLM 规划链路，并自动弹出 PlanPreview。"));
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
		if (UserInput.IsEmpty())
		{
			AppendAssistantLine(TEXT("输入为空，未发送。"));
			return FReply::Handled();
		}

		InputTextBox->SetText(FText::GetEmpty());
		AppendUserLine(UserInput);
		AppendAssistantLine(TEXT("已发送，等待规划结果..."));
		SetStatus(TEXT("状态：发送中"));

		const TWeakPtr<SHCIAbilityKitAgentChatWindow> WeakWidget(SharedThis(this));
		const bool bAccepted = HCI_RequestAgentPlanPreviewFromUi(
			UserInput,
			TEXT("AgentChatUI"),
			[WeakWidget](const bool bSuccess,
			           const FString& RequestText,
			           const FHCIAbilityKitAgentPlan& Plan,
			           const FString& RouteReason,
			           const FHCIAbilityKitAgentPlannerResultMetadata& PlannerMetadata,
			           const FString& Error)
			{
				const FString ResultLine = bSuccess
					? FString::Printf(
						TEXT("完成：intent=%s route_reason=%s steps=%d provider=%s fallback=%s"),
						Plan.Intent.IsEmpty() ? TEXT("-") : *Plan.Intent,
						RouteReason.IsEmpty() ? TEXT("-") : *RouteReason,
						Plan.Steps.Num(),
						PlannerMetadata.PlannerProvider.IsEmpty() ? TEXT("-") : *PlannerMetadata.PlannerProvider,
						PlannerMetadata.bFallbackUsed ? *PlannerMetadata.FallbackReason : TEXT("none"))
					: FString::Printf(
						TEXT("失败：input=%s reason=%s provider=%s fallback=%s error_code=%s"),
						RequestText.IsEmpty() ? TEXT("-") : *RequestText,
						Error.IsEmpty() ? TEXT("-") : *Error,
						PlannerMetadata.PlannerProvider.IsEmpty() ? TEXT("-") : *PlannerMetadata.PlannerProvider,
						PlannerMetadata.FallbackReason.IsEmpty() ? TEXT("-") : *PlannerMetadata.FallbackReason,
						PlannerMetadata.ErrorCode.IsEmpty() ? TEXT("-") : *PlannerMetadata.ErrorCode);

				AsyncTask(ENamedThreads::GameThread, [WeakWidget, bSuccess, ResultLine]()
				{
					const TSharedPtr<SHCIAbilityKitAgentChatWindow> Pinned = WeakWidget.Pin();
					if (!Pinned.IsValid())
					{
						return;
					}

					Pinned->AppendAssistantLine(ResultLine);
					Pinned->SetStatus(bSuccess ? TEXT("状态：空闲") : TEXT("状态：空闲（上次失败）"));
				});
			});

		if (!bAccepted)
		{
			SetStatus(TEXT("状态：忙碌"));
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
		AppendAssistantLine(TEXT("历史已清空。"));
		return FReply::Handled();
	}

	void HandleInputCommitted(const FText& InText, const ETextCommit::Type CommitType)
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
		constexpr int32 MaxLines = 80;
		if (HistoryLines.Num() > MaxLines)
		{
			HistoryLines.RemoveAt(0, HistoryLines.Num() - MaxLines);
		}
		RefreshHistoryText();
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

private:
	FString InitialInput;
	TArray<FString> HistoryLines;
	TSharedPtr<SMultiLineEditableTextBox> HistoryTextBox;
	TSharedPtr<SEditableTextBox> InputTextBox;
	TSharedPtr<STextBlock> StatusTextBlock;
};
}

void FHCIAbilityKitAgentChatWindow::OpenWindow(const FString& InitialInput)
{
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(FText::FromString(TEXT("HCIAbilityKit Agent Chat (Stage I6 Draft)")))
		.ClientSize(FVector2D(860.0f, 620.0f))
		.SupportsMaximize(true)
		.SupportsMinimize(true);

	Window->SetContent(
		SNew(SHCIAbilityKitAgentChatWindow)
		.InitialInput(InitialInput));

	FSlateApplication::Get().AddWindow(Window);
}
