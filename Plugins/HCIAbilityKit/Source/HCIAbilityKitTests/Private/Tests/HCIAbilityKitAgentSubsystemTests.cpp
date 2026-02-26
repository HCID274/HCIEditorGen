#if WITH_DEV_AUTOMATION_TESTS

#include "Subsystems/HCIAbilityKitAgentSubsystem.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"

namespace
{
static FHCIAbilityKitAgentPlan MakePlan(const FString& RequestId)
{
	FHCIAbilityKitAgentPlan Plan;
	Plan.PlanVersion = 1;
	Plan.RequestId = RequestId;
	Plan.Intent = TEXT("test_intent");
	return Plan;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentSubsystemStateDefaultIdleTest,
	"HCIAbilityKit.Editor.AgentChat.StateMachine.DefaultStateIsIdle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentSubsystemStateDefaultIdleTest::RunTest(const FString& Parameters)
{
	const UHCIAbilityKitAgentSubsystem* Subsystem = NewObject<UHCIAbilityKitAgentSubsystem>();
	TestNotNull(TEXT("Subsystem should be created"), Subsystem);
	if (!Subsystem)
	{
		return false;
	}

	TestEqual(TEXT("Default state should be Idle"), Subsystem->GetCurrentState(), EHCIAbilityKitAgentSessionState::Idle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentSubsystemReadOnlyBranchTest,
	"HCIAbilityKit.Editor.AgentChat.StateMachine.ReadOnlyPlanSelectsAutoExecuteBranch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentSubsystemReadOnlyBranchTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitAgentPlan Plan = MakePlan(TEXT("req_test_chat_branch_read_only"));
	FHCIAbilityKitAgentPlanStep& Step = Plan.Steps.AddDefaulted_GetRef();
	Step.StepId = TEXT("step_1_scan");
	Step.ToolName = TEXT("ScanAssets");
	Step.RiskLevel = EHCIAbilityKitAgentPlanRiskLevel::ReadOnly;
	Step.Args = MakeShared<FJsonObject>();
	Step.Args->SetStringField(TEXT("directory"), TEXT("/Game/HCI"));

	TestFalse(TEXT("Plan should not be write-like"), UHCIAbilityKitAgentSubsystem::IsWriteLikePlan(Plan));
	TestEqual(
		TEXT("Read-only plan should auto execute"),
		UHCIAbilityKitAgentSubsystem::ClassifyPlanExecutionBranch(Plan),
		EHCIAbilityKitAgentPlanExecutionBranch::AutoExecuteReadOnly);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAgentSubsystemWriteBranchTest,
	"HCIAbilityKit.Editor.AgentChat.StateMachine.WritePlanSelectsAwaitUserConfirmBranch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAgentSubsystemWriteBranchTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitAgentPlan Plan = MakePlan(TEXT("req_test_chat_branch_write"));
	FHCIAbilityKitAgentPlanStep& Step = Plan.Steps.AddDefaulted_GetRef();
	Step.StepId = TEXT("step_2_set_max_size");
	Step.ToolName = TEXT("SetTextureMaxSize");
	Step.RiskLevel = EHCIAbilityKitAgentPlanRiskLevel::Write;
	Step.bRequiresConfirm = true;
	Step.Args = MakeShared<FJsonObject>();
	Step.Args->SetNumberField(TEXT("max_size"), 1024);

	TestTrue(TEXT("Plan should be write-like"), UHCIAbilityKitAgentSubsystem::IsWriteLikePlan(Plan));
	TestEqual(
		TEXT("Write plan should await user confirm"),
		UHCIAbilityKitAgentSubsystem::ClassifyPlanExecutionBranch(Plan),
		EHCIAbilityKitAgentPlanExecutionBranch::AwaitUserConfirm);
	return true;
}

#endif

