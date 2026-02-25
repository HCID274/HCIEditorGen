#pragma once

#include "CoreMinimal.h"

class FJsonObject;

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentToolActionRequest
{
	FString RequestId;
	FString StepId;
	FName ToolName;
	TSharedPtr<FJsonObject> Args;
};

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentToolActionResult
{
	bool bSucceeded = false;
	FString ErrorCode;
	FString Reason;
	int32 EstimatedAffectedCount = 0;
	TMap<FString, FString> Evidence;
};

class HCIABILITYKITRUNTIME_API IHCIAbilityKitAgentToolAction
{
public:
	virtual ~IHCIAbilityKitAgentToolAction() = default;

	virtual FName GetToolName() const = 0;

	// Dry-run mode: produce preview evidence, no real write.
	virtual bool DryRun(
		const FHCIAbilityKitAgentToolActionRequest& Request,
		FHCIAbilityKitAgentToolActionResult& OutResult) const = 0;

	// Execute mode: perform real tool action.
	virtual bool Execute(
		const FHCIAbilityKitAgentToolActionRequest& Request,
		FHCIAbilityKitAgentToolActionResult& OutResult) const = 0;
};
