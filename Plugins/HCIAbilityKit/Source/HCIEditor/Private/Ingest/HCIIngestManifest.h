#pragma once

#include "CoreMinimal.h"

struct FHCIIngestManifestFile
{
	FString RelativePath;
	FString Kind;
	int64 SizeBytes = -1;
	FString Sha1;
	FString SuggestedAssetName;
};

struct FHCIIngestManifest
{
	int32 SchemaVersion = 0;
	FString BatchId;
	FString SourceApp;
	FString StagedAtUtc;
	FString SuggestedUnrealTargetRoot;
	bool bPreflightOk = false;
	TArray<FString> PreflightWarnings;
	TArray<FString> PreflightErrors;
	TArray<FHCIIngestManifestFile> Files;

	FString ManifestPath;
	FString BatchDirectory;

	FString BuildEnvContextSnippet(int32 MaxFileNames = 12) const;

	static bool TryFindLatestManifestUnderRoot(
		const FString& IngestRootDir,
		FString& OutManifestPath,
		FString& OutError);

	static bool TryLoadFromFile(
		const FString& InManifestPath,
		FHCIIngestManifest& OutManifest,
		FString& OutError);
};


