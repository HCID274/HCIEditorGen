#pragma once

#include "CoreMinimal.h"

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAuditAssetRow
{
	FString AssetPath;
	FString AssetName;
	FString Id;
	FString DisplayName;
	float Damage = 0.0f;
	FString RepresentingMeshPath;
	int32 TriangleCountLod0Actual = INDEX_NONE;
	FString TriangleSource;
	FString TriangleSourceTagKey;
	FString ScanState = TEXT("ok");
	FString SkipReason;
};

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAuditScanStats
{
	int32 AssetCount = 0;
	int32 IdCoveredCount = 0;
	int32 DisplayNameCoveredCount = 0;
	int32 RepresentingMeshCoveredCount = 0;
	int32 TriangleTagCoveredCount = 0;
	int32 SkippedLockedOrDirtyCount = 0;
	FDateTime UpdatedUtc;
	double DurationMs = 0.0;
	FString Source;

	FString ToSummaryString() const;
};

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAuditScanSnapshot
{
	TArray<FHCIAbilityKitAuditAssetRow> Rows;
	FHCIAbilityKitAuditScanStats Stats;
};

class HCIABILITYKITRUNTIME_API FHCIAbilityKitAuditScanService
{
public:
	static FHCIAbilityKitAuditScanService& Get();

	FHCIAbilityKitAuditScanSnapshot ScanFromAssetRegistry() const;
};
