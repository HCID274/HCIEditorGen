#pragma once

#include "CoreMinimal.h"
#include "Search/HCISearchSchema.h"

class UHCIAsset;

struct HCIRUNTIME_API FHCIAbilitySearchIndexStats
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

class HCIRUNTIME_API FHCISearchIndexService
{
public:
	static FHCISearchIndexService& Get();

	void RebuildFromAssetRegistry();
	bool RefreshAsset(const UHCIAsset* Asset);
	bool RemoveAssetByPath(const FString& AssetPath);

	const FHCIAbilitySearchIndex& GetIndex() const;
	const FHCIAbilitySearchIndexStats& GetStats() const;

private:
	void Reset();
	void UpdateDocumentStats(const FHCIAbilitySearchDocument& Document, bool bAdd);
	void UpdateStatsMetadata(const FString& RefreshMode, double DurationMs);

	FHCIAbilitySearchIndex Index;
	TMap<FString, FString> AssetPathToId;
	FHCIAbilitySearchIndexStats Stats;
};


