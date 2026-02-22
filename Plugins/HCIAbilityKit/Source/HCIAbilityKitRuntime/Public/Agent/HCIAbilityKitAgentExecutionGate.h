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

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentBlastRadiusCheckInput
{
	FString RequestId;
	FName ToolName;
	int32 TargetModifyCount = 0;
};

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentBlastRadiusDecision
{
	bool bAllowed = false;
	FString ErrorCode;
	FString Reason;

	FString RequestId;
	FString ToolName;
	FString Capability;
	bool bWriteLike = false;
	int32 TargetModifyCount = 0;
	int32 MaxAssetModifyLimit = 0;
};

class HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentExecutionGate
{
public:
	static constexpr int32 MaxAssetModifyLimit = 50;

	static FHCIAbilityKitAgentExecutionDecision EvaluateConfirmGate(
		const FHCIAbilityKitAgentExecutionStep& Step,
		const FHCIAbilityKitToolRegistry& Registry);

	static FHCIAbilityKitAgentBlastRadiusDecision EvaluateBlastRadius(
		const FHCIAbilityKitAgentBlastRadiusCheckInput& Input,
		const FHCIAbilityKitToolRegistry& Registry);

	static bool IsWriteLikeCapability(EHCIAbilityKitToolCapability Capability);
};
