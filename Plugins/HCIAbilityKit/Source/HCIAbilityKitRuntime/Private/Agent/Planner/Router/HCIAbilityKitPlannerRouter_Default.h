#pragma once

#include "Agent/Planner/Interfaces/IHCIAbilityKitPlannerRouter.h"

#include "HAL/CriticalSection.h"

class IHCIAbilityKitPlannerProvider;

struct FHCIAbilityKitPlannerRouterRuntimeState
{
	int32 ConsecutiveLlmFailures = 0;
	int32 CircuitOpenRemainingRequests = 0;
	FHCIAbilityKitAgentPlannerMetricsSnapshot Metrics;
};

class FHCIAbilityKitPlannerRouter_Default final : public IHCIAbilityKitPlannerRouter
{
public:
	FHCIAbilityKitPlannerRouter_Default(
		const TSharedRef<IHCIAbilityKitPlannerProvider>& InLlmProvider,
		const TSharedRef<IHCIAbilityKitPlannerProvider>& InKeywordProvider);

	virtual TSharedRef<IHCIAbilityKitPlannerProvider> SelectProvider(
		const FHCIAbilityKitAgentPlannerBuildOptions& Options) override;

	virtual FHCIAbilityKitAgentPlannerMetricsSnapshot GetMetricsSnapshot() const override;
	virtual void ResetMetricsForTesting() override;

private:
	TSharedRef<IHCIAbilityKitPlannerProvider> LlmProvider;
	TSharedRef<IHCIAbilityKitPlannerProvider> KeywordProvider;

	mutable FCriticalSection RuntimeStateMutex;
	FHCIAbilityKitPlannerRouterRuntimeState RuntimeState;
};
