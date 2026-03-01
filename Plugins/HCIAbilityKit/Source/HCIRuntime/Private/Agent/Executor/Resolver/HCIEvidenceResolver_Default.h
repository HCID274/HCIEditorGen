#pragma once

#include "Agent/Executor/Interfaces/IHCIEvidenceResolver.h"

class IHCIEvidenceContextView;

class FHCIEvidenceResolver_Default final : public IHCIEvidenceResolver
{
public:
	virtual bool StepArgsMayContainTemplates(const TSharedPtr<FJsonObject>& Args) const override;

	virtual bool ResolveStepArgs(
		const FHCIAgentPlanStep& InStep,
		const IHCIEvidenceContextView& EvidenceContext,
		FHCIAgentPlanStep& OutResolvedStep,
		FHCIEvidenceResolveError& OutError) const override;
};


