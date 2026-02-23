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

static FHCIAbilityKitAgentPlan MakeExecutorTwoStepPlan()
{
	FHCIAbilityKitAgentPlan Plan;
	Plan.PlanVersion = 1;
	Plan.RequestId = TEXT("req_f4_test");
	Plan.Intent = TEXT("batch_fix_asset_compliance");

	{
		FHCIAbilityKitAgentPlanStep& Step = Plan.Steps.AddDefaulted_GetRef();
		Step.StepId = TEXT("s1");
		Step.ToolName = TEXT("SetTextureMaxSize");
		Step.RiskLevel = EHCIAbilityKitAgentPlanRiskLevel::Write;
		Step.bRequiresConfirm = true;
		Step.RollbackStrategy = TEXT("all_or_nothing");
		Step.ExpectedEvidence = {TEXT("asset_path"), TEXT("before"), TEXT("after")};
		Step.Args = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> AssetPaths;
		AssetPaths.Add(MakeShared<FJsonValueString>(TEXT("/Game/Art/T_Test_A.T_Test_A")));
		AssetPaths.Add(MakeShared<FJsonValueString>(TEXT("/Game/Art/T_Test_B.T_Test_B")));
		Step.Args->SetArrayField(TEXT("asset_paths"), AssetPaths);
		Step.Args->SetNumberField(TEXT("max_size"), 1024);
	}

	{
		FHCIAbilityKitAgentPlanStep& Step = Plan.Steps.AddDefaulted_GetRef();
		Step.StepId = TEXT("s2");
		Step.ToolName = TEXT("SetMeshLODGroup");
		Step.RiskLevel = EHCIAbilityKitAgentPlanRiskLevel::Write;
		Step.bRequiresConfirm = true;
		Step.RollbackStrategy = TEXT("all_or_nothing");
		Step.ExpectedEvidence = {TEXT("asset_path"), TEXT("before"), TEXT("after")};
		Step.Args = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> AssetPaths;
		AssetPaths.Add(MakeShared<FJsonValueString>(TEXT("/Game/Art/SM_Test.SM_Test")));
		Step.Args->SetArrayField(TEXT("asset_paths"), AssetPaths);
		Step.Args->SetStringField(TEXT("lod_group"), TEXT("SmallProp"));
	}

	return Plan;
}

