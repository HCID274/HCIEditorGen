#pragma once

#include "CoreMinimal.h"

struct FHCIAbilityKitAgentPlan;
class FHCIAbilityKitToolRegistry;

enum class EHCIAbilityKitAgentPlannerLlmMockMode : uint8
{
	None,
	Timeout,
	InvalidJson,
	ContractInvalid,
	EmptyResponse
};

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentPlannerBuildOptions
{
	bool bPreferLlm = false;
	EHCIAbilityKitAgentPlannerLlmMockMode LlmMockMode = EHCIAbilityKitAgentPlannerLlmMockMode::None;
};

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentPlannerResultMetadata
{
	FString PlannerProvider = TEXT("keyword_fallback");
	bool bFallbackUsed = false;
	FString FallbackReason = TEXT("none");
	FString ErrorCode = TEXT("-");
};

class HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentPlanner
{
public:
	static bool BuildPlanFromNaturalLanguage(
		const FString& UserText,
		const FString& RequestId,
		const FHCIAbilityKitToolRegistry& ToolRegistry,
		FHCIAbilityKitAgentPlan& OutPlan,
		FString& OutRouteReason,
		FString& OutError);

	static bool BuildPlanFromNaturalLanguageWithProvider(
		const FString& UserText,
		const FString& RequestId,
		const FHCIAbilityKitToolRegistry& ToolRegistry,
		const FHCIAbilityKitAgentPlannerBuildOptions& Options,
		FHCIAbilityKitAgentPlan& OutPlan,
		FString& OutRouteReason,
		FHCIAbilityKitAgentPlannerResultMetadata& OutMetadata,
		FString& OutError);
};
