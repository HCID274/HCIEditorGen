#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

struct FHCIAbilityKitAgentPlan;
class FHCIAbilityKitToolRegistry;

enum class EHCIAbilityKitAgentPlannerLlmMockMode : uint8
{
	None,
	Timeout,
	InvalidJson,
	ContractInvalid,
	EmptyResponse
};

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentPlannerBuildOptions
{
	bool bPreferLlm = false;
	bool bUseRealHttpProvider = false;
	EHCIAbilityKitAgentPlannerLlmMockMode LlmMockMode = EHCIAbilityKitAgentPlannerLlmMockMode::None;
	int32 LlmRetryCount = 0;
	bool bEnableCircuitBreaker = true;
	int32 CircuitBreakerFailureThreshold = 3;
	int32 CircuitBreakerOpenForRequests = 2;
	int32 LlmMockFailAttempts = 0;
	FString LlmApiUrl = TEXT("https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions");
	FString LlmModel = TEXT("qwen3.5-plus");
	FString LlmApiKeyConfigPath = TEXT("Saved/HCIAbilityKit/Config/llm_provider.local.json");
	int32 LlmHttpTimeoutMs = 12000;
	bool bLlmEnableThinking = false;
	bool bLlmStream = false;
};

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentPlannerResultMetadata
{
	FString PlannerProvider = TEXT("keyword_fallback");
	FString ProviderMode = TEXT("keyword");
	bool bFallbackUsed = false;
	FString FallbackReason = TEXT("none");
	FString ErrorCode = TEXT("-");
	int32 LlmAttemptCount = 0;
	bool bRetryUsed = false;
	bool bCircuitBreakerOpen = false;
	int32 ConsecutiveLlmFailures = 0;
};

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentPlannerMetricsSnapshot
{
	int32 TotalRequests = 0;
	int32 LlmPreferredRequests = 0;
	int32 LlmSuccessRequests = 0;
	int32 KeywordFallbackRequests = 0;
	int32 RetryUsedRequests = 0;
	int32 RetryAttempts = 0;
	int32 CircuitOpenFallbackRequests = 0;
	int32 ConsecutiveLlmFailures = 0;
};

class HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentPlanner
{
public:
	static bool BuildPlanFromNaturalLanguage(
		const FString& UserText,
		const FString& RequestId,
		const FHCIAbilityKitToolRegistry& ToolRegistry,
		FHCIAbilityKitAgentPlan& OutPlan,
		FString& OutRouteReason,
		FString& OutError);

	static bool BuildPlanFromNaturalLanguageWithProvider(
		const FString& UserText,
		const FString& RequestId,
		const FHCIAbilityKitToolRegistry& ToolRegistry,
		const FHCIAbilityKitAgentPlannerBuildOptions& Options,
		FHCIAbilityKitAgentPlan& OutPlan,
		FString& OutRouteReason,
		FHCIAbilityKitAgentPlannerResultMetadata& OutMetadata,
		FString& OutError);

	static void BuildPlanFromNaturalLanguageWithProviderAsync(
		const FString& UserText,
		const FString& RequestId,
		const FHCIAbilityKitToolRegistry& ToolRegistry,
		const FHCIAbilityKitAgentPlannerBuildOptions& Options,
		TFunction<void(bool, FHCIAbilityKitAgentPlan, FString, FHCIAbilityKitAgentPlannerResultMetadata, FString)>&& OnComplete);

	static FHCIAbilityKitAgentPlannerMetricsSnapshot GetMetricsSnapshot();
	static void ResetMetricsForTesting();
};
