#include "Subsystems/HCIAbilityKitAgentSubsystem.h"

#include "Agent/LLM/HCIAbilityKitAgentPromptBuilder.h"
#include "Async/Async.h"
#include "Commands/HCIAbilityKitAgentCommand_ChatPlanAndSummary.h"
#include "Editor.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "Ingest/HCIAbilityKitIngestManifest.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UI/HCIAbilityKitAgentPlanPreviewWindow.h"
#include "UObject/SoftObjectPath.h"

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

static bool HCI_Subsystem_IsWriteLikeRisk(const EHCIAbilityKitAgentPlanRiskLevel RiskLevel)
{
	return RiskLevel == EHCIAbilityKitAgentPlanRiskLevel::Write ||
		RiskLevel == EHCIAbilityKitAgentPlanRiskLevel::Destructive;
}

static const TCHAR* HCI_SessionStateToLabel(const EHCIAbilityKitAgentSessionState State)
{
	switch (State)
	{
	case EHCIAbilityKitAgentSessionState::Idle:
		return TEXT("Idle");
	case EHCIAbilityKitAgentSessionState::Thinking:
		return TEXT("Thinking");
	case EHCIAbilityKitAgentSessionState::PlanReady:
		return TEXT("PlanReady");
	case EHCIAbilityKitAgentSessionState::AutoExecuteReadOnly:
		return TEXT("AutoExecuteReadOnly");
	case EHCIAbilityKitAgentSessionState::AwaitUserConfirm:
		return TEXT("AwaitUserConfirm");
	case EHCIAbilityKitAgentSessionState::Executing:
		return TEXT("Executing");
	case EHCIAbilityKitAgentSessionState::Summarizing:
		return TEXT("Summarizing");
	case EHCIAbilityKitAgentSessionState::Completed:
		return TEXT("Completed");
	case EHCIAbilityKitAgentSessionState::Failed:
		return TEXT("Failed");
	case EHCIAbilityKitAgentSessionState::Cancelled:
		return TEXT("Cancelled");
	default:
		return TEXT("Unknown");
	}
}

static FString HCI_GetStepArgString(const FHCIAbilityKitAgentPlanStep& Step, const TCHAR* FieldName)
{
	if (!Step.Args.IsValid())
	{
		return FString();
	}

	FString Value;
	Step.Args->TryGetStringField(FieldName, Value);
	return Value;
}

static int32 HCI_GetStepArgInt(const FHCIAbilityKitAgentPlanStep& Step, const TCHAR* FieldName, const int32 DefaultValue = 0)
{
	if (!Step.Args.IsValid())
	{
		return DefaultValue;
	}

	double Number = static_cast<double>(DefaultValue);
	if (Step.Args->TryGetNumberField(FieldName, Number))
	{
		return static_cast<int32>(Number);
	}
	return DefaultValue;
}

static FString HCI_SanitizeUiTextForArtist(FString InOut)
{
	// Remove obvious technical terms but preserve paths like /Game/...
	InOut.ReplaceInline(TEXT("Texture2D"), TEXT("贴图"));
	InOut.ReplaceInline(TEXT("StaticMesh"), TEXT("静态网格"));
	InOut.ReplaceInline(TEXT("Static Mesh"), TEXT("静态网格"));
	InOut.ReplaceInline(TEXT("Maximum Texture Size"), TEXT("最大纹理尺寸"));
	InOut.ReplaceInline(TEXT("Max Texture Size"), TEXT("最大纹理尺寸"));
	InOut.ReplaceInline(TEXT("LOD Group"), TEXT("细节等级组"));
	// Common UE preset values that are not user-friendly in Chinese UI.
	InOut.ReplaceInline(TEXT("LevelArchitecture"), TEXT("关卡建筑"));
	InOut.ReplaceInline(TEXT("SmallProp"), TEXT("小道具"));
	InOut.ReplaceInline(TEXT("LargeProp"), TEXT("大道具"));
	InOut.ReplaceInline(TEXT("Foliage"), TEXT("植被"));
	return InOut;
}

static FString HCI_BuildStepSummaryFallback(const FHCIAbilityKitAgentPlanStep& Step)
{
	if (Step.UiPresentation.bHasStepSummary)
	{
		FString Trimmed = Step.UiPresentation.StepSummary.TrimStartAndEnd();
		if (!Trimmed.IsEmpty())
		{
			return HCI_SanitizeUiTextForArtist(MoveTemp(Trimmed));
		}
	}

	if (Step.ToolName == TEXT("ScanLevelMeshRisks"))
	{
		const FString Scope = HCI_GetStepArgString(Step, TEXT("scope"));
		return Scope == TEXT("all")
			? TEXT("正在扫描场景模型风险（全场景）")
			: TEXT("正在扫描场景模型风险（当前选中）");
	}
	if (Step.ToolName == TEXT("ScanMeshTriangleCount"))
	{
		const FString Directory = HCI_GetStepArgString(Step, TEXT("directory"));
		return Directory.IsEmpty() ? TEXT("正在统计模型面数") : FString::Printf(TEXT("正在统计模型面数（%s）"), *Directory);
	}
	if (Step.ToolName == TEXT("ScanAssets"))
	{
		const FString Directory = HCI_GetStepArgString(Step, TEXT("directory"));
		return Directory.IsEmpty() ? TEXT("正在扫描目录资产") : FString::Printf(TEXT("正在扫描目录资产（%s）"), *Directory);
	}
	if (Step.ToolName == TEXT("SearchPath"))
	{
		const FString Keyword = HCI_GetStepArgString(Step, TEXT("keyword"));
		return Keyword.IsEmpty() ? TEXT("正在定位目标目录") : FString::Printf(TEXT("正在定位目标目录（关键词：%s）"), *Keyword);
	}
	if (Step.ToolName == TEXT("SetTextureMaxSize"))
	{
		const int32 MaxSize = HCI_GetStepArgInt(Step, TEXT("max_size"), 0);
		return MaxSize > 0
			? FString::Printf(TEXT("准备批量将贴图最大纹理尺寸限制为 %d"), MaxSize)
			: TEXT("准备批量限制贴图最大纹理尺寸");
	}
	if (Step.ToolName == TEXT("SetMeshLODGroup"))
	{
		const FString LODGroup = HCI_GetStepArgString(Step, TEXT("lod_group"));
		return LODGroup.IsEmpty()
			? TEXT("准备批量设置模型细节等级组")
			: FString::Printf(TEXT("准备批量设置模型细节等级组（%s）"), *LODGroup);
	}
	if (Step.ToolName == TEXT("NormalizeAssetNamingByMetadata"))
	{
		return TEXT("准备按元数据规范命名并归档资产");
	}
	if (Step.ToolName == TEXT("RenameAsset"))
	{
		return TEXT("准备重命名资产");
	}
	if (Step.ToolName == TEXT("MoveAsset"))
	{
		return TEXT("准备移动资产");
	}

	const FString ToolName = Step.ToolName.ToString();
	return ToolName.IsEmpty() ? TEXT("正在处理步骤") : FString::Printf(TEXT("正在执行步骤（%s）"), *ToolName);
}

