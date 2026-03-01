#pragma once

#include "CoreMinimal.h"
#include "Search/HCISearchSchema.h"

enum class EHCIAbilityDamagePreference : uint8
{
	Any,
	PreferLow,
	PreferMedium,
	PreferHigh
};

struct HCIRUNTIME_API FHCIAbilitySearchQuery
{
	FString RawText;
	FString NormalizedText;
	int32 TopK = 5;
	TOptional<EHCIAbilityElement> RequiredElement;
	TOptional<EHCIAbilityControlProfile> RequiredControlProfile;
	TArray<EHCIAbilityUsageScene> RequiredScenes;
	EHCIAbilityDamagePreference DamagePreference = EHCIAbilityDamagePreference::Any;
	bool bRequireAnyControl = false;
	bool bRequireLowerDamageThanReference = false;
	FString SimilarToId;
	TArray<FString> QueryTokens;

	FString ToSummaryString() const;
};

struct HCIRUNTIME_API FHCIAbilitySearchCandidate
{
	FString Id;
	float Score = 0.0f;
};

struct HCIRUNTIME_API FHCIAbilitySearchResult
{
	FHCIAbilitySearchQuery ParsedQuery;
	TArray<FHCIAbilitySearchCandidate> Candidates;
	TArray<FString> Suggestions;

	bool HasResults() const;
	FString ToSummaryString() const;
};

class HCIRUNTIME_API FHCISearchQueryService
{
public:
	static FHCIAbilitySearchQuery ParseQuery(
		const FString& UserQuery,
		int32 TopK,
		const FHCIAbilitySearchIndex& Index);

	static FHCIAbilitySearchResult RunQuery(
		const FString& UserQuery,
		int32 TopK,
		const FHCIAbilitySearchIndex& Index);
};


