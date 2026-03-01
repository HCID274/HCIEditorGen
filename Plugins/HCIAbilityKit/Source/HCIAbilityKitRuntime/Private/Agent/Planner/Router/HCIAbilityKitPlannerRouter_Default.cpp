#include "Agent/Planner/Router/HCIAbilityKitPlannerRouter_Default.h"

#include "Agent/Planner/Interfaces/IHCIAbilityKitPlannerProvider.h"
#include "Agent/Planner/Providers/HCIAbilityKitKeywordPlannerProvider.h"
#include "Agent/Planner/Providers/HCIAbilityKitLlmPlannerProvider.h"

#include "HAL/CriticalSection.h"

DEFINE_LOG_CATEGORY_STATIC(LogHCIAbilityKitAgentPlanner, Log, All);

namespace
{
static const TCHAR* const HCI_LlmProviderName = TEXT("llm");
static const TCHAR* const HCI_KeywordProviderName = TEXT("keyword_fallback");
static const TCHAR* const HCI_FallbackReasonNone = TEXT("none");
static const TCHAR* const HCI_FallbackReasonCircuitOpen = TEXT("llm_circuit_open");
static const TCHAR* const HCI_FallbackReasonSyncRealHttpDeprecated = TEXT("llm_sync_real_http_deprecated");

struct FHCIAbilityKitAgentPlannerRuntimeState
{
	int32 ConsecutiveLlmFailures = 0;
	int32 CircuitOpenRemainingRequests = 0;
	FHCIAbilityKitAgentPlannerMetricsSnapshot Metrics;
};

static FCriticalSection GHCIAbilityKitAgentPlannerRuntimeStateMutex;
static FHCIAbilityKitAgentPlannerRuntimeState GHCIAbilityKitAgentPlannerRuntimeState;

static FString HCI_GetProviderModeForPreferredLlm(const FHCIAbilityKitAgentPlannerBuildOptions& Options)
{
	return Options.bUseRealHttpProvider ? TEXT("real_http") : TEXT("mock");
}

class FHCIAbilityKitPlannerProvider_KeywordDirect final : public IHCIAbilityKitPlannerProvider
{
public:
	explicit FHCIAbilityKitPlannerProvider_KeywordDirect(const TSharedRef<IHCIAbilityKitPlannerProvider>& InKeywordProvider)
		: KeywordProvider(InKeywordProvider)
	{
	}

	virtual const TCHAR* GetName() const override { return HCI_KeywordProviderName; }
	virtual bool IsNetworkBacked() const override { return false; }

	virtual bool BuildPlan(
		const FString& UserText,
		const FString& RequestId,
		const FHCIAbilityKitToolRegistry& ToolRegistry,
		const FHCIAbilityKitAgentPlannerBuildOptions& Options,
		FHCIAbilityKitAgentPlan& OutPlan,
		FString& OutRouteReason,
		FHCIAbilityKitAgentPlannerResultMetadata& OutMetadata,
		FString& OutError) override
	{
		const bool bBuilt = KeywordProvider->BuildPlan(UserText, RequestId, ToolRegistry, Options, OutPlan, OutRouteReason, OutMetadata, OutError);
		if (!bBuilt)
		{
			return false;
		}

		OutMetadata = FHCIAbilityKitAgentPlannerResultMetadata();
		OutMetadata.PlannerProvider = HCI_KeywordProviderName;
		OutMetadata.ProviderMode = TEXT("keyword");
		OutMetadata.bFallbackUsed = false;
		OutMetadata.FallbackReason = HCI_FallbackReasonNone;
		{
			FScopeLock Lock(&GHCIAbilityKitAgentPlannerRuntimeStateMutex);
			OutMetadata.ConsecutiveLlmFailures = GHCIAbilityKitAgentPlannerRuntimeState.ConsecutiveLlmFailures;
		}
		return true;
	}

