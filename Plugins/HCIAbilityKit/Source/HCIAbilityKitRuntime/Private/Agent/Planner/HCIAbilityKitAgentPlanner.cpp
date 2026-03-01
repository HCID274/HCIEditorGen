#include "Agent/Planner/HCIAbilityKitAgentPlanner.h"

#include "HCIAbilityKitRuntimeModule.h"

#include "Agent/Planner/Interfaces/IHCIAbilityKitPlannerProvider.h"
#include "Agent/Planner/Interfaces/IHCIAbilityKitPlannerRouter.h"
#include "Agent/Planner/Providers/HCIAbilityKitKeywordPlannerProvider.h"

bool FHCIAbilityKitAgentPlanner::BuildPlanFromNaturalLanguage(
	const FString& UserText,
	const FString& RequestId,
	const FHCIAbilityKitToolRegistry& ToolRegistry,
	FHCIAbilityKitAgentPlan& OutPlan,
	FString& OutRouteReason,
	FString& OutError)
{
	FHCIAbilityKitKeywordPlannerProvider KeywordProvider;
	FHCIAbilityKitAgentPlannerBuildOptions Options;
	FHCIAbilityKitAgentPlannerResultMetadata Metadata;
	return KeywordProvider.BuildPlan(UserText, RequestId, ToolRegistry, Options, OutPlan, OutRouteReason, Metadata, OutError);
}

bool FHCIAbilityKitAgentPlanner::BuildPlanFromNaturalLanguageWithProvider(
	const FString& UserText,
	const FString& RequestId,
	const FHCIAbilityKitToolRegistry& ToolRegistry,
	const FHCIAbilityKitAgentPlannerBuildOptions& Options,
	FHCIAbilityKitAgentPlan& OutPlan,
	FString& OutRouteReason,
	FHCIAbilityKitAgentPlannerResultMetadata& OutMetadata,
	FString& OutError)
{
	const TSharedRef<IHCIAbilityKitPlannerProvider> Provider = FHCIAbilityKitRuntimeModule::Get().GetPlannerRouter()->SelectProvider(Options);
	return Provider->BuildPlan(UserText, RequestId, ToolRegistry, Options, OutPlan, OutRouteReason, OutMetadata, OutError);
}

void FHCIAbilityKitAgentPlanner::BuildPlanFromNaturalLanguageWithProviderAsync(
	const FString& UserText,
	const FString& RequestId,
	const FHCIAbilityKitToolRegistry& ToolRegistry,
	const FHCIAbilityKitAgentPlannerBuildOptions& Options,
	TFunction<void(bool, FHCIAbilityKitAgentPlan, FString, FHCIAbilityKitAgentPlannerResultMetadata, FString)>&& OnComplete)
{
	if (!OnComplete)
	{
		return;
	}

	const TSharedRef<IHCIAbilityKitPlannerProvider> Provider = FHCIAbilityKitRuntimeModule::Get().GetPlannerRouter()->SelectProvider(Options);
	Provider->BuildPlanAsync(UserText, RequestId, ToolRegistry, Options, MoveTemp(OnComplete));
}

FHCIAbilityKitAgentPlannerMetricsSnapshot FHCIAbilityKitAgentPlanner::GetMetricsSnapshot()
{
	return FHCIAbilityKitRuntimeModule::Get().GetPlannerRouter()->GetMetricsSnapshot();
}

void FHCIAbilityKitAgentPlanner::ResetMetricsForTesting()
{
	FHCIAbilityKitRuntimeModule::Get().GetPlannerRouter()->ResetMetricsForTesting();
}
