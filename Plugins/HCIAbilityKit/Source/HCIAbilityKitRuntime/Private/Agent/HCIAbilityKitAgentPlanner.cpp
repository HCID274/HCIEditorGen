#include "Agent/HCIAbilityKitAgentPlanner.h"

#include "Agent/HCIAbilityKitAgentLlmClient.h"
#include "Agent/HCIAbilityKitAgentPlan.h"
#include "Agent/HCIAbilityKitAgentPlanValidator.h"
#include "Agent/HCIAbilityKitAgentPromptBuilder.h"
#include "Agent/HCIAbilityKitToolRegistry.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Templates/Atomic.h"

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
	return true;
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
	return MakeShared<FJsonObject>();
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
		return true;
	}

	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Object->TryGetArrayField(FieldName, Values) || Values == nullptr)
	{
		return false;
	}

	for (const TSharedPtr<FJsonValue>& Value : *Values)
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

static TSharedPtr<FJsonObject> HCI_MakeDefaultArgsForToolName(const FName ToolName)
{
	if (ToolName == TEXT("NormalizeAssetNamingByMetadata"))
	{
		return HCI_MakeNamingArgs();
	}
	if (ToolName == TEXT("ScanLevelMeshRisks"))
	{
		return HCI_MakeLevelRiskArgs();
	}
	if (ToolName == TEXT("SetTextureMaxSize"))
	{
		return HCI_MakeTextureComplianceArgs();
	}
	if (ToolName == TEXT("SetMeshLODGroup"))
	{
		return HCI_MakeLodComplianceArgs();
	}
	if (ToolName == TEXT("ScanAssets"))
	{
		return HCI_MakeScanAssetsArgs();
	}
	return nullptr;
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

	const TSharedPtr<FJsonObject> DefaultArgs = HCI_MakeDefaultArgsForToolName(ToolName);
	const TSharedPtr<FJsonObject> SafeRawArgs = RawArgsObject.IsValid() ? RawArgsObject : MakeShared<FJsonObject>();
	const TSharedPtr<FJsonObject> NormalizedArgs = MakeShared<FJsonObject>();

	for (const FHCIAbilityKitToolArgSchema& ArgSchema : Tool->ArgsSchema)
	{
		const FString Key = ArgSchema.ArgName.ToString();
		const TSharedPtr<FJsonValue>* ExistingValue = SafeRawArgs->Values.Find(Key);
		if (ExistingValue != nullptr && ExistingValue->IsValid())
		{
			NormalizedArgs->SetField(Key, *ExistingValue);
			continue;
		}

		if (DefaultArgs.IsValid())
		{
			const TSharedPtr<FJsonValue>* DefaultValue = DefaultArgs->Values.Find(Key);
			if (DefaultValue != nullptr && DefaultValue->IsValid())
			{
				NormalizedArgs->SetField(Key, *DefaultValue);
			}
		}
	}

	return NormalizedArgs;
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

	const TArray<TSharedPtr<FJsonValue>>* Steps = nullptr;
	if (!LlmPlanObject->TryGetArrayField(TEXT("steps"), Steps) || Steps == nullptr || Steps->Num() <= 0)
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
			if (!HCI_TryGetStringArrayField(StepObject, TEXT("expected_evidence"), ExpectedEvidence))
		{
			OutError = FString::Printf(TEXT("llm_plan.steps[%d].expected_evidence invalid"), Index);
			return false;
		}

		FHCIAbilityKitAgentPlanStep& Step = OutPlan.Steps.AddDefaulted_GetRef();
		if (!HCI_StepFromTool(
				ToolRegistry,
				*StepId,
				*ToolName,
				ArgsObject,
				ExpectedEvidence,
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

	const bool bLevelRiskIntent =
		HCI_TextContainsAny(Text, {TEXT("关卡"), TEXT("场景"), TEXT("level")}) &&
		HCI_TextContainsAny(Text, {TEXT("碰撞"), TEXT("collision"), TEXT("材质丢失"), TEXT("默认材质"), TEXT("default material")});

	const bool bAssetComplianceIntent =
		HCI_TextContainsAny(Text, {TEXT("贴图"), TEXT("texture"), TEXT("分辨率"), TEXT("npot"), TEXT("面数"), TEXT("lod")});

	if (bNamingIntent)
	{
		OutPlan.Intent = TEXT("normalize_temp_assets_by_metadata");
		OutRouteReason = TEXT("naming_traceability_temp_assets");

		FHCIAbilityKitAgentPlanStep& Step = OutPlan.Steps.AddDefaulted_GetRef();
		if (!HCI_StepFromTool(
				ToolRegistry,
				TEXT("s1"),
				TEXT("NormalizeAssetNamingByMetadata"),
				HCI_MakeNamingArgs(),
				{TEXT("asset_path"), TEXT("before"), TEXT("after")},
				Step,
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
					{TEXT("asset_path"), TEXT("before"), TEXT("after")},
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
					{TEXT("asset_path"), TEXT("before"), TEXT("after")},
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
				{TEXT("asset_path"), TEXT("result")},
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
		OutPlan = FHCIAbilityKitAgentPlan();
		OutPlan.RequestId = RequestId;
		OutPlan.Intent = TEXT("invalid_plan_for_contract_check");
		OutRouteReason = TEXT("llm_mock_contract_invalid");
		break;
	case EHCIAbilityKitAgentPlannerLlmMockMode::None:
	default:
		return HCI_BuildKeywordPlan(UserText, RequestId, ToolRegistry, OutPlan, OutRouteReason, OutError);
	}

	FString ContractError;
	if (!FHCIAbilityKitAgentPlanContract::ValidateMinimalContract(OutPlan, ContractError))
	{
		OutError = ContractError;
		return false;
	}
	return true;
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

	FString SystemPrompt;
	if (!FHCIAbilityKitAgentPromptBuilder::BuildSystemPromptFromBundle(
			State->UserText,
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

	if (!HCI_BuildKeywordPlan(UserText, RequestId, ToolRegistry, OutPlan, OutRouteReason, OutError))
	{
		return false;
	}
	OutError.Reset();
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
