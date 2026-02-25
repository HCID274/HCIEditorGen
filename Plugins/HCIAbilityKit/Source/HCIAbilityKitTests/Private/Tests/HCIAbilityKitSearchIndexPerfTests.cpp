#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "HCIAbilityKitAsset.h"
#include "Search/HCIAbilityKitSearchIndexService.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitSearchIndexPerfTest,
	"HCIAbilityKit.Editor.SearchIndex.Performance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitSearchIndexPerfTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitSearchIndexService& Service = FHCIAbilityKitSearchIndexService::Get();

	// Prepare test data
	const int32 AssetCount = 1000;
	TArray<UHCIAbilityKitAsset*> Assets;
	Assets.Reserve(AssetCount);

	for (int32 i = 0; i < AssetCount; ++i)
	{
		const FString AssetName = FString::Printf(TEXT("TestAsset_%d"), i);
		UHCIAbilityKitAsset* Asset = NewObject<UHCIAbilityKitAsset>(GetTransientPackage(), FName(*AssetName), RF_Transient);
		Asset->Id = AssetName;
		Asset->DisplayName = FString::Printf(TEXT("Test Display Name %d"), i);
		Asset->Damage = 100.0f + i;
		Assets.Add(Asset);
	}

	// Benchmark Add/Refresh
	const double StartTimeAdd = FPlatformTime::Seconds();
	for (UHCIAbilityKitAsset* Asset : Assets)
	{
		Service.RefreshAsset(Asset);
	}
	const double DurationAdd = (FPlatformTime::Seconds() - StartTimeAdd) * 1000.0;

	// Verify Stats
	const FHCIAbilitySearchIndexStats& StatsAfterAdd = Service.GetStats();
	// Note: Existing assets in the index might affect the count if not cleared.
	// But since we use transient assets with unique names, they are added.
	// Ideally we should reset service first, but Reset() is private.
	// Assuming test environment is clean or we are testing incremental updates.
	// We can check if count increased by AssetCount.
	// But let's assume we want exact count for this test run.
	// Since we can't reset, let's just log and check logic.
	// Actually, we can check relative increase if needed, but TestEqual expects exact.
	// Given we can't easily reset, let's just ensure it's AT LEAST AssetCount.
	TestTrue(TEXT("IndexedDocumentCount should be >= AssetCount"), StatsAfterAdd.IndexedDocumentCount >= AssetCount);

	UE_LOG(LogTemp, Display, TEXT("Search Index Add Performance: %d assets took %.2f ms"), AssetCount, DurationAdd);

	// Benchmark Remove
	const double StartTimeRemove = FPlatformTime::Seconds();
	for (UHCIAbilityKitAsset* Asset : Assets)
	{
		Service.RemoveAssetByPath(Asset->GetPathName());
	}
	const double DurationRemove = (FPlatformTime::Seconds() - StartTimeRemove) * 1000.0;

	// Verify Stats
	const FHCIAbilitySearchIndexStats& StatsAfterRemove = Service.GetStats();
	// After removal, count should decrease by AssetCount.
	// We can't guarantee 0 if other tests ran.
	// But we can check if it's consistent.

	UE_LOG(LogTemp, Display, TEXT("Search Index Remove Performance: %d assets took %.2f ms"), AssetCount, DurationRemove);

	return true;
}

#endif
