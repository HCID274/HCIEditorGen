#pragma once

#include "Agent/Planner/Interfaces/IHCIAbilityKitPlannerRouter.h"

class IHCIAbilityKitPlannerProvider;

class FHCIAbilityKitPlannerRouter_Default final : public IHCIAbilityKitPlannerRouter
{
public:
	virtual TSharedRef<IHCIAbilityKitPlannerProvider> SelectProvider(
		const FHCIAbilityKitAgentPlannerBuildOptions& Options) override;

	static FHCIAbilityKitAgentPlannerMetricsSnapshot GetMetricsSnapshot();
	static void ResetMetricsForTesting();
};

