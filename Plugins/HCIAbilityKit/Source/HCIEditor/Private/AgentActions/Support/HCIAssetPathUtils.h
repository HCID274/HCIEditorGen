#pragma once

#include "CoreMinimal.h"

namespace HCIAssetPathUtils
{
bool TrySplitObjectPath(
	const FString& ObjectPath,
	FString& OutPackagePath,
	FString& OutAssetName);

FString ToObjectPath(const FString& PackagePath, const FString& AssetName);

void NormalizeAssetPathVariants(
	const FString& InPath,
	FString& OutAssetPath,
	FString& OutObjectPath);

FString GetDirectoryFromPackagePath(const FString& PackagePath);

FString TrimTrailingSlash(const FString& InPath);

FString GetDirectoryLeafName(const FString& Path);
}


