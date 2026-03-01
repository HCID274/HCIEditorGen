#include "Agent/Planner/Providers/HCIAbilityKitKeywordPlannerProvider.h"

const TCHAR* FHCIAbilityKitKeywordPlannerProvider::GetName() const
{
	return TEXT("keyword_fallback");
}

bool FHCIAbilityKitKeywordPlannerProvider::IsNetworkBacked() const
{
	return false;
}

bool FHCIAbilityKitKeywordPlannerProvider::BuildPlan(
	const FString& UserText,
	const FString& RequestId,
	const FHCIAbilityKitToolRegistry& ToolRegistry,
	const FHCIAbilityKitAgentPlannerBuildOptions& Options,
	FHCIAbilityKitAgentPlan& OutPlan,
	FString& OutRouteReason,
	FHCIAbilityKitAgentPlannerResultMetadata& OutMetadata,
	FString& OutError)
{
	(void)UserText;
	(void)RequestId;
	(void)ToolRegistry;
	(void)Options;
	(void)OutPlan;
	(void)OutRouteReason;
	(void)OutMetadata;

	OutError = TEXT("keyword_planner_provider_not_wired");
	return false;
}

void FHCIAbilityKitKeywordPlannerProvider::BuildPlanAsync(
	const FString& UserText,
	const FString& RequestId,
	const FHCIAbilityKitToolRegistry& ToolRegistry,
	const FHCIAbilityKitAgentPlannerBuildOptions& Options,
	TFunction<void(bool, FHCIAbilityKitAgentPlan, FString, FHCIAbilityKitAgentPlannerResultMetadata, FString)>&& OnComplete)
{
	FHCIAbilityKitAgentPlan Plan;
	FString RouteReason;
	FHCIAbilityKitAgentPlannerResultMetadata Metadata;
	FString Error;
	const bool bBuilt = BuildPlan(UserText, RequestId, ToolRegistry, Options, Plan, RouteReason, Metadata, Error);
	OnComplete(bBuilt, MoveTemp(Plan), MoveTemp(RouteReason), MoveTemp(Metadata), MoveTemp(Error));
}