static FHCIAbilityKitAgentPlan MakeExecutorTexturePlanWithAssetCount(const int32 AssetCount)
{
	FHCIAbilityKitAgentPlan Plan = MakeExecutorTexturePlan();
	Plan.RequestId = TEXT("req_f5_asset_count");
	Plan.Steps[0].Args = MakeShared<FJsonObject>();

	TArray<TSharedPtr<FJsonValue>> AssetPaths;
	AssetPaths.Reserve(FMath::Max(0, AssetCount));
	for (int32 Index = 0; Index < AssetCount; ++Index)
	{
		AssetPaths.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("/Game/Art/T_Batch_%03d.T_Batch_%03d"), Index, Index)));
	}
	Plan.Steps[0].Args->SetArrayField(TEXT("asset_paths"), AssetPaths);
	Plan.Steps[0].Args->SetNumberField(TEXT("max_size"), 1024);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorStopOnFirstFailureTest,
	"HCIAbilityKit.Editor.AgentExecutor.StepFailureStopOnFirstPolicyConverges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorStopOnFirstFailureTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAgentPlan Plan = MakeExecutorTwoStepPlan();
	Plan.RequestId = TEXT("req_f4_stop_on_first");

	FHCIAbilityKitAgentExecutorOptions Options;
	Options.bValidatePlanBeforeExecute = true;
	Options.bDryRun = true;
	Options.TerminationPolicy = EHCIAbilityKitAgentExecutorTerminationPolicy::StopOnFirstFailure;
	Options.SimulatedFailureStepIndex = 0;
	Options.SimulatedFailureErrorCode = TEXT("E4101");
	Options.SimulatedFailureReason = TEXT("simulated_tool_execution_failed");

	FHCIAbilityKitAgentExecutorRunResult RunResult;
	TestFalse(TEXT("Executor should return false when a step fails"), FHCIAbilityKitAgentExecutor::ExecutePlan(
		Plan,
		Registry,
		FHCIAbilityKitAgentPlanValidationContext(),
		Options,
		RunResult));

	TestTrue(TEXT("RunResult should be accepted"), RunResult.bAccepted);
	TestFalse(TEXT("RunResult should not be completed"), RunResult.bCompleted);
	TestEqual(TEXT("Termination policy"), RunResult.TerminationPolicy, FString(TEXT("stop_on_first_failure")));
	TestEqual(TEXT("Terminal status"), RunResult.TerminalStatus, FString(TEXT("failed")));
	TestEqual(TEXT("Terminal reason"), RunResult.TerminalReason, FString(TEXT("executor_step_failed_stop_on_first_failure")));
	TestEqual(TEXT("Top-level error code"), RunResult.ErrorCode, FString(TEXT("E4101")));
	TestEqual(TEXT("Failed step index"), RunResult.FailedStepIndex, 0);
	TestEqual(TEXT("Executed steps"), RunResult.ExecutedSteps, 1);
	TestEqual(TEXT("Succeeded steps"), RunResult.SucceededSteps, 0);
	TestEqual(TEXT("Failed steps"), RunResult.FailedSteps, 1);
	TestEqual(TEXT("Skipped steps"), RunResult.SkippedSteps, 1);
	TestEqual(TEXT("Step row count should include skipped rows"), RunResult.StepResults.Num(), 2);

	if (RunResult.StepResults.Num() >= 2)
	{
		const FHCIAbilityKitAgentExecutorStepResult& FailedRow = RunResult.StepResults[0];
		const FHCIAbilityKitAgentExecutorStepResult& SkippedRow = RunResult.StepResults[1];

		TestEqual(TEXT("Failed row status"), FailedRow.Status, FString(TEXT("failed")));
		TestFalse(TEXT("Failed row should not succeed"), FailedRow.bSucceeded);
		TestEqual(TEXT("Failed row error code"), FailedRow.ErrorCode, FString(TEXT("E4101")));

		TestEqual(TEXT("Skipped row status"), SkippedRow.Status, FString(TEXT("skipped")));
		TestFalse(TEXT("Skipped row attempted"), SkippedRow.bAttempted);
		TestFalse(TEXT("Skipped row succeeded"), SkippedRow.bSucceeded);
		TestEqual(TEXT("Skipped row reason"), SkippedRow.Reason, FString(TEXT("terminated_by_stop_on_first_failure")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorContinueOnFailureTest,
	"HCIAbilityKit.Editor.AgentExecutor.StepFailureContinuePolicyKeepsRunning",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorContinueOnFailureTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAgentPlan Plan = MakeExecutorTwoStepPlan();
	Plan.RequestId = TEXT("req_f4_continue");

	FHCIAbilityKitAgentExecutorOptions Options;
	Options.bValidatePlanBeforeExecute = true;
	Options.bDryRun = true;
	Options.TerminationPolicy = EHCIAbilityKitAgentExecutorTerminationPolicy::ContinueOnFailure;
	Options.SimulatedFailureStepIndex = 0;
	Options.SimulatedFailureErrorCode = TEXT("E4102");
	Options.SimulatedFailureReason = TEXT("simulated_first_step_failed_continue");

	FHCIAbilityKitAgentExecutorRunResult RunResult;
	TestFalse(TEXT("Executor should return false when any step fails"), FHCIAbilityKitAgentExecutor::ExecutePlan(
		Plan,
		Registry,
		FHCIAbilityKitAgentPlanValidationContext(),
		Options,
		RunResult));

	TestTrue(TEXT("RunResult should be accepted"), RunResult.bAccepted);
	TestFalse(TEXT("RunResult should not be completed"), RunResult.bCompleted);
	TestEqual(TEXT("Termination policy"), RunResult.TerminationPolicy, FString(TEXT("continue_on_failure")));
	TestEqual(TEXT("Terminal status"), RunResult.TerminalStatus, FString(TEXT("completed_with_failures")));
	TestEqual(TEXT("Terminal reason"), RunResult.TerminalReason, FString(TEXT("executor_step_failed_continue_on_failure")));
	TestEqual(TEXT("Executed steps"), RunResult.ExecutedSteps, 2);
	TestEqual(TEXT("Succeeded steps"), RunResult.SucceededSteps, 1);
	TestEqual(TEXT("Failed steps"), RunResult.FailedSteps, 1);
	TestEqual(TEXT("Skipped steps"), RunResult.SkippedSteps, 0);
	TestEqual(TEXT("Step row count"), RunResult.StepResults.Num(), 2);

	if (RunResult.StepResults.Num() >= 2)
	{
		TestEqual(TEXT("Row0 failed"), RunResult.StepResults[0].Status, FString(TEXT("failed")));
		TestEqual(TEXT("Row1 succeeded"), RunResult.StepResults[1].Status, FString(TEXT("succeeded")));
		TestTrue(TEXT("Row1 attempted"), RunResult.StepResults[1].bAttempted);
		TestTrue(TEXT("Row1 succeeded flag"), RunResult.StepResults[1].bSucceeded);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorPreflightConfirmGateTest,
	"HCIAbilityKit.Editor.AgentExecutor.PreflightBlocksUnconfirmedWriteWithE4005",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorPreflightConfirmGateTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAgentExecutorOptions Options;
	Options.bEnablePreflightGates = true;
	Options.bUserConfirmedWriteSteps = false;
	Options.bDryRun = true;

	FHCIAbilityKitAgentExecutorRunResult RunResult;
	TestFalse(TEXT("Preflight confirm gate should block unconfirmed write"), FHCIAbilityKitAgentExecutor::ExecutePlan(
		MakeExecutorTexturePlan(),
		Registry,
		FHCIAbilityKitAgentPlanValidationContext(),
		Options,
		RunResult));

	TestTrue(TEXT("Run should be accepted after validator"), RunResult.bAccepted);
	TestEqual(TEXT("Failed gate"), RunResult.FailedGate, FString(TEXT("confirm_gate")));
	TestEqual(TEXT("Top-level error code"), RunResult.ErrorCode, FString(TEXT("E4005")));
	TestEqual(TEXT("Terminal reason"), RunResult.TerminalReason, FString(TEXT("executor_preflight_gate_failed_stop_on_first_failure")));
	TestEqual(TEXT("Preflight blocked steps"), RunResult.PreflightBlockedSteps, 1);
	if (RunResult.StepResults.Num() > 0)
	{
		TestEqual(TEXT("Row failure phase"), RunResult.StepResults[0].FailurePhase, FString(TEXT("preflight")));
		TestEqual(TEXT("Row preflight gate"), RunResult.StepResults[0].PreflightGate, FString(TEXT("confirm_gate")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorPreflightBlastRadiusTest,
	"HCIAbilityKit.Editor.AgentExecutor.PreflightBlocksOverLimitWithE4004",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorPreflightBlastRadiusTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAgentExecutorOptions Options;
	Options.bEnablePreflightGates = true;
	Options.bUserConfirmedWriteSteps = true;
	Options.bValidatePlanBeforeExecute = false; // let F5 preflight blast-radius gate own the failure instead of F2 validator

	FHCIAbilityKitAgentExecutorRunResult RunResult;
	TestFalse(TEXT("Preflight blast radius should block >50 assets"), FHCIAbilityKitAgentExecutor::ExecutePlan(
		MakeExecutorTexturePlanWithAssetCount(51),
		Registry,
		FHCIAbilityKitAgentPlanValidationContext(),
		Options,
		RunResult));

	TestEqual(TEXT("Failed gate"), RunResult.FailedGate, FString(TEXT("blast_radius")));
	TestEqual(TEXT("Top-level error code"), RunResult.ErrorCode, FString(TEXT("E4004")));
	TestTrue(TEXT("Should produce at least one row"), RunResult.StepResults.Num() > 0);
	if (RunResult.StepResults.Num() > 0)
	{
		TestEqual(TEXT("Row target count estimate"), RunResult.StepResults[0].TargetCountEstimate, 51);
		TestEqual(TEXT("Row failure phase"), RunResult.StepResults[0].FailurePhase, FString(TEXT("preflight")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorPreflightSourceControlTest,
	"HCIAbilityKit.Editor.AgentExecutor.PreflightBlocksSourceControlFailFastWithE4006",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorPreflightSourceControlTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAgentExecutorOptions Options;
	Options.bEnablePreflightGates = true;
	Options.bSourceControlEnabled = true;
	Options.bSourceControlCheckoutSucceeded = false;

	FHCIAbilityKitAgentExecutorRunResult RunResult;
	TestFalse(TEXT("Preflight source control fail-fast should block"), FHCIAbilityKitAgentExecutor::ExecutePlan(
		MakeExecutorTexturePlan(),
		Registry,
		FHCIAbilityKitAgentPlanValidationContext(),
		Options,
		RunResult));

	TestEqual(TEXT("Failed gate"), RunResult.FailedGate, FString(TEXT("source_control")));
	TestEqual(TEXT("Top-level error code"), RunResult.ErrorCode, FString(TEXT("E4006")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorPreflightLodSafetyTest,
	"HCIAbilityKit.Editor.AgentExecutor.PreflightBlocksNaniteLodToolWithE4010",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorPreflightLodSafetyTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAgentExecutorOptions Options;
	Options.bEnablePreflightGates = true;
	Options.bSimulatedLodTargetNaniteEnabled = true;
	Options.SimulatedLodTargetObjectClass = TEXT("UStaticMesh");

	FHCIAbilityKitAgentExecutorRunResult RunResult;
	TestFalse(TEXT("Preflight LOD safety should block nanite mesh"), FHCIAbilityKitAgentExecutor::ExecutePlan(
		MakeExecutorTwoStepPlan(),
		Registry,
		FHCIAbilityKitAgentPlanValidationContext(),
		Options,
		RunResult));

	TestEqual(TEXT("Failed step should be second step (0-based index 1)"), RunResult.FailedStepIndex, 1);
	TestEqual(TEXT("Failed gate"), RunResult.FailedGate, FString(TEXT("lod_safety")));
	TestEqual(TEXT("Top-level error code"), RunResult.ErrorCode, FString(TEXT("E4010")));
	if (RunResult.StepResults.Num() >= 2)
	{
		TestEqual(TEXT("First row preflight passed"), RunResult.StepResults[0].PreflightGate, FString(TEXT("passed")));
		TestEqual(TEXT("Second row preflight gate"), RunResult.StepResults[1].PreflightGate, FString(TEXT("lod_safety")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorPreflightPassesAuthorizedWriteTest,
	"HCIAbilityKit.Editor.AgentExecutor.PreflightAuthorizedWritePassesAndMarksRow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorPreflightPassesAuthorizedWriteTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAgentExecutorOptions Options;
	Options.bEnablePreflightGates = true;
	Options.bDryRun = true;
	Options.bUserConfirmedWriteSteps = true;
	Options.bSourceControlEnabled = false; // offline local mode path
	Options.MockUserName = TEXT("artist_a");
	Options.MockResolvedRole = TEXT("Artist");
	Options.bMockUserMatchedWhitelist = true;
	Options.MockAllowedCapabilities = {TEXT("read_only"), TEXT("write")};

	FHCIAbilityKitAgentExecutorRunResult RunResult;
	TestTrue(TEXT("Authorized write should pass preflight and succeed"), FHCIAbilityKitAgentExecutor::ExecutePlan(
		MakeExecutorTexturePlan(),
		Registry,
		FHCIAbilityKitAgentPlanValidationContext(),
		Options,
		RunResult));

	TestTrue(TEXT("Preflight enabled flag"), RunResult.bPreflightEnabled);
	TestEqual(TEXT("Preflight blocked steps"), RunResult.PreflightBlockedSteps, 0);
	TestEqual(TEXT("Failed gate empty"), RunResult.FailedGate, FString(TEXT("")));
	if (RunResult.StepResults.Num() > 0)
	{
		TestEqual(TEXT("Row preflight gate"), RunResult.StepResults[0].PreflightGate, FString(TEXT("passed")));
		TestEqual(TEXT("Row failure phase"), RunResult.StepResults[0].FailurePhase, FString(TEXT("-")));
	}
	return true;
}

#endif
