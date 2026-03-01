#include "Commands/HCIAgentCommandHandlers.h"

#include "Commands/HCIAgentDemoState.h"
#include "UI/HCIAgentPlanPreviewWindow.h"

#include "Agent/LLM/HCIAgentLlmClient.h"
#include "Agent/Planner/HCIAgentPlan.h"
#include "Agent/Planner/HCIAgentPlanValidator.h"
#include "Agent/Planner/HCIAgentPlanner.h"
#include "Agent/Tools/HCIToolRegistry.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "EditorAssetLibrary.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Modules/ModuleManager.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/SoftObjectPath.h"

DEFINE_LOG_CATEGORY_STATIC(LogHCIAgentDemoLlm, Log, All);
#ifdef LogHCIAgentDemo
#undef LogHCIAgentDemo
#endif
#define LogHCIAgentDemo LogHCIAgentDemoLlm

static FHCIAgentDemoState& HCI_Llm_State()
{
	return HCI_GetAgentDemoState();
}

static FString HCI_Llm_JoinConsoleArgsAsText(const TArray<FString>& Args)
{
	FString Joined;
	for (int32 Index = 0; Index < Args.Num(); ++Index)
	{
		if (Index > 0)
		{
			Joined += TEXT(" ");
		}
		Joined += Args[Index];
	}
	Joined.TrimStartAndEndInline();
	return Joined;
}

static FString HCI_Llm_JsonObjectToCompactString(const TSharedPtr<FJsonObject>& JsonObject)
{
	if (!JsonObject.IsValid())
	{
		return TEXT("{}");
	}

	FString OutJson;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutJson);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	return OutJson;
}

static bool HCI_Llm_TryParseLlmMockModeArg(const FString& InMode, EHCIAgentPlannerLlmMockMode& OutMode)
{
	const FString Mode = InMode.TrimStartAndEnd().ToLower();
	if (Mode == TEXT("timeout"))
	{
		OutMode = EHCIAgentPlannerLlmMockMode::Timeout;
		return true;
	}
	if (Mode == TEXT("invalid_json"))
	{
		OutMode = EHCIAgentPlannerLlmMockMode::InvalidJson;
		return true;
	}
	if (Mode == TEXT("empty"))
	{
		OutMode = EHCIAgentPlannerLlmMockMode::EmptyResponse;
		return true;
	}
	if (Mode == TEXT("contract_invalid"))
	{
		OutMode = EHCIAgentPlannerLlmMockMode::ContractInvalid;
		return true;
	}
	if (Mode == TEXT("none"))
	{
		OutMode = EHCIAgentPlannerLlmMockMode::None;
		return true;
	}
	return false;
}

static int64 HCI_Llm_TryExtractAssetSizeFromTags(const FAssetData& AssetData)
{
	int64 SizeBytes = -1;
	if (AssetData.GetTagValue(FName(TEXT("DiskSize")), SizeBytes))
	{
		return SizeBytes;
	}
	if (AssetData.GetTagValue(FName(TEXT("FileSize")), SizeBytes))
	{
		return SizeBytes;
	}

	FString SizeText;
	if (AssetData.GetTagValue(FName(TEXT("DiskSize")), SizeText) && LexTryParseString(SizeBytes, *SizeText))
	{
		return SizeBytes;
	}
	if (AssetData.GetTagValue(FName(TEXT("FileSize")), SizeText) && LexTryParseString(SizeBytes, *SizeText))
	{
		return SizeBytes;
	}
	return -1;
}

static bool HCI_Llm_ScanAssetsForPlannerEnvContext(
	const FString& ScanRoot,
	TArray<FHCIAgentPlannerEnvAssetEntry>& OutAssets,
	FString& OutError)
{
	OutAssets.Reset();
	OutError.Reset();

	FString SafeRoot = ScanRoot.TrimStartAndEnd();
	if (SafeRoot.IsEmpty() || !SafeRoot.StartsWith(TEXT("/Game/")))
	{
		SafeRoot = TEXT("/Game/Temp");
	}

	const TArray<FString> AssetPaths = UEditorAssetLibrary::ListAssets(SafeRoot, true, false);
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	OutAssets.Reserve(AssetPaths.Num());
	for (const FString& ObjectPath : AssetPaths)
	{
		FHCIAgentPlannerEnvAssetEntry Entry;
		Entry.ObjectPath = ObjectPath;

		FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(ObjectPath));
		if (AssetData.IsValid())
		{
			Entry.AssetClass = AssetData.AssetClassPath.GetAssetName().ToString();
			Entry.SizeBytes = HCI_Llm_TryExtractAssetSizeFromTags(AssetData);
		}
		else
		{
			Entry.AssetClass = TEXT("Unknown");
			Entry.SizeBytes = -1;
		}

		OutAssets.Add(MoveTemp(Entry));
	}

	OutAssets.Sort([](const FHCIAgentPlannerEnvAssetEntry& Lhs, const FHCIAgentPlannerEnvAssetEntry& Rhs)
	{
		return Lhs.ObjectPath < Rhs.ObjectPath;
	});
	return true;
}

