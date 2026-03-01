#if WITH_DEV_AUTOMATION_TESTS

#include "Agent/Executor/HCIAgentExecutor.h"
#include "Agent/Bridges/HCIAgentExecutorDryRunBridge.h"
#include "Agent/Planner/HCIAgentPlanner.h"
#include "Agent/Executor/HCIDryRunDiff.h"
#include "Agent/Executor/HCIDryRunDiffJsonSerializer.h"
#include "Agent/Tools/HCIToolRegistry.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"

namespace
{
static FHCIAgentPlan MakeF6TexturePlan()
{
	FHCIAgentPlan Plan;
	Plan.PlanVersion = 1;
	Plan.RequestId = TEXT("req_f6_texture");
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
	AssetPaths.Add(MakeShared<FJsonValueString>(TEXT("/Game/Art/T_F6.T_F6")));
	Step.Args->SetArrayField(TEXT("asset_paths"), AssetPaths);
	Step.Args->SetNumberField(TEXT("max_size"), 1024);
	return Plan;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorDryRunBridgeLevelRiskMapsActorLocateTest,
	"HCI.Editor.AgentExecutorReview.BridgeMapsLevelRiskToActorLocate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorDryRunBridgeLevelRiskMapsActorLocateTest::RunTest(const FString& Parameters)
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
			TEXT("req_f6_level"),
			Registry,
			Plan,
			RouteReason,
			Error));

	FHCIAgentExecutorRunResult RunResult;
	TestTrue(TEXT("Executor should succeed"), FHCIAgentExecutor::ExecutePlan(Plan, Registry, RunResult));

	FHCIDryRunDiffReport Report;
	TestTrue(TEXT("Bridge should build dry-run diff report"), FHCIAgentExecutorDryRunBridge::BuildDryRunDiffReport(RunResult, Report));
	TestEqual(TEXT("RequestId should carry through"), Report.RequestId, FString(TEXT("req_f6_level")));
	TestTrue(TEXT("DiffItems should not be empty"), Report.DiffItems.Num() > 0);
	if (Report.DiffItems.Num() <= 0)
	{
		return false;
	}

	const FHCIDryRunDiffItem& Item = Report.DiffItems[0];
	TestEqual(TEXT("ObjectType should be actor"), FHCIDryRunDiff::ObjectTypeToString(Item.ObjectType), FString(TEXT("actor")));
	TestEqual(TEXT("LocateStrategy should be camera_focus"), FHCIDryRunDiff::LocateStrategyToString(Item.LocateStrategy), FString(TEXT("camera_focus")));
	TestEqual(TEXT("EvidenceKey should be actor_path"), Item.EvidenceKey, FString(TEXT("actor_path")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorDryRunBridgePreflightFailureMapsSkipReasonTest,
	"HCI.Editor.AgentExecutorReview.BridgeMapsPreflightFailureToBlockedDiffItem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorDryRunBridgePreflightFailureMapsSkipReasonTest::RunTest(const FString& Parameters)
{
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentExecutorOptions Options;
	Options.bEnablePreflightGates = true;
	Options.bUserConfirmedWriteSteps = false;
	Options.bDryRun = true;

	FHCIAgentExecutorRunResult RunResult;
	TestFalse(TEXT("Executor should be blocked by confirm gate"), FHCIAgentExecutor::ExecutePlan(
		MakeF6TexturePlan(),
		Registry,
		FHCIAgentPlanValidationContext(),
		Options,
		RunResult));

	FHCIDryRunDiffReport Report;
	TestTrue(TEXT("Bridge should build report for failure runs"), FHCIAgentExecutorDryRunBridge::BuildDryRunDiffReport(RunResult, Report));
	TestEqual(TEXT("DiffItems should contain one blocked row"), Report.DiffItems.Num(), 1);
	if (Report.DiffItems.Num() != 1)
	{
		return false;
	}

	const FHCIDryRunDiffItem& Item = Report.DiffItems[0];
	TestEqual(TEXT("LocateStrategy should be sync_browser"), FHCIDryRunDiff::LocateStrategyToString(Item.LocateStrategy), FString(TEXT("sync_browser")));
	TestTrue(TEXT("SkipReason should contain E4005"), Item.SkipReason.Contains(TEXT("E4005")));
	TestTrue(TEXT("SkipReason should contain confirm_gate"), Item.SkipReason.Contains(TEXT("confirm_gate")));
	TestEqual(TEXT("Summary skipped should be 1"), Report.Summary.Skipped, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutorDryRunBridgeJsonContainsBlockedFieldsTest,
	"HCI.Editor.AgentExecutorReview.BridgeJsonIncludesBlockedDiffFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutorDryRunBridgeJsonContainsBlockedFieldsTest::RunTest(const FString& Parameters)
{
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentExecutorOptions Options;
	Options.bEnablePreflightGates = true;
	Options.bUserConfirmedWriteSteps = false;
	Options.bDryRun = true;

	FHCIAgentExecutorRunResult RunResult;
	TestFalse(TEXT("Executor should fail preflight"), FHCIAgentExecutor::ExecutePlan(
		MakeF6TexturePlan(),
		Registry,
		FHCIAgentPlanValidationContext(),
		Options,
		RunResult));

	FHCIDryRunDiffReport Report;
	TestTrue(TEXT("Bridge should build report"), FHCIAgentExecutorDryRunBridge::BuildDryRunDiffReport(RunResult, Report));

	FString JsonText;
	TestTrue(TEXT("DryRunDiff JSON serialization should succeed"), FHCIDryRunDiffJsonSerializer::SerializeToJsonString(Report, JsonText));
	TestTrue(TEXT("JSON should contain request_id"), JsonText.Contains(TEXT("\"request_id\"")));
	TestTrue(TEXT("JSON should contain diff_items"), JsonText.Contains(TEXT("\"diff_items\"")));
	TestTrue(TEXT("JSON should contain skip_reason"), JsonText.Contains(TEXT("\"skip_reason\"")));
	TestTrue(TEXT("JSON should contain locate_strategy"), JsonText.Contains(TEXT("\"locate_strategy\"")));
	TestTrue(TEXT("JSON should contain confirm_gate"), JsonText.Contains(TEXT("confirm_gate")));
	return true;
}

#endif