static FString HCI_BuildStepIntentReasonFallback(const FHCIAbilityKitAgentPlanStep& Step)
{
	if (Step.UiPresentation.bHasIntentReason)
	{
		FString Trimmed = Step.UiPresentation.IntentReason.TrimStartAndEnd();
		if (!Trimmed.IsEmpty())
		{
			return HCI_SanitizeUiTextForArtist(MoveTemp(Trimmed));
		}
	}

	if (Step.ToolName == TEXT("SearchPath"))
	{
		return TEXT("用户未提供绝对路径，先定位目录再继续执行。");
	}
	if (Step.ToolName == TEXT("ScanAssets"))
	{
		return TEXT("先获取资产清单，作为后续分析或修改的输入范围。");
	}
	if (Step.ToolName == TEXT("ScanLevelMeshRisks"))
	{
		return TEXT("先收集缺碰撞与默认材质证据，再决定是否需要修复。");
	}
	if (Step.ToolName == TEXT("SetTextureMaxSize") || Step.ToolName == TEXT("SetMeshLODGroup"))
	{
		return TEXT("根据前序扫描结果执行批量合规修改。");
	}
	return FString();
}

static FString HCI_BuildStepRiskWarningFallback(const FHCIAbilityKitAgentPlanStep& Step)
{
	if (Step.UiPresentation.bHasRiskWarning)
	{
		FString Trimmed = Step.UiPresentation.RiskWarning.TrimStartAndEnd();
		if (!Trimmed.IsEmpty())
		{
			return HCI_SanitizeUiTextForArtist(MoveTemp(Trimmed));
		}
	}

	if (Step.RiskLevel == EHCIAbilityKitAgentPlanRiskLevel::Destructive)
	{
		return TEXT("高风险操作：可能删除或覆盖大量资产，请确认后执行。");
	}
	if (Step.bRequiresConfirm || Step.RiskLevel == EHCIAbilityKitAgentPlanRiskLevel::Write)
	{
		return TEXT("将真实修改项目资产，执行前请确认范围与目标。");
	}
	return FString();
}

