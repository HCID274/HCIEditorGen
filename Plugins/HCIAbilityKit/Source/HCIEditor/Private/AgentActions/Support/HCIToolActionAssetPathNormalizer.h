#pragma once

#include "CoreMinimal.h"

class FHCIToolActionAssetPathNormalizer
{
public:
	static void NormalizeAssetPathVariants(const FString& InPath, FString& OutAssetPath, FString& OutObjectPath);
	static FString TrimTrailingSlash(const FString& InPath);
	static bool IsGamePathLoose(const FString& Path); // "/Game" 前缀（与历史实现一致）
	static bool IsGamePathStrict(const FString& Path); // "/Game/" 前缀
};

