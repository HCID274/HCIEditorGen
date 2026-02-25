#include "Commands/HCIAbilityKitAgentCommand_ChatPlanAndSummary.h"

#include "Agent/HCIAbilityKitAgentLlmClient.h"
#include "Agent/HCIAbilityKitAgentPlan.h"
#include "Agent/HCIAbilityKitAgentPlanJsonSerializer.h"
#include "Agent/HCIAbilityKitAgentPlanner.h"
#include "Agent/HCIAbilityKitAgentPromptBuilder.h"
#include "Commands/HCIAbilityKitAgentCommandHandlers.h"
#include "Dom/JsonObject.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
static FHCIAbilityKitAgentPromptBundleOptions HCI_MakeChatSummaryPromptBundleOptions()
{
	FHCIAbilityKitAgentPromptBundleOptions Options;
	Options.SkillBundleRelativeDir = TEXT("Source/HCIEditorGen/文档/提示词/Skills/I7_AgentChatSummary");
	Options.PromptTemplateFileName = TEXT("prompt.md");
	Options.ToolsSchemaFileName = TEXT("tools_schema.json");
	return Options;
}

static FString HCI_SerializePlanContextForSummary(
	const FString& UserText,
	const FHCIAbilityKitAgentPlan& Plan,
	const FString& RouteReason,
	const FHCIAbilityKitAgentPlannerResultMetadata& PlannerMetadata)
{
	FString PlanJson;
	if (!FHCIAbilityKitAgentPlanJsonSerializer::SerializeToJsonString(Plan, PlanJson))
	{
		PlanJson = TEXT("{}");
	}

	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("user_text"), UserText);
	Root->SetStringField(TEXT("route_reason"), RouteReason);
	Root->SetStringField(TEXT("planner_provider"), PlannerMetadata.PlannerProvider);
	Root->SetStringField(TEXT("provider_mode"), PlannerMetadata.ProviderMode);
	Root->SetBoolField(TEXT("fallback_used"), PlannerMetadata.bFallbackUsed);
	Root->SetStringField(TEXT("fallback_reason"), PlannerMetadata.FallbackReason);
	Root->SetStringField(TEXT("error_code"), PlannerMetadata.ErrorCode);
	Root->SetStringField(TEXT("plan_json"), PlanJson);

	FString Out;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
	FJsonSerializer::Serialize(Root, Writer);
	return Out;
}

static bool HCI_TryFormatSummaryTextFromLlmOutput(const FString& RawOutput, FString& OutSummary)
{
	OutSummary.Reset();
	const FString Trimmed = RawOutput.TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Trimmed);
	if (FJsonSerializer::Deserialize(Reader, RootObject) && RootObject.IsValid())
	{
		FString Summary;
		FString RiskNote;
		FString NextAction;
		RootObject->TryGetStringField(TEXT("summary"), Summary);
		RootObject->TryGetStringField(TEXT("risk_note"), RiskNote);
		RootObject->TryGetStringField(TEXT("next_action"), NextAction);

		Summary.TrimStartAndEndInline();
		RiskNote.TrimStartAndEndInline();
		NextAction.TrimStartAndEndInline();
		if (!Summary.IsEmpty())
		{
			OutSummary = FString::Printf(TEXT("摘要：%s"), *Summary);
			if (!RiskNote.IsEmpty())
			{
				OutSummary += FString::Printf(TEXT(" | 风险：%s"), *RiskNote);
			}
			if (!NextAction.IsEmpty())
			{
				OutSummary += FString::Printf(TEXT(" | 建议：%s"), *NextAction);
			}
			return true;
		}
	}

	OutSummary = Trimmed;
	return true;
}

