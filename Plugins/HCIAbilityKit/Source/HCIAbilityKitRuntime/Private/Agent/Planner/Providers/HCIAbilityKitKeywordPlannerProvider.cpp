#include "Agent/Planner/Providers/HCIAbilityKitKeywordPlannerProvider.h"

#include "Agent/Planner/HCIAbilityKitAgentPlan.h"
#include "Agent/Tools/HCIAbilityKitToolRegistry.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace
{
static EHCIAbilityKitAgentPlanRiskLevel HCI_ToPlanRiskLevel(EHCIAbilityKitToolCapability Capability)
{
	switch (Capability)
	{
	case EHCIAbilityKitToolCapability::ReadOnly:
		return EHCIAbilityKitAgentPlanRiskLevel::ReadOnly;
	case EHCIAbilityKitToolCapability::Write:
		return EHCIAbilityKitAgentPlanRiskLevel::Write;
	case EHCIAbilityKitToolCapability::Destructive:
		return EHCIAbilityKitAgentPlanRiskLevel::Destructive;
	default:
		return EHCIAbilityKitAgentPlanRiskLevel::ReadOnly;
	}
}

static bool HCI_StepFromTool(
	const FHCIAbilityKitToolRegistry& ToolRegistry,
	const TCHAR* StepId,
	const TCHAR* ToolName,
	const TSharedPtr<FJsonObject>& Args,
	const TArray<FString>& ExpectedEvidence,
	const FHCIAbilityKitAgentPlanStep::FUiPresentation* UiPresentation,
	FHCIAbilityKitAgentPlanStep& OutStep,
	FString& OutError)
{
	const FHCIAbilityKitToolDescriptor* Tool = ToolRegistry.FindTool(FName(ToolName));
	if (!Tool)
	{
		OutError = FString::Printf(TEXT("tool not found in whitelist: %s"), ToolName);
		return false;
	}

	OutStep = FHCIAbilityKitAgentPlanStep();
	OutStep.StepId = StepId;
	OutStep.ToolName = Tool->ToolName;
	OutStep.Args = Args.IsValid() ? Args : MakeShared<FJsonObject>();
	OutStep.RiskLevel = HCI_ToPlanRiskLevel(Tool->Capability);
	OutStep.bRequiresConfirm = (Tool->Capability != EHCIAbilityKitToolCapability::ReadOnly) || Tool->bDestructive;
	OutStep.RollbackStrategy = TEXT("all_or_nothing");
	OutStep.ExpectedEvidence = ExpectedEvidence;
	if (UiPresentation != nullptr)
	{
		OutStep.UiPresentation = *UiPresentation;
	}
	return true;
}

static bool HCI_StepFromTool(
	const FHCIAbilityKitToolRegistry& ToolRegistry,
	const TCHAR* StepId,
	const TCHAR* ToolName,
	const TSharedPtr<FJsonObject>& Args,
	const TArray<FString>& ExpectedEvidence,
	FHCIAbilityKitAgentPlanStep& OutStep,
	FString& OutError)
{
	return HCI_StepFromTool(ToolRegistry, StepId, ToolName, Args, ExpectedEvidence, nullptr, OutStep, OutError);
}

static FString HCI_NormalizeText(const FString& InText)
{
	FString Text = InText;
	Text.TrimStartAndEndInline();
	Text.ToLowerInline();
	return Text;
}

static bool HCI_TextContainsAny(const FString& Text, std::initializer_list<const TCHAR*> Keywords)
{
	for (const TCHAR* Keyword : Keywords)
	{
		if (Text.Contains(Keyword))
		{
			return true;
		}
	}
	return false;
}

static bool HCI_BuildPreparedMessageOnlyPlan(
	const FString& RequestId,
	const FString& Intent,
	const FString& RouteReason,
	const FString& AssistantMessage,
	FHCIAbilityKitAgentPlan& OutPlan,
	FString& OutRouteReason,
	FString& OutError)
{
	OutPlan = FHCIAbilityKitAgentPlan();
	OutPlan.RequestId = RequestId;
	OutPlan.Intent = Intent;
	OutPlan.AssistantMessage = AssistantMessage;
	OutRouteReason = RouteReason;
	FString ContractError;
	if (!FHCIAbilityKitAgentPlanContract::ValidateMinimalContract(OutPlan, ContractError))
	{
		OutError = ContractError;
		return false;
	}
	OutError.Reset();
	return true;
}

static bool HCI_IsGamePathTerminalChar(const TCHAR Ch)
{
	return FChar::IsWhitespace(Ch) ||
		   Ch == TCHAR('"') ||
		   Ch == TCHAR('\'') ||
		   Ch == TCHAR(',') ||
		   Ch == TCHAR(0xFF0C) || // '，'
		   Ch == TCHAR(';') ||
		   Ch == TCHAR(0xFF1B) || // '；'
		   Ch == TCHAR(':') ||
		   Ch == TCHAR(0xFF1A) || // '：'
		   Ch == TCHAR(0x3002) || // '。'
		   Ch == TCHAR(')') ||
		   Ch == TCHAR('(') ||
		   Ch == TCHAR(']') ||
		   Ch == TCHAR('[');
}

static bool HCI_TryExtractFirstGamePathToken(const FString& UserText, FString& OutToken)
{
	OutToken.Reset();
	int32 Start = UserText.Find(TEXT("/Game/"), ESearchCase::IgnoreCase);
	if (Start == INDEX_NONE)
	{
		// Accept common user typo: missing leading slash ("Game/...").
		Start = UserText.Find(TEXT("Game/"), ESearchCase::IgnoreCase);
		if (Start == INDEX_NONE)
		{
			return false;
		}
	}

	int32 End = Start;
	while (End < UserText.Len() && !HCI_IsGamePathTerminalChar(UserText[End]))
	{
		++End;
	}

	OutToken = UserText.Mid(Start, End - Start).TrimStartAndEnd();
	if (OutToken.StartsWith(TEXT("Game/"), ESearchCase::IgnoreCase))
	{
		OutToken = FString::Printf(TEXT("/%s"), *OutToken);
	}
	return !OutToken.IsEmpty();
}

static void HCI_ExtractAllGamePathTokens(const FString& UserText, TArray<FString>& OutTokens)
{
	OutTokens.Reset();

	int32 SearchFrom = 0;
	while (SearchFrom < UserText.Len())
	{
		int32 Start = UserText.Find(TEXT("/Game/"), ESearchCase::IgnoreCase, ESearchDir::FromStart, SearchFrom);
		if (Start == INDEX_NONE)
		{
			Start = UserText.Find(TEXT("Game/"), ESearchCase::IgnoreCase, ESearchDir::FromStart, SearchFrom);
			if (Start == INDEX_NONE)
			{
				return;
			}
		}

		int32 End = Start;
		while (End < UserText.Len() && !HCI_IsGamePathTerminalChar(UserText[End]))
		{
			++End;
		}

		FString Token = UserText.Mid(Start, End - Start).TrimStartAndEnd();
		if (Token.StartsWith(TEXT("Game/"), ESearchCase::IgnoreCase))
		{
			Token = FString::Printf(TEXT("/%s"), *Token);
		}
		if (!Token.IsEmpty())
		{
			OutTokens.Add(MoveTemp(Token));
		}

		SearchFrom = FMath::Max(End, Start + 5);
	}
}

static FString HCI_DeriveDirectoryFromPathToken(const FString& PathToken)
{
	FString Candidate = PathToken;
	Candidate.TrimStartAndEndInline();
	if (Candidate.IsEmpty())
	{
		return FString();
	}

	if (Candidate.EndsWith(TEXT("/")))
	{
		Candidate.LeftChopInline(1, false);
	}

	const int32 DotIndex = Candidate.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromStart);
	const bool bObjectPath = (DotIndex != INDEX_NONE);
	if (bObjectPath)
	{
		Candidate = Candidate.Left(DotIndex);
	}
	else
	{
		return Candidate;
	}

	const int32 LastSlash = Candidate.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	if (LastSlash > 0)
	{
		return Candidate.Left(LastSlash);
	}
	return Candidate;
}

