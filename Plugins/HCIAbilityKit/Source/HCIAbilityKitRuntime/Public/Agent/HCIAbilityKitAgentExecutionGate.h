#pragma once

#include "CoreMinimal.h"

#include "Agent/HCIAbilityKitToolRegistry.h"

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentExecutionStep
{
	FString StepId;
	FName ToolName;
	bool bRequiresConfirm = false;
	bool bUserConfirmed = false;
};

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentExecutionDecision
{
	bool bAllowed = false;
	FString ErrorCode;
	FString Reason;

	FString StepId;
	FString ToolName;
	FString Capability;
	bool bWriteLike = false;
	bool bRequiresConfirm = false;
	bool bUserConfirmed = false;
};

class HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentExecutionGate
{
public:
	static FHCIAbilityKitAgentExecutionDecision EvaluateConfirmGate(
		const FHCIAbilityKitAgentExecutionStep& Step,
		const FHCIAbilityKitToolRegistry& Registry);

	static bool IsWriteLikeCapability(EHCIAbilityKitToolCapability Capability);
};

