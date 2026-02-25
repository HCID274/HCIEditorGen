#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"

class IHttpRequest;

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentLlmProviderConfig
{
	FString ApiKey;
	FString ApiUrl = TEXT("https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions");
	FString Model = TEXT("qwen3.5-plus");
	bool bEnableThinking = false;
	bool bStream = false;
	int32 HttpTimeoutMs = 12000;
};

class HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentLlmClient
{
public:
	static bool LoadProviderConfigFromJsonFile(
		const FString& ConfigPath,
		const FString& DefaultApiUrl,
		const FString& DefaultModel,
		FHCIAbilityKitAgentLlmProviderConfig& OutConfig,
		FString& OutError);

	static bool BuildChatCompletionsRequestBody(
		const FString& SystemPrompt,
		const FString& UserText,
		const FHCIAbilityKitAgentLlmProviderConfig& Config,
		FString& OutRequestBody,
		FString& OutError);

	static TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> CreateChatCompletionsHttpRequest(
		const FHCIAbilityKitAgentLlmProviderConfig& Config,
		const FString& RequestBody);

	static bool TryExtractMessageContentFromResponse(
		const FString& ResponseBody,
		FString& OutContent,
		FString& OutErrorCode,
		FString& OutError);

	static void ResetRoutingStateForTesting();
};
