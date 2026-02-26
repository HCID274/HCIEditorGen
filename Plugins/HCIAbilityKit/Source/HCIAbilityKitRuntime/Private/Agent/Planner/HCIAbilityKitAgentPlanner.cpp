#include "Agent/Planner/HCIAbilityKitAgentPlanner.h"

#include "Agent/LLM/HCIAbilityKitAgentLlmClient.h"
#include "Agent/Planner/HCIAbilityKitAgentPlan.h"
#include "Agent/Planner/HCIAbilityKitAgentPlanValidator.h"
#include "Agent/LLM/HCIAbilityKitAgentPromptBuilder.h"
#include "Agent/Tools/HCIAbilityKitToolRegistry.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Templates/Atomic.h"
#include "Internationalization/Regex.h"

DEFINE_LOG_CATEGORY_STATIC(LogHCIAbilityKitAgentPlanner, Log, All);

namespace
{
static const TCHAR* const HCI_LlmProviderName = TEXT("llm");
static const TCHAR* const HCI_KeywordProviderName = TEXT("keyword_fallback");
static const TCHAR* const HCI_FallbackReasonNone = TEXT("none");
static const TCHAR* const HCI_FallbackReasonTimeout = TEXT("llm_timeout");
static const TCHAR* const HCI_FallbackReasonInvalidJson = TEXT("llm_invalid_json");
static const TCHAR* const HCI_FallbackReasonContractInvalid = TEXT("llm_contract_invalid");
static const TCHAR* const HCI_FallbackReasonEmptyResponse = TEXT("llm_empty_response");
static const TCHAR* const HCI_FallbackReasonCircuitOpen = TEXT("llm_circuit_open");
static const TCHAR* const HCI_FallbackReasonHttpError = TEXT("llm_http_error");
static const TCHAR* const HCI_FallbackReasonConfigMissing = TEXT("llm_config_missing");
static const TCHAR* const HCI_FallbackReasonSyncRealHttpDeprecated = TEXT("llm_sync_real_http_deprecated");

struct FHCIAbilityKitAgentPlannerRuntimeState
{
	int32 ConsecutiveLlmFailures = 0;
	int32 CircuitOpenRemainingRequests = 0;
	FHCIAbilityKitAgentPlannerMetricsSnapshot Metrics;
};

static FHCIAbilityKitAgentPlannerRuntimeState GHCIAbilityKitAgentPlannerRuntimeState;

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

static bool HCI_IsVariableTemplateString(const FString& InText)
{
	const FRegexPattern Pattern(TEXT("^\\{\\{\\s*[A-Za-z0-9_]+\\.[A-Za-z0-9_]+(?:\\[\\d+\\])?\\s*\\}\\}$"));
	FRegexMatcher Matcher(Pattern, InText.TrimStartAndEnd());
	return Matcher.FindNext();
}

static bool HCI_TryParseJsonStringArrayLiteral(const FString& InText, TArray<FString>& OutValues)
{
	OutValues.Reset();
	TArray<TSharedPtr<FJsonValue>> JsonArray;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InText);
	if (!FJsonSerializer::Deserialize(Reader, JsonArray))
	{
		return false;
	}

	for (const TSharedPtr<FJsonValue>& Value : JsonArray)
	{
		if (!Value.IsValid() || Value->Type != EJson::String)
		{
			OutValues.Reset();
			return false;
		}

		FString Parsed = Value->AsString();
		Parsed.TrimStartAndEndInline();
		if (!Parsed.IsEmpty())
		{
			OutValues.Add(Parsed);
		}
	}
	return OutValues.Num() > 0;
}

static bool HCI_TryParseOptionalUiPresentation(
	const TSharedPtr<FJsonObject>& StepObject,
	const int32 StepIndex,
	FHCIAbilityKitAgentPlanStep::FUiPresentation& OutUi,
	FString& OutError)
{
	OutUi = FHCIAbilityKitAgentPlanStep::FUiPresentation();
	if (!StepObject.IsValid() || !StepObject->HasField(TEXT("ui_presentation")))
	{
		return true;
	}

	const TSharedPtr<FJsonObject>* UiObjectPtr = nullptr;
	if (!StepObject->TryGetObjectField(TEXT("ui_presentation"), UiObjectPtr) || UiObjectPtr == nullptr || !UiObjectPtr->IsValid())
	{
		OutError = FString::Printf(TEXT("Invalid field type: ui_presentation (llm_plan.steps[%d])"), StepIndex);
		return false;
	}

	const TSharedPtr<FJsonObject>& UiObject = *UiObjectPtr;
	auto TryReadOptionalStringField = [&](const TCHAR* FieldName, bool& bOutHasValue, FString& OutValue) -> bool
	{
		bOutHasValue = false;
		OutValue.Reset();
		if (!UiObject->HasField(FieldName))
		{
			return true;
		}

		FString Parsed;
		if (!UiObject->TryGetStringField(FieldName, Parsed))
		{
			OutError = FString::Printf(
				TEXT("Invalid field type: ui_presentation.%s (llm_plan.steps[%d])"),
				FieldName,
				StepIndex);
			return false;
		}

		bOutHasValue = true;
		OutValue = Parsed;
		return true;
	};

	if (!TryReadOptionalStringField(TEXT("step_summary"), OutUi.bHasStepSummary, OutUi.StepSummary))
	{
		return false;
	}
	if (!TryReadOptionalStringField(TEXT("intent_reason"), OutUi.bHasIntentReason, OutUi.IntentReason))
	{
		return false;
	}
	if (!TryReadOptionalStringField(TEXT("risk_warning"), OutUi.bHasRiskWarning, OutUi.RiskWarning))
	{
		return false;
	}

	return true;
}

static void HCI_ParseLooseStringList(const FString& InText, TArray<FString>& OutValues)
{
	OutValues.Reset();

	FString Trimmed = InText;
	Trimmed.TrimStartAndEndInline();
	if (Trimmed.IsEmpty())
	{
		return;
	}

	if (HCI_IsVariableTemplateString(Trimmed))
	{
		OutValues.Add(Trimmed);
		return;
	}

	if (HCI_TryParseJsonStringArrayLiteral(Trimmed, OutValues))
	{
		return;
	}

	FString Delimiter;
	if (Trimmed.Contains(TEXT("|")))
	{
		Delimiter = TEXT("|");
	}
	else if (Trimmed.Contains(TEXT(",")))
	{
		Delimiter = TEXT(",");
	}

	if (Delimiter.IsEmpty())
	{
		OutValues.Add(Trimmed);
		return;
	}

	TArray<FString> Tokens;
	Trimmed.ParseIntoArray(Tokens, *Delimiter, true);
	for (FString& Token : Tokens)
	{
		Token.TrimStartAndEndInline();
		if (!Token.IsEmpty())
		{
			OutValues.Add(Token);
		}
	}
}

static TSharedPtr<FJsonValue> HCI_NormalizeJsonValueForArgSchema(
	const FHCIAbilityKitToolArgSchema& ArgSchema,
	const TSharedPtr<FJsonValue>& InValue)
{
	if (!InValue.IsValid())
	{
		return InValue;
	}

	if (ArgSchema.ValueType == EHCIAbilityKitToolArgValueType::StringArray && InValue->Type == EJson::String)
	{
		FString RawString;
		if (!InValue->TryGetString(RawString))
		{
			return InValue;
		}

		TArray<FString> ParsedValues;
		HCI_ParseLooseStringList(RawString, ParsedValues);
		if (ParsedValues.Num() <= 0)
		{
			return InValue;
		}

		TArray<TSharedPtr<FJsonValue>> JsonValues;
		JsonValues.Reserve(ParsedValues.Num());
		for (const FString& ParsedValue : ParsedValues)
		{
			JsonValues.Add(MakeShared<FJsonValueString>(ParsedValue));
		}
		return MakeShared<FJsonValueArray>(JsonValues);
	}

	return InValue;
}

static bool HCI_IsDirectoryStyleRequest(const FString& UserText)
{
	const FString Text = HCI_NormalizeText(UserText);
	return HCI_TextContainsAny(Text, {TEXT("目录"), TEXT("文件夹"), TEXT("folder"), TEXT("directory"), TEXT("临时"), TEXT("temp"), TEXT("tmp")});
}

static bool HCI_IsGamePathTerminalChar(const TCHAR Ch)
{
	return FChar::IsWhitespace(Ch) ||
		   Ch == TCHAR('"') ||
		   Ch == TCHAR('\'') ||
		   Ch == TCHAR(',') ||
		   Ch == TCHAR(';') ||
		   Ch == TCHAR(')') ||
		   Ch == TCHAR('(') ||
		   Ch == TCHAR(']') ||
		   Ch == TCHAR('[');
}

