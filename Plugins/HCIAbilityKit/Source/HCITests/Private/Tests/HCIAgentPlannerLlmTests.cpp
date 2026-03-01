#if WITH_DEV_AUTOMATION_TESTS

#include "Agent/Planner/HCIAgentPlan.h"
#include "Agent/Planner/HCIAgentPlanner.h"
#include "Agent/Planner/HCIAgentPlanValidator.h"
#include "Agent/Tools/HCIToolRegistry.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentPlannerLlmSuccessTest,
	"HCI.Editor.AgentPlanLLM.LlmProviderSuccessNoFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentPlannerLlmSuccessTest::RunTest(const FString& Parameters)
{
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentPlannerBuildOptions Options;
	Options.bPreferLlm = true;
	Options.LlmMockMode = EHCIAgentPlannerLlmMockMode::None;

	FHCIAgentPlan Plan;
	FString RouteReason;
	FHCIAgentPlannerResultMetadata Metadata;
	FString Error;
	const bool bBuilt = FHCIAgentPlanner::BuildPlanFromNaturalLanguageWithProvider(
		TEXT("整理临时目录资产并归档"),
		TEXT("req_h1_llm_01"),
		Registry,
		Options,
		Plan,
		RouteReason,
		Metadata,
		Error);
	TestTrue(TEXT("LLM preferred should build plan"), bBuilt);
	TestEqual(TEXT("Planner provider should be llm"), Metadata.PlannerProvider, FString(TEXT("llm")));
	TestFalse(TEXT("Fallback should not be used"), Metadata.bFallbackUsed);
	TestEqual(TEXT("Fallback reason should be none"), Metadata.FallbackReason, FString(TEXT("none")));
	TestEqual(TEXT("Error code should be -"), Metadata.ErrorCode, FString(TEXT("-")));

	FHCIAgentPlanValidationResult Validation;
	TestTrue(TEXT("Built plan should pass validator"), FHCIAgentPlanValidator::ValidatePlan(Plan, Registry, Validation));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentPlannerLlmTimeoutFallbackTest,
	"HCI.Editor.AgentPlanLLM.TimeoutFallsBackToKeyword",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentPlannerLlmTimeoutFallbackTest::RunTest(const FString& Parameters)
{
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentPlannerBuildOptions Options;
	Options.bPreferLlm = true;
	Options.LlmMockMode = EHCIAgentPlannerLlmMockMode::Timeout;

	FHCIAgentPlan Plan;
	FString RouteReason;
	FHCIAgentPlannerResultMetadata Metadata;
	FString Error;
	const bool bBuilt = FHCIAgentPlanner::BuildPlanFromNaturalLanguageWithProvider(
		TEXT("整理临时目录资产并归档"),
		TEXT("req_h1_llm_02"),
		Registry,
		Options,
		Plan,
		RouteReason,
		Metadata,
		Error);
	TestTrue(TEXT("Timeout should fallback to keyword plan"), bBuilt);
	TestEqual(TEXT("Planner provider should be keyword_fallback"), Metadata.PlannerProvider, FString(TEXT("keyword_fallback")));
	TestTrue(TEXT("Fallback should be used"), Metadata.bFallbackUsed);
	TestEqual(TEXT("Fallback reason should be llm_timeout"), Metadata.FallbackReason, FString(TEXT("llm_timeout")));
	TestEqual(TEXT("Error code should be E4301"), Metadata.ErrorCode, FString(TEXT("E4301")));
	TestEqual(TEXT("Fallback plan intent should still be naming"), Plan.Intent, FString(TEXT("normalize_temp_assets_by_metadata")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentPlannerLlmInvalidJsonFallbackTest,
	"HCI.Editor.AgentPlanLLM.InvalidJsonFallsBackToKeyword",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentPlannerLlmInvalidJsonFallbackTest::RunTest(const FString& Parameters)
{
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentPlannerBuildOptions Options;
	Options.bPreferLlm = true;
	Options.LlmMockMode = EHCIAgentPlannerLlmMockMode::InvalidJson;

	FHCIAgentPlan Plan;
	FString RouteReason;
	FHCIAgentPlannerResultMetadata Metadata;
	FString Error;
	const bool bBuilt = FHCIAgentPlanner::BuildPlanFromNaturalLanguageWithProvider(
		TEXT("扫描关卡里缺碰撞和默认材质"),
		TEXT("req_h1_llm_03"),
		Registry,
		Options,
		Plan,
		RouteReason,
		Metadata,
		Error);
	TestTrue(TEXT("Invalid JSON should fallback to keyword plan"), bBuilt);
	TestEqual(TEXT("Planner provider should be keyword_fallback"), Metadata.PlannerProvider, FString(TEXT("keyword_fallback")));
	TestTrue(TEXT("Fallback should be used"), Metadata.bFallbackUsed);
	TestEqual(TEXT("Fallback reason should be llm_invalid_json"), Metadata.FallbackReason, FString(TEXT("llm_invalid_json")));
	TestEqual(TEXT("Error code should be E4302"), Metadata.ErrorCode, FString(TEXT("E4302")));
	TestEqual(TEXT("Fallback plan intent should be level risk"), Plan.Intent, FString(TEXT("scan_level_mesh_risks")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentPlannerLlmContractInvalidFallbackTest,
	"HCI.Editor.AgentPlanLLM.ContractInvalidFallsBackToKeywordWithMissingExpectedEvidenceDetail",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentPlannerLlmContractInvalidFallbackTest::RunTest(const FString& Parameters)
{
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentPlannerBuildOptions Options;
	Options.bPreferLlm = true;
	Options.LlmMockMode = EHCIAgentPlannerLlmMockMode::ContractInvalid;

	FHCIAgentPlan Plan;
	FString RouteReason;
	FHCIAgentPlannerResultMetadata Metadata;
	FString Error;
	const bool bBuilt = FHCIAgentPlanner::BuildPlanFromNaturalLanguageWithProvider(
		TEXT("扫描关卡缺碰撞和默认材质"),
		TEXT("req_h5_llm_contract_01"),
		Registry,
		Options,
		Plan,
		RouteReason,
		Metadata,
		Error);
	TestTrue(TEXT("Contract invalid should fallback to keyword plan"), bBuilt);
	TestEqual(TEXT("Planner provider should be keyword_fallback"), Metadata.PlannerProvider, FString(TEXT("keyword_fallback")));
	TestTrue(TEXT("Fallback should be used"), Metadata.bFallbackUsed);
	TestEqual(TEXT("Fallback reason should be llm_contract_invalid"), Metadata.FallbackReason, FString(TEXT("llm_contract_invalid")));
	TestEqual(TEXT("Error code should be E4303"), Metadata.ErrorCode, FString(TEXT("E4303")));
	TestTrue(TEXT("Error detail should mention missing expected_evidence"), Error.Contains(TEXT("Missing required field: expected_evidence")));
	TestEqual(TEXT("Fallback plan intent should be level risk"), Plan.Intent, FString(TEXT("scan_level_mesh_risks")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentPlannerLlmContractInvalidChitchatFallsBackToMessageOnlyTest,
	"HCI.Editor.AgentPlanLLM.ContractInvalidChitchatFallsBackToPreparedMessageOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentPlannerLlmContractInvalidChitchatFallsBackToMessageOnlyTest::RunTest(const FString& Parameters)
{
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentPlannerBuildOptions Options;
	Options.bPreferLlm = true;
	Options.LlmMockMode = EHCIAgentPlannerLlmMockMode::ContractInvalid;

	FHCIAgentPlan Plan;
	FString RouteReason;
	FHCIAgentPlannerResultMetadata Metadata;
	FString Error;
	const bool bBuilt = FHCIAgentPlanner::BuildPlanFromNaturalLanguageWithProvider(
		TEXT("你是谁？"),
		TEXT("req_h5_llm_contract_chitchat_01"),
		Registry,
		Options,
		Plan,
		RouteReason,
		Metadata,
		Error);

	TestTrue(TEXT("Contract invalid on chitchat should still build message-only plan"), bBuilt);
	TestEqual(TEXT("Planner provider should be keyword_fallback"), Metadata.PlannerProvider, FString(TEXT("keyword_fallback")));
	TestTrue(TEXT("Fallback should be used"), Metadata.bFallbackUsed);
	TestEqual(TEXT("Fallback reason should remain llm_contract_invalid"), Metadata.FallbackReason, FString(TEXT("llm_contract_invalid")));
	TestEqual(TEXT("Route should be prepared_message_only_identity"), RouteReason, FString(TEXT("prepared_message_only_identity")));
	TestEqual(TEXT("No tool steps should be generated"), Plan.Steps.Num(), 0);
	TestFalse(TEXT("Assistant message should not be empty"), Plan.AssistantMessage.IsEmpty());
	TestTrue(TEXT("Assistant message should mention HCI"), Plan.AssistantMessage.Contains(TEXT("HCI")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentPlannerKeywordUnknownInputReturnsClarifyMessageTest,
	"HCI.Editor.AgentPlanLLM.KeywordFallbackUnknownInputReturnsPreparedClarifyMessage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentPlannerKeywordUnknownInputReturnsClarifyMessageTest::RunTest(const FString& Parameters)
{
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentPlannerBuildOptions Options;
	Options.bPreferLlm = false;

	FHCIAgentPlan Plan;
	FString RouteReason;
	FHCIAgentPlannerResultMetadata Metadata;
	FString Error;
	const bool bBuilt = FHCIAgentPlanner::BuildPlanFromNaturalLanguageWithProvider(
		TEXT("随便聊聊"),
		TEXT("req_keyword_clarify_01"),
		Registry,
		Options,
		Plan,
		RouteReason,
		Metadata,
		Error);

	TestTrue(TEXT("Unknown non-task input should return prepared clarify message"), bBuilt);
	TestEqual(TEXT("Provider should be keyword_fallback when LLM disabled"), Metadata.PlannerProvider, FString(TEXT("keyword_fallback")));
	TestEqual(TEXT("Route should be prepared_message_only_clarify"), RouteReason, FString(TEXT("prepared_message_only_clarify")));
	TestEqual(TEXT("No tool steps should be generated"), Plan.Steps.Num(), 0);
	TestFalse(TEXT("Assistant message should not be empty"), Plan.AssistantMessage.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentPlannerLlmSuccessMisroutedChitchatIsGuardedTest,
	"HCI.Editor.AgentPlanLLM.LlmSuccessMisroutedChitchatIsForcedToPreparedMessageOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentPlannerLlmSuccessMisroutedChitchatIsGuardedTest::RunTest(const FString& Parameters)
{
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentPlannerBuildOptions Options;
	Options.bPreferLlm = true;
	Options.LlmMockMode = EHCIAgentPlannerLlmMockMode::ValidButMisroutedScanAssets;

	FHCIAgentPlan Plan;
	FString RouteReason;
	FHCIAgentPlannerResultMetadata Metadata;
	FString Error;
	const bool bBuilt = FHCIAgentPlanner::BuildPlanFromNaturalLanguageWithProvider(
		TEXT("你是哪个？"),
		TEXT("req_h5_llm_guard_chitchat_01"),
		Registry,
		Options,
		Plan,
		RouteReason,
		Metadata,
		Error);

	TestTrue(TEXT("LLM misrouted chitchat plan should be guarded into message-only"), bBuilt);
	TestEqual(TEXT("Planner provider should still be llm"), Metadata.PlannerProvider, FString(TEXT("llm")));
	TestFalse(TEXT("Fallback should not be used for semantic guard"), Metadata.bFallbackUsed);
	TestEqual(TEXT("Guard route should be prepared_message_only_identity"), RouteReason, FString(TEXT("prepared_message_only_identity")));
	TestEqual(TEXT("Guarded plan should have no steps"), Plan.Steps.Num(), 0);
	TestFalse(TEXT("Assistant message should not be empty"), Plan.AssistantMessage.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentPlannerLlmRetrySuccessTest,
	"HCI.Editor.AgentPlanLLM.RetryTimeoutThenLlmSuccess",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentPlannerLlmRetrySuccessTest::RunTest(const FString& Parameters)
{
	FHCIAgentPlanner::ResetMetricsForTesting();
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentPlannerBuildOptions Options;
	Options.bPreferLlm = true;
	Options.LlmMockMode = EHCIAgentPlannerLlmMockMode::Timeout;
	Options.LlmRetryCount = 1;
	Options.LlmMockFailAttempts = 1;

	FHCIAgentPlan Plan;
	FString RouteReason;
	FHCIAgentPlannerResultMetadata Metadata;
	FString Error;
	const bool bBuilt = FHCIAgentPlanner::BuildPlanFromNaturalLanguageWithProvider(
		TEXT("整理临时目录资产并归档"),
		TEXT("req_h2_llm_01"),
		Registry,
		Options,
		Plan,
		RouteReason,
		Metadata,
		Error);

	TestTrue(TEXT("Retry success should build plan"), bBuilt);
	TestEqual(TEXT("Planner provider should be llm"), Metadata.PlannerProvider, FString(TEXT("llm")));
	TestFalse(TEXT("Fallback should not be used"), Metadata.bFallbackUsed);
	TestTrue(TEXT("Retry should be used"), Metadata.bRetryUsed);
	TestEqual(TEXT("LLM attempts should be 2"), Metadata.LlmAttemptCount, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentPlannerLlmCircuitOpenFallbackTest,
	"HCI.Editor.AgentPlanLLM.CircuitOpenForcesKeywordFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentPlannerLlmCircuitOpenFallbackTest::RunTest(const FString& Parameters)
{
	FHCIAgentPlanner::ResetMetricsForTesting();
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentPlannerBuildOptions TriggerOptions;
	TriggerOptions.bPreferLlm = true;
	TriggerOptions.LlmMockMode = EHCIAgentPlannerLlmMockMode::Timeout;
	TriggerOptions.CircuitBreakerFailureThreshold = 1;
	TriggerOptions.CircuitBreakerOpenForRequests = 1;

	FHCIAgentPlan TriggerPlan;
	FString TriggerRoute;
	FHCIAgentPlannerResultMetadata TriggerMetadata;
	FString TriggerError;
	TestTrue(
		TEXT("Initial timeout should still fallback to keyword"),
		FHCIAgentPlanner::BuildPlanFromNaturalLanguageWithProvider(
			TEXT("整理临时目录资产并归档"),
			TEXT("req_h2_llm_02"),
			Registry,
			TriggerOptions,
			TriggerPlan,
			TriggerRoute,
			TriggerMetadata,
			TriggerError));

	FHCIAgentPlannerBuildOptions OpenOptions;
	OpenOptions.bPreferLlm = true;
	OpenOptions.LlmMockMode = EHCIAgentPlannerLlmMockMode::None;
	OpenOptions.CircuitBreakerFailureThreshold = 1;
	OpenOptions.CircuitBreakerOpenForRequests = 1;

	FHCIAgentPlan OpenPlan;
	FString OpenRoute;
	FHCIAgentPlannerResultMetadata OpenMetadata;
	FString OpenError;
	const bool bOpenBuilt = FHCIAgentPlanner::BuildPlanFromNaturalLanguageWithProvider(
		TEXT("整理临时目录资产并归档"),
		TEXT("req_h2_llm_03"),
		Registry,
		OpenOptions,
		OpenPlan,
		OpenRoute,
		OpenMetadata,
		OpenError);
	TestTrue(TEXT("Circuit-open request should fallback and still build plan"), bOpenBuilt);
	TestEqual(TEXT("Provider should be keyword_fallback"), OpenMetadata.PlannerProvider, FString(TEXT("keyword_fallback")));
	TestEqual(TEXT("Fallback reason should be llm_circuit_open"), OpenMetadata.FallbackReason, FString(TEXT("llm_circuit_open")));
	TestEqual(TEXT("Error code should be E4305"), OpenMetadata.ErrorCode, FString(TEXT("E4305")));
	TestTrue(TEXT("Circuit-open metadata should be true"), OpenMetadata.bCircuitBreakerOpen);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentPlannerLlmMetricsSnapshotTest,
	"HCI.Editor.AgentPlanLLM.MetricsSnapshotTracksRetryAndCircuit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentPlannerLlmMetricsSnapshotTest::RunTest(const FString& Parameters)
{
	FHCIAgentPlanner::ResetMetricsForTesting();
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentPlannerBuildOptions Options;
	Options.bPreferLlm = true;
	Options.LlmMockMode = EHCIAgentPlannerLlmMockMode::Timeout;
	Options.LlmRetryCount = 1;

	FHCIAgentPlan Plan;
	FString RouteReason;
	FHCIAgentPlannerResultMetadata Metadata;
	FString Error;
	TestTrue(
		TEXT("Retry fallback should still build"),
		FHCIAgentPlanner::BuildPlanFromNaturalLanguageWithProvider(
			TEXT("整理临时目录资产并归档"),
			TEXT("req_h2_llm_04"),
			Registry,
			Options,
			Plan,
			RouteReason,
			Metadata,
			Error));

	const FHCIAgentPlannerMetricsSnapshot Metrics = FHCIAgentPlanner::GetMetricsSnapshot();
	TestEqual(TEXT("Total requests should be 1"), Metrics.TotalRequests, 1);
	TestEqual(TEXT("LLM preferred requests should be 1"), Metrics.LlmPreferredRequests, 1);
	TestEqual(TEXT("Keyword fallback requests should be 1"), Metrics.KeywordFallbackRequests, 1);
	TestEqual(TEXT("Retry used requests should be 1"), Metrics.RetryUsedRequests, 1);
	TestEqual(TEXT("Retry attempts should be 1"), Metrics.RetryAttempts, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentPlannerRealHttpSyncDeprecatedTest,
	"HCI.Editor.AgentPlanLLM.RealHttpSyncRequiresAsyncApi",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentPlannerRealHttpSyncDeprecatedTest::RunTest(const FString& Parameters)
{
	FHCIAgentPlanner::ResetMetricsForTesting();
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentPlannerBuildOptions Options;
	Options.bPreferLlm = true;
	Options.bUseRealHttpProvider = true;
	Options.LlmApiKeyConfigPath = TEXT("Saved/HCIAbilityKit/Config/h3_missing_config.json");
	Options.LlmRetryCount = 0;

	FHCIAgentPlan Plan;
	FString RouteReason;
	FHCIAgentPlannerResultMetadata Metadata;
	FString Error;
	const bool bBuilt = FHCIAgentPlanner::BuildPlanFromNaturalLanguageWithProvider(
		TEXT("整理临时目录资产并归档"),
		TEXT("req_h3_llm_01"),
		Registry,
		Options,
		Plan,
		RouteReason,
			Metadata,
			Error);
	TestFalse(TEXT("Sync real_http should be rejected"), bBuilt);
	TestEqual(TEXT("Provider should be keyword_fallback"), Metadata.PlannerProvider, FString(TEXT("keyword_fallback")));
	TestEqual(TEXT("Provider mode should be real_http"), Metadata.ProviderMode, FString(TEXT("real_http")));
	TestEqual(TEXT("Fallback reason should be llm_sync_real_http_deprecated"), Metadata.FallbackReason, FString(TEXT("llm_sync_real_http_deprecated")));
	TestEqual(TEXT("Error code should be E4310"), Metadata.ErrorCode, FString(TEXT("E4310")));
	TestEqual(TEXT("Error should require async API"), Error, FString(TEXT("real_http_provider_requires_async_api")));
	return true;
}

#endif

