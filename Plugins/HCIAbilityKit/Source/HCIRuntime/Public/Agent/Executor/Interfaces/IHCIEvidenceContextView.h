#pragma once

#include "CoreMinimal.h"

class HCIRUNTIME_API IHCIEvidenceContextView
{
public:
	virtual ~IHCIEvidenceContextView() = default;

	// Returns whether the given StepId exists in the evidence context.
	virtual bool HasStep(const FString& StepId) const = 0;

	// Returns evidence value for (StepId, EvidenceKey). The empty string is treated as "not found".
	virtual bool TryGetEvidenceValue(
		const FString& StepId,
		const FString& EvidenceKey,
		FString& OutValue) const = 0;
};



