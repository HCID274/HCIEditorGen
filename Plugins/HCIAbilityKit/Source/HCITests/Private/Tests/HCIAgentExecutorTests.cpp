#if WITH_DEV_AUTOMATION_TESTS

#include "Agent/Executor/HCIAgentExecutor.h"
#include "Agent/Planner/HCIAgentPlan.h"
#include "Agent/Planner/HCIAgentPlanner.h"
#include "Agent/Tools/HCIAgentToolAction.h"
#include "Agent/Tools/HCIToolRegistry.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"

namespace
{
class FHCITestRecordRenameAction final : public IHCIAgentToolAction
{
public:
	virtual FName GetToolName() const override
	{
		return TEXT("RenameAsset");
	}

	virtual bool DryRun(
		const FHCIAgentToolActionRequest& Request,
		FHCIAgentToolActionResult& OutResult) const override
	{
		++DryRunCalls;
		OutResult = FHCIAgentToolActionResult();
		OutResult.bSucceeded = true;
		OutResult.Reason = TEXT("test_dry_run_ok");
		OutResult.Evidence.Add(TEXT("asset_path"), Request.Args.IsValid() ? Request.Args->GetStringField(TEXT("asset_path")) : TEXT("-"));
		return true;
	}

	virtual bool Execute(
		const FHCIAgentToolActionRequest& Request,
		FHCIAgentToolActionResult& OutResult) const override
	{
		++ExecuteCalls;
		OutResult = FHCIAgentToolActionResult();
		OutResult.bSucceeded = true;
		OutResult.Reason = TEXT("test_execute_ok");
		OutResult.Evidence.Add(TEXT("before"), Request.Args.IsValid() ? Request.Args->GetStringField(TEXT("asset_path")) : TEXT("-"));
		OutResult.Evidence.Add(TEXT("after"), TEXT("/Game/Art/SM_TestCommit_Renamed.SM_TestCommit_Renamed"));
		OutResult.Evidence.Add(TEXT("result"), TEXT("test_execute_ok"));
		return true;
	}

