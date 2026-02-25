#include "Agent/HCIAbilityKitAgentLlmClient.h"

#include "Containers/Map.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include <cfloat>

DEFINE_LOG_CATEGORY_STATIC(LogHCIAbilityKitAgentLlmClient, Log, All);

namespace
{
struct FHCIAbilityKitLlmRouterEntry
{
	FString Model;
	double Weight = 0.0;
	double Current = 0.0;
};

struct FHCIAbilityKitLlmRouterState
{
	FString RouterConfigPath;
	FString Strategy = TEXT("smooth_wrr");
	double MinSuccessRate = 0.5;
	FDateTime SourceTimestamp = FDateTime::MinValue();
	TArray<FHCIAbilityKitLlmRouterEntry> Entries;
};

static FCriticalSection GHCIAbilityKitLlmRouterStateMutex;
static TMap<FString, FHCIAbilityKitLlmRouterState> GHCIAbilityKitLlmRouterStates;

static FString HCI_ResolvePathFromProviderConfig(const FString& ProviderConfigPath, const FString& InPath)
{
	FString ResolvedPath = InPath.TrimStartAndEnd();
	if (ResolvedPath.IsEmpty())
	{
		return FString();
	}

	if (FPaths::IsRelative(ResolvedPath))
	{
		const FString ProviderConfigDir = FPaths::GetPath(ProviderConfigPath);
		ResolvedPath = FPaths::ConvertRelativePathToFull(ProviderConfigDir, ResolvedPath);
	}

	return ResolvedPath;
}

static FString HCI_GetRouterConfigPathFromProviderConfig(
	const TSharedPtr<FJsonObject>& ProviderConfigRoot,
	const FString& ProviderConfigPath)
{
	FString RouterConfigPath;
	if (ProviderConfigRoot.IsValid() &&
		ProviderConfigRoot->TryGetStringField(TEXT("router_config_path"), RouterConfigPath) &&
		!RouterConfigPath.TrimStartAndEnd().IsEmpty())
	{
		return HCI_ResolvePathFromProviderConfig(ProviderConfigPath, RouterConfigPath);
	}

	const FString DefaultRouterPath = FPaths::Combine(FPaths::GetPath(ProviderConfigPath), TEXT("llm_router.local.json"));
	return FPaths::ConvertRelativePathToFull(DefaultRouterPath);
}

static bool HCI_LoadRouterStateFromJsonFile(
	const FString& RouterConfigPath,
	const FDateTime& Timestamp,
	FHCIAbilityKitLlmRouterState& OutState,
	FString& OutError)
{
	OutState = FHCIAbilityKitLlmRouterState();
	OutState.RouterConfigPath = RouterConfigPath;
	OutState.SourceTimestamp = Timestamp;
	OutError.Reset();

	FString RouterText;
	if (!FFileHelper::LoadFileToString(RouterText, *RouterConfigPath))
	{
		OutError = TEXT("router_config_file_read_failed");
		return false;
	}

	TSharedPtr<FJsonObject> RouterRoot;
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RouterText);
		if (!FJsonSerializer::Deserialize(Reader, RouterRoot) || !RouterRoot.IsValid())
		{
			OutError = TEXT("router_config_invalid_json");
			return false;
		}
	}

	bool bEnabled = true;
	RouterRoot->TryGetBoolField(TEXT("enabled"), bEnabled);
	if (!bEnabled)
	{
		return true;
	}

	RouterRoot->TryGetStringField(TEXT("strategy"), OutState.Strategy);
	if (OutState.Strategy.TrimStartAndEnd().IsEmpty())
	{
		OutState.Strategy = TEXT("smooth_wrr");
	}
	RouterRoot->TryGetNumberField(TEXT("min_success_rate"), OutState.MinSuccessRate);
	OutState.MinSuccessRate = FMath::Clamp(OutState.MinSuccessRate, 0.0, 1.0);

	const TArray<TSharedPtr<FJsonValue>>* Models = nullptr;
	if (!RouterRoot->TryGetArrayField(TEXT("models"), Models) || Models == nullptr || Models->Num() <= 0)
	{
		return true;
	}

	for (const TSharedPtr<FJsonValue>& Value : *Models)
	{
		const TSharedPtr<FJsonObject> ModelObject = Value.IsValid() ? Value->AsObject() : nullptr;
		if (!ModelObject.IsValid())
		{
			continue;
		}

		FString ModelName;
		if (!ModelObject->TryGetStringField(TEXT("model"), ModelName) || ModelName.TrimStartAndEnd().IsEmpty())
		{
			continue;
		}

		double Weight = 0.0;
		if (!ModelObject->TryGetNumberField(TEXT("weight"), Weight) || Weight <= 0.0)
		{
			continue;
		}

		bool bEligible = true;
		ModelObject->TryGetBoolField(TEXT("eligible"), bEligible);
		if (!bEligible)
		{
			continue;
		}

		double SuccessRate = 1.0;
		if (ModelObject->TryGetNumberField(TEXT("success_rate"), SuccessRate) &&
			SuccessRate < OutState.MinSuccessRate)
		{
			continue;
		}

		FHCIAbilityKitLlmRouterEntry Entry;
		Entry.Model = ModelName.TrimStartAndEnd();
		Entry.Weight = Weight;
		Entry.Current = 0.0;
		OutState.Entries.Add(MoveTemp(Entry));
	}

	return true;
}

