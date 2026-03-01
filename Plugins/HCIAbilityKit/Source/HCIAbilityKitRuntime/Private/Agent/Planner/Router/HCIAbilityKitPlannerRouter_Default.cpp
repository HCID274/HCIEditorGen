#include "Agent/Planner/Router/HCIAbilityKitPlannerRouter_Default.h"

#include "Agent/Planner/Interfaces/IHCIAbilityKitPlannerProvider.h"

#include "Misc/ScopeLock.h"

DEFINE_LOG_CATEGORY_STATIC(LogHCIAbilityKitAgentPlanner, Log, All);

namespace
{
static const TCHAR* const HCI_LlmProviderName = TEXT("llm");
static const TCHAR* const HCI_KeywordProviderName = TEXT("keyword_fallback");
static const TCHAR* const HCI_FallbackReasonNone = TEXT("none");
static const TCHAR* const HCI_FallbackReasonCircuitOpen = TEXT("llm_circuit_open");
static const TCHAR* const HCI_FallbackReasonSyncRealHttpDeprecated = TEXT("llm_sync_real_http_deprecated");

static FString HCI_GetProviderModeForPreferredLlm(const FHCIAbilityKitAgentPlannerBuildOptions& Options)
{
	return Options.bUseRealHttpProvider ? TEXT("real_http") : TEXT("mock");
}

static bool HCI_IsCircuitBreakerEnabled(const FHCIAbilityKitAgentPlannerBuildOptions& Options)
{
	return Options.bEnableCircuitBreaker && Options.CircuitBreakerFailureThreshold > 0 && Options.CircuitBreakerOpenForRequests > 0;
}

static bool HCI_IsCircuitBreakerOpen(
	FCriticalSection* RuntimeStateMutex,
	FHCIAbilityKitPlannerRouterRuntimeState* RuntimeState,
	const FHCIAbilityKitAgentPlannerBuildOptions& Options)
{
	if (!HCI_IsCircuitBreakerEnabled(Options))
	{
		return false;
	}

	FScopeLock Lock(RuntimeStateMutex);
	return RuntimeState != nullptr && RuntimeState->CircuitOpenRemainingRequests > 0;
}

class FHCIAbilityKitPlannerProvider_KeywordDirect final : public IHCIAbilityKitPlannerProvider
{
public:
	FHCIAbilityKitPlannerProvider_KeywordDirect(
		const TSharedRef<IHCIAbilityKitPlannerProvider>& InKeywordProvider,
		FCriticalSection* InRuntimeStateMutex,
		FHCIAbilityKitPlannerRouterRuntimeState* InRuntimeState)
		: KeywordProvider(InKeywordProvider)
		, RuntimeStateMutex(InRuntimeStateMutex)
		, RuntimeState(InRuntimeState)
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
			FScopeLock Lock(RuntimeStateMutex);
			OutMetadata.ConsecutiveLlmFailures = RuntimeState ? RuntimeState->ConsecutiveLlmFailures : 0;
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
	FCriticalSection* RuntimeStateMutex = nullptr;
	FHCIAbilityKitPlannerRouterRuntimeState* RuntimeState = nullptr;
};

class FHCIAbilityKitPlannerProvider_CircuitOpenKeywordFallback final : public IHCIAbilityKitPlannerProvider
{
public:
	FHCIAbilityKitPlannerProvider_CircuitOpenKeywordFallback(
		const TSharedRef<IHCIAbilityKitPlannerProvider>& InKeywordProvider,
		FCriticalSection* InRuntimeStateMutex,
		FHCIAbilityKitPlannerRouterRuntimeState* InRuntimeState)
		: KeywordProvider(InKeywordProvider)
		, RuntimeStateMutex(InRuntimeStateMutex)
		, RuntimeState(InRuntimeState)
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
			FScopeLock Lock(RuntimeStateMutex);
			if (RuntimeState && RuntimeState->CircuitOpenRemainingRequests > 0)
			{
				RuntimeState->CircuitOpenRemainingRequests -= 1;
			}
			if (RuntimeState)
			{
				RuntimeState->Metrics.CircuitOpenFallbackRequests += 1;
				RuntimeState->Metrics.KeywordFallbackRequests += 1;
			}
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
			FScopeLock Lock(RuntimeStateMutex);
			OutMetadata.ConsecutiveLlmFailures = RuntimeState ? RuntimeState->ConsecutiveLlmFailures : 0;
			OutMetadata.bCircuitBreakerOpen = (RuntimeState != nullptr && RuntimeState->CircuitOpenRemainingRequests > 0);
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
	FCriticalSection* RuntimeStateMutex = nullptr;
	FHCIAbilityKitPlannerRouterRuntimeState* RuntimeState = nullptr;
};

class FHCIAbilityKitPlannerProvider_LlmFirstWithKeywordFallback final : public IHCIAbilityKitPlannerProvider
{
public:
	FHCIAbilityKitPlannerProvider_LlmFirstWithKeywordFallback(
		const TSharedRef<IHCIAbilityKitPlannerProvider>& InLlmProvider,
		const TSharedRef<IHCIAbilityKitPlannerProvider>& InKeywordProvider,
		FCriticalSection* InRuntimeStateMutex,
		FHCIAbilityKitPlannerRouterRuntimeState* InRuntimeState)
		: LlmProvider(InLlmProvider)
		, KeywordProvider(InKeywordProvider)
		, RuntimeStateMutex(InRuntimeStateMutex)
		, RuntimeState(InRuntimeState)
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
		FString& OutError) override
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
				FScopeLock Lock(RuntimeStateMutex);
				OutMetadata.ConsecutiveLlmFailures = RuntimeState ? RuntimeState->ConsecutiveLlmFailures : 0;
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
			FScopeLock Lock(RuntimeStateMutex);
			if (RuntimeState)
			{
				RuntimeState->Metrics.RetryUsedRequests += 1;
				RuntimeState->Metrics.RetryAttempts += (LlmMetadata.LlmAttemptCount - 1);
			}
		}

		if (bLlmBuilt)
		{
			{
				FScopeLock Lock(RuntimeStateMutex);
				if (RuntimeState)
				{
					RuntimeState->ConsecutiveLlmFailures = 0;
					RuntimeState->Metrics.ConsecutiveLlmFailures = 0;
					RuntimeState->Metrics.LlmSuccessRequests += 1;
				}
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
			const bool bCircuitBreakerEnabled = HCI_IsCircuitBreakerEnabled(Options);

			FScopeLock Lock(RuntimeStateMutex);
			if (RuntimeState)
			{
				RuntimeState->ConsecutiveLlmFailures += 1;
				ConsecutiveFailures = RuntimeState->ConsecutiveLlmFailures;
				RuntimeState->Metrics.ConsecutiveLlmFailures = ConsecutiveFailures;
				if (bCircuitBreakerEnabled && ConsecutiveFailures >= Options.CircuitBreakerFailureThreshold)
				{
					RuntimeState->CircuitOpenRemainingRequests = Options.CircuitBreakerOpenForRequests;
				}
				RuntimeState->Metrics.KeywordFallbackRequests += 1;
				bCircuitBreakerOpen = (RuntimeState->CircuitOpenRemainingRequests > 0);
			}
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

	virtual void BuildPlanAsync(
		const FString& UserText,
		const FString& RequestId,
		const FHCIAbilityKitToolRegistry& ToolRegistry,
		const FHCIAbilityKitAgentPlannerBuildOptions& Options,
		TFunction<void(bool, FHCIAbilityKitAgentPlan, FString, FHCIAbilityKitAgentPlannerResultMetadata, FString)>&& OnComplete) override
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
			[UserText,
			 RequestId,
			 ToolRegistryPtr = &ToolRegistry,
			 Options,
			 ProviderMode,
			 KeywordProviderPinned,
			 RuntimeStateMutex = RuntimeStateMutex,
			 RuntimeState = RuntimeState,
			 OnComplete = MoveTemp(OnComplete)](
				bool bBuilt,
				FHCIAbilityKitAgentPlan Plan,
				FString RouteReason,
				FHCIAbilityKitAgentPlannerResultMetadata LlmMetadata,
				FString Error) mutable
			{
				const bool bRetryUsed = LlmMetadata.LlmAttemptCount > 1;
				if (bRetryUsed)
				{
					FScopeLock Lock(RuntimeStateMutex);
					if (RuntimeState)
					{
						RuntimeState->Metrics.RetryUsedRequests += 1;
						RuntimeState->Metrics.RetryAttempts += (LlmMetadata.LlmAttemptCount - 1);
					}
				}

				if (bBuilt)
				{
					{
						FScopeLock Lock(RuntimeStateMutex);
						if (RuntimeState)
						{
							RuntimeState->ConsecutiveLlmFailures = 0;
							RuntimeState->Metrics.ConsecutiveLlmFailures = 0;
							RuntimeState->Metrics.LlmSuccessRequests += 1;
						}
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
					const bool bCircuitBreakerEnabled = HCI_IsCircuitBreakerEnabled(Options);

					FScopeLock Lock(RuntimeStateMutex);
					if (RuntimeState)
					{
						RuntimeState->ConsecutiveLlmFailures += 1;
						ConsecutiveFailures = RuntimeState->ConsecutiveLlmFailures;
						RuntimeState->Metrics.ConsecutiveLlmFailures = ConsecutiveFailures;
						if (bCircuitBreakerEnabled && ConsecutiveFailures >= Options.CircuitBreakerFailureThreshold)
						{
							RuntimeState->CircuitOpenRemainingRequests = Options.CircuitBreakerOpenForRequests;
						}
						RuntimeState->Metrics.KeywordFallbackRequests += 1;
						bCircuitBreakerOpen = (RuntimeState->CircuitOpenRemainingRequests > 0);
					}
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

private:
	TSharedRef<IHCIAbilityKitPlannerProvider> LlmProvider;
	TSharedRef<IHCIAbilityKitPlannerProvider> KeywordProvider;
	FCriticalSection* RuntimeStateMutex = nullptr;
	FHCIAbilityKitPlannerRouterRuntimeState* RuntimeState = nullptr;
};
} // namespace

FHCIAbilityKitPlannerRouter_Default::FHCIAbilityKitPlannerRouter_Default(
	const TSharedRef<IHCIAbilityKitPlannerProvider>& InLlmProvider,
	const TSharedRef<IHCIAbilityKitPlannerProvider>& InKeywordProvider)
	: LlmProvider(InLlmProvider)
	, KeywordProvider(InKeywordProvider)
{
}

TSharedRef<IHCIAbilityKitPlannerProvider> FHCIAbilityKitPlannerRouter_Default::SelectProvider(
	const FHCIAbilityKitAgentPlannerBuildOptions& Options)
{
	{
		FScopeLock Lock(&RuntimeStateMutex);
		RuntimeState.Metrics.TotalRequests += 1;
		if (Options.bPreferLlm)
		{
			RuntimeState.Metrics.LlmPreferredRequests += 1;
		}
	}

	if (!Options.bPreferLlm)
	{
		return MakeShared<FHCIAbilityKitPlannerProvider_KeywordDirect>(KeywordProvider, &RuntimeStateMutex, &RuntimeState);
	}

	if (HCI_IsCircuitBreakerOpen(&RuntimeStateMutex, &RuntimeState, Options))
	{
		return MakeShared<FHCIAbilityKitPlannerProvider_CircuitOpenKeywordFallback>(KeywordProvider, &RuntimeStateMutex, &RuntimeState);
	}

	return MakeShared<FHCIAbilityKitPlannerProvider_LlmFirstWithKeywordFallback>(LlmProvider, KeywordProvider, &RuntimeStateMutex, &RuntimeState);
}

FHCIAbilityKitAgentPlannerMetricsSnapshot FHCIAbilityKitPlannerRouter_Default::GetMetricsSnapshot() const
{
	FScopeLock Lock(&RuntimeStateMutex);
	return RuntimeState.Metrics;
}

void FHCIAbilityKitPlannerRouter_Default::ResetMetricsForTesting()
{
	FScopeLock Lock(&RuntimeStateMutex);
	RuntimeState = FHCIAbilityKitPlannerRouterRuntimeState();
}