static void HCI_RequestChatSummaryFromPromptAsync(
	const FString& UserText,
	const FHCIAbilityKitAgentPlan& Plan,
	const FString& RouteReason,
	const FHCIAbilityKitAgentPlannerResultMetadata& PlannerMetadata,
	TFunction<void(bool, const FString&)>&& OnComplete)
{
	auto Complete = [OnComplete = MoveTemp(OnComplete)](const bool bOk, const FString& Message) mutable
	{
		if (OnComplete)
		{
			OnComplete(bOk, Message);
		}
	};

	const FString ContextJson = HCI_SerializePlanContextForSummary(UserText, Plan, RouteReason, PlannerMetadata);

	const FHCIAbilityKitAgentPromptBundleOptions BundleOptions = HCI_MakeChatSummaryPromptBundleOptions();
	FString SystemPrompt;
	FString PromptError;
	if (!FHCIAbilityKitAgentPromptBuilder::BuildSystemPromptFromBundleWithEnvContext(
		ContextJson,
		TEXT("source=AgentChatUI"),
		BundleOptions,
		SystemPrompt,
		PromptError))
	{
		Complete(false, FString::Printf(TEXT("summary_prompt_build_failed:%s"), *PromptError));
		return;
	}

	FHCIAbilityKitAgentLlmProviderConfig ProviderConfig;
	FString ConfigError;
	if (!FHCIAbilityKitAgentLlmClient::LoadProviderConfigFromJsonFile(
		TEXT("Saved/HCIAbilityKit/Config/llm_provider.local.json"),
		TEXT("https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions"),
		TEXT("qwen3.5-plus"),
		ProviderConfig,
		ConfigError))
	{
		Complete(false, FString::Printf(TEXT("summary_config_error:%s"), *ConfigError));
		return;
	}

	FString RequestBody;
	FString RequestBodyError;
	if (!FHCIAbilityKitAgentLlmClient::BuildChatCompletionsRequestBody(
		SystemPrompt,
		ContextJson,
		ProviderConfig,
		RequestBody,
		RequestBodyError))
	{
		Complete(false, FString::Printf(TEXT("summary_request_body_error:%s"), *RequestBodyError));
		return;
	}

	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request =
		FHCIAbilityKitAgentLlmClient::CreateChatCompletionsHttpRequest(ProviderConfig, RequestBody);
	if (!Request.IsValid())
	{
		Complete(false, TEXT("summary_create_request_failed"));
		return;
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("[HCIAbilityKit][AgentChatUI][Summary] dispatched model=%s bundle=%s"),
		*ProviderConfig.Model,
		*BundleOptions.SkillBundleRelativeDir);

	Request->OnProcessRequestComplete().BindLambda(
		[Complete = MoveTemp(Complete)](
			const FHttpRequestPtr,
			const FHttpResponsePtr HttpResponse,
			const bool bRequestSuccess) mutable
		{
			if (!bRequestSuccess || !HttpResponse.IsValid())
			{
				Complete(false, TEXT("summary_http_failed"));
				return;
			}

			if (HttpResponse->GetResponseCode() < 200 || HttpResponse->GetResponseCode() >= 300)
			{
				UE_LOG(
					LogTemp,
					Warning,
					TEXT("[HCIAbilityKit][AgentChatUI][Summary] done success=false status=%d"),
					HttpResponse->GetResponseCode());
				Complete(false, FString::Printf(TEXT("summary_http_status_%d"), HttpResponse->GetResponseCode()));
				return;
			}

			FString Content;
			FString LlmErrorCode;
			FString LlmError;
			if (!FHCIAbilityKitAgentLlmClient::TryExtractMessageContentFromResponse(
				HttpResponse->GetContentAsString(),
				Content,
				LlmErrorCode,
				LlmError))
			{
				UE_LOG(
					LogTemp,
					Warning,
					TEXT("[HCIAbilityKit][AgentChatUI][Summary] done success=false reason=extract_failed error_code=%s"),
					*LlmErrorCode);
				Complete(false, FString::Printf(TEXT("summary_extract_failed:%s:%s"), *LlmErrorCode, *LlmError));
				return;
			}

			FString SummaryText;
			if (!HCI_TryFormatSummaryTextFromLlmOutput(Content, SummaryText))
			{
				UE_LOG(
					LogTemp,
					Warning,
					TEXT("[HCIAbilityKit][AgentChatUI][Summary] done success=false reason=output_empty"));
				Complete(false, TEXT("summary_output_empty"));
				return;
			}

			UE_LOG(LogTemp, Display, TEXT("[HCIAbilityKit][AgentChatUI][Summary] done success=true status=200"));
			Complete(true, SummaryText);
		});

	if (!Request->ProcessRequest())
	{
		Complete(false, TEXT("summary_process_request_failed"));
	}
}
}

void UHCIAbilityKitAgentCommand_ChatPlanAndSummary::ExecuteAsync(
	const FHCIAbilityKitAgentCommandContext& Context,
	FHCIAbilityKitAgentCommandComplete&& OnComplete)
{
	const FString UserText = Context.InputParam.TrimStartAndEnd();
	const FString SourceTag = Context.SourceTag.IsEmpty() ? TEXT("AgentChatUI") : Context.SourceTag;

	auto Complete = [OnComplete = MoveTemp(OnComplete)](
		const bool bSuccess,
		const bool bSummaryFailure,
		const FString& Message) mutable
	{
		if (!OnComplete.IsBound())
		{
			return;
		}

		FHCIAbilityKitAgentCommandResult Result;
		Result.bSuccess = bSuccess;
		Result.bSummaryFailure = bSummaryFailure;
		Result.Message = Message;
		OnComplete.Execute(Result);
	};

	const bool bAccepted = HCI_RequestAgentPlanPreviewFromUi(
		UserText,
		SourceTag,
		[Complete](const bool bSuccess,
			const FString& RequestText,
			const FHCIAbilityKitAgentPlan& Plan,
			const FString& RouteReason,
			const FHCIAbilityKitAgentPlannerResultMetadata& PlannerMetadata,
			const FString& Error) mutable
		{
			if (!bSuccess)
			{
				Complete(
					false,
					false,
					FString::Printf(
						TEXT("规划失败：input=%s reason=%s provider=%s fallback=%s error_code=%s"),
						RequestText.IsEmpty() ? TEXT("-") : *RequestText,
						Error.IsEmpty() ? TEXT("-") : *Error,
						PlannerMetadata.PlannerProvider.IsEmpty() ? TEXT("-") : *PlannerMetadata.PlannerProvider,
						PlannerMetadata.FallbackReason.IsEmpty() ? TEXT("-") : *PlannerMetadata.FallbackReason,
						PlannerMetadata.ErrorCode.IsEmpty() ? TEXT("-") : *PlannerMetadata.ErrorCode));
				return;
			}

			HCI_RequestChatSummaryFromPromptAsync(
				RequestText,
				Plan,
				RouteReason,
				PlannerMetadata,
				[Complete](const bool bSummaryOk, const FString& SummaryMessage) mutable
				{
					if (bSummaryOk)
					{
						Complete(true, false, SummaryMessage);
						return;
					}

					Complete(false, true, SummaryMessage);
				});
		});

	if (!bAccepted)
	{
		Complete(false, false, TEXT("已有请求执行中，请等待当前请求完成后再发送。"));
	}
}