static FString HCI_DetermineEnvScanRoot(const FString& UserText)
{
	FString PathToken;
	if (!HCI_TryExtractFirstGamePathToken(UserText, PathToken))
	{
		return FString();
	}

	const FString Derived = HCI_DeriveDirectoryFromPathToken(PathToken);
	if (Derived.StartsWith(TEXT("/Game/")))
	{
		return Derived;
	}
	return FString();
}


static TSharedPtr<FJsonObject> HCI_MakeNamingArgsWithPipeline(
	const FString& ScanStepId,
	const FString& TargetRoot)
{
	TSharedPtr<FJsonObject> Args = MakeShared<FJsonObject>();
	// Field-level pipeline variable: executor will resolve it into a concrete string array before tool execution.
	Args->SetStringField(TEXT("asset_paths"), FString::Printf(TEXT("{{%s.asset_paths}}"), *ScanStepId));
	Args->SetStringField(TEXT("metadata_source"), TEXT("auto"));
	Args->SetStringField(TEXT("prefix_mode"), TEXT("auto_by_asset_class"));
	Args->SetStringField(TEXT("target_root"), TargetRoot.IsEmpty() ? TEXT("/Game/Art/Organized") : TargetRoot);
	return Args;
}

static TSharedPtr<FJsonObject> HCI_MakeLevelRiskArgs()
{
	TSharedPtr<FJsonObject> Args = MakeShared<FJsonObject>();
	Args->SetStringField(TEXT("scope"), TEXT("selected"));
	TArray<TSharedPtr<FJsonValue>> Checks;
	Checks.Add(MakeShared<FJsonValueString>(TEXT("missing_collision")));
	Checks.Add(MakeShared<FJsonValueString>(TEXT("default_material")));
	Args->SetArrayField(TEXT("checks"), Checks);
	Args->SetNumberField(TEXT("max_actor_count"), 500);
	return Args;
}

