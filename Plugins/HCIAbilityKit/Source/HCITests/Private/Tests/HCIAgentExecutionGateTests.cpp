#if WITH_DEV_AUTOMATION_TESTS

#include "Agent/Executor/HCIAgentExecutionGate.h"
#include "Agent/Tools/HCIToolRegistry.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutionGateBlocksUnconfirmedWriteTest,
	"HCI.Editor.AgentExec.ConfirmGateBlocksUnconfirmedWrite",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutionGateBlocksUnconfirmedWriteTest::RunTest(const FString& Parameters)
{
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentExecutionStep Step;
	Step.StepId = TEXT("step_001");
	Step.ToolName = TEXT("RenameAsset");
	Step.bRequiresConfirm = true;
	Step.bUserConfirmed = false;

	const FHCIAgentExecutionDecision Decision = FHCIAgentExecutionGate::EvaluateConfirmGate(Step, Registry);

	TestFalse(TEXT("Unconfirmed write step should be blocked"), Decision.bAllowed);
	TestEqual(TEXT("Blocked step should use E4005"), Decision.ErrorCode, FString(TEXT("E4005")));
	TestEqual(TEXT("Blocked step should report tool name"), Decision.ToolName, FString(TEXT("RenameAsset")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutionGateAllowsReadOnlyWithoutConfirmTest,
	"HCI.Editor.AgentExec.ConfirmGateAllowsReadOnlyWithoutConfirm",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutionGateAllowsReadOnlyWithoutConfirmTest::RunTest(const FString& Parameters)
{
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentExecutionStep Step;
	Step.StepId = TEXT("step_002");
	Step.ToolName = TEXT("ScanAssets");
	Step.bRequiresConfirm = false;
	Step.bUserConfirmed = false;

	const FHCIAgentExecutionDecision Decision = FHCIAgentExecutionGate::EvaluateConfirmGate(Step, Registry);

	TestTrue(TEXT("Read-only step should bypass confirm gate"), Decision.bAllowed);
	TestEqual(TEXT("Allowed step should have empty error code"), Decision.ErrorCode, FString());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutionGateAllowsConfirmedWriteTest,
	"HCI.Editor.AgentExec.ConfirmGateAllowsConfirmedWrite",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutionGateAllowsConfirmedWriteTest::RunTest(const FString& Parameters)
{
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentExecutionStep Step;
	Step.StepId = TEXT("step_003");
	Step.ToolName = TEXT("SetTextureMaxSize");
	Step.bRequiresConfirm = true;
	Step.bUserConfirmed = true;

	const FHCIAgentExecutionDecision Decision = FHCIAgentExecutionGate::EvaluateConfirmGate(Step, Registry);

	TestTrue(TEXT("Confirmed write step should pass confirm gate"), Decision.bAllowed);
	TestEqual(TEXT("Allowed step should preserve capability=write"), Decision.Capability, FString(TEXT("write")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutionGateBlastRadiusBlocksOverLimitTest,
	"HCI.Editor.AgentExec.BlastRadiusBlocksOverLimit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutionGateBlastRadiusBlocksOverLimitTest::RunTest(const FString& Parameters)
{
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentBlastRadiusCheckInput Input;
	Input.RequestId = TEXT("req_e4_001");
	Input.ToolName = TEXT("RenameAsset");
	Input.TargetModifyCount = 51;

	const FHCIAgentBlastRadiusDecision Decision = FHCIAgentExecutionGate::EvaluateBlastRadius(Input, Registry);

	TestFalse(TEXT("Write tool over limit should be blocked"), Decision.bAllowed);
	TestEqual(TEXT("Over-limit should return E4004"), Decision.ErrorCode, FString(TEXT("E4004")));
	TestEqual(TEXT("Limit should be frozen at 50"), Decision.MaxAssetModifyLimit, 50);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutionGateBlastRadiusAllowsAtLimitTest,
	"HCI.Editor.AgentExec.BlastRadiusAllowsAtLimit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutionGateBlastRadiusAllowsAtLimitTest::RunTest(const FString& Parameters)
{
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentBlastRadiusCheckInput Input;
	Input.RequestId = TEXT("req_e4_002");
	Input.ToolName = TEXT("SetTextureMaxSize");
	Input.TargetModifyCount = 50;

	const FHCIAgentBlastRadiusDecision Decision = FHCIAgentExecutionGate::EvaluateBlastRadius(Input, Registry);

	TestTrue(TEXT("Write tool at limit should be allowed"), Decision.bAllowed);
	TestEqual(TEXT("Capability should be write"), Decision.Capability, FString(TEXT("write")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutionGateBlastRadiusSkipsReadOnlyToolTest,
	"HCI.Editor.AgentExec.BlastRadiusSkipsReadOnlyTool",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutionGateBlastRadiusSkipsReadOnlyToolTest::RunTest(const FString& Parameters)
{
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentBlastRadiusCheckInput Input;
	Input.RequestId = TEXT("req_e4_003");
	Input.ToolName = TEXT("ScanAssets");
	Input.TargetModifyCount = 999;

	const FHCIAgentBlastRadiusDecision Decision = FHCIAgentExecutionGate::EvaluateBlastRadius(Input, Registry);

	TestTrue(TEXT("Read-only tool should not be blocked by blast radius"), Decision.bAllowed);
	TestEqual(TEXT("Capability should be read_only"), Decision.Capability, FString(TEXT("read_only")));
	TestFalse(TEXT("Read-only tool should not be write-like"), Decision.bWriteLike);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutionGateTransactionCommitsAllStepsOnSuccessTest,
	"HCI.Editor.AgentExec.TransactionCommitsAllStepsOnSuccess",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutionGateTransactionCommitsAllStepsOnSuccessTest::RunTest(const FString& Parameters)
{
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentTransactionExecutionInput Input;
	Input.RequestId = TEXT("req_e5_001");

	FHCIAgentTransactionStepSimulation& Step1 = Input.Steps.AddDefaulted_GetRef();
	Step1.StepId = TEXT("step_001");
	Step1.ToolName = TEXT("RenameAsset");
	Step1.bShouldSucceed = true;

	FHCIAgentTransactionStepSimulation& Step2 = Input.Steps.AddDefaulted_GetRef();
	Step2.StepId = TEXT("step_002");
	Step2.ToolName = TEXT("MoveAsset");
	Step2.bShouldSucceed = true;

	const FHCIAgentTransactionExecutionDecision Decision =
		FHCIAgentExecutionGate::EvaluateAllOrNothingTransaction(Input, Registry);

	TestTrue(TEXT("All-success transaction should commit"), Decision.bCommitted);
	TestFalse(TEXT("All-success transaction should not roll back"), Decision.bRolledBack);
	TestEqual(TEXT("Committed steps should equal total steps"), Decision.CommittedSteps, 2);
	TestEqual(TEXT("Rolled back steps should be zero"), Decision.RolledBackSteps, 0);
	TestEqual(TEXT("Transaction mode should be all_or_nothing"), Decision.TransactionMode, FString(TEXT("all_or_nothing")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutionGateTransactionRollsBackOnFailureTest,
	"HCI.Editor.AgentExec.TransactionRollsBackOnFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutionGateTransactionRollsBackOnFailureTest::RunTest(const FString& Parameters)
{
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentTransactionExecutionInput Input;
	Input.RequestId = TEXT("req_e5_002");

	FHCIAgentTransactionStepSimulation& Step1 = Input.Steps.AddDefaulted_GetRef();
	Step1.StepId = TEXT("step_001");
	Step1.ToolName = TEXT("RenameAsset");
	Step1.bShouldSucceed = true;

	FHCIAgentTransactionStepSimulation& Step2 = Input.Steps.AddDefaulted_GetRef();
	Step2.StepId = TEXT("step_002");
	Step2.ToolName = TEXT("MoveAsset");
	Step2.bShouldSucceed = false;

	FHCIAgentTransactionStepSimulation& Step3 = Input.Steps.AddDefaulted_GetRef();
	Step3.StepId = TEXT("step_003");
	Step3.ToolName = TEXT("SetTextureMaxSize");
	Step3.bShouldSucceed = true;

	const FHCIAgentTransactionExecutionDecision Decision =
		FHCIAgentExecutionGate::EvaluateAllOrNothingTransaction(Input, Registry);

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
	FHCIAgentExecutionGateTransactionRejectsUnknownToolBeforeExecutionTest,
	"HCI.Editor.AgentExec.TransactionRejectsUnknownToolBeforeExecution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutionGateTransactionRejectsUnknownToolBeforeExecutionTest::RunTest(const FString& Parameters)
{
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentTransactionExecutionInput Input;
	Input.RequestId = TEXT("req_e5_003");

	FHCIAgentTransactionStepSimulation& Step1 = Input.Steps.AddDefaulted_GetRef();
	Step1.StepId = TEXT("step_001");
	Step1.ToolName = TEXT("UnknownTool");
	Step1.bShouldSucceed = true;

	const FHCIAgentTransactionExecutionDecision Decision =
		FHCIAgentExecutionGate::EvaluateAllOrNothingTransaction(Input, Registry);

	TestFalse(TEXT("Unknown tool should not commit"), Decision.bCommitted);
	TestFalse(TEXT("Preflight whitelist rejection should not require rollback"), Decision.bRolledBack);
	TestEqual(TEXT("Unknown tool should return E4002"), Decision.ErrorCode, FString(TEXT("E4002")));
	TestEqual(TEXT("No steps should execute on preflight failure"), Decision.ExecutedSteps, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutionGateSourceControlAllowsOfflineLocalModeWhenDisabledTest,
	"HCI.Editor.AgentExec.SourceControlAllowsOfflineLocalModeWhenDisabled",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutionGateSourceControlAllowsOfflineLocalModeWhenDisabledTest::RunTest(const FString& Parameters)
{
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentSourceControlCheckInput Input;
	Input.RequestId = TEXT("req_e6_001");
	Input.ToolName = TEXT("RenameAsset");
	Input.bSourceControlEnabled = false;
	Input.bCheckoutSucceeded = false;

	const FHCIAgentSourceControlDecision Decision =
		FHCIAgentExecutionGate::EvaluateSourceControlFailFast(Input, Registry);

	TestTrue(TEXT("Offline local mode should allow write when source control is disabled"), Decision.bAllowed);
	TestTrue(TEXT("Offline local mode flag should be set"), Decision.bOfflineLocalMode);
	TestFalse(TEXT("Checkout should not be attempted when source control is disabled"), Decision.bCheckoutAttempted);
	TestEqual(TEXT("Decision should preserve write capability"), Decision.Capability, FString(TEXT("write")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutionGateSourceControlBlocksEnabledCheckoutFailureTest,
	"HCI.Editor.AgentExec.SourceControlBlocksEnabledCheckoutFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutionGateSourceControlBlocksEnabledCheckoutFailureTest::RunTest(const FString& Parameters)
{
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentSourceControlCheckInput Input;
	Input.RequestId = TEXT("req_e6_002");
	Input.ToolName = TEXT("MoveAsset");
	Input.bSourceControlEnabled = true;
	Input.bCheckoutSucceeded = false;

	const FHCIAgentSourceControlDecision Decision =
		FHCIAgentExecutionGate::EvaluateSourceControlFailFast(Input, Registry);

	TestFalse(TEXT("Checkout failure should fail-fast block the write tool"), Decision.bAllowed);
	TestTrue(TEXT("Checkout should be attempted when source control is enabled"), Decision.bCheckoutAttempted);
	TestEqual(TEXT("Checkout failure should return E4006"), Decision.ErrorCode, FString(TEXT("E4006")));
	TestEqual(TEXT("Checkout failure reason should be frozen"), Decision.Reason, FString(TEXT("source_control_checkout_failed_fail_fast")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutionGateSourceControlAllowsReadOnlyWithoutCheckoutTest,
	"HCI.Editor.AgentExec.SourceControlAllowsReadOnlyWithoutCheckout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutionGateSourceControlAllowsReadOnlyWithoutCheckoutTest::RunTest(const FString& Parameters)
{
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentSourceControlCheckInput Input;
	Input.RequestId = TEXT("req_e6_003");
	Input.ToolName = TEXT("ScanAssets");
	Input.bSourceControlEnabled = true;
	Input.bCheckoutSucceeded = false;

	const FHCIAgentSourceControlDecision Decision =
		FHCIAgentExecutionGate::EvaluateSourceControlFailFast(Input, Registry);

	TestTrue(TEXT("Read-only tool should bypass source control checkout"), Decision.bAllowed);
	TestEqual(TEXT("Capability should be read_only"), Decision.Capability, FString(TEXT("read_only")));
	TestFalse(TEXT("Read-only tool should not attempt checkout"), Decision.bCheckoutAttempted);
	TestFalse(TEXT("Read-only tool should not enter offline local mode"), Decision.bOfflineLocalMode);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutionGateSourceControlAllowsEnabledCheckoutSuccessTest,
	"HCI.Editor.AgentExec.SourceControlAllowsEnabledCheckoutSuccess",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutionGateSourceControlAllowsEnabledCheckoutSuccessTest::RunTest(const FString& Parameters)
{
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentSourceControlCheckInput Input;
	Input.RequestId = TEXT("req_e6_004");
	Input.ToolName = TEXT("SetTextureMaxSize");
	Input.bSourceControlEnabled = true;
	Input.bCheckoutSucceeded = true;

	const FHCIAgentSourceControlDecision Decision =
		FHCIAgentExecutionGate::EvaluateSourceControlFailFast(Input, Registry);

	TestTrue(TEXT("Checkout success should allow write tool"), Decision.bAllowed);
	TestTrue(TEXT("Checkout should be attempted when source control is enabled"), Decision.bCheckoutAttempted);
	TestTrue(TEXT("Checkout success should be recorded"), Decision.bCheckoutSucceeded);
	TestFalse(TEXT("Enabled source control success should not be offline mode"), Decision.bOfflineLocalMode);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutionGateMockRbacBlocksGuestWriteFallbackTest,
	"HCI.Editor.AgentExec.MockRbacBlocksGuestWriteFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutionGateMockRbacBlocksGuestWriteFallbackTest::RunTest(const FString& Parameters)
{
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentMockRbacCheckInput Input;
	Input.RequestId = TEXT("req_e7_001");
	Input.UserName = TEXT("unknown_user");
	Input.ResolvedRole = TEXT("Guest");
	Input.bUserMatchedWhitelist = false;
	Input.ToolName = TEXT("RenameAsset");
	Input.TargetAssetCount = 1;
	Input.AllowedCapabilities.Add(TEXT("read_only"));

	const FHCIAgentMockRbacDecision Decision =
		FHCIAgentExecutionGate::EvaluateMockRbac(Input, Registry);

	TestFalse(TEXT("Guest fallback should block write tools"), Decision.bAllowed);
	TestEqual(TEXT("Guest write denial should return E4008"), Decision.ErrorCode, FString(TEXT("E4008")));
	TestTrue(TEXT("Guest fallback flag should be set"), Decision.bGuestFallback);
	TestEqual(TEXT("Capability should be write"), Decision.Capability, FString(TEXT("write")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutionGateMockRbacAllowsGuestReadOnlyFallbackTest,
	"HCI.Editor.AgentExec.MockRbacAllowsGuestReadOnlyFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutionGateMockRbacAllowsGuestReadOnlyFallbackTest::RunTest(const FString& Parameters)
{
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentMockRbacCheckInput Input;
	Input.RequestId = TEXT("req_e7_002");
	Input.UserName = TEXT("unknown_user");
	Input.ResolvedRole = TEXT("Guest");
	Input.bUserMatchedWhitelist = false;
	Input.ToolName = TEXT("ScanAssets");
	Input.TargetAssetCount = 0;
	Input.AllowedCapabilities.Add(TEXT("read_only"));

	const FHCIAgentMockRbacDecision Decision =
		FHCIAgentExecutionGate::EvaluateMockRbac(Input, Registry);

	TestTrue(TEXT("Guest fallback should allow read-only tool"), Decision.bAllowed);
	TestTrue(TEXT("Guest fallback flag should be set"), Decision.bGuestFallback);
	TestEqual(TEXT("Capability should be read_only"), Decision.Capability, FString(TEXT("read_only")));
	TestEqual(TEXT("Allowed guest read-only reason should be stable"), Decision.Reason, FString(TEXT("guest_read_only_allowed")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutionGateMockRbacAllowsConfiguredWriteUserTest,
	"HCI.Editor.AgentExec.MockRbacAllowsConfiguredWriteUser",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutionGateMockRbacAllowsConfiguredWriteUserTest::RunTest(const FString& Parameters)
{
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentMockRbacCheckInput Input;
	Input.RequestId = TEXT("req_e7_003");
	Input.UserName = TEXT("artist_a");
	Input.ResolvedRole = TEXT("Artist");
	Input.bUserMatchedWhitelist = true;
	Input.ToolName = TEXT("SetTextureMaxSize");
	Input.TargetAssetCount = 3;
	Input.AllowedCapabilities.Add(TEXT("read_only"));
	Input.AllowedCapabilities.Add(TEXT("write"));

	const FHCIAgentMockRbacDecision Decision =
		FHCIAgentExecutionGate::EvaluateMockRbac(Input, Registry);

	TestTrue(TEXT("Configured write-capable user should be allowed"), Decision.bAllowed);
	TestFalse(TEXT("Configured user should not be guest fallback"), Decision.bGuestFallback);
	TestEqual(TEXT("Resolved role should be preserved"), Decision.ResolvedRole, FString(TEXT("Artist")));
	TestEqual(TEXT("Capability should be write"), Decision.Capability, FString(TEXT("write")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutionGateLocalAuditLogJsonLineIncludesCoreFieldsTest,
	"HCI.Editor.AgentExec.LocalAuditLogJsonLineIncludesCoreFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutionGateLocalAuditLogJsonLineIncludesCoreFieldsTest::RunTest(const FString& Parameters)
{
	FHCIAgentLocalAuditLogRecord Record;
	Record.TimestampUtc = TEXT("2026-02-22T12:00:00Z");
	Record.UserName = TEXT("artist_a");
	Record.ResolvedRole = TEXT("Artist");
	Record.RequestId = TEXT("req_e7_log_001");
	Record.ToolName = TEXT("SetTextureMaxSize");
	Record.Capability = TEXT("write");
	Record.AssetCount = 3;
	Record.Result = TEXT("allowed");
	Record.ErrorCode = TEXT("");
	Record.Reason = TEXT("rbac_allowed");

	FString JsonLine;
	FString Error;
	const bool bOk = FHCIAgentExecutionGate::SerializeLocalAuditLogRecordToJsonLine(Record, JsonLine, Error);

	TestTrue(TEXT("Audit log JSON line serialization should succeed"), bOk);
	TestTrue(TEXT("JSON line should include timestamp_utc"), JsonLine.Contains(TEXT("\"timestamp_utc\"")));
	TestTrue(TEXT("JSON line should include user"), JsonLine.Contains(TEXT("\"user\"")));
	TestTrue(TEXT("JSON line should include request_id"), JsonLine.Contains(TEXT("\"request_id\"")));
	TestTrue(TEXT("JSON line should include tool_name"), JsonLine.Contains(TEXT("\"tool_name\"")));
	TestTrue(TEXT("JSON line should include asset_count"), JsonLine.Contains(TEXT("\"asset_count\"")));
	TestTrue(TEXT("JSON line should include result"), JsonLine.Contains(TEXT("\"result\"")));
	TestTrue(TEXT("JSON line should include error_code"), JsonLine.Contains(TEXT("\"error_code\"")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutionGateLodSafetyBlocksNonStaticMeshTargetTest,
	"HCI.Editor.AgentExec.LodSafetyBlocksNonStaticMeshTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutionGateLodSafetyBlocksNonStaticMeshTargetTest::RunTest(const FString& Parameters)
{
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentLodToolSafetyCheckInput Input;
	Input.RequestId = TEXT("req_e8_001");
	Input.ToolName = TEXT("SetMeshLODGroup");
	Input.TargetObjectClass = TEXT("Texture2D");
	Input.bNaniteEnabled = false;

	const FHCIAgentLodToolSafetyDecision Decision =
		FHCIAgentExecutionGate::EvaluateLodToolSafety(Input, Registry);

	TestFalse(TEXT("SetMeshLODGroup must reject non-StaticMesh target"), Decision.bAllowed);
	TestEqual(TEXT("Type violation should return E4010"), Decision.ErrorCode, FString(TEXT("E4010")));
	TestEqual(TEXT("Type violation reason should be stable"), Decision.Reason, FString(TEXT("lod_tool_requires_static_mesh")));
	TestFalse(TEXT("Target should not be classified as StaticMesh"), Decision.bStaticMeshTarget);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutionGateLodSafetyBlocksNaniteMeshTest,
	"HCI.Editor.AgentExec.LodSafetyBlocksNaniteMesh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutionGateLodSafetyBlocksNaniteMeshTest::RunTest(const FString& Parameters)
{
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentLodToolSafetyCheckInput Input;
	Input.RequestId = TEXT("req_e8_002");
	Input.ToolName = TEXT("SetMeshLODGroup");
	Input.TargetObjectClass = TEXT("StaticMesh");
	Input.bNaniteEnabled = true;

	const FHCIAgentLodToolSafetyDecision Decision =
		FHCIAgentExecutionGate::EvaluateLodToolSafety(Input, Registry);

	TestFalse(TEXT("SetMeshLODGroup must reject Nanite-enabled StaticMesh"), Decision.bAllowed);
	TestEqual(TEXT("Nanite violation should return E4010"), Decision.ErrorCode, FString(TEXT("E4010")));
	TestEqual(TEXT("Nanite violation reason should be stable"), Decision.Reason, FString(TEXT("lod_tool_nanite_enabled_blocked")));
	TestTrue(TEXT("StaticMesh target flag should be set"), Decision.bStaticMeshTarget);
	TestTrue(TEXT("Nanite flag should be preserved"), Decision.bNaniteEnabled);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAgentExecutionGateLodSafetyAllowsNonNaniteStaticMeshTest,
	"HCI.Editor.AgentExec.LodSafetyAllowsNonNaniteStaticMesh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAgentExecutionGateLodSafetyAllowsNonNaniteStaticMeshTest::RunTest(const FString& Parameters)
{
	FHCIToolRegistry& Registry = FHCIToolRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAgentLodToolSafetyCheckInput Input;
	Input.RequestId = TEXT("req_e8_003");
	Input.ToolName = TEXT("SetMeshLODGroup");
	Input.TargetObjectClass = TEXT("UStaticMesh");
	Input.bNaniteEnabled = false;

	const FHCIAgentLodToolSafetyDecision Decision =
		FHCIAgentExecutionGate::EvaluateLodToolSafety(Input, Registry);

	TestTrue(TEXT("SetMeshLODGroup should allow non-Nanite StaticMesh"), Decision.bAllowed);
	TestEqual(TEXT("Capability should remain write"), Decision.Capability, FString(TEXT("write")));
	TestTrue(TEXT("StaticMesh target detection should accept UStaticMesh"), Decision.bStaticMeshTarget);
	TestEqual(TEXT("Allow reason should be stable"), Decision.Reason, FString(TEXT("lod_tool_static_mesh_non_nanite_allowed")));

	return true;
}

#endif
