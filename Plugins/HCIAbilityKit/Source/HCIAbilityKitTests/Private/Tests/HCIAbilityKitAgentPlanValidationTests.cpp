#if WITH_DEV_AUTOMATION_TESTS

#include "Agent/Planner/HCIAbilityKitAgentPlan.h"
#include "Agent/Planner/HCIAbilityKitAgentPlanner.h"
#include "Agent/Planner/HCIAbilityKitAgentPlanValidator.h"
#include "Agent/Tools/HCIAbilityKitToolRegistry.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"

namespace
{
static TArray<TSharedPtr<FJsonValue>> HCI_MakeStringArray(std::initializer_list<const TCHAR*> Values)
{
	TArray<TSharedPtr<FJsonValue>> Out;
	for (const TCHAR* Value : Values)
	{
		Out.Add(MakeShared<FJsonValueString>(Value));
	}
	return Out;
}

static FHCIAbilityKitAgentPlan MakeValidTexturePlan()
{
	FHCIAbilityKitAgentPlan Plan;
	Plan.PlanVersion = 1;
	Plan.RequestId = TEXT("req_f2_test");
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
	Step.Args->SetArrayField(TEXT("asset_paths"), HCI_MakeStringArray({TEXT("/Game/Art/T_Test.T_Test")}));
	Step.Args->SetNumberField(TEXT("max_size"), 1024);
	return Plan;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentPlanValidatorAcceptsValidPlanTest,
	"HCIAbilityKit.Editor.AgentPlanValidation.AcceptsValidPlan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentPlanValidatorAcceptsValidPlanTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAgentPlan Plan = MakeValidTexturePlan();
	FHCIAbilityKitAgentPlanValidationResult Result;

	TestTrue(TEXT("Valid plan should pass validator"), FHCIAbilityKitAgentPlanValidator::ValidatePlan(Plan, Registry, Result));
	TestTrue(TEXT("Result should be valid"), Result.bValid);
	TestEqual(TEXT("Validated step count"), Result.ValidatedStepCount, 1);
	TestEqual(TEXT("Write-like step count"), Result.WriteLikeStepCount, 1);
	TestEqual(TEXT("Total target modify count"), Result.TotalTargetModifyCount, 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentPlanValidatorMissingRequiredFieldTest,
	"HCIAbilityKit.Editor.AgentPlanValidation.MissingRequiredFieldReturnsE4001",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentPlanValidatorMissingRequiredFieldTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAgentPlan Plan = MakeValidTexturePlan();
	Plan.Steps[0].ToolName = NAME_None;

	FHCIAbilityKitAgentPlanValidationResult Result;
	TestFalse(TEXT("Missing tool_name should fail"), FHCIAbilityKitAgentPlanValidator::ValidatePlan(Plan, Registry, Result));
	TestFalse(TEXT("Result should be invalid"), Result.bValid);
	TestEqual(TEXT("Error code"), Result.ErrorCode, FString(TEXT("E4001")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentPlanValidatorUnknownToolTest,
	"HCIAbilityKit.Editor.AgentPlanValidation.UnknownToolReturnsE4002",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentPlanValidatorUnknownToolTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAgentPlan Plan = MakeValidTexturePlan();
	Plan.Steps[0].ToolName = TEXT("UnknownTool_X");

	FHCIAbilityKitAgentPlanValidationResult Result;
	TestFalse(TEXT("Unknown tool should fail"), FHCIAbilityKitAgentPlanValidator::ValidatePlan(Plan, Registry, Result));
	TestEqual(TEXT("Error code"), Result.ErrorCode, FString(TEXT("E4002")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentPlanValidatorInvalidEnumReturnsE4009Test,
	"HCIAbilityKit.Editor.AgentPlanValidation.InvalidEnumReturnsE4009",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentPlanValidatorInvalidEnumReturnsE4009Test::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAgentPlan Plan = MakeValidTexturePlan();
	Plan.Steps[0].Args->SetNumberField(TEXT("max_size"), 123);

	FHCIAbilityKitAgentPlanValidationResult Result;
	TestFalse(TEXT("Invalid enum should fail"), FHCIAbilityKitAgentPlanValidator::ValidatePlan(Plan, Registry, Result));
	TestEqual(TEXT("Error code"), Result.ErrorCode, FString(TEXT("E4009")));
	TestTrue(TEXT("Field should mention max_size"), Result.Field.Contains(TEXT("max_size")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentPlanValidatorOverLimitReturnsE4004Test,
	"HCIAbilityKit.Editor.AgentPlanValidation.OverModifyLimitReturnsE4004",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentPlanValidatorOverLimitReturnsE4004Test::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAgentPlan Plan = MakeValidTexturePlan();
	TArray<TSharedPtr<FJsonValue>> AssetPathsA;
	TArray<TSharedPtr<FJsonValue>> AssetPathsB;
	for (int32 Index = 0; Index < 50; ++Index)
	{
		AssetPathsA.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("/Game/Temp/TA_%03d.TA_%03d"), Index, Index)));
		AssetPathsB.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("/Game/Temp/TB_%03d.TB_%03d"), Index, Index)));
	}
	Plan.Steps[0].Args->SetArrayField(TEXT("asset_paths"), AssetPathsA);

	FHCIAbilityKitAgentPlanStep& Step2 = Plan.Steps.AddDefaulted_GetRef();
	Step2.StepId = TEXT("s2");
		Step2.ToolName = TEXT("SetTextureMaxSize");
		Step2.RiskLevel = EHCIAbilityKitAgentPlanRiskLevel::Write;
		Step2.bRequiresConfirm = true;
		Step2.RollbackStrategy = TEXT("all_or_nothing");
		Step2.ExpectedEvidence = {
			TEXT("target_max_size"),
			TEXT("scanned_count"),
			TEXT("modified_count"),
			TEXT("failed_count"),
			TEXT("modified_assets"),
			TEXT("failed_assets"),
			TEXT("result")};
	Step2.Args = MakeShared<FJsonObject>();
	Step2.Args->SetArrayField(TEXT("asset_paths"), AssetPathsB);
	Step2.Args->SetNumberField(TEXT("max_size"), 1024);

	FHCIAbilityKitAgentPlanValidationResult Result;
	TestFalse(TEXT("Over limit write plan should fail"), FHCIAbilityKitAgentPlanValidator::ValidatePlan(Plan, Registry, Result));
	TestEqual(TEXT("Error code"), Result.ErrorCode, FString(TEXT("E4004")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentPlanValidatorLevelRiskInvalidScopeReturnsE4011Test,
	"HCIAbilityKit.Editor.AgentPlanValidation.LevelRiskInvalidScopeReturnsE4011",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentPlanValidatorLevelRiskInvalidScopeReturnsE4011Test::RunTest(const FString& Parameters)
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
			TEXT("req_f2_level"),
			Registry,
			Plan,
			RouteReason,
			Error));

	Plan.Steps[0].Args->SetStringField(TEXT("scope"), TEXT("world"));

	FHCIAbilityKitAgentPlanValidationResult Result;
	TestFalse(TEXT("Invalid level risk scope should fail"), FHCIAbilityKitAgentPlanValidator::ValidatePlan(Plan, Registry, Result));
	TestEqual(TEXT("Error code"), Result.ErrorCode, FString(TEXT("E4011")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentPlanValidatorLevelRiskDuplicateChecksReturnsE4011Test,
	"HCIAbilityKit.Editor.AgentPlanValidation.LevelRiskDuplicateChecksReturnsE4011",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentPlanValidatorLevelRiskDuplicateChecksReturnsE4011Test::RunTest(const FString& Parameters)
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
			TEXT("req_f2_level_dup"),
			Registry,
			Plan,
			RouteReason,
			Error));

	TArray<TSharedPtr<FJsonValue>> DuplicateChecks;
	DuplicateChecks.Add(MakeShared<FJsonValueString>(TEXT("missing_collision")));
	DuplicateChecks.Add(MakeShared<FJsonValueString>(TEXT("missing_collision")));
	Plan.Steps[0].Args->SetArrayField(TEXT("checks"), DuplicateChecks);

	FHCIAbilityKitAgentPlanValidationResult Result;
	TestFalse(TEXT("Duplicate checks should fail for subset-of-enum arg"), FHCIAbilityKitAgentPlanValidator::ValidatePlan(Plan, Registry, Result));
	TestEqual(TEXT("Error code"), Result.ErrorCode, FString(TEXT("E4011")));
	TestTrue(TEXT("Field should mention checks"), Result.Field.Contains(TEXT("checks")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentPlanValidatorNamingMetadataInsufficientReturnsE4012Test,
	"HCIAbilityKit.Editor.AgentPlanValidation.NamingMetadataInsufficientReturnsE4012",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentPlanValidatorNamingMetadataInsufficientReturnsE4012Test::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAgentPlan Plan;
	FString RouteReason;
	FString Error;
	TestTrue(
		TEXT("Planner should build naming plan"),
		FHCIAbilityKitAgentPlanner::BuildPlanFromNaturalLanguage(
			TEXT("整理临时目录资产，按规范命名并归档"),
			TEXT("req_f2_naming"),
			Registry,
			Plan,
			RouteReason,
			Error));

	FHCIAbilityKitAgentPlanValidationContext Context;
	Context.MockMetadataUnavailableAssetPaths.Add(TEXT("/Game/Temp/SM_RockTemp_01.SM_RockTemp_01"));

	FHCIAbilityKitAgentPlanValidationResult Result;
	TestFalse(TEXT("Metadata insufficient mock should fail"), FHCIAbilityKitAgentPlanValidator::ValidatePlan(Plan, Registry, Context, Result));
	TestEqual(TEXT("Error code"), Result.ErrorCode, FString(TEXT("E4012")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentPlanValidatorVariableSourceMustPrecedeConsumerTest,
	"HCIAbilityKit.Editor.AgentPlanValidation.VariableSourceMustPrecedeConsumer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentPlanValidatorVariableSourceMustPrecedeConsumerTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAgentPlan Plan;
	Plan.PlanVersion = 1;
	Plan.RequestId = TEXT("req_f2_variable_order");
	Plan.Intent = TEXT("scan_assets");

	FHCIAbilityKitAgentPlanStep& ScanStep = Plan.Steps.AddDefaulted_GetRef();
	ScanStep.StepId = TEXT("step_2_scan");
	ScanStep.ToolName = TEXT("ScanAssets");
	ScanStep.RiskLevel = EHCIAbilityKitAgentPlanRiskLevel::ReadOnly;
	ScanStep.bRequiresConfirm = false;
	ScanStep.RollbackStrategy = TEXT("all_or_nothing");
	ScanStep.ExpectedEvidence = {TEXT("asset_paths"), TEXT("asset_count"), TEXT("result")};
	ScanStep.Args = MakeShared<FJsonObject>();
	ScanStep.Args->SetStringField(TEXT("directory"), TEXT("{{step_1_search.matched_directories[0]}}"));

	FHCIAbilityKitAgentPlanStep& SearchStep = Plan.Steps.AddDefaulted_GetRef();
	SearchStep.StepId = TEXT("step_1_search");
	SearchStep.ToolName = TEXT("SearchPath");
	SearchStep.RiskLevel = EHCIAbilityKitAgentPlanRiskLevel::ReadOnly;
	SearchStep.bRequiresConfirm = false;
	SearchStep.RollbackStrategy = TEXT("all_or_nothing");
	SearchStep.ExpectedEvidence = {TEXT("matched_directories"), TEXT("best_directory"), TEXT("result")};
	SearchStep.Args = MakeShared<FJsonObject>();
	SearchStep.Args->SetStringField(TEXT("keyword"), TEXT("MNew"));

	FHCIAbilityKitAgentPlanValidationResult Result;
	TestFalse(
		TEXT("Consumer step should fail when source step appears later"),
		FHCIAbilityKitAgentPlanValidator::ValidatePlan(Plan, Registry, Result));
	TestEqual(TEXT("Error code"), Result.ErrorCode, FString(TEXT("E4009")));
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("variable_source_step_must_precede_consumer")));
	TestTrue(TEXT("Field should point to directory"), Result.Field.Contains(TEXT("steps[0].args.directory")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentPlanValidatorMissingExpectedEvidenceReturnsE4001Test,
	"HCIAbilityKit.Editor.AgentPlanValidation.MissingExpectedEvidenceReturnsE4001",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentPlanValidatorMissingExpectedEvidenceReturnsE4001Test::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAgentPlan Plan = MakeValidTexturePlan();
	Plan.Steps[0].ExpectedEvidence.Reset();

	FHCIAbilityKitAgentPlanValidationResult Result;
	TestFalse(TEXT("Missing expected_evidence should fail"), FHCIAbilityKitAgentPlanValidator::ValidatePlan(Plan, Registry, Result));
	TestEqual(TEXT("Error code"), Result.ErrorCode, FString(TEXT("E4001")));
	TestTrue(TEXT("Reason should mention expected_evidence"), Result.Reason.Contains(TEXT("expected_evidence")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentPlanValidatorExpectedEvidenceNotAllowedReturnsE4009Test,
	"HCIAbilityKit.Editor.AgentPlanValidation.ExpectedEvidenceNotAllowedReturnsE4009",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentPlanValidatorExpectedEvidenceNotAllowedReturnsE4009Test::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAgentPlan Plan;
	Plan.PlanVersion = 1;
	Plan.RequestId = TEXT("req_expected_evidence_illegal_key");
	Plan.Intent = TEXT("inspect_folder_contents");

	FHCIAbilityKitAgentPlanStep& Step = Plan.Steps.AddDefaulted_GetRef();
	Step.StepId = TEXT("s1");
	Step.ToolName = TEXT("ScanAssets");
	Step.RiskLevel = EHCIAbilityKitAgentPlanRiskLevel::ReadOnly;
	Step.bRequiresConfirm = false;
	Step.RollbackStrategy = TEXT("all_or_nothing");
	Step.ExpectedEvidence = {TEXT("unknown_key")};
	Step.Args = MakeShared<FJsonObject>();
	Step.Args->SetStringField(TEXT("directory"), TEXT("/Game/Temp"));

	FHCIAbilityKitAgentPlanValidationResult Result;
	TestFalse(TEXT("Unexpected expected_evidence value should fail"), FHCIAbilityKitAgentPlanValidator::ValidatePlan(Plan, Registry, Result));
	TestEqual(TEXT("Error code"), Result.ErrorCode, FString(TEXT("E4009")));
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("Illegal evidence key \"unknown_key\" for tool \"ScanAssets\".")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentPlanValidatorModifyIntentRequiresWriteStepTest,
	"HCIAbilityKit.Editor.AgentPlanValidation.ModifyIntentRequiresWriteStepAtPlanReady",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentPlanValidatorModifyIntentRequiresWriteStepTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAgentPlan Plan;
	Plan.PlanVersion = 1;
	Plan.RequestId = TEXT("req_modify_intent_read_only_only");
	Plan.Intent = TEXT("batch_fix_asset_compliance");

	FHCIAbilityKitAgentPlanStep& Step = Plan.Steps.AddDefaulted_GetRef();
	Step.StepId = TEXT("step_1_scan");
	Step.ToolName = TEXT("ScanAssets");
	Step.RiskLevel = EHCIAbilityKitAgentPlanRiskLevel::ReadOnly;
	Step.bRequiresConfirm = false;
	Step.RollbackStrategy = TEXT("all_or_nothing");
	Step.ExpectedEvidence = {TEXT("scan_root"), TEXT("asset_count"), TEXT("asset_paths"), TEXT("result")};
	Step.Args = MakeShared<FJsonObject>();
	Step.Args->SetStringField(TEXT("directory"), TEXT("/Game/__HCI_Auto"));

	FHCIAbilityKitAgentPlanValidationContext Context;
	Context.bRequireWriteStepForModifyIntent = true;

	FHCIAbilityKitAgentPlanValidationResult Result;
	TestFalse(TEXT("Modify intent without write step should fail"), FHCIAbilityKitAgentPlanValidator::ValidatePlan(Plan, Registry, Context, Result));
	TestEqual(TEXT("Error code"), Result.ErrorCode, FString(TEXT("E4009")));
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("modify_intent_requires_write_step")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentPlanValidatorPipelineInputRequiredForWriteArgTest,
	"HCIAbilityKit.Editor.AgentPlanValidation.WriteArgRequiresPipelineInputAtPlanReady",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentPlanValidatorPipelineInputRequiredForWriteArgTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAgentPlan Plan = MakeValidTexturePlan();

	FHCIAbilityKitAgentPlanValidationContext Context;
	Context.bRequirePipelineInputs = true;

	FHCIAbilityKitAgentPlanValidationResult Result;
	TestFalse(TEXT("Write arg should fail when asset_paths is not pipeline variable"), FHCIAbilityKitAgentPlanValidator::ValidatePlan(Plan, Registry, Context, Result));
	TestEqual(TEXT("Error code"), Result.ErrorCode, FString(TEXT("E4009")));
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("pipeline_input_required_for_arg")));
	TestTrue(TEXT("Field should mention asset_paths"), Result.Field.Contains(TEXT("asset_paths")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentPlanValidatorPipelineInputPassesForScanToWriteChainTest,
	"HCIAbilityKit.Editor.AgentPlanValidation.WriteArgPipelineInputPassesForScanChain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentPlanValidatorPipelineInputPassesForScanToWriteChainTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAgentPlan Plan;
	Plan.PlanVersion = 1;
	Plan.RequestId = TEXT("req_pipeline_scan_to_write_ok");
	Plan.Intent = TEXT("batch_fix_asset_compliance");

	FHCIAbilityKitAgentPlanStep& ScanStep = Plan.Steps.AddDefaulted_GetRef();
	ScanStep.StepId = TEXT("step_1_scan");
	ScanStep.ToolName = TEXT("ScanAssets");
	ScanStep.RiskLevel = EHCIAbilityKitAgentPlanRiskLevel::ReadOnly;
	ScanStep.bRequiresConfirm = false;
	ScanStep.RollbackStrategy = TEXT("all_or_nothing");
	ScanStep.ExpectedEvidence = {TEXT("scan_root"), TEXT("asset_count"), TEXT("asset_paths"), TEXT("result")};
	ScanStep.Args = MakeShared<FJsonObject>();
	ScanStep.Args->SetStringField(TEXT("directory"), TEXT("/Game/__HCI_Auto"));

	FHCIAbilityKitAgentPlanStep& WriteStep = Plan.Steps.AddDefaulted_GetRef();
	WriteStep.StepId = TEXT("step_2_set_max_size");
	WriteStep.ToolName = TEXT("SetTextureMaxSize");
	WriteStep.RiskLevel = EHCIAbilityKitAgentPlanRiskLevel::Write;
	WriteStep.bRequiresConfirm = true;
	WriteStep.RollbackStrategy = TEXT("all_or_nothing");
	WriteStep.ExpectedEvidence = {
		TEXT("target_max_size"),
		TEXT("scanned_count"),
		TEXT("modified_count"),
		TEXT("failed_count"),
		TEXT("modified_assets"),
		TEXT("failed_assets"),
		TEXT("result")};
	WriteStep.Args = MakeShared<FJsonObject>();
	WriteStep.Args->SetArrayField(TEXT("asset_paths"), HCI_MakeStringArray({TEXT("{{step_1_scan.asset_paths}}")}));
	WriteStep.Args->SetNumberField(TEXT("max_size"), 1024);

	FHCIAbilityKitAgentPlanValidationContext Context;
	Context.bRequireWriteStepForModifyIntent = true;
	Context.bRequirePipelineInputs = true;

	FHCIAbilityKitAgentPlanValidationResult Result;
	TestTrue(TEXT("Scan->write chain should pass plan-ready constraints"), FHCIAbilityKitAgentPlanValidator::ValidatePlan(Plan, Registry, Context, Result));
	TestTrue(TEXT("Result should be valid"), Result.bValid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentPlanValidatorUiPresentationEmptySummaryRejectedTest,
	"HCIAbilityKit.Editor.AgentPlanValidation.UiPresentationEmptySummaryRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentPlanValidatorUiPresentationEmptySummaryRejectedTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAgentPlan Plan = MakeValidTexturePlan();
	Plan.Steps[0].UiPresentation.bHasStepSummary = true;
	Plan.Steps[0].UiPresentation.StepSummary = TEXT("   ");

	FHCIAbilityKitAgentPlanValidationResult Result;
	TestFalse(TEXT("Empty ui_presentation.step_summary should fail"), FHCIAbilityKitAgentPlanValidator::ValidatePlan(Plan, Registry, Result));
	TestEqual(TEXT("Error code"), Result.ErrorCode, FString(TEXT("E4009")));
	TestTrue(TEXT("Field should mention ui_presentation.step_summary"), Result.Field.Contains(TEXT("ui_presentation.step_summary")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentPlanValidatorUiPresentationLongRiskWarningRejectedTest,
	"HCIAbilityKit.Editor.AgentPlanValidation.UiPresentationRiskWarningTooLongRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentPlanValidatorUiPresentationLongRiskWarningRejectedTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAgentPlan Plan = MakeValidTexturePlan();
	Plan.Steps[0].UiPresentation.bHasRiskWarning = true;
	Plan.Steps[0].UiPresentation.RiskWarning = FString::ChrN(201, TEXT('W'));

	FHCIAbilityKitAgentPlanValidationResult Result;
	TestFalse(TEXT("Overlong ui_presentation.risk_warning should fail"), FHCIAbilityKitAgentPlanValidator::ValidatePlan(Plan, Registry, Result));
	TestEqual(TEXT("Error code"), Result.ErrorCode, FString(TEXT("E4009")));
	TestTrue(TEXT("Field should mention ui_presentation.risk_warning"), Result.Field.Contains(TEXT("ui_presentation.risk_warning")));
	return true;
}

#endif
