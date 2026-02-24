#include "UI/HCIAbilityKitAgentPlanPreviewWindow.h"

#include "Agent/HCIAbilityKitAgentExecutor.h"
#include "Agent/HCIAbilityKitToolRegistry.h"
#include "AgentActions/HCIAbilityKitAgentToolActions.h"
#include "AssetRegistry/AssetData.h"
#include "ContentBrowserModule.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Framework/Application/SlateApplication.h"
#include "IContentBrowserSingleton.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogHCIAbilityKitAgentPlanPreview, Log, All);

namespace
{
static TArray<FString> HCI_ExtractAssetObjectPaths(const FHCIAbilityKitAgentPlanStep& Step)
{
	TArray<FString> OutPaths;
	if (!Step.Args.IsValid())
	{
		return OutPaths;
	}

	const TArray<TSharedPtr<FJsonValue>>* AssetPaths = nullptr;
	if (!Step.Args->TryGetArrayField(TEXT("asset_paths"), AssetPaths) || AssetPaths == nullptr)
	{
		return OutPaths;
	}

	OutPaths.Reserve(AssetPaths->Num());
	for (const TSharedPtr<FJsonValue>& Value : *AssetPaths)
	{
		const FString ObjectPath = Value.IsValid() ? Value->AsString() : FString();
		if (!ObjectPath.IsEmpty())
		{
			OutPaths.Add(ObjectPath);
		}
	}

	return OutPaths;
}

static bool HCI_JsonValueContainsPlaceholder(const TSharedPtr<FJsonValue>& Value)
{
	if (!Value.IsValid())
	{
		return false;
	}

	switch (Value->Type)
	{
	case EJson::String:
	{
		const FString Text = Value->AsString();
		return Text.Contains(TEXT("{{")) && Text.Contains(TEXT("}}"));
	}
	case EJson::Array:
		for (const TSharedPtr<FJsonValue>& Item : Value->AsArray())
		{
			if (HCI_JsonValueContainsPlaceholder(Item))
			{
				return true;
			}
		}
		return false;
	case EJson::Object:
	{
		const TSharedPtr<FJsonObject> Obj = Value->AsObject();
		if (!Obj.IsValid())
		{
			return false;
		}
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Obj->Values)
		{
			if (HCI_JsonValueContainsPlaceholder(Pair.Value))
			{
				return true;
			}
		}
		return false;
	}
	default:
		return false;
	}
}

static bool HCI_StepHasArgumentPlaceholder(const FHCIAbilityKitAgentPlanStep& Step)
{
	if (!Step.Args.IsValid())
	{
		return false;
	}

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Step.Args->Values)
	{
		if (HCI_JsonValueContainsPlaceholder(Pair.Value))
		{
			return true;
		}
	}
	return false;
}

static FString HCI_BuildArgsPreview(const FHCIAbilityKitAgentPlanStep& Step)
{
	if (!Step.Args.IsValid())
	{
		return TEXT("{}");
	}

	FString JsonText;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
	if (!FJsonSerializer::Serialize(Step.Args.ToSharedRef(), Writer))
	{
		return TEXT("{\"_error\":\"serialize_failed\"}");
	}

	constexpr int32 MaxPreviewChars = 220;
	if (JsonText.Len() > MaxPreviewChars)
	{
		return JsonText.Left(MaxPreviewChars) + TEXT("...");
	}
	return JsonText;
}

static void HCI_LocateAssetsInContentBrowser(const TArray<FString>& AssetObjectPaths)
{
	TArray<FAssetData> AssetsToSync;
	AssetsToSync.Reserve(AssetObjectPaths.Num());

	for (const FString& ObjectPath : AssetObjectPaths)
	{
		if (ObjectPath.IsEmpty())
		{
			continue;
		}

		if (UObject* LoadedObject = LoadObject<UObject>(nullptr, *ObjectPath))
		{
			AssetsToSync.Add(FAssetData(LoadedObject));
		}
	}

	if (AssetsToSync.Num() <= 0)
	{
		UE_LOG(
			LogHCIAbilityKitAgentPlanPreview,
			Warning,
			TEXT("[HCIAbilityKit][AgentPlanPreview] locate=failed reason=no_resolved_assets"));
		return;
	}

	FContentBrowserModule& ContentBrowserModule =
		FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	ContentBrowserModule.Get().SyncBrowserToAssets(AssetsToSync);

	UE_LOG(
		LogHCIAbilityKitAgentPlanPreview,
		Display,
		TEXT("[HCIAbilityKit][AgentPlanPreview] locate=ok resolved_assets=%d"),
		AssetsToSync.Num());
}

