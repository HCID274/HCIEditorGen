#if WITH_DEV_AUTOMATION_TESTS

#include "Agent/HCIAbilityKitAgentExecutionGate.h"
#include "Agent/HCIAbilityKitToolRegistry.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutionGateBlocksUnconfirmedWriteTest,
	"HCIAbilityKit.Editor.AgentExec.ConfirmGateBlocksUnconfirmedWrite",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutionGateBlocksUnconfirmedWriteTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAgentExecutionStep Step;
	Step.StepId = TEXT("step_001");
	Step.ToolName = TEXT("RenameAsset");
	Step.bRequiresConfirm = true;
	Step.bUserConfirmed = false;

	const FHCIAbilityKitAgentExecutionDecision Decision = FHCIAbilityKitAgentExecutionGate::EvaluateConfirmGate(Step, Registry);

	TestFalse(TEXT("Unconfirmed write step should be blocked"), Decision.bAllowed);
	TestEqual(TEXT("Blocked step should use E4005"), Decision.ErrorCode, FString(TEXT("E4005")));
	TestEqual(TEXT("Blocked step should report tool name"), Decision.ToolName, FString(TEXT("RenameAsset")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutionGateAllowsReadOnlyWithoutConfirmTest,
	"HCIAbilityKit.Editor.AgentExec.ConfirmGateAllowsReadOnlyWithoutConfirm",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutionGateAllowsReadOnlyWithoutConfirmTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAgentExecutionStep Step;
	Step.StepId = TEXT("step_002");
	Step.ToolName = TEXT("ScanAssets");
	Step.bRequiresConfirm = false;
	Step.bUserConfirmed = false;

	const FHCIAbilityKitAgentExecutionDecision Decision = FHCIAbilityKitAgentExecutionGate::EvaluateConfirmGate(Step, Registry);

	TestTrue(TEXT("Read-only step should bypass confirm gate"), Decision.bAllowed);
	TestEqual(TEXT("Allowed step should have empty error code"), Decision.ErrorCode, FString());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutionGateAllowsConfirmedWriteTest,
	"HCIAbilityKit.Editor.AgentExec.ConfirmGateAllowsConfirmedWrite",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutionGateAllowsConfirmedWriteTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAgentExecutionStep Step;
	Step.StepId = TEXT("step_003");
	Step.ToolName = TEXT("SetTextureMaxSize");
	Step.bRequiresConfirm = true;
	Step.bUserConfirmed = true;

	const FHCIAbilityKitAgentExecutionDecision Decision = FHCIAbilityKitAgentExecutionGate::EvaluateConfirmGate(Step, Registry);

	TestTrue(TEXT("Confirmed write step should pass confirm gate"), Decision.bAllowed);
	TestEqual(TEXT("Allowed step should preserve capability=write"), Decision.Capability, FString(TEXT("write")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutionGateBlastRadiusBlocksOverLimitTest,
	"HCIAbilityKit.Editor.AgentExec.BlastRadiusBlocksOverLimit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutionGateBlastRadiusBlocksOverLimitTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAgentBlastRadiusCheckInput Input;
	Input.RequestId = TEXT("req_e4_001");
	Input.ToolName = TEXT("RenameAsset");
	Input.TargetModifyCount = 51;

	const FHCIAbilityKitAgentBlastRadiusDecision Decision = FHCIAbilityKitAgentExecutionGate::EvaluateBlastRadius(Input, Registry);

	TestFalse(TEXT("Write tool over limit should be blocked"), Decision.bAllowed);
	TestEqual(TEXT("Over-limit should return E4004"), Decision.ErrorCode, FString(TEXT("E4004")));
	TestEqual(TEXT("Limit should be frozen at 50"), Decision.MaxAssetModifyLimit, 50);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutionGateBlastRadiusAllowsAtLimitTest,
	"HCIAbilityKit.Editor.AgentExec.BlastRadiusAllowsAtLimit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutionGateBlastRadiusAllowsAtLimitTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAgentBlastRadiusCheckInput Input;
	Input.RequestId = TEXT("req_e4_002");
	Input.ToolName = TEXT("SetTextureMaxSize");
	Input.TargetModifyCount = 50;

	const FHCIAbilityKitAgentBlastRadiusDecision Decision = FHCIAbilityKitAgentExecutionGate::EvaluateBlastRadius(Input, Registry);

	TestTrue(TEXT("Write tool at limit should be allowed"), Decision.bAllowed);
	TestEqual(TEXT("Capability should be write"), Decision.Capability, FString(TEXT("write")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutionGateBlastRadiusSkipsReadOnlyToolTest,
	"HCIAbilityKit.Editor.AgentExec.BlastRadiusSkipsReadOnlyTool",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutionGateBlastRadiusSkipsReadOnlyToolTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAgentBlastRadiusCheckInput Input;
	Input.RequestId = TEXT("req_e4_003");
	Input.ToolName = TEXT("ScanAssets");
	Input.TargetModifyCount = 999;

	const FHCIAbilityKitAgentBlastRadiusDecision Decision = FHCIAbilityKitAgentExecutionGate::EvaluateBlastRadius(Input, Registry);

	TestTrue(TEXT("Read-only tool should not be blocked by blast radius"), Decision.bAllowed);
	TestEqual(TEXT("Capability should be read_only"), Decision.Capability, FString(TEXT("read_only")));
	TestFalse(TEXT("Read-only tool should not be write-like"), Decision.bWriteLike);

	return true;
}

#endif
