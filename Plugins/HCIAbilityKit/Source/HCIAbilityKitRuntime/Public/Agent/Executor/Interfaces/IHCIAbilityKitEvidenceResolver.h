#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"

class FJsonObject;
struct FHCIAbilityKitAgentPlanStep;
class IHCIAbilityKitEvidenceContextView;

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitEvidenceResolveError
{
	FString ErrorCode;
	FString Reason;
	FString Detail;

	void Reset()
	{
		ErrorCode.Reset();
		Reason.Reset();
		Detail.Reset();
	}
};

class HCIABILITYKITRUNTIME_API IHCIAbilityKitEvidenceResolver
{
public:
	virtual ~IHCIAbilityKitEvidenceResolver() = default;

	// Fast scan to decide whether ResolveStepArgs should run.
	virtual bool StepArgsMayContainTemplates(const TSharedPtr<FJsonObject>& Args) const = 0;

	// Deep module: resolves variable templates inside step args (e.g. "{{step_1_scan.asset_paths}}"),
	// including array expansion and nested JSON traversal.
	virtual bool ResolveStepArgs(
		const FHCIAbilityKitAgentPlanStep& InStep,
		const IHCIAbilityKitEvidenceContextView& EvidenceContext,
		FHCIAbilityKitAgentPlanStep& OutResolvedStep,
		FHCIAbilityKitEvidenceResolveError& OutError) const = 0;
};