static bool HCI_TryExtractFirstGamePathToken(const FString& UserText, FString& OutToken)
{
	OutToken.Reset();
	const int32 Start = UserText.Find(TEXT("/Game/"), ESearchCase::IgnoreCase);
	if (Start == INDEX_NONE)
	{
		return false;
	}

	int32 End = Start;
	while (End < UserText.Len() && !HCI_IsGamePathTerminalChar(UserText[End]))
	{
		++End;
	}

	OutToken = UserText.Mid(Start, End - Start).TrimStartAndEnd();
	return !OutToken.IsEmpty();
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

static FString HCI_LeafNameFromObjectPath(const FString& ObjectPath)
{
	FString Token = ObjectPath;
	const int32 SlashIndex = Token.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	if (SlashIndex != INDEX_NONE && SlashIndex + 1 < Token.Len())
	{
		Token = Token.Mid(SlashIndex + 1);
	}

	const int32 DotIndex = Token.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromStart);
	if (DotIndex != INDEX_NONE)
	{
		Token = Token.Left(DotIndex);
	}
	return Token;
}

static FString HCI_SerializeEnvContext(
	const FString& ScanRoot,
	const TArray<FHCIAbilityKitAgentPlannerEnvAssetEntry>& Assets)
{
	const int32 MaxRows = 40;
	FString Out = FString::Printf(TEXT("scan_root: %s\nasset_count: %d\nfile_list:\n"), *ScanRoot, Assets.Num());

	const int32 EmitCount = FMath::Min(MaxRows, Assets.Num());
	for (int32 Index = 0; Index < EmitCount; ++Index)
	{
		const FHCIAbilityKitAgentPlannerEnvAssetEntry& Entry = Assets[Index];
		const FString Name = HCI_LeafNameFromObjectPath(Entry.ObjectPath);
		const FString ClassName = Entry.AssetClass.IsEmpty() ? TEXT("Unknown") : Entry.AssetClass;
		const FString SizeField = Entry.SizeBytes >= 0
			? FString::Printf(TEXT("size_bytes=%lld"), static_cast<long long>(Entry.SizeBytes))
			: TEXT("size_bytes=-");
		Out += FString::Printf(
			TEXT("- %s (%s, %s, path=%s)\n"),
			Name.IsEmpty() ? TEXT("-") : *Name,
			*ClassName,
			*SizeField,
			Entry.ObjectPath.IsEmpty() ? TEXT("-") : *Entry.ObjectPath);
	}
	if (Assets.Num() > MaxRows)
	{
		Out += FString::Printf(TEXT("- ... and %d more\n"), Assets.Num() - MaxRows);
	}
	if (Assets.Num() == 0)
	{
		Out += TEXT("- (empty)\n");
	}
	return Out;
}

static bool HCI_TryBuildAutoEnvContext(
	const FString& UserText,
	const FHCIAbilityKitAgentPlannerBuildOptions& Options,
	FString& OutScanRoot,
	FString& OutEnvContext,
	int32& OutAssetCount,
	bool& bOutInjected)
{
	OutScanRoot.Reset();
	OutEnvContext.Reset();
	OutAssetCount = 0;
	bOutInjected = false;

	if (!Options.bEnableAutoEnvContextScan)
	{
		return true;
	}
	if (!HCI_IsDirectoryStyleRequest(UserText))
	{
		return true;
	}
	if (!Options.ScanAssetsForEnvContext)
	{
		return true;
	}

	const FString ScanRoot = HCI_DetermineEnvScanRoot(UserText);
	if (ScanRoot.IsEmpty())
	{
		return true;
	}

	TArray<FHCIAbilityKitAgentPlannerEnvAssetEntry> ScannedAssets;
	FString ScanError;
	if (!Options.ScanAssetsForEnvContext(ScanRoot, ScannedAssets, ScanError))
	{
		OutScanRoot = ScanRoot;
		return true;
	}

	OutScanRoot = ScanRoot;
	OutAssetCount = ScannedAssets.Num();
	OutEnvContext = HCI_SerializeEnvContext(ScanRoot, ScannedAssets);
	bOutInjected = true;
	return true;
}

