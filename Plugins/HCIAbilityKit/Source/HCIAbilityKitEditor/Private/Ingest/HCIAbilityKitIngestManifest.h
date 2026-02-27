#pragma once

#include "CoreMinimal.h"

struct FHCIAbilityKitIngestManifestFile
{
	FString RelativePath;
	FString Kind;
	int64 SizeBytes = -1;
	FString Sha1;
	FString SuggestedAssetName;
};

struct FHCIAbilityKitIngestManifest
{
	int32 SchemaVersion = 0;
	FString BatchId;
	FString SourceApp;
	FString StagedAtUtc;
	FString SuggestedUnrealTargetRoot;
	bool bPreflightOk = false;
	TArray<FString> PreflightWarnings;
	TArray<FString> PreflightErrors;
	TArray<FHCIAbilityKitIngestManifestFile> Files;

	FString ManifestPath;
	FString BatchDirectory;

	FString BuildEnvContextSnippet(int32 MaxFileNames = 12) const;

	static bool TryFindLatestManifestUnderRoot(
		const FString& IngestRootDir,
		FString& OutManifestPath,
		FString& OutError);

	static bool TryLoadFromFile(
		const FString& InManifestPath,
		FHCIAbilityKitIngestManifest& OutManifest,
		FString& OutError);
};

