#include "Search/HCIAbilityKitSearchIndexService.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "HCIAbilityKitAsset.h"
#include "Modules/ModuleManager.h"

namespace
{
DEFINE_LOG_CATEGORY_STATIC(LogHCIAbilityKitSearchIndex, Log, All);

FHCIAbilityKitParsedData BuildParsedDataFromAsset(const UHCIAbilityKitAsset* Asset)
{
	FHCIAbilityKitParsedData ParsedData;
	if (!Asset)
	{
		return ParsedData;
	}

	ParsedData.SchemaVersion = Asset->SchemaVersion;
	ParsedData.Id = Asset->Id;
	ParsedData.DisplayName = Asset->DisplayName;
	ParsedData.Damage = Asset->Damage;
	return ParsedData;
}
}

FString FHCIAbilitySearchIndexStats::ToSummaryString() const
{
	const double SafeCount = IndexedDocumentCount > 0 ? static_cast<double>(IndexedDocumentCount) : 1.0;
	const double DisplayNameCoverage = (static_cast<double>(DisplayNameCoveredCount) / SafeCount) * 100.0;
	const double SceneCoverage = (static_cast<double>(SceneCoveredCount) / SafeCount) * 100.0;
	const double TokenCoverage = (static_cast<double>(TokenCoveredCount) / SafeCount) * 100.0;
	const FString LastRefreshUtcText = LastRefreshUtc.ToIso8601();

	return FString::Printf(
		TEXT("mode=%s docs=%d display_name_coverage=%.1f%% scene_coverage=%.1f%% token_coverage=%.1f%% refresh_ms=%.2f updated_utc=%s"),
		*LastRefreshMode,
		IndexedDocumentCount,
		DisplayNameCoverage,
		SceneCoverage,
		TokenCoverage,
		LastRefreshDurationMs,
		*LastRefreshUtcText);
}

FHCIAbilityKitSearchIndexService& FHCIAbilityKitSearchIndexService::Get()
{
	static FHCIAbilityKitSearchIndexService Instance;
	return Instance;
}

void FHCIAbilityKitSearchIndexService::RebuildFromAssetRegistry()
{
	const double StartTime = FPlatformTime::Seconds();
	Reset();

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	FARFilter Filter;
	Filter.ClassPaths.Add(UHCIAbilityKitAsset::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;

	TArray<FAssetData> AssetDatas;
	AssetRegistryModule.Get().GetAssets(Filter, AssetDatas);

	for (const FAssetData& AssetData : AssetDatas)
	{
		const UHCIAbilityKitAsset* Asset = Cast<UHCIAbilityKitAsset>(AssetData.GetAsset());
		if (!Asset)
		{
			continue;
		}

		const FString AssetPath = AssetData.GetObjectPathString();
		const FHCIAbilityKitParsedData ParsedData = BuildParsedDataFromAsset(Asset);
		const FHCIAbilitySearchDocument Document = FHCIAbilitySearchSchema::BuildDocument(ParsedData, AssetPath);
		if (!Index.AddDocument(Document))
		{
			UE_LOG(LogHCIAbilityKitSearchIndex, Warning, TEXT("Skip duplicated or invalid document: id=%s path=%s"), *Document.Id, *AssetPath);
			continue;
		}

		AssetPathToId.Add(AssetPath, Document.Id);
	}

	const double DurationMs = (FPlatformTime::Seconds() - StartTime) * 1000.0;
	RebuildStats(TEXT("full_rebuild"), DurationMs);
	UE_LOG(LogHCIAbilityKitSearchIndex, Display, TEXT("[HCIAbilityKit][SearchIndex] %s"), *Stats.ToSummaryString());
}

bool FHCIAbilityKitSearchIndexService::RefreshAsset(const UHCIAbilityKitAsset* Asset)
{
	if (!Asset)
	{
		return false;
	}

	const double StartTime = FPlatformTime::Seconds();
	const FString AssetPath = Asset->GetPathName();
	if (AssetPath.IsEmpty())
	{
		return false;
	}

	const FString* ExistingId = AssetPathToId.Find(AssetPath);
	if (ExistingId)
	{
		Index.RemoveDocumentById(*ExistingId);
	}

	const FHCIAbilityKitParsedData ParsedData = BuildParsedDataFromAsset(Asset);
	const FHCIAbilitySearchDocument Document = FHCIAbilitySearchSchema::BuildDocument(ParsedData, AssetPath);
	if (!Index.AddDocument(Document))
	{
		return false;
	}

	AssetPathToId.Add(AssetPath, Document.Id);
	const double DurationMs = (FPlatformTime::Seconds() - StartTime) * 1000.0;
	RebuildStats(TEXT("incremental_refresh"), DurationMs);

	UE_LOG(LogHCIAbilityKitSearchIndex, Display, TEXT("[HCIAbilityKit][SearchIndex] %s"), *Stats.ToSummaryString());
	return true;
}

bool FHCIAbilityKitSearchIndexService::RemoveAssetByPath(const FString& AssetPath)
{
	const FString* ExistingId = AssetPathToId.Find(AssetPath);
	if (!ExistingId)
	{
		return false;
	}

	const bool bRemoved = Index.RemoveDocumentById(*ExistingId);
	AssetPathToId.Remove(AssetPath);
	RebuildStats(TEXT("incremental_remove"), 0.0);
	return bRemoved;
}

const FHCIAbilitySearchIndex& FHCIAbilityKitSearchIndexService::GetIndex() const
{
	return Index;
}

const FHCIAbilitySearchIndexStats& FHCIAbilityKitSearchIndexService::GetStats() const
{
	return Stats;
}

void FHCIAbilityKitSearchIndexService::Reset()
{
	Index.Reset();
	AssetPathToId.Reset();
	Stats = FHCIAbilitySearchIndexStats();
}

void FHCIAbilityKitSearchIndexService::RebuildStats(const FString& RefreshMode, const double DurationMs)
{
	Stats = FHCIAbilitySearchIndexStats();
	Stats.IndexedDocumentCount = Index.GetDocumentCount();
	Stats.LastRefreshUtc = FDateTime::UtcNow();
	Stats.LastRefreshDurationMs = DurationMs;
	Stats.LastRefreshMode = RefreshMode;

	for (const TPair<FString, FHCIAbilitySearchDocument>& Pair : Index.DocumentsById)
	{
		const FHCIAbilitySearchDocument& Document = Pair.Value;
		if (!Document.DisplayName.IsEmpty())
		{
			++Stats.DisplayNameCoveredCount;
		}
		if (Document.UsageScenes.Num() > 0)
		{
			++Stats.SceneCoveredCount;
		}
		if (Document.Tokens.Num() > 0)
		{
			++Stats.TokenCoveredCount;
		}
	}
}