static bool HCI_Llm_BuildAgentPlanWithLlmPreferred(
	const FString& UserText,
	const FString& RequestId,
	const FHCIAgentPlannerBuildOptions& PlannerOptions,
	FHCIAgentPlan& OutPlan,
	FString& OutRouteReason,
	FHCIAgentPlannerResultMetadata& OutPlannerMetadata,
	FHCIAgentPlanValidationResult& OutValidation,
	FString& OutError)
{
	const FHCIToolRegistry& ToolRegistry = FHCIToolRegistry::GetReadOnly();

	if (!FHCIAgentPlanner::BuildPlanFromNaturalLanguageWithProvider(
			UserText,
			RequestId,
			ToolRegistry,
			PlannerOptions,
			OutPlan,
			OutRouteReason,
			OutPlannerMetadata,
			OutError))
	{
		OutValidation = FHCIAgentPlanValidationResult();
		return false;
	}

	if (!FHCIAgentPlanValidator::ValidatePlan(OutPlan, ToolRegistry, OutValidation))
	{
		OutError = FString::Printf(
			TEXT("plan_validation_failed code=%s reason=%s field=%s"),
			OutValidation.ErrorCode.IsEmpty() ? TEXT("-") : *OutValidation.ErrorCode,
			OutValidation.Reason.IsEmpty() ? TEXT("-") : *OutValidation.Reason,
			OutValidation.Field.IsEmpty() ? TEXT("-") : *OutValidation.Field);
		return false;
	}

	return true;
}

static FHCIAgentPlannerBuildOptions HCI_Llm_MakeRealHttpPlannerOptions()
{
	FHCIAgentPlannerBuildOptions PlannerOptions;
	PlannerOptions.bPreferLlm = true;
	PlannerOptions.bUseRealHttpProvider = true;
	PlannerOptions.LlmMockMode = EHCIAgentPlannerLlmMockMode::None;
	PlannerOptions.LlmRetryCount = 1;
	PlannerOptions.bEnableAutoEnvContextScan = true;
	PlannerOptions.ScanAssetsForEnvContext = HCI_Llm_ScanAssetsForPlannerEnvContext;
	PlannerOptions.bLlmEnableThinking = false;
	PlannerOptions.bLlmStream = false;
	PlannerOptions.LlmHttpTimeoutMs = 30000;
	return PlannerOptions;
}

static FHCIAgentPlanPreviewContext HCI_Llm_MakePlanPreviewContext(
	const FString& RouteReason,
	const FHCIAgentPlannerResultMetadata& PlannerMetadata)
{
	FHCIAgentPlanPreviewContext Context;
	Context.RouteReason = RouteReason;
	Context.PlannerProvider = PlannerMetadata.PlannerProvider;
	Context.ProviderMode = PlannerMetadata.ProviderMode;
	Context.bFallbackUsed = PlannerMetadata.bFallbackUsed;
	Context.FallbackReason = PlannerMetadata.FallbackReason;
	Context.EnvScannedAssetCount = PlannerMetadata.bEnvContextInjected ? PlannerMetadata.EnvContextAssetCount : INDEX_NONE;
	return Context;
}

