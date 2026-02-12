#pragma once

#include "CoreMinimal.h"

/**
 * AbilityKit 源文件解析后的标准化数据结构。
 */
struct HCIABILITYKITRUNTIME_API FHCIAbilityKitParsedData
{
	int32 SchemaVersion = 1;
	FString Id;
	FString DisplayName;
	float Damage = 0.0f;
};

/**
 * AbilityKit 解析服务：负责读取与校验 .hciabilitykit 源文件。
 */
class HCIABILITYKITRUNTIME_API FHCIAbilityKitParserService
{
public:
	static bool TryParseKitFile(const FString& FullFilename, FHCIAbilityKitParsedData& OutParsed, FString& OutError);
};