static TSharedPtr<FJsonObject> HCI_MakeTextureComplianceArgs()
{
	TSharedPtr<FJsonObject> Args = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> AssetPaths;
	AssetPaths.Add(MakeShared<FJsonValueString>(TEXT("/Game/Art/Trees/T_Tree_01_D.T_Tree_01_D")));
	Args->SetArrayField(TEXT("asset_paths"), AssetPaths);
	Args->SetNumberField(TEXT("max_size"), 1024);
	return Args;
}

static TSharedPtr<FJsonObject> HCI_MakeLodComplianceArgs()
{
	TSharedPtr<FJsonObject> Args = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> AssetPaths;
	AssetPaths.Add(MakeShared<FJsonValueString>(TEXT("/Game/Art/Props/SM_Rock_01.SM_Rock_01")));
	Args->SetArrayField(TEXT("asset_paths"), AssetPaths);
	Args->SetStringField(TEXT("lod_group"), TEXT("SmallProp"));
	return Args;
}

static TSharedPtr<FJsonObject> HCI_MakeScanAssetsArgs()
{
	TSharedPtr<FJsonObject> Args = MakeShared<FJsonObject>();
	Args->SetStringField(TEXT("directory"), TEXT("/Game/Temp"));
	return Args;
}

static TSharedPtr<FJsonObject> HCI_MakeScanAssetsArgsWithDirectory(const FString& Directory)
{
	TSharedPtr<FJsonObject> Args = MakeShared<FJsonObject>();
	Args->SetStringField(TEXT("directory"), Directory);
	return Args;
}

static TSharedPtr<FJsonObject> HCI_MakeScanMeshTriangleCountArgsWithDirectory(const FString& Directory)
{
	TSharedPtr<FJsonObject> Args = MakeShared<FJsonObject>();
	Args->SetStringField(TEXT("directory"), Directory);
	return Args;
}

static TSharedPtr<FJsonObject> HCI_MakeSearchPathArgs(const FString& Keyword)
{
	TSharedPtr<FJsonObject> Args = MakeShared<FJsonObject>();
	Args->SetStringField(TEXT("keyword"), Keyword);
	return Args;
}

