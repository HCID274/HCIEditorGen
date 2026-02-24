#pragma once

#include "CoreMinimal.h"

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
	EHCIAbilityKitAgentPlannerLlmMockMode LlmMockMode = EHCIAbilityKitAgentPlannerLlmMockMode::None;
	int32 LlmRetryCount = 0;
	bool bEnableCircuitBreaker = true;
	int32 CircuitBreakerFailureThreshold = 3;
	int32 CircuitBreakerOpenForRequests = 2;
	int32 LlmMockFailAttempts = 0;
};

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentPlannerResultMetadata
{
	FString PlannerProvider = TEXT("keyword_fallback");
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

	static FHCIAbilityKitAgentPlannerMetricsSnapshot GetMetricsSnapshot();
	static void ResetMetricsForTesting();
};
