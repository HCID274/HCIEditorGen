#pragma once

#include "CoreMinimal.h"

#include "Agent/Tools/HCIToolRegistry.h"

struct HCIRUNTIME_API FHCIAgentExecutionStep
{
	FString StepId;
	FName ToolName;
	bool bRequiresConfirm = false;
	bool bUserConfirmed = false;
};

struct HCIRUNTIME_API FHCIAgentExecutionDecision
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

struct HCIRUNTIME_API FHCIAgentBlastRadiusCheckInput
{
	FString RequestId;
	FName ToolName;
	int32 TargetModifyCount = 0;
};

struct HCIRUNTIME_API FHCIAgentBlastRadiusDecision
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

struct HCIRUNTIME_API FHCIAgentTransactionStepSimulation
{
	FString StepId;
	FName ToolName;
	bool bShouldSucceed = true;
};

struct HCIRUNTIME_API FHCIAgentTransactionExecutionInput
{
	FString RequestId;
	TArray<FHCIAgentTransactionStepSimulation> Steps;
};

struct HCIRUNTIME_API FHCIAgentTransactionExecutionDecision
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

struct HCIRUNTIME_API FHCIAgentSourceControlCheckInput
{
	FString RequestId;
	FName ToolName;
	bool bSourceControlEnabled = false;
	bool bCheckoutSucceeded = false; // Only consulted when source control is enabled and tool is write-like.
};

struct HCIRUNTIME_API FHCIAgentSourceControlDecision
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

struct HCIRUNTIME_API FHCIAgentMockRbacCheckInput
{
	FString RequestId;
	FString UserName;
	FString ResolvedRole;
	bool bUserMatchedWhitelist = false;
	FName ToolName;
	int32 TargetAssetCount = 0;
	TArray<FString> AllowedCapabilities;
};

struct HCIRUNTIME_API FHCIAgentMockRbacDecision
{
	bool bAllowed = false;
	FString ErrorCode;
	FString Reason;

	FString RequestId;
	FString UserName;
	FString ResolvedRole;
	bool bUserMatchedWhitelist = false;
	bool bGuestFallback = false;
	FString ToolName;
	FString Capability;
	bool bWriteLike = false;
	int32 TargetAssetCount = 0;
};

struct HCIRUNTIME_API FHCIAgentLocalAuditLogRecord
{
	FString TimestampUtc;
	FString UserName;
	FString ResolvedRole;
	FString RequestId;
	FString ToolName;
	FString Capability;
	int32 AssetCount = 0;
	FString Result;
	FString ErrorCode;
	FString Reason;
};

struct HCIRUNTIME_API FHCIAgentLodToolSafetyCheckInput
{
	FString RequestId;
	FName ToolName;
	FString TargetObjectClass;
	bool bNaniteEnabled = false;
};

struct HCIRUNTIME_API FHCIAgentLodToolSafetyDecision
{
	bool bAllowed = false;
	FString ErrorCode;
	FString Reason;

	FString RequestId;
	FString ToolName;
	FString Capability;
	bool bWriteLike = false;
	bool bLodTool = false;
	FString TargetObjectClass;
	bool bStaticMeshTarget = false;
	bool bNaniteEnabled = false;
};

class HCIRUNTIME_API FHCIAgentExecutionGate
{
public:
	static constexpr int32 MaxAssetModifyLimit = 50;

	static FHCIAgentExecutionDecision EvaluateConfirmGate(
		const FHCIAgentExecutionStep& Step,
		const FHCIToolRegistry& Registry);

	static FHCIAgentBlastRadiusDecision EvaluateBlastRadius(
		const FHCIAgentBlastRadiusCheckInput& Input,
		const FHCIToolRegistry& Registry);

	static FHCIAgentTransactionExecutionDecision EvaluateAllOrNothingTransaction(
		const FHCIAgentTransactionExecutionInput& Input,
		const FHCIToolRegistry& Registry);

	static FHCIAgentSourceControlDecision EvaluateSourceControlFailFast(
		const FHCIAgentSourceControlCheckInput& Input,
		const FHCIToolRegistry& Registry);

	static FHCIAgentMockRbacDecision EvaluateMockRbac(
		const FHCIAgentMockRbacCheckInput& Input,
		const FHCIToolRegistry& Registry);

	static FHCIAgentLodToolSafetyDecision EvaluateLodToolSafety(
		const FHCIAgentLodToolSafetyCheckInput& Input,
		const FHCIToolRegistry& Registry);

	static bool SerializeLocalAuditLogRecordToJsonLine(
		const FHCIAgentLocalAuditLogRecord& Record,
		FString& OutJsonLine,
		FString& OutError);

	static bool IsWriteLikeCapability(EHCIToolCapability Capability);
};