static void HCI_Llm_LogAgentPlanWithProviderSummary(
	const TCHAR* CaseName,
	const FString& UserText,
	const FString& RouteReason,
	const FHCIAgentPlan& Plan,
	const FHCIAgentPlannerResultMetadata& PlannerMetadata,
	const FHCIAgentPlanValidationResult& Validation)
{
	UE_LOG(
		LogHCIAgentDemo,
		Display,
		TEXT("[HCI][AgentPlanLLM] case=%s summary request_id=%s intent=%s input=%s provider=%s provider_mode=%s fallback_used=%s fallback_reason=%s error_code=%s llm_attempts=%d retry_used=%s circuit_open=%s consecutive_llm_failures=%d env_context_injected=%s env_assets=%d env_scan_root=%s plan_validation=%s validation_code=%s validation_reason=%s route_reason=%s steps=%d"),
		CaseName,
		*Plan.RequestId,
		*Plan.Intent,
		*UserText,
		*PlannerMetadata.PlannerProvider,
		*PlannerMetadata.ProviderMode,
		PlannerMetadata.bFallbackUsed ? TEXT("true") : TEXT("false"),
		*PlannerMetadata.FallbackReason,
		*PlannerMetadata.ErrorCode,
		PlannerMetadata.LlmAttemptCount,
		PlannerMetadata.bRetryUsed ? TEXT("true") : TEXT("false"),
		PlannerMetadata.bCircuitBreakerOpen ? TEXT("true") : TEXT("false"),
		PlannerMetadata.ConsecutiveLlmFailures,
		PlannerMetadata.bEnvContextInjected ? TEXT("true") : TEXT("false"),
		PlannerMetadata.EnvContextAssetCount,
		PlannerMetadata.EnvContextScanRoot.IsEmpty() ? TEXT("") : *PlannerMetadata.EnvContextScanRoot,
		Validation.bValid ? TEXT("ok") : TEXT("fail"),
		Validation.ErrorCode.IsEmpty() ? TEXT("-") : *Validation.ErrorCode,
		Validation.Reason.IsEmpty() ? TEXT("-") : *Validation.Reason,
		*RouteReason,
		Plan.Steps.Num());
}

static void HCI_Llm_LogAgentPlanRows(const TCHAR* CaseName, const FString& UserText, const FString& RouteReason, const FHCIAgentPlan& Plan)
{
	for (int32 Index = 0; Index < Plan.Steps.Num(); ++Index)
	{
		const FHCIAgentPlanStep& Step = Plan.Steps[Index];
		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentPlan] case=%s row=%d request_id=%s intent=%s input=%s step_id=%s tool_name=%s risk_level=%s requires_confirm=%s rollback_strategy=%s route_reason=%s expected_evidence=%s args=%s"),
			CaseName,
			Index,
			*Plan.RequestId,
			*Plan.Intent,
			*UserText,
			*Step.StepId,
			*Step.ToolName.ToString(),
			*FHCIAgentPlanContract::RiskLevelToString(Step.RiskLevel),
			Step.bRequiresConfirm ? TEXT("true") : TEXT("false"),
			*Step.RollbackStrategy,
			*RouteReason,
			*FString::Join(Step.ExpectedEvidence, TEXT("|")),
			*HCI_Llm_JsonObjectToCompactString(Step.Args));
	}
}

static void HCI_Llm_RemoveRealLlmProbeRequest(const TSharedPtr<IHttpRequest, ESPMode::ThreadSafe>& Request)
{
	for (int32 Index = HCI_Llm_State().RealLlmProbeRequests.Num() - 1; Index >= 0; --Index)
	{
		if (HCI_Llm_State().RealLlmProbeRequests[Index] == Request)
		{
			HCI_Llm_State().RealLlmProbeRequests.RemoveAtSwap(Index);
			return;
		}
	}
}

static void HCI_Llm_LogAgentPlannerMetrics()
{
	const FHCIAgentPlannerMetricsSnapshot Metrics = FHCIAgentPlanner::GetMetricsSnapshot();
	UE_LOG(
		LogHCIAgentDemo,
		Display,
		TEXT("[HCI][AgentPlanLLM][Metrics] total_requests=%d llm_preferred_requests=%d llm_success_requests=%d keyword_fallback_requests=%d retry_used_requests=%d retry_attempts=%d circuit_open_fallback_requests=%d consecutive_llm_failures=%d"),
		Metrics.TotalRequests,
		Metrics.LlmPreferredRequests,
		Metrics.LlmSuccessRequests,
		Metrics.KeywordFallbackRequests,
		Metrics.RetryUsedRequests,
		Metrics.RetryAttempts,
		Metrics.CircuitOpenFallbackRequests,
		Metrics.ConsecutiveLlmFailures);
}

