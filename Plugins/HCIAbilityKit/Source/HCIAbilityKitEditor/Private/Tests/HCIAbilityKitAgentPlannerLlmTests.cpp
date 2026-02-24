#if WITH_DEV_AUTOMATION_TESTS

#include "Agent/HCIAbilityKitAgentPlan.h"
#include "Agent/HCIAbilityKitAgentPlanner.h"
#include "Agent/HCIAbilityKitAgentPlanValidator.h"
#include "Agent/HCIAbilityKitToolRegistry.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentPlannerLlmSuccessTest,
	"HCIAbilityKit.Editor.AgentPlanLLM.LlmProviderSuccessNoFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentPlannerLlmSuccessTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAgentPlannerBuildOptions Options;
	Options.bPreferLlm = true;
	Options.LlmMockMode = EHCIAbilityKitAgentPlannerLlmMockMode::None;

	FHCIAbilityKitAgentPlan Plan;
	FString RouteReason;
	FHCIAbilityKitAgentPlannerResultMetadata Metadata;
	FString Error;
	const bool bBuilt = FHCIAbilityKitAgentPlanner::BuildPlanFromNaturalLanguageWithProvider(
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

	FHCIAbilityKitAgentPlanValidationResult Validation;
	TestTrue(TEXT("Built plan should pass validator"), FHCIAbilityKitAgentPlanValidator::ValidatePlan(Plan, Registry, Validation));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentPlannerLlmTimeoutFallbackTest,
	"HCIAbilityKit.Editor.AgentPlanLLM.TimeoutFallsBackToKeyword",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentPlannerLlmTimeoutFallbackTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAgentPlannerBuildOptions Options;
	Options.bPreferLlm = true;
	Options.LlmMockMode = EHCIAbilityKitAgentPlannerLlmMockMode::Timeout;

	FHCIAbilityKitAgentPlan Plan;
	FString RouteReason;
	FHCIAbilityKitAgentPlannerResultMetadata Metadata;
	FString Error;
	const bool bBuilt = FHCIAbilityKitAgentPlanner::BuildPlanFromNaturalLanguageWithProvider(
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
	FHCIAbilityKitAgentPlannerLlmInvalidJsonFallbackTest,
	"HCIAbilityKit.Editor.AgentPlanLLM.InvalidJsonFallsBackToKeyword",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentPlannerLlmInvalidJsonFallbackTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAgentPlannerBuildOptions Options;
	Options.bPreferLlm = true;
	Options.LlmMockMode = EHCIAbilityKitAgentPlannerLlmMockMode::InvalidJson;

	FHCIAbilityKitAgentPlan Plan;
	FString RouteReason;
	FHCIAbilityKitAgentPlannerResultMetadata Metadata;
	FString Error;
	const bool bBuilt = FHCIAbilityKitAgentPlanner::BuildPlanFromNaturalLanguageWithProvider(
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

#endif
