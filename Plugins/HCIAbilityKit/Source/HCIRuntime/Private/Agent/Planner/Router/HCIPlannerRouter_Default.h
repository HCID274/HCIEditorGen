#pragma once

#include "Agent/Planner/Interfaces/IHCIPlannerRouter.h"

#include "HAL/CriticalSection.h"

class IHCIPlannerProvider;

struct FHCIPlannerRouterRuntimeState
{
	int32 ConsecutiveLlmFailures = 0;
	int32 CircuitOpenRemainingRequests = 0;
	FHCIAgentPlannerMetricsSnapshot Metrics;
};

class FHCIPlannerRouter_Default final : public IHCIPlannerRouter
{
public:
	FHCIPlannerRouter_Default(
		const TSharedRef<IHCIPlannerProvider>& InLlmProvider,
		const TSharedRef<IHCIPlannerProvider>& InKeywordProvider);

	virtual TSharedRef<IHCIPlannerProvider> SelectProvider(
		const FHCIAgentPlannerBuildOptions& Options) override;

	virtual FHCIAgentPlannerMetricsSnapshot GetMetricsSnapshot() const override;
	virtual void ResetMetricsForTesting() override;

private:
	TSharedRef<IHCIPlannerProvider> LlmProvider;
	TSharedRef<IHCIPlannerProvider> KeywordProvider;

	mutable FCriticalSection RuntimeStateMutex;
	FHCIPlannerRouterRuntimeState RuntimeState;
};

