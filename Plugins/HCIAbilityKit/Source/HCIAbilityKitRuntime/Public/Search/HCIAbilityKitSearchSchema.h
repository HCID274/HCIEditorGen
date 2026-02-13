#pragma once

#include "CoreMinimal.h"
#include "Services/HCIAbilityKitParserService.h"

enum class EHCIAbilityElement : uint8
{
	Unknown,
	Fire,
	Ice,
	Nature,
	Lightning,
	Physical
};

enum class EHCIAbilityDamageTier : uint8
{
	Low,
	Medium,
	High
};

enum class EHCIAbilityControlProfile : uint8
{
	None,
	SoftControl,
	HardControl
};

enum class EHCIAbilityUsageScene : uint8
{
	General,
	Forest,
	BossPhase2
};

/**
 * 检索文档：用于承载可检索的语义字段喵。
 */
struct HCIABILITYKITRUNTIME_API FHCIAbilitySearchDocument
{
	FString AssetPath;
	FString Id;
	FString DisplayName;
	float Damage = 0.0f;
	EHCIAbilityElement Element = EHCIAbilityElement::Unknown;
	EHCIAbilityDamageTier DamageTier = EHCIAbilityDamageTier::Low;
	EHCIAbilityControlProfile ControlProfile = EHCIAbilityControlProfile::None;
	TArray<EHCIAbilityUsageScene> UsageScenes;
	TArray<FString> Tokens;

	bool IsValid() const;
};

/**
 * 索引结构：当前只定义结构，不包含资产扫描与刷新流程喵。
 */
struct HCIABILITYKITRUNTIME_API FHCIAbilitySearchIndex
{
	TMap<FString, FHCIAbilitySearchDocument> DocumentsById;
	TMap<EHCIAbilityElement, TArray<FString>> IdsByElement;
	TMap<EHCIAbilityDamageTier, TArray<FString>> IdsByDamageTier;
	TMap<EHCIAbilityControlProfile, TArray<FString>> IdsByControlProfile;
	TMap<EHCIAbilityUsageScene, TArray<FString>> IdsByUsageScene;
	TMap<FString, TArray<FString>> IdsByToken;

	void Reset();
	int32 GetDocumentCount() const;
	bool ContainsId(const FString& InId) const;
	bool AddDocument(const FHCIAbilitySearchDocument& InDocument);
	bool RemoveDocumentById(const FString& InId);
};

/**
 * 语义规范器：把解析数据映射为检索文档喵。
 */
class HCIABILITYKITRUNTIME_API FHCIAbilitySearchSchema
{
public:
	static FHCIAbilitySearchDocument BuildDocument(
		const FHCIAbilityKitParsedData& ParsedData,
		const FString& AssetPath);

	static EHCIAbilityElement ResolveElement(const FString& Id, const FString& DisplayName);
	static EHCIAbilityDamageTier ResolveDamageTier(float Damage);
	static EHCIAbilityControlProfile ResolveControlProfile(const FString& Id, const FString& DisplayName);
	static TArray<EHCIAbilityUsageScene> ResolveUsageScenes(const FString& Id, const FString& DisplayName);
	static TArray<FString> BuildTokens(
		const FString& Id,
		const FString& DisplayName,
		EHCIAbilityElement Element,
		EHCIAbilityDamageTier DamageTier,
		EHCIAbilityControlProfile ControlProfile,
		const TArray<EHCIAbilityUsageScene>& UsageScenes);
};
