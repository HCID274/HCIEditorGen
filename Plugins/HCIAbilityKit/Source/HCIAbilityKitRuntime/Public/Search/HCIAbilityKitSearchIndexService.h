#pragma once

#include "CoreMinimal.h"
#include "Search/HCIAbilityKitSearchSchema.h"

class UHCIAbilityKitAsset;

struct HCIABILITYKITRUNTIME_API FHCIAbilitySearchIndexStats
{
	int32 IndexedDocumentCount = 0;
	int32 DisplayNameCoveredCount = 0;
	int32 SceneCoveredCount = 0;
	int32 TokenCoveredCount = 0;
	FDateTime LastRefreshUtc;
	double LastRefreshDurationMs = 0.0;
	FString LastRefreshMode;

	FString ToSummaryString() const;
};

class HCIABILITYKITRUNTIME_API FHCIAbilityKitSearchIndexService
{
public:
	static FHCIAbilityKitSearchIndexService& Get();

	void RebuildFromAssetRegistry();
	bool RefreshAsset(const UHCIAbilityKitAsset* Asset);
	bool RemoveAssetByPath(const FString& AssetPath);

	const FHCIAbilitySearchIndex& GetIndex() const;
	const FHCIAbilitySearchIndexStats& GetStats() const;

private:
	void Reset();
	void RebuildStats(const FString& RefreshMode, double DurationMs);

	FHCIAbilitySearchIndex Index;
	TMap<FString, FString> AssetPathToId;
	FHCIAbilitySearchIndexStats Stats;
};