static FString HCI_PickSearchKeywordForFolderIntent(const FString& NormalizedText)
{
	if (HCI_TextContainsAny(NormalizedText, {TEXT("角色"), TEXT("character")}))
	{
		return TEXT("角色");
	}
	if (HCI_TextContainsAny(NormalizedText, {TEXT("美术"), TEXT("艺术"), TEXT("art")}))
	{
		return TEXT("Art");
	}
	if (HCI_TextContainsAny(NormalizedText, {TEXT("临时"), TEXT("temp"), TEXT("tmp")}))
	{
		return TEXT("Temp");
	}
	return TEXT("Temp");
}


static bool HCI_BuildKeywordPlan(
	const FString& UserText,
	const FString& RequestId,
	const FHCIAbilityKitToolRegistry& ToolRegistry,
	FHCIAbilityKitAgentPlan& OutPlan,
	FString& OutRouteReason,
	FString& OutError)
{
	OutPlan = FHCIAbilityKitAgentPlan();
	OutPlan.RequestId = RequestId;
	OutRouteReason.Reset();
	OutError.Reset();

	const FString Text = HCI_NormalizeText(UserText);
	if (Text.IsEmpty())
	{
		OutError = TEXT("empty_input");
		return false;
	}

	const bool bNamingIntent =
		HCI_TextContainsAny(Text, {TEXT("整理"), TEXT("归档"), TEXT("重命名"), TEXT("命名"), TEXT("organize"), TEXT("archive"), TEXT("rename"), TEXT("naming")}) &&
		HCI_TextContainsAny(Text, {TEXT("临时"), TEXT("temp"), TEXT("tmp")});
	const bool bDirectoryNamingIntent =
		HCI_TextContainsAny(Text, {TEXT("整理"), TEXT("归档"), TEXT("重命名"), TEXT("命名"), TEXT("organize"), TEXT("archive"), TEXT("rename"), TEXT("naming")}) &&
		HCI_TextContainsAny(Text, {TEXT("目录"), TEXT("文件夹"), TEXT("folder"), TEXT("directory")});
	const bool bExplicitPathNamingIntent =
		HCI_TextContainsAny(Text, {TEXT("整理"), TEXT("归档"), TEXT("重命名"), TEXT("命名"), TEXT("规范"), TEXT("规范化"), TEXT("organize"), TEXT("archive"), TEXT("rename"), TEXT("naming")}) &&
		(HCI_TextContainsAny(Text, {TEXT("扫描"), TEXT("scan"), TEXT("inspect")}) || UserText.Contains(TEXT("/Game/"), ESearchCase::IgnoreCase) || UserText.Contains(TEXT("Game/"), ESearchCase::IgnoreCase)) &&
		(UserText.Contains(TEXT("/Game/"), ESearchCase::IgnoreCase) || UserText.Contains(TEXT("Game/"), ESearchCase::IgnoreCase));

	const bool bLevelRiskIntent =
		HCI_TextContainsAny(Text, {TEXT("关卡"), TEXT("场景"), TEXT("level")}) &&
		HCI_TextContainsAny(Text, {TEXT("碰撞"), TEXT("collision"), TEXT("材质丢失"), TEXT("默认材质"), TEXT("default material")});

	const bool bMeshTriangleCountIntent =
		HCI_TextContainsAny(Text, {TEXT("面数"), TEXT("triangle"), TEXT("triangles"), TEXT("poly")}) &&
		HCI_TextContainsAny(Text, {TEXT("检查"), TEXT("扫描"), TEXT("统计"), TEXT("分析"), TEXT("查看"), TEXT("check"), TEXT("scan"), TEXT("inspect"), TEXT("analyze")});

	const bool bAssetComplianceIntent =
		HCI_TextContainsAny(Text, {TEXT("贴图"), TEXT("texture"), TEXT("分辨率"), TEXT("npot"), TEXT("面数"), TEXT("lod")});
	const bool bIdentityIntent =
		HCI_TextContainsAny(Text, {TEXT("你是谁"), TEXT("你是哪个"), TEXT("你是哪位"), TEXT("你是做什么"), TEXT("你是什么"), TEXT("who are you"), TEXT("what are you")});
	const bool bCapabilityIntent =
		HCI_TextContainsAny(Text, {TEXT("你能做什么"), TEXT("你有哪些能力"), TEXT("有什么能力"), TEXT("有啥能力"), TEXT("能做什么"), TEXT("可以做什么"), TEXT("会什么"), TEXT("what can you do"), TEXT("capability"), TEXT("abilities")});
	const bool bGreetingIntent =
		HCI_TextContainsAny(Text, {TEXT("你好"), TEXT("早上好"), TEXT("晚上好"), TEXT("嗨"), TEXT("hello"), TEXT("hi")});
	const bool bLikelyChitchatIntent = bIdentityIntent || bCapabilityIntent || bGreetingIntent;

	if (bLikelyChitchatIntent)
	{
		if (bIdentityIntent)
		{
			return HCI_BuildPreparedMessageOnlyPlan(
				RequestId,
				TEXT("chat_identity"),
				TEXT("prepared_message_only_identity"),
				TEXT("我是 HCIAbilityKit 的编辑器内资产审计与执行助理。我的职责是帮你在 UE 编辑器里做资产扫描、风险检查、批量合规修复与命名归档，并在涉及写操作时先给出确认。"),
				OutPlan,
				OutRouteReason,
				OutError);
		}

		if (bCapabilityIntent)
		{
			return HCI_BuildPreparedMessageOnlyPlan(
				RequestId,
				TEXT("chat_capability"),
				TEXT("prepared_message_only_capability"),
				TEXT("我可以帮你做四类事：1）资产扫描与统计（如面数、目录资产清单）；2）关卡风险检查（缺碰撞、默认材质等）；3）资产合规修改（如贴图最大尺寸、StaticMesh LOD Group）；4）按元数据规范命名并归档资产。读操作默认自动执行，写操作会先让你确认。"),
				OutPlan,
				OutRouteReason,
				OutError);
		}

		return HCI_BuildPreparedMessageOnlyPlan(
			RequestId,
			TEXT("chat_greeting"),
			TEXT("prepared_message_only_greeting"),
			TEXT("你好，我可以直接在 UE 编辑器里帮你检查资产问题、统计模型面数、扫描关卡风险，以及在你确认后执行批量修复。你可以直接说目标目录或想检查的问题。"),
			OutPlan,
			OutRouteReason,
			OutError);
	}
	else if (bNamingIntent || bExplicitPathNamingIntent)
	{
		OutPlan.Intent = TEXT("normalize_temp_assets_by_metadata");
		OutRouteReason = bNamingIntent ? TEXT("naming_traceability_temp_assets") : TEXT("naming_traceability_explicit_paths");

		// If user provided explicit /Game/... paths, prefer them over the default /Game/Temp.
		TArray<FString> PathTokens;
		HCI_ExtractAllGamePathTokens(UserText, PathTokens);
		const FString ScanRoot = PathTokens.Num() > 0 ? HCI_DetermineEnvScanRoot(PathTokens[0]) : FString();
		FString TargetRoot;
		if (PathTokens.Num() > 1)
		{
			TargetRoot = HCI_DeriveDirectoryFromPathToken(PathTokens.Last());
		}
		if (TargetRoot.IsEmpty())
		{
			TargetRoot = TEXT("/Game/Art/Organized");
		}

		FHCIAbilityKitAgentPlanStep& ScanStep = OutPlan.Steps.AddDefaulted_GetRef();
		if (!HCI_StepFromTool(
				ToolRegistry,
				TEXT("step_1_scan"),
					TEXT("ScanAssets"),
					ScanRoot.IsEmpty() ? HCI_MakeScanAssetsArgs() : HCI_MakeScanAssetsArgsWithDirectory(ScanRoot),
					{TEXT("scan_root"), TEXT("asset_count"), TEXT("asset_paths"), TEXT("result")},
					ScanStep,
					OutError))
			{
				return false;
		}

		FHCIAbilityKitAgentPlanStep& Step = OutPlan.Steps.AddDefaulted_GetRef();
		if (!HCI_StepFromTool(
				ToolRegistry,
				TEXT("step_2_normalize"),
					TEXT("NormalizeAssetNamingByMetadata"),
					HCI_MakeNamingArgsWithPipeline(TEXT("step_1_scan"), TargetRoot),
					{TEXT("result"), TEXT("proposed_renames"), TEXT("proposed_moves"), TEXT("affected_count")},
					Step,
					OutError))
			{
				return false;
		}
		}
		else if (bDirectoryNamingIntent)
		{
			OutPlan.Intent = TEXT("scan_assets");
			OutRouteReason = TEXT("naming_traceability_search_then_scan");

			const FString SearchKeyword = HCI_PickSearchKeywordForFolderIntent(Text);

				FHCIAbilityKitAgentPlanStep& SearchStep = OutPlan.Steps.AddDefaulted_GetRef();
				if (!HCI_StepFromTool(
						ToolRegistry,
						TEXT("step_1_search"),
							TEXT("SearchPath"),
							HCI_MakeSearchPathArgs(SearchKeyword),
							{TEXT("matched_directories"), TEXT("best_directory"), TEXT("keyword_normalized"), TEXT("result")},
							SearchStep,
							OutError))
				{
					return false;
			}

				FHCIAbilityKitAgentPlanStep& ScanStep = OutPlan.Steps.AddDefaulted_GetRef();
				if (!HCI_StepFromTool(
						ToolRegistry,
						TEXT("step_2_scan"),
							TEXT("ScanAssets"),
							HCI_MakeScanAssetsArgsWithDirectory(TEXT("{{step_1_search.matched_directories[0]}}")),
							{TEXT("scan_root"), TEXT("asset_count"), TEXT("asset_paths"), TEXT("result")},
							ScanStep,
							OutError))
				{
					return false;
			}
		}
		else if (bLevelRiskIntent)
		{
			OutPlan.Intent = TEXT("scan_level_mesh_risks");
			OutRouteReason = TEXT("level_risk_collision_material");

			FHCIAbilityKitAgentPlanStep& Step = OutPlan.Steps.AddDefaulted_GetRef();
			if (!HCI_StepFromTool(
					ToolRegistry,
					TEXT("s1"),
					TEXT("ScanLevelMeshRisks"),
					HCI_MakeLevelRiskArgs(),
					{TEXT("actor_path"), TEXT("issue"), TEXT("evidence")},
					Step,
					OutError))
				{
					return false;
			}
		}
	else if (bMeshTriangleCountIntent)
	{
		OutPlan.Intent = TEXT("scan_mesh_triangle_count");
		OutRouteReason = TEXT("mesh_triangle_count_analysis");

		FString PathToken;
		FString Directory = TEXT("/Game/Temp");
		if (HCI_TryExtractFirstGamePathToken(UserText, PathToken))
		{
			const FString DerivedDirectory = HCI_DeriveDirectoryFromPathToken(PathToken);
			if (DerivedDirectory.StartsWith(TEXT("/Game/")))
			{
				Directory = DerivedDirectory;
			}
		}

		FHCIAbilityKitAgentPlanStep& Step = OutPlan.Steps.AddDefaulted_GetRef();
		if (!HCI_StepFromTool(
				ToolRegistry,
				TEXT("s1"),
				TEXT("ScanMeshTriangleCount"),
				HCI_MakeScanMeshTriangleCountArgsWithDirectory(Directory),
				{TEXT("scan_root"), TEXT("scanned_count"), TEXT("mesh_count"), TEXT("max_triangle_count"), TEXT("max_triangle_asset"), TEXT("top_meshes"), TEXT("result")},
				Step,
				OutError))
		{
			return false;
		}
	}
	else if (bAssetComplianceIntent)
	{
		OutPlan.Intent = TEXT("batch_fix_asset_compliance");
		OutRouteReason = TEXT("asset_compliance_texture_lod");

		FHCIAbilityKitAgentPlanStep* TextureStep = nullptr;
		if (HCI_TextContainsAny(Text, {TEXT("贴图"), TEXT("texture"), TEXT("分辨率"), TEXT("npot")}))
		{
			TextureStep = &OutPlan.Steps.AddDefaulted_GetRef();
			if (!HCI_StepFromTool(
					ToolRegistry,
					TEXT("s1"),
						TEXT("SetTextureMaxSize"),
						HCI_MakeTextureComplianceArgs(),
						{TEXT("target_max_size"), TEXT("scanned_count"), TEXT("modified_count"), TEXT("failed_count"), TEXT("modified_assets"), TEXT("failed_assets"), TEXT("result")},
						*TextureStep,
						OutError))
				{
					return false;
			}
		}

		if (HCI_TextContainsAny(Text, {TEXT("面数"), TEXT("lod")}))
		{
			const TCHAR* StepId = TextureStep ? TEXT("s2") : TEXT("s1");
			FHCIAbilityKitAgentPlanStep& LodStep = OutPlan.Steps.AddDefaulted_GetRef();
			if (!HCI_StepFromTool(
					ToolRegistry,
					StepId,
						TEXT("SetMeshLODGroup"),
						HCI_MakeLodComplianceArgs(),
						{TEXT("target_lod_group"), TEXT("scanned_count"), TEXT("modified_count"), TEXT("failed_count"), TEXT("modified_assets"), TEXT("failed_assets"), TEXT("result")},
						LodStep,
						OutError))
				{
					return false;
			}
		}

		if (OutPlan.Steps.Num() == 0)
		{
			OutError = TEXT("asset_compliance_route_produced_no_steps");
			return false;
		}
	}
	else
	{
		return HCI_BuildPreparedMessageOnlyPlan(
			RequestId,
			TEXT("chat_clarify"),
			TEXT("prepared_message_only_clarify"),
			TEXT("我这次没有明确识别到可执行的资产审计指令。你可以直接说要检查什么（例如“检查 /Game/HCI 目录下的模型面数”）或要修改什么（例如“把 /Game/__HCI_Auto 下贴图最大尺寸设为 1024”）。"),
			OutPlan,
			OutRouteReason,
			OutError);
	}

	FString ContractError;
	if (!FHCIAbilityKitAgentPlanContract::ValidateMinimalContract(OutPlan, ContractError))
	{
		OutError = ContractError;
		return false;
	}

	return true;
}

}

