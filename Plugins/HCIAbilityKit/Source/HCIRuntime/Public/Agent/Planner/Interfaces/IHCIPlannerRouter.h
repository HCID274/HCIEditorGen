#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"

#include "Agent/Planner/HCIAgentPlanner.h"

class IHCIPlannerProvider;

class HCIRUNTIME_API IHCIPlannerRouter
{
public:
	virtual ~IHCIPlannerRouter() = default;

	virtual TSharedRef<IHCIPlannerProvider> SelectProvider(
		const FHCIAgentPlannerBuildOptions& Options) = 0;

	virtual FHCIAgentPlannerMetricsSnapshot GetMetricsSnapshot() const = 0;
	virtual void ResetMetricsForTesting() = 0;
};


