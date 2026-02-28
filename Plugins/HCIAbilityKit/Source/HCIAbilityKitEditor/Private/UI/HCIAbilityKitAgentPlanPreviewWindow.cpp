#include "UI/HCIAbilityKitAgentPlanPreviewWindow.h"

#include "Agent/Executor/HCIAbilityKitAgentExecutor.h"
#include "Agent/Presentation/HCIAbilityKitAgentToolResultSummaryFormatter.h"
#include "Agent/Tools/HCIAbilityKitToolRegistry.h"
#include "AgentActions/HCIAbilityKitAgentToolActions.h"
#include "AssetRegistry/AssetData.h"
#include "ContentBrowserModule.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Framework/Application/SlateApplication.h"
#include "IContentBrowserSingleton.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "GameFramework/Actor.h"
#include "UObject/SoftObjectPath.h"
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

static FString HCI_GetEvidenceValue(
	const TMap<FString, FString>& Evidence,
	const TCHAR* Key,
	const TCHAR* DefaultValue = TEXT("-"))
{
	const FString* Found = Evidence.Find(Key);
	return Found != nullptr && !Found->IsEmpty() ? *Found : FString(DefaultValue);
}

static bool HCI_Preview_IsWriteLikeRisk(const EHCIAbilityKitAgentPlanRiskLevel RiskLevel)
{
	return RiskLevel == EHCIAbilityKitAgentPlanRiskLevel::Write ||
		   RiskLevel == EHCIAbilityKitAgentPlanRiskLevel::Destructive;
}

static void HCI_SplitEvidenceList(
	const FString& RawValue,
	const TCHAR* Delimiter,
	TArray<FString>& OutItems)
{
	OutItems.Reset();
	if (RawValue.IsEmpty() || RawValue.Equals(TEXT("none"), ESearchCase::IgnoreCase) || RawValue.Equals(TEXT("-"), ESearchCase::CaseSensitive))
	{
		return;
	}

	RawValue.ParseIntoArray(OutItems, Delimiter, true);
	for (FString& Item : OutItems)
	{
		Item.TrimStartAndEndInline();
	}
	OutItems.RemoveAll([](const FString& Item) { return Item.IsEmpty(); });
}