	virtual void BuildPlanAsync(
		const FString& UserText,
		const FString& RequestId,
		const FHCIAbilityKitToolRegistry& ToolRegistry,
		const FHCIAbilityKitAgentPlannerBuildOptions& Options,
		TFunction<void(bool, FHCIAbilityKitAgentPlan, FString, FHCIAbilityKitAgentPlannerResultMetadata, FString)>&& OnComplete) override
	{
		FHCIAbilityKitAgentPlan Plan;
		FString RouteReason;
		FHCIAbilityKitAgentPlannerResultMetadata Metadata;
		FString Error;
		const bool bBuilt = BuildPlan(UserText, RequestId, ToolRegistry, Options, Plan, RouteReason, Metadata, Error);
		OnComplete(bBuilt, MoveTemp(Plan), MoveTemp(RouteReason), MoveTemp(Metadata), MoveTemp(Error));
	}

private:
	TSharedRef<IHCIAbilityKitPlannerProvider> KeywordProvider;
};

class FHCIAbilityKitPlannerProvider_CircuitOpenKeywordFallback final : public IHCIAbilityKitPlannerProvider
{
public:
	explicit FHCIAbilityKitPlannerProvider_CircuitOpenKeywordFallback(const TSharedRef<IHCIAbilityKitPlannerProvider>& InKeywordProvider)
		: KeywordProvider(InKeywordProvider)
	{
	}

	virtual const TCHAR* GetName() const override { return HCI_KeywordProviderName; }
	virtual bool IsNetworkBacked() const override { return false; }

	virtual bool BuildPlan(
		const FString& UserText,
		const FString& RequestId,
		const FHCIAbilityKitToolRegistry& ToolRegistry,
		const FHCIAbilityKitAgentPlannerBuildOptions& Options,
		FHCIAbilityKitAgentPlan& OutPlan,
		FString& OutRouteReason,
		FHCIAbilityKitAgentPlannerResultMetadata& OutMetadata,
		FString& OutError) override
	{
		{
			FScopeLock Lock(&GHCIAbilityKitAgentPlannerRuntimeStateMutex);
			if (GHCIAbilityKitAgentPlannerRuntimeState.CircuitOpenRemainingRequests > 0)
			{
				GHCIAbilityKitAgentPlannerRuntimeState.CircuitOpenRemainingRequests -= 1;
			}
			GHCIAbilityKitAgentPlannerRuntimeState.Metrics.CircuitOpenFallbackRequests += 1;
			GHCIAbilityKitAgentPlannerRuntimeState.Metrics.KeywordFallbackRequests += 1;
		}

		const bool bBuilt = KeywordProvider->BuildPlan(UserText, RequestId, ToolRegistry, Options, OutPlan, OutRouteReason, OutMetadata, OutError);
		if (!bBuilt)
		{
			return false;
		}

		OutMetadata = FHCIAbilityKitAgentPlannerResultMetadata();
		OutMetadata.PlannerProvider = HCI_KeywordProviderName;
		OutMetadata.ProviderMode = HCI_GetProviderModeForPreferredLlm(Options);
		OutMetadata.bFallbackUsed = true;
		OutMetadata.FallbackReason = HCI_FallbackReasonCircuitOpen;
		OutMetadata.ErrorCode = TEXT("E4305");
		OutMetadata.bCircuitBreakerOpen = true;
		{
			FScopeLock Lock(&GHCIAbilityKitAgentPlannerRuntimeStateMutex);
			OutMetadata.ConsecutiveLlmFailures = GHCIAbilityKitAgentPlannerRuntimeState.ConsecutiveLlmFailures;
			OutMetadata.bCircuitBreakerOpen = (GHCIAbilityKitAgentPlannerRuntimeState.CircuitOpenRemainingRequests > 0);
		}
		return true;
	}

	virtual void BuildPlanAsync(
		const FString& UserText,
		const FString& RequestId,
		const FHCIAbilityKitToolRegistry& ToolRegistry,
		const FHCIAbilityKitAgentPlannerBuildOptions& Options,
		TFunction<void(bool, FHCIAbilityKitAgentPlan, FString, FHCIAbilityKitAgentPlannerResultMetadata, FString)>&& OnComplete) override
	{
		FHCIAbilityKitAgentPlan Plan;
		FString RouteReason;
		FHCIAbilityKitAgentPlannerResultMetadata Metadata;
		FString Error;
		const bool bBuilt = BuildPlan(UserText, RequestId, ToolRegistry, Options, Plan, RouteReason, Metadata, Error);
		OnComplete(bBuilt, MoveTemp(Plan), MoveTemp(RouteReason), MoveTemp(Metadata), MoveTemp(Error));
	}

private:
	TSharedRef<IHCIAbilityKitPlannerProvider> KeywordProvider;
};

class FHCIAbilityKitPlannerProvider_LlmFirstWithKeywordFallback final : public IHCIAbilityKitPlannerProvider
{
public:
	FHCIAbilityKitPlannerProvider_LlmFirstWithKeywordFallback(
		const TSharedRef<IHCIAbilityKitPlannerProvider>& InLlmProvider,
		const TSharedRef<IHCIAbilityKitPlannerProvider>& InKeywordProvider)
		: LlmProvider(InLlmProvider)
		, KeywordProvider(InKeywordProvider)
	{
	}