static bool HCI_TryGetStepArgStringArray(const FHCIAbilityKitAgentPlanStep& Step, const TCHAR* FieldName, TArray<FString>& OutValues)
{
	OutValues.Reset();
	if (!Step.Args.IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* JsonArray = nullptr;
	if (!Step.Args->TryGetArrayField(FieldName, JsonArray) || JsonArray == nullptr)
	{
		return false;
	}

	for (const TSharedPtr<FJsonValue>& Value : *JsonArray)
	{
		FString StringValue;
		if (Value.IsValid() && Value->TryGetString(StringValue))
		{
			OutValues.Add(StringValue);
		}
	}
	return true;
}

static FString HCI_BuildStepImpactHintFallback(const FHCIAbilityKitAgentPlanStep& Step)
{
	TArray<FString> AssetPaths;
	if (HCI_TryGetStepArgStringArray(Step, TEXT("asset_paths"), AssetPaths))
	{
		if (AssetPaths.Num() == 1 && AssetPaths[0].Contains(TEXT("{{")))
		{
			return TEXT("影响范围：将基于前序扫描结果确定（管道输入）。");
		}

		int32 ExplicitAssetCount = 0;
		int32 ExplicitDirectoryCount = 0;
		for (const FString& Path : AssetPaths)
		{
			if (Path.Contains(TEXT("{{")))
			{
				continue;
			}

			if (Path.StartsWith(TEXT("/Game/")) && Path.Contains(TEXT(".")))
			{
				++ExplicitAssetCount;
			}
			else if (Path.StartsWith(TEXT("/Game/")))
			{
				++ExplicitDirectoryCount;
			}
		}

		if (ExplicitAssetCount > 0)
		{
			return FString::Printf(TEXT("影响数量：预计修改 %d 个资产（基于计划参数）。"), ExplicitAssetCount);
		}
		if (ExplicitDirectoryCount > 0)
		{
			return FString::Printf(TEXT("影响范围：目录输入 %d 项（执行时统计具体资产数量）。"), ExplicitDirectoryCount);
		}
	}

	const FString Directory = HCI_GetStepArgString(Step, TEXT("directory"));
	if (!Directory.IsEmpty())
	{
		return FString::Printf(TEXT("影响范围：目录 %s（执行时统计具体数量）。"), *Directory);
	}

	return FString();
}

static FHCIAbilityKitAgentUiProgressState HCI_MakeProgressState(
	const bool bVisible,
	const bool bIndeterminate,
	const float Percent01,
	const FString& Label)
{
	FHCIAbilityKitAgentUiProgressState State;
	State.bVisible = bVisible;
	State.bIndeterminate = bIndeterminate;
	State.Percent01 = FMath::Clamp(Percent01, 0.0f, 1.0f);
	State.Label = Label;
	return State;
}

static FHCIAbilityKitAgentUiProgressState HCI_BuildProgressStateForSessionState(const EHCIAbilityKitAgentSessionState State)
{
	switch (State)
	{
	case EHCIAbilityKitAgentSessionState::Idle:
		return HCI_MakeProgressState(false, false, 0.0f, TEXT(""));
	case EHCIAbilityKitAgentSessionState::Thinking:
		return HCI_MakeProgressState(true, true, 0.10f, TEXT("进度：正在规划..."));
	case EHCIAbilityKitAgentSessionState::PlanReady:
		return HCI_MakeProgressState(true, false, 0.35f, TEXT("进度：计划已生成"));
	case EHCIAbilityKitAgentSessionState::AutoExecuteReadOnly:
		return HCI_MakeProgressState(true, false, 0.45f, TEXT("进度：只读计划准备执行"));
	case EHCIAbilityKitAgentSessionState::AwaitUserConfirm:
		return HCI_MakeProgressState(true, false, 0.40f, TEXT("进度：等待确认执行"));
	case EHCIAbilityKitAgentSessionState::Executing:
		return HCI_MakeProgressState(true, true, 0.65f, TEXT("进度：执行中..."));
	case EHCIAbilityKitAgentSessionState::Summarizing:
		return HCI_MakeProgressState(true, true, 0.90f, TEXT("进度：整理结果摘要..."));
	case EHCIAbilityKitAgentSessionState::Completed:
		return HCI_MakeProgressState(true, false, 1.0f, TEXT("进度：完成"));
	case EHCIAbilityKitAgentSessionState::Failed:
		return HCI_MakeProgressState(true, false, 1.0f, TEXT("进度：失败"));
	case EHCIAbilityKitAgentSessionState::Cancelled:
		return HCI_MakeProgressState(true, false, 0.0f, TEXT("进度：已取消"));
	default:
		return HCI_MakeProgressState(false, false, 0.0f, TEXT(""));
	}
}

static FString HCI_BuildStepActionHintForChat(const FHCIAbilityKitAgentPlanStep& Step)
{
	if (Step.ToolName == TEXT("SearchPath"))
	{
		return TEXT("正在定位目录...");
	}
	if (Step.ToolName == TEXT("ScanAssets"))
	{
		return TEXT("正在扫描资产...");
	}
	if (Step.ToolName == TEXT("ScanMeshTriangleCount"))
	{
		return TEXT("正在统计面数...");
	}
	if (Step.ToolName == TEXT("ScanLevelMeshRisks"))
	{
		return TEXT("正在扫描场景风险...");
	}
	if (Step.ToolName == TEXT("SetTextureMaxSize"))
	{
		return TEXT("正在调整贴图分辨率...");
	}
	if (Step.ToolName == TEXT("SetMeshLODGroup"))
	{
		return TEXT("正在设置模型细节等级组...");
	}
	if (Step.ToolName == TEXT("NormalizeAssetNamingByMetadata"))
	{
		return TEXT("正在按规范命名并归档资产...");
	}

	FString Summary = HCI_BuildStepSummaryFallback(Step);
	Summary.TrimStartAndEndInline();
	if (Summary.IsEmpty())
	{
		return TEXT("正在执行步骤...");
	}
	if (!Summary.EndsWith(TEXT("...")) && !Summary.EndsWith(TEXT("…")))
	{
		Summary += TEXT("...");
	}
	return Summary;
}

static bool HCI_Subsystem_TryLocateActorByPathCameraFocus(const FString& ActorPath, FString& OutReason)
{
	if (!GEditor)
	{
		OutReason = TEXT("g_editor_unavailable");
		return false;
	}

	UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
	if (!EditorWorld)
	{
		OutReason = TEXT("editor_world_unavailable");
		return false;
	}

	for (TActorIterator<AActor> It(EditorWorld); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}

		if (Actor->GetPathName().Equals(ActorPath, ESearchCase::CaseSensitive))
		{
			GEditor->MoveViewportCamerasToActor(*Actor, false);
			OutReason = TEXT("ok");
			return true;
		}
	}

	OutReason = TEXT("actor_not_found");
	return false;
}

static bool HCI_Subsystem_TryLocateAssetSyncBrowser(const FString& AssetPath, FString& OutReason)
{
	if (!GEditor)
	{
		OutReason = TEXT("g_editor_unavailable");
		return false;
	}

	FString ObjectPath = AssetPath;
	if (AssetPath.StartsWith(TEXT("/Game/")) && !AssetPath.Contains(TEXT(".")))
	{
		int32 LastSlash = INDEX_NONE;
		if (AssetPath.FindLastChar(TEXT('/'), LastSlash) && LastSlash + 1 < AssetPath.Len())
		{
			const FString AssetName = AssetPath.Mid(LastSlash + 1);
			ObjectPath = FString::Printf(TEXT("%s.%s"), *AssetPath, *AssetName);
		}
	}

	FSoftObjectPath SoftPath(ObjectPath);
	UObject* Object = SoftPath.ResolveObject();
	if (!Object)
	{
		Object = SoftPath.TryLoad();
	}

	if (!Object)
	{
		OutReason = TEXT("asset_load_failed");
		return false;
	}

	TArray<UObject*> Objects;
	Objects.Add(Object);
	GEditor->SyncBrowserToObjects(Objects);
	OutReason = TEXT("ok");
	return true;
}

static FString HCI_IndentAsYamlLiteralBlock(const FString& Text, const FString& Indent)
{
	TArray<FString> Lines;
	Text.ParseIntoArrayLines(Lines, false);
	FString Out;
	for (const FString& Line : Lines)
	{
		Out += Indent;
		Out += Line;
		Out += TEXT("\n");
	}
	return Out;
}

static bool HCI_TryLoadProjectRulesMarkdown(FString& OutText, bool& bOutTruncated, FString& OutError)
{
	OutText.Reset();
	OutError.Reset();
	bOutTruncated = false;

	const FString RulesPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("HCIAbilityKit/Rules/Project_Rules.md"));
	if (!FPaths::FileExists(RulesPath))
	{
		OutError = TEXT("project_rules_md_not_found");
		return false;
	}

	FString Raw;
	if (!FFileHelper::LoadFileToString(Raw, *RulesPath))
	{
		OutError = FString::Printf(TEXT("project_rules_md_read_failed path=%s"), *RulesPath);
		return false;
	}

	Raw.TrimStartAndEndInline();
	if (Raw.IsEmpty())
	{
		OutError = TEXT("project_rules_md_empty");
		return false;
	}

	// Keep injection bounded: we want rules to be editable by non-programmers, but never let it explode prompt size.
	const int32 MaxChars = 12000;
	if (Raw.Len() > MaxChars)
	{
		bOutTruncated = true;
		Raw.LeftInline(MaxChars);
	}

	OutText = MoveTemp(Raw);
	return true;
}

