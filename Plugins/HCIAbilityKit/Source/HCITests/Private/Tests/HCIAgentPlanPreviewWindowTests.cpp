#if WITH_DEV_AUTOMATION_TESTS

#include "UI/HCIAgentPlanPreviewWindow.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"

namespace
{
static FHCIAgentPlan MakeBasePlan(const FString& RequestId)
{
	FHCIAgentPlan Plan;
	Plan.PlanVersion = 1;
	Plan.RequestId = RequestId;
	Plan.Intent = TEXT("normalize_temp_assets_by_metadata");
	return Plan;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentPlanPreviewRowsPendingStateTest,
	"HCI.Editor.AgentPreviewUI.Rows.PendingStateForPlaceholderStep",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentPlanPreviewRowsPendingStateTest::RunTest(const FString& Parameters)
{
	FHCIAgentPlan Plan = MakeBasePlan(TEXT("req_test_preview_pending"));
	FHCIAgentPlanStep& Step = Plan.Steps.AddDefaulted_GetRef();
	Step.StepId = TEXT("step_2_scan");
	Step.ToolName = TEXT("ScanAssets");
	Step.RiskLevel = EHCIAgentPlanRiskLevel::ReadOnly;
	Step.bRequiresConfirm = false;
	Step.RollbackStrategy = TEXT("all_or_nothing");
	Step.Args = MakeShared<FJsonObject>();
	Step.Args->SetStringField(TEXT("directory"), TEXT("{{step_1_search.matched_directories[0]}}"));

	const TArray<TSharedPtr<FHCIAgentPlanPreviewRow>> Rows = FHCIAgentPlanPreviewWindow::BuildRows(Plan);
	TestEqual(TEXT("rows count"), Rows.Num(), 1);
	if (Rows.Num() != 1 || !Rows[0].IsValid())
	{
		return false;
	}

	const TSharedPtr<FHCIAgentPlanPreviewRow>& Row = Rows[0];
	TestEqual(TEXT("asset count label should be pending"), Row->AssetCountLabel, FString(TEXT("? (Pending)")));
	TestFalse(TEXT("locate should be disabled for placeholder step"), Row->bLocateEnabled);
	TestEqual(TEXT("locate tooltip"), Row->LocateTooltip, FString(TEXT("等待前置步骤结果")));
	TestEqual(TEXT("step state"), Row->StepStateLabel, FString(TEXT("pending_inputs")));
	TestTrue(TEXT("args preview keeps placeholder"), Row->ArgsPreview.Contains(TEXT("{{step_1_search.matched_directories[0]}}")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentPlanPreviewRowsMissingAssetStateTest,
	"HCI.Editor.AgentPreviewUI.Rows.AssetMissingStateForInvalidAssetPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentPlanPreviewRowsMissingAssetStateTest::RunTest(const FString& Parameters)
{
	FHCIAgentPlan Plan = MakeBasePlan(TEXT("req_test_preview_missing_asset"));
	FHCIAgentPlanStep& Step = Plan.Steps.AddDefaulted_GetRef();
	Step.StepId = TEXT("step_3_rename");
	Step.ToolName = TEXT("RenameAsset");
	Step.RiskLevel = EHCIAgentPlanRiskLevel::Write;
	Step.bRequiresConfirm = true;
	Step.RollbackStrategy = TEXT("all_or_nothing");
	Step.Args = MakeShared<FJsonObject>();

	TArray<TSharedPtr<FJsonValue>> AssetPaths;
	AssetPaths.Add(MakeShared<FJsonValueString>(TEXT("/Game/__HCI_NotExists__/SM_NotExists.SM_NotExists")));
	Step.Args->SetArrayField(TEXT("asset_paths"), AssetPaths);
	Step.Args->SetStringField(TEXT("new_name"), TEXT("SM_Renamed"));

	const TArray<TSharedPtr<FHCIAgentPlanPreviewRow>> Rows = FHCIAgentPlanPreviewWindow::BuildRows(Plan);
	TestEqual(TEXT("rows count"), Rows.Num(), 1);
	if (Rows.Num() != 1 || !Rows[0].IsValid())
	{
		return false;
	}

	const TSharedPtr<FHCIAgentPlanPreviewRow>& Row = Rows[0];
	TestEqual(TEXT("asset count label should keep numeric count"), Row->AssetCountLabel, FString(TEXT("1")));
	TestFalse(TEXT("locate should be disabled for missing assets"), Row->bLocateEnabled);
	TestEqual(TEXT("locate tooltip"), Row->LocateTooltip, FString(TEXT("资产在路径下不存在")));
	TestEqual(TEXT("step state"), Row->StepStateLabel, FString(TEXT("asset_missing")));
	TestTrue(TEXT("args preview includes key new_name"), Row->ArgsPreview.Contains(TEXT("new_name")));
	TestTrue(TEXT("args preview includes value SM_Renamed"), Row->ArgsPreview.Contains(TEXT("SM_Renamed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentPlanPreviewCommitRiskSummaryTest,
	"HCI.Editor.AgentPreviewUI.CommitRiskSummary.CountsWriteAndDestructiveSteps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentPlanPreviewCommitRiskSummaryTest::RunTest(const FString& Parameters)
{
	FHCIAgentPlan Plan = MakeBasePlan(TEXT("req_test_preview_commit_summary"));

	{
		FHCIAgentPlanStep& ReadStep = Plan.Steps.AddDefaulted_GetRef();
		ReadStep.StepId = TEXT("step_1_scan");
		ReadStep.ToolName = TEXT("ScanAssets");
		ReadStep.RiskLevel = EHCIAgentPlanRiskLevel::ReadOnly;
	}

	{
		FHCIAgentPlanStep& WriteStep = Plan.Steps.AddDefaulted_GetRef();
		WriteStep.StepId = TEXT("step_2_rename");
		WriteStep.ToolName = TEXT("RenameAsset");
		WriteStep.RiskLevel = EHCIAgentPlanRiskLevel::Write;
		WriteStep.bRequiresConfirm = true;
	}

	{
		FHCIAgentPlanStep& DestructiveStep = Plan.Steps.AddDefaulted_GetRef();
		DestructiveStep.StepId = TEXT("step_3_move");
		DestructiveStep.ToolName = TEXT("MoveAsset");
		DestructiveStep.RiskLevel = EHCIAgentPlanRiskLevel::Destructive;
		DestructiveStep.bRequiresConfirm = true;
	}

	const FHCIAgentPlanCommitRiskSummary Summary = FHCIAgentPlanPreviewWindow::BuildCommitRiskSummary(Plan);
	TestEqual(TEXT("total steps"), Summary.TotalSteps, 3);
	TestEqual(TEXT("write-like steps"), Summary.WriteLikeSteps, 2);
	TestEqual(TEXT("destructive steps"), Summary.DestructiveSteps, 1);
	TestTrue(TEXT("should require confirm"), Summary.bRequiresConfirmDialog);

	const FString ConfirmText = FHCIAgentPlanPreviewWindow::BuildCommitConfirmMessage(Plan);
	TestTrue(TEXT("confirm text should include total"), ConfirmText.Contains(TEXT("3")));
	TestTrue(TEXT("confirm text should include write count"), ConfirmText.Contains(TEXT("2")));
	TestTrue(TEXT("confirm text should include destructive count"), ConfirmText.Contains(TEXT("1")));
	TestTrue(TEXT("confirm text should include irreversible warning"), ConfirmText.Contains(TEXT("不可逆")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentPlanPreviewSearchPathEvidenceSummaryTest,
	"HCI.Editor.AgentPreviewUI.SearchPathEvidence.SummaryIncludesBestDirectoryAndSemanticFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentPlanPreviewSearchPathEvidenceSummaryTest::RunTest(const FString& Parameters)
{
	FHCIAgentExecutorStepResult SearchStep;
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

	TArray<FHCIAgentExecutorStepResult> StepResults;
	StepResults.Add(SearchStep);

	const FString Summary = FHCIAgentPlanPreviewWindow::BuildSearchPathEvidenceSummary(StepResults);
	TestTrue(TEXT("summary should include keyword"), Summary.Contains(TEXT("keyword=Temp")));
	TestTrue(TEXT("summary should include normalized"), Summary.Contains(TEXT("normalized=temp")));
	TestTrue(TEXT("summary should include best directory"), Summary.Contains(TEXT("best_directory=/Game/Temp")));
	TestTrue(TEXT("summary should include semantic fallback"), Summary.Contains(TEXT("semantic_fallback=true(/Game/Temp)")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentPlanPreviewSearchPathEvidenceNoStepTest,
	"HCI.Editor.AgentPreviewUI.SearchPathEvidence.SummaryReportsNoSearchStep",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentPlanPreviewSearchPathEvidenceNoStepTest::RunTest(const FString& Parameters)
{
	FHCIAgentExecutorStepResult ScanStep;
	ScanStep.StepIndex = 0;
	ScanStep.StepId = TEXT("step_1_scan");
	ScanStep.ToolName = TEXT("ScanAssets");

	TArray<FHCIAgentExecutorStepResult> StepResults;
	StepResults.Add(ScanStep);

	const FString Summary = FHCIAgentPlanPreviewWindow::BuildSearchPathEvidenceSummary(StepResults);
	TestTrue(TEXT("summary should report no search path step"), Summary.Contains(TEXT("本计划不含 SearchPath 步骤")));
	return true;
}

#endif