void HCI_RunAbilityKitAgentPlanWithRealLLMProbeCommand(const TArray<FString>& Args)
{
	const FString UserText = Args.Num() > 0 ? HCI_Llm_JoinConsoleArgsAsText(Args) : FString(TEXT("你是谁"));

	FHCIAgentLlmProviderConfig ProviderConfig;
	FString ConfigError;
	if (!FHCIAgentLlmClient::LoadProviderConfigFromJsonFile(
			TEXT("Saved/HCIAbilityKit/Config/llm_provider.local.json"),
			TEXT("https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions"),
			TEXT("qwen3.5-plus"),
			ProviderConfig,
			ConfigError))
	{
		UE_LOG(LogHCIAgentDemo, Error, TEXT("[HCI][AgentPlanLLM][Probe] config_error=%s"), *ConfigError);
		return;
	}
	ProviderConfig.bEnableThinking = false;
	ProviderConfig.bStream = false;

	FString RequestBody;
	FString RequestBodyError;
	if (!FHCIAgentLlmClient::BuildChatCompletionsRequestBody(TEXT(""), UserText, ProviderConfig, RequestBody, RequestBodyError))
	{
		UE_LOG(LogHCIAgentDemo, Error, TEXT("[HCI][AgentPlanLLM][Probe] request_body_error=%s"), *RequestBodyError);
		return;
	}

	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request =
		FHCIAgentLlmClient::CreateChatCompletionsHttpRequest(ProviderConfig, RequestBody);
	if (!Request.IsValid())
	{
		UE_LOG(LogHCIAgentDemo, Error, TEXT("[HCI][AgentPlanLLM][Probe] create_request_failed"));
		return;
	}
	Request->OnProcessRequestComplete().BindLambda(
		[UserText](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
		{
			HCI_Llm_RemoveRealLlmProbeRequest(HttpRequest);
			const int32 StatusCode = HttpResponse.IsValid() ? HttpResponse->GetResponseCode() : 0;
			const FString Raw = HttpResponse.IsValid() ? HttpResponse->GetContentAsString() : FString(TEXT("<no_response_body>"));

			UE_LOG(
				LogHCIAgentDemo,
				Display,
				TEXT("[HCI][AgentPlanLLM][Probe] done input=%s success=%s status=%d"),
				*UserText,
				bSucceeded ? TEXT("true") : TEXT("false"),
				StatusCode);
			UE_LOG(LogHCIAgentDemo, Display, TEXT("[HCI][AgentPlanLLM][Probe] raw_response=%s"), *Raw);
		});

	if (!Request->ProcessRequest())
	{
		UE_LOG(LogHCIAgentDemo, Error, TEXT("[HCI][AgentPlanLLM][Probe] process_request_failed"));
		return;
	}

	HCI_Llm_State().RealLlmProbeRequests.Add(Request);
	UE_LOG(
		LogHCIAgentDemo,
		Display,
		TEXT("[HCI][AgentPlanLLM][Probe] dispatched input=%s endpoint=%s model=%s enable_thinking=false stream=false"),
		*UserText,
		*ProviderConfig.ApiUrl,
		*ProviderConfig.Model);
}

void HCI_RunAbilityKitAgentPlanWithLLMDemoCommand(const TArray<FString>& Args)
{
	const FString UserText =
		Args.Num() > 0 ? HCI_Llm_JoinConsoleArgsAsText(Args) : FString(TEXT("整理临时目录资产并归档"));
	if (UserText.IsEmpty())
	{
		UE_LOG(LogHCIAgentDemo, Error, TEXT("[HCI][AgentPlanLLM] invalid_args reason=empty input text"));
		return;
	}

	FHCIAgentPlan Plan;
	FString RouteReason;
	FHCIAgentPlannerResultMetadata PlannerMetadata;
	FHCIAgentPlanValidationResult Validation;
	FHCIAgentPlannerBuildOptions PlannerOptions;
	PlannerOptions.bPreferLlm = true;
	PlannerOptions.LlmMockMode = EHCIAgentPlannerLlmMockMode::None;
	FString Error;
	if (!HCI_Llm_BuildAgentPlanWithLlmPreferred(
			UserText,
			TEXT("req_cli_h1_llm"),
			PlannerOptions,
			Plan,
			RouteReason,
			PlannerMetadata,
			Validation,
			Error))
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentPlanLLM] build_failed input=%s provider=%s provider_mode=%s fallback_used=%s fallback_reason=%s error_code=%s reason=%s"),
			*UserText,
			*PlannerMetadata.PlannerProvider,
			*PlannerMetadata.ProviderMode,
			PlannerMetadata.bFallbackUsed ? TEXT("true") : TEXT("false"),
			*PlannerMetadata.FallbackReason,
			PlannerMetadata.ErrorCode.IsEmpty() ? TEXT("-") : *PlannerMetadata.ErrorCode,
			*Error);
		return;
	}

	HCI_Llm_State().AgentPlanPreviewState = Plan;
	HCI_Llm_LogAgentPlanWithProviderSummary(TEXT("custom"), UserText, RouteReason, Plan, PlannerMetadata, Validation);
	HCI_Llm_LogAgentPlanRows(TEXT("custom"), UserText, RouteReason, Plan);
}