static int32 HCI_TryGetIntEvidence(const TMap<FString, FString>& Evidence, const TCHAR* Key, const int32 DefaultValue = 0)
{
	const FString* Value = Evidence.Find(Key);
	if (Value == nullptr)
	{
		return DefaultValue;
	}

	int32 Parsed = DefaultValue;
	return LexTryParseString(Parsed, **Value) ? Parsed : DefaultValue;
}

class SHCIAbilityKitAgentPlanPreviewWindow final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHCIAbilityKitAgentPlanPreviewWindow)
		: _Plan()
		, _Rows()
		, _Context()
	{
	}
		SLATE_ARGUMENT(FHCIAbilityKitAgentPlan, Plan)
		SLATE_ARGUMENT(TArray<TSharedPtr<FHCIAbilityKitAgentPlanPreviewRow>>, Rows)
		SLATE_ARGUMENT(FHCIAbilityKitAgentPlanPreviewContext, Context)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Plan = InArgs._Plan;
		Rows = InArgs._Rows;
		Context = InArgs._Context;

		const auto RunPlan = [this](const bool bDryRun)
		{
			const FHCIAbilityKitToolRegistry& ToolRegistry = FHCIAbilityKitToolRegistry::GetReadOnly();
			FHCIAbilityKitAgentExecutorOptions Options;
			Options.bDryRun = bDryRun;
			Options.bValidatePlanBeforeExecute = true;
			Options.TerminationPolicy = EHCIAbilityKitAgentExecutorTerminationPolicy::ContinueOnFailure;
			HCIAbilityKitAgentToolActions::BuildStageIDraftActions(Options.ToolActions);

			FHCIAbilityKitAgentExecutorRunResult RunResult;
			const bool bRunOk = FHCIAbilityKitAgentExecutor::ExecutePlan(
				Plan,
				ToolRegistry,
				FHCIAbilityKitAgentPlanValidationContext(),
				Options,
				RunResult);

			int32 ScannedAssets = 0;
			for (const FHCIAbilityKitAgentExecutorStepResult& Step : RunResult.StepResults)
			{
				if (Step.ToolName == TEXT("ScanAssets"))
				{
					ScannedAssets += HCI_TryGetIntEvidence(Step.Evidence, TEXT("asset_count"), Step.TargetCountEstimate);
				}
			}

			const FString ModeLabel = bDryRun ? TEXT("DryRun") : TEXT("Commit");
			const FString Summary = FString::Printf(
				TEXT("%s: ok=%s terminal=%s succeeded=%d failed=%d scanned_assets=%d"),
				*ModeLabel,
				bRunOk ? TEXT("true") : TEXT("false"),
				*RunResult.TerminalStatus,
				RunResult.SucceededSteps,
				RunResult.FailedSteps,
				ScannedAssets);
			if (RunSummaryText.IsValid())
			{
				RunSummaryText->SetText(FText::FromString(Summary));
			}

			UE_LOG(
				LogHCIAbilityKitAgentPlanPreview,
				Display,
				TEXT("[HCIAbilityKit][AgentPlanPreview] mode=%s executed request_id=%s steps=%d terminal=%s succeeded=%d failed=%d scanned_assets=%d"),
				bDryRun ? TEXT("dry_run") : TEXT("execute"),
				*Plan.RequestId,
				Plan.Steps.Num(),
				*RunResult.TerminalStatus,
				RunResult.SucceededSteps,
				RunResult.FailedSteps,
				ScannedAssets);
		};

		TSharedRef<SVerticalBox> RowsBox = SNew(SVerticalBox);
		for (const TSharedPtr<FHCIAbilityKitAgentPlanPreviewRow>& Row : Rows)
		{
			if (!Row.IsValid())
			{
				continue;
			}

			const FString RowSummary = FString::Printf(
				TEXT("Step %d | %s | tool=%s | risk=%s | state=%s | assets=%s"),
				Row->StepIndex + 1,
				Row->StepId.IsEmpty() ? TEXT("-") : *Row->StepId,
				Row->ToolName.IsEmpty() ? TEXT("-") : *Row->ToolName,
				Row->RiskLevel.IsEmpty() ? TEXT("-") : *Row->RiskLevel,
				Row->StepStateLabel.IsEmpty() ? TEXT("-") : *Row->StepStateLabel,
				Row->AssetCountLabel.IsEmpty() ? TEXT("0") : *Row->AssetCountLabel);
			const FString ArgsSummary = FString::Printf(
				TEXT("args=%s | locate=%s"),
				Row->ArgsPreview.IsEmpty() ? TEXT("{}") : *Row->ArgsPreview,
				Row->LocateTooltip.IsEmpty() ? TEXT("-") : *Row->LocateTooltip);

			RowsBox->AddSlot()
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
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SNew(STextBlock)
							.Text(FText::FromString(RowSummary))
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(8.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(SButton)
							.Text(FText::FromString(TEXT("定位")))
							.ToolTipText(FText::FromString(Row->LocateTooltip))
							.IsEnabled(Row->bLocateEnabled)
							.OnClicked_Lambda([AssetPaths = Row->ResolvedAssetObjectPaths]()
							{
								HCI_LocateAssetsInContentBrowser(AssetPaths);
								return FReply::Handled();
							})
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(ArgsSummary))
					]
				]
			];
		}

		ChildSlot
		[
			SNew(SBorder)
			.Padding(12.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(
						TEXT("Intent: %s | RequestId: %s | Steps: %d"),
						*Plan.Intent,
						*Plan.RequestId,
						Plan.Steps.Num())))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 4.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(
						Context.EnvScannedAssetCount >= 0
							? FString::Printf(TEXT("证据链: 已基于扫描到的 %d 个资产生成计划"), Context.EnvScannedAssetCount)
							: FString(TEXT("证据链: 当前计划未注入扫描上下文"))))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 4.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(
						TEXT("Planner: provider=%s mode=%s fallback_used=%s fallback_reason=%s route_reason=%s"),
						*Context.PlannerProvider,
						*Context.ProviderMode,
						Context.bFallbackUsed ? TEXT("true") : TEXT("false"),
						Context.FallbackReason.IsEmpty() ? TEXT("-") : *Context.FallbackReason,
						Context.RouteReason.IsEmpty() ? TEXT("-") : *Context.RouteReason)))
				]
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(0.0f, 10.0f, 0.0f, 10.0f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						RowsBox
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 8.0f)
				[
					SAssignNew(RunSummaryText, STextBlock)
					.Text(FText::FromString(TEXT("DryRun: 未执行")))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.Text(FText::FromString(TEXT("模拟执行（草案）")))
						.OnClicked_Lambda([this, RunPlan]()
						{
							RunPlan(true);
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(8.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.Text(FText::FromString(TEXT("确认并执行（Commit Changes）")))
						.OnClicked_Lambda([this, RunPlan]()
						{
							const EAppReturnType::Type UserDecision = FMessageDialog::Open(
								EAppMsgType::YesNo,
								FText::FromString(TEXT("AI 即将真实修改项目资产，操作不可逆，是否继续？")));
							if (UserDecision != EAppReturnType::Yes)
							{
								if (RunSummaryText.IsValid())
								{
									RunSummaryText->SetText(FText::FromString(TEXT("Commit: 用户取消执行")));
								}
								return FReply::Handled();
							}

							RunPlan(false);
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(8.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.Text(FText::FromString(TEXT("关闭")))
						.OnClicked_Lambda([WeakThis = TWeakPtr<SHCIAbilityKitAgentPlanPreviewWindow>(SharedThis(this))]()
						{
							if (const TSharedPtr<SHCIAbilityKitAgentPlanPreviewWindow> Pinned = WeakThis.Pin())
							{
								if (const TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(Pinned.ToSharedRef()))
								{
									Window->RequestDestroyWindow();
								}
							}
							return FReply::Handled();
						})
					]
				]
			]
		];
	}

private:
	FHCIAbilityKitAgentPlan Plan;
	TArray<TSharedPtr<FHCIAbilityKitAgentPlanPreviewRow>> Rows;
	FHCIAbilityKitAgentPlanPreviewContext Context;
	TSharedPtr<STextBlock> RunSummaryText;
};
}

