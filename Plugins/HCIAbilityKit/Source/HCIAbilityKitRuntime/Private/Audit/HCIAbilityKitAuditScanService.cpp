#include "Audit/HCIAbilityKitAuditScanService.h"

#include "Audit/HCIAbilityKitAuditRuleRegistry.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Audit/HCIAbilityKitAuditTagNames.h"
#include "Common/HCIAbilityKitTimeFormat.h"
#include "HAL/FileManager.h"
#include "HCIAbilityKitAsset.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectGlobals.h"

namespace
{
void TryFillGenericAssetSignalsFromTags(const FAssetData& AssetData, FHCIAbilityKitAuditAssetRow& OutRow)
{
	FName TextureDimensionsTagKey;
	if (HCIAbilityKitAuditTagNames::TryResolveTextureDimensionsFromTags(
			[&AssetData](const FName& TagName, FString& OutValue)
			{
				return AssetData.GetTagValue(TagName, OutValue);
			},
			OutRow.TextureWidth,
			OutRow.TextureHeight,
			TextureDimensionsTagKey))
	{
		OutRow.TextureDimensionsTagKey = TextureDimensionsTagKey.ToString();
	}
}

bool TryMarkScanSkippedByState(const FAssetData& AssetData, FHCIAbilityKitAuditAssetRow& OutRow)
{
	const FString PackageName = AssetData.PackageName.ToString();

	if (UPackage* LoadedPackage = FindPackage(nullptr, *PackageName))
	{
		if (LoadedPackage->IsDirty())
		{
			OutRow.ScanState = TEXT("skipped_locked_or_dirty");
			OutRow.SkipReason = TEXT("package_dirty");
			OutRow.TriangleSource = TEXT("unavailable");
			return true;
		}
	}

	const FString PackageFilename = FPackageName::LongPackageNameToFilename(
		PackageName,
		FPackageName::GetAssetPackageExtension());
	if (!PackageFilename.IsEmpty() && IFileManager::Get().FileExists(*PackageFilename) && IFileManager::Get().IsReadOnly(*PackageFilename))
	{
		OutRow.ScanState = TEXT("skipped_locked_or_dirty");
		OutRow.SkipReason = TEXT("package_read_only");
		OutRow.TriangleSource = TEXT("unavailable");
		return true;
	}

	OutRow.ScanState = TEXT("ok");
	OutRow.SkipReason = TEXT("");
	return false;
}

void TryFillTriangleFromTagCached(
	const FAssetData& AssetData,
	IAssetRegistry& AssetRegistry,
	FHCIAbilityKitAuditAssetRow& OutRow)
{
	FName SourceTagKey;
	if (HCIAbilityKitAuditTagNames::TryResolveTriangleCountFromTags(
			[&AssetData](const FName& TagName, FString& OutValue)
			{
				return AssetData.GetTagValue(TagName, OutValue);
			},
			OutRow.TriangleCountLod0Actual,
			SourceTagKey))
	{
		OutRow.TriangleSource = TEXT("tag_cached");
		OutRow.TriangleSourceTagKey = SourceTagKey.ToString();
		return;
	}

	if (OutRow.RepresentingMeshPath.IsEmpty())
	{
		OutRow.TriangleSource = TEXT("unavailable");
		return;
	}

	const FSoftObjectPath RepresentingMeshObjectPath(OutRow.RepresentingMeshPath);
	if (!RepresentingMeshObjectPath.IsValid())
	{
		OutRow.TriangleSource = TEXT("unavailable");
		return;
	}

	const FAssetData MeshAssetData = AssetRegistry.GetAssetByObjectPath(RepresentingMeshObjectPath);
	if (!MeshAssetData.IsValid())
	{
		OutRow.TriangleSource = TEXT("unavailable");
		return;
	}

	FName MeshLodTagKey;
	if (HCIAbilityKitAuditTagNames::TryResolveMeshLodCountFromTags(
			[&MeshAssetData](const FName& TagName, FString& OutValue)
			{
				return MeshAssetData.GetTagValue(TagName, OutValue);
			},
			OutRow.MeshLodCount,
			MeshLodTagKey))
	{
		OutRow.MeshLodCountTagKey = MeshLodTagKey.ToString();
	}

	FName MeshNaniteTagKey;
	if (HCIAbilityKitAuditTagNames::TryResolveMeshNaniteEnabledFromTags(
			[&MeshAssetData](const FName& TagName, FString& OutValue)
			{
				return MeshAssetData.GetTagValue(TagName, OutValue);
			},
			OutRow.bMeshNaniteEnabled,
			MeshNaniteTagKey))
	{
		OutRow.bMeshNaniteEnabledKnown = true;
		OutRow.MeshNaniteTagKey = MeshNaniteTagKey.ToString();
	}

	if (HCIAbilityKitAuditTagNames::TryResolveTriangleCountFromTags(
			[&MeshAssetData](const FName& TagName, FString& OutValue)
			{
				return MeshAssetData.GetTagValue(TagName, OutValue);
			},
			OutRow.TriangleCountLod0Actual,
			SourceTagKey))
	{
		OutRow.TriangleSource = TEXT("tag_cached");
		OutRow.TriangleSourceTagKey = SourceTagKey.ToString();
		return;
	}

	OutRow.TriangleSource = TEXT("unavailable");
}

void ReadAssetRowFromTags(const FAssetData& AssetData, IAssetRegistry& AssetRegistry, FHCIAbilityKitAuditAssetRow& OutRow)
{
	OutRow.AssetPath = AssetData.GetObjectPathString();
	OutRow.AssetName = AssetData.AssetName.ToString();
	OutRow.AssetClass = AssetData.AssetClassPath.GetAssetName().ToString();

	AssetData.GetTagValue(HCIAbilityKitAuditTagNames::Id, OutRow.Id);
	AssetData.GetTagValue(HCIAbilityKitAuditTagNames::DisplayName, OutRow.DisplayName);
	AssetData.GetTagValue(HCIAbilityKitAuditTagNames::RepresentingMesh, OutRow.RepresentingMeshPath);
	TryFillGenericAssetSignalsFromTags(AssetData, OutRow);

	FName TriangleExpectedTagKey;
	HCIAbilityKitAuditTagNames::TryResolveTriangleExpectedFromTags(
		[&AssetData](const FName& TagName, FString& OutValue)
		{
			return AssetData.GetTagValue(TagName, OutValue);
		},
		OutRow.TriangleCountLod0ExpectedJson,
		TriangleExpectedTagKey);
	(void)TriangleExpectedTagKey;

	FString DamageText;
	if (AssetData.GetTagValue(HCIAbilityKitAuditTagNames::Damage, DamageText))
	{
		float Damage = 0.0f;
		if (LexTryParseString(Damage, *DamageText))
		{
			OutRow.Damage = Damage;
		}
	}

	if (TryMarkScanSkippedByState(AssetData, OutRow))
	{
		return;
	}

	TryFillTriangleFromTagCached(AssetData, AssetRegistry, OutRow);
}
}

