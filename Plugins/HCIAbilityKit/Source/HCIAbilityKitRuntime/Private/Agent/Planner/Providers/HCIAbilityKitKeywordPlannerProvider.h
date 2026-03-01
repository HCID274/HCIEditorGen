#pragma once

#include "Agent/Planner/Interfaces/IHCIAbilityKitPlannerProvider.h"

class FHCIAbilityKitKeywordPlannerProvider final : public IHCIAbilityKitPlannerProvider
{
public:
	virtual const TCHAR* GetName() const override;
	virtual bool IsNetworkBacked() const override;

	virtual bool BuildPlan(
		const FString& UserText,
		const FString& RequestId,
		const FHCIAbilityKitToolRegistry& ToolRegistry,
		const FHCIAbilityKitAgentPlannerBuildOptions& Options,
		FHCIAbilityKitAgentPlan& OutPlan,
		FString& OutRouteReason,
		FHCIAbilityKitAgentPlannerResultMetadata& OutMetadata,
		FString& OutError) override;

	virtual void BuildPlanAsync(
		const FString& UserText,
		const FString& RequestId,
		const FHCIAbilityKitToolRegistry& ToolRegistry,
		const FHCIAbilityKitAgentPlannerBuildOptions& Options,
		TFunction<void(bool, FHCIAbilityKitAgentPlan, FString, FHCIAbilityKitAgentPlannerResultMetadata, FString)>&& OnComplete) override;
};

