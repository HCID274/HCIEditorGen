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

#endif
