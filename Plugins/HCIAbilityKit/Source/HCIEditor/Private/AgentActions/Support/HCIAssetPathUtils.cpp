#include "AgentActions/Support/HCIAssetPathUtils.h"

namespace HCIAssetPathUtils
{
bool TrySplitObjectPath(
	const FString& ObjectPath,
	FString& OutPackagePath,
	FString& OutAssetName)
{
	OutPackagePath.Reset();
	OutAssetName.Reset();

	if (!ObjectPath.StartsWith(TEXT("/Game/")))
	{
		return false;
	}

	FString PackagePath = ObjectPath;
	const int32 DotIndex = PackagePath.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromStart);
	if (DotIndex != INDEX_NONE)
	{
		PackagePath = PackagePath.Left(DotIndex);
	}

	int32 LastSlash = INDEX_NONE;
	if (!PackagePath.FindLastChar(TEXT('/'), LastSlash) || LastSlash <= 0 || LastSlash + 1 >= PackagePath.Len())
	{
		return false;
	}

	OutPackagePath = PackagePath;
	OutAssetName = PackagePath.Mid(LastSlash + 1);
	return !OutAssetName.IsEmpty();
}

FString ToObjectPath(const FString& PackagePath, const FString& AssetName)
{
	return FString::Printf(TEXT("%s.%s"), *PackagePath, *AssetName);
}

void NormalizeAssetPathVariants(
	const FString& InPath,
	FString& OutAssetPath,
	FString& OutObjectPath)
{
	OutAssetPath = InPath;
	OutObjectPath = InPath;

	FString PackagePath;
	FString AssetName;
	if (TrySplitObjectPath(InPath, PackagePath, AssetName))
	{
		OutAssetPath = PackagePath;
		OutObjectPath = ToObjectPath(PackagePath, AssetName);
	}
}

FString GetDirectoryFromPackagePath(const FString& PackagePath)
{
	int32 LastSlash = INDEX_NONE;
	if (!PackagePath.FindLastChar(TEXT('/'), LastSlash) || LastSlash <= 0)
	{
		return FString();
	}
	return PackagePath.Left(LastSlash);
}

FString TrimTrailingSlash(const FString& InPath)
{
	FString Out = InPath;
	while (Out.EndsWith(TEXT("/")))
	{
		Out.LeftChopInline(1, false);
	}
	return Out;
}

FString GetDirectoryLeafName(const FString& Path)
{
	int32 LastSlash = INDEX_NONE;
	if (!Path.FindLastChar(TEXT('/'), LastSlash) || LastSlash < 0 || LastSlash + 1 >= Path.Len())
	{
		return Path;
	}
	return Path.Mid(LastSlash + 1);
}
}


