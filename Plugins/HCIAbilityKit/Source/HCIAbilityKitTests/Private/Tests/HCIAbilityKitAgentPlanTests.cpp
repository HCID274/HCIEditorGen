#if WITH_DEV_AUTOMATION_TESTS

#include "Agent/Planner/HCIAbilityKitAgentPlan.h"
#include "Agent/Planner/HCIAbilityKitAgentPlanJsonSerializer.h"
#include "Agent/Planner/HCIAbilityKitAgentPlanner.h"
#include "Agent/Tools/HCIAbilityKitToolRegistry.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"

namespace
{
static const FHCIAbilityKitAgentPlanStep* FindStepByToolName(const FHCIAbilityKitAgentPlan& Plan, const TCHAR* ToolName)
{
	return Plan.Steps.FindByPredicate(
		[ToolName](const FHCIAbilityKitAgentPlanStep& Step)
		{
			return Step.ToolName == FName(ToolName);
		});
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentPlanNamingIntentTest,
	"HCIAbilityKit.Editor.AgentPlan.PlannerSupportsNamingArchiveIntent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentPlanNamingIntentTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAgentPlan Plan;
	FString RouteReason;
	FString Error;
	const bool bOk = FHCIAbilityKitAgentPlanner::BuildPlanFromNaturalLanguage(
		TEXT("整理临时目录资产，按规范命名并归档"),
		TEXT("req_f1_001"),
		Registry,
		Plan,
		RouteReason,
		Error);

	TestTrue(TEXT("Planner should support naming/archive intent"), bOk);
	TestEqual(TEXT("Planner route should be naming"), RouteReason, FString(TEXT("naming_traceability_temp_assets")));
	TestEqual(TEXT("Intent should be naming traceability"), Plan.Intent, FString(TEXT("normalize_temp_assets_by_metadata")));
	TestEqual(TEXT("Plan version should be frozen to 1"), Plan.PlanVersion, 1);

	const FHCIAbilityKitAgentPlanStep* Step = FindStepByToolName(Plan, TEXT("NormalizeAssetNamingByMetadata"));
	TestNotNull(TEXT("Naming plan should contain NormalizeAssetNamingByMetadata"), Step);
	if (Step && Step->Args.IsValid())
	{
		FString MetadataSource;
		FString PrefixMode;
		FString TargetRoot;
		TestTrue(TEXT("metadata_source should exist"), Step->Args->TryGetStringField(TEXT("metadata_source"), MetadataSource));
		TestTrue(TEXT("prefix_mode should exist"), Step->Args->TryGetStringField(TEXT("prefix_mode"), PrefixMode));
		TestTrue(TEXT("target_root should exist"), Step->Args->TryGetStringField(TEXT("target_root"), TargetRoot));
		TestEqual(TEXT("metadata_source should be auto"), MetadataSource, FString(TEXT("auto")));
		TestEqual(TEXT("prefix_mode should be auto_by_asset_class"), PrefixMode, FString(TEXT("auto_by_asset_class")));
		TestTrue(TEXT("target_root should start with /Game/"), TargetRoot.StartsWith(TEXT("/Game/")));
		TestEqual(
			TEXT("Write intent should require confirm"),
			Step->bRequiresConfirm,
			true);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentPlanLevelRiskIntentTest,
	"HCIAbilityKit.Editor.AgentPlan.PlannerSupportsLevelRiskIntent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentPlanLevelRiskIntentTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAgentPlan Plan;
	FString RouteReason;
	FString Error;
	const bool bOk = FHCIAbilityKitAgentPlanner::BuildPlanFromNaturalLanguage(
		TEXT("检查当前关卡选中物体的碰撞和默认材质"),
		TEXT("req_f1_002"),
		Registry,
		Plan,
		RouteReason,
		Error);

	TestTrue(TEXT("Planner should support level risk intent"), bOk);
	TestEqual(TEXT("Planner route should be level risk"), RouteReason, FString(TEXT("level_risk_collision_material")));

	const FHCIAbilityKitAgentPlanStep* Step = FindStepByToolName(Plan, TEXT("ScanLevelMeshRisks"));
	TestNotNull(TEXT("Level risk plan should contain ScanLevelMeshRisks"), Step);
	if (Step && Step->Args.IsValid())
	{
		TestEqual(
			TEXT("Risk level should map to read_only"),
			FHCIAbilityKitAgentPlanContract::RiskLevelToString(Step->RiskLevel),
			FString(TEXT("read_only")));
		TestFalse(TEXT("Read-only scan should not require confirm"), Step->bRequiresConfirm);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentPlanMeshTriangleCountIntentTest,
	"HCIAbilityKit.Editor.AgentPlan.PlannerSupportsMeshTriangleCountIntent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentPlanMeshTriangleCountIntentTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAgentPlan Plan;
	FString RouteReason;
	FString Error;
	const bool bOk = FHCIAbilityKitAgentPlanner::BuildPlanFromNaturalLanguage(
		TEXT("检查一下 /Game/HCI 目录下的模型面数"),
		TEXT("req_f1_mesh_triangle_scan"),
		Registry,
		Plan,
		RouteReason,
		Error);

	TestTrue(TEXT("Planner should support mesh triangle count intent"), bOk);
	TestEqual(TEXT("Planner route should be mesh triangle count"), RouteReason, FString(TEXT("mesh_triangle_count_analysis")));
	TestEqual(TEXT("Intent should be mesh triangle count"), Plan.Intent, FString(TEXT("scan_mesh_triangle_count")));
	TestEqual(TEXT("Plan should contain a single read-only step"), Plan.Steps.Num(), 1);

	const FHCIAbilityKitAgentPlanStep* Step = FindStepByToolName(Plan, TEXT("ScanMeshTriangleCount"));
	TestNotNull(TEXT("Mesh triangle plan should contain ScanMeshTriangleCount"), Step);
	if (Step && Step->Args.IsValid())
	{
		FString Directory;
		TestTrue(TEXT("directory should exist"), Step->Args->TryGetStringField(TEXT("directory"), Directory));
		TestEqual(TEXT("directory should be /Game/HCI"), Directory, FString(TEXT("/Game/HCI")));
		TestEqual(
			TEXT("Read-only scan should not require confirm"),
			Step->bRequiresConfirm,
			false);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentPlanMinimalContractAllowsAssistantMessageOnlyTest,
	"HCIAbilityKit.Editor.AgentPlan.ContractAllowsAssistantMessageOnlyPlan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentPlanMinimalContractAllowsAssistantMessageOnlyTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitAgentPlan Plan;
	Plan.PlanVersion = 1;
	Plan.RequestId = TEXT("req_contract_text_only_01");
	Plan.Intent = TEXT("chat_answer_only");
	Plan.AssistantMessage = TEXT("这是一个无需工具执行的直接回复。");

	FString Error;
	TestTrue(TEXT("Minimal contract should allow assistant_message-only plan"), FHCIAbilityKitAgentPlanContract::ValidateMinimalContract(Plan, Error));
	TestTrue(TEXT("Contract error should be empty"), Error.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentPlanJsonSerializerCoreFieldsTest,
	"HCIAbilityKit.Editor.AgentPlan.JsonSerializerIncludesCoreContractFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentPlanJsonSerializerCoreFieldsTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAgentPlan Plan;
	FString RouteReason;
	FString Error;
	TestTrue(
		TEXT("Planner should build an asset-compliance plan"),
		FHCIAbilityKitAgentPlanner::BuildPlanFromNaturalLanguage(
			TEXT("检查贴图分辨率并处理LOD"),
			TEXT("req_f1_003"),
			Registry,
			Plan,
			RouteReason,
			Error));

	FString JsonText;
	TestTrue(TEXT("Serializer should succeed"), FHCIAbilityKitAgentPlanJsonSerializer::SerializeToJsonString(Plan, JsonText));

	TestTrue(TEXT("JSON should include plan_version"), JsonText.Contains(TEXT("\"plan_version\"")));
	TestTrue(TEXT("JSON should include request_id"), JsonText.Contains(TEXT("\"request_id\"")));
	TestTrue(TEXT("JSON should include intent"), JsonText.Contains(TEXT("\"intent\"")));
	TestTrue(TEXT("JSON should include steps"), JsonText.Contains(TEXT("\"steps\"")));
	TestTrue(TEXT("JSON should include tool_name"), JsonText.Contains(TEXT("\"tool_name\"")));
	TestTrue(TEXT("JSON should include args"), JsonText.Contains(TEXT("\"args\"")));
	TestTrue(TEXT("JSON should include risk_level"), JsonText.Contains(TEXT("\"risk_level\"")));
	TestTrue(TEXT("JSON should include requires_confirm"), JsonText.Contains(TEXT("\"requires_confirm\"")));
	TestTrue(TEXT("JSON should include rollback_strategy"), JsonText.Contains(TEXT("\"rollback_strategy\"")));
	TestTrue(TEXT("JSON should include expected_evidence"), JsonText.Contains(TEXT("\"expected_evidence\"")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentPlanJsonSerializerAssistantMessageTest,
	"HCIAbilityKit.Editor.AgentPlan.JsonSerializerIncludesAssistantMessageWhenPresent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentPlanJsonSerializerAssistantMessageTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitAgentPlan Plan;
	Plan.PlanVersion = 1;
	Plan.RequestId = TEXT("req_contract_text_only_serialize_01");
	Plan.Intent = TEXT("chat_answer_only");
	Plan.AssistantMessage = TEXT("这是一条直接回复。");

	FString JsonText;
	TestTrue(TEXT("Serializer should succeed"), FHCIAbilityKitAgentPlanJsonSerializer::SerializeToJsonString(Plan, JsonText));
	TestTrue(TEXT("JSON should include assistant_message"), JsonText.Contains(TEXT("\"assistant_message\"")));
	return true;
}

#endif
