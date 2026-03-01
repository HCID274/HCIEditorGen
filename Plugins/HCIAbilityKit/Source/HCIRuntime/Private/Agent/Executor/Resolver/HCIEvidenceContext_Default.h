#pragma once

#include "Agent/Executor/Interfaces/IHCIEvidenceContextView.h"

class FHCIEvidenceContext_Default final : public IHCIEvidenceContextView
{
public:
	explicit FHCIEvidenceContext_Default(const TMap<FString, TMap<FString, FString>>& InStepEvidenceContext)
		: StepEvidenceContext(&InStepEvidenceContext)
	{
	}

	virtual bool HasStep(const FString& StepId) const override
	{
		return StepEvidenceContext != nullptr && StepEvidenceContext->Contains(StepId);
	}

	virtual bool TryGetEvidenceValue(const FString& StepId, const FString& EvidenceKey, FString& OutValue) const override
	{
		OutValue.Reset();
		if (StepEvidenceContext == nullptr)
		{
			return false;
		}

		const TMap<FString, FString>* Evidence = StepEvidenceContext->Find(StepId);
		if (Evidence == nullptr)
		{
			return false;
		}

		if (const FString* Found = Evidence->Find(EvidenceKey))
		{
			OutValue = *Found;
			return !OutValue.IsEmpty();
		}
		return false;
	}

private:
	const TMap<FString, TMap<FString, FString>>* StepEvidenceContext = nullptr;
};


