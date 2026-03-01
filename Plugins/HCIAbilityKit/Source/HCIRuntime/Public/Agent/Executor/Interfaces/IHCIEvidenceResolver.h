#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"

class FJsonObject;
struct FHCIAgentPlanStep;
class IHCIEvidenceContextView;

struct HCIRUNTIME_API FHCIEvidenceResolveError
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

class HCIRUNTIME_API IHCIEvidenceResolver
{
public:
	virtual ~IHCIEvidenceResolver() = default;

	// Fast scan to decide whether ResolveStepArgs should run.
	virtual bool StepArgsMayContainTemplates(const TSharedPtr<FJsonObject>& Args) const = 0;

	// Deep module: resolves variable templates inside step args (e.g. "{{step_1_scan.asset_paths}}"),
	// including array expansion and nested JSON traversal.
	virtual bool ResolveStepArgs(
		const FHCIAgentPlanStep& InStep,
		const IHCIEvidenceContextView& EvidenceContext,
		FHCIAgentPlanStep& OutResolvedStep,
		FHCIEvidenceResolveError& OutError) const = 0;
};



