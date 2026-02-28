#include "AgentActions/Support/HCIAbilityKitToolActionAssetPathNormalizer.h"

#include "AgentActions/Support/HCIAbilityKitAssetPathUtils.h"

void FHCIAbilityKitToolActionAssetPathNormalizer::NormalizeAssetPathVariants(
	const FString& InPath,
	FString& OutAssetPath,
	FString& OutObjectPath)
{
	HCIAbilityKitAssetPathUtils::NormalizeAssetPathVariants(InPath, OutAssetPath, OutObjectPath);
}

FString FHCIAbilityKitToolActionAssetPathNormalizer::TrimTrailingSlash(const FString& InPath)
{
	return HCIAbilityKitAssetPathUtils::TrimTrailingSlash(InPath);
}

bool FHCIAbilityKitToolActionAssetPathNormalizer::IsGamePathLoose(const FString& Path)
{
	return Path.StartsWith(TEXT("/Game"), ESearchCase::CaseSensitive);
}

bool FHCIAbilityKitToolActionAssetPathNormalizer::IsGamePathStrict(const FString& Path)
{
	return Path.StartsWith(TEXT("/Game/"), ESearchCase::CaseSensitive);
}

