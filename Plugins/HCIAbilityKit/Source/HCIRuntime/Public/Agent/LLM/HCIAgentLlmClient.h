#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"

class IHttpRequest;

struct HCIRUNTIME_API FHCIAgentLlmProviderConfig
{
	FString ApiKey;
	FString ApiUrl = TEXT("https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions");
	FString Model = TEXT("qwen3.5-plus");
	bool bEnableThinking = false;
	bool bStream = false;
	int32 HttpTimeoutMs = 12000;
};

class HCIRUNTIME_API FHCIAgentLlmClient
{
public:
	static bool LoadProviderConfigFromJsonFile(
		const FString& ConfigPath,
		const FString& DefaultApiUrl,
		const FString& DefaultModel,
		FHCIAgentLlmProviderConfig& OutConfig,
		FString& OutError);

	static bool BuildChatCompletionsRequestBody(
		const FString& SystemPrompt,
		const FString& UserText,
		const FHCIAgentLlmProviderConfig& Config,
		FString& OutRequestBody,
		FString& OutError);

	static TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> CreateChatCompletionsHttpRequest(
		const FHCIAgentLlmProviderConfig& Config,
		const FString& RequestBody);

	static bool TryExtractMessageContentFromResponse(
		const FString& ResponseBody,
		FString& OutContent,
		FString& OutErrorCode,
		FString& OutError);

	static void ResetRoutingStateForTesting();
};