void HCI_RunAbilityKitAgentPlanWithRealLLMDemoCommand(const TArray<FString>& Args)
{
	FString Mode = TEXT("normal");
	TArray<FString> PromptArgs = Args;
	if (Args.Num() > 0)
	{
		const FString FirstArg = Args[0].TrimStartAndEnd().ToLower();
		if (FirstArg == TEXT("http_fail") || FirstArg == TEXT("config_missing"))
		{
			Mode = FirstArg;
			PromptArgs.RemoveAt(0);
		}
	}

	const FString UserText =
		PromptArgs.Num() > 0 ? HCI_Llm_JoinConsoleArgsAsText(PromptArgs) : FString(TEXT("整理临时目录资产并归档"));
	if (UserText.IsEmpty())
	{
		UE_LOG(LogHCIAgentDemo, Error, TEXT("[HCI][AgentPlanLLM][H3] invalid_args reason=empty input text"));
		return;
	}

	if (HCI_Llm_State().bRealLlmPlanCommandInFlight.Exchange(true))
	{
		UE_LOG(
			LogHCIAgentDemo,
			Warning,
			TEXT("[HCI][AgentPlanLLM][H3] busy reason=previous_request_in_flight mode=%s input=%s"),
			*Mode,
			*UserText);
		return;
	}

	UE_LOG(
		LogHCIAgentDemo,
		Display,
		TEXT("[HCI][AgentPlanLLM][H3] dispatched mode=%s input=%s hint=异步执行中，结果将随后输出"),
		*Mode,
		*UserText);

	const FHCIToolRegistry& ToolRegistry = FHCIToolRegistry::GetReadOnly();

	FHCIAgentPlannerBuildOptions PlannerOptions = HCI_Llm_MakeRealHttpPlannerOptions();
	if (Mode == TEXT("http_fail"))
	{
		PlannerOptions.LlmApiUrl = TEXT("http://127.0.0.1:1/invalid");
	}
	else if (Mode == TEXT("config_missing"))
	{
		PlannerOptions.LlmApiKeyConfigPath = TEXT("Saved/HCIAbilityKit/Config/h3_missing_config.json");
	}

	FHCIAgentPlanner::BuildPlanFromNaturalLanguageWithProviderAsync(
		UserText,
		FString::Printf(TEXT("req_cli_h3_real_llm_%s"), *Mode),
		ToolRegistry,
		PlannerOptions,
		[Mode, UserText, ToolRegistryPtr = &ToolRegistry](bool bBuilt, FHCIAgentPlan Plan, FString RouteReason, FHCIAgentPlannerResultMetadata PlannerMetadata, FString Error)
		{
			HCI_Llm_State().bRealLlmPlanCommandInFlight.Store(false);

			if (!bBuilt)
			{
				UE_LOG(
					LogHCIAgentDemo,
					Error,
					TEXT("[HCI][AgentPlanLLM][H3] build_failed mode=%s input=%s provider=%s provider_mode=%s fallback_used=%s fallback_reason=%s error_code=%s reason=%s hint=检查配置文件 Saved/HCIAbilityKit/Config/llm_provider.local.json"),
					*Mode,
					*UserText,
					*PlannerMetadata.PlannerProvider,
					*PlannerMetadata.ProviderMode,
					PlannerMetadata.bFallbackUsed ? TEXT("true") : TEXT("false"),
					*PlannerMetadata.FallbackReason,
					PlannerMetadata.ErrorCode.IsEmpty() ? TEXT("-") : *PlannerMetadata.ErrorCode,
					*Error);
				return;
			}

			FHCIAgentPlanValidationResult Validation;
			FHCIAgentPlanValidationContext ValidationContext;
			ValidationContext.bRequireWriteStepForModifyIntent = true;
			ValidationContext.bRequirePipelineInputs = true;
			if (!FHCIAgentPlanValidator::ValidatePlan(Plan, *ToolRegistryPtr, ValidationContext, Validation))
			{
				UE_LOG(
					LogHCIAgentDemo,
					Error,
					TEXT("[HCI][AgentPlanLLM][H3] build_failed mode=%s input=%s provider=%s provider_mode=%s fallback_used=%s fallback_reason=%s error_code=%s reason=plan_validation_failed code=%s field=%s validation_reason=%s"),
					*Mode,
					*UserText,
					*PlannerMetadata.PlannerProvider,
					*PlannerMetadata.ProviderMode,
					PlannerMetadata.bFallbackUsed ? TEXT("true") : TEXT("false"),
					*PlannerMetadata.FallbackReason,
					PlannerMetadata.ErrorCode.IsEmpty() ? TEXT("-") : *PlannerMetadata.ErrorCode,
					Validation.ErrorCode.IsEmpty() ? TEXT("-") : *Validation.ErrorCode,
					Validation.Field.IsEmpty() ? TEXT("-") : *Validation.Field,
					Validation.Reason.IsEmpty() ? TEXT("-") : *Validation.Reason);
				return;
			}

			HCI_Llm_State().AgentPlanPreviewState = Plan;
			const FString CaseLabel = FString::Printf(TEXT("real_http_%s"), *Mode);
			HCI_Llm_LogAgentPlanWithProviderSummary(*CaseLabel, UserText, RouteReason, Plan, PlannerMetadata, Validation);
			HCI_Llm_LogAgentPlanRows(*CaseLabel, UserText, RouteReason, Plan);
			const FHCIAgentPlanPreviewContext PreviewContext = HCI_Llm_MakePlanPreviewContext(RouteReason, PlannerMetadata);
			FHCIAgentPlanPreviewWindow::OpenWindow(
				Plan,
				PreviewContext);
		});
}

