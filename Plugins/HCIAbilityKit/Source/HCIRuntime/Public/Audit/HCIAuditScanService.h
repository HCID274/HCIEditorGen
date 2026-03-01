#pragma once

#include "CoreMinimal.h"
#include "Audit/HCIAuditRule.h"

struct HCIRUNTIME_API FHCIAuditAssetRow
{
	FString AssetPath;
	FString AssetName;
	FString AssetClass;
	FString Id;
	FString DisplayName;
	float Damage = 0.0f;
	FString RepresentingMeshPath;
	int32 TriangleCountLod0Actual = INDEX_NONE;
	int32 TriangleCountLod0ExpectedJson = INDEX_NONE;
	FString TriangleSource;
	FString TriangleSourceTagKey;
	int32 MeshLodCount = INDEX_NONE;
	FString MeshLodCountTagKey;
	bool bMeshNaniteEnabled = false;
	bool bMeshNaniteEnabledKnown = false;
	FString MeshNaniteTagKey;
	int32 TextureWidth = INDEX_NONE;
	int32 TextureHeight = INDEX_NONE;
	FString TextureDimensionsTagKey;
	FString ScanState = TEXT("ok");
	FString SkipReason;
	TArray<FHCIAuditIssue> AuditIssues;
};

struct HCIRUNTIME_API FHCIAuditScanStats
{
	int32 AssetCount = 0;
	int32 IdCoveredCount = 0;
	int32 DisplayNameCoveredCount = 0;
	int32 RepresentingMeshCoveredCount = 0;
	int32 TriangleTagCoveredCount = 0;
	int32 SkippedLockedOrDirtyCount = 0;
	int32 AssetsWithIssuesCount = 0;
	int32 InfoIssueCount = 0;
	int32 WarnIssueCount = 0;
	int32 ErrorIssueCount = 0;
	FDateTime UpdatedUtc;
	double DurationMs = 0.0;
	FString Source;

	FString ToSummaryString() const;
};

struct HCIRUNTIME_API FHCIAuditScanSnapshot
{
	TArray<FHCIAuditAssetRow> Rows;
	FHCIAuditScanStats Stats;
};

class HCIRUNTIME_API FHCIAuditScanService
{
public:
	static FHCIAuditScanService& Get();

	FHCIAuditScanSnapshot ScanFromAssetRegistry() const;
};


