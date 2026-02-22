#if WITH_DEV_AUTOMATION_TESTS

#include "Audit/HCIAbilityKitAuditPerfMetrics.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAuditScanAsyncDeepModeRetryPersistenceTest,
	"HCIAbilityKit.Editor.AuditScanAsync.DeepModeRetryPersistence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAuditScanAsyncDeepModeRetryPersistenceTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitAuditScanAsyncController Controller;

	TArray<FAssetData> SeedAssets;
	SeedAssets.SetNum(4);

	FString Error;
	TestTrue(TEXT("Start should succeed with deep mode enabled"), Controller.Start(MoveTemp(SeedAssets), 2, 10, true, Error));
	TestTrue(TEXT("Deep mode should be enabled after start"), Controller.IsDeepMeshCheckEnabled());

	TestTrue(TEXT("Cancel should succeed"), Controller.Cancel(Error));
	TestTrue(TEXT("Retry should succeed"), Controller.Retry(Error));
	TestTrue(TEXT("Deep mode should persist after retry"), Controller.IsDeepMeshCheckEnabled());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAuditScanAsyncGcThrottleRetryPersistenceTest,
	"HCIAbilityKit.Editor.AuditScanAsync.GcThrottleRetryPersistence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAuditScanAsyncGcThrottleRetryPersistenceTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitAuditScanAsyncController Controller;

	TArray<FAssetData> SeedAssets;
	SeedAssets.SetNum(6);

	FString Error;
	TestTrue(TEXT("Start should succeed with deep mode and GC throttle"), Controller.Start(MoveTemp(SeedAssets), 2, 10, true, 3, Error));
	TestEqual(TEXT("GC throttle interval should be captured"), Controller.GetGcEveryNBatches(), 3);

	TestTrue(TEXT("Cancel should succeed"), Controller.Cancel(Error));
	TestTrue(TEXT("Retry should succeed"), Controller.Retry(Error));
	TestEqual(TEXT("GC throttle interval should persist after retry"), Controller.GetGcEveryNBatches(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAuditScanAsyncPerfMetricsHelperTest,
	"HCIAbilityKit.Editor.AuditScanAsync.PerfMetricsHelper",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAuditScanAsyncPerfMetricsHelperTest::RunTest(const FString& Parameters)
{
	TestEqual(
		TEXT("AssetsPerSecond should convert ms to per-second throughput"),
		FHCIAbilityKitAuditPerfMetrics::AssetsPerSecond(10, 200.0),
		50.0);
	TestEqual(
		TEXT("AssetsPerSecond should return 0 for zero duration"),
		FHCIAbilityKitAuditPerfMetrics::AssetsPerSecond(10, 0.0),
		0.0);

	TArray<double> Samples{50.0, 10.0, 30.0, 20.0, 40.0};
	TestEqual(
		TEXT("P50 should use nearest-rank on sorted samples"),
		FHCIAbilityKitAuditPerfMetrics::PercentileNearestRank(Samples, 50.0),
		30.0);
	TestEqual(
		TEXT("P95 should clamp to the max sample"),
		FHCIAbilityKitAuditPerfMetrics::PercentileNearestRank(Samples, 95.0),
		50.0);
	TestEqual(
		TEXT("Empty samples should return 0"),
		FHCIAbilityKitAuditPerfMetrics::PercentileNearestRank(TArray<double>{}, 50.0),
		0.0);
	return true;
}

#endif
