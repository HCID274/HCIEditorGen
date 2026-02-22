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

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentTransactionStepSimulation
{
	FString StepId;
	FName ToolName;
	bool bShouldSucceed = true;
};

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentTransactionExecutionInput
{
	FString RequestId;
	TArray<FHCIAbilityKitAgentTransactionStepSimulation> Steps;
};

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentTransactionExecutionDecision
{
	bool bCommitted = false;
	bool bRolledBack = false;
	FString ErrorCode;
	FString Reason;

	FString RequestId;
	FString TransactionMode;
	int32 TotalSteps = 0;
	int32 ExecutedSteps = 0;
	int32 CommittedSteps = 0;
	int32 RolledBackSteps = 0;
	int32 FailedStepIndex = -1; // 1-based; -1 means no failure
	FString FailedStepId;
	FString FailedToolName;
};

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentSourceControlCheckInput
{
	FString RequestId;
	FName ToolName;
	bool bSourceControlEnabled = false;
	bool bCheckoutSucceeded = false; // Only consulted when source control is enabled and tool is write-like.
};

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentSourceControlDecision
{
	bool bAllowed = false;
	FString ErrorCode;
	FString Reason;

	FString RequestId;
	FString ToolName;
	FString Capability;
	bool bWriteLike = false;
	bool bSourceControlEnabled = false;
	bool bFailFastPolicy = true;
	bool bOfflineLocalMode = false;
	bool bCheckoutAttempted = false;
	bool bCheckoutSucceeded = false;
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

	static FHCIAbilityKitAgentTransactionExecutionDecision EvaluateAllOrNothingTransaction(
		const FHCIAbilityKitAgentTransactionExecutionInput& Input,
		const FHCIAbilityKitToolRegistry& Registry);

	static FHCIAbilityKitAgentSourceControlDecision EvaluateSourceControlFailFast(
		const FHCIAbilityKitAgentSourceControlCheckInput& Input,
		const FHCIAbilityKitToolRegistry& Registry);

	static bool IsWriteLikeCapability(EHCIAbilityKitToolCapability Capability);
};
