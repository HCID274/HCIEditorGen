#pragma once

#include "Agent/Planner/Interfaces/IHCIPlannerProvider.h"

class FHCIKeywordPlannerProvider final : public IHCIPlannerProvider
{
public:
	virtual const TCHAR* GetName() const override;
	virtual bool IsNetworkBacked() const override;

	virtual bool BuildPlan(
		const FString& UserText,
		const FString& RequestId,
		const FHCIToolRegistry& ToolRegistry,
		const FHCIAgentPlannerBuildOptions& Options,
		FHCIAgentPlan& OutPlan,
		FString& OutRouteReason,
		FHCIAgentPlannerResultMetadata& OutMetadata,
		FString& OutError) override;

	virtual void BuildPlanAsync(
		const FString& UserText,
		const FString& RequestId,
		const FHCIToolRegistry& ToolRegistry,
		const FHCIAgentPlannerBuildOptions& Options,
		TFunction<void(bool, FHCIAgentPlan, FString, FHCIAgentPlannerResultMetadata, FString)>&& OnComplete) override;
};