TArray<TSharedPtr<FHCIAbilityKitAgentPlanPreviewRow>> FHCIAbilityKitAgentPlanPreviewWindow::BuildRows(const FHCIAbilityKitAgentPlan& Plan)
{
	TArray<TSharedPtr<FHCIAbilityKitAgentPlanPreviewRow>> Rows;
	Rows.Reserve(Plan.Steps.Num());

	for (int32 StepIndex = 0; StepIndex < Plan.Steps.Num(); ++StepIndex)
	{
		const FHCIAbilityKitAgentPlanStep& Step = Plan.Steps[StepIndex];

		TSharedPtr<FHCIAbilityKitAgentPlanPreviewRow> Row = MakeShared<FHCIAbilityKitAgentPlanPreviewRow>();
		Row->StepIndex = StepIndex;
		Row->StepId = Step.StepId;
		Row->ToolName = Step.ToolName.ToString();
		Row->RiskLevel = FHCIAbilityKitAgentPlanContract::RiskLevelToString(Step.RiskLevel);
		Row->ArgsPreview = HCI_BuildArgsPreview(Step);
		const bool bHasPlaceholder = HCI_StepHasArgumentPlaceholder(Step);
		Row->AssetObjectPaths = HCI_ExtractAssetObjectPaths(Step);
		Row->AssetCountLabel = FString::FromInt(Row->AssetObjectPaths.Num());
		if (bHasPlaceholder)
		{
			Row->StepStateLabel = TEXT("pending_inputs");
			Row->AssetCountLabel = TEXT("? (Pending)");
			Row->bLocateEnabled = false;
			Row->LocateTooltip = TEXT("等待前置步骤结果");
			Rows.Add(Row);
			continue;
		}
		for (const FString& ObjectPath : Row->AssetObjectPaths)
		{
			if (!ObjectPath.IsEmpty() && LoadObject<UObject>(nullptr, *ObjectPath) != nullptr)
			{
				Row->ResolvedAssetObjectPaths.Add(ObjectPath);
			}
		}
		if (Row->AssetObjectPaths.Num() <= 0)
		{
			Row->StepStateLabel = TEXT("no_locate_target");
			Row->bLocateEnabled = false;
			Row->LocateTooltip = TEXT("该步骤未提供可定位资产");
		}
		else if (Row->ResolvedAssetObjectPaths.Num() <= 0)
		{
			Row->StepStateLabel = TEXT("asset_missing");
			Row->bLocateEnabled = false;
			Row->LocateTooltip = TEXT("资产在路径下不存在");
		}
		else
		{
			Row->StepStateLabel = TEXT("ready_to_locate");
			Row->bLocateEnabled = true;
			Row->LocateTooltip = TEXT("在 Content Browser 中定位资产");
		}
		Rows.Add(Row);
	}

	return Rows;
}

void FHCIAbilityKitAgentPlanPreviewWindow::OpenWindow(const FHCIAbilityKitAgentPlan& Plan, const FHCIAbilityKitAgentPlanPreviewContext& Context)
{
	const TArray<TSharedPtr<FHCIAbilityKitAgentPlanPreviewRow>> Rows = BuildRows(Plan);

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(FText::FromString(TEXT("HCIAbilityKit Plan Preview (Stage I2 Draft)")))
		.ClientSize(FVector2D(1080.0f, 680.0f))
		.SupportsMaximize(true)
		.SupportsMinimize(true);

	Window->SetContent(
		SNew(SHCIAbilityKitAgentPlanPreviewWindow)
		.Plan(Plan)
		.Rows(Rows)
		.Context(Context));

	FSlateApplication::Get().AddWindow(Window);
}