	virtual const TCHAR* GetName() const override { return HCI_LlmProviderName; }
	virtual bool IsNetworkBacked() const override { return true; }

	virtual bool BuildPlan(
		const FString& UserText,
		const FString& RequestId,
		const FHCIAbilityKitToolRegistry& ToolRegistry,
		const FHCIAbilityKitAgentPlannerBuildOptions& Options,
		FHCIAbilityKitAgentPlan& OutPlan,
		FString& OutRouteReason,
		FHCIAbilityKitAgentPlannerResultMetadata& OutMetadata,
		FString& OutError) override;

	virtual void BuildPlanAsync(
		const FString& UserText,
		const FString& RequestId,
		const FHCIAbilityKitToolRegistry& ToolRegistry,
		const FHCIAbilityKitAgentPlannerBuildOptions& Options,
		TFunction<void(bool, FHCIAbilityKitAgentPlan, FString, FHCIAbilityKitAgentPlannerResultMetadata, FString)>&& OnComplete) override;

private:
	TSharedRef<IHCIAbilityKitPlannerProvider> LlmProvider;
	TSharedRef<IHCIAbilityKitPlannerProvider> KeywordProvider;
};

TSharedRef<IHCIAbilityKitPlannerProvider> HCI_GetSharedLlmProvider()
{
	static TSharedRef<IHCIAbilityKitPlannerProvider> Provider = MakeShared<FHCIAbilityKitLlmPlannerProvider>();
	return Provider;
}

TSharedRef<IHCIAbilityKitPlannerProvider> HCI_GetSharedKeywordProvider()
{
	static TSharedRef<IHCIAbilityKitPlannerProvider> Provider = MakeShared<FHCIAbilityKitKeywordPlannerProvider>();
	return Provider;
}

TSharedRef<IHCIAbilityKitPlannerProvider> HCI_GetSharedLlmFirstProvider()
{
	static TSharedRef<IHCIAbilityKitPlannerProvider> Provider =
		MakeShared<FHCIAbilityKitPlannerProvider_LlmFirstWithKeywordFallback>(HCI_GetSharedLlmProvider(), HCI_GetSharedKeywordProvider());
	return Provider;
}

TSharedRef<IHCIAbilityKitPlannerProvider> HCI_GetSharedKeywordDirectProvider()
{
	static TSharedRef<IHCIAbilityKitPlannerProvider> Provider =
		MakeShared<FHCIAbilityKitPlannerProvider_KeywordDirect>(HCI_GetSharedKeywordProvider());
	return Provider;
}

TSharedRef<IHCIAbilityKitPlannerProvider> HCI_MakeCircuitOpenProvider()
{
	return MakeShared<FHCIAbilityKitPlannerProvider_CircuitOpenKeywordFallback>(HCI_GetSharedKeywordProvider());
}

bool HCI_IsCircuitBreakerOpen(const FHCIAbilityKitAgentPlannerBuildOptions& Options)
{
	const bool bCircuitBreakerEnabled =
		Options.bEnableCircuitBreaker &&
		Options.CircuitBreakerFailureThreshold > 0 &&
		Options.CircuitBreakerOpenForRequests > 0;
	if (!bCircuitBreakerEnabled)
	{
		return false;
	}
	FScopeLock Lock(&GHCIAbilityKitAgentPlannerRuntimeStateMutex);
	return GHCIAbilityKitAgentPlannerRuntimeState.CircuitOpenRemainingRequests > 0;
}
} // namespace

