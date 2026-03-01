#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

struct FHCIAgentPlan;
class FHCIToolRegistry;

enum class EHCIAgentPlannerLlmMockMode : uint8
{
	None,
	Timeout,
	InvalidJson,
	ContractInvalid,
	EmptyResponse,
	ValidButMisroutedScanAssets
};

struct HCIRUNTIME_API FHCIAgentPlannerEnvAssetEntry
{
	FString ObjectPath;
	FString AssetClass;
	int64 SizeBytes = -1;
};

struct HCIRUNTIME_API FHCIAgentPlannerBuildOptions
{
	bool bPreferLlm = false;
	bool bUseRealHttpProvider = false;
	EHCIAgentPlannerLlmMockMode LlmMockMode = EHCIAgentPlannerLlmMockMode::None;
	int32 LlmRetryCount = 0;
	bool bEnableCircuitBreaker = true;
	int32 CircuitBreakerFailureThreshold = 3;
	int32 CircuitBreakerOpenForRequests = 2;
	int32 LlmMockFailAttempts = 0;
	FString LlmApiUrl = TEXT("https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions");
	FString LlmModel = TEXT("qwen3.5-plus");
	FString LlmApiKeyConfigPath = TEXT("Saved/HCIAbilityKit/Config/llm_provider.local.json");
	FString PromptBundleRelativeDir = TEXT("Source/HCIEditorGen/文档/提示词/Skills/H3_AgentPlanner");
	FString PromptTemplateFileName = TEXT("prompt.md");
	FString PromptToolsSchemaFileName = TEXT("tools_schema.json");
	bool bForceDirectoryScanFirst = true;
	bool bEnableAutoEnvContextScan = true;
	FString EnvContextDefaultScanRoot;
	// Optional extra context injected into the prompt ENV_CONTEXT. Intended for structured external signals
	// (e.g. latest ingest batch manifest summary) to reduce user-side "context engineering".
	FString ExtraEnvContextText;
	TFunction<bool(const FString&, TArray<FHCIAgentPlannerEnvAssetEntry>&, FString&)> ScanAssetsForEnvContext;
	int32 LlmHttpTimeoutMs = 12000;
	bool bLlmEnableThinking = false;
	bool bLlmStream = false;
};

struct HCIRUNTIME_API FHCIAgentPlannerResultMetadata
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
	bool bEnvContextInjected = false;
	int32 EnvContextAssetCount = 0;
	FString EnvContextScanRoot;
};

struct HCIRUNTIME_API FHCIAgentPlannerMetricsSnapshot
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

class HCIRUNTIME_API FHCIAgentPlanner
{
public:
	static bool BuildPlanFromNaturalLanguage(
		const FString& UserText,
		const FString& RequestId,
		const FHCIToolRegistry& ToolRegistry,
		FHCIAgentPlan& OutPlan,
		FString& OutRouteReason,
		FString& OutError);

	static bool BuildPlanFromNaturalLanguageWithProvider(
		const FString& UserText,
		const FString& RequestId,
		const FHCIToolRegistry& ToolRegistry,
		const FHCIAgentPlannerBuildOptions& Options,
		FHCIAgentPlan& OutPlan,
		FString& OutRouteReason,
		FHCIAgentPlannerResultMetadata& OutMetadata,
		FString& OutError);

	static void BuildPlanFromNaturalLanguageWithProviderAsync(
		const FString& UserText,
		const FString& RequestId,
		const FHCIToolRegistry& ToolRegistry,
		const FHCIAgentPlannerBuildOptions& Options,
		TFunction<void(bool, FHCIAgentPlan, FString, FHCIAgentPlannerResultMetadata, FString)>&& OnComplete);

	static FHCIAgentPlannerMetricsSnapshot GetMetricsSnapshot();
	static void ResetMetricsForTesting();
};


