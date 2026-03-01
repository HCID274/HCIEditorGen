#include "Commands/HCIAgentCommand_ChatPlanAndSummary.h"

#include "Agent/LLM/HCIAgentLlmClient.h"
#include "Agent/Planner/HCIAgentPlan.h"
#include "Agent/Planner/HCIAgentPlanJsonSerializer.h"
#include "Agent/Planner/HCIAgentPlanner.h"
#include "Agent/LLM/HCIAgentPromptBuilder.h"
#include "Commands/HCIAgentCommandHandlers.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/ScopeLock.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
static FHCIAgentPromptBundleOptions HCI_MakeChatSummaryPromptBundleOptions()
{
	FHCIAgentPromptBundleOptions Options;
	Options.SkillBundleRelativeDir = TEXT("Source/HCIEditorGen/文档/提示词/Skills/I7_AgentChatSummary");
	Options.PromptTemplateFileName = TEXT("prompt.md");
	Options.ToolsSchemaFileName = TEXT("tools_schema.json");
	return Options;
}

static FString HCI_SerializePlanContextForSummary(
	const FString& UserText,
	const FHCIAgentPlan& Plan,
	const FString& RouteReason,
	const FHCIAgentPlannerResultMetadata& PlannerMetadata)
{
	FString PlanJson;
	if (!FHCIAgentPlanJsonSerializer::SerializeToJsonString(Plan, PlanJson))
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

static bool HCI_ChatSummary_IsWriteLikeRisk(const EHCIAgentPlanRiskLevel RiskLevel)
{
	return RiskLevel == EHCIAgentPlanRiskLevel::Write ||
		RiskLevel == EHCIAgentPlanRiskLevel::Destructive;
}

static FString HCI_BuildLocalSummaryFallbackText(
	const FString& UserText,
	const FHCIAgentPlan& Plan,
	const FString& RouteReason,
	const FHCIAgentPlannerResultMetadata& PlannerMetadata,
	const FString& SummaryFailureReason)
{
	int32 WriteLikeSteps = 0;
	for (const FHCIAgentPlanStep& Step : Plan.Steps)
	{
		if (Step.bRequiresConfirm || HCI_ChatSummary_IsWriteLikeRisk(Step.RiskLevel))
		{
			++WriteLikeSteps;
		}
	}

	const bool bHasWrite = WriteLikeSteps > 0;
	const FString ProviderLabel = PlannerMetadata.PlannerProvider.IsEmpty() ? TEXT("-") : PlannerMetadata.PlannerProvider;
	const FString ReasonLabel = SummaryFailureReason.IsEmpty() ? TEXT("summary_failed") : SummaryFailureReason;
	const FString RouteLabel = RouteReason.IsEmpty() ? TEXT("-") : RouteReason;
	const FString UserLabel = UserText.TrimStartAndEnd().IsEmpty() ? TEXT("（未提供）") : UserText.TrimStartAndEnd();
	const FString PlanActionLabel = bHasWrite
		? TEXT("我已生成含写操作的执行计划，等待你在聊天内确认执行。")
		: TEXT("我已生成只读计划并将自动执行以更新证据链。");

	return FString::Printf(
		TEXT("摘要（本地降级）：已收到你的请求“%s”。%s 当前计划共 %d 步（写步骤 %d）。route=%s，planner=%s。摘要服务暂不可用（%s），已切换本地模板输出。"),
		*UserLabel,
		*PlanActionLabel,
		Plan.Steps.Num(),
		WriteLikeSteps,
		*RouteLabel,
		*ProviderLabel,
		*ReasonLabel);
}

static FString HCI_ComposeChatReplyWithAssistantLeadIn(
	const FHCIAgentPlan& Plan,
	const FString& MainMessage)
{
	const FString LeadIn = Plan.AssistantMessage.TrimStartAndEnd();
	const FString Body = MainMessage.TrimStartAndEnd();
	if (LeadIn.IsEmpty())
	{
		return MainMessage;
	}
	if (Body.IsEmpty())
	{
		return LeadIn;
	}
	return FString::Printf(TEXT("%s\n%s"), *LeadIn, *Body);
}

static void HCI_RequestChatSummaryFromPromptAsync(
	const FString& UserText,
	const FHCIAgentPlan& Plan,
	const FString& RouteReason,
	const FHCIAgentPlannerResultMetadata& PlannerMetadata,
	TFunction<void(bool, const FString&)>&& OnComplete)
{
	auto Complete = [OnComplete = MoveTemp(OnComplete)](const bool bOk, const FString& Message) mutable
	{
		if (OnComplete)
		{
			OnComplete(bOk, Message);
		}
	};
	struct FHCIChatSummaryRaceState
	{
		FCriticalSection Mutex;
		bool bCompleted = false;
		TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request;
	};
	const TSharedRef<FHCIChatSummaryRaceState, ESPMode::ThreadSafe> RaceState = MakeShared<FHCIChatSummaryRaceState, ESPMode::ThreadSafe>();
	const TSharedRef<TFunction<void(bool, const FString&)>, ESPMode::ThreadSafe> CompleteRef =
		MakeShared<TFunction<void(bool, const FString&)>, ESPMode::ThreadSafe>(MoveTemp(Complete));
	auto CompleteOnce = [RaceState, CompleteRef](const bool bOk, const FString& Message) mutable
	{
		{
			FScopeLock Lock(&RaceState->Mutex);
			if (RaceState->bCompleted)
			{
				return;
			}
			RaceState->bCompleted = true;
		}

		if (*CompleteRef)
		{
			(*CompleteRef)(bOk, Message);
		}
	};

	const FString ContextJson = HCI_SerializePlanContextForSummary(UserText, Plan, RouteReason, PlannerMetadata);

	const FHCIAgentPromptBundleOptions BundleOptions = HCI_MakeChatSummaryPromptBundleOptions();
	FString SystemPrompt;
	FString PromptError;
	if (!FHCIAgentPromptBuilder::BuildSystemPromptFromBundleWithEnvContext(
		ContextJson,
		TEXT("source=AgentChatUI"),
		BundleOptions,
		SystemPrompt,
		PromptError))
	{
		Complete(false, FString::Printf(TEXT("summary_prompt_build_failed:%s"), *PromptError));
		return;
	}

	FHCIAgentLlmProviderConfig ProviderConfig;
	FString ConfigError;
	if (!FHCIAgentLlmClient::LoadProviderConfigFromJsonFile(
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
	if (!FHCIAgentLlmClient::BuildChatCompletionsRequestBody(
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
		FHCIAgentLlmClient::CreateChatCompletionsHttpRequest(ProviderConfig, RequestBody);
	if (!Request.IsValid())
	{
		Complete(false, TEXT("summary_create_request_failed"));
		return;
	}
	RaceState->Request = Request;

	UE_LOG(
		LogTemp,
		Display,
		TEXT("[HCI][AgentChatUI][Summary] dispatched model=%s bundle=%s"),
		*ProviderConfig.Model,
		*BundleOptions.SkillBundleRelativeDir);

	Request->OnProcessRequestComplete().BindLambda(
		[CompleteOnce](
			const FHttpRequestPtr,
			const FHttpResponsePtr HttpResponse,
			const bool bRequestSuccess) mutable
		{
			if (!bRequestSuccess || !HttpResponse.IsValid())
			{
				CompleteOnce(false, TEXT("summary_http_failed"));
				return;
			}

			if (HttpResponse->GetResponseCode() < 200 || HttpResponse->GetResponseCode() >= 300)
			{
				UE_LOG(
					LogTemp,
					Warning,
					TEXT("[HCI][AgentChatUI][Summary] done success=false status=%d"),
					HttpResponse->GetResponseCode());
				CompleteOnce(false, FString::Printf(TEXT("summary_http_status_%d"), HttpResponse->GetResponseCode()));
				return;
			}

			FString Content;
			FString LlmErrorCode;
			FString LlmError;
			if (!FHCIAgentLlmClient::TryExtractMessageContentFromResponse(
				HttpResponse->GetContentAsString(),
				Content,
				LlmErrorCode,
				LlmError))
			{
				UE_LOG(
					LogTemp,
					Warning,
					TEXT("[HCI][AgentChatUI][Summary] done success=false reason=extract_failed error_code=%s"),
					*LlmErrorCode);
				CompleteOnce(false, FString::Printf(TEXT("summary_extract_failed:%s:%s"), *LlmErrorCode, *LlmError));
				return;
			}

			FString SummaryText;
			if (!HCI_TryFormatSummaryTextFromLlmOutput(Content, SummaryText))
			{
				UE_LOG(
					LogTemp,
					Warning,
					TEXT("[HCI][AgentChatUI][Summary] done success=false reason=output_empty"));
				CompleteOnce(false, TEXT("summary_output_empty"));
				return;
			}

			UE_LOG(LogTemp, Display, TEXT("[HCI][AgentChatUI][Summary] done success=true status=200"));
			CompleteOnce(true, SummaryText);
		});

	constexpr float SummaryTimeoutSeconds = 3.0f;
	FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([RaceState, CompleteOnce](float) mutable
		{
			bool bShouldTimeout = false;
			{
				FScopeLock Lock(&RaceState->Mutex);
				bShouldTimeout = !RaceState->bCompleted;
			}
			if (!bShouldTimeout)
			{
				return false;
			}

			if (RaceState->Request.IsValid())
			{
				RaceState->Request->CancelRequest();
			}
			UE_LOG(LogTemp, Warning, TEXT("[HCI][AgentChatUI][Summary] timeout seconds=3.0"));
			CompleteOnce(false, TEXT("summary_timeout"));
			return false;
		}),
		SummaryTimeoutSeconds);

	if (!Request->ProcessRequest())
	{
		CompleteOnce(false, TEXT("summary_process_request_failed"));
	}
}
}

void UHCIAgentCommand_ChatPlanAndSummary::ExecuteAsync(
	const FHCIAgentCommandContext& Context,
	FHCIAgentCommandComplete&& OnComplete)
{
	const FString UserText = Context.InputParam.TrimStartAndEnd();
	const FString SourceTag = Context.SourceTag.IsEmpty() ? TEXT("AgentChatUI") : Context.SourceTag;

	auto Complete = [OnComplete = MoveTemp(OnComplete)](FHCIAgentCommandResult&& Result) mutable
	{
		if (!OnComplete.IsBound())
		{
			return;
		}

		OnComplete.Execute(Result);
	};

	const bool bAccepted = HCI_RequestAgentPlanPreviewFromUi(
		UserText,
		SourceTag,
		Context.ExtraEnvContextText,
		false,
		[Complete](const bool bSuccess,
			const FString& RequestText,
			const FHCIAgentPlan& Plan,
			const FString& RouteReason,
			const FHCIAgentPlannerResultMetadata& PlannerMetadata,
			const FString& Error) mutable
		{
			if (!bSuccess)
			{
				FHCIAgentCommandResult Result;
				Result.bSuccess = false;
				Result.bSummaryFailure = false;
				Result.Message = FString::Printf(
					TEXT("规划失败：input=%s reason=%s provider=%s fallback=%s error_code=%s"),
					RequestText.IsEmpty() ? TEXT("-") : *RequestText,
					Error.IsEmpty() ? TEXT("-") : *Error,
					PlannerMetadata.PlannerProvider.IsEmpty() ? TEXT("-") : *PlannerMetadata.PlannerProvider,
					PlannerMetadata.FallbackReason.IsEmpty() ? TEXT("-") : *PlannerMetadata.FallbackReason,
					PlannerMetadata.ErrorCode.IsEmpty() ? TEXT("-") : *PlannerMetadata.ErrorCode);
				Complete(MoveTemp(Result));
				return;
			}

			const FString AssistantMessage = Plan.AssistantMessage.TrimStartAndEnd();
			const bool bMessageOnlyPlan = Plan.Steps.Num() <= 0 && !AssistantMessage.IsEmpty();
			if (bMessageOnlyPlan)
			{
				FHCIAgentCommandResult Result;
				Result.bSuccess = true;
				Result.bSummaryFailure = false;
				Result.bHasPlanPayload = false;
				Result.Plan = Plan;
				Result.RouteReason = RouteReason;
				Result.PlannerMetadata = PlannerMetadata;
				Result.Message = AssistantMessage;
				Complete(MoveTemp(Result));
				return;
			}

			FHCIAgentCommandResult BaseResult;
			BaseResult.bHasPlanPayload = Plan.Steps.Num() > 0;
			BaseResult.Plan = Plan;
			BaseResult.RouteReason = RouteReason;
			BaseResult.PlannerMetadata = PlannerMetadata;

			// For executable plans, do NOT block command completion on the optional LLM summary.
			// Chat UI will surface deterministic execution/preview results instead.
			if (BaseResult.bHasPlanPayload)
			{
				FHCIAgentCommandResult Result = BaseResult;
				Result.bSuccess = true;
				Result.bSummaryFailure = false;
				Result.Message = FString();
				Complete(MoveTemp(Result));
				return;
			}

			HCI_RequestChatSummaryFromPromptAsync(
				RequestText,
				Plan,
				RouteReason,
				PlannerMetadata,
				[Complete, BaseResult = MoveTemp(BaseResult), RequestText](const bool bSummaryOk, const FString& SummaryMessage) mutable
				{
					FHCIAgentCommandResult Result = BaseResult;
					if (bSummaryOk)
					{
						Result.bSuccess = true;
						Result.bSummaryFailure = false;
						Result.Message = HCI_ComposeChatReplyWithAssistantLeadIn(Result.Plan, SummaryMessage);
						Complete(MoveTemp(Result));
						return;
					}

					UE_LOG(
						LogTemp,
						Warning,
						TEXT("[HCI][AgentChatUI][Summary] fallback reason=%s"),
						*SummaryMessage);
					Result.bSuccess = true;
					Result.bSummaryFailure = false;
					Result.Message = HCI_ComposeChatReplyWithAssistantLeadIn(Result.Plan, HCI_BuildLocalSummaryFallbackText(
						RequestText,
						Result.Plan,
						Result.RouteReason,
						Result.PlannerMetadata,
						SummaryMessage));
					Complete(MoveTemp(Result));
				});
		});

	if (!bAccepted)
	{
		FHCIAgentCommandResult Result;
		Result.bSuccess = false;
		Result.bSummaryFailure = false;
		Result.Message = TEXT("已有请求执行中，请等待当前请求完成后再发送。");
		Complete(MoveTemp(Result));
	}
}

