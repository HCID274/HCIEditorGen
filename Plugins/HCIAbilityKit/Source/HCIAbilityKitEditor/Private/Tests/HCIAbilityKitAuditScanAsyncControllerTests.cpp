#if WITH_DEV_AUTOMATION_TESTS

#include "Audit/HCIAbilityKitAuditScanAsyncController.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAuditScanAsyncCancelAndRetryTest,
	"HCIAbilityKit.Editor.AuditScanAsync.CancelAndRetry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAuditScanAsyncCancelAndRetryTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitAuditScanAsyncController Controller;

	TArray<FAssetData> SeedAssets;
	SeedAssets.SetNum(5);

	FString Error;
	TestTrue(TEXT("Start should succeed"), Controller.Start(MoveTemp(SeedAssets), 2, 10, Error));
	TestEqual(TEXT("State should be running"), static_cast<int32>(Controller.GetPhase()), static_cast<int32>(EHCIAbilityKitAuditScanAsyncPhase::Running));

	const FHCIAbilityKitAuditScanBatch Batch = Controller.DequeueBatch();
	TestTrue(TEXT("First batch should be valid"), Batch.bValid);
	TestEqual(TEXT("First batch start"), Batch.StartIndex, 0);
	TestEqual(TEXT("First batch end"), Batch.EndIndex, 2);

	TestTrue(TEXT("Cancel should succeed"), Controller.Cancel(Error));
	TestEqual(TEXT("State should be cancelled"), static_cast<int32>(Controller.GetPhase()), static_cast<int32>(EHCIAbilityKitAuditScanAsyncPhase::Cancelled));
	TestTrue(TEXT("Retry should be available after cancel"), Controller.CanRetry());

	TestTrue(TEXT("Retry should succeed"), Controller.Retry(Error));
	TestEqual(TEXT("State should return to running"), static_cast<int32>(Controller.GetPhase()), static_cast<int32>(EHCIAbilityKitAuditScanAsyncPhase::Running));
	TestEqual(TEXT("Retry should restart from zero"), Controller.GetNextIndex(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAuditScanAsyncFailureConvergenceTest,
	"HCIAbilityKit.Editor.AuditScanAsync.FailureConvergence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAuditScanAsyncFailureConvergenceTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitAuditScanAsyncController Controller;

	TArray<FAssetData> SeedAssets;
	SeedAssets.SetNum(3);

	FString Error;
	TestTrue(TEXT("Start should succeed"), Controller.Start(MoveTemp(SeedAssets), 1, 5, Error));

	Controller.Fail(TEXT("synthetic_failure"));
	TestEqual(TEXT("State should be failed"), static_cast<int32>(Controller.GetPhase()), static_cast<int32>(EHCIAbilityKitAuditScanAsyncPhase::Failed));
	TestTrue(TEXT("Retry should be available after failure"), Controller.CanRetry());
	TestEqual(TEXT("Failure reason should be preserved"), Controller.GetLastFailureReason(), FString(TEXT("synthetic_failure")));

	TestTrue(TEXT("Retry should recover from failed state"), Controller.Retry(Error));
	TestEqual(TEXT("State should become running"), static_cast<int32>(Controller.GetPhase()), static_cast<int32>(EHCIAbilityKitAuditScanAsyncPhase::Running));
	TestEqual(TEXT("Retry should reset index"), Controller.GetNextIndex(), 0);
	return true;
}

#endif
