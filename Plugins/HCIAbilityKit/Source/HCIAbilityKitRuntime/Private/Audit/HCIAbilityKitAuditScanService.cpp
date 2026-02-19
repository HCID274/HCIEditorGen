#include "Audit/HCIAbilityKitAuditScanService.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Audit/HCIAbilityKitAuditTagNames.h"
#include "HCIAbilityKitAsset.h"
#include "Modules/ModuleManager.h"

namespace
{
void ReadAssetRowFromTags(const FAssetData& AssetData, FHCIAbilityKitAuditAssetRow& OutRow)
{
	OutRow.AssetPath = AssetData.GetObjectPathString();
	OutRow.AssetName = AssetData.AssetName.ToString();

	AssetData.GetTagValue(HCIAbilityKitAuditTagNames::Id, OutRow.Id);
	AssetData.GetTagValue(HCIAbilityKitAuditTagNames::DisplayName, OutRow.DisplayName);
	AssetData.GetTagValue(HCIAbilityKitAuditTagNames::RepresentingMesh, OutRow.RepresentingMeshPath);

	FString DamageText;
	if (AssetData.GetTagValue(HCIAbilityKitAuditTagNames::Damage, DamageText))
	{
		float Damage = 0.0f;
		if (LexTryParseString(Damage, *DamageText))
		{
			OutRow.Damage = Damage;
		}
	}
}
}

FString FHCIAbilityKitAuditScanStats::ToSummaryString() const
{
	const double SafeCount = AssetCount > 0 ? static_cast<double>(AssetCount) : 1.0;
	const double IdCoverage = (static_cast<double>(IdCoveredCount) / SafeCount) * 100.0;
	const double DisplayNameCoverage = (static_cast<double>(DisplayNameCoveredCount) / SafeCount) * 100.0;
	const double RepresentingMeshCoverage = (static_cast<double>(RepresentingMeshCoveredCount) / SafeCount) * 100.0;

	return FString::Printf(
		TEXT("source=%s assets=%d id_coverage=%.1f%% display_name_coverage=%.1f%% representing_mesh_coverage=%.1f%% refresh_ms=%.2f updated_utc=%s"),
		*Source,
		AssetCount,
		IdCoverage,
		DisplayNameCoverage,
		RepresentingMeshCoverage,
		DurationMs,
		*UpdatedUtc.ToIso8601());
}

FHCIAbilityKitAuditScanService& FHCIAbilityKitAuditScanService::Get()
{
	static FHCIAbilityKitAuditScanService Instance;
	return Instance;
}

FHCIAbilityKitAuditScanSnapshot FHCIAbilityKitAuditScanService::ScanFromAssetRegistry() const
{
	const double StartTime = FPlatformTime::Seconds();
	FHCIAbilityKitAuditScanSnapshot Snapshot;
	Snapshot.Stats.Source = TEXT("asset_registry_fassetdata");

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	FARFilter Filter;
	Filter.ClassPaths.Add(UHCIAbilityKitAsset::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;

	TArray<FAssetData> AssetDatas;
	AssetRegistryModule.Get().GetAssets(Filter, AssetDatas);
	Snapshot.Rows.Reserve(AssetDatas.Num());

	for (const FAssetData& AssetData : AssetDatas)
	{
		FHCIAbilityKitAuditAssetRow& Row = Snapshot.Rows.Emplace_GetRef();
		ReadAssetRowFromTags(AssetData, Row);

		if (!Row.Id.IsEmpty())
		{
			++Snapshot.Stats.IdCoveredCount;
		}
		if (!Row.DisplayName.IsEmpty())
		{
			++Snapshot.Stats.DisplayNameCoveredCount;
		}
		if (!Row.RepresentingMeshPath.IsEmpty())
		{
			++Snapshot.Stats.RepresentingMeshCoveredCount;
		}
	}

	Snapshot.Stats.AssetCount = Snapshot.Rows.Num();
	Snapshot.Stats.UpdatedUtc = FDateTime::UtcNow();
	Snapshot.Stats.DurationMs = (FPlatformTime::Seconds() - StartTime) * 1000.0;
	return Snapshot;
}