	mutable int32 DryRunCalls = 0;
	mutable int32 ExecuteCalls = 0;
};

static FHCIAgentPlan MakeExecutorTexturePlan()
{
	FHCIAgentPlan Plan;
	Plan.PlanVersion = 1;
	Plan.RequestId = TEXT("req_f3_test");
	Plan.Intent = TEXT("batch_fix_asset_compliance");

	FHCIAgentPlanStep& Step = Plan.Steps.AddDefaulted_GetRef();
	Step.StepId = TEXT("s1");
	Step.ToolName = TEXT("SetTextureMaxSize");
	Step.RiskLevel = EHCIAgentPlanRiskLevel::Write;
	Step.bRequiresConfirm = true;
	Step.RollbackStrategy = TEXT("all_or_nothing");
	Step.ExpectedEvidence = {
		TEXT("target_max_size"),
		TEXT("scanned_count"),
		TEXT("modified_count"),
		TEXT("failed_count"),
		TEXT("modified_assets"),
		TEXT("failed_assets"),
		TEXT("result")};
	Step.Args = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> AssetPaths;
	AssetPaths.Add(MakeShared<FJsonValueString>(TEXT("/Game/Art/T_Test.T_Test")));
	Step.Args->SetArrayField(TEXT("asset_paths"), AssetPaths);
	Step.Args->SetNumberField(TEXT("max_size"), 1024);
	return Plan;
}

static FHCIAgentPlan MakeExecutorTwoStepPlan()
{
	FHCIAgentPlan Plan;
	Plan.PlanVersion = 1;
	Plan.RequestId = TEXT("req_f4_test");
	Plan.Intent = TEXT("batch_fix_asset_compliance");

	{
		FHCIAgentPlanStep& Step = Plan.Steps.AddDefaulted_GetRef();
		Step.StepId = TEXT("s1");
		Step.ToolName = TEXT("SetTextureMaxSize");
		Step.RiskLevel = EHCIAgentPlanRiskLevel::Write;
		Step.bRequiresConfirm = true;
		Step.RollbackStrategy = TEXT("all_or_nothing");
			Step.ExpectedEvidence = {
				TEXT("target_max_size"),
				TEXT("scanned_count"),
				TEXT("modified_count"),
				TEXT("failed_count"),
				TEXT("modified_assets"),
				TEXT("failed_assets"),
				TEXT("result")};
		Step.Args = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> AssetPaths;
		AssetPaths.Add(MakeShared<FJsonValueString>(TEXT("/Game/Art/T_Test_A.T_Test_A")));
		AssetPaths.Add(MakeShared<FJsonValueString>(TEXT("/Game/Art/T_Test_B.T_Test_B")));
		Step.Args->SetArrayField(TEXT("asset_paths"), AssetPaths);
		Step.Args->SetNumberField(TEXT("max_size"), 1024);
	}

	{
		FHCIAgentPlanStep& Step = Plan.Steps.AddDefaulted_GetRef();
		Step.StepId = TEXT("s2");
		Step.ToolName = TEXT("SetMeshLODGroup");
		Step.RiskLevel = EHCIAgentPlanRiskLevel::Write;
		Step.bRequiresConfirm = true;
		Step.RollbackStrategy = TEXT("all_or_nothing");
			Step.ExpectedEvidence = {
				TEXT("target_lod_group"),
				TEXT("scanned_count"),
				TEXT("modified_count"),
				TEXT("failed_count"),
				TEXT("modified_assets"),
				TEXT("failed_assets"),
				TEXT("result")};
		Step.Args = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> AssetPaths;
		AssetPaths.Add(MakeShared<FJsonValueString>(TEXT("/Game/Art/SM_Test.SM_Test")));
		Step.Args->SetArrayField(TEXT("asset_paths"), AssetPaths);
		Step.Args->SetStringField(TEXT("lod_group"), TEXT("SmallProp"));
	}

	return Plan;
}

static FHCIAgentPlan MakeExecutorTexturePlanWithAssetCount(const int32 AssetCount)
{
	FHCIAgentPlan Plan = MakeExecutorTexturePlan();
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

static FHCIAgentPlan MakeExecutorRenamePlan()
{
	FHCIAgentPlan Plan;
	Plan.PlanVersion = 1;
	Plan.RequestId = TEXT("req_i3_execute_mode");
	Plan.Intent = TEXT("normalize_temp_assets_by_metadata");

	FHCIAgentPlanStep& Step = Plan.Steps.AddDefaulted_GetRef();
	Step.StepId = TEXT("step_rename");
	Step.ToolName = TEXT("RenameAsset");
	Step.RiskLevel = EHCIAgentPlanRiskLevel::Write;
	Step.bRequiresConfirm = true;
	Step.RollbackStrategy = TEXT("all_or_nothing");
	Step.ExpectedEvidence = {TEXT("before"), TEXT("after"), TEXT("result")};
	Step.Args = MakeShared<FJsonObject>();
	Step.Args->SetStringField(TEXT("asset_path"), TEXT("/Game/Art/SM_TestCommit.SM_TestCommit"));
	Step.Args->SetStringField(TEXT("new_name"), TEXT("SM_TestCommit_Renamed"));
	return Plan;
}

static FHCIAgentPlan MakeExecutorPipelineBypassPlan()
{
	FHCIAgentPlan Plan;
	Plan.PlanVersion = 1;
	Plan.RequestId = TEXT("req_i2_pipeline_bypass");
	Plan.Intent = TEXT("scan_assets");

	FHCIAgentPlanStep& SearchStep = Plan.Steps.AddDefaulted_GetRef();
	SearchStep.StepId = TEXT("step_1_search");
	SearchStep.ToolName = TEXT("SearchPath");
	SearchStep.RiskLevel = EHCIAgentPlanRiskLevel::ReadOnly;
	SearchStep.bRequiresConfirm = false;
	SearchStep.RollbackStrategy = TEXT("all_or_nothing");
	SearchStep.ExpectedEvidence = {TEXT("matched_directories"), TEXT("best_directory"), TEXT("result")};
	SearchStep.Args = MakeShared<FJsonObject>();
	SearchStep.Args->SetStringField(TEXT("keyword"), TEXT("MNew"));

	FHCIAgentPlanStep& ScanStep = Plan.Steps.AddDefaulted_GetRef();
	ScanStep.StepId = TEXT("step_2_scan");
	ScanStep.ToolName = TEXT("ScanAssets");
	ScanStep.RiskLevel = EHCIAgentPlanRiskLevel::ReadOnly;
	ScanStep.bRequiresConfirm = false;
	ScanStep.RollbackStrategy = TEXT("all_or_nothing");
	ScanStep.ExpectedEvidence = {TEXT("scan_root"), TEXT("asset_count"), TEXT("asset_paths"), TEXT("result")};
	ScanStep.Args = MakeShared<FJsonObject>();
	ScanStep.Args->SetStringField(TEXT("directory"), TEXT("/Game/Temp"));
	return Plan;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorValidPlanExecutesTest,
	"HCI.Editor.AgentExecutor.ValidPlanProducesStepResults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorValidPlanExecutesTest::RunTest(const FString& Parameters)
{
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentPlan Plan;
	FString RouteReason;
	FString Error;
	TestTrue(
		TEXT("Planner should build asset compliance plan"),
		FHCIAgentPlanner::BuildPlanFromNaturalLanguage(
			TEXT("检查贴图分辨率并处理LOD"),
			TEXT("req_f3_valid"),
			Registry,
			Plan,
			RouteReason,
			Error));

	FHCIAgentExecutorRunResult RunResult;
	TestTrue(TEXT("Executor should complete valid plan"), FHCIAgentExecutor::ExecutePlan(Plan, Registry, RunResult));
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
		const FHCIAgentExecutorStepResult& StepResult = RunResult.StepResults[0];
		TestTrue(TEXT("Step should succeed"), StepResult.bSucceeded);
		TestEqual(TEXT("Step status"), StepResult.Status, FString(TEXT("succeeded")));
		TestTrue(TEXT("Evidence should contain target_max_size"), StepResult.Evidence.Contains(TEXT("target_max_size")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorRejectsInvalidPlanTest,
	"HCI.Editor.AgentExecutor.InvalidPlanRejectedByValidator",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorRejectsInvalidPlanTest::RunTest(const FString& Parameters)
{
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentPlan Plan = MakeExecutorTexturePlan();
	Plan.RequestId = TEXT("req_f3_invalid");
	Plan.Steps[0].ToolName = TEXT("UnknownTool_X");

	FHCIAgentExecutorRunResult RunResult;
	TestFalse(TEXT("Executor should reject invalid plan in precheck"), FHCIAgentExecutor::ExecutePlan(Plan, Registry, RunResult));
	TestFalse(TEXT("RunResult should not be accepted"), RunResult.bAccepted);
	TestFalse(TEXT("RunResult should not be completed"), RunResult.bCompleted);
	TestEqual(TEXT("Terminal status"), RunResult.TerminalStatus, FString(TEXT("rejected_precheck")));
	TestEqual(TEXT("Validator should return E4002"), RunResult.ErrorCode, FString(TEXT("E4002")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorLevelRiskEvidenceKeysTest,
	"HCI.Editor.AgentExecutor.LevelRiskPlanProducesExpectedEvidenceKeys",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorLevelRiskEvidenceKeysTest::RunTest(const FString& Parameters)
{
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentPlan Plan;
	FString RouteReason;
	FString Error;
	TestTrue(
		TEXT("Planner should build level risk plan"),
		FHCIAgentPlanner::BuildPlanFromNaturalLanguage(
			TEXT("检查当前关卡选中物体的碰撞和默认材质"),
			TEXT("req_f3_level"),
			Registry,
			Plan,
			RouteReason,
			Error));

	FHCIAgentExecutorRunResult RunResult;
	TestTrue(TEXT("Executor should complete level risk plan"), FHCIAgentExecutor::ExecutePlan(Plan, Registry, RunResult));
	TestEqual(TEXT("Step result count"), RunResult.StepResults.Num(), 1);
	if (RunResult.StepResults.Num() > 0)
	{
		const FHCIAgentExecutorStepResult& StepResult = RunResult.StepResults[0];
		TestTrue(TEXT("Evidence should contain actor_path"), StepResult.Evidence.Contains(TEXT("actor_path")));
		TestTrue(TEXT("Evidence should contain issue"), StepResult.Evidence.Contains(TEXT("issue")));
		TestTrue(TEXT("Evidence should contain evidence"), StepResult.Evidence.Contains(TEXT("evidence")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorStopOnFirstFailureTest,
	"HCI.Editor.AgentExecutor.StepFailureStopOnFirstPolicyConverges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorStopOnFirstFailureTest::RunTest(const FString& Parameters)
{
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentPlan Plan = MakeExecutorTwoStepPlan();
	Plan.RequestId = TEXT("req_f4_stop_on_first");

	FHCIAgentExecutorOptions Options;
	Options.bValidatePlanBeforeExecute = true;
	Options.bDryRun = true;
	Options.TerminationPolicy = EHCIAgentExecutorTerminationPolicy::StopOnFirstFailure;
	Options.SimulatedFailureStepIndex = 0;
	Options.SimulatedFailureErrorCode = TEXT("E4101");
	Options.SimulatedFailureReason = TEXT("simulated_tool_execution_failed");

	FHCIAgentExecutorRunResult RunResult;
	TestFalse(TEXT("Executor should return false when a step fails"), FHCIAgentExecutor::ExecutePlan(
		Plan,
		Registry,
		FHCIAgentPlanValidationContext(),
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
		const FHCIAgentExecutorStepResult& FailedRow = RunResult.StepResults[0];
		const FHCIAgentExecutorStepResult& SkippedRow = RunResult.StepResults[1];

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
	FHCIAgentExecutorContinueOnFailureTest,
	"HCI.Editor.AgentExecutor.StepFailureContinuePolicyKeepsRunning",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorContinueOnFailureTest::RunTest(const FString& Parameters)
{
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentPlan Plan = MakeExecutorTwoStepPlan();
	Plan.RequestId = TEXT("req_f4_continue");

	FHCIAgentExecutorOptions Options;
	Options.bValidatePlanBeforeExecute = true;
	Options.bDryRun = true;
	Options.TerminationPolicy = EHCIAgentExecutorTerminationPolicy::ContinueOnFailure;
	Options.SimulatedFailureStepIndex = 0;
	Options.SimulatedFailureErrorCode = TEXT("E4102");
	Options.SimulatedFailureReason = TEXT("simulated_first_step_failed_continue");

	FHCIAgentExecutorRunResult RunResult;
	TestFalse(TEXT("Executor should return false when any step fails"), FHCIAgentExecutor::ExecutePlan(
		Plan,
		Registry,
		FHCIAgentPlanValidationContext(),
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
	FHCIAgentExecutorPreflightConfirmGateTest,
	"HCI.Editor.AgentExecutor.PreflightBlocksUnconfirmedWriteWithE4005",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorPreflightConfirmGateTest::RunTest(const FString& Parameters)
{
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentExecutorOptions Options;
	Options.bEnablePreflightGates = true;
	Options.bUserConfirmedWriteSteps = false;
	Options.bDryRun = true;

	FHCIAgentExecutorRunResult RunResult;
	TestFalse(TEXT("Preflight confirm gate should block unconfirmed write"), FHCIAgentExecutor::ExecutePlan(
		MakeExecutorTexturePlan(),
		Registry,
		FHCIAgentPlanValidationContext(),
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
	FHCIAgentExecutorPreflightBlastRadiusTest,
	"HCI.Editor.AgentExecutor.PreflightBlocksOverLimitWithE4004",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorPreflightBlastRadiusTest::RunTest(const FString& Parameters)
{
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentExecutorOptions Options;
	Options.bEnablePreflightGates = true;
	Options.bUserConfirmedWriteSteps = true;
	Options.bValidatePlanBeforeExecute = false; // let F5 preflight blast-radius gate own the failure instead of F2 validator

	FHCIAgentExecutorRunResult RunResult;
	TestFalse(TEXT("Preflight blast radius should block >50 assets"), FHCIAgentExecutor::ExecutePlan(
		MakeExecutorTexturePlanWithAssetCount(51),
		Registry,
		FHCIAgentPlanValidationContext(),
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
	FHCIAgentExecutorPreflightSourceControlTest,
	"HCI.Editor.AgentExecutor.PreflightBlocksSourceControlFailFastWithE4006",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorPreflightSourceControlTest::RunTest(const FString& Parameters)
{
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentExecutorOptions Options;
	Options.bEnablePreflightGates = true;
	Options.bSourceControlEnabled = true;
	Options.bSourceControlCheckoutSucceeded = false;

	FHCIAgentExecutorRunResult RunResult;
	TestFalse(TEXT("Preflight source control fail-fast should block"), FHCIAgentExecutor::ExecutePlan(
		MakeExecutorTexturePlan(),
		Registry,
		FHCIAgentPlanValidationContext(),
		Options,
		RunResult));

	TestEqual(TEXT("Failed gate"), RunResult.FailedGate, FString(TEXT("source_control")));
	TestEqual(TEXT("Top-level error code"), RunResult.ErrorCode, FString(TEXT("E4006")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorPreflightLodSafetyTest,
	"HCI.Editor.AgentExecutor.PreflightBlocksNaniteLodToolWithE4010",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorPreflightLodSafetyTest::RunTest(const FString& Parameters)
{
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentExecutorOptions Options;
	Options.bEnablePreflightGates = true;
	Options.bSimulatedLodTargetNaniteEnabled = true;
	Options.SimulatedLodTargetObjectClass = TEXT("UStaticMesh");

	FHCIAgentExecutorRunResult RunResult;
	TestFalse(TEXT("Preflight LOD safety should block nanite mesh"), FHCIAgentExecutor::ExecutePlan(
		MakeExecutorTwoStepPlan(),
		Registry,
		FHCIAgentPlanValidationContext(),
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
	FHCIAgentExecutorPreflightPassesAuthorizedWriteTest,
	"HCI.Editor.AgentExecutor.PreflightAuthorizedWritePassesAndMarksRow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorPreflightPassesAuthorizedWriteTest::RunTest(const FString& Parameters)
{
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentExecutorOptions Options;
	Options.bEnablePreflightGates = true;
	Options.bDryRun = true;
	Options.bUserConfirmedWriteSteps = true;
	Options.bSourceControlEnabled = false; // offline local mode path
	Options.MockUserName = TEXT("artist_a");
	Options.MockResolvedRole = TEXT("Artist");
	Options.bMockUserMatchedWhitelist = true;
	Options.MockAllowedCapabilities = {TEXT("read_only"), TEXT("write")};

	FHCIAgentExecutorRunResult RunResult;
	TestTrue(TEXT("Authorized write should pass preflight and succeed"), FHCIAgentExecutor::ExecutePlan(
		MakeExecutorTexturePlan(),
		Registry,
		FHCIAgentPlanValidationContext(),
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorCommitModeSemanticTest,
	"HCI.Editor.AgentExecutor.CommitModeUsesExecuteSemantics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorCommitModeSemanticTest::RunTest(const FString& Parameters)
{
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentExecutorOptions Options;
	Options.bDryRun = false;
	Options.bValidatePlanBeforeExecute = true;

	const TSharedPtr<FHCITestRecordRenameAction> Action = MakeShared<FHCITestRecordRenameAction>();
	Options.ToolActions.Add(TEXT("RenameAsset"), Action);

	FHCIAgentExecutorRunResult RunResult;
	TestTrue(TEXT("Commit run should succeed with tool action"), FHCIAgentExecutor::ExecutePlan(
		MakeExecutorRenamePlan(),
		Registry,
		FHCIAgentPlanValidationContext(),
		Options,
		RunResult));

	TestEqual(TEXT("Execution mode should be execute_apply"), RunResult.ExecutionMode, FString(TEXT("execute_apply")));
	TestEqual(TEXT("Terminal status should be completed"), RunResult.TerminalStatus, FString(TEXT("completed")));
	TestEqual(TEXT("Terminal reason should reflect execute completion"), RunResult.TerminalReason, FString(TEXT("executor_execute_completed")));
	TestEqual(TEXT("DryRun call count"), Action->DryRunCalls, 0);
	TestEqual(TEXT("Execute call count"), Action->ExecuteCalls, 1);

	if (RunResult.StepResults.Num() > 0)
	{
		TestEqual(TEXT("Row reason should come from action execute"), RunResult.StepResults[0].Reason, FString(TEXT("test_execute_ok")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorPipelineBypassBlockedTest,
	"HCI.Editor.AgentExecutor.PipelineBypassAfterSearchBlockedWithE4009",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorPipelineBypassBlockedTest::RunTest(const FString& Parameters)
{
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentExecutorOptions Options;
	Options.bDryRun = true;
	Options.bValidatePlanBeforeExecute = true;
	Options.TerminationPolicy = EHCIAgentExecutorTerminationPolicy::StopOnFirstFailure;

	FHCIAgentExecutorRunResult RunResult;
	TestFalse(
		TEXT("Pipeline bypass should be blocked"),
		FHCIAgentExecutor::ExecutePlan(
			MakeExecutorPipelineBypassPlan(),
			Registry,
			FHCIAgentPlanValidationContext(),
			Options,
			RunResult));

	TestEqual(TEXT("Top-level error code"), RunResult.ErrorCode, FString(TEXT("E4009")));
	TestEqual(TEXT("Top-level reason"), RunResult.Reason, FString(TEXT("planner_pipeline_variable_not_used_after_search")));
	TestEqual(TEXT("Failed step index should be scan step"), RunResult.FailedStepIndex, 1);
	if (RunResult.StepResults.Num() > 1)
	{
		const FString* WarningDetail = RunResult.StepResults[1].Evidence.Find(TEXT("planning_warning_detail"));
		TestNotNull(TEXT("Planning warning detail should exist"), WarningDetail);
		if (WarningDetail != nullptr)
		{
			TestTrue(
				TEXT("Planning warning detail should mention unconsumed pipe variable"),
				WarningDetail->Contains(TEXT("Pipe variable from Step 1 (SearchPath) is defined but not consumed by subsequent steps")));
		}
	}
	return true;
}

#endif