static TSharedPtr<FJsonObject> HCI_MakeNamingArgs()
{
	TSharedPtr<FJsonObject> Args = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> AssetPaths;
	AssetPaths.Add(MakeShared<FJsonValueString>(TEXT("/Game/Temp/SM_RockTemp_01.SM_RockTemp_01")));
	AssetPaths.Add(MakeShared<FJsonValueString>(TEXT("/Game/Temp/T_DiffuseTemp_01.T_DiffuseTemp_01")));
	Args->SetArrayField(TEXT("asset_paths"), AssetPaths);
	Args->SetStringField(TEXT("metadata_source"), TEXT("auto"));
	Args->SetStringField(TEXT("prefix_mode"), TEXT("auto_by_asset_class"));
	Args->SetStringField(TEXT("target_root"), TEXT("/Game/Art/Organized"));
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

static FHCIAbilityKitAgentPromptBundleOptions HCI_MakePromptBundleOptions(const FHCIAbilityKitAgentPlannerBuildOptions& Options)
{
	FHCIAbilityKitAgentPromptBundleOptions BundleOptions;
	BundleOptions.SkillBundleRelativeDir = Options.PromptBundleRelativeDir;
	BundleOptions.PromptTemplateFileName = Options.PromptTemplateFileName;
	BundleOptions.ToolsSchemaFileName = Options.PromptToolsSchemaFileName;
	return BundleOptions;
}

static bool HCI_TryGetStringArrayField(const TSharedPtr<FJsonObject>& Object, const FString& FieldName, TArray<FString>& OutValues)
{
	OutValues.Reset();
	if (!Object.IsValid() || !Object->HasField(FieldName))
	{
		return false;
	}

	const TSharedPtr<FJsonValue>* FieldValue = Object->Values.Find(FieldName);
	if (FieldValue == nullptr || !FieldValue->IsValid())
	{
		return false;
	}

	if ((*FieldValue)->Type == EJson::String)
	{
		HCI_ParseLooseStringList((*FieldValue)->AsString(), OutValues);
		return OutValues.Num() > 0;
	}

	if ((*FieldValue)->Type != EJson::Array)
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>& Values = (*FieldValue)->AsArray();
	for (const TSharedPtr<FJsonValue>& Value : Values)
	{
		FString StringValue;
		if (!Value.IsValid() || !Value->TryGetString(StringValue))
		{
			return false;
		}
		OutValues.Add(StringValue);
	}
	return true;
}

static bool HCI_TryExtractJsonObjectString(const FString& InText, FString& OutJsonObjectText)
{
	FString Text = InText;
	Text.TrimStartAndEndInline();

	if (Text.StartsWith(TEXT("```")))
	{
		int32 FirstNewLine = INDEX_NONE;
		if (Text.FindChar(TEXT('\n'), FirstNewLine))
		{
			const int32 LastFence = Text.Find(TEXT("```"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			if (LastFence != INDEX_NONE && LastFence > FirstNewLine)
			{
				Text = Text.Mid(FirstNewLine + 1, LastFence - FirstNewLine - 1);
				Text.TrimStartAndEndInline();
			}
		}
	}

	const int32 StartIndex = Text.Find(TEXT("{"), ESearchCase::CaseSensitive, ESearchDir::FromStart);
	const int32 EndIndex = Text.Find(TEXT("}"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	if (StartIndex == INDEX_NONE || EndIndex == INDEX_NONE || EndIndex <= StartIndex)
	{
		return false;
	}

	OutJsonObjectText = Text.Mid(StartIndex, EndIndex - StartIndex + 1);
	return true;
}

static TSharedPtr<FJsonObject> HCI_NormalizeStepArgsBySchema(
	const FHCIAbilityKitToolRegistry& ToolRegistry,
	const FName ToolName,
	const TSharedPtr<FJsonObject>& RawArgsObject)
{
	const FHCIAbilityKitToolDescriptor* Tool = ToolRegistry.FindTool(ToolName);
	if (Tool == nullptr)
	{
		return RawArgsObject.IsValid() ? RawArgsObject : MakeShared<FJsonObject>();
	}

	const TSharedPtr<FJsonObject> SafeRawArgs = RawArgsObject.IsValid() ? RawArgsObject : MakeShared<FJsonObject>();
	const TSharedPtr<FJsonObject> NormalizedArgs = MakeShared<FJsonObject>();

	for (const FHCIAbilityKitToolArgSchema& ArgSchema : Tool->ArgsSchema)
	{
		const FString Key = ArgSchema.ArgName.ToString();
		const TSharedPtr<FJsonValue>* ExistingValue = SafeRawArgs->Values.Find(Key);
		if (ExistingValue != nullptr && ExistingValue->IsValid())
		{
			const TSharedPtr<FJsonValue> NormalizedExistingValue =
				HCI_NormalizeJsonValueForArgSchema(ArgSchema, *ExistingValue);
			NormalizedArgs->SetField(Key, NormalizedExistingValue.IsValid() ? NormalizedExistingValue : *ExistingValue);
			continue;
		}
	}

	return NormalizedArgs;
}

static bool HCI_TryParseVariableTemplateStepId(const FString& InText, FString& OutStepId)
{
	OutStepId.Reset();
	const FString Trimmed = InText.TrimStartAndEnd();
	const FRegexPattern Pattern(TEXT("^\\{\\{\\s*([A-Za-z0-9_]+)\\.[A-Za-z0-9_]+(?:\\[\\d+\\])?\\s*\\}\\}$"));
	FRegexMatcher Matcher(Pattern, Trimmed);
	if (!Matcher.FindNext())
	{
		return false;
	}
	OutStepId = Matcher.GetCaptureGroup(1);
	return !OutStepId.IsEmpty();
}

static void HCI_CollectStepDependencyIdsFromJsonValue(
	const TSharedPtr<FJsonValue>& InValue,
	TSet<FString>& OutStepIds)
{
	if (!InValue.IsValid())
	{
		return;
	}

	switch (InValue->Type)
	{
	case EJson::String:
	{
		FString SourceStepId;
		if (HCI_TryParseVariableTemplateStepId(InValue->AsString(), SourceStepId))
		{
			OutStepIds.Add(SourceStepId);
		}
		break;
	}
	case EJson::Array:
		for (const TSharedPtr<FJsonValue>& Item : InValue->AsArray())
		{
			HCI_CollectStepDependencyIdsFromJsonValue(Item, OutStepIds);
		}
		break;
	case EJson::Object:
	{
		const TSharedPtr<FJsonObject> Obj = InValue->AsObject();
		if (!Obj.IsValid())
		{
			return;
		}
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Obj->Values)
		{
			HCI_CollectStepDependencyIdsFromJsonValue(Pair.Value, OutStepIds);
		}
		break;
	}
	default:
		break;
	}
}

static bool HCI_ReorderPlanStepsByVariableDependencies(
	FHCIAbilityKitAgentPlan& InOutPlan,
	bool& bOutReordered,
	FString& OutError)
{
	bOutReordered = false;
	OutError.Reset();

	const int32 StepCount = InOutPlan.Steps.Num();
	if (StepCount <= 1)
	{
		return true;
	}

	TMap<FString, int32> StepIndexById;
	StepIndexById.Reserve(StepCount);
	for (int32 StepIndex = 0; StepIndex < StepCount; ++StepIndex)
	{
		const FString& StepId = InOutPlan.Steps[StepIndex].StepId;
		if (StepId.IsEmpty())
		{
			continue;
		}
		if (StepIndexById.Contains(StepId))
		{
			OutError = FString::Printf(TEXT("llm_plan_duplicate_step_id step_id=%s"), *StepId);
			return false;
		}
		StepIndexById.Add(StepId, StepIndex);
	}

	TArray<TSet<int32>> OutEdges;
	OutEdges.SetNum(StepCount);
	TArray<int32> InDegree;
	InDegree.Init(0, StepCount);

	for (int32 StepIndex = 0; StepIndex < StepCount; ++StepIndex)
	{
		const FHCIAbilityKitAgentPlanStep& Step = InOutPlan.Steps[StepIndex];
		if (!Step.Args.IsValid())
		{
			continue;
		}

		TSet<FString> DependencyStepIds;
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Step.Args->Values)
		{
			HCI_CollectStepDependencyIdsFromJsonValue(Pair.Value, DependencyStepIds);
		}

		for (const FString& DependencyStepId : DependencyStepIds)
		{
			const int32* DependencyIndexPtr = StepIndexById.Find(DependencyStepId);
			if (DependencyIndexPtr == nullptr)
			{
				OutError = FString::Printf(
					TEXT("llm_plan_variable_source_step_missing step_id=%s referenced_by=%s"),
					*DependencyStepId,
					*Step.StepId);
				return false;
			}

			const int32 DependencyIndex = *DependencyIndexPtr;
			if (DependencyIndex == StepIndex)
			{
				OutError = FString::Printf(
					TEXT("llm_plan_variable_source_step_self_reference step_id=%s"),
					*Step.StepId);
				return false;
			}

			if (!OutEdges[DependencyIndex].Contains(StepIndex))
			{
				OutEdges[DependencyIndex].Add(StepIndex);
				InDegree[StepIndex] += 1;
			}
		}
	}

	TArray<int32> Ready;
	Ready.Reserve(StepCount);
	for (int32 StepIndex = 0; StepIndex < StepCount; ++StepIndex)
	{
		if (InDegree[StepIndex] == 0)
		{
			Ready.Add(StepIndex);
		}
	}
	Ready.Sort();

	TArray<int32> OrderedIndices;
	OrderedIndices.Reserve(StepCount);
	while (Ready.Num() > 0)
	{
		const int32 Current = Ready[0];
		Ready.RemoveAt(0, 1, EAllowShrinking::No);
		OrderedIndices.Add(Current);

		TArray<int32> Dependents = OutEdges[Current].Array();
		Dependents.Sort();
		for (const int32 DependentIndex : Dependents)
		{
			InDegree[DependentIndex] -= 1;
			if (InDegree[DependentIndex] == 0)
			{
				Ready.Add(DependentIndex);
				Ready.Sort();
			}
		}
	}

	if (OrderedIndices.Num() != StepCount)
	{
		OutError = TEXT("llm_plan_variable_dependency_cycle_detected");
		return false;
	}

	bool bOrderChanged = false;
	for (int32 Index = 0; Index < StepCount; ++Index)
	{
		if (OrderedIndices[Index] != Index)
		{
			bOrderChanged = true;
			break;
		}
	}

	if (!bOrderChanged)
	{
		return true;
	}

	TArray<FHCIAbilityKitAgentPlanStep> ReorderedSteps;
	ReorderedSteps.Reserve(StepCount);
	for (const int32 SourceIndex : OrderedIndices)
	{
		ReorderedSteps.Add(InOutPlan.Steps[SourceIndex]);
	}
	InOutPlan.Steps = MoveTemp(ReorderedSteps);
	bOutReordered = true;
	return true;
}

static bool HCI_ValidateBuiltPlanOrSetError(
	const FHCIAbilityKitAgentPlan& Plan,
	const FHCIAbilityKitToolRegistry& ToolRegistry,
	FString& OutError)
{
	FHCIAbilityKitAgentPlanValidationResult Validation;
	if (FHCIAbilityKitAgentPlanValidator::ValidatePlan(Plan, ToolRegistry, Validation))
	{
		return true;
	}

	OutError = FString::Printf(
		TEXT("llm_plan_validation_failed code=%s field=%s reason=%s"),
		Validation.ErrorCode.IsEmpty() ? TEXT("-") : *Validation.ErrorCode,
		Validation.Field.IsEmpty() ? TEXT("-") : *Validation.Field,
		Validation.Reason.IsEmpty() ? TEXT("-") : *Validation.Reason);
	return false;
}

static bool HCI_EnsureScanAssetsFirstForDirectoryIntent(
	const FString& UserText,
	const FHCIAbilityKitToolRegistry& ToolRegistry,
	const bool bForceDirectoryScanFirst,
	FHCIAbilityKitAgentPlan& InOutPlan,
	FString& InOutRouteReason,
	FString& OutError)
{
	OutError.Reset();
	if (!bForceDirectoryScanFirst)
	{
		return true;
	}

	if (!HCI_IsDirectoryStyleRequest(UserText))
	{
		return true;
	}

	const FString IntentLower = InOutPlan.Intent.ToLower();
	const bool bNamingIntent =
		IntentLower.Contains(TEXT("normalize")) ||
		IntentLower.Contains(TEXT("naming")) ||
		IntentLower.Contains(TEXT("rename")) ||
		IntentLower.Contains(TEXT("move"));
	if (!bNamingIntent)
	{
		return true;
	}

	const int32 ExistingIndex = InOutPlan.Steps.IndexOfByPredicate(
		[](const FHCIAbilityKitAgentPlanStep& Step)
		{
			return Step.ToolName == TEXT("ScanAssets");
		});

	if (ExistingIndex == 0)
	{
		return true;
	}

	if (ExistingIndex > 0)
	{
		FHCIAbilityKitAgentPlanStep ExistingStep = InOutPlan.Steps[ExistingIndex];
		InOutPlan.Steps.RemoveAt(ExistingIndex, 1, EAllowShrinking::No);
		InOutPlan.Steps.Insert(MoveTemp(ExistingStep), 0);
	}
	else
	{
		FHCIAbilityKitAgentPlanStep ScanStep;
		if (!HCI_StepFromTool(
				ToolRegistry,
				TEXT("s_scan_assets_preflight"),
				TEXT("ScanAssets"),
				HCI_MakeScanAssetsArgs(),
				{TEXT("scan_root"), TEXT("asset_count"), TEXT("asset_paths"), TEXT("result")},
				ScanStep,
				OutError))
		{
			return false;
		}
		InOutPlan.Steps.Insert(MoveTemp(ScanStep), 0);
	}

	if (!InOutRouteReason.Contains(TEXT("scan_first")))
	{
		InOutRouteReason = InOutRouteReason.IsEmpty()
			? TEXT("directory_scan_first")
			: InOutRouteReason + TEXT("_scan_first");
	}
	return true;
}

static bool HCI_TryBuildPlanFromLlmPlanJson(
	const TSharedPtr<FJsonObject>& LlmPlanObject,
	const FString& RequestId,
	const FHCIAbilityKitToolRegistry& ToolRegistry,
	FHCIAbilityKitAgentPlan& OutPlan,
	FString& OutRouteReason,
	FString& OutError)
{
	OutPlan = FHCIAbilityKitAgentPlan();
	OutPlan.RequestId = RequestId;
	OutError.Reset();
	OutRouteReason = TEXT("llm_real_http_route");

	if (!LlmPlanObject.IsValid())
	{
		OutError = TEXT("llm_plan_json_root_invalid");
		return false;
	}

	if (!LlmPlanObject->TryGetStringField(TEXT("intent"), OutPlan.Intent) || OutPlan.Intent.IsEmpty())
	{
		OutError = TEXT("llm_plan.intent missing");
		return false;
	}

	LlmPlanObject->TryGetStringField(TEXT("route_reason"), OutRouteReason);
	if (OutRouteReason.IsEmpty())
	{
		OutRouteReason = TEXT("llm_real_http_route");
	}

	LlmPlanObject->TryGetStringField(TEXT("assistant_message"), OutPlan.AssistantMessage);
	OutPlan.AssistantMessage.TrimStartAndEndInline();

	TArray<TSharedPtr<FJsonValue>> EmptySteps;
	const TArray<TSharedPtr<FJsonValue>>* Steps = nullptr;
	if (!LlmPlanObject->TryGetArrayField(TEXT("steps"), Steps) || Steps == nullptr)
	{
		Steps = &EmptySteps;
	}

	if (Steps->Num() <= 0 && OutPlan.AssistantMessage.IsEmpty())
	{
		OutError = TEXT("llm_plan.steps missing");
		return false;
	}

	for (int32 Index = 0; Index < Steps->Num(); ++Index)
	{
		const TSharedPtr<FJsonObject> StepObject = (*Steps)[Index].IsValid() ? (*Steps)[Index]->AsObject() : nullptr;
		if (!StepObject.IsValid())
		{
			OutError = FString::Printf(TEXT("llm_plan.steps[%d] must be object"), Index);
			return false;
		}

		FString StepId;
		if (!StepObject->TryGetStringField(TEXT("step_id"), StepId) || StepId.IsEmpty())
		{
			StepId = FString::Printf(TEXT("s%d"), Index + 1);
		}

		FString ToolName;
		if (!StepObject->TryGetStringField(TEXT("tool_name"), ToolName) || ToolName.IsEmpty())
		{
			OutError = FString::Printf(TEXT("llm_plan.steps[%d].tool_name missing"), Index);
			return false;
		}

		TSharedPtr<FJsonObject> ArgsObject;
		const TSharedPtr<FJsonObject>* ArgsObjectPtr = nullptr;
		if (!StepObject->TryGetObjectField(TEXT("args"), ArgsObjectPtr) || ArgsObjectPtr == nullptr || !ArgsObjectPtr->IsValid())
		{
			ArgsObject = MakeShared<FJsonObject>();
		}
		else
		{
			ArgsObject = *ArgsObjectPtr;
		}
		ArgsObject = HCI_NormalizeStepArgsBySchema(ToolRegistry, FName(*ToolName), ArgsObject);

		TArray<FString> ExpectedEvidence;
		if (!StepObject->HasField(TEXT("expected_evidence")))
		{
			OutError = FString::Printf(
				TEXT("Missing required field: expected_evidence (llm_plan.steps[%d])"),
				Index);
			return false;
		}
		if (!HCI_TryGetStringArrayField(StepObject, TEXT("expected_evidence"), ExpectedEvidence))
		{
			OutError = FString::Printf(
				TEXT("Invalid field type: expected_evidence (llm_plan.steps[%d])"),
				Index);
			return false;
		}
		if (ExpectedEvidence.Num() <= 0)
		{
			OutError = FString::Printf(
				TEXT("Invalid field value: expected_evidence must not be empty (llm_plan.steps[%d])"),
				Index);
			return false;
		}

		FHCIAbilityKitAgentPlanStep::FUiPresentation UiPresentation;
		if (!HCI_TryParseOptionalUiPresentation(StepObject, Index, UiPresentation, OutError))
		{
			return false;
		}

		FHCIAbilityKitAgentPlanStep& Step = OutPlan.Steps.AddDefaulted_GetRef();
		if (!HCI_StepFromTool(
				ToolRegistry,
				*StepId,
				*ToolName,
				ArgsObject,
				ExpectedEvidence,
				&UiPresentation,
				Step,
				OutError))
		{
			return false;
		}
	}

	FString ContractError;
	if (!FHCIAbilityKitAgentPlanContract::ValidateMinimalContract(OutPlan, ContractError))
	{
		OutError = ContractError;
		return false;
	}

	return true;
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

	const bool bLevelRiskIntent =
		HCI_TextContainsAny(Text, {TEXT("关卡"), TEXT("场景"), TEXT("level")}) &&
		HCI_TextContainsAny(Text, {TEXT("碰撞"), TEXT("collision"), TEXT("材质丢失"), TEXT("默认材质"), TEXT("default material")});

	const bool bMeshTriangleCountIntent =
		HCI_TextContainsAny(Text, {TEXT("面数"), TEXT("triangle"), TEXT("triangles"), TEXT("poly")}) &&
		HCI_TextContainsAny(Text, {TEXT("检查"), TEXT("扫描"), TEXT("统计"), TEXT("分析"), TEXT("查看"), TEXT("check"), TEXT("scan"), TEXT("inspect"), TEXT("analyze")});

	const bool bAssetComplianceIntent =
		HCI_TextContainsAny(Text, {TEXT("贴图"), TEXT("texture"), TEXT("分辨率"), TEXT("npot"), TEXT("面数"), TEXT("lod")});

	if (bNamingIntent)
	{
		OutPlan.Intent = TEXT("normalize_temp_assets_by_metadata");
		OutRouteReason = TEXT("naming_traceability_temp_assets");

		FHCIAbilityKitAgentPlanStep& ScanStep = OutPlan.Steps.AddDefaulted_GetRef();
		if (!HCI_StepFromTool(
				ToolRegistry,
				TEXT("s1"),
					TEXT("ScanAssets"),
					HCI_MakeScanAssetsArgs(),
					{TEXT("scan_root"), TEXT("asset_count"), TEXT("asset_paths"), TEXT("result")},
					ScanStep,
					OutError))
			{
				return false;
		}

		FHCIAbilityKitAgentPlanStep& Step = OutPlan.Steps.AddDefaulted_GetRef();
		if (!HCI_StepFromTool(
				ToolRegistry,
				TEXT("s2"),
					TEXT("NormalizeAssetNamingByMetadata"),
					HCI_MakeNamingArgs(),
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
		OutPlan.Intent = TEXT("scan_assets");
		OutRouteReason = TEXT("fallback_scan_assets");

		FHCIAbilityKitAgentPlanStep& Step = OutPlan.Steps.AddDefaulted_GetRef();
		if (!HCI_StepFromTool(
				ToolRegistry,
				TEXT("s1"),
					TEXT("ScanAssets"),
					HCI_MakeScanAssetsArgs(),
					{TEXT("scan_root"), TEXT("asset_count"), TEXT("asset_paths"), TEXT("result")},
					Step,
					OutError))
			{
				return false;
		}
	}

	FString ContractError;
	if (!FHCIAbilityKitAgentPlanContract::ValidateMinimalContract(OutPlan, ContractError))
	{
		OutError = ContractError;
		return false;
	}

	return true;
}

static bool HCI_TryBuildMockLlmPlan(
	const FString& UserText,
	const FString& RequestId,
	const FHCIAbilityKitToolRegistry& ToolRegistry,
	const FHCIAbilityKitAgentPlannerBuildOptions& Options,
	const int32 AttemptIndex,
	FHCIAbilityKitAgentPlan& OutPlan,
	FString& OutRouteReason,
	FString& OutFallbackReason,
	FString& OutErrorCode,
	FString& OutError)
{
	OutPlan = FHCIAbilityKitAgentPlan();
	OutRouteReason.Reset();
	OutFallbackReason = HCI_FallbackReasonNone;
	OutErrorCode = TEXT("-");
	OutError.Reset();

	if (Options.LlmMockMode != EHCIAbilityKitAgentPlannerLlmMockMode::None &&
		Options.LlmMockFailAttempts > 0 &&
		AttemptIndex > Options.LlmMockFailAttempts)
	{
		return HCI_BuildKeywordPlan(UserText, RequestId, ToolRegistry, OutPlan, OutRouteReason, OutError);
	}

	switch (Options.LlmMockMode)
	{
	case EHCIAbilityKitAgentPlannerLlmMockMode::Timeout:
		OutFallbackReason = HCI_FallbackReasonTimeout;
		OutErrorCode = TEXT("E4301");
		OutError = TEXT("llm_request_timeout");
		return false;
	case EHCIAbilityKitAgentPlannerLlmMockMode::InvalidJson:
		OutFallbackReason = HCI_FallbackReasonInvalidJson;
		OutErrorCode = TEXT("E4302");
		OutError = TEXT("llm_response_invalid_json");
		return false;
	case EHCIAbilityKitAgentPlannerLlmMockMode::EmptyResponse:
		OutFallbackReason = HCI_FallbackReasonEmptyResponse;
		OutErrorCode = TEXT("E4304");
		OutError = TEXT("llm_empty_response");
		return false;
	case EHCIAbilityKitAgentPlannerLlmMockMode::ContractInvalid:
		OutFallbackReason = HCI_FallbackReasonContractInvalid;
		OutErrorCode = TEXT("E4303");
		OutError = TEXT("Missing required field: expected_evidence");
		return false;
	case EHCIAbilityKitAgentPlannerLlmMockMode::None:
	default:
		return HCI_BuildKeywordPlan(UserText, RequestId, ToolRegistry, OutPlan, OutRouteReason, OutError);
	}
}

static bool HCI_IsRetryableLlmFailure(const FString& FallbackReason)
{
	return
		FallbackReason == HCI_FallbackReasonTimeout ||
		FallbackReason == HCI_FallbackReasonEmptyResponse ||
		FallbackReason == HCI_FallbackReasonHttpError;
}

using FHCIPlannerAsyncCallback = TFunction<void(bool, FHCIAbilityKitAgentPlan, FString, FHCIAbilityKitAgentPlannerResultMetadata, FString)>;

struct FHCIAbilityKitAsyncPlanBuildState : public TSharedFromThis<FHCIAbilityKitAsyncPlanBuildState, ESPMode::ThreadSafe>
{
	FString UserText;
	FString RequestId;
	const FHCIAbilityKitToolRegistry* ToolRegistry = nullptr;
	FHCIAbilityKitAgentPlannerBuildOptions Options;
	FString ProviderMode = TEXT("real_http");
	int32 MaxAttempts = 1;
	int32 AttemptsUsed = 0;
	FString LastFallbackReason = HCI_FallbackReasonNone;
	FString LastErrorCode = TEXT("-");
	FString LastError;
	bool bEnvContextPrepared = false;
	bool bEnvContextInjected = false;
	int32 EnvContextAssetCount = 0;
	FString EnvContextScanRoot;
	FString EnvContextText;
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> ActiveRequest;
	FTSTicker::FDelegateHandle TimeoutHandle;
	TAtomic<bool> bAttemptResolved{false};
	TAtomic<bool> bCompleted{false};
	FHCIPlannerAsyncCallback OnComplete;
};

static TArray<TSharedRef<FHCIAbilityKitAsyncPlanBuildState, ESPMode::ThreadSafe>> GHCIAbilityKitPlannerAsyncInFlightStates;

static void HCI_RegisterAsyncPlanBuildState(const TSharedRef<FHCIAbilityKitAsyncPlanBuildState, ESPMode::ThreadSafe>& State)
{
	GHCIAbilityKitPlannerAsyncInFlightStates.Add(State);
}

static void HCI_UnregisterAsyncPlanBuildState(const TSharedRef<FHCIAbilityKitAsyncPlanBuildState, ESPMode::ThreadSafe>& State)
{
	for (int32 Index = GHCIAbilityKitPlannerAsyncInFlightStates.Num() - 1; Index >= 0; --Index)
	{
		if (&GHCIAbilityKitPlannerAsyncInFlightStates[Index].Get() == &State.Get())
		{
			GHCIAbilityKitPlannerAsyncInFlightStates.RemoveAtSwap(Index);
			return;
		}
	}
}

static void HCI_CompleteAsyncPlanBuild(
	const TSharedRef<FHCIAbilityKitAsyncPlanBuildState, ESPMode::ThreadSafe>& State,
	const bool bBuilt,
	FHCIAbilityKitAgentPlan&& Plan,
	FString&& RouteReason,
	FHCIAbilityKitAgentPlannerResultMetadata&& Metadata,
	FString&& Error)
{
	if (State->bCompleted.Exchange(true))
	{
		return;
	}

	HCI_UnregisterAsyncPlanBuildState(State);

	if (State->TimeoutHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(State->TimeoutHandle);
		State->TimeoutHandle.Reset();
	}

	if (State->ActiveRequest.IsValid())
	{
		State->ActiveRequest->OnProcessRequestComplete().Unbind();
		State->ActiveRequest.Reset();
	}

	if (State->OnComplete)
	{
		State->OnComplete(bBuilt, MoveTemp(Plan), MoveTemp(RouteReason), MoveTemp(Metadata), MoveTemp(Error));
	}
}

static void HCI_FinishAsyncWithKeywordFallback(const TSharedRef<FHCIAbilityKitAsyncPlanBuildState, ESPMode::ThreadSafe>& State)
{
	if (State->LastFallbackReason == HCI_FallbackReasonContractInvalid)
	{
		UE_LOG(
			LogHCIAbilityKitAgentPlanner,
			Warning,
			TEXT("[HCIAbilityKit][AgentPlanLLM][E4303] request_id=%s provider_mode=%s attempts=%d error_code=%s fallback_reason=%s detail=%s input=%s"),
			*State->RequestId,
			*State->ProviderMode,
			State->AttemptsUsed,
			State->LastErrorCode.IsEmpty() ? TEXT("E4303") : *State->LastErrorCode,
			*State->LastFallbackReason,
			State->LastError.IsEmpty() ? TEXT("-") : *State->LastError,
			*State->UserText);
	}

	const bool bCircuitBreakerEnabled =
		State->Options.bEnableCircuitBreaker &&
		State->Options.CircuitBreakerFailureThreshold > 0 &&
		State->Options.CircuitBreakerOpenForRequests > 0;

	GHCIAbilityKitAgentPlannerRuntimeState.ConsecutiveLlmFailures += 1;
	GHCIAbilityKitAgentPlannerRuntimeState.Metrics.ConsecutiveLlmFailures = GHCIAbilityKitAgentPlannerRuntimeState.ConsecutiveLlmFailures;
	if (bCircuitBreakerEnabled &&
		GHCIAbilityKitAgentPlannerRuntimeState.ConsecutiveLlmFailures >= State->Options.CircuitBreakerFailureThreshold)
	{
		GHCIAbilityKitAgentPlannerRuntimeState.CircuitOpenRemainingRequests = State->Options.CircuitBreakerOpenForRequests;
	}

	GHCIAbilityKitAgentPlannerRuntimeState.Metrics.KeywordFallbackRequests += 1;

	FHCIAbilityKitAgentPlannerResultMetadata Metadata;
	Metadata.PlannerProvider = HCI_KeywordProviderName;
	Metadata.ProviderMode = State->ProviderMode;
	Metadata.bFallbackUsed = true;
	Metadata.FallbackReason = State->LastFallbackReason;
	Metadata.ErrorCode = State->LastErrorCode;
	Metadata.LlmAttemptCount = State->AttemptsUsed;
	Metadata.bRetryUsed = State->AttemptsUsed > 1;
	Metadata.bCircuitBreakerOpen = (GHCIAbilityKitAgentPlannerRuntimeState.CircuitOpenRemainingRequests > 0);
	Metadata.ConsecutiveLlmFailures = GHCIAbilityKitAgentPlannerRuntimeState.ConsecutiveLlmFailures;
	Metadata.bEnvContextInjected = State->bEnvContextInjected;
	Metadata.EnvContextAssetCount = State->EnvContextAssetCount;
	Metadata.EnvContextScanRoot = State->EnvContextScanRoot;
	if (Metadata.bRetryUsed)
	{
		GHCIAbilityKitAgentPlannerRuntimeState.Metrics.RetryUsedRequests += 1;
		GHCIAbilityKitAgentPlannerRuntimeState.Metrics.RetryAttempts += (State->AttemptsUsed - 1);
	}

	FHCIAbilityKitAgentPlan FallbackPlan;
	FString FallbackRouteReason;
	FString FallbackBuildError;
	if (!HCI_BuildKeywordPlan(
			State->UserText,
			State->RequestId,
			*State->ToolRegistry,
			FallbackPlan,
			FallbackRouteReason,
			FallbackBuildError))
	{
		HCI_CompleteAsyncPlanBuild(State, false, FHCIAbilityKitAgentPlan(), FString(), MoveTemp(Metadata), MoveTemp(FallbackBuildError));
		return;
	}

	HCI_CompleteAsyncPlanBuild(State, true, MoveTemp(FallbackPlan), MoveTemp(FallbackRouteReason), MoveTemp(Metadata), FString());
}

static void HCI_StartAsyncRealHttpAttempt(const TSharedRef<FHCIAbilityKitAsyncPlanBuildState, ESPMode::ThreadSafe>& State)
{
	State->AttemptsUsed += 1;
	State->bAttemptResolved.Store(false);

	FHCIAbilityKitAgentLlmProviderConfig ProviderConfig;
	FString ConfigError;
	if (!FHCIAbilityKitAgentLlmClient::LoadProviderConfigFromJsonFile(
			State->Options.LlmApiKeyConfigPath,
			State->Options.LlmApiUrl,
			State->Options.LlmModel,
			ProviderConfig,
			ConfigError))
	{
		State->LastFallbackReason = HCI_FallbackReasonConfigMissing;
		State->LastErrorCode = TEXT("E4307");
		State->LastError = ConfigError.IsEmpty() ? FString::Printf(TEXT("llm_api_key_missing config=%s"), *State->Options.LlmApiKeyConfigPath) : ConfigError;
		HCI_FinishAsyncWithKeywordFallback(State);
		return;
	}
	ProviderConfig.bEnableThinking = State->Options.bLlmEnableThinking;
	ProviderConfig.bStream = State->Options.bLlmStream;
	ProviderConfig.HttpTimeoutMs = State->Options.LlmHttpTimeoutMs;

	if (!State->bEnvContextPrepared)
	{
		State->bEnvContextPrepared = true;
		HCI_TryBuildAutoEnvContext(
			State->UserText,
			State->Options,
			State->EnvContextScanRoot,
			State->EnvContextText,
			State->EnvContextAssetCount,
			State->bEnvContextInjected);
	}

	FString SystemPrompt;
	if (!FHCIAbilityKitAgentPromptBuilder::BuildSystemPromptFromBundleWithEnvContext(
				State->UserText,
				State->EnvContextText,
				HCI_MakePromptBundleOptions(State->Options),
				SystemPrompt,
				State->LastError))
	{
		State->LastFallbackReason = HCI_FallbackReasonContractInvalid;
		State->LastErrorCode = TEXT("E4303");
		HCI_FinishAsyncWithKeywordFallback(State);
		return;
	}

	FString RequestBody;
	if (!FHCIAbilityKitAgentLlmClient::BuildChatCompletionsRequestBody(
			SystemPrompt,
			State->UserText,
			ProviderConfig,
			RequestBody,
			State->LastError))
	{
		State->LastFallbackReason = HCI_FallbackReasonContractInvalid;
		State->LastErrorCode = TEXT("E4303");
		HCI_FinishAsyncWithKeywordFallback(State);
		return;
	}

	State->ActiveRequest = FHCIAbilityKitAgentLlmClient::CreateChatCompletionsHttpRequest(ProviderConfig, RequestBody);
	if (!State->ActiveRequest.IsValid())
	{
		State->LastFallbackReason = HCI_FallbackReasonHttpError;
		State->LastErrorCode = TEXT("E4306");
		State->LastError = TEXT("llm_http_create_request_failed");
		HCI_FinishAsyncWithKeywordFallback(State);
		return;
	}

	TWeakPtr<FHCIAbilityKitAsyncPlanBuildState, ESPMode::ThreadSafe> WeakState = State;
	State->ActiveRequest->OnProcessRequestComplete().BindLambda(
		[WeakState](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
		{
			const TSharedPtr<FHCIAbilityKitAsyncPlanBuildState, ESPMode::ThreadSafe> Pinned = WeakState.Pin();
			if (!Pinned.IsValid() || Pinned->bCompleted.Load())
			{
				return;
			}
			if (Pinned->bAttemptResolved.Exchange(true))
			{
				return;
			}

			if (Pinned->TimeoutHandle.IsValid())
			{
				FTSTicker::GetCoreTicker().RemoveTicker(Pinned->TimeoutHandle);
				Pinned->TimeoutHandle.Reset();
			}

			if (!bSucceeded || !HttpResponse.IsValid() || HttpResponse->GetResponseCode() < 200 || HttpResponse->GetResponseCode() >= 300)
			{
				Pinned->LastFallbackReason = HCI_FallbackReasonHttpError;
				Pinned->LastErrorCode = TEXT("E4306");
				Pinned->LastError = HttpResponse.IsValid()
					? FString::Printf(TEXT("llm_http_status_not_ok status=%d"), HttpResponse->GetResponseCode())
					: TEXT("llm_http_request_failed_no_response");

				if (Pinned->AttemptsUsed < Pinned->MaxAttempts && HCI_IsRetryableLlmFailure(Pinned->LastFallbackReason))
				{
					HCI_StartAsyncRealHttpAttempt(Pinned.ToSharedRef());
					return;
				}

				HCI_FinishAsyncWithKeywordFallback(Pinned.ToSharedRef());
				return;
			}

				FString Content;
				FString LlmErrorCode;
				FString LlmError;
				if (!FHCIAbilityKitAgentLlmClient::TryExtractMessageContentFromResponse(HttpResponse->GetContentAsString(), Content, LlmErrorCode, LlmError))
				{
					Pinned->LastErrorCode = LlmErrorCode.IsEmpty() ? TEXT("E4303") : LlmErrorCode;
					Pinned->LastError = LlmError;
					if (Pinned->LastErrorCode == TEXT("E4302"))
					{
						Pinned->LastFallbackReason = HCI_FallbackReasonInvalidJson;
					}
					else if (Pinned->LastErrorCode == TEXT("E4304"))
					{
						Pinned->LastFallbackReason = HCI_FallbackReasonEmptyResponse;
					}
					else
					{
						Pinned->LastFallbackReason = HCI_FallbackReasonContractInvalid;
					}
					HCI_FinishAsyncWithKeywordFallback(Pinned.ToSharedRef());
					return;
				}

			FString LlmPlanJsonText;
			if (!HCI_TryExtractJsonObjectString(Content, LlmPlanJsonText))
			{
				Pinned->LastFallbackReason = HCI_FallbackReasonInvalidJson;
				Pinned->LastErrorCode = TEXT("E4302");
				Pinned->LastError = TEXT("llm_content_no_json_object");
				HCI_FinishAsyncWithKeywordFallback(Pinned.ToSharedRef());
				return;
			}

			TSharedPtr<FJsonObject> LlmPlanObject;
			{
				const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(LlmPlanJsonText);
				if (!FJsonSerializer::Deserialize(Reader, LlmPlanObject) || !LlmPlanObject.IsValid())
				{
					Pinned->LastFallbackReason = HCI_FallbackReasonInvalidJson;
					Pinned->LastErrorCode = TEXT("E4302");
					Pinned->LastError = TEXT("llm_plan_invalid_json");
					HCI_FinishAsyncWithKeywordFallback(Pinned.ToSharedRef());
					return;
				}
			}

			FHCIAbilityKitAgentPlan Plan;
			FString RouteReason;
			FString BuildError;
				if (!HCI_TryBuildPlanFromLlmPlanJson(LlmPlanObject, Pinned->RequestId, *Pinned->ToolRegistry, Plan, RouteReason, BuildError))
				{
					Pinned->LastFallbackReason = HCI_FallbackReasonContractInvalid;
					Pinned->LastErrorCode = TEXT("E4303");
					Pinned->LastError = BuildError;
					HCI_FinishAsyncWithKeywordFallback(Pinned.ToSharedRef());
					return;
				}
					if (!HCI_EnsureScanAssetsFirstForDirectoryIntent(
							Pinned->UserText,
							*Pinned->ToolRegistry,
							Pinned->Options.bForceDirectoryScanFirst,
							Plan,
							RouteReason,
							BuildError))
					{
						Pinned->LastFallbackReason = HCI_FallbackReasonContractInvalid;
						Pinned->LastErrorCode = TEXT("E4303");
						Pinned->LastError = BuildError;
						HCI_FinishAsyncWithKeywordFallback(Pinned.ToSharedRef());
						return;
					}
					bool bStepOrderReordered = false;
					if (!HCI_ReorderPlanStepsByVariableDependencies(Plan, bStepOrderReordered, BuildError))
					{
						Pinned->LastFallbackReason = HCI_FallbackReasonContractInvalid;
						Pinned->LastErrorCode = TEXT("E4303");
						Pinned->LastError = BuildError;
						HCI_FinishAsyncWithKeywordFallback(Pinned.ToSharedRef());
						return;
					}
					if (bStepOrderReordered && !RouteReason.Contains(TEXT("step_order_normalized")))
					{
						RouteReason = RouteReason.IsEmpty()
							? TEXT("llm_step_order_normalized")
							: RouteReason + TEXT("_step_order_normalized");
					}
					if (!HCI_ValidateBuiltPlanOrSetError(Plan, *Pinned->ToolRegistry, BuildError))
					{
						Pinned->LastFallbackReason = HCI_FallbackReasonContractInvalid;
						Pinned->LastErrorCode = TEXT("E4303");
					Pinned->LastError = BuildError;
					HCI_FinishAsyncWithKeywordFallback(Pinned.ToSharedRef());
					return;
				}

				GHCIAbilityKitAgentPlannerRuntimeState.ConsecutiveLlmFailures = 0;
			GHCIAbilityKitAgentPlannerRuntimeState.Metrics.ConsecutiveLlmFailures = 0;
			GHCIAbilityKitAgentPlannerRuntimeState.Metrics.LlmSuccessRequests += 1;
			if (Pinned->AttemptsUsed > 1)
			{
				GHCIAbilityKitAgentPlannerRuntimeState.Metrics.RetryUsedRequests += 1;
				GHCIAbilityKitAgentPlannerRuntimeState.Metrics.RetryAttempts += (Pinned->AttemptsUsed - 1);
			}

			FHCIAbilityKitAgentPlannerResultMetadata Metadata;
			Metadata.PlannerProvider = HCI_LlmProviderName;
			Metadata.ProviderMode = Pinned->ProviderMode;
			Metadata.bFallbackUsed = false;
			Metadata.FallbackReason = HCI_FallbackReasonNone;
			Metadata.ErrorCode = TEXT("-");
				Metadata.LlmAttemptCount = Pinned->AttemptsUsed;
				Metadata.bRetryUsed = Pinned->AttemptsUsed > 1;
				Metadata.bCircuitBreakerOpen = false;
				Metadata.ConsecutiveLlmFailures = 0;
				Metadata.bEnvContextInjected = Pinned->bEnvContextInjected;
				Metadata.EnvContextAssetCount = Pinned->EnvContextAssetCount;
				Metadata.EnvContextScanRoot = Pinned->EnvContextScanRoot;

			HCI_CompleteAsyncPlanBuild(Pinned.ToSharedRef(), true, MoveTemp(Plan), MoveTemp(RouteReason), MoveTemp(Metadata), FString());
		});

	if (!State->ActiveRequest->ProcessRequest())
	{
		State->LastFallbackReason = HCI_FallbackReasonHttpError;
		State->LastErrorCode = TEXT("E4306");
		State->LastError = TEXT("llm_http_process_request_failed");
		HCI_FinishAsyncWithKeywordFallback(State);
		return;
	}

	const float TimeoutSeconds = FMath::Max(1.0f, static_cast<float>(State->Options.LlmHttpTimeoutMs) / 1000.0f);
	State->TimeoutHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([WeakState](float DeltaSeconds)
		{
			const TSharedPtr<FHCIAbilityKitAsyncPlanBuildState, ESPMode::ThreadSafe> Pinned = WeakState.Pin();
			if (!Pinned.IsValid() || Pinned->bCompleted.Load())
			{
				return false;
			}
			if (Pinned->bAttemptResolved.Exchange(true))
			{
				return false;
			}

			if (Pinned->ActiveRequest.IsValid())
			{
				Pinned->ActiveRequest->OnProcessRequestComplete().Unbind();
				Pinned->ActiveRequest->CancelRequest();
			}
			Pinned->TimeoutHandle.Reset();
			Pinned->LastFallbackReason = HCI_FallbackReasonTimeout;
			Pinned->LastErrorCode = TEXT("E4301");
			Pinned->LastError = TEXT("llm_request_timeout");

			if (Pinned->AttemptsUsed < Pinned->MaxAttempts && HCI_IsRetryableLlmFailure(Pinned->LastFallbackReason))
			{
				HCI_StartAsyncRealHttpAttempt(Pinned.ToSharedRef());
			}
			else
			{
				HCI_FinishAsyncWithKeywordFallback(Pinned.ToSharedRef());
			}
			return false;
		}),
		TimeoutSeconds);
}
}

bool FHCIAbilityKitAgentPlanner::BuildPlanFromNaturalLanguage(
	const FString& UserText,
	const FString& RequestId,
	const FHCIAbilityKitToolRegistry& ToolRegistry,
	FHCIAbilityKitAgentPlan& OutPlan,
	FString& OutRouteReason,
	FString& OutError)
{
	return HCI_BuildKeywordPlan(UserText, RequestId, ToolRegistry, OutPlan, OutRouteReason, OutError);
}

bool FHCIAbilityKitAgentPlanner::BuildPlanFromNaturalLanguageWithProvider(
	const FString& UserText,
	const FString& RequestId,
	const FHCIAbilityKitToolRegistry& ToolRegistry,
	const FHCIAbilityKitAgentPlannerBuildOptions& Options,
	FHCIAbilityKitAgentPlan& OutPlan,
	FString& OutRouteReason,
	FHCIAbilityKitAgentPlannerResultMetadata& OutMetadata,
	FString& OutError)
{
	GHCIAbilityKitAgentPlannerRuntimeState.Metrics.TotalRequests += 1;
	OutMetadata = FHCIAbilityKitAgentPlannerResultMetadata();
	OutError.Reset();

	if (!Options.bPreferLlm)
	{
		OutMetadata.PlannerProvider = HCI_KeywordProviderName;
		OutMetadata.ProviderMode = TEXT("keyword");
		OutMetadata.bFallbackUsed = false;
		OutMetadata.FallbackReason = HCI_FallbackReasonNone;
		OutMetadata.ConsecutiveLlmFailures = GHCIAbilityKitAgentPlannerRuntimeState.ConsecutiveLlmFailures;
		return HCI_BuildKeywordPlan(UserText, RequestId, ToolRegistry, OutPlan, OutRouteReason, OutError);
	}

	GHCIAbilityKitAgentPlannerRuntimeState.Metrics.LlmPreferredRequests += 1;
	const FString ProviderMode = Options.bUseRealHttpProvider ? TEXT("real_http") : TEXT("mock");
	const bool bCircuitBreakerEnabled = Options.bEnableCircuitBreaker && Options.CircuitBreakerFailureThreshold > 0 && Options.CircuitBreakerOpenForRequests > 0;
	if (bCircuitBreakerEnabled && GHCIAbilityKitAgentPlannerRuntimeState.CircuitOpenRemainingRequests > 0)
	{
		GHCIAbilityKitAgentPlannerRuntimeState.CircuitOpenRemainingRequests -= 1;
		GHCIAbilityKitAgentPlannerRuntimeState.Metrics.CircuitOpenFallbackRequests += 1;
		GHCIAbilityKitAgentPlannerRuntimeState.Metrics.KeywordFallbackRequests += 1;

		OutMetadata.PlannerProvider = HCI_KeywordProviderName;
		OutMetadata.ProviderMode = ProviderMode;
		OutMetadata.bFallbackUsed = true;
		OutMetadata.FallbackReason = HCI_FallbackReasonCircuitOpen;
		OutMetadata.ErrorCode = TEXT("E4305");
		OutMetadata.bCircuitBreakerOpen = true;
		OutMetadata.ConsecutiveLlmFailures = GHCIAbilityKitAgentPlannerRuntimeState.ConsecutiveLlmFailures;

		if (!HCI_BuildKeywordPlan(UserText, RequestId, ToolRegistry, OutPlan, OutRouteReason, OutError))
		{
			return false;
		}
			OutError.Reset();
			return true;
		}

	if (Options.bUseRealHttpProvider)
	{
		OutMetadata.PlannerProvider = HCI_KeywordProviderName;
		OutMetadata.ProviderMode = TEXT("real_http");
		OutMetadata.bFallbackUsed = true;
		OutMetadata.FallbackReason = HCI_FallbackReasonSyncRealHttpDeprecated;
		OutMetadata.ErrorCode = TEXT("E4310");
		OutMetadata.ConsecutiveLlmFailures = GHCIAbilityKitAgentPlannerRuntimeState.ConsecutiveLlmFailures;
		OutError = TEXT("real_http_provider_requires_async_api");
		return false;
	}

	const int32 RetryCount = FMath::Max(0, Options.LlmRetryCount);
	const int32 MaxAttempts = 1 + RetryCount;
	int32 AttemptsUsed = 0;
	FString LastFallbackReason = HCI_FallbackReasonNone;
	FString LastErrorCode = TEXT("-");
	FString LastError;

	bool bLlmSucceeded = false;
	for (int32 AttemptIndex = 1; AttemptIndex <= MaxAttempts; ++AttemptIndex)
	{
		AttemptsUsed = AttemptIndex;
		const bool bAttemptSucceeded = HCI_TryBuildMockLlmPlan(
			UserText,
			RequestId,
			ToolRegistry,
			Options,
			AttemptIndex,
			OutPlan,
			OutRouteReason,
			LastFallbackReason,
			LastErrorCode,
			LastError);
		if (bAttemptSucceeded)
		{
			bLlmSucceeded = true;
			break;
		}

		if (AttemptIndex < MaxAttempts && HCI_IsRetryableLlmFailure(LastFallbackReason))
		{
			continue;
		}
		break;
	}

	OutMetadata.LlmAttemptCount = AttemptsUsed;
	OutMetadata.bRetryUsed = AttemptsUsed > 1;
	if (OutMetadata.bRetryUsed)
	{
		GHCIAbilityKitAgentPlannerRuntimeState.Metrics.RetryUsedRequests += 1;
		GHCIAbilityKitAgentPlannerRuntimeState.Metrics.RetryAttempts += (AttemptsUsed - 1);
	}

	if (bLlmSucceeded)
	{
		GHCIAbilityKitAgentPlannerRuntimeState.ConsecutiveLlmFailures = 0;
		GHCIAbilityKitAgentPlannerRuntimeState.Metrics.ConsecutiveLlmFailures = 0;
		OutMetadata.PlannerProvider = HCI_LlmProviderName;
		OutMetadata.ProviderMode = ProviderMode;
		OutMetadata.bFallbackUsed = false;
		OutMetadata.FallbackReason = HCI_FallbackReasonNone;
		OutMetadata.ErrorCode = TEXT("-");
		OutMetadata.ConsecutiveLlmFailures = 0;
		GHCIAbilityKitAgentPlannerRuntimeState.Metrics.LlmSuccessRequests += 1;
		return true;
	}

	GHCIAbilityKitAgentPlannerRuntimeState.ConsecutiveLlmFailures += 1;
	GHCIAbilityKitAgentPlannerRuntimeState.Metrics.ConsecutiveLlmFailures = GHCIAbilityKitAgentPlannerRuntimeState.ConsecutiveLlmFailures;
	if (bCircuitBreakerEnabled &&
		GHCIAbilityKitAgentPlannerRuntimeState.ConsecutiveLlmFailures >= Options.CircuitBreakerFailureThreshold)
	{
		GHCIAbilityKitAgentPlannerRuntimeState.CircuitOpenRemainingRequests = Options.CircuitBreakerOpenForRequests;
		OutMetadata.bCircuitBreakerOpen = true;
	}

	GHCIAbilityKitAgentPlannerRuntimeState.Metrics.KeywordFallbackRequests += 1;
	OutMetadata.PlannerProvider = HCI_KeywordProviderName;
	OutMetadata.ProviderMode = ProviderMode;
	OutMetadata.bFallbackUsed = true;
	OutMetadata.FallbackReason = LastFallbackReason;
	OutMetadata.ErrorCode = LastErrorCode;
	OutMetadata.ConsecutiveLlmFailures = GHCIAbilityKitAgentPlannerRuntimeState.ConsecutiveLlmFailures;
	OutError = LastError;
	if (LastFallbackReason == HCI_FallbackReasonContractInvalid)
	{
		UE_LOG(
			LogHCIAbilityKitAgentPlanner,
			Display,
			TEXT("[HCIAbilityKit][AgentPlanLLM][E4303] request_id=%s provider_mode=%s attempts=%d error_code=%s fallback_reason=%s detail=%s input=%s"),
			*RequestId,
			*ProviderMode,
			AttemptsUsed,
			LastErrorCode.IsEmpty() ? TEXT("E4303") : *LastErrorCode,
			*LastFallbackReason,
			LastError.IsEmpty() ? TEXT("-") : *LastError,
			*UserText);
	}

	const FString FallbackDetail = OutError;
	if (!HCI_BuildKeywordPlan(UserText, RequestId, ToolRegistry, OutPlan, OutRouteReason, OutError))
	{
		return false;
	}
	OutError = FallbackDetail;
	return true;
}

void FHCIAbilityKitAgentPlanner::BuildPlanFromNaturalLanguageWithProviderAsync(
	const FString& UserText,
	const FString& RequestId,
	const FHCIAbilityKitToolRegistry& ToolRegistry,
	const FHCIAbilityKitAgentPlannerBuildOptions& Options,
	TFunction<void(bool, FHCIAbilityKitAgentPlan, FString, FHCIAbilityKitAgentPlannerResultMetadata, FString)>&& OnComplete)
{
	if (!OnComplete)
	{
		return;
	}

	if (!Options.bPreferLlm || !Options.bUseRealHttpProvider)
	{
		FHCIAbilityKitAgentPlan Plan;
		FString RouteReason;
		FHCIAbilityKitAgentPlannerResultMetadata Metadata;
		FString Error;
		const bool bBuilt = BuildPlanFromNaturalLanguageWithProvider(
			UserText,
			RequestId,
			ToolRegistry,
			Options,
			Plan,
			RouteReason,
			Metadata,
			Error);
		OnComplete(bBuilt, MoveTemp(Plan), MoveTemp(RouteReason), MoveTemp(Metadata), MoveTemp(Error));
		return;
	}

	GHCIAbilityKitAgentPlannerRuntimeState.Metrics.TotalRequests += 1;
	GHCIAbilityKitAgentPlannerRuntimeState.Metrics.LlmPreferredRequests += 1;

	const bool bCircuitBreakerEnabled =
		Options.bEnableCircuitBreaker &&
		Options.CircuitBreakerFailureThreshold > 0 &&
		Options.CircuitBreakerOpenForRequests > 0;
	if (bCircuitBreakerEnabled && GHCIAbilityKitAgentPlannerRuntimeState.CircuitOpenRemainingRequests > 0)
	{
		GHCIAbilityKitAgentPlannerRuntimeState.CircuitOpenRemainingRequests -= 1;
		GHCIAbilityKitAgentPlannerRuntimeState.Metrics.CircuitOpenFallbackRequests += 1;
		GHCIAbilityKitAgentPlannerRuntimeState.Metrics.KeywordFallbackRequests += 1;

		FHCIAbilityKitAgentPlan Plan;
		FString RouteReason;
		FString Error;
		FHCIAbilityKitAgentPlannerResultMetadata Metadata;
		Metadata.PlannerProvider = HCI_KeywordProviderName;
		Metadata.ProviderMode = TEXT("real_http");
		Metadata.bFallbackUsed = true;
		Metadata.FallbackReason = HCI_FallbackReasonCircuitOpen;
		Metadata.ErrorCode = TEXT("E4305");
		Metadata.bCircuitBreakerOpen = true;
		Metadata.ConsecutiveLlmFailures = GHCIAbilityKitAgentPlannerRuntimeState.ConsecutiveLlmFailures;
		const bool bBuilt = HCI_BuildKeywordPlan(UserText, RequestId, ToolRegistry, Plan, RouteReason, Error);
		OnComplete(bBuilt, MoveTemp(Plan), MoveTemp(RouteReason), MoveTemp(Metadata), MoveTemp(Error));
		return;
	}

	TSharedRef<FHCIAbilityKitAsyncPlanBuildState, ESPMode::ThreadSafe> State =
		MakeShared<FHCIAbilityKitAsyncPlanBuildState, ESPMode::ThreadSafe>();
	State->UserText = UserText;
	State->RequestId = RequestId;
	State->ToolRegistry = &ToolRegistry;
	State->Options = Options;
	State->ProviderMode = TEXT("real_http");
	State->MaxAttempts = 1 + FMath::Max(0, Options.LlmRetryCount);
	State->OnComplete = MoveTemp(OnComplete);

	HCI_RegisterAsyncPlanBuildState(State);
	HCI_StartAsyncRealHttpAttempt(State);
}

FHCIAbilityKitAgentPlannerMetricsSnapshot FHCIAbilityKitAgentPlanner::GetMetricsSnapshot()
{
	return GHCIAbilityKitAgentPlannerRuntimeState.Metrics;
}

void FHCIAbilityKitAgentPlanner::ResetMetricsForTesting()
{
	GHCIAbilityKitAgentPlannerRuntimeState = FHCIAbilityKitAgentPlannerRuntimeState();
}
