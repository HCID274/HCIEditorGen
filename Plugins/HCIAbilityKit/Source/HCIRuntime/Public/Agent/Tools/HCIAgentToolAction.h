#pragma once

#include "CoreMinimal.h"

class FJsonObject;

struct HCIRUNTIME_API FHCIAgentToolActionRequest
{
	FString RequestId;
	FString StepId;
	FName ToolName;
	TSharedPtr<FJsonObject> Args;
};

struct HCIRUNTIME_API FHCIAgentToolActionResult
{
	bool bSucceeded = false;
	FString ErrorCode;
	FString Reason;
	int32 EstimatedAffectedCount = 0;
	TMap<FString, FString> Evidence;
};

class HCIRUNTIME_API IHCIAgentToolAction
{
public:
	virtual ~IHCIAgentToolAction() = default;

	virtual FName GetToolName() const = 0;

	// Dry-run mode: produce preview evidence, no real write.
	virtual bool DryRun(
		const FHCIAgentToolActionRequest& Request,
		FHCIAgentToolActionResult& OutResult) const = 0;

	// Execute mode: perform real tool action.
	virtual bool Execute(
		const FHCIAgentToolActionRequest& Request,
		FHCIAgentToolActionResult& OutResult) const = 0;
};


