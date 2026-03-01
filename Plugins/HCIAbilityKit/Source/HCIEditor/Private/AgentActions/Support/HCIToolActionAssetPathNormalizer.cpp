#include "AgentActions/Support/HCIToolActionAssetPathNormalizer.h"

#include "AgentActions/Support/HCIAssetPathUtils.h"

void FHCIToolActionAssetPathNormalizer::NormalizeAssetPathVariants(
	const FString& InPath,
	FString& OutAssetPath,
	FString& OutObjectPath)
{
	HCIAssetPathUtils::NormalizeAssetPathVariants(InPath, OutAssetPath, OutObjectPath);
}

FString FHCIToolActionAssetPathNormalizer::TrimTrailingSlash(const FString& InPath)
{
	return HCIAssetPathUtils::TrimTrailingSlash(InPath);
}

bool FHCIToolActionAssetPathNormalizer::IsGamePathLoose(const FString& Path)
{
	return Path.StartsWith(TEXT("/Game"), ESearchCase::CaseSensitive);
}

bool FHCIToolActionAssetPathNormalizer::IsGamePathStrict(const FString& Path)
{
	return Path.StartsWith(TEXT("/Game/"), ESearchCase::CaseSensitive);
}


