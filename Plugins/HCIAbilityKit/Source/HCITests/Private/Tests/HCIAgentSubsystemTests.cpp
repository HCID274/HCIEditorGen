#if WITH_DEV_AUTOMATION_TESTS

#include "Subsystems/HCIAgentSubsystem.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"

namespace
{
static FHCIAgentPlan MakePlan(const FString& RequestId)
{
	FHCIAgentPlan Plan;
	Plan.PlanVersion = 1;
	Plan.RequestId = RequestId;
	Plan.Intent = TEXT("test_intent");
	return Plan;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentSubsystemStateDefaultIdleTest,
	"HCI.Editor.AgentChat.StateMachine.DefaultStateIsIdle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentSubsystemStateDefaultIdleTest::RunTest(const FString& Parameters)
{
	const UHCIAgentSubsystem* Subsystem = NewObject<UHCIAgentSubsystem>();
	TestNotNull(TEXT("Subsystem should be created"), Subsystem);
	if (!Subsystem)
	{
		return false;
	}

	TestEqual(TEXT("Default state should be Idle"), Subsystem->GetCurrentState(), EHCIAgentSessionState::Idle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentSubsystemReadOnlyBranchTest,
	"HCI.Editor.AgentChat.StateMachine.ReadOnlyPlanSelectsAutoExecuteBranch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentSubsystemReadOnlyBranchTest::RunTest(const FString& Parameters)
{
	FHCIAgentPlan Plan = MakePlan(TEXT("req_test_chat_branch_read_only"));
	FHCIAgentPlanStep& Step = Plan.Steps.AddDefaulted_GetRef();
	Step.StepId = TEXT("step_1_scan");
	Step.ToolName = TEXT("ScanAssets");
	Step.RiskLevel = EHCIAgentPlanRiskLevel::ReadOnly;
	Step.Args = MakeShared<FJsonObject>();
	Step.Args->SetStringField(TEXT("directory"), TEXT("/Game/HCI"));

	TestFalse(TEXT("Plan should not be write-like"), UHCIAgentSubsystem::IsWriteLikePlan(Plan));
	TestEqual(
		TEXT("Read-only plan should auto execute"),
		UHCIAgentSubsystem::ClassifyPlanExecutionBranch(Plan),
		EHCIAgentPlanExecutionBranch::AutoExecuteReadOnly);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentSubsystemWriteBranchTest,
	"HCI.Editor.AgentChat.StateMachine.WritePlanSelectsAwaitUserConfirmBranch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentSubsystemWriteBranchTest::RunTest(const FString& Parameters)
{
	FHCIAgentPlan Plan = MakePlan(TEXT("req_test_chat_branch_write"));
	FHCIAgentPlanStep& Step = Plan.Steps.AddDefaulted_GetRef();
	Step.StepId = TEXT("step_2_set_max_size");
	Step.ToolName = TEXT("SetTextureMaxSize");
	Step.RiskLevel = EHCIAgentPlanRiskLevel::Write;
	Step.bRequiresConfirm = true;
	Step.Args = MakeShared<FJsonObject>();
	Step.Args->SetNumberField(TEXT("max_size"), 1024);

	TestTrue(TEXT("Plan should be write-like"), UHCIAgentSubsystem::IsWriteLikePlan(Plan));
	TestEqual(
		TEXT("Write plan should await user confirm"),
		UHCIAgentSubsystem::ClassifyPlanExecutionBranch(Plan),
		EHCIAgentPlanExecutionBranch::AwaitUserConfirm);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentSubsystemUiPresentationSummaryPreferredTest,
	"HCI.Editor.AgentChat.UiPresentation.StepSummaryPreferredOverFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentSubsystemUiPresentationSummaryPreferredTest::RunTest(const FString& Parameters)
{
	FHCIAgentPlanStep Step;
	Step.StepId = TEXT("step_1_scan_risks");
	Step.ToolName = TEXT("ScanLevelMeshRisks");
	Step.RiskLevel = EHCIAgentPlanRiskLevel::ReadOnly;
	Step.UiPresentation.bHasStepSummary = true;
	Step.UiPresentation.StepSummary = TEXT("正在扫描场景模型风险");

	const FString Summary = UHCIAgentSubsystem::BuildStepDisplaySummaryForUi(Step);
	TestEqual(TEXT("Should prefer ui_presentation.step_summary"), Summary, FString(TEXT("正在扫描场景模型风险")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentSubsystemUiPresentationFallbackSummaryTest,
	"HCI.Editor.AgentChat.UiPresentation.MissingSummaryFallsBackToCppHumanText",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentSubsystemUiPresentationFallbackSummaryTest::RunTest(const FString& Parameters)
{
	FHCIAgentPlanStep Step;
	Step.StepId = TEXT("step_1_scan_risks");
	Step.ToolName = TEXT("ScanLevelMeshRisks");
	Step.RiskLevel = EHCIAgentPlanRiskLevel::ReadOnly;
	Step.Args = MakeShared<FJsonObject>();
	Step.Args->SetStringField(TEXT("scope"), TEXT("selected"));

	const FString Summary = UHCIAgentSubsystem::BuildStepDisplaySummaryForUi(Step);
	TestTrue(TEXT("Fallback summary should be human-readable"), Summary.Contains(TEXT("扫描场景模型风险")));
	TestFalse(TEXT("Fallback summary should avoid raw tool name"), Summary.Contains(TEXT("ScanLevelMeshRisks")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentSubsystemReviewImpactPipelineHintTest,
	"HCI.Editor.AgentChat.ReviewCard.PipelineAssetPathsShowsDeferredImpactHint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentSubsystemReviewImpactPipelineHintTest::RunTest(const FString& Parameters)
{
	FHCIAgentPlanStep Step;
	Step.StepId = TEXT("step_2_set_max_size");
	Step.ToolName = TEXT("SetTextureMaxSize");
	Step.RiskLevel = EHCIAgentPlanRiskLevel::Write;
	Step.bRequiresConfirm = true;
	Step.Args = MakeShared<FJsonObject>();

	TArray<TSharedPtr<FJsonValue>> AssetPaths;
	AssetPaths.Add(MakeShared<FJsonValueString>(TEXT("{{step_1_scan.asset_paths}}")));
	Step.Args->SetArrayField(TEXT("asset_paths"), AssetPaths);

	const FString ImpactHint = UHCIAgentSubsystem::BuildStepImpactHintForUi(Step);
	TestTrue(TEXT("Pipeline input should mention deferred impact"), ImpactHint.Contains(TEXT("前序扫描结果")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentSubsystemReviewImpactExplicitCountTest,
	"HCI.Editor.AgentChat.ReviewCard.ExplicitAssetPathsShowsImpactCount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentSubsystemReviewImpactExplicitCountTest::RunTest(const FString& Parameters)
{
	FHCIAgentPlanStep Step;
	Step.StepId = TEXT("step_2_set_lod");
	Step.ToolName = TEXT("SetMeshLODGroup");
	Step.RiskLevel = EHCIAgentPlanRiskLevel::Write;
	Step.bRequiresConfirm = true;
	Step.Args = MakeShared<FJsonObject>();

	TArray<TSharedPtr<FJsonValue>> AssetPaths;
	AssetPaths.Add(MakeShared<FJsonValueString>(TEXT("/Game/__HCI_Auto/SM_A.SM_A")));
	AssetPaths.Add(MakeShared<FJsonValueString>(TEXT("/Game/__HCI_Auto/SM_B.SM_B")));
	Step.Args->SetArrayField(TEXT("asset_paths"), AssetPaths);

	const FString ImpactHint = UHCIAgentSubsystem::BuildStepImpactHintForUi(Step);
	TestTrue(TEXT("Explicit paths should report asset count"), ImpactHint.Contains(TEXT("2")));
	TestTrue(TEXT("Explicit paths should use impact count wording"), ImpactHint.Contains(TEXT("影响数量")));
	return true;
}

#endif

