#if WITH_DEV_AUTOMATION_TESTS

#include "Agent/HCIAbilityKitAgentPlan.h"
#include "Agent/HCIAbilityKitAgentPlanner.h"
#include "Agent/HCIAbilityKitAgentPlanValidator.h"
#include "Agent/HCIAbilityKitToolRegistry.h"
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
	Step.ExpectedEvidence = {TEXT("asset_path"), TEXT("before"), TEXT("after")};
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
	Step2.ExpectedEvidence = {TEXT("asset_path"), TEXT("before"), TEXT("after")};
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

#endif
