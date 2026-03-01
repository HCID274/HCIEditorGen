#include "Agent/Executor/HCIAgentExecutionGate.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
static const TCHAR* const HCI_Error_ToolNotWhitelisted = TEXT("E4002");
static const TCHAR* const HCI_Error_BlastRadiusExceeded = TEXT("E4004");
static const TCHAR* const HCI_Error_WriteNotConfirmed = TEXT("E4005");
static const TCHAR* const HCI_Error_SourceControlCheckoutFailed = TEXT("E4006");
static const TCHAR* const HCI_Error_TransactionRolledBack = TEXT("E4007");
static const TCHAR* const HCI_Error_RbacDenied = TEXT("E4008");
static const TCHAR* const HCI_Error_LodToolSafetyViolation = TEXT("E4010");

static bool HCI_ContainsCapabilityIgnoreCase(const TArray<FString>& AllowedCapabilities, const FString& Capability)
{
	for (const FString& Candidate : AllowedCapabilities)
	{
		if (Candidate.Equals(Capability, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}

static FString HCI_NormalizeObjectClassForSafetyCheck(const FString& InClassName)
{
	FString Normalized = InClassName.TrimStartAndEnd();
	if (Normalized.StartsWith(TEXT("U")) && Normalized.Len() > 1)
	{
		Normalized = Normalized.RightChop(1);
	}
	return Normalized;
}
}

bool FHCIAgentExecutionGate::IsWriteLikeCapability(const EHCIToolCapability Capability)
{
	return Capability == EHCIToolCapability::Write || Capability == EHCIToolCapability::Destructive;
}

FHCIAgentExecutionDecision FHCIAgentExecutionGate::EvaluateConfirmGate(
	const FHCIAgentExecutionStep& Step,
	const FHCIToolRegistry& Registry)
{
	FHCIAgentExecutionDecision Decision;
	Decision.bAllowed = false;
	Decision.StepId = Step.StepId;
	Decision.ToolName = Step.ToolName.ToString();
	Decision.bRequiresConfirm = Step.bRequiresConfirm;
	Decision.bUserConfirmed = Step.bUserConfirmed;

	const FHCIToolDescriptor* Tool = Registry.FindTool(Step.ToolName);
	if (!Tool)
	{
		Decision.ErrorCode = HCI_Error_ToolNotWhitelisted;
		Decision.Reason = TEXT("tool_not_whitelisted");
		Decision.Capability = TEXT("unknown");
		Decision.bWriteLike = true;
		return Decision;
	}

	Decision.Capability = FHCIToolRegistry::CapabilityToString(Tool->Capability);
	Decision.bWriteLike = IsWriteLikeCapability(Tool->Capability);

	if (!Decision.bWriteLike)
	{
		Decision.bAllowed = true;
		return Decision;
	}

	if (!Step.bRequiresConfirm)
	{
		Decision.ErrorCode = HCI_Error_WriteNotConfirmed;
		Decision.Reason = TEXT("write_step_requires_confirm");
		return Decision;
	}

	if (!Step.bUserConfirmed)
	{
		Decision.ErrorCode = HCI_Error_WriteNotConfirmed;
		Decision.Reason = TEXT("user_not_confirmed");
		return Decision;
	}

	Decision.bAllowed = true;
	return Decision;
}

FHCIAgentBlastRadiusDecision FHCIAgentExecutionGate::EvaluateBlastRadius(
	const FHCIAgentBlastRadiusCheckInput& Input,
	const FHCIToolRegistry& Registry)
{
	FHCIAgentBlastRadiusDecision Decision;
	Decision.bAllowed = false;
	Decision.RequestId = Input.RequestId;
	Decision.ToolName = Input.ToolName.ToString();
	Decision.TargetModifyCount = Input.TargetModifyCount;
	Decision.MaxAssetModifyLimit = MaxAssetModifyLimit;

	const FHCIToolDescriptor* Tool = Registry.FindTool(Input.ToolName);
	if (!Tool)
	{
		Decision.ErrorCode = HCI_Error_ToolNotWhitelisted;
		Decision.Reason = TEXT("tool_not_whitelisted");
		Decision.Capability = TEXT("unknown");
		Decision.bWriteLike = true;
		return Decision;
	}

	Decision.Capability = FHCIToolRegistry::CapabilityToString(Tool->Capability);
	Decision.bWriteLike = IsWriteLikeCapability(Tool->Capability);

	if (!Decision.bWriteLike)
	{
		Decision.bAllowed = true;
		return Decision;
	}

	if (Input.TargetModifyCount > MaxAssetModifyLimit)
	{
		Decision.ErrorCode = HCI_Error_BlastRadiusExceeded;
		Decision.Reason = TEXT("modify_limit_exceeded");
		return Decision;
	}

	Decision.bAllowed = true;
	return Decision;
}

FHCIAgentTransactionExecutionDecision FHCIAgentExecutionGate::EvaluateAllOrNothingTransaction(
	const FHCIAgentTransactionExecutionInput& Input,
	const FHCIToolRegistry& Registry)
{
	FHCIAgentTransactionExecutionDecision Decision;
	Decision.RequestId = Input.RequestId;
	Decision.TransactionMode = TEXT("all_or_nothing");
	Decision.TotalSteps = Input.Steps.Num();

	// Preflight whitelist validation happens before any write is attempted.
	for (int32 Index = 0; Index < Input.Steps.Num(); ++Index)
	{
		const FHCIAgentTransactionStepSimulation& Step = Input.Steps[Index];
		if (Registry.FindTool(Step.ToolName) == nullptr)
		{
			Decision.ErrorCode = HCI_Error_ToolNotWhitelisted;
			Decision.Reason = TEXT("tool_not_whitelisted");
			Decision.FailedStepIndex = Index + 1; // 1-based for logs/UI readability
			Decision.FailedStepId = Step.StepId;
			Decision.FailedToolName = Step.ToolName.ToString();
			return Decision;
		}
	}

	int32 SuccessfulExecutedSteps = 0;
	for (int32 Index = 0; Index < Input.Steps.Num(); ++Index)
	{
		const FHCIAgentTransactionStepSimulation& Step = Input.Steps[Index];
		Decision.ExecutedSteps = Index + 1;

		if (!Step.bShouldSucceed)
		{
			Decision.bCommitted = false;
			Decision.bRolledBack = true;
			Decision.ErrorCode = HCI_Error_TransactionRolledBack;
			Decision.Reason = TEXT("step_failed_all_or_nothing_rollback");
			Decision.CommittedSteps = 0;
			Decision.RolledBackSteps = SuccessfulExecutedSteps;
			Decision.FailedStepIndex = Index + 1; // 1-based for logs/UI readability
			Decision.FailedStepId = Step.StepId;
			Decision.FailedToolName = Step.ToolName.ToString();
			return Decision;
		}

		++SuccessfulExecutedSteps;
	}

	Decision.bCommitted = true;
	Decision.bRolledBack = false;
	Decision.CommittedSteps = SuccessfulExecutedSteps;
	Decision.RolledBackSteps = 0;
	Decision.FailedStepIndex = -1;
	return Decision;
}

FHCIAgentSourceControlDecision FHCIAgentExecutionGate::EvaluateSourceControlFailFast(
	const FHCIAgentSourceControlCheckInput& Input,
	const FHCIToolRegistry& Registry)
{
	FHCIAgentSourceControlDecision Decision;
	Decision.RequestId = Input.RequestId;
	Decision.ToolName = Input.ToolName.ToString();
	Decision.bSourceControlEnabled = Input.bSourceControlEnabled;
	Decision.bCheckoutSucceeded = false;

	const FHCIToolDescriptor* Tool = Registry.FindTool(Input.ToolName);
	if (Tool == nullptr)
	{
		Decision.ErrorCode = HCI_Error_ToolNotWhitelisted;
		Decision.Reason = TEXT("tool_not_whitelisted");
		Decision.Capability = TEXT("unknown");
		Decision.bWriteLike = true;
		return Decision;
	}

	Decision.Capability = FHCIToolRegistry::CapabilityToString(Tool->Capability);
	Decision.bWriteLike = IsWriteLikeCapability(Tool->Capability);

	if (!Decision.bWriteLike)
	{
		Decision.bAllowed = true;
		return Decision;
	}

	if (!Input.bSourceControlEnabled)
	{
		Decision.bAllowed = true;
		Decision.bOfflineLocalMode = true;
		Decision.Reason = TEXT("source_control_disabled_offline_local_mode");
		return Decision;
	}

	Decision.bCheckoutAttempted = true;
	Decision.bCheckoutSucceeded = Input.bCheckoutSucceeded;
	if (!Input.bCheckoutSucceeded)
	{
		Decision.ErrorCode = HCI_Error_SourceControlCheckoutFailed;
		Decision.Reason = TEXT("source_control_checkout_failed_fail_fast");
		return Decision;
	}

	Decision.bAllowed = true;
	Decision.Reason = TEXT("checkout_succeeded");
	return Decision;
}

FHCIAgentMockRbacDecision FHCIAgentExecutionGate::EvaluateMockRbac(
	const FHCIAgentMockRbacCheckInput& Input,
	const FHCIToolRegistry& Registry)
{
	FHCIAgentMockRbacDecision Decision;
	Decision.RequestId = Input.RequestId;
	Decision.UserName = Input.UserName;
	Decision.ResolvedRole = Input.ResolvedRole.IsEmpty() ? TEXT("Guest") : Input.ResolvedRole;
	Decision.bUserMatchedWhitelist = Input.bUserMatchedWhitelist;
	Decision.bGuestFallback = !Input.bUserMatchedWhitelist;
	Decision.ToolName = Input.ToolName.ToString();
	Decision.TargetAssetCount = Input.TargetAssetCount;

	const FHCIToolDescriptor* Tool = Registry.FindTool(Input.ToolName);
	if (Tool == nullptr)
	{
		Decision.ErrorCode = HCI_Error_ToolNotWhitelisted;
		Decision.Reason = TEXT("tool_not_whitelisted");
		Decision.Capability = TEXT("unknown");
		Decision.bWriteLike = true;
		return Decision;
	}

	Decision.Capability = FHCIToolRegistry::CapabilityToString(Tool->Capability);
	Decision.bWriteLike = IsWriteLikeCapability(Tool->Capability);

	if (HCI_ContainsCapabilityIgnoreCase(Input.AllowedCapabilities, Decision.Capability))
	{
		Decision.bAllowed = true;
		if (Decision.bGuestFallback && !Decision.bWriteLike)
		{
			Decision.Reason = TEXT("guest_read_only_allowed");
		}
		else
		{
			Decision.Reason = TEXT("rbac_allowed");
		}
		return Decision;
	}

	Decision.bAllowed = false;
	Decision.ErrorCode = HCI_Error_RbacDenied;
	if (Decision.bGuestFallback && Decision.bWriteLike)
	{
		Decision.Reason = TEXT("guest_read_only_write_blocked");
	}
	else
	{
		Decision.Reason = TEXT("capability_not_allowed_by_role");
	}
	return Decision;
}

FHCIAgentLodToolSafetyDecision FHCIAgentExecutionGate::EvaluateLodToolSafety(
	const FHCIAgentLodToolSafetyCheckInput& Input,
	const FHCIToolRegistry& Registry)
{
	FHCIAgentLodToolSafetyDecision Decision;
	Decision.RequestId = Input.RequestId;
	Decision.ToolName = Input.ToolName.ToString();
	Decision.TargetObjectClass = Input.TargetObjectClass.TrimStartAndEnd();
	Decision.bNaniteEnabled = Input.bNaniteEnabled;
	Decision.bLodTool = Input.ToolName == TEXT("SetMeshLODGroup");

	const FHCIToolDescriptor* Tool = Registry.FindTool(Input.ToolName);
	if (Tool == nullptr)
	{
		Decision.ErrorCode = HCI_Error_ToolNotWhitelisted;
		Decision.Reason = TEXT("tool_not_whitelisted");
		Decision.Capability = TEXT("unknown");
		Decision.bWriteLike = true;
		return Decision;
	}

	Decision.Capability = FHCIToolRegistry::CapabilityToString(Tool->Capability);
	Decision.bWriteLike = IsWriteLikeCapability(Tool->Capability);

	if (!Decision.bLodTool)
	{
		Decision.bAllowed = true;
		Decision.Reason = TEXT("non_lod_tool_bypass");
		return Decision;
	}

	const FString NormalizedTargetClass = HCI_NormalizeObjectClassForSafetyCheck(Decision.TargetObjectClass);
	Decision.bStaticMeshTarget = NormalizedTargetClass.Equals(TEXT("StaticMesh"), ESearchCase::IgnoreCase);

	if (!Decision.bStaticMeshTarget)
	{
		Decision.ErrorCode = HCI_Error_LodToolSafetyViolation;
		Decision.Reason = TEXT("lod_tool_requires_static_mesh");
		return Decision;
	}

	if (Decision.bNaniteEnabled)
	{
		Decision.ErrorCode = HCI_Error_LodToolSafetyViolation;
		Decision.Reason = TEXT("lod_tool_nanite_enabled_blocked");
		return Decision;
	}

	Decision.bAllowed = true;
	Decision.Reason = TEXT("lod_tool_static_mesh_non_nanite_allowed");
	return Decision;
}

bool FHCIAgentExecutionGate::SerializeLocalAuditLogRecordToJsonLine(
	const FHCIAgentLocalAuditLogRecord& Record,
	FString& OutJsonLine,
	FString& OutError)
{
	OutJsonLine.Reset();
	OutError.Reset();

	const TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
	JsonObject->SetStringField(TEXT("timestamp_utc"), Record.TimestampUtc);
	JsonObject->SetStringField(TEXT("user"), Record.UserName);
	JsonObject->SetStringField(TEXT("role"), Record.ResolvedRole);
	JsonObject->SetStringField(TEXT("request_id"), Record.RequestId);
	JsonObject->SetStringField(TEXT("tool_name"), Record.ToolName);
	JsonObject->SetStringField(TEXT("capability"), Record.Capability);
	JsonObject->SetNumberField(TEXT("asset_count"), Record.AssetCount);
	JsonObject->SetStringField(TEXT("result"), Record.Result);
	JsonObject->SetStringField(TEXT("error_code"), Record.ErrorCode);
	JsonObject->SetStringField(TEXT("reason"), Record.Reason);

	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJsonLine);
	if (!FJsonSerializer::Serialize(JsonObject, Writer))
	{
		OutError = TEXT("json_serialize_failed");
		OutJsonLine.Reset();
		return false;
	}

	return true;
}
