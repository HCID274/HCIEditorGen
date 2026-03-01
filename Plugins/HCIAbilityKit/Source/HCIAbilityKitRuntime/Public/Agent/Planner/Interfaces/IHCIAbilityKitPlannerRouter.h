#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"

#include "Agent/Planner/HCIAbilityKitAgentPlanner.h"

class IHCIAbilityKitPlannerProvider;

class HCIABILITYKITRUNTIME_API IHCIAbilityKitPlannerRouter
{
public:
	virtual ~IHCIAbilityKitPlannerRouter() = default;

	virtual TSharedRef<IHCIAbilityKitPlannerProvider> SelectProvider(
		const FHCIAbilityKitAgentPlannerBuildOptions& Options) = 0;

	virtual FHCIAbilityKitAgentPlannerMetricsSnapshot GetMetricsSnapshot() const = 0;
	virtual void ResetMetricsForTesting() = 0;
};