static bool HCI_TryLoadLatestIngestManifest(FHCIAbilityKitIngestManifest& OutManifest, FString& OutError)
{
	OutManifest = FHCIAbilityKitIngestManifest();
	OutError.Reset();

	const FString IngestRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("HCIAbilityKit/Ingest"));
	FString ManifestPath;
	if (!FHCIAbilityKitIngestManifest::TryFindLatestManifestUnderRoot(IngestRoot, ManifestPath, OutError))
	{
		return false;
	}

	return FHCIAbilityKitIngestManifest::TryLoadFromFile(ManifestPath, OutManifest, OutError);
}

static FString HCI_BuildStageNExtraEnvContextText()
{
	TArray<FString> Blocks;

	{
		FString RulesText;
		bool bTruncated = false;
		FString Error;
		if (HCI_TryLoadProjectRulesMarkdown(RulesText, bTruncated, Error))
		{
			FString Block;
			Block += TEXT("project_rules_md: |\n");
			Block += HCI_IndentAsYamlLiteralBlock(RulesText, TEXT("  "));
			if (bTruncated)
			{
				Block += TEXT("  (truncated)\n");
			}
			Blocks.Add(MoveTemp(Block));
		}
	}

	{
		FHCIAbilityKitIngestManifest Manifest;
		FString Error;
		if (HCI_TryLoadLatestIngestManifest(Manifest, Error))
		{
			Blocks.Add(Manifest.BuildEnvContextSnippet());
		}
	}

	FString Out = FString::Join(Blocks, TEXT("\n"));
	Out.TrimStartAndEndInline();
	return Out;
}

static bool HCI_ShouldAugmentChatInputWithLatestIngestRoot(const FString& UserText)
{
	// Even if user already provided a /Game/... target (e.g. archive destination),
	// we still want to inject the latest ingest root as the SOURCE scope when they say "刚导入/最新批次".
	if (UserText.Contains(TEXT("latest_ingest_root=")) || UserText.Contains(TEXT("最近导入批次目录：")))
	{
		return false;
	}
	return
		UserText.Contains(TEXT("刚导入")) ||
		UserText.Contains(TEXT("最新批次")) ||
		UserText.Contains(TEXT("最新导入")) ||
		UserText.Contains(TEXT("latest ingest")) ||
		UserText.Contains(TEXT("latest batch"));
}

static bool HCI_TryAugmentChatInputWithLatestIngestRoot(const FString& UserText, FString& OutAugmented, FString& OutIngestRoot)
{
	OutAugmented = UserText;
	OutIngestRoot.Reset();

	if (!HCI_ShouldAugmentChatInputWithLatestIngestRoot(UserText))
	{
		return false;
	}

	FHCIAbilityKitIngestManifest Manifest;
	FString Error;
	if (!HCI_TryLoadLatestIngestManifest(Manifest, Error))
	{
		return false;
	}

	const FString Root = Manifest.SuggestedUnrealTargetRoot.TrimStartAndEnd();
	if (!Root.StartsWith(TEXT("/Game/")))
	{
		return false;
	}
	if (UserText.Contains(Root))
	{
		// User already provided it; no need to augment.
		return false;
	}

	OutIngestRoot = Root;
	// Make ingest root the FIRST /Game/... token so both auto env scan and keyword fallback pick correct scan root.
	OutAugmented = FString::Printf(TEXT("最近导入批次目录：%s。%s"), *Root, *UserText);
	return true;
}
}

void UHCIAbilityKitAgentSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	CommandRegistry.Reset();
	CommandRegistry.Add(TEXT("chat_submit"), UHCIAbilityKitAgentCommand_ChatPlanAndSummary::StaticClass());

	ReloadQuickCommands();
	SetCurrentState(EHCIAbilityKitAgentSessionState::Idle);
	ClearLocateTargets();
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
	CurrentState = EHCIAbilityKitAgentSessionState::Idle;
	CurrentProgressState = FHCIAbilityKitAgentUiProgressState();
	CurrentActivityHint.Reset();
	LastExecutionLocateTargets.Reset();
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

	FString EffectiveInput = TrimmedInput;
	FString DetectedIngestRoot;
	if (HCI_TryAugmentChatInputWithLatestIngestRoot(TrimmedInput, EffectiveInput, DetectedIngestRoot))
	{
		EmitStatus(FString::Printf(TEXT("已识别最近导入批次目录：%s"), *DetectedIngestRoot));
		UE_LOG(LogTemp, Display, TEXT("[HCIAbilityKit][AgentChatUI][Env] detected_latest_ingest_root=%s"), *DetectedIngestRoot);
	}

	EmitUserLine(TrimmedInput);
	EmitAssistantLine(TEXT("已发送，等待规划结果..."));
	bHasLastPlan = false;
	LastPlan = FHCIAbilityKitAgentPlan();
	LastRouteReason.Reset();
	LastPlannerMetadata = FHCIAbilityKitAgentPlannerResultMetadata();
	ClearLocateTargets();
	SetCurrentState(EHCIAbilityKitAgentSessionState::Thinking);
	SetActivityHint(TEXT("思考中..."));

	FHCIAbilityKitAgentCommandContext Context;
	Context.InputParam = EffectiveInput;
	Context.SourceTag = SourceTag;
	Context.ExtraEnvContextText = HCI_BuildStageNExtraEnvContextText();
	if (!ExecuteRegisteredCommand(TEXT("chat_submit"), Context))
	{
		SetCurrentState(EHCIAbilityKitAgentSessionState::Failed);
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

EHCIAbilityKitAgentSessionState UHCIAbilityKitAgentSubsystem::GetCurrentState() const
{
	return CurrentState;
}

FString UHCIAbilityKitAgentSubsystem::GetCurrentStateLabel() const
{
	return HCI_SessionStateToLabel(CurrentState);
}

bool UHCIAbilityKitAgentSubsystem::CanCommitLastPlanFromChat() const
{
	return bHasLastPlan && CurrentState == EHCIAbilityKitAgentSessionState::AwaitUserConfirm;
}

bool UHCIAbilityKitAgentSubsystem::CanCancelPendingPlanFromChat() const
{
	return bHasLastPlan && CurrentState == EHCIAbilityKitAgentSessionState::AwaitUserConfirm;
}

void UHCIAbilityKitAgentSubsystem::GetCurrentProgressState(FHCIAbilityKitAgentUiProgressState& OutState) const
{
	OutState = CurrentProgressState;
}

void UHCIAbilityKitAgentSubsystem::GetCurrentActivityHint(FString& OutHint) const
{
	OutHint = CurrentActivityHint;
}

void UHCIAbilityKitAgentSubsystem::GetLastExecutionLocateTargets(TArray<FHCIAbilityKitAgentUiLocateTarget>& OutTargets) const
{
	OutTargets = LastExecutionLocateTargets;
}

bool UHCIAbilityKitAgentSubsystem::BuildLastPlanCardLines(TArray<FString>& OutLines) const
{
	OutLines.Reset();
	if (!bHasLastPlan)
	{
		return false;
	}

	if (CurrentState == EHCIAbilityKitAgentSessionState::AwaitUserConfirm && IsWriteLikePlan(LastPlan))
	{
		int32 WriteStepCount = 0;
		int32 DestructiveStepCount = 0;
		int32 ReadOnlyPrepCount = 0;
		for (const FHCIAbilityKitAgentPlanStep& Step : LastPlan.Steps)
		{
			if (Step.bRequiresConfirm || HCI_Subsystem_IsWriteLikeRisk(Step.RiskLevel))
			{
				++WriteStepCount;
				if (Step.RiskLevel == EHCIAbilityKitAgentPlanRiskLevel::Destructive)
				{
					++DestructiveStepCount;
				}
			}
			else
			{
				++ReadOnlyPrepCount;
			}
		}

		OutLines.Add(TEXT("审查卡：待确认执行（写操作）"));
		OutLines.Add(FString::Printf(
			TEXT("request_id=%s | intent=%s | route_reason=%s"),
			LastPlan.RequestId.IsEmpty() ? TEXT("-") : *LastPlan.RequestId,
			LastPlan.Intent.IsEmpty() ? TEXT("-") : *LastPlan.Intent,
			LastRouteReason.IsEmpty() ? TEXT("-") : *LastRouteReason));
		OutLines.Add(FString::Printf(
			TEXT("总步骤=%d | 写步骤=%d | 高风险=%d | 前置只读步骤=%d"),
			LastPlan.Steps.Num(),
			WriteStepCount,
			DestructiveStepCount,
			ReadOnlyPrepCount));
		OutLines.Add(TEXT("请检查关键修改步骤、风险与影响范围；点击“确认执行”后才会真实修改资产。"));

		int32 ReviewIndex = 0;
		for (const FHCIAbilityKitAgentPlanStep& Step : LastPlan.Steps)
		{
			if (!(Step.bRequiresConfirm || HCI_Subsystem_IsWriteLikeRisk(Step.RiskLevel)))
			{
				continue;
			}

			++ReviewIndex;
			const FString Summary = BuildStepDisplaySummaryForUi(Step);
			const FString IntentReason = BuildStepIntentReasonForUi(Step);
			const FString RiskWarning = BuildStepRiskWarningForUi(Step);
			const FString ImpactHint = BuildStepImpactHintForUi(Step);

			OutLines.Add(FString::Printf(TEXT("%d. %s"), ReviewIndex, *Summary));
			OutLines.Add(FString::Printf(
				TEXT("   step_id=%s | risk=%s"),
				Step.StepId.IsEmpty() ? TEXT("-") : *Step.StepId,
				*FHCIAbilityKitAgentPlanContract::RiskLevelToString(Step.RiskLevel)));
			if (!ImpactHint.IsEmpty())
			{
				OutLines.Add(FString::Printf(TEXT("   %s"), *ImpactHint));
			}
			if (!IntentReason.IsEmpty())
			{
				OutLines.Add(FString::Printf(TEXT("   原因：%s"), *IntentReason));
			}
			if (!RiskWarning.IsEmpty())
			{
				OutLines.Add(FString::Printf(TEXT("   风险：%s"), *RiskWarning));
			}
		}

		return true;
	}

	OutLines.Add(FString::Printf(
		TEXT("state=%s | request_id=%s | intent=%s | steps=%d"),
		*GetCurrentStateLabel(),
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
		const FString Summary = BuildStepDisplaySummaryForUi(Step);
		const FString IntentReason = BuildStepIntentReasonForUi(Step);
		const FString RiskWarning = BuildStepRiskWarningForUi(Step);
		OutLines.Add(FString::Printf(
			TEXT("%d. %s"),
			Index + 1,
			*Summary));
		OutLines.Add(FString::Printf(
			TEXT("   step_id=%s | risk=%s"),
			Step.StepId.IsEmpty() ? TEXT("-") : *Step.StepId,
			*FHCIAbilityKitAgentPlanContract::RiskLevelToString(Step.RiskLevel)));
		if (!IntentReason.IsEmpty())
		{
			OutLines.Add(FString::Printf(TEXT("   原因：%s"), *IntentReason));
		}
		if (!RiskWarning.IsEmpty())
		{
			OutLines.Add(FString::Printf(TEXT("   风险：%s"), *RiskWarning));
		}
	}

	return true;
}

static int32 HCI_RiskRankForApprovalCard(const EHCIAbilityKitAgentPlanRiskLevel RiskLevel)
{
	switch (RiskLevel)
	{
	case EHCIAbilityKitAgentPlanRiskLevel::Destructive:
		return 30;
	case EHCIAbilityKitAgentPlanRiskLevel::Write:
		return 20;
	case EHCIAbilityKitAgentPlanRiskLevel::ReadOnly:
		return 10;
	default:
		return 0;
	}
}

bool UHCIAbilityKitAgentSubsystem::BuildLastPlanApprovalCard(FHCIAbilityKitAgentUiApprovalCard& OutCard) const
{
	OutCard = FHCIAbilityKitAgentUiApprovalCard();
	if (!bHasLastPlan)
	{
		return false;
	}

	if (!(CurrentState == EHCIAbilityKitAgentSessionState::AwaitUserConfirm && IsWriteLikePlan(LastPlan)))
	{
		return false;
	}

	const FHCIAbilityKitAgentPlanStep* BestStep = nullptr;
	int32 BestRank = -1;
	for (const FHCIAbilityKitAgentPlanStep& Step : LastPlan.Steps)
	{
		if (!(Step.bRequiresConfirm || HCI_Subsystem_IsWriteLikeRisk(Step.RiskLevel)))
		{
			continue;
		}

		// Prefer destructive > write, then earlier steps. requires_confirm adds a small bias.
		const int32 Rank = HCI_RiskRankForApprovalCard(Step.RiskLevel) + (Step.bRequiresConfirm ? 1 : 0);
		if (Rank > BestRank)
		{
			BestRank = Rank;
			BestStep = &Step;
		}
	}

	if (!BestStep)
	{
		return false;
	}

	OutCard.Title = TEXT("请确认修改");
	OutCard.KeyAction = HCI_SanitizeUiTextForArtist(BuildStepDisplaySummaryForUi(*BestStep));
	OutCard.ImpactHint = HCI_SanitizeUiTextForArtist(BuildStepImpactHintForUi(*BestStep));

	// Only show warning for write-like steps; hide read-only warnings (no decision value).
	const bool bWriteLike = BestStep->bRequiresConfirm || HCI_Subsystem_IsWriteLikeRisk(BestStep->RiskLevel);
	if (bWriteLike && (BestStep->RiskLevel == EHCIAbilityKitAgentPlanRiskLevel::Write || BestStep->RiskLevel == EHCIAbilityKitAgentPlanRiskLevel::Destructive))
	{
		OutCard.Warning = HCI_SanitizeUiTextForArtist(BuildStepRiskWarningForUi(*BestStep));
	}

	OutCard.Title.TrimStartAndEndInline();
	OutCard.KeyAction.TrimStartAndEndInline();
	OutCard.ImpactHint.TrimStartAndEndInline();
	OutCard.Warning.TrimStartAndEndInline();

	if (OutCard.KeyAction.IsEmpty())
	{
		OutCard.KeyAction = TEXT("将对项目资产进行修改");
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

	if (!CanCommitLastPlanFromChat())
	{
		EmitAssistantLine(TEXT("当前计划无需确认执行，或尚未进入可确认状态。"));
		return false;
	}

	EmitAssistantLine(TEXT("已确认执行，开始应用写操作计划..."));
	return ExecuteLastPlan(EHCIAbilityKitAgentPlanExecutionBranch::AwaitUserConfirm, TEXT("manual_commit"));
}

bool UHCIAbilityKitAgentSubsystem::CancelPendingPlanFromChat()
{
	if (ActiveCommand != nullptr)
	{
		EmitStatus(TEXT("状态：忙碌"));
		EmitAssistantLine(TEXT("已有请求执行中，请等待当前请求完成后再取消。"));
		return false;
	}

	if (!bHasLastPlan)
	{
		EmitAssistantLine(TEXT("暂无可取消计划，请先发送一条请求。"));
		return false;
	}

	if (!CanCancelPendingPlanFromChat())
	{
		EmitAssistantLine(TEXT("当前没有处于待确认状态的写计划。"));
		return false;
	}

	UE_LOG(LogTemp, Display, TEXT("[HCIAbilityKit][AgentChatStateMachine] trigger=manual_cancel state=await_user_confirm"));
	EmitAssistantLine(TEXT("已取消本次执行（未触发真实写操作）。"));
	SetCurrentState(EHCIAbilityKitAgentSessionState::Cancelled);
	return true;
}

bool UHCIAbilityKitAgentSubsystem::TryLocateLastExecutionTargetByIndex(const int32 TargetIndex)
{
	if (!LastExecutionLocateTargets.IsValidIndex(TargetIndex))
	{
		EmitAssistantLine(TEXT("结果定位失败：目标索引无效。"));
		return false;
	}

	const FHCIAbilityKitAgentUiLocateTarget& Target = LastExecutionLocateTargets[TargetIndex];
	FString LocateReason;
	bool bLocateOk = false;
	if (Target.Kind == EHCIAbilityKitAgentUiLocateTargetKind::Actor)
	{
		bLocateOk = HCI_Subsystem_TryLocateActorByPathCameraFocus(Target.TargetPath, LocateReason);
	}
	else
	{
		bLocateOk = HCI_Subsystem_TryLocateAssetSyncBrowser(Target.TargetPath, LocateReason);
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("[HCIAbilityKit][AgentChatUI][Locate] ok=%s kind=%s target=%s reason=%s"),
		bLocateOk ? TEXT("true") : TEXT("false"),
		Target.Kind == EHCIAbilityKitAgentUiLocateTargetKind::Actor ? TEXT("actor") : TEXT("asset"),
		*Target.TargetPath,
		*LocateReason);

	EmitAssistantLine(
		bLocateOk
			? FString::Printf(TEXT("已定位：%s"), *Target.DisplayLabel)
			: FString::Printf(TEXT("定位失败：%s（%s）"), *Target.DisplayLabel, *LocateReason));
	return bLocateOk;
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

bool UHCIAbilityKitAgentSubsystem::IsWriteLikePlan(const FHCIAbilityKitAgentPlan& Plan)
{
	for (const FHCIAbilityKitAgentPlanStep& Step : Plan.Steps)
	{
		if (Step.bRequiresConfirm || HCI_Subsystem_IsWriteLikeRisk(Step.RiskLevel))
		{
			return true;
		}
	}
	return false;
}

EHCIAbilityKitAgentPlanExecutionBranch UHCIAbilityKitAgentSubsystem::ClassifyPlanExecutionBranch(const FHCIAbilityKitAgentPlan& Plan)
{
	return IsWriteLikePlan(Plan)
		? EHCIAbilityKitAgentPlanExecutionBranch::AwaitUserConfirm
		: EHCIAbilityKitAgentPlanExecutionBranch::AutoExecuteReadOnly;
}

FString UHCIAbilityKitAgentSubsystem::BuildStepDisplaySummaryForUi(const FHCIAbilityKitAgentPlanStep& Step)
{
	return HCI_BuildStepSummaryFallback(Step);
}

FString UHCIAbilityKitAgentSubsystem::BuildStepIntentReasonForUi(const FHCIAbilityKitAgentPlanStep& Step)
{
	return HCI_BuildStepIntentReasonFallback(Step);
}

FString UHCIAbilityKitAgentSubsystem::BuildStepRiskWarningForUi(const FHCIAbilityKitAgentPlanStep& Step)
{
	return HCI_BuildStepRiskWarningFallback(Step);
}

FString UHCIAbilityKitAgentSubsystem::BuildStepImpactHintForUi(const FHCIAbilityKitAgentPlanStep& Step)
{
	return HCI_BuildStepImpactHintFallback(Step);
}

void UHCIAbilityKitAgentSubsystem::SetProgressState(const FHCIAbilityKitAgentUiProgressState& InState)
{
	CurrentProgressState = InState;
	OnProgressStateChanged.Broadcast(CurrentProgressState);
}

void UHCIAbilityKitAgentSubsystem::SetActivityHint(const FString& InHint)
{
	CurrentActivityHint = InHint;
	OnActivityHintChanged.Broadcast(CurrentActivityHint);
}

void UHCIAbilityKitAgentSubsystem::ClearLocateTargets()
{
	LastExecutionLocateTargets.Reset();
	OnLocateTargetsChanged.Broadcast();
}

void UHCIAbilityKitAgentSubsystem::SetLocateTargetsFromExecutionReport(const FHCIAbilityKitAgentPlanExecutionReport& Report)
{
	LastExecutionLocateTargets.Reset();
	LastExecutionLocateTargets.Reserve(Report.LocateTargets.Num());

	for (const FHCIAbilityKitAgentExecutionLocateTarget& Target : Report.LocateTargets)
	{
		FHCIAbilityKitAgentUiLocateTarget& UiTarget = LastExecutionLocateTargets.AddDefaulted_GetRef();
		UiTarget.Kind = (Target.Kind == EHCIAbilityKitAgentExecutionLocateTargetKind::Actor)
			? EHCIAbilityKitAgentUiLocateTargetKind::Actor
			: EHCIAbilityKitAgentUiLocateTargetKind::Asset;
		UiTarget.DisplayLabel = Target.DisplayLabel;
		UiTarget.TargetPath = Target.TargetPath;
		UiTarget.SourceToolName = Target.SourceToolName;
		UiTarget.SourceEvidenceKey = Target.SourceEvidenceKey;
	}

	OnLocateTargetsChanged.Broadcast();
}

void UHCIAbilityKitAgentSubsystem::SetCurrentState(const EHCIAbilityKitAgentSessionState NewState)
{
	CurrentState = NewState;
	OnSessionStateChanged.Broadcast(CurrentState);
	EmitStatus(BuildStateStatusText(CurrentState));
	SetProgressState(HCI_BuildProgressStateForSessionState(CurrentState));
	switch (CurrentState)
	{
	case EHCIAbilityKitAgentSessionState::Idle:
		SetActivityHint(TEXT(""));
		break;
	case EHCIAbilityKitAgentSessionState::Thinking:
		SetActivityHint(TEXT("思考中..."));
		break;
	case EHCIAbilityKitAgentSessionState::PlanReady:
		SetActivityHint(TEXT("计划已生成，正在准备..."));
		break;
	case EHCIAbilityKitAgentSessionState::AutoExecuteReadOnly:
		SetActivityHint(TEXT("正在自动执行只读操作..."));
		break;
	case EHCIAbilityKitAgentSessionState::AwaitUserConfirm:
		SetActivityHint(TEXT("已生成待确认计划，请确认后执行。"));
		break;
	case EHCIAbilityKitAgentSessionState::Executing:
		SetActivityHint(TEXT("正在执行..."));
		break;
	case EHCIAbilityKitAgentSessionState::Summarizing:
		SetActivityHint(TEXT("正在整理结果..."));
		break;
	case EHCIAbilityKitAgentSessionState::Completed:
		SetActivityHint(TEXT("已完成"));
		break;
	case EHCIAbilityKitAgentSessionState::Failed:
		SetActivityHint(TEXT("执行失败"));
		break;
	case EHCIAbilityKitAgentSessionState::Cancelled:
		SetActivityHint(TEXT("已取消"));
		break;
	default:
		break;
	}
}

FString UHCIAbilityKitAgentSubsystem::BuildStateStatusText(const EHCIAbilityKitAgentSessionState State) const
{
	switch (State)
	{
	case EHCIAbilityKitAgentSessionState::Idle:
		return TEXT("状态：空闲");
	case EHCIAbilityKitAgentSessionState::Thinking:
		return TEXT("状态：思考中");
	case EHCIAbilityKitAgentSessionState::PlanReady:
		return TEXT("状态：计划已就绪");
	case EHCIAbilityKitAgentSessionState::AutoExecuteReadOnly:
		return TEXT("状态：只读计划自动执行");
	case EHCIAbilityKitAgentSessionState::AwaitUserConfirm:
		return TEXT("状态：等待用户确认");
	case EHCIAbilityKitAgentSessionState::Executing:
		return TEXT("状态：执行中");
	case EHCIAbilityKitAgentSessionState::Summarizing:
		return TEXT("状态：结果整理中");
	case EHCIAbilityKitAgentSessionState::Completed:
		return TEXT("状态：已完成");
	case EHCIAbilityKitAgentSessionState::Failed:
		return TEXT("状态：执行失败");
	case EHCIAbilityKitAgentSessionState::Cancelled:
		return TEXT("状态：已取消");
	default:
		return TEXT("状态：未知");
	}
}

bool UHCIAbilityKitAgentSubsystem::ExecuteLastPlan(
	const EHCIAbilityKitAgentPlanExecutionBranch Branch,
	const TCHAR* TriggerTag)
{
	if (!bHasLastPlan)
	{
		SetCurrentState(EHCIAbilityKitAgentSessionState::Failed);
		EmitAssistantLine(TEXT("执行失败：未找到可执行计划。"));
		return false;
	}

	SetCurrentState(EHCIAbilityKitAgentSessionState::Executing);
	SetProgressState(HCI_MakeProgressState(
		true,
		true,
		0.60f,
		FString::Printf(TEXT("进度：执行计划（0/%d 步）..."), LastPlan.Steps.Num())));
	FHCIAbilityKitAgentPlanExecutionReport Report;
	const bool bRunOk = FHCIAbilityKitAgentPlanPreviewWindow::ExecutePlan(
		LastPlan,
		false,
		Branch == EHCIAbilityKitAgentPlanExecutionBranch::AwaitUserConfirm,
		Report,
		[this](const int32 StepIndex, const int32 TotalSteps, const FHCIAbilityKitAgentPlanStep& Step)
		{
			const FString ActionHint = HCI_BuildStepActionHintForChat(Step);
			AsyncTask(ENamedThreads::GameThread, [this, StepIndex, TotalSteps, ActionHint]()
			{
				if (!IsValid(this))
				{
					return;
				}
				SetActivityHint(ActionHint);
				SetProgressState(HCI_MakeProgressState(
					true,
					true,
					0.60f,
					FString::Printf(TEXT("进度：执行计划（%d/%d）%s"), StepIndex + 1, TotalSteps, *ActionHint)));
			});
		});

	SetLocateTargetsFromExecutionReport(Report);
	const int32 ExecutedSteps = Report.SucceededSteps + Report.FailedSteps;
	SetProgressState(HCI_MakeProgressState(
		true,
		false,
		0.88f,
		FString::Printf(TEXT("进度：执行完成（%d/%d 步），正在整理结果..."), ExecutedSteps, LastPlan.Steps.Num())));
	SetCurrentState(EHCIAbilityKitAgentSessionState::Summarizing);
	EmitAssistantLine(Report.SummaryText);
	if (!Report.SearchPathEvidenceText.IsEmpty())
	{
		EmitAssistantLine(Report.SearchPathEvidenceText);
	}
	if (Report.LocateTargets.Num() > 0)
	{
		EmitAssistantLine(FString::Printf(TEXT("已生成 %d 个可定位结果项，可在结果面板点击定位。"), Report.LocateTargets.Num()));
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("[HCIAbilityKit][AgentChatStateMachine] trigger=%s branch=%s run_ok=%s terminal=%s reason=%s"),
		TriggerTag == nullptr ? TEXT("-") : TriggerTag,
		Branch == EHCIAbilityKitAgentPlanExecutionBranch::AwaitUserConfirm ? TEXT("await_user_confirm") : TEXT("auto_read_only"),
		bRunOk ? TEXT("true") : TEXT("false"),
		*Report.TerminalStatus,
		*Report.TerminalReason);

	SetCurrentState(bRunOk ? EHCIAbilityKitAgentSessionState::Completed : EHCIAbilityKitAgentSessionState::Failed);
	return bRunOk;
}

void UHCIAbilityKitAgentSubsystem::HandleCommandCompleted(const FHCIAbilityKitAgentCommandResult& Result)
{
	ActiveCommand = nullptr;
	bool bCanProcessPlanBranch = false;
	const bool bHasExecutablePlanPayload = Result.bHasPlanPayload && Result.Plan.Steps.Num() > 0;
	if (bHasExecutablePlanPayload)
	{
		bHasLastPlan = true;
		LastPlan = Result.Plan;
		LastRouteReason = Result.RouteReason;
		LastPlannerMetadata = Result.PlannerMetadata;
		SetCurrentState(EHCIAbilityKitAgentSessionState::PlanReady);
		OnPlanReady.Broadcast();
		bCanProcessPlanBranch = true;
	}

	if (Result.bSuccess)
	{
		EmitAssistantLine(Result.Message);
		OnSummaryReceived.Broadcast(Result.Message);
	}
	else if (Result.bSummaryFailure)
	{
		EmitAssistantLine(FString::Printf(TEXT("摘要生成失败：%s"), *Result.Message));
	}
	else
	{
		EmitAssistantLine(Result.Message);
		SetCurrentState(EHCIAbilityKitAgentSessionState::Failed);
		return;
	}

	if (!bCanProcessPlanBranch)
	{
		if (Result.bSuccess && !Result.Message.IsEmpty())
		{
			SetActivityHint(TEXT("回复已生成"));
		}
		SetCurrentState(EHCIAbilityKitAgentSessionState::Completed);
		return;
	}

	const EHCIAbilityKitAgentPlanExecutionBranch Branch = ClassifyPlanExecutionBranch(LastPlan);
	if (Branch == EHCIAbilityKitAgentPlanExecutionBranch::AutoExecuteReadOnly)
	{
		SetCurrentState(EHCIAbilityKitAgentSessionState::AutoExecuteReadOnly);
		if (LastPlan.Steps.Num() > 0)
		{
			SetActivityHint(HCI_BuildStepActionHintForChat(LastPlan.Steps[0]));
		}
		EmitAssistantLine(TEXT("检测到只读计划，正在自动执行并更新证据链..."));
		ExecuteLastPlan(Branch, TEXT("auto_read_only"));
		return;
	}

	SetCurrentState(EHCIAbilityKitAgentSessionState::AwaitUserConfirm);
	SetActivityHint(TEXT("已生成待确认计划，请确认后执行。"));
	EmitAssistantLine(TEXT("计划包含写操作，请在聊天窗口点击“确认并执行”继续。"));
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
