#include "Agent/Planner/HCIAgentPlanner.h"

#include "HCIRuntimeModule.h"

#include "Agent/Planner/Interfaces/IHCIPlannerProvider.h"
#include "Agent/Planner/Interfaces/IHCIPlannerRouter.h"
#include "Agent/Planner/Providers/HCIKeywordPlannerProvider.h"

bool FHCIAgentPlanner::BuildPlanFromNaturalLanguage(
	const FString& UserText,
	const FString& RequestId,
	const FHCIToolRegistry& ToolRegistry,
	FHCIAgentPlan& OutPlan,
	FString& OutRouteReason,
	FString& OutError)
{
	FHCIKeywordPlannerProvider KeywordProvider;
	FHCIAgentPlannerBuildOptions Options;
	FHCIAgentPlannerResultMetadata Metadata;
	return KeywordProvider.BuildPlan(UserText, RequestId, ToolRegistry, Options, OutPlan, OutRouteReason, Metadata, OutError);
}

bool FHCIAgentPlanner::BuildPlanFromNaturalLanguageWithProvider(
	const FString& UserText,
	const FString& RequestId,
	const FHCIToolRegistry& ToolRegistry,
	const FHCIAgentPlannerBuildOptions& Options,
	FHCIAgentPlan& OutPlan,
	FString& OutRouteReason,
	FHCIAgentPlannerResultMetadata& OutMetadata,
	FString& OutError)
{
	const TSharedRef<IHCIPlannerProvider> Provider = FHCIRuntimeModule::Get().GetPlannerRouter()->SelectProvider(Options);
	return Provider->BuildPlan(UserText, RequestId, ToolRegistry, Options, OutPlan, OutRouteReason, OutMetadata, OutError);
}

void FHCIAgentPlanner::BuildPlanFromNaturalLanguageWithProviderAsync(
	const FString& UserText,
	const FString& RequestId,
	const FHCIToolRegistry& ToolRegistry,
	const FHCIAgentPlannerBuildOptions& Options,
	TFunction<void(bool, FHCIAgentPlan, FString, FHCIAgentPlannerResultMetadata, FString)>&& OnComplete)
{
	if (!OnComplete)
	{
		return;
	}

	const TSharedRef<IHCIPlannerProvider> Provider = FHCIRuntimeModule::Get().GetPlannerRouter()->SelectProvider(Options);
	Provider->BuildPlanAsync(UserText, RequestId, ToolRegistry, Options, MoveTemp(OnComplete));
}

FHCIAgentPlannerMetricsSnapshot FHCIAgentPlanner::GetMetricsSnapshot()
{
	return FHCIRuntimeModule::Get().GetPlannerRouter()->GetMetricsSnapshot();
}

void FHCIAgentPlanner::ResetMetricsForTesting()
{
	FHCIRuntimeModule::Get().GetPlannerRouter()->ResetMetricsForTesting();
}