TSharedRef<IHCIAbilityKitPlannerProvider> FHCIAbilityKitPlannerRouter_Default::SelectProvider(
	const FHCIAbilityKitAgentPlannerBuildOptions& Options)
{
	{
		FScopeLock Lock(&GHCIAbilityKitAgentPlannerRuntimeStateMutex);
		GHCIAbilityKitAgentPlannerRuntimeState.Metrics.TotalRequests += 1;
		if (Options.bPreferLlm)
		{
			GHCIAbilityKitAgentPlannerRuntimeState.Metrics.LlmPreferredRequests += 1;
		}
	}

	if (!Options.bPreferLlm)
	{
		return HCI_GetSharedKeywordDirectProvider();
	}

	if (HCI_IsCircuitBreakerOpen(Options))
	{
		return HCI_MakeCircuitOpenProvider();
	}

	return HCI_GetSharedLlmFirstProvider();
}

FHCIAbilityKitAgentPlannerMetricsSnapshot FHCIAbilityKitPlannerRouter_Default::GetMetricsSnapshot()
{
	FScopeLock Lock(&GHCIAbilityKitAgentPlannerRuntimeStateMutex);
	return GHCIAbilityKitAgentPlannerRuntimeState.Metrics;
}

void FHCIAbilityKitPlannerRouter_Default::ResetMetricsForTesting()
{
	FScopeLock Lock(&GHCIAbilityKitAgentPlannerRuntimeStateMutex);
	GHCIAbilityKitAgentPlannerRuntimeState = FHCIAbilityKitAgentPlannerRuntimeState();
}