static FString HCI_SanitizeAssetPathLikeToken(FString InValue)
{
	InValue.TrimStartAndEndInline();
	if (InValue.IsEmpty())
	{
		return FString();
	}

	const int32 ReasonSuffixIndex = InValue.Find(TEXT(" ("), ESearchCase::CaseSensitive, ESearchDir::FromStart);
	if (ReasonSuffixIndex > 0)
	{
		InValue = InValue.Left(ReasonSuffixIndex);
		InValue.TrimStartAndEndInline();
	}

	const int32 ColonIndex = InValue.Find(TEXT(":"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	if (ColonIndex > 0)
	{
		bool bAllDigitsAfterColon = true;
		for (int32 Index = ColonIndex + 1; Index < InValue.Len(); ++Index)
		{
			if (!FChar::IsDigit(InValue[Index]))
			{
				bAllDigitsAfterColon = false;
				break;
			}
		}
		if (bAllDigitsAfterColon)
		{
			InValue = InValue.Left(ColonIndex);
			InValue.TrimStartAndEndInline();
		}
	}

	return InValue;
}

static void HCI_ParseLocateReasonMapEvidence(
	const FString& RawValue,
	TMap<FString, FString>& InOutReasonByPath)
{
	if (RawValue.IsEmpty() || RawValue.Equals(TEXT("none"), ESearchCase::IgnoreCase))
	{
		return;
	}

	TArray<FString> Items;
	HCI_SplitEvidenceList(RawValue, TEXT(" | "), Items);
	if (Items.Num() <= 0)
	{
		HCI_SplitEvidenceList(RawValue, TEXT("|"), Items);
	}

	for (FString& Item : Items)
	{
		Item.TrimStartAndEndInline();
		if (Item.IsEmpty())
		{
			continue;
		}

		FString Left;
		FString Right;
		if (!Item.Split(TEXT(" => "), &Left, &Right, ESearchCase::CaseSensitive))
		{
			continue;
		}
		Left.TrimStartAndEndInline();
		Right.TrimStartAndEndInline();
		Left = HCI_SanitizeAssetPathLikeToken(MoveTemp(Left));
		if (!Left.IsEmpty() && !Right.IsEmpty())
		{
			InOutReasonByPath.Add(MoveTemp(Left), MoveTemp(Right));
		}
	}
}

static FString HCI_BuildLocateTargetLabelFromPath(
	const FString& TargetPath,
	const bool bActor,
	const FString& ToolName,
	const FString& EvidenceKey)
{
	FString LabelCore = TargetPath;
	int32 DotIndex = INDEX_NONE;
	if (bActor)
	{
#if WITH_EDITOR
		if (UObject* Resolved = FindObject<UObject>(nullptr, *TargetPath))
		{
			if (const AActor* Actor = Cast<AActor>(Resolved))
			{
				const FString ActorLabel = Actor->GetActorLabel();
				if (!ActorLabel.IsEmpty())
				{
					LabelCore = ActorLabel;
				}
			}
		}
		else
		{
			const FSoftObjectPath SoftPath(TargetPath);
			if (UObject* SoftResolved = SoftPath.ResolveObject())
			{
				if (const AActor* Actor = Cast<AActor>(SoftResolved))
				{
					const FString ActorLabel = Actor->GetActorLabel();
					if (!ActorLabel.IsEmpty())
					{
						LabelCore = ActorLabel;
					}
				}
			}
		}
#endif

		if (TargetPath.FindLastChar(TEXT('.'), DotIndex) && DotIndex + 1 < TargetPath.Len())
		{
			if (LabelCore == TargetPath)
			{
				LabelCore = TargetPath.Mid(DotIndex + 1);
			}
		}
	}
	else
	{
		FString PackagePath = TargetPath;
		if (PackagePath.FindLastChar(TEXT('.'), DotIndex) && DotIndex > 0)
		{
			PackagePath = PackagePath.Left(DotIndex);
		}
		int32 LastSlash = INDEX_NONE;
		if (PackagePath.FindLastChar(TEXT('/'), LastSlash) && LastSlash + 1 < PackagePath.Len())
		{
			LabelCore = PackagePath.Mid(LastSlash + 1);
		}
	}

	const TCHAR* KindLabel = bActor ? TEXT("Actor") : TEXT("Asset");
	const FString EvidenceLabel = EvidenceKey.IsEmpty() ? ToolName : FString::Printf(TEXT("%s/%s"), *ToolName, *EvidenceKey);
	return FString::Printf(TEXT("%s: %s (%s)"), KindLabel, *LabelCore, *EvidenceLabel);
}

static void HCI_AddLocateTargetUnique(
	const EHCIAbilityKitAgentExecutionLocateTargetKind Kind,
	const FString& RawTargetPath,
	const FString& Detail,
	const FString& ToolName,
	const FString& EvidenceKey,
	TSet<FString>& InOutKeys,
	TArray<FHCIAbilityKitAgentExecutionLocateTarget>& OutTargets)
{
	FString TargetPath = RawTargetPath;
	TargetPath.TrimStartAndEndInline();
	if (TargetPath.IsEmpty() || !TargetPath.StartsWith(TEXT("/Game/")))
	{
		return;
	}

	const FString DedupKey = FString::Printf(TEXT("%d|%s"), static_cast<int32>(Kind), *TargetPath);
	if (InOutKeys.Contains(DedupKey))
	{
		return;
	}
	InOutKeys.Add(DedupKey);

	FHCIAbilityKitAgentExecutionLocateTarget& Target = OutTargets.AddDefaulted_GetRef();
	Target.Kind = Kind;
	Target.TargetPath = TargetPath;
	Target.SourceToolName = ToolName;
	Target.SourceEvidenceKey = EvidenceKey;
	Target.Detail = Detail;
	Target.DisplayLabel = HCI_BuildLocateTargetLabelFromPath(
		TargetPath,
		Kind == EHCIAbilityKitAgentExecutionLocateTargetKind::Actor,
		ToolName,
		EvidenceKey);
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

		const auto RunPlan = [this](const bool bDryRun, const bool bUserConfirmedWriteSteps)
		{
			FHCIAbilityKitAgentPlanExecutionReport Report;
			FHCIAbilityKitAgentPlanPreviewWindow::ExecutePlan(Plan, bDryRun, bUserConfirmedWriteSteps, Report);
			if (RunSummaryText.IsValid())
			{
				RunSummaryText->SetText(FText::FromString(Report.SummaryText));
			}
			if (ToolResultSummaryText.IsValid())
			{
				HCIAbilityKitAgentToolResultSummaryFormatter::FHCIAbilityKitToolResultSummaryOptions SummaryOptions;
				SummaryOptions.MaxLines = 10;
				SummaryOptions.MaxItemsPerList = 4;
				SummaryOptions.MaxCharsPerLine = 260;

				TArray<FString> Lines;
				HCIAbilityKitAgentToolResultSummaryFormatter::BuildSummaryLines(Report.StepResults, SummaryOptions, Lines);
				const FString Summary = Lines.Num() > 0 ? FString::Join(Lines, TEXT("\n")) : FString(TEXT("结果摘要：无"));
				ToolResultSummaryText->SetText(FText::FromString(Summary));
			}
			if (SearchPathEvidenceText.IsValid())
			{
				SearchPathEvidenceText->SetText(FText::FromString(Report.SearchPathEvidenceText));
			}
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
				.Padding(0.0f, 0.0f, 0.0f, 8.0f)
				[
					SAssignNew(ToolResultSummaryText, STextBlock)
					.Text(FText::FromString(TEXT("结果摘要：未执行")))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 8.0f)
				[
					SAssignNew(SearchPathEvidenceText, STextBlock)
					.Text(FText::FromString(TEXT("SearchPath证据: 未执行")))
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
								RunPlan(true, true);
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
								const FString ConfirmText = FHCIAbilityKitAgentPlanPreviewWindow::BuildCommitConfirmMessage(Plan);
								const EAppReturnType::Type UserDecision = FMessageDialog::Open(
									EAppMsgType::YesNo,
									FText::FromString(ConfirmText));
								if (UserDecision != EAppReturnType::Yes)
								{
									UE_LOG(
										LogHCIAbilityKitAgentPlanPreview,
										Display,
										TEXT("[HCIAbilityKit][AgentPlanPreview] mode=execute cancelled_by_user request_id=%s"),
										*Plan.RequestId);
									if (RunSummaryText.IsValid())
									{
										RunSummaryText->SetText(FText::FromString(TEXT("Commit: 用户取消执行（未触发真实写操作）")));
									}
									return FReply::Handled();
								}

								RunPlan(false, true);
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
	TSharedPtr<STextBlock> ToolResultSummaryText;
	TSharedPtr<STextBlock> SearchPathEvidenceText;
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

FHCIAbilityKitAgentPlanCommitRiskSummary FHCIAbilityKitAgentPlanPreviewWindow::BuildCommitRiskSummary(const FHCIAbilityKitAgentPlan& Plan)
{
	FHCIAbilityKitAgentPlanCommitRiskSummary Summary;
	Summary.TotalSteps = Plan.Steps.Num();
	for (const FHCIAbilityKitAgentPlanStep& Step : Plan.Steps)
	{
		if (!HCI_Preview_IsWriteLikeRisk(Step.RiskLevel))
		{
			continue;
		}

		++Summary.WriteLikeSteps;
		if (Step.RiskLevel == EHCIAbilityKitAgentPlanRiskLevel::Destructive)
		{
			++Summary.DestructiveSteps;
		}
	}

	Summary.bRequiresConfirmDialog = Summary.WriteLikeSteps > 0;
	return Summary;
}

FString FHCIAbilityKitAgentPlanPreviewWindow::BuildCommitConfirmMessage(const FHCIAbilityKitAgentPlan& Plan)
{
	const FHCIAbilityKitAgentPlanCommitRiskSummary Summary = BuildCommitRiskSummary(Plan);
	if (!Summary.bRequiresConfirmDialog)
	{
		return FString::Printf(
			TEXT("当前计划共 %d 步，未检测到写操作。\n将执行只读步骤以更新证据链，是否继续？"),
			Summary.TotalSteps);
	}

	return FString::Printf(
		TEXT("当前计划共 %d 步，其中写操作 %d 步（含高风险破坏性 %d 步）。\nAI 即将真实修改项目资产，操作不可逆，是否继续？"),
		Summary.TotalSteps,
		Summary.WriteLikeSteps,
		Summary.DestructiveSteps);
}

FString FHCIAbilityKitAgentPlanPreviewWindow::BuildSearchPathEvidenceSummary(const TArray<FHCIAbilityKitAgentExecutorStepResult>& StepResults)
{
	for (const FHCIAbilityKitAgentExecutorStepResult& StepResult : StepResults)
	{
		if (!StepResult.ToolName.Equals(TEXT("SearchPath"), ESearchCase::IgnoreCase))
		{
			continue;
		}

		const FString Keyword = HCI_GetEvidenceValue(StepResult.Evidence, TEXT("keyword"));
		const FString Normalized = HCI_GetEvidenceValue(StepResult.Evidence, TEXT("keyword_normalized"));
		const FString Expanded = HCI_GetEvidenceValue(StepResult.Evidence, TEXT("keyword_expanded"));
		const FString MatchedCount = HCI_GetEvidenceValue(StepResult.Evidence, TEXT("matched_count"), TEXT("0"));
		const FString BestDirectory = HCI_GetEvidenceValue(StepResult.Evidence, TEXT("best_directory"));
		const FString FallbackUsed = HCI_GetEvidenceValue(StepResult.Evidence, TEXT("semantic_fallback_used"), TEXT("false"));
		const FString FallbackDirectory = HCI_GetEvidenceValue(StepResult.Evidence, TEXT("semantic_fallback_directory"), TEXT("-"));

		const bool bSemanticFallbackUsed = FallbackUsed.Equals(TEXT("true"), ESearchCase::IgnoreCase);
		const FString FallbackLabel = bSemanticFallbackUsed ? FallbackDirectory : FString(TEXT("false"));

		return FString::Printf(
			TEXT("SearchPath：%s -> %s（matched=%s fallback=%s）"),
			Keyword.IsEmpty() ? TEXT("-") : *Keyword,
			BestDirectory.IsEmpty() ? TEXT("-") : *BestDirectory,
			MatchedCount.IsEmpty() ? TEXT("0") : *MatchedCount,
			FallbackLabel.IsEmpty() ? TEXT("false") : *FallbackLabel);
	}

	// Keep UI clean: if no SearchPath step exists, return empty so callers can skip rendering.
	return FString();
}

void FHCIAbilityKitAgentPlanPreviewWindow::BuildLocateTargetsFromStepResults(
	const TArray<FHCIAbilityKitAgentExecutorStepResult>& StepResults,
	TArray<FHCIAbilityKitAgentExecutionLocateTarget>& OutTargets)
{
	OutTargets.Reset();
	TSet<FString> SeenKeys;
	TArray<FString> Tokens;
	TMap<FString, FString> OrphanReasonByPath;
	TMap<FString, FString> UnresolvedReasonByPath;

	for (const FHCIAbilityKitAgentExecutorStepResult& StepResult : StepResults)
	{
		if (const FString* Raw = StepResult.Evidence.Find(TEXT("orphan_asset_reasons")))
		{
			HCI_ParseLocateReasonMapEvidence(*Raw, OrphanReasonByPath);
		}
		if (const FString* Raw = StepResult.Evidence.Find(TEXT("unresolved_asset_reasons")))
		{
			HCI_ParseLocateReasonMapEvidence(*Raw, UnresolvedReasonByPath);
		}
	}

	auto AddActorEvidenceList = [&](const FHCIAbilityKitAgentExecutorStepResult& StepResult, const TCHAR* EvidenceKey)
	{
		const FString* Raw = StepResult.Evidence.Find(EvidenceKey);
		if (Raw == nullptr)
		{
			return;
		}

		HCI_SplitEvidenceList(*Raw, TEXT(" | "), Tokens);
		if (Tokens.Num() <= 0)
		{
			HCI_SplitEvidenceList(*Raw, TEXT("|"), Tokens);
		}

		for (const FString& ActorPath : Tokens)
		{
			HCI_AddLocateTargetUnique(
				EHCIAbilityKitAgentExecutionLocateTargetKind::Actor,
				ActorPath,
				FString(),
				StepResult.ToolName,
				EvidenceKey,
				SeenKeys,
				OutTargets);
		}
	};

	auto AddAssetEvidenceList = [&](const FHCIAbilityKitAgentExecutorStepResult& StepResult, const TCHAR* EvidenceKey, const TCHAR* Delimiter = TEXT(" | "))
	{
		const FString* Raw = StepResult.Evidence.Find(EvidenceKey);
		if (Raw == nullptr)
		{
			return;
		}

		HCI_SplitEvidenceList(*Raw, Delimiter, Tokens);
		if (Tokens.Num() <= 0 && FCString::Strcmp(Delimiter, TEXT("|")) != 0)
		{
			HCI_SplitEvidenceList(*Raw, TEXT("|"), Tokens);
		}

		for (const FString& Token : Tokens)
		{
			HCI_AddLocateTargetUnique(
				EHCIAbilityKitAgentExecutionLocateTargetKind::Asset,
				HCI_SanitizeAssetPathLikeToken(Token),
				FString(),
				StepResult.ToolName,
				EvidenceKey,
				SeenKeys,
				OutTargets);
		}
	};

	auto AddSingleAssetEvidence = [&](const FHCIAbilityKitAgentExecutorStepResult& StepResult, const TCHAR* EvidenceKey)
	{
		const FString* Raw = StepResult.Evidence.Find(EvidenceKey);
		if (Raw == nullptr)
		{
			return;
		}

		HCI_AddLocateTargetUnique(
			EHCIAbilityKitAgentExecutionLocateTargetKind::Asset,
			HCI_SanitizeAssetPathLikeToken(*Raw),
			FString(),
			StepResult.ToolName,
			EvidenceKey,
			SeenKeys,
			OutTargets);
	};

	// Pass 1: add "orphanish" locate items first so they win de-duplication over generic ScanAssets.asset_paths.
	for (const FHCIAbilityKitAgentExecutorStepResult& StepResult : StepResults)
	{
		{
			const FString* Raw = StepResult.Evidence.Find(TEXT("orphan_assets"));
			if (Raw != nullptr)
			{
				HCI_SplitEvidenceList(*Raw, TEXT(" | "), Tokens);
				if (Tokens.Num() <= 0)
				{
					HCI_SplitEvidenceList(*Raw, TEXT("|"), Tokens);
				}
				for (const FString& Token : Tokens)
				{
					const FString Path = HCI_SanitizeAssetPathLikeToken(Token);
					const FString Detail = OrphanReasonByPath.FindRef(Path);
					HCI_AddLocateTargetUnique(
						EHCIAbilityKitAgentExecutionLocateTargetKind::Asset,
						Path,
						Detail,
						StepResult.ToolName,
						TEXT("orphan_assets"),
						SeenKeys,
						OutTargets);
				}
			}
		}
		{
			const FString* Raw = StepResult.Evidence.Find(TEXT("unresolved_assets"));
			if (Raw != nullptr)
			{
				HCI_SplitEvidenceList(*Raw, TEXT(" | "), Tokens);
				if (Tokens.Num() <= 0)
				{
					HCI_SplitEvidenceList(*Raw, TEXT("|"), Tokens);
				}
				for (const FString& Token : Tokens)
				{
					const FString Path = HCI_SanitizeAssetPathLikeToken(Token);
					const FString Detail = UnresolvedReasonByPath.FindRef(Path);
					HCI_AddLocateTargetUnique(
						EHCIAbilityKitAgentExecutionLocateTargetKind::Asset,
						Path,
						Detail,
						StepResult.ToolName,
						TEXT("unresolved_assets"),
						SeenKeys,
						OutTargets);
				}
			}
		}
	}

	// Pass 2: add the rest of locate hints.
	for (const FHCIAbilityKitAgentExecutorStepResult& StepResult : StepResults)
	{
		AddActorEvidenceList(StepResult, TEXT("risky_actors"));
		AddActorEvidenceList(StepResult, TEXT("missing_collision_actors"));
		AddActorEvidenceList(StepResult, TEXT("default_material_actors"));
		{
			const FString* ActorPath = StepResult.Evidence.Find(TEXT("actor_path"));
			if (ActorPath != nullptr)
			{
				const FString Issue = HCI_GetEvidenceValue(StepResult.Evidence, TEXT("issue"));
				HCI_AddLocateTargetUnique(
					EHCIAbilityKitAgentExecutionLocateTargetKind::Actor,
					*ActorPath,
					Issue,
					StepResult.ToolName,
					TEXT("actor_path"),
					SeenKeys,
					OutTargets);
			}
		}

		AddAssetEvidenceList(StepResult, TEXT("asset_paths"), TEXT("|"));
		AddAssetEvidenceList(StepResult, TEXT("modified_assets"));
		AddAssetEvidenceList(StepResult, TEXT("failed_assets"));
		AddAssetEvidenceList(StepResult, TEXT("top_meshes"));
		AddSingleAssetEvidence(StepResult, TEXT("asset_path"));
		AddSingleAssetEvidence(StepResult, TEXT("max_triangle_asset"));
		AddSingleAssetEvidence(StepResult, TEXT("before"));
		AddSingleAssetEvidence(StepResult, TEXT("after"));
	}
}

bool FHCIAbilityKitAgentPlanPreviewWindow::ExecutePlan(
	const FHCIAbilityKitAgentPlan& Plan,
	const bool bDryRun,
	const bool bUserConfirmedWriteSteps,
	FHCIAbilityKitAgentPlanExecutionReport& OutReport,
	TFunction<void(int32, int32, const FHCIAbilityKitAgentPlanStep&)> OnStepBegin)
{
	OutReport = FHCIAbilityKitAgentPlanExecutionReport();
	OutReport.bDryRun = bDryRun;

	const FHCIAbilityKitToolRegistry& ToolRegistry = FHCIAbilityKitToolRegistry::GetReadOnly();
	FHCIAbilityKitAgentExecutorOptions Options;
	Options.bDryRun = bDryRun;
	Options.bValidatePlanBeforeExecute = true;
	Options.TerminationPolicy = EHCIAbilityKitAgentExecutorTerminationPolicy::ContinueOnFailure;
	Options.bEnablePreflightGates = true;
	Options.bUserConfirmedWriteSteps = bUserConfirmedWriteSteps;
	if (OnStepBegin)
	{
		const int32 TotalSteps = Plan.Steps.Num();
		Options.OnStepBegin = [OnStepBegin, TotalSteps](const int32 StepIndex, const FHCIAbilityKitAgentPlanStep& Step)
		{
			OnStepBegin(StepIndex, TotalSteps, Step);
		};
	}
	HCIAbilityKitAgentToolActions::BuildStageIDraftActions(Options.ToolActions);

	// Business-level Undo: one approved commit == one undo record (Context=HCIAbilityKit).
	// Keep DryRun free of transactions.
	TUniquePtr<FScopedTransaction> BusinessTransaction;
	if (!bDryRun && bUserConfirmedWriteSteps)
	{
		const FString SessionName = FString::Printf(TEXT("HCIAbilityKit: %s (%s)"), *Plan.Intent, *Plan.RequestId);
		BusinessTransaction = MakeUnique<FScopedTransaction>(TEXT("HCIAbilityKit"), FText::FromString(SessionName), nullptr, true);
	}

	FHCIAbilityKitAgentExecutorRunResult RunResult;
	OutReport.bRunOk = FHCIAbilityKitAgentExecutor::ExecutePlan(
		Plan,
		ToolRegistry,
		FHCIAbilityKitAgentPlanValidationContext(),
		Options,
		RunResult);
	OutReport.ExecutionMode = RunResult.ExecutionMode;
	OutReport.TerminalStatus = RunResult.TerminalStatus;
	OutReport.TerminalReason = RunResult.TerminalReason;
	OutReport.SucceededSteps = RunResult.SucceededSteps;
	OutReport.FailedSteps = RunResult.FailedSteps;

	int32 ScannedAssets = 0;
	int32 ScannedLevelActors = 0;
	int32 RiskyLevelActors = 0;
	for (const FHCIAbilityKitAgentExecutorStepResult& Step : RunResult.StepResults)
	{
		if (Step.ToolName == TEXT("ScanAssets"))
		{
			ScannedAssets += HCI_TryGetIntEvidence(Step.Evidence, TEXT("asset_count"), Step.TargetCountEstimate);
		}
		else if (Step.ToolName == TEXT("ScanMeshTriangleCount"))
		{
			int32 Count = HCI_TryGetIntEvidence(Step.Evidence, TEXT("scanned_count"), -1);
			if (Count < 0)
			{
				Count = HCI_TryGetIntEvidence(Step.Evidence, TEXT("mesh_count"), Step.TargetCountEstimate);
			}
			ScannedAssets += FMath::Max(0, Count);
		}
		else if (Step.ToolName == TEXT("ScanLevelMeshRisks"))
		{
			ScannedLevelActors += HCI_TryGetIntEvidence(Step.Evidence, TEXT("scanned_count"), 0);
			RiskyLevelActors += HCI_TryGetIntEvidence(Step.Evidence, TEXT("risky_count"), Step.TargetCountEstimate);
		}
	}
	OutReport.ScannedAssets = ScannedAssets + ScannedLevelActors;

	const FString ModeLabel = bDryRun ? TEXT("DryRun") : TEXT("Commit");
	OutReport.SummaryText = FString::Printf(
		TEXT("%s: ok=%s execution_mode=%s terminal=%s reason=%s succeeded=%d failed=%d scanned_assets=%d scanned_level_actors=%d risky_level_actors=%d"),
		*ModeLabel,
		OutReport.bRunOk ? TEXT("true") : TEXT("false"),
		*OutReport.ExecutionMode,
		*OutReport.TerminalStatus,
		*OutReport.TerminalReason,
		OutReport.SucceededSteps,
		OutReport.FailedSteps,
		OutReport.ScannedAssets,
		ScannedLevelActors,
		RiskyLevelActors);
	OutReport.SearchPathEvidenceText = BuildSearchPathEvidenceSummary(RunResult.StepResults);
	OutReport.StepResults = RunResult.StepResults;
	BuildLocateTargetsFromStepResults(RunResult.StepResults, OutReport.LocateTargets);

	UE_LOG(
		LogHCIAbilityKitAgentPlanPreview,
		Display,
		TEXT("[HCIAbilityKit][AgentPlanPreview] mode=%s execution_mode=%s executed request_id=%s steps=%d terminal=%s terminal_reason=%s succeeded=%d failed=%d scanned_assets=%d scanned_level_actors=%d risky_level_actors=%d"),
		bDryRun ? TEXT("dry_run") : TEXT("execute"),
		*OutReport.ExecutionMode,
		*Plan.RequestId,
		Plan.Steps.Num(),
		*OutReport.TerminalStatus,
		*OutReport.TerminalReason,
		OutReport.SucceededSteps,
		OutReport.FailedSteps,
		OutReport.ScannedAssets,
		ScannedLevelActors,
		RiskyLevelActors);

	if (OutReport.LocateTargets.Num() > 0)
	{
		UE_LOG(
			LogHCIAbilityKitAgentPlanPreview,
			Display,
			TEXT("[HCIAbilityKit][AgentPlanPreview] locate_targets=%d request_id=%s"),
			OutReport.LocateTargets.Num(),
			*Plan.RequestId);
	}

	return OutReport.bRunOk;
}

void FHCIAbilityKitAgentPlanPreviewWindow::OpenWindow(const FHCIAbilityKitAgentPlan& Plan, const FHCIAbilityKitAgentPlanPreviewContext& Context)
{
	const TArray<TSharedPtr<FHCIAbilityKitAgentPlanPreviewRow>> Rows = BuildRows(Plan);

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(FText::FromString(TEXT("HCIAbilityKit Plan Preview (Stage I3 Draft)")))
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
