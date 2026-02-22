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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutionGateTransactionCommitsAllStepsOnSuccessTest,
	"HCIAbilityKit.Editor.AgentExec.TransactionCommitsAllStepsOnSuccess",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutionGateTransactionCommitsAllStepsOnSuccessTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAgentTransactionExecutionInput Input;
	Input.RequestId = TEXT("req_e5_001");

	FHCIAbilityKitAgentTransactionStepSimulation& Step1 = Input.Steps.AddDefaulted_GetRef();
	Step1.StepId = TEXT("step_001");
	Step1.ToolName = TEXT("RenameAsset");
	Step1.bShouldSucceed = true;

	FHCIAbilityKitAgentTransactionStepSimulation& Step2 = Input.Steps.AddDefaulted_GetRef();
	Step2.StepId = TEXT("step_002");
	Step2.ToolName = TEXT("MoveAsset");
	Step2.bShouldSucceed = true;

	const FHCIAbilityKitAgentTransactionExecutionDecision Decision =
		FHCIAbilityKitAgentExecutionGate::EvaluateAllOrNothingTransaction(Input, Registry);

	TestTrue(TEXT("All-success transaction should commit"), Decision.bCommitted);
	TestFalse(TEXT("All-success transaction should not roll back"), Decision.bRolledBack);
	TestEqual(TEXT("Committed steps should equal total steps"), Decision.CommittedSteps, 2);
	TestEqual(TEXT("Rolled back steps should be zero"), Decision.RolledBackSteps, 0);
	TestEqual(TEXT("Transaction mode should be all_or_nothing"), Decision.TransactionMode, FString(TEXT("all_or_nothing")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutionGateTransactionRollsBackOnFailureTest,
	"HCIAbilityKit.Editor.AgentExec.TransactionRollsBackOnFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutionGateTransactionRollsBackOnFailureTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAgentTransactionExecutionInput Input;
	Input.RequestId = TEXT("req_e5_002");

	FHCIAbilityKitAgentTransactionStepSimulation& Step1 = Input.Steps.AddDefaulted_GetRef();
	Step1.StepId = TEXT("step_001");
	Step1.ToolName = TEXT("RenameAsset");
	Step1.bShouldSucceed = true;

	FHCIAbilityKitAgentTransactionStepSimulation& Step2 = Input.Steps.AddDefaulted_GetRef();
	Step2.StepId = TEXT("step_002");
	Step2.ToolName = TEXT("MoveAsset");
	Step2.bShouldSucceed = false;

	FHCIAbilityKitAgentTransactionStepSimulation& Step3 = Input.Steps.AddDefaulted_GetRef();
	Step3.StepId = TEXT("step_003");
	Step3.ToolName = TEXT("SetTextureMaxSize");
	Step3.bShouldSucceed = true;

	const FHCIAbilityKitAgentTransactionExecutionDecision Decision =
		FHCIAbilityKitAgentExecutionGate::EvaluateAllOrNothingTransaction(Input, Registry);

	TestFalse(TEXT("Failure should prevent commit"), Decision.bCommitted);
	TestTrue(TEXT("Failure should trigger rollback semantics"), Decision.bRolledBack);
	TestEqual(TEXT("Transaction failure should use E4007"), Decision.ErrorCode, FString(TEXT("E4007")));
	TestEqual(TEXT("Failed step index should be 2 (1-based)"), Decision.FailedStepIndex, 2);
	TestEqual(TEXT("Executed steps should stop at failure"), Decision.ExecutedSteps, 2);
	TestEqual(TEXT("Rolled back steps should equal successful steps before failure"), Decision.RolledBackSteps, 1);
	TestEqual(TEXT("No partial commit should remain"), Decision.CommittedSteps, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentExecutionGateTransactionRejectsUnknownToolBeforeExecutionTest,
	"HCIAbilityKit.Editor.AgentExec.TransactionRejectsUnknownToolBeforeExecution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentExecutionGateTransactionRejectsUnknownToolBeforeExecutionTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAgentTransactionExecutionInput Input;
	Input.RequestId = TEXT("req_e5_003");

	FHCIAbilityKitAgentTransactionStepSimulation& Step1 = Input.Steps.AddDefaulted_GetRef();
	Step1.StepId = TEXT("step_001");
	Step1.ToolName = TEXT("UnknownTool");
	Step1.bShouldSucceed = true;

	const FHCIAbilityKitAgentTransactionExecutionDecision Decision =
		FHCIAbilityKitAgentExecutionGate::EvaluateAllOrNothingTransaction(Input, Registry);

	TestFalse(TEXT("Unknown tool should not commit"), Decision.bCommitted);
	TestFalse(TEXT("Preflight whitelist rejection should not require rollback"), Decision.bRolledBack);
	TestEqual(TEXT("Unknown tool should return E4002"), Decision.ErrorCode, FString(TEXT("E4002")));
	TestEqual(TEXT("No steps should execute on preflight failure"), Decision.ExecutedSteps, 0);

	return true;
}

#endif
