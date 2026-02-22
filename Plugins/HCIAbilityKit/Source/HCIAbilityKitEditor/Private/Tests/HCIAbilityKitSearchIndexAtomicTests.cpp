#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "HCIAbilityKitAsset.h"
#include "Search/HCIAbilityKitSearchIndexService.h"
#include "Misc/Guid.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitSearchIndexAtomicRefreshRollbackTest,
	"HCIAbilityKit.Editor.SearchIndex.AtomicRefreshRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitSearchIndexAtomicRemoveStaleMappingGuardTest,
	"HCIAbilityKit.Editor.SearchIndex.AtomicRemoveStaleMappingGuard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitSearchIndexAtomicRefreshRollbackTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitSearchIndexService& Service = FHCIAbilityKitSearchIndexService::Get();

	const FString UniqueSuffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString AssetNameA = FString::Printf(TEXT("AtomicAsset_A_%s"), *UniqueSuffix);
	const FString AssetNameB = FString::Printf(TEXT("AtomicAsset_B_%s"), *UniqueSuffix);
	const FString IdA = FString::Printf(TEXT("atomic_id_a_%s"), *UniqueSuffix);
	const FString IdB = FString::Printf(TEXT("atomic_id_b_%s"), *UniqueSuffix);

	UHCIAbilityKitAsset* AssetA = NewObject<UHCIAbilityKitAsset>(GetTransientPackage(), FName(*AssetNameA), RF_Transient);
	UHCIAbilityKitAsset* AssetB = NewObject<UHCIAbilityKitAsset>(GetTransientPackage(), FName(*AssetNameB), RF_Transient);
	TestNotNull(TEXT("AssetA should be created"), AssetA);
	TestNotNull(TEXT("AssetB should be created"), AssetB);
	if (!AssetA || !AssetB)
	{
		return false;
	}

	AssetA->Id = IdA;
	AssetA->DisplayName = TEXT("Atomic A");
	AssetA->Damage = 100.0f;
	AssetB->Id = IdB;
	AssetB->DisplayName = TEXT("Atomic B");
	AssetB->Damage = 200.0f;

	TestTrue(TEXT("RefreshAsset A should succeed"), Service.RefreshAsset(AssetA));
	TestTrue(TEXT("RefreshAsset B should succeed"), Service.RefreshAsset(AssetB));

	const FHCIAbilitySearchIndexStats StatsBeforeConflict = Service.GetStats();
	const int32 DocCountBeforeConflict = Service.GetIndex().GetDocumentCount();

	// Intentionally trigger AddDocument conflict with existing IdB.
	AssetA->Id = IdB;
	TestFalse(TEXT("RefreshAsset should fail on duplicate id conflict"), Service.RefreshAsset(AssetA));

	const FHCIAbilitySearchIndexStats StatsAfterConflict = Service.GetStats();
	const FHCIAbilitySearchIndex& IndexAfterConflict = Service.GetIndex();
	TestEqual(TEXT("Document count should remain unchanged after failed refresh"), IndexAfterConflict.GetDocumentCount(), DocCountBeforeConflict);
	TestTrue(TEXT("Original document IdA must still exist after rollback"), IndexAfterConflict.ContainsId(IdA));
	TestTrue(TEXT("Conflicting document IdB must still exist"), IndexAfterConflict.ContainsId(IdB));
	TestEqual(TEXT("IndexedDocumentCount should remain unchanged"), StatsAfterConflict.IndexedDocumentCount, StatsBeforeConflict.IndexedDocumentCount);
	TestEqual(TEXT("DisplayNameCoveredCount should remain unchanged"), StatsAfterConflict.DisplayNameCoveredCount, StatsBeforeConflict.DisplayNameCoveredCount);
	TestEqual(TEXT("SceneCoveredCount should remain unchanged"), StatsAfterConflict.SceneCoveredCount, StatsBeforeConflict.SceneCoveredCount);
	TestEqual(TEXT("TokenCoveredCount should remain unchanged"), StatsAfterConflict.TokenCoveredCount, StatsBeforeConflict.TokenCoveredCount);

	const FString AssetPathA = AssetA->GetPathName();
	const FString AssetPathB = AssetB->GetPathName();
	TestTrue(TEXT("Cleanup remove A"), Service.RemoveAssetByPath(AssetPathA));
	TestTrue(TEXT("Cleanup remove B"), Service.RemoveAssetByPath(AssetPathB));

	return true;
}