FString FHCIAbilityKitAuditScanStats::ToSummaryString() const
{
	const double SafeCount = AssetCount > 0 ? static_cast<double>(AssetCount) : 1.0;
	const double IdCoverage = (static_cast<double>(IdCoveredCount) / SafeCount) * 100.0;
	const double DisplayNameCoverage = (static_cast<double>(DisplayNameCoveredCount) / SafeCount) * 100.0;
	const double RepresentingMeshCoverage = (static_cast<double>(RepresentingMeshCoveredCount) / SafeCount) * 100.0;
	const double TriangleTagCoverage = (static_cast<double>(TriangleTagCoveredCount) / SafeCount) * 100.0;

	return FString::Printf(
		TEXT("source=%s assets=%d id_coverage=%.1f%% display_name_coverage=%.1f%% representing_mesh_coverage=%.1f%% triangle_tag_coverage=%.1f%% skipped_locked_or_dirty=%d issue_assets=%d issue_info=%d issue_warn=%d issue_error=%d refresh_ms=%.2f updated_utc=%s"),
		*Source,
		AssetCount,
		IdCoverage,
		DisplayNameCoverage,
		RepresentingMeshCoverage,
		TriangleTagCoverage,
		SkippedLockedOrDirtyCount,
		AssetsWithIssuesCount,
		InfoIssueCount,
		WarnIssueCount,
		ErrorIssueCount,
		DurationMs,
		*FHCIAbilityKitTimeFormat::FormatUtcAsBeijingIso8601(UpdatedUtc));
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
		ReadAssetRowFromTags(AssetData, AssetRegistryModule.Get(), Row);

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
		if (Row.TriangleSource == TEXT("tag_cached") && Row.TriangleCountLod0Actual >= 0)
		{
			++Snapshot.Stats.TriangleTagCoveredCount;
		}
		if (Row.ScanState == TEXT("skipped_locked_or_dirty"))
		{
			++Snapshot.Stats.SkippedLockedOrDirtyCount;
		}

		FHCIAbilityKitAuditContext Context{Row};
		FHCIAbilityKitAuditRuleRegistry::Get().Evaluate(Context, Row.AuditIssues);
		if (Row.AuditIssues.Num() > 0)
		{
			++Snapshot.Stats.AssetsWithIssuesCount;
			for (const FHCIAbilityKitAuditIssue& Issue : Row.AuditIssues)
			{
				switch (Issue.Severity)
				{
				case EHCIAbilityKitAuditSeverity::Info:
					++Snapshot.Stats.InfoIssueCount;
					break;
				case EHCIAbilityKitAuditSeverity::Warn:
					++Snapshot.Stats.WarnIssueCount;
					break;
				case EHCIAbilityKitAuditSeverity::Error:
					++Snapshot.Stats.ErrorIssueCount;
					break;
				default:
					break;
				}
			}
		}
	}

	Snapshot.Stats.AssetCount = Snapshot.Rows.Num();
	Snapshot.Stats.UpdatedUtc = FDateTime::UtcNow();
	Snapshot.Stats.DurationMs = (FPlatformTime::Seconds() - StartTime) * 1000.0;
	return Snapshot;
}