bool FHCIAbilityKitPlannerProvider_LlmFirstWithKeywordFallback::BuildPlan(
	const FString& UserText,
	const FString& RequestId,
	const FHCIAbilityKitToolRegistry& ToolRegistry,
	const FHCIAbilityKitAgentPlannerBuildOptions& Options,
	FHCIAbilityKitAgentPlan& OutPlan,
	FString& OutRouteReason,
	FHCIAbilityKitAgentPlannerResultMetadata& OutMetadata,
	FString& OutError)
{
	OutPlan = FHCIAbilityKitAgentPlan();
	OutRouteReason.Reset();
	OutError.Reset();
	OutMetadata = FHCIAbilityKitAgentPlannerResultMetadata();

	const FString ProviderMode = HCI_GetProviderModeForPreferredLlm(Options);

	if (Options.bUseRealHttpProvider)
	{
		OutMetadata.PlannerProvider = HCI_KeywordProviderName;
		OutMetadata.ProviderMode = TEXT("real_http");
		OutMetadata.bFallbackUsed = true;
		OutMetadata.FallbackReason = HCI_FallbackReasonSyncRealHttpDeprecated;
		OutMetadata.ErrorCode = TEXT("E4310");
		{
			FScopeLock Lock(&GHCIAbilityKitAgentPlannerRuntimeStateMutex);
			OutMetadata.ConsecutiveLlmFailures = GHCIAbilityKitAgentPlannerRuntimeState.ConsecutiveLlmFailures;
		}
		OutError = TEXT("real_http_provider_requires_async_api");
		return false;
	}

	FHCIAbilityKitAgentPlan LlmPlan;
	FString LlmRouteReason;
	FHCIAbilityKitAgentPlannerResultMetadata LlmMetadata;
	FString LlmError;
	const bool bLlmBuilt = LlmProvider->BuildPlan(
		UserText,
		RequestId,
		ToolRegistry,
		Options,
		LlmPlan,
		LlmRouteReason,
		LlmMetadata,
		LlmError);

	const bool bRetryUsed = LlmMetadata.LlmAttemptCount > 1;
	if (bRetryUsed)
	{
		FScopeLock Lock(&GHCIAbilityKitAgentPlannerRuntimeStateMutex);
		GHCIAbilityKitAgentPlannerRuntimeState.Metrics.RetryUsedRequests += 1;
		GHCIAbilityKitAgentPlannerRuntimeState.Metrics.RetryAttempts += (LlmMetadata.LlmAttemptCount - 1);
	}

	if (bLlmBuilt)
	{
		{
			FScopeLock Lock(&GHCIAbilityKitAgentPlannerRuntimeStateMutex);
			GHCIAbilityKitAgentPlannerRuntimeState.ConsecutiveLlmFailures = 0;
			GHCIAbilityKitAgentPlannerRuntimeState.Metrics.ConsecutiveLlmFailures = 0;
			GHCIAbilityKitAgentPlannerRuntimeState.Metrics.LlmSuccessRequests += 1;
		}

		OutPlan = MoveTemp(LlmPlan);
		OutRouteReason = MoveTemp(LlmRouteReason);

		OutMetadata = LlmMetadata;
		OutMetadata.PlannerProvider = HCI_LlmProviderName;
		OutMetadata.ProviderMode = ProviderMode;
		OutMetadata.bFallbackUsed = false;
		OutMetadata.FallbackReason = HCI_FallbackReasonNone;
		OutMetadata.ErrorCode = TEXT("-");
		OutMetadata.bCircuitBreakerOpen = false;
		OutMetadata.ConsecutiveLlmFailures = 0;

		OutError.Reset();
		return true;
	}

	// LLM failed: record failures, then fall back to keyword plan.
	if (LlmMetadata.FallbackReason == TEXT("llm_contract_invalid"))
	{
		UE_LOG(
			LogHCIAbilityKitAgentPlanner,
			Display,
			TEXT("[HCIAbilityKit][AgentPlanLLM][E4303] request_id=%s provider_mode=%s attempts=%d error_code=%s fallback_reason=%s detail=%s input=%s"),
			*RequestId,
			*ProviderMode,
			LlmMetadata.LlmAttemptCount,
			LlmMetadata.ErrorCode.IsEmpty() ? TEXT("E4303") : *LlmMetadata.ErrorCode,
			*LlmMetadata.FallbackReason,
			LlmError.IsEmpty() ? TEXT("-") : *LlmError,
			*UserText);
	}

	bool bCircuitBreakerOpen = false;
	int32 ConsecutiveFailures = 0;
	{
		const bool bCircuitBreakerEnabled =
			Options.bEnableCircuitBreaker &&
			Options.CircuitBreakerFailureThreshold > 0 &&
			Options.CircuitBreakerOpenForRequests > 0;

		FScopeLock Lock(&GHCIAbilityKitAgentPlannerRuntimeStateMutex);
		GHCIAbilityKitAgentPlannerRuntimeState.ConsecutiveLlmFailures += 1;
		ConsecutiveFailures = GHCIAbilityKitAgentPlannerRuntimeState.ConsecutiveLlmFailures;
		GHCIAbilityKitAgentPlannerRuntimeState.Metrics.ConsecutiveLlmFailures = ConsecutiveFailures;
		if (bCircuitBreakerEnabled && ConsecutiveFailures >= Options.CircuitBreakerFailureThreshold)
		{
			GHCIAbilityKitAgentPlannerRuntimeState.CircuitOpenRemainingRequests = Options.CircuitBreakerOpenForRequests;
		}
		GHCIAbilityKitAgentPlannerRuntimeState.Metrics.KeywordFallbackRequests += 1;
		bCircuitBreakerOpen = (GHCIAbilityKitAgentPlannerRuntimeState.CircuitOpenRemainingRequests > 0);
	}

	const FString FallbackDetail = LlmError;
	FHCIAbilityKitAgentPlan KeywordPlan;
	FString KeywordRouteReason;
	FHCIAbilityKitAgentPlannerResultMetadata KeywordMetadata;
	FString KeywordError;
	if (!KeywordProvider->BuildPlan(
			UserText,
			RequestId,
			ToolRegistry,
			Options,
			KeywordPlan,
			KeywordRouteReason,
			KeywordMetadata,
			KeywordError))
	{
		OutError = KeywordError;
		return false;
	}

	OutPlan = MoveTemp(KeywordPlan);
	OutRouteReason = MoveTemp(KeywordRouteReason);
	OutError = FallbackDetail;

	OutMetadata = FHCIAbilityKitAgentPlannerResultMetadata();
	OutMetadata.PlannerProvider = HCI_KeywordProviderName;
	OutMetadata.ProviderMode = ProviderMode;
	OutMetadata.bFallbackUsed = true;
	OutMetadata.FallbackReason = LlmMetadata.FallbackReason;
	OutMetadata.ErrorCode = LlmMetadata.ErrorCode;
	OutMetadata.LlmAttemptCount = LlmMetadata.LlmAttemptCount;
	OutMetadata.bRetryUsed = bRetryUsed;
	OutMetadata.bCircuitBreakerOpen = bCircuitBreakerOpen;
	OutMetadata.ConsecutiveLlmFailures = ConsecutiveFailures;
	OutMetadata.bEnvContextInjected = LlmMetadata.bEnvContextInjected;
	OutMetadata.EnvContextAssetCount = LlmMetadata.EnvContextAssetCount;
	OutMetadata.EnvContextScanRoot = LlmMetadata.EnvContextScanRoot;
	return true;
}