static bool HCI_SelectModelFromRouterState(FHCIAbilityKitLlmRouterState& State, FString& OutModel)
{
	OutModel.Reset();
	if (State.Entries.Num() <= 0)
	{
		return false;
	}

	if (State.Strategy.Equals(TEXT("random"), ESearchCase::IgnoreCase) ||
		State.Strategy.Equals(TEXT("weighted_random"), ESearchCase::IgnoreCase))
	{
		double WeightSum = 0.0;
		for (const FHCIAbilityKitLlmRouterEntry& Entry : State.Entries)
		{
			WeightSum += Entry.Weight;
		}
		if (WeightSum <= 0.0)
		{
			return false;
		}

		const double Pick = FMath::FRandRange(0.0, static_cast<float>(WeightSum));
		double Running = 0.0;
		for (const FHCIAbilityKitLlmRouterEntry& Entry : State.Entries)
		{
			Running += Entry.Weight;
			if (Pick <= Running)
			{
				OutModel = Entry.Model;
				return true;
			}
		}

		OutModel = State.Entries.Last().Model;
		return true;
	}

	double TotalWeight = 0.0;
	int32 BestIndex = INDEX_NONE;
	double BestScore = -DBL_MAX;
	for (int32 Index = 0; Index < State.Entries.Num(); ++Index)
	{
		FHCIAbilityKitLlmRouterEntry& Entry = State.Entries[Index];
		Entry.Current += Entry.Weight;
		TotalWeight += Entry.Weight;
		if (Entry.Current > BestScore)
		{
			BestScore = Entry.Current;
			BestIndex = Index;
		}
	}

	if (BestIndex == INDEX_NONE || TotalWeight <= 0.0)
	{
		return false;
	}

	FHCIAbilityKitLlmRouterEntry& Selected = State.Entries[BestIndex];
	Selected.Current -= TotalWeight;
	OutModel = Selected.Model;
	return true;
}

static bool HCI_TrySelectModelByRouter(
	const TSharedPtr<FJsonObject>& ProviderConfigRoot,
	const FString& ProviderConfigPath,
	FString& OutModel,
	FString& OutReason)
{
	OutModel.Reset();
	OutReason.Reset();

	const FString RouterConfigPath = HCI_GetRouterConfigPathFromProviderConfig(ProviderConfigRoot, ProviderConfigPath);
	if (RouterConfigPath.IsEmpty() || !FPaths::FileExists(RouterConfigPath))
	{
		OutReason = TEXT("router_config_missing");
		return false;
	}

	const FDateTime Timestamp = IFileManager::Get().GetTimeStamp(*RouterConfigPath);
	if (Timestamp == FDateTime::MinValue())
	{
		OutReason = TEXT("router_config_timestamp_invalid");
		return false;
	}

	FScopeLock Lock(&GHCIAbilityKitLlmRouterStateMutex);
	FHCIAbilityKitLlmRouterState* State = GHCIAbilityKitLlmRouterStates.Find(RouterConfigPath);
	if (State == nullptr || State->SourceTimestamp != Timestamp)
	{
		FHCIAbilityKitLlmRouterState LoadedState;
		FString LoadError;
		if (!HCI_LoadRouterStateFromJsonFile(RouterConfigPath, Timestamp, LoadedState, LoadError))
		{
			OutReason = LoadError.IsEmpty() ? TEXT("router_config_load_failed") : LoadError;
			GHCIAbilityKitLlmRouterStates.Remove(RouterConfigPath);
			return false;
		}

		GHCIAbilityKitLlmRouterStates.Add(RouterConfigPath, MoveTemp(LoadedState));
		State = GHCIAbilityKitLlmRouterStates.Find(RouterConfigPath);
	}

	if (State == nullptr || State->Entries.Num() <= 0)
	{
		OutReason = TEXT("router_no_eligible_models");
		return false;
	}

	if (!HCI_SelectModelFromRouterState(*State, OutModel) || OutModel.IsEmpty())
	{
		OutReason = TEXT("router_select_failed");
		return false;
	}

	OutReason = FString::Printf(
		TEXT("router_selected strategy=%s min_success_rate=%.2f pool=%d config=%s"),
		*State->Strategy,
		State->MinSuccessRate,
		State->Entries.Num(),
		*RouterConfigPath);
	return true;
}
}

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

	FString RoutedModel;
	FString RouterReason;
	if (HCI_TrySelectModelByRouter(Root, ResolvedConfigPath, RoutedModel, RouterReason))
	{
		OutConfig.Model = RoutedModel;
		UE_LOG(
			LogHCIAbilityKitAgentLlmClient,
			Display,
			TEXT("[HCIAbilityKit][LlmRouter] selected_model=%s reason=%s"),
			*OutConfig.Model,
			*RouterReason);
	}
	else if (!RouterReason.IsEmpty() && !RouterReason.Equals(TEXT("router_config_missing"), ESearchCase::CaseSensitive))
	{
		UE_LOG(
			LogHCIAbilityKitAgentLlmClient,
			Warning,
			TEXT("[HCIAbilityKit][LlmRouter] fallback_to_default_model=%s reason=%s"),
			*OutConfig.Model,
			*RouterReason);
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

void FHCIAbilityKitAgentLlmClient::ResetRoutingStateForTesting()
{
	FScopeLock Lock(&GHCIAbilityKitLlmRouterStateMutex);
	GHCIAbilityKitLlmRouterStates.Reset();
}