void HCI_RunAbilityKitAgentPlanWithLLMFallbackDemoCommand(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentPlanLLM] invalid_args usage=HCI.AgentPlanWithLLMFallbackDemo [timeout|invalid_json|empty|contract_invalid] [自然语言文本...]"));
		return;
	}

	EHCIAgentPlannerLlmMockMode LlmMockMode = EHCIAgentPlannerLlmMockMode::None;
	if (!HCI_Llm_TryParseLlmMockModeArg(Args[0], LlmMockMode) || LlmMockMode == EHCIAgentPlannerLlmMockMode::None)
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentPlanLLM] invalid_args reason=mock_mode must be timeout|invalid_json|empty|contract_invalid"));
		return;
	}

	TArray<FString> PromptArgs;
	if (Args.Num() > 1)
	{
		for (int32 Index = 1; Index < Args.Num(); ++Index)
		{
			PromptArgs.Add(Args[Index]);
		}
	}
	const FString UserText =
		PromptArgs.Num() > 0 ? HCI_Llm_JoinConsoleArgsAsText(PromptArgs) : FString(TEXT("整理临时目录资产并归档"));

	FHCIAgentPlan Plan;
	FString RouteReason;
	FHCIAgentPlannerResultMetadata PlannerMetadata;
	FHCIAgentPlanValidationResult Validation;
	FHCIAgentPlannerBuildOptions PlannerOptions;
	PlannerOptions.bPreferLlm = true;
	PlannerOptions.LlmMockMode = LlmMockMode;
	FString Error;
	if (!HCI_Llm_BuildAgentPlanWithLlmPreferred(
			UserText,
			TEXT("req_cli_h1_fallback"),
			PlannerOptions,
			Plan,
			RouteReason,
			PlannerMetadata,
			Validation,
			Error))
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentPlanLLM] fallback_build_failed input=%s provider=%s provider_mode=%s fallback_used=%s fallback_reason=%s error_code=%s reason=%s"),
			*UserText,
			*PlannerMetadata.PlannerProvider,
			*PlannerMetadata.ProviderMode,
			PlannerMetadata.bFallbackUsed ? TEXT("true") : TEXT("false"),
			*PlannerMetadata.FallbackReason,
			PlannerMetadata.ErrorCode.IsEmpty() ? TEXT("-") : *PlannerMetadata.ErrorCode,
			*Error);
		return;
	}

	HCI_Llm_State().AgentPlanPreviewState = Plan;
	HCI_Llm_LogAgentPlanWithProviderSummary(TEXT("fallback"), UserText, RouteReason, Plan, PlannerMetadata, Validation);
	HCI_Llm_LogAgentPlanRows(TEXT("fallback"), UserText, RouteReason, Plan);
}

