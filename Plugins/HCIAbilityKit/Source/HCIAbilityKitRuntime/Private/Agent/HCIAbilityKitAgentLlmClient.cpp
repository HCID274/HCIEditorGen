#include "Agent/HCIAbilityKitAgentLlmClient.h"

#include "Dom/JsonObject.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

bool FHCIAbilityKitAgentLlmClient::LoadProviderConfigFromJsonFile(
	const FString& ConfigPath,
	const FString& DefaultApiUrl,
	const FString& DefaultModel,
	FHCIAbilityKitAgentLlmProviderConfig& OutConfig,
	FString& OutError)
{
	OutConfig = FHCIAbilityKitAgentLlmProviderConfig();
	OutConfig.ApiUrl = DefaultApiUrl;
	OutConfig.Model = DefaultModel;
	OutError.Reset();

	FString ResolvedConfigPath = ConfigPath;
	if (ResolvedConfigPath.TrimStartAndEnd().IsEmpty())
	{
		OutError = TEXT("llm_config_path_empty");
		return false;
	}

	if (FPaths::IsRelative(ResolvedConfigPath))
	{
		ResolvedConfigPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), ResolvedConfigPath);
	}

	if (!FPaths::FileExists(ResolvedConfigPath))
	{
		OutError = FString::Printf(TEXT("llm_config_file_not_found path=%s"), *ResolvedConfigPath);
		return false;
	}

	FString ConfigText;
	if (!FFileHelper::LoadFileToString(ConfigText, *ResolvedConfigPath))
	{
		OutError = FString::Printf(TEXT("llm_config_file_read_failed path=%s"), *ResolvedConfigPath);
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ConfigText);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			OutError = TEXT("llm_config_invalid_json");
			return false;
		}
	}

	if (!Root->TryGetStringField(TEXT("api_key"), OutConfig.ApiKey) || OutConfig.ApiKey.TrimStartAndEnd().IsEmpty())
	{
		OutError = TEXT("llm_config_missing_api_key");
		return false;
	}

	FString OverrideApiUrl;
	if (Root->TryGetStringField(TEXT("api_url"), OverrideApiUrl) && !OverrideApiUrl.TrimStartAndEnd().IsEmpty())
	{
		OutConfig.ApiUrl = OverrideApiUrl;
	}

	FString OverrideModel;
	if (Root->TryGetStringField(TEXT("model"), OverrideModel) && !OverrideModel.TrimStartAndEnd().IsEmpty())
	{
		OutConfig.Model = OverrideModel;
	}

	return true;
}

bool FHCIAbilityKitAgentLlmClient::BuildChatCompletionsRequestBody(
	const FString& SystemPrompt,
	const FString& UserText,
	const FHCIAbilityKitAgentLlmProviderConfig& Config,
	FString& OutRequestBody,
	FString& OutError)
{
	OutRequestBody.Reset();
	OutError.Reset();

	if (Config.ApiUrl.TrimStartAndEnd().IsEmpty())
	{
		OutError = TEXT("llm_api_url_empty");
		return false;
	}
	if (Config.Model.TrimStartAndEnd().IsEmpty())
	{
		OutError = TEXT("llm_model_empty");
		return false;
	}
	if (UserText.TrimStartAndEnd().IsEmpty())
	{
		OutError = TEXT("llm_user_text_empty");
		return false;
	}

	TSharedPtr<FJsonObject> RequestObject = MakeShared<FJsonObject>();
	RequestObject->SetStringField(TEXT("model"), Config.Model);
	RequestObject->SetBoolField(TEXT("stream"), Config.bStream);
	RequestObject->SetBoolField(TEXT("enable_thinking"), Config.bEnableThinking);

	TArray<TSharedPtr<FJsonValue>> Messages;
	if (!SystemPrompt.TrimStartAndEnd().IsEmpty())
	{
		TSharedPtr<FJsonObject> SystemMessage = MakeShared<FJsonObject>();
		SystemMessage->SetStringField(TEXT("role"), TEXT("system"));
		SystemMessage->SetStringField(TEXT("content"), SystemPrompt);
		Messages.Add(MakeShared<FJsonValueObject>(SystemMessage));
	}

	TSharedPtr<FJsonObject> UserMessage = MakeShared<FJsonObject>();
	UserMessage->SetStringField(TEXT("role"), TEXT("user"));
	UserMessage->SetStringField(TEXT("content"), UserText);
	Messages.Add(MakeShared<FJsonValueObject>(UserMessage));
	RequestObject->SetArrayField(TEXT("messages"), Messages);

	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutRequestBody);
	if (!FJsonSerializer::Serialize(RequestObject.ToSharedRef(), Writer))
	{
		OutError = TEXT("llm_request_body_serialize_failed");
		return false;
	}

	return true;
}

TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> FHCIAbilityKitAgentLlmClient::CreateChatCompletionsHttpRequest(
	const FHCIAbilityKitAgentLlmProviderConfig& Config,
	const FString& RequestBody)
{
	if (Config.ApiKey.TrimStartAndEnd().IsEmpty() ||
		Config.ApiUrl.TrimStartAndEnd().IsEmpty() ||
		RequestBody.TrimStartAndEnd().IsEmpty())
	{
		return nullptr;
	}

	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Config.ApiUrl);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *Config.ApiKey));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetContentAsString(RequestBody);
	return Request;
}

bool FHCIAbilityKitAgentLlmClient::TryExtractMessageContentFromResponse(
	const FString& ResponseBody,
	FString& OutContent,
	FString& OutErrorCode,
	FString& OutError)
{
	OutContent.Reset();
	OutErrorCode = TEXT("-");
	OutError.Reset();

	TSharedPtr<FJsonObject> ResponseObject;
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
		if (!FJsonSerializer::Deserialize(Reader, ResponseObject) || !ResponseObject.IsValid())
		{
			OutErrorCode = TEXT("E4302");
			OutError = TEXT("llm_response_invalid_json");
			return false;
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* Choices = nullptr;
	if (!ResponseObject->TryGetArrayField(TEXT("choices"), Choices) || Choices == nullptr || Choices->Num() <= 0)
	{
		OutErrorCode = TEXT("E4303");
		OutError = TEXT("llm_response_missing_choices");
		return false;
	}

	const TSharedPtr<FJsonObject> ChoiceObject = (*Choices)[0].IsValid() ? (*Choices)[0]->AsObject() : nullptr;
	TSharedPtr<FJsonObject> MessageObject;
	if (ChoiceObject.IsValid())
	{
		const TSharedPtr<FJsonObject>* MessageObjectPtr = nullptr;
		if (ChoiceObject->TryGetObjectField(TEXT("message"), MessageObjectPtr) && MessageObjectPtr != nullptr && MessageObjectPtr->IsValid())
		{
			MessageObject = *MessageObjectPtr;
		}
	}
	if (!MessageObject.IsValid())
	{
		OutErrorCode = TEXT("E4303");
		OutError = TEXT("llm_response_missing_message");
		return false;
	}

	if (!MessageObject->TryGetStringField(TEXT("content"), OutContent))
	{
		OutErrorCode = TEXT("E4303");
		OutError = TEXT("llm_response_missing_content");
		return false;
	}
	if (OutContent.TrimStartAndEnd().IsEmpty())
	{
		OutErrorCode = TEXT("E4304");
		OutError = TEXT("llm_empty_response");
		return false;
	}

	return true;
}