bool FHCIAbilityKitSearchIndexAtomicRemoveStaleMappingGuardTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitSearchIndexService& Service = FHCIAbilityKitSearchIndexService::Get();
	Service.RebuildFromAssetRegistry();

	const FString UniqueSuffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString AssetName = FString::Printf(TEXT("AtomicAsset_Stale_%s"), *UniqueSuffix);
	const FString AssetId = FString::Printf(TEXT("atomic_stale_id_%s"), *UniqueSuffix);

	UHCIAbilityKitAsset* Asset = NewObject<UHCIAbilityKitAsset>(GetTransientPackage(), FName(*AssetName), RF_Transient);
	TestNotNull(TEXT("Stale test asset should be created"), Asset);
	if (!Asset)
	{
		Service.RebuildFromAssetRegistry();
		return false;
	}

	Asset->Id = AssetId;
	Asset->DisplayName = TEXT("Atomic stale mapping guard");
	Asset->Damage = 150.0f;

	TestTrue(TEXT("RefreshAsset should succeed before stale simulation"), Service.RefreshAsset(Asset));
	const FString AssetPath = Asset->GetPathName();
	const FHCIAbilitySearchIndexStats StatsBeforeStaleDelete = Service.GetStats();

	// Simulate corrupted state: path->id mapping exists, but index document is missing.
	FHCIAbilitySearchIndex& MutableIndex = const_cast<FHCIAbilitySearchIndex&>(Service.GetIndex());
	TestTrue(TEXT("Direct index remove should succeed for stale simulation"), MutableIndex.RemoveDocumentById(AssetId));

	const FHCIAbilitySearchIndexStats StatsBeforeGuardedRemove = Service.GetStats();
	AddExpectedError(
		TEXT("RemoveAssetByPath aborted: AssetPathToId stale mapping detected"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	TestFalse(TEXT("RemoveAssetByPath should fail-fast on stale mapping"), Service.RemoveAssetByPath(AssetPath));

	const FHCIAbilitySearchIndexStats StatsAfterGuardedRemove = Service.GetStats();
	TestEqual(
		TEXT("IndexedDocumentCount should stay unchanged on guarded stale remove"),
		StatsAfterGuardedRemove.IndexedDocumentCount,
		StatsBeforeGuardedRemove.IndexedDocumentCount);
	TestEqual(
		TEXT("DisplayNameCoveredCount should stay unchanged on guarded stale remove"),
		StatsAfterGuardedRemove.DisplayNameCoveredCount,
		StatsBeforeGuardedRemove.DisplayNameCoveredCount);
	TestEqual(
		TEXT("SceneCoveredCount should stay unchanged on guarded stale remove"),
		StatsAfterGuardedRemove.SceneCoveredCount,
		StatsBeforeGuardedRemove.SceneCoveredCount);
	TestEqual(
		TEXT("TokenCoveredCount should stay unchanged on guarded stale remove"),
		StatsAfterGuardedRemove.TokenCoveredCount,
		StatsBeforeGuardedRemove.TokenCoveredCount);

	// Restore service state after intentionally corrupting internals in test.
	Service.RebuildFromAssetRegistry();
	TestTrue(
		TEXT("RebuildFromAssetRegistry should clear transient stale test entry"),
		!Service.GetIndex().ContainsId(AssetId));
	TestTrue(
		TEXT("Rebuild should not increase document count after stale simulation"),
		Service.GetStats().IndexedDocumentCount <= StatsBeforeStaleDelete.IndexedDocumentCount);

	return true;
}

#endif
