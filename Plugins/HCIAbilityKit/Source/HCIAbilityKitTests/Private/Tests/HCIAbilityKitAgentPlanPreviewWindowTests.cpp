#if WITH_DEV_AUTOMATION_TESTS

#include "UI/HCIAbilityKitAgentPlanPreviewWindow.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"

namespace
{
static FHCIAbilityKitAgentPlan MakeBasePlan(const FString& RequestId)
{
	FHCIAbilityKitAgentPlan Plan;
	Plan.PlanVersion = 1;
	Plan.RequestId = RequestId;
	Plan.Intent = TEXT("normalize_temp_assets_by_metadata");
	return Plan;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentPlanPreviewRowsPendingStateTest,
	"HCIAbilityKit.Editor.AgentPreviewUI.Rows.PendingStateForPlaceholderStep",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentPlanPreviewRowsPendingStateTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitAgentPlan Plan = MakeBasePlan(TEXT("req_test_preview_pending"));
	FHCIAbilityKitAgentPlanStep& Step = Plan.Steps.AddDefaulted_GetRef();
	Step.StepId = TEXT("step_2_scan");
	Step.ToolName = TEXT("ScanAssets");
	Step.RiskLevel = EHCIAbilityKitAgentPlanRiskLevel::ReadOnly;
	Step.bRequiresConfirm = false;
	Step.RollbackStrategy = TEXT("all_or_nothing");
	Step.Args = MakeShared<FJsonObject>();
	Step.Args->SetStringField(TEXT("directory"), TEXT("{{step_1_search.matched_directories[0]}}"));

	const TArray<TSharedPtr<FHCIAbilityKitAgentPlanPreviewRow>> Rows = FHCIAbilityKitAgentPlanPreviewWindow::BuildRows(Plan);
	TestEqual(TEXT("rows count"), Rows.Num(), 1);
	if (Rows.Num() != 1 || !Rows[0].IsValid())
	{
		return false;
	}

	const TSharedPtr<FHCIAbilityKitAgentPlanPreviewRow>& Row = Rows[0];
	TestEqual(TEXT("asset count label should be pending"), Row->AssetCountLabel, FString(TEXT("? (Pending)")));
	TestFalse(TEXT("locate should be disabled for placeholder step"), Row->bLocateEnabled);
	TestEqual(TEXT("locate tooltip"), Row->LocateTooltip, FString(TEXT("等待前置步骤结果")));
	TestEqual(TEXT("step state"), Row->StepStateLabel, FString(TEXT("pending_inputs")));
	TestTrue(TEXT("args preview keeps placeholder"), Row->ArgsPreview.Contains(TEXT("{{step_1_search.matched_directories[0]}}")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentPlanPreviewRowsMissingAssetStateTest,
	"HCIAbilityKit.Editor.AgentPreviewUI.Rows.AssetMissingStateForInvalidAssetPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentPlanPreviewRowsMissingAssetStateTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitAgentPlan Plan = MakeBasePlan(TEXT("req_test_preview_missing_asset"));
	FHCIAbilityKitAgentPlanStep& Step = Plan.Steps.AddDefaulted_GetRef();
	Step.StepId = TEXT("step_3_rename");
	Step.ToolName = TEXT("RenameAsset");
	Step.RiskLevel = EHCIAbilityKitAgentPlanRiskLevel::Write;
	Step.bRequiresConfirm = true;
	Step.RollbackStrategy = TEXT("all_or_nothing");
	Step.Args = MakeShared<FJsonObject>();

	TArray<TSharedPtr<FJsonValue>> AssetPaths;
	AssetPaths.Add(MakeShared<FJsonValueString>(TEXT("/Game/__HCI_NotExists__/SM_NotExists.SM_NotExists")));
	Step.Args->SetArrayField(TEXT("asset_paths"), AssetPaths);
	Step.Args->SetStringField(TEXT("new_name"), TEXT("SM_Renamed"));

	const TArray<TSharedPtr<FHCIAbilityKitAgentPlanPreviewRow>> Rows = FHCIAbilityKitAgentPlanPreviewWindow::BuildRows(Plan);
	TestEqual(TEXT("rows count"), Rows.Num(), 1);
	if (Rows.Num() != 1 || !Rows[0].IsValid())
	{
		return false;
	}

	const TSharedPtr<FHCIAbilityKitAgentPlanPreviewRow>& Row = Rows[0];
	TestEqual(TEXT("asset count label should keep numeric count"), Row->AssetCountLabel, FString(TEXT("1")));
	TestFalse(TEXT("locate should be disabled for missing assets"), Row->bLocateEnabled);
	TestEqual(TEXT("locate tooltip"), Row->LocateTooltip, FString(TEXT("资产在路径下不存在")));
	TestEqual(TEXT("step state"), Row->StepStateLabel, FString(TEXT("asset_missing")));
	TestTrue(TEXT("args preview includes key new_name"), Row->ArgsPreview.Contains(TEXT("new_name")));
	TestTrue(TEXT("args preview includes value SM_Renamed"), Row->ArgsPreview.Contains(TEXT("SM_Renamed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentPlanPreviewCommitRiskSummaryTest,
	"HCIAbilityKit.Editor.AgentPreviewUI.CommitRiskSummary.CountsWriteAndDestructiveSteps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentPlanPreviewCommitRiskSummaryTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitAgentPlan Plan = MakeBasePlan(TEXT("req_test_preview_commit_summary"));

	{
		FHCIAbilityKitAgentPlanStep& ReadStep = Plan.Steps.AddDefaulted_GetRef();
		ReadStep.StepId = TEXT("step_1_scan");
		ReadStep.ToolName = TEXT("ScanAssets");
		ReadStep.RiskLevel = EHCIAbilityKitAgentPlanRiskLevel::ReadOnly;
	}

	{
		FHCIAbilityKitAgentPlanStep& WriteStep = Plan.Steps.AddDefaulted_GetRef();
		WriteStep.StepId = TEXT("step_2_rename");
		WriteStep.ToolName = TEXT("RenameAsset");
		WriteStep.RiskLevel = EHCIAbilityKitAgentPlanRiskLevel::Write;
		WriteStep.bRequiresConfirm = true;
	}

	{
		FHCIAbilityKitAgentPlanStep& DestructiveStep = Plan.Steps.AddDefaulted_GetRef();
		DestructiveStep.StepId = TEXT("step_3_move");
		DestructiveStep.ToolName = TEXT("MoveAsset");
		DestructiveStep.RiskLevel = EHCIAbilityKitAgentPlanRiskLevel::Destructive;
		DestructiveStep.bRequiresConfirm = true;
	}

	const FHCIAbilityKitAgentPlanCommitRiskSummary Summary = FHCIAbilityKitAgentPlanPreviewWindow::BuildCommitRiskSummary(Plan);
	TestEqual(TEXT("total steps"), Summary.TotalSteps, 3);
	TestEqual(TEXT("write-like steps"), Summary.WriteLikeSteps, 2);
	TestEqual(TEXT("destructive steps"), Summary.DestructiveSteps, 1);
	TestTrue(TEXT("should require confirm"), Summary.bRequiresConfirmDialog);

	const FString ConfirmText = FHCIAbilityKitAgentPlanPreviewWindow::BuildCommitConfirmMessage(Plan);
	TestTrue(TEXT("confirm text should include total"), ConfirmText.Contains(TEXT("3")));
	TestTrue(TEXT("confirm text should include write count"), ConfirmText.Contains(TEXT("2")));
	TestTrue(TEXT("confirm text should include destructive count"), ConfirmText.Contains(TEXT("1")));
	TestTrue(TEXT("confirm text should include irreversible warning"), ConfirmText.Contains(TEXT("不可逆")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentPlanPreviewSearchPathEvidenceSummaryTest,
	"HCIAbilityKit.Editor.AgentPreviewUI.SearchPathEvidence.SummaryIncludesBestDirectoryAndSemanticFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentPlanPreviewSearchPathEvidenceSummaryTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitAgentExecutorStepResult SearchStep;
	SearchStep.StepIndex = 0;
	SearchStep.StepId = TEXT("step_1_search");
	SearchStep.ToolName = TEXT("SearchPath");
	SearchStep.Evidence.Add(TEXT("keyword"), TEXT("Temp"));
	SearchStep.Evidence.Add(TEXT("keyword_normalized"), TEXT("temp"));
	SearchStep.Evidence.Add(TEXT("keyword_expanded"), TEXT("Temp"));
	SearchStep.Evidence.Add(TEXT("matched_count"), TEXT("1"));
	SearchStep.Evidence.Add(TEXT("best_directory"), TEXT("/Game/Temp"));
	SearchStep.Evidence.Add(TEXT("semantic_fallback_used"), TEXT("true"));
	SearchStep.Evidence.Add(TEXT("semantic_fallback_directory"), TEXT("/Game/Temp"));

	TArray<FHCIAbilityKitAgentExecutorStepResult> StepResults;
	StepResults.Add(SearchStep);

	const FString Summary = FHCIAbilityKitAgentPlanPreviewWindow::BuildSearchPathEvidenceSummary(StepResults);
	TestTrue(TEXT("summary should include keyword"), Summary.Contains(TEXT("keyword=Temp")));
	TestTrue(TEXT("summary should include normalized"), Summary.Contains(TEXT("normalized=temp")));
	TestTrue(TEXT("summary should include best directory"), Summary.Contains(TEXT("best_directory=/Game/Temp")));
	TestTrue(TEXT("summary should include semantic fallback"), Summary.Contains(TEXT("semantic_fallback=true(/Game/Temp)")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentPlanPreviewSearchPathEvidenceNoStepTest,
	"HCIAbilityKit.Editor.AgentPreviewUI.SearchPathEvidence.SummaryReportsNoSearchStep",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentPlanPreviewSearchPathEvidenceNoStepTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitAgentExecutorStepResult ScanStep;
	ScanStep.StepIndex = 0;
	ScanStep.StepId = TEXT("step_1_scan");
	ScanStep.ToolName = TEXT("ScanAssets");

	TArray<FHCIAbilityKitAgentExecutorStepResult> StepResults;
	StepResults.Add(ScanStep);

	const FString Summary = FHCIAbilityKitAgentPlanPreviewWindow::BuildSearchPathEvidenceSummary(StepResults);
	TestTrue(TEXT("summary should report no search path step"), Summary.Contains(TEXT("本计划不含 SearchPath 步骤")));
	return true;
}

#endif