const TCHAR* FHCIAbilityKitKeywordPlannerProvider::GetName() const
{
	return TEXT("keyword_fallback");
}

bool FHCIAbilityKitKeywordPlannerProvider::IsNetworkBacked() const
{
	return false;
}

bool FHCIAbilityKitKeywordPlannerProvider::BuildPlan(
	const FString& UserText,
	const FString& RequestId,
	const FHCIAbilityKitToolRegistry& ToolRegistry,
	const FHCIAbilityKitAgentPlannerBuildOptions& Options,
	FHCIAbilityKitAgentPlan& OutPlan,
	FString& OutRouteReason,
	FHCIAbilityKitAgentPlannerResultMetadata& OutMetadata,
	FString& OutError)
{
	(void)Options;
	OutMetadata = FHCIAbilityKitAgentPlannerResultMetadata();
	OutMetadata.PlannerProvider = TEXT("keyword_fallback");
	OutMetadata.ProviderMode = TEXT("keyword");
	OutMetadata.bFallbackUsed = false;
	OutMetadata.FallbackReason = TEXT("none");
	OutMetadata.ErrorCode = TEXT("-");
	return HCI_BuildKeywordPlan(UserText, RequestId, ToolRegistry, OutPlan, OutRouteReason, OutError);
}

void FHCIAbilityKitKeywordPlannerProvider::BuildPlanAsync(
	const FString& UserText,
	const FString& RequestId,
	const FHCIAbilityKitToolRegistry& ToolRegistry,
	const FHCIAbilityKitAgentPlannerBuildOptions& Options,
	TFunction<void(bool, FHCIAbilityKitAgentPlan, FString, FHCIAbilityKitAgentPlannerResultMetadata, FString)>&& OnComplete)
{
	FHCIAbilityKitAgentPlan Plan;
	FString RouteReason;
	FHCIAbilityKitAgentPlannerResultMetadata Metadata;
	FString Error;
	const bool bBuilt = BuildPlan(UserText, RequestId, ToolRegistry, Options, Plan, RouteReason, Metadata, Error);
	OnComplete(bBuilt, MoveTemp(Plan), MoveTemp(RouteReason), MoveTemp(Metadata), MoveTemp(Error));
}
