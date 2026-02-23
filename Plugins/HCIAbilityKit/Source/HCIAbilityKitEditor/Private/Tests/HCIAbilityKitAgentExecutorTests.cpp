#if WITH_DEV_AUTOMATION_TESTS

#include "Agent/HCIAbilityKitAgentExecutor.h"
#include "Agent/HCIAbilityKitAgentPlan.h"
#include "Agent/HCIAbilityKitAgentPlanner.h"
#include "Agent/HCIAbilityKitToolRegistry.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"

namespace
{
static FHCIAbilityKitAgentPlan MakeExecutorTexturePlan()
{
	FHCIAbilityKitAgentPlan Plan;
	Plan.PlanVersion = 1;
	Plan.RequestId = TEXT("req_f3_test");
	Plan.Intent = TEXT("batch_fix_asset_compliance");

	FHCIAbilityKitAgentPlanStep& Step = Plan.Steps.AddDefaulted_GetRef();
	Step.StepId = TEXT("s1");
	Step.ToolName = TEXT("SetTextureMaxSize");
	Step.RiskLevel = EHCIAbilityKitAgentPlanRiskLevel::Write;
	Step.bRequiresConfirm = true;
	Step.RollbackStrategy = TEXT("all_or_nothing");
	Step.ExpectedEvidence = {TEXT("asset_path"), TEXT("before"), TEXT("after")};
	Step.Args = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> AssetPaths;
	AssetPaths.Add(MakeShared<FJsonValueString>(TEXT("/Game/Art/T_Test.T_Test")));
	Step.Args->SetArrayField(TEXT("asset_paths"), AssetPaths);
	Step.Args->SetNumberField(TEXT("max_size"), 1024);
	return Plan;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorValidPlanExecutesTest,
	"HCIAbilityKit.Editor.AgentExecutor.ValidPlanProducesStepResults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorValidPlanExecutesTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAgentPlan Plan;
	FString RouteReason;
	FString Error;
	TestTrue(
		TEXT("Planner should build asset compliance plan"),
		FHCIAbilityKitAgentPlanner::BuildPlanFromNaturalLanguage(
			TEXT("检查贴图分辨率并处理LOD"),
			TEXT("req_f3_valid"),
			Registry,
			Plan,
			RouteReason,
			Error));

	FHCIAbilityKitAgentExecutorRunResult RunResult;
	TestTrue(TEXT("Executor should complete valid plan"), FHCIAbilityKitAgentExecutor::ExecutePlan(Plan, Registry, RunResult));
	TestTrue(TEXT("RunResult should be accepted"), RunResult.bAccepted);
	TestTrue(TEXT("RunResult should be completed"), RunResult.bCompleted);
	TestEqual(TEXT("Terminal status"), RunResult.TerminalStatus, FString(TEXT("completed")));
	TestEqual(TEXT("Execution mode"), RunResult.ExecutionMode, FString(TEXT("simulate_dry_run")));
	TestEqual(TEXT("Step result count"), RunResult.StepResults.Num(), Plan.Steps.Num());
	TestEqual(TEXT("Succeeded steps"), RunResult.SucceededSteps, Plan.Steps.Num());
	TestTrue(TEXT("StartedAt should be +08:00"), RunResult.StartedAtUtc.Contains(TEXT("+08:00")));
	TestTrue(TEXT("FinishedAt should be +08:00"), RunResult.FinishedAtUtc.Contains(TEXT("+08:00")));

	if (RunResult.StepResults.Num() > 0)
	{
		const FHCIAbilityKitAgentExecutorStepResult& StepResult = RunResult.StepResults[0];
		TestTrue(TEXT("Step should succeed"), StepResult.bSucceeded);
		TestEqual(TEXT("Step status"), StepResult.Status, FString(TEXT("succeeded")));
		TestTrue(TEXT("Evidence should contain asset_path"), StepResult.Evidence.Contains(TEXT("asset_path")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorRejectsInvalidPlanTest,
	"HCIAbilityKit.Editor.AgentExecutor.InvalidPlanRejectedByValidator",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorRejectsInvalidPlanTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAgentPlan Plan = MakeExecutorTexturePlan();
	Plan.RequestId = TEXT("req_f3_invalid");
	Plan.Steps[0].ToolName = TEXT("UnknownTool_X");

	FHCIAbilityKitAgentExecutorRunResult RunResult;
	TestFalse(TEXT("Executor should reject invalid plan in precheck"), FHCIAbilityKitAgentExecutor::ExecutePlan(Plan, Registry, RunResult));
	TestFalse(TEXT("RunResult should not be accepted"), RunResult.bAccepted);
	TestFalse(TEXT("RunResult should not be completed"), RunResult.bCompleted);
	TestEqual(TEXT("Terminal status"), RunResult.TerminalStatus, FString(TEXT("rejected_precheck")));
	TestEqual(TEXT("Validator should return E4002"), RunResult.ErrorCode, FString(TEXT("E4002")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorLevelRiskEvidenceKeysTest,
	"HCIAbilityKit.Editor.AgentExecutor.LevelRiskPlanProducesExpectedEvidenceKeys",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorLevelRiskEvidenceKeysTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAgentPlan Plan;
	FString RouteReason;
	FString Error;
	TestTrue(
		TEXT("Planner should build level risk plan"),
		FHCIAbilityKitAgentPlanner::BuildPlanFromNaturalLanguage(
			TEXT("检查当前关卡选中物体的碰撞和默认材质"),
			TEXT("req_f3_level"),
			Registry,
			Plan,
			RouteReason,
			Error));

	FHCIAbilityKitAgentExecutorRunResult RunResult;
	TestTrue(TEXT("Executor should complete level risk plan"), FHCIAbilityKitAgentExecutor::ExecutePlan(Plan, Registry, RunResult));
	TestEqual(TEXT("Step result count"), RunResult.StepResults.Num(), 1);
	if (RunResult.StepResults.Num() > 0)
	{
		const FHCIAbilityKitAgentExecutorStepResult& StepResult = RunResult.StepResults[0];
		TestTrue(TEXT("Evidence should contain actor_path"), StepResult.Evidence.Contains(TEXT("actor_path")));
		TestTrue(TEXT("Evidence should contain issue"), StepResult.Evidence.Contains(TEXT("issue")));
		TestTrue(TEXT("Evidence should contain evidence"), StepResult.Evidence.Contains(TEXT("evidence")));
	}

	return true;
}

#endif