void HCI_RunAbilityKitAgentPlanWithLLMStabilityDemoCommand(const TArray<FString>& Args)
{
	const FString CaseKey = Args.Num() > 0 ? Args[0].TrimStartAndEnd().ToLower() : FString(TEXT("all"));
	TArray<FString> PromptArgs;
	if (Args.Num() > 1)
	{
		for (int32 Index = 1; Index < Args.Num(); ++Index)
		{
			PromptArgs.Add(Args[Index]);
		}
	}
	const FString UserText =
		PromptArgs.Num() > 0 ? HCI_Llm_JoinConsoleArgsAsText(PromptArgs) : FString(TEXT("整理临时目录资产并归档"));

	auto RunOneCase = [&UserText](const TCHAR* InCaseName, const TCHAR* InRequestId, const FHCIAgentPlannerBuildOptions& InPlannerOptions)
	{
		FHCIAgentPlan Plan;
		FString RouteReason;
		FHCIAgentPlannerResultMetadata PlannerMetadata;
		FHCIAgentPlanValidationResult Validation;
		FString Error;
		if (!HCI_Llm_BuildAgentPlanWithLlmPreferred(
				UserText,
				InRequestId,
				InPlannerOptions,
				Plan,
				RouteReason,
				PlannerMetadata,
				Validation,
				Error))
		{
			UE_LOG(
				LogHCIAgentDemo,
				Error,
				TEXT("[HCI][AgentPlanLLM][H2] case=%s build_failed input=%s provider=%s fallback_used=%s fallback_reason=%s error_code=%s reason=%s"),
				InCaseName,
				*UserText,
				*PlannerMetadata.PlannerProvider,
				PlannerMetadata.bFallbackUsed ? TEXT("true") : TEXT("false"),
				*PlannerMetadata.FallbackReason,
				PlannerMetadata.ErrorCode.IsEmpty() ? TEXT("-") : *PlannerMetadata.ErrorCode,
				*Error);
			return false;
		}

		HCI_Llm_State().AgentPlanPreviewState = Plan;
		HCI_Llm_LogAgentPlanWithProviderSummary(InCaseName, UserText, RouteReason, Plan, PlannerMetadata, Validation);
		HCI_Llm_LogAgentPlanRows(InCaseName, UserText, RouteReason, Plan);
		return true;
	};

	if (CaseKey == TEXT("reset_metrics"))
	{
		FHCIAgentPlanner::ResetMetricsForTesting();
		UE_LOG(LogHCIAgentDemo, Display, TEXT("[HCI][AgentPlanLLM][H2] reset_metrics=ok"));
		HCI_Llm_LogAgentPlannerMetrics();
		return;
	}

	if (CaseKey == TEXT("retry_success"))
	{
		FHCIAgentPlannerBuildOptions RetrySuccessOptions;
		RetrySuccessOptions.bPreferLlm = true;
		RetrySuccessOptions.LlmMockMode = EHCIAgentPlannerLlmMockMode::Timeout;
		RetrySuccessOptions.LlmRetryCount = 1;
		RetrySuccessOptions.LlmMockFailAttempts = 1;
		RetrySuccessOptions.CircuitBreakerFailureThreshold = 3;
		RetrySuccessOptions.CircuitBreakerOpenForRequests = 2;
		RunOneCase(TEXT("retry_success"), TEXT("req_cli_h2_retry_success"), RetrySuccessOptions);
		HCI_Llm_LogAgentPlannerMetrics();
		return;
	}

	if (CaseKey == TEXT("retry_fallback"))
	{
		FHCIAgentPlannerBuildOptions RetryFallbackOptions;
		RetryFallbackOptions.bPreferLlm = true;
		RetryFallbackOptions.LlmMockMode = EHCIAgentPlannerLlmMockMode::Timeout;
		RetryFallbackOptions.LlmRetryCount = 1;
		RetryFallbackOptions.CircuitBreakerFailureThreshold = 3;
		RetryFallbackOptions.CircuitBreakerOpenForRequests = 2;
		RunOneCase(TEXT("retry_fallback"), TEXT("req_cli_h2_retry_fallback"), RetryFallbackOptions);
		HCI_Llm_LogAgentPlannerMetrics();
		return;
	}

	if (CaseKey == TEXT("circuit_open"))
	{
		FHCIAgentPlanner::ResetMetricsForTesting();

		FHCIAgentPlannerBuildOptions TriggerOpenOptions;
		TriggerOpenOptions.bPreferLlm = true;
		TriggerOpenOptions.LlmMockMode = EHCIAgentPlannerLlmMockMode::Timeout;
		TriggerOpenOptions.LlmRetryCount = 0;
		TriggerOpenOptions.CircuitBreakerFailureThreshold = 1;
		TriggerOpenOptions.CircuitBreakerOpenForRequests = 1;
		RunOneCase(TEXT("circuit_open_trigger"), TEXT("req_cli_h2_circuit_01"), TriggerOpenOptions);

		FHCIAgentPlannerBuildOptions OpenFallbackOptions;
		OpenFallbackOptions.bPreferLlm = true;
		OpenFallbackOptions.LlmMockMode = EHCIAgentPlannerLlmMockMode::None;
		OpenFallbackOptions.LlmRetryCount = 0;
		OpenFallbackOptions.CircuitBreakerFailureThreshold = 1;
		OpenFallbackOptions.CircuitBreakerOpenForRequests = 1;
		RunOneCase(TEXT("circuit_open_fallback"), TEXT("req_cli_h2_circuit_02"), OpenFallbackOptions);

		HCI_Llm_LogAgentPlannerMetrics();
		return;
	}

	if (CaseKey == TEXT("all"))
	{
		FHCIAgentPlanner::ResetMetricsForTesting();

		FHCIAgentPlannerBuildOptions RetrySuccessOptions;
		RetrySuccessOptions.bPreferLlm = true;
		RetrySuccessOptions.LlmMockMode = EHCIAgentPlannerLlmMockMode::Timeout;
		RetrySuccessOptions.LlmRetryCount = 1;
		RetrySuccessOptions.LlmMockFailAttempts = 1;
		RetrySuccessOptions.CircuitBreakerFailureThreshold = 3;
		RetrySuccessOptions.CircuitBreakerOpenForRequests = 2;
		RunOneCase(TEXT("retry_success"), TEXT("req_cli_h2_all_01"), RetrySuccessOptions);

		FHCIAgentPlannerBuildOptions RetryFallbackOptions;
		RetryFallbackOptions.bPreferLlm = true;
		RetryFallbackOptions.LlmMockMode = EHCIAgentPlannerLlmMockMode::Timeout;
		RetryFallbackOptions.LlmRetryCount = 1;
		RetryFallbackOptions.CircuitBreakerFailureThreshold = 3;
		RetryFallbackOptions.CircuitBreakerOpenForRequests = 2;
		RunOneCase(TEXT("retry_fallback"), TEXT("req_cli_h2_all_02"), RetryFallbackOptions);

		FHCIAgentPlannerBuildOptions TriggerOpenOptions;
		TriggerOpenOptions.bPreferLlm = true;
		TriggerOpenOptions.LlmMockMode = EHCIAgentPlannerLlmMockMode::Timeout;
		TriggerOpenOptions.LlmRetryCount = 0;
		TriggerOpenOptions.CircuitBreakerFailureThreshold = 1;
		TriggerOpenOptions.CircuitBreakerOpenForRequests = 1;
		RunOneCase(TEXT("circuit_open_trigger"), TEXT("req_cli_h2_all_03"), TriggerOpenOptions);

		FHCIAgentPlannerBuildOptions OpenFallbackOptions;
		OpenFallbackOptions.bPreferLlm = true;
		OpenFallbackOptions.LlmMockMode = EHCIAgentPlannerLlmMockMode::None;
		OpenFallbackOptions.LlmRetryCount = 0;
		OpenFallbackOptions.CircuitBreakerFailureThreshold = 1;
		OpenFallbackOptions.CircuitBreakerOpenForRequests = 1;
		RunOneCase(TEXT("circuit_open_fallback"), TEXT("req_cli_h2_all_04"), OpenFallbackOptions);

		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentPlanLLM][H2] summary total_cases=4 includes=retry_success|retry_fallback|circuit_open_trigger|circuit_open_fallback validation=ok"));
		HCI_Llm_LogAgentPlannerMetrics();
		return;
	}

	UE_LOG(
		LogHCIAgentDemo,
		Error,
		TEXT("[HCI][AgentPlanLLM][H2] invalid_args usage=HCI.AgentPlanWithLLMStabilityDemo [all|retry_success|retry_fallback|circuit_open|reset_metrics] [自然语言文本...]"));
}

void HCI_RunAbilityKitAgentPlanWithLLMMetricsDumpCommand(const TArray<FString>& Args)
{
	(void)Args;
	HCI_Llm_LogAgentPlannerMetrics();
}

#undef LogHCIAgentDemo