void FHCIAbilityKitPlannerProvider_LlmFirstWithKeywordFallback::BuildPlanAsync(
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

	if (!Options.bUseRealHttpProvider)
	{
		FHCIAbilityKitAgentPlan Plan;
		FString RouteReason;
		FHCIAbilityKitAgentPlannerResultMetadata Metadata;
		FString Error;
		const bool bBuilt = BuildPlan(UserText, RequestId, ToolRegistry, Options, Plan, RouteReason, Metadata, Error);
		OnComplete(bBuilt, MoveTemp(Plan), MoveTemp(RouteReason), MoveTemp(Metadata), MoveTemp(Error));
		return;
	}

	const FString ProviderMode = TEXT("real_http");
	TSharedRef<IHCIAbilityKitPlannerProvider> KeywordProviderPinned = KeywordProvider;

	LlmProvider->BuildPlanAsync(
		UserText,
		RequestId,
		ToolRegistry,
		Options,
		[UserText, RequestId, ToolRegistryPtr = &ToolRegistry, Options, ProviderMode, KeywordProviderPinned, OnComplete = MoveTemp(OnComplete)](
			bool bBuilt,
			FHCIAbilityKitAgentPlan Plan,
			FString RouteReason,
			FHCIAbilityKitAgentPlannerResultMetadata LlmMetadata,
			FString Error) mutable
		{
			const bool bRetryUsed = LlmMetadata.LlmAttemptCount > 1;
			if (bRetryUsed)
			{
				FScopeLock Lock(&GHCIAbilityKitAgentPlannerRuntimeStateMutex);
				GHCIAbilityKitAgentPlannerRuntimeState.Metrics.RetryUsedRequests += 1;
				GHCIAbilityKitAgentPlannerRuntimeState.Metrics.RetryAttempts += (LlmMetadata.LlmAttemptCount - 1);
			}

			if (bBuilt)
			{
				{
					FScopeLock Lock(&GHCIAbilityKitAgentPlannerRuntimeStateMutex);
					GHCIAbilityKitAgentPlannerRuntimeState.ConsecutiveLlmFailures = 0;
					GHCIAbilityKitAgentPlannerRuntimeState.Metrics.ConsecutiveLlmFailures = 0;
					GHCIAbilityKitAgentPlannerRuntimeState.Metrics.LlmSuccessRequests += 1;
				}

				FHCIAbilityKitAgentPlannerResultMetadata Metadata = LlmMetadata;
				Metadata.PlannerProvider = HCI_LlmProviderName;
				Metadata.ProviderMode = ProviderMode;
				Metadata.bFallbackUsed = false;
				Metadata.FallbackReason = HCI_FallbackReasonNone;
				Metadata.ErrorCode = TEXT("-");
				Metadata.bCircuitBreakerOpen = false;
				Metadata.ConsecutiveLlmFailures = 0;

				OnComplete(true, MoveTemp(Plan), MoveTemp(RouteReason), MoveTemp(Metadata), MoveTemp(Error));
				return;
			}

			if (LlmMetadata.FallbackReason == TEXT("llm_contract_invalid"))
			{
				UE_LOG(
					LogHCIAbilityKitAgentPlanner,
					Warning,
					TEXT("[HCIAbilityKit][AgentPlanLLM][E4303] request_id=%s provider_mode=%s attempts=%d error_code=%s fallback_reason=%s detail=%s input=%s"),
					*RequestId,
					*ProviderMode,
					LlmMetadata.LlmAttemptCount,
					LlmMetadata.ErrorCode.IsEmpty() ? TEXT("E4303") : *LlmMetadata.ErrorCode,
					*LlmMetadata.FallbackReason,
					Error.IsEmpty() ? TEXT("-") : *Error,
					*UserText);
			}

			bool bCircuitBreakerOpen = false;
			int32 ConsecutiveFailures = 0;
			{
				const bool bCircuitBreakerEnabled =
					Options.bEnableCircuitBreaker &&
					Options.CircuitBreakerFailureThreshold > 0 &&
					Options.CircuitBreakerOpenForRequests > 0;

				FScopeLock Lock(&GHCIAbilityKitAgentPlannerRuntimeStateMutex);
				GHCIAbilityKitAgentPlannerRuntimeState.ConsecutiveLlmFailures += 1;
				ConsecutiveFailures = GHCIAbilityKitAgentPlannerRuntimeState.ConsecutiveLlmFailures;
				GHCIAbilityKitAgentPlannerRuntimeState.Metrics.ConsecutiveLlmFailures = ConsecutiveFailures;
				if (bCircuitBreakerEnabled && ConsecutiveFailures >= Options.CircuitBreakerFailureThreshold)
				{
					GHCIAbilityKitAgentPlannerRuntimeState.CircuitOpenRemainingRequests = Options.CircuitBreakerOpenForRequests;
				}
				GHCIAbilityKitAgentPlannerRuntimeState.Metrics.KeywordFallbackRequests += 1;
				bCircuitBreakerOpen = (GHCIAbilityKitAgentPlannerRuntimeState.CircuitOpenRemainingRequests > 0);
			}

			FHCIAbilityKitAgentPlan KeywordPlan;
			FString KeywordRouteReason;
			FHCIAbilityKitAgentPlannerResultMetadata KeywordMetadata;
			FString KeywordError;
			if (!KeywordProviderPinned->BuildPlan(
					UserText,
					RequestId,
					*ToolRegistryPtr,
					Options,
					KeywordPlan,
					KeywordRouteReason,
					KeywordMetadata,
					KeywordError))
			{
				FHCIAbilityKitAgentPlannerResultMetadata Metadata = FHCIAbilityKitAgentPlannerResultMetadata();
				Metadata.PlannerProvider = HCI_KeywordProviderName;
				Metadata.ProviderMode = ProviderMode;
				Metadata.bFallbackUsed = true;
				Metadata.FallbackReason = LlmMetadata.FallbackReason;
				Metadata.ErrorCode = LlmMetadata.ErrorCode;
				Metadata.LlmAttemptCount = LlmMetadata.LlmAttemptCount;
				Metadata.bRetryUsed = bRetryUsed;
				Metadata.bCircuitBreakerOpen = bCircuitBreakerOpen;
				Metadata.ConsecutiveLlmFailures = ConsecutiveFailures;
				Metadata.bEnvContextInjected = LlmMetadata.bEnvContextInjected;
				Metadata.EnvContextAssetCount = LlmMetadata.EnvContextAssetCount;
				Metadata.EnvContextScanRoot = LlmMetadata.EnvContextScanRoot;
				OnComplete(false, FHCIAbilityKitAgentPlan(), FString(), MoveTemp(Metadata), MoveTemp(KeywordError));
				return;
			}

			FHCIAbilityKitAgentPlannerResultMetadata Metadata = FHCIAbilityKitAgentPlannerResultMetadata();
			Metadata.PlannerProvider = HCI_KeywordProviderName;
			Metadata.ProviderMode = ProviderMode;
			Metadata.bFallbackUsed = true;
			Metadata.FallbackReason = LlmMetadata.FallbackReason;
			Metadata.ErrorCode = LlmMetadata.ErrorCode;
			Metadata.LlmAttemptCount = LlmMetadata.LlmAttemptCount;
			Metadata.bRetryUsed = bRetryUsed;
			Metadata.bCircuitBreakerOpen = bCircuitBreakerOpen;
			Metadata.ConsecutiveLlmFailures = ConsecutiveFailures;
			Metadata.bEnvContextInjected = LlmMetadata.bEnvContextInjected;
			Metadata.EnvContextAssetCount = LlmMetadata.EnvContextAssetCount;
			Metadata.EnvContextScanRoot = LlmMetadata.EnvContextScanRoot;

			OnComplete(true, MoveTemp(KeywordPlan), MoveTemp(KeywordRouteReason), MoveTemp(Metadata), FString());
		});
}
