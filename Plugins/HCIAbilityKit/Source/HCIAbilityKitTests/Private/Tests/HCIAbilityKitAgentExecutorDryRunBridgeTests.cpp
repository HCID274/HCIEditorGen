#if WITH_DEV_AUTOMATION_TESTS

#include "Agent/Executor/HCIAbilityKitAgentExecutor.h"
#include "Agent/Bridges/HCIAbilityKitAgentExecutorDryRunBridge.h"
#include "Agent/Planner/HCIAbilityKitAgentPlanner.h"
#include "Agent/Executor/HCIAbilityKitDryRunDiff.h"
#include "Agent/Executor/HCIAbilityKitDryRunDiffJsonSerializer.h"
#include "Agent/Tools/HCIAbilityKitToolRegistry.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"

namespace
{
static FHCIAbilityKitAgentPlan MakeF6TexturePlan()
{
	FHCIAbilityKitAgentPlan Plan;
	Plan.PlanVersion = 1;
	Plan.RequestId = TEXT("req_f6_texture");
	Plan.Intent = TEXT("batch_fix_asset_compliance");

	FHCIAbilityKitAgentPlanStep& Step = Plan.Steps.AddDefaulted_GetRef();
	Step.StepId = TEXT("s1");
	Step.ToolName = TEXT("SetTextureMaxSize");
	Step.RiskLevel = EHCIAbilityKitAgentPlanRiskLevel::Write;
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
	FHCIAbilityKitAgentExecutorDryRunBridgeLevelRiskMapsActorLocateTest,
	"HCIAbilityKit.Editor.AgentExecutorReview.BridgeMapsLevelRiskToActorLocate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorDryRunBridgeLevelRiskMapsActorLocateTest::RunTest(const FString& Parameters)
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
			TEXT("req_f6_level"),
			Registry,
			Plan,
			RouteReason,
			Error));

	FHCIAbilityKitAgentExecutorRunResult RunResult;
	TestTrue(TEXT("Executor should succeed"), FHCIAbilityKitAgentExecutor::ExecutePlan(Plan, Registry, RunResult));

	FHCIAbilityKitDryRunDiffReport Report;
	TestTrue(TEXT("Bridge should build dry-run diff report"), FHCIAbilityKitAgentExecutorDryRunBridge::BuildDryRunDiffReport(RunResult, Report));
	TestEqual(TEXT("RequestId should carry through"), Report.RequestId, FString(TEXT("req_f6_level")));
	TestTrue(TEXT("DiffItems should not be empty"), Report.DiffItems.Num() > 0);
	if (Report.DiffItems.Num() <= 0)
	{
		return false;
	}

	const FHCIAbilityKitDryRunDiffItem& Item = Report.DiffItems[0];
	TestEqual(TEXT("ObjectType should be actor"), FHCIAbilityKitDryRunDiff::ObjectTypeToString(Item.ObjectType), FString(TEXT("actor")));
	TestEqual(TEXT("LocateStrategy should be camera_focus"), FHCIAbilityKitDryRunDiff::LocateStrategyToString(Item.LocateStrategy), FString(TEXT("camera_focus")));
	TestEqual(TEXT("EvidenceKey should be actor_path"), Item.EvidenceKey, FString(TEXT("actor_path")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorDryRunBridgePreflightFailureMapsSkipReasonTest,
	"HCIAbilityKit.Editor.AgentExecutorReview.BridgeMapsPreflightFailureToBlockedDiffItem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorDryRunBridgePreflightFailureMapsSkipReasonTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAgentExecutorOptions Options;
	Options.bEnablePreflightGates = true;
	Options.bUserConfirmedWriteSteps = false;
	Options.bDryRun = true;

	FHCIAbilityKitAgentExecutorRunResult RunResult;
	TestFalse(TEXT("Executor should be blocked by confirm gate"), FHCIAbilityKitAgentExecutor::ExecutePlan(
		MakeF6TexturePlan(),
		Registry,
		FHCIAbilityKitAgentPlanValidationContext(),
		Options,
		RunResult));

	FHCIAbilityKitDryRunDiffReport Report;
	TestTrue(TEXT("Bridge should build report for failure runs"), FHCIAbilityKitAgentExecutorDryRunBridge::BuildDryRunDiffReport(RunResult, Report));
	TestEqual(TEXT("DiffItems should contain one blocked row"), Report.DiffItems.Num(), 1);
	if (Report.DiffItems.Num() != 1)
	{
		return false;
	}

	const FHCIAbilityKitDryRunDiffItem& Item = Report.DiffItems[0];
	TestEqual(TEXT("LocateStrategy should be sync_browser"), FHCIAbilityKitDryRunDiff::LocateStrategyToString(Item.LocateStrategy), FString(TEXT("sync_browser")));
	TestTrue(TEXT("SkipReason should contain E4005"), Item.SkipReason.Contains(TEXT("E4005")));
	TestTrue(TEXT("SkipReason should contain confirm_gate"), Item.SkipReason.Contains(TEXT("confirm_gate")));
	TestEqual(TEXT("Summary skipped should be 1"), Report.Summary.Skipped, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutorDryRunBridgeJsonContainsBlockedFieldsTest,
	"HCIAbilityKit.Editor.AgentExecutorReview.BridgeJsonIncludesBlockedDiffFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutorDryRunBridgeJsonContainsBlockedFieldsTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAgentExecutorOptions Options;
	Options.bEnablePreflightGates = true;
	Options.bUserConfirmedWriteSteps = false;
	Options.bDryRun = true;

	FHCIAbilityKitAgentExecutorRunResult RunResult;
	TestFalse(TEXT("Executor should fail preflight"), FHCIAbilityKitAgentExecutor::ExecutePlan(
		MakeF6TexturePlan(),
		Registry,
		FHCIAbilityKitAgentPlanValidationContext(),
		Options,
		RunResult));

	FHCIAbilityKitDryRunDiffReport Report;
	TestTrue(TEXT("Bridge should build report"), FHCIAbilityKitAgentExecutorDryRunBridge::BuildDryRunDiffReport(RunResult, Report));

	FString JsonText;
	TestTrue(TEXT("DryRunDiff JSON serialization should succeed"), FHCIAbilityKitDryRunDiffJsonSerializer::SerializeToJsonString(Report, JsonText));
	TestTrue(TEXT("JSON should contain request_id"), JsonText.Contains(TEXT("\"request_id\"")));
	TestTrue(TEXT("JSON should contain diff_items"), JsonText.Contains(TEXT("\"diff_items\"")));
	TestTrue(TEXT("JSON should contain skip_reason"), JsonText.Contains(TEXT("\"skip_reason\"")));
	TestTrue(TEXT("JSON should contain locate_strategy"), JsonText.Contains(TEXT("\"locate_strategy\"")));
	TestTrue(TEXT("JSON should contain confirm_gate"), JsonText.Contains(TEXT("confirm_gate")));
	return true;
}

#endif
