#pragma once

#include "Agent/Executor/Interfaces/IHCIAbilityKitEvidenceResolver.h"

class IHCIAbilityKitEvidenceContextView;

class FHCIAbilityKitEvidenceResolver_Default final : public IHCIAbilityKitEvidenceResolver
{
public:
	virtual bool StepArgsMayContainTemplates(const TSharedPtr<FJsonObject>& Args) const override;

	virtual bool ResolveStepArgs(
		const FHCIAbilityKitAgentPlanStep& InStep,
		const IHCIAbilityKitEvidenceContextView& EvidenceContext,
		FHCIAbilityKitAgentPlanStep& OutResolvedStep,
		FHCIAbilityKitEvidenceResolveError& OutError) const override;
};

