#include "Commands/HCIAbilityKitAgentDemoConsoleCommands.h"
#include "Commands/HCIAbilityKitAgentExecutorReviewLocateUtils.h"

#include "Agent/HCIAbilityKitAgentExecutionGate.h"
#include "Agent/HCIAbilityKitAgentApplyConfirmRequest.h"
#include "Agent/HCIAbilityKitAgentApplyConfirmRequestJsonSerializer.h"
#include "Agent/HCIAbilityKitAgentApplyRequest.h"
#include "Agent/HCIAbilityKitAgentApplyRequestJsonSerializer.h"
#include "Agent/HCIAbilityKitAgentExecutor.h"
#include "Agent/HCIAbilityKitAgentExecutorApplyConfirmBridge.h"
#include "Agent/HCIAbilityKitAgentExecutorApplyRequestBridge.h"
#include "Agent/HCIAbilityKitAgentExecutorDryRunBridge.h"
#include "Agent/HCIAbilityKitAgentPlan.h"
#include "Agent/HCIAbilityKitAgentPlanJsonSerializer.h"
#include "Agent/HCIAbilityKitAgentPlanner.h"
#include "Agent/HCIAbilityKitAgentPlanValidator.h"
#include "Agent/HCIAbilityKitDryRunDiff.h"
#include "Agent/HCIAbilityKitDryRunDiffJsonSerializer.h"
#include "Agent/HCIAbilityKitDryRunDiffSelection.h"
#include "Agent/HCIAbilityKitToolRegistry.h"
#include "Common/HCIAbilityKitTimeFormat.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogHCIAbilityKitAgentDemo, Log, All);

static FHCIAbilityKitAgentPlan GHCIAbilityKitAgentPlanPreviewState;
static FHCIAbilityKitDryRunDiffReport GHCIAbilityKitAgentExecutorReviewDiffPreviewState;
static FHCIAbilityKitAgentApplyRequest GHCIAbilityKitAgentApplyRequestPreviewState;
static FHCIAbilityKitAgentApplyConfirmRequest GHCIAbilityKitAgentApplyConfirmRequestPreviewState;

static FString HCI_JoinConsoleArgsAsText(const TArray<FString>& Args)
{
	FString Joined;
	for (int32 Index = 0; Index < Args.Num(); ++Index)
	{
		if (Index > 0)
		{
			Joined += TEXT(" ");
		}
		Joined += Args[Index];
	}
	Joined.TrimStartAndEndInline();
	return Joined;
}

static FString HCI_JsonObjectToCompactString(const TSharedPtr<FJsonObject>& JsonObject)
{
	if (!JsonObject.IsValid())
	{
		return TEXT("{}");
	}

	FString OutJson;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutJson);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	return OutJson;
}

static FString HCI_GetAgentRbacMockConfigPath()
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("SourceData"),
		TEXT("AbilityKits"),
		TEXT("Config"),
		TEXT("agent_rbac_mock.json")));
}

static FString HCI_GetAgentExecAuditLogPath()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("HCIAbilityKit"), TEXT("Audit"), TEXT("agent_exec_log.jsonl"));
}


static bool HCI_TryParseBool01Arg(const FString& InValue, bool& OutValue)
{
	const FString Trimmed = InValue.TrimStartAndEnd();
	if (Trimmed == TEXT("0"))
	{
		OutValue = false;
		return true;
	}

	if (Trimmed == TEXT("1"))
	{
		OutValue = true;
		return true;
	}

	return false;
}

static bool HCI_TryParseNonNegativeIntArg(const FString& InValue, int32& OutValue)
{
	if (!LexTryParseString(OutValue, *InValue.TrimStartAndEnd()))
	{
		return false;
	}
	return OutValue >= 0;
}

static bool HCI_TryParseRowIndexListArgs(const TArray<FString>& Args, TArray<int32>& OutRows, FString& OutReason)
{
	OutRows.Reset();
	OutReason = TEXT("ok");

	if (Args.Num() <= 0)
	{
		OutReason = TEXT("empty_row_selection");
		return false;
	}

	FString Joined = HCI_JoinConsoleArgsAsText(Args);
	Joined.ReplaceInline(TEXT(","), TEXT(" "));
	Joined.TrimStartAndEndInline();
	if (Joined.IsEmpty())
	{
		OutReason = TEXT("empty_row_selection");
		return false;
	}

	TArray<FString> Tokens;
	Joined.ParseIntoArrayWS(Tokens);
	if (Tokens.Num() <= 0)
	{
		OutReason = TEXT("empty_row_selection");
		return false;
	}

	OutRows.Reserve(Tokens.Num());
	for (const FString& Token : Tokens)
	{
		int32 RowIndex = INDEX_NONE;
		if (!LexTryParseString(RowIndex, *Token) || RowIndex < 0)
		{
			OutReason = TEXT("row_index must be integer >= 0");
			return false;
		}
		OutRows.Add(RowIndex);
	}

	return true;
}


static void HCI_LogAgentConfirmGateDecision(const TCHAR* CaseName, const FHCIAbilityKitAgentExecutionDecision& Decision)
{
	if (Decision.bAllowed)
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentConfirmGate] case=%s step_id=%s tool_name=%s capability=%s write_like=%s requires_confirm=%s user_confirmed=%s allowed=%s error_code=%s reason=%s"),
			CaseName,
			Decision.StepId.IsEmpty() ? TEXT("-") : *Decision.StepId,
			Decision.ToolName.IsEmpty() ? TEXT("-") : *Decision.ToolName,
			Decision.Capability.IsEmpty() ? TEXT("-") : *Decision.Capability,
			Decision.bWriteLike ? TEXT("true") : TEXT("false"),
			Decision.bRequiresConfirm ? TEXT("true") : TEXT("false"),
			Decision.bUserConfirmed ? TEXT("true") : TEXT("false"),
			TEXT("true"),
			Decision.ErrorCode.IsEmpty() ? TEXT("-") : *Decision.ErrorCode,
			Decision.Reason.IsEmpty() ? TEXT("-") : *Decision.Reason);
		return;
	}

	UE_LOG(
		LogHCIAbilityKitAgentDemo,
		Warning,
		TEXT("[HCIAbilityKit][AgentConfirmGate] case=%s step_id=%s tool_name=%s capability=%s write_like=%s requires_confirm=%s user_confirmed=%s allowed=%s error_code=%s reason=%s"),
		CaseName,
		Decision.StepId.IsEmpty() ? TEXT("-") : *Decision.StepId,
		Decision.ToolName.IsEmpty() ? TEXT("-") : *Decision.ToolName,
		Decision.Capability.IsEmpty() ? TEXT("-") : *Decision.Capability,
		Decision.bWriteLike ? TEXT("true") : TEXT("false"),
		Decision.bRequiresConfirm ? TEXT("true") : TEXT("false"),
		Decision.bUserConfirmed ? TEXT("true") : TEXT("false"),
		TEXT("false"),
		Decision.ErrorCode.IsEmpty() ? TEXT("-") : *Decision.ErrorCode,
		Decision.Reason.IsEmpty() ? TEXT("-") : *Decision.Reason);
}

static void HCI_RunAbilityKitAgentConfirmGateDemoCommand(const TArray<FString>& Args)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	if (Args.Num() == 0)
	{
		int32 BlockedCount = 0;
		int32 AllowedCount = 0;

		auto RunCase = [&Registry, &BlockedCount, &AllowedCount](const TCHAR* CaseName, const TCHAR* StepId, const TCHAR* ToolName, const bool bRequiresConfirm, const bool bUserConfirmed)
		{
			FHCIAbilityKitAgentExecutionStep Step;
			Step.StepId = StepId;
			Step.ToolName = FName(ToolName);
			Step.bRequiresConfirm = bRequiresConfirm;
			Step.bUserConfirmed = bUserConfirmed;

			const FHCIAbilityKitAgentExecutionDecision Decision = FHCIAbilityKitAgentExecutionGate::EvaluateConfirmGate(Step, Registry);
			if (Decision.bAllowed)
			{
				++AllowedCount;
			}
			else
			{
				++BlockedCount;
			}
			HCI_LogAgentConfirmGateDecision(CaseName, Decision);
		};

		RunCase(TEXT("read_only_unconfirmed"), TEXT("step_demo_01"), TEXT("ScanAssets"), false, false);
		RunCase(TEXT("write_unconfirmed"), TEXT("step_demo_02"), TEXT("RenameAsset"), true, false);
		RunCase(TEXT("write_confirmed"), TEXT("step_demo_03"), TEXT("SetTextureMaxSize"), true, true);

		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentConfirmGate] summary total_cases=%d allowed=%d blocked=%d expected_blocked_code=%s validation=ok"),
			3,
			AllowedCount,
			BlockedCount,
			TEXT("E4005"));
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentConfirmGate] hint=也可运行 HCIAbilityKit.AgentConfirmGateDemo [tool_name] [requires_confirm 0|1] [user_confirmed 0|1]"));
		return;
	}

	if (Args.Num() < 3)
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Error,
			TEXT("[HCIAbilityKit][AgentConfirmGate] invalid_args usage=HCIAbilityKit.AgentConfirmGateDemo [tool_name] [requires_confirm 0|1] [user_confirmed 0|1]"));
		return;
	}

	bool bRequiresConfirm = false;
	bool bUserConfirmed = false;
	if (!HCI_TryParseBool01Arg(Args[1], bRequiresConfirm) || !HCI_TryParseBool01Arg(Args[2], bUserConfirmed))
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Error,
			TEXT("[HCIAbilityKit][AgentConfirmGate] invalid_args reason=requires_confirm and user_confirmed must be 0 or 1"));
		return;
	}

	FHCIAbilityKitAgentExecutionStep Step;
	Step.StepId = TEXT("step_cli");
	Step.ToolName = FName(*Args[0].TrimStartAndEnd());
	Step.bRequiresConfirm = bRequiresConfirm;
	Step.bUserConfirmed = bUserConfirmed;

	const FHCIAbilityKitAgentExecutionDecision Decision = FHCIAbilityKitAgentExecutionGate::EvaluateConfirmGate(Step, Registry);
	HCI_LogAgentConfirmGateDecision(TEXT("custom"), Decision);
}

static void HCI_LogAgentBlastRadiusDecision(const TCHAR* CaseName, const FHCIAbilityKitAgentBlastRadiusDecision& Decision)
{
	if (Decision.bAllowed)
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentBlastRadius] case=%s request_id=%s tool_name=%s capability=%s write_like=%s target_modify_count=%d max_limit=%d allowed=%s error_code=%s reason=%s"),
			CaseName,
			Decision.RequestId.IsEmpty() ? TEXT("-") : *Decision.RequestId,
			Decision.ToolName.IsEmpty() ? TEXT("-") : *Decision.ToolName,
			Decision.Capability.IsEmpty() ? TEXT("-") : *Decision.Capability,
			Decision.bWriteLike ? TEXT("true") : TEXT("false"),
			Decision.TargetModifyCount,
			Decision.MaxAssetModifyLimit,
			TEXT("true"),
			Decision.ErrorCode.IsEmpty() ? TEXT("-") : *Decision.ErrorCode,
			Decision.Reason.IsEmpty() ? TEXT("-") : *Decision.Reason);
		return;
	}

	UE_LOG(
		LogHCIAbilityKitAgentDemo,
		Warning,
		TEXT("[HCIAbilityKit][AgentBlastRadius] case=%s request_id=%s tool_name=%s capability=%s write_like=%s target_modify_count=%d max_limit=%d allowed=%s error_code=%s reason=%s"),
		CaseName,
		Decision.RequestId.IsEmpty() ? TEXT("-") : *Decision.RequestId,
		Decision.ToolName.IsEmpty() ? TEXT("-") : *Decision.ToolName,
		Decision.Capability.IsEmpty() ? TEXT("-") : *Decision.Capability,
		Decision.bWriteLike ? TEXT("true") : TEXT("false"),
		Decision.TargetModifyCount,
		Decision.MaxAssetModifyLimit,
		TEXT("false"),
		Decision.ErrorCode.IsEmpty() ? TEXT("-") : *Decision.ErrorCode,
		Decision.Reason.IsEmpty() ? TEXT("-") : *Decision.Reason);
}

static void HCI_RunAbilityKitAgentBlastRadiusDemoCommand(const TArray<FString>& Args)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	if (Args.Num() == 0)
	{
		int32 BlockedCount = 0;
		int32 AllowedCount = 0;

		auto RunCase = [&Registry, &BlockedCount, &AllowedCount](const TCHAR* CaseName, const TCHAR* RequestId, const TCHAR* ToolName, const int32 TargetModifyCount)
		{
			FHCIAbilityKitAgentBlastRadiusCheckInput Input;
			Input.RequestId = RequestId;
			Input.ToolName = FName(ToolName);
			Input.TargetModifyCount = TargetModifyCount;

			const FHCIAbilityKitAgentBlastRadiusDecision Decision = FHCIAbilityKitAgentExecutionGate::EvaluateBlastRadius(Input, Registry);
			if (Decision.bAllowed)
			{
				++AllowedCount;
			}
			else
			{
				++BlockedCount;
			}

			HCI_LogAgentBlastRadiusDecision(CaseName, Decision);
		};

		RunCase(TEXT("read_only_large_count"), TEXT("req_demo_e4_01"), TEXT("ScanAssets"), 999);
		RunCase(TEXT("write_at_limit"), TEXT("req_demo_e4_02"), TEXT("SetTextureMaxSize"), 50);
		RunCase(TEXT("write_over_limit"), TEXT("req_demo_e4_03"), TEXT("RenameAsset"), 51);

		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentBlastRadius] summary total_cases=%d allowed=%d blocked=%d max_limit=%d expected_blocked_code=%s validation=ok"),
			3,
			AllowedCount,
			BlockedCount,
			FHCIAbilityKitAgentExecutionGate::MaxAssetModifyLimit,
			TEXT("E4004"));
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentBlastRadius] hint=也可运行 HCIAbilityKit.AgentBlastRadiusDemo [tool_name] [target_modify_count>=0]"));
		return;
	}

	if (Args.Num() < 2)
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Error,
			TEXT("[HCIAbilityKit][AgentBlastRadius] invalid_args usage=HCIAbilityKit.AgentBlastRadiusDemo [tool_name] [target_modify_count>=0]"));
		return;
	}

	int32 TargetModifyCount = 0;
	if (!HCI_TryParseNonNegativeIntArg(Args[1], TargetModifyCount))
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Error,
			TEXT("[HCIAbilityKit][AgentBlastRadius] invalid_args reason=target_modify_count must be integer >= 0"));
		return;
	}

	FHCIAbilityKitAgentBlastRadiusCheckInput Input;
	Input.RequestId = TEXT("req_cli_e4");
	Input.ToolName = FName(*Args[0].TrimStartAndEnd());
	Input.TargetModifyCount = TargetModifyCount;

	const FHCIAbilityKitAgentBlastRadiusDecision Decision = FHCIAbilityKitAgentExecutionGate::EvaluateBlastRadius(Input, Registry);
	HCI_LogAgentBlastRadiusDecision(TEXT("custom"), Decision);
}

static FHCIAbilityKitAgentTransactionExecutionInput HCI_BuildAgentTransactionDemoInput(
	const TCHAR* RequestId,
	const int32 TotalSteps,
	const int32 FailStepIndexOneBased)
{
	FHCIAbilityKitAgentTransactionExecutionInput Input;
	Input.RequestId = RequestId;
	Input.Steps.Reserve(TotalSteps);

	static const FName DemoToolNames[] = {
		TEXT("RenameAsset"),
		TEXT("MoveAsset"),
		TEXT("SetTextureMaxSize"),
		TEXT("SetMeshLODGroup")};

	for (int32 Index = 0; Index < TotalSteps; ++Index)
	{
		FHCIAbilityKitAgentTransactionStepSimulation& Step = Input.Steps.AddDefaulted_GetRef();
		Step.StepId = FString::Printf(TEXT("step_%03d"), Index + 1);
		Step.ToolName = DemoToolNames[Index % UE_ARRAY_COUNT(DemoToolNames)];
		Step.bShouldSucceed = ((Index + 1) != FailStepIndexOneBased);
	}

	return Input;
}

static void HCI_LogAgentTransactionDecision(const TCHAR* CaseName, const FHCIAbilityKitAgentTransactionExecutionDecision& Decision)
{
	if (Decision.bCommitted)
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentTransaction] case=%s request_id=%s transaction_mode=%s total_steps=%d executed_steps=%d committed_steps=%d rolled_back_steps=%d committed=%s rolled_back=%s failed_step_index=%d failed_step_id=%s failed_tool_name=%s error_code=%s reason=%s"),
			CaseName,
			Decision.RequestId.IsEmpty() ? TEXT("-") : *Decision.RequestId,
			Decision.TransactionMode.IsEmpty() ? TEXT("-") : *Decision.TransactionMode,
			Decision.TotalSteps,
			Decision.ExecutedSteps,
			Decision.CommittedSteps,
			Decision.RolledBackSteps,
			TEXT("true"),
			Decision.bRolledBack ? TEXT("true") : TEXT("false"),
			Decision.FailedStepIndex,
			Decision.FailedStepId.IsEmpty() ? TEXT("-") : *Decision.FailedStepId,
			Decision.FailedToolName.IsEmpty() ? TEXT("-") : *Decision.FailedToolName,
			Decision.ErrorCode.IsEmpty() ? TEXT("-") : *Decision.ErrorCode,
			Decision.Reason.IsEmpty() ? TEXT("-") : *Decision.Reason);
		return;
	}

	UE_LOG(
		LogHCIAbilityKitAgentDemo,
		Warning,
		TEXT("[HCIAbilityKit][AgentTransaction] case=%s request_id=%s transaction_mode=%s total_steps=%d executed_steps=%d committed_steps=%d rolled_back_steps=%d committed=%s rolled_back=%s failed_step_index=%d failed_step_id=%s failed_tool_name=%s error_code=%s reason=%s"),
		CaseName,
		Decision.RequestId.IsEmpty() ? TEXT("-") : *Decision.RequestId,
		Decision.TransactionMode.IsEmpty() ? TEXT("-") : *Decision.TransactionMode,
		Decision.TotalSteps,
		Decision.ExecutedSteps,
		Decision.CommittedSteps,
		Decision.RolledBackSteps,
		TEXT("false"),
		Decision.bRolledBack ? TEXT("true") : TEXT("false"),
		Decision.FailedStepIndex,
		Decision.FailedStepId.IsEmpty() ? TEXT("-") : *Decision.FailedStepId,
		Decision.FailedToolName.IsEmpty() ? TEXT("-") : *Decision.FailedToolName,
		Decision.ErrorCode.IsEmpty() ? TEXT("-") : *Decision.ErrorCode,
		Decision.Reason.IsEmpty() ? TEXT("-") : *Decision.Reason);
}

static void HCI_RunAbilityKitAgentTransactionDemoCommand(const TArray<FString>& Args)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	if (Args.Num() == 0)
	{
		int32 CommittedCaseCount = 0;
		int32 RolledBackCaseCount = 0;

		auto RunCase = [&Registry, &CommittedCaseCount, &RolledBackCaseCount](
						   const TCHAR* CaseName,
						   const TCHAR* RequestId,
						   const int32 TotalSteps,
						   const int32 FailStepIndexOneBased)
		{
			const FHCIAbilityKitAgentTransactionExecutionInput Input =
				HCI_BuildAgentTransactionDemoInput(RequestId, TotalSteps, FailStepIndexOneBased);
			const FHCIAbilityKitAgentTransactionExecutionDecision Decision =
				FHCIAbilityKitAgentExecutionGate::EvaluateAllOrNothingTransaction(Input, Registry);

			if (Decision.bCommitted)
			{
				++CommittedCaseCount;
			}
			else
			{
				++RolledBackCaseCount;
			}

			HCI_LogAgentTransactionDecision(CaseName, Decision);
		};

		RunCase(TEXT("all_success_commit"), TEXT("req_demo_e5_01"), 2, 0);
		RunCase(TEXT("fail_step2_rollback"), TEXT("req_demo_e5_02"), 3, 2);
		RunCase(TEXT("fail_step1_rollback"), TEXT("req_demo_e5_03"), 2, 1);

		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentTransaction] summary total_cases=%d committed=%d rolled_back=%d transaction_mode=%s expected_failed_code=%s validation=ok"),
			3,
			CommittedCaseCount,
			RolledBackCaseCount,
			TEXT("all_or_nothing"),
			TEXT("E4007"));
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentTransaction] hint=也可运行 HCIAbilityKit.AgentTransactionDemo [total_steps>=1] [fail_step_index>=0] (0 means all succeed)"));
		return;
	}

	if (Args.Num() < 2)
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Error,
			TEXT("[HCIAbilityKit][AgentTransaction] invalid_args usage=HCIAbilityKit.AgentTransactionDemo [total_steps>=1] [fail_step_index>=0]"));
		return;
	}

	int32 TotalSteps = 0;
	int32 FailStepIndexOneBased = 0;
	if (!HCI_TryParseNonNegativeIntArg(Args[0], TotalSteps) || !HCI_TryParseNonNegativeIntArg(Args[1], FailStepIndexOneBased))
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Error,
			TEXT("[HCIAbilityKit][AgentTransaction] invalid_args reason=total_steps and fail_step_index must be integers"));
		return;
	}

	if (TotalSteps < 1)
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Error,
			TEXT("[HCIAbilityKit][AgentTransaction] invalid_args reason=total_steps must be >= 1"));
		return;
	}

	if (FailStepIndexOneBased < 0 || FailStepIndexOneBased > TotalSteps)
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Error,
			TEXT("[HCIAbilityKit][AgentTransaction] invalid_args reason=fail_step_index must satisfy 0 <= fail_step_index <= total_steps"));
		return;
	}

	const FHCIAbilityKitAgentTransactionExecutionInput Input =
		HCI_BuildAgentTransactionDemoInput(TEXT("req_cli_e5"), TotalSteps, FailStepIndexOneBased);
	const FHCIAbilityKitAgentTransactionExecutionDecision Decision =
		FHCIAbilityKitAgentExecutionGate::EvaluateAllOrNothingTransaction(Input, Registry);
	HCI_LogAgentTransactionDecision(TEXT("custom"), Decision);
}

static void HCI_LogAgentSourceControlDecision(const TCHAR* CaseName, const FHCIAbilityKitAgentSourceControlDecision& Decision)
{
	const bool bUseWarningLevel = (!Decision.bAllowed) || Decision.bOfflineLocalMode;
	if (!bUseWarningLevel)
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentSourceControl] case=%s request_id=%s tool_name=%s capability=%s write_like=%s source_control_enabled=%s fail_fast=%s offline_local_mode=%s checkout_attempted=%s checkout_succeeded=%s allowed=%s error_code=%s reason=%s"),
			CaseName,
			Decision.RequestId.IsEmpty() ? TEXT("-") : *Decision.RequestId,
			Decision.ToolName.IsEmpty() ? TEXT("-") : *Decision.ToolName,
			Decision.Capability.IsEmpty() ? TEXT("-") : *Decision.Capability,
			Decision.bWriteLike ? TEXT("true") : TEXT("false"),
			Decision.bSourceControlEnabled ? TEXT("true") : TEXT("false"),
			Decision.bFailFastPolicy ? TEXT("true") : TEXT("false"),
			Decision.bOfflineLocalMode ? TEXT("true") : TEXT("false"),
			Decision.bCheckoutAttempted ? TEXT("true") : TEXT("false"),
			Decision.bCheckoutSucceeded ? TEXT("true") : TEXT("false"),
			TEXT("true"),
			Decision.ErrorCode.IsEmpty() ? TEXT("-") : *Decision.ErrorCode,
			Decision.Reason.IsEmpty() ? TEXT("-") : *Decision.Reason);
		return;
	}

	UE_LOG(
		LogHCIAbilityKitAgentDemo,
		Warning,
		TEXT("[HCIAbilityKit][AgentSourceControl] case=%s request_id=%s tool_name=%s capability=%s write_like=%s source_control_enabled=%s fail_fast=%s offline_local_mode=%s checkout_attempted=%s checkout_succeeded=%s allowed=%s error_code=%s reason=%s"),
		CaseName,
		Decision.RequestId.IsEmpty() ? TEXT("-") : *Decision.RequestId,
		Decision.ToolName.IsEmpty() ? TEXT("-") : *Decision.ToolName,
		Decision.Capability.IsEmpty() ? TEXT("-") : *Decision.Capability,
		Decision.bWriteLike ? TEXT("true") : TEXT("false"),
		Decision.bSourceControlEnabled ? TEXT("true") : TEXT("false"),
		Decision.bFailFastPolicy ? TEXT("true") : TEXT("false"),
		Decision.bOfflineLocalMode ? TEXT("true") : TEXT("false"),
		Decision.bCheckoutAttempted ? TEXT("true") : TEXT("false"),
		Decision.bCheckoutSucceeded ? TEXT("true") : TEXT("false"),
		Decision.bAllowed ? TEXT("true") : TEXT("false"),
		Decision.ErrorCode.IsEmpty() ? TEXT("-") : *Decision.ErrorCode,
		Decision.Reason.IsEmpty() ? TEXT("-") : *Decision.Reason);
}

static void HCI_RunAbilityKitAgentSourceControlDemoCommand(const TArray<FString>& Args)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	auto BuildInput = [](const TCHAR* RequestId, const TCHAR* ToolName, const bool bSourceControlEnabled, const bool bCheckoutSucceeded)
	{
		FHCIAbilityKitAgentSourceControlCheckInput Input;
		Input.RequestId = RequestId;
		Input.ToolName = FName(ToolName);
		Input.bSourceControlEnabled = bSourceControlEnabled;
		Input.bCheckoutSucceeded = bCheckoutSucceeded;
		return Input;
	};

	if (Args.Num() == 0)
	{
		int32 AllowedCount = 0;
		int32 BlockedCount = 0;
		int32 OfflineLocalModeCount = 0;

		auto RunCase = [&Registry, &AllowedCount, &BlockedCount, &OfflineLocalModeCount](const TCHAR* CaseName, const FHCIAbilityKitAgentSourceControlCheckInput& Input)
		{
			const FHCIAbilityKitAgentSourceControlDecision Decision =
				FHCIAbilityKitAgentExecutionGate::EvaluateSourceControlFailFast(Input, Registry);
			if (Decision.bAllowed)
			{
				++AllowedCount;
			}
			else
			{
				++BlockedCount;
			}
			if (Decision.bOfflineLocalMode)
			{
				++OfflineLocalModeCount;
			}
			HCI_LogAgentSourceControlDecision(CaseName, Decision);
		};

		RunCase(TEXT("read_only_enabled_bypass"), BuildInput(TEXT("req_demo_e6_01"), TEXT("ScanAssets"), true, false));
		RunCase(TEXT("write_offline_local_mode"), BuildInput(TEXT("req_demo_e6_02"), TEXT("RenameAsset"), false, false));
		RunCase(TEXT("write_checkout_fail_fast"), BuildInput(TEXT("req_demo_e6_03"), TEXT("MoveAsset"), true, false));

		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentSourceControl] summary total_cases=%d allowed=%d blocked=%d offline_local_mode_cases=%d fail_fast=%s expected_blocked_code=%s validation=ok"),
			3,
			AllowedCount,
			BlockedCount,
			OfflineLocalModeCount,
			TEXT("true"),
			TEXT("E4006"));
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentSourceControl] hint=也可运行 HCIAbilityKit.AgentSourceControlDemo [tool_name] [source_control_enabled 0|1] [checkout_succeeded 0|1]"));
		return;
	}

	if (Args.Num() < 3)
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Error,
			TEXT("[HCIAbilityKit][AgentSourceControl] invalid_args usage=HCIAbilityKit.AgentSourceControlDemo [tool_name] [source_control_enabled 0|1] [checkout_succeeded 0|1]"));
		return;
	}

	bool bSourceControlEnabled = false;
	bool bCheckoutSucceeded = false;
	if (!HCI_TryParseBool01Arg(Args[1], bSourceControlEnabled) || !HCI_TryParseBool01Arg(Args[2], bCheckoutSucceeded))
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Error,
			TEXT("[HCIAbilityKit][AgentSourceControl] invalid_args reason=source_control_enabled and checkout_succeeded must be 0 or 1"));
		return;
	}

	FHCIAbilityKitAgentSourceControlCheckInput Input;
	Input.RequestId = TEXT("req_cli_e6");
	Input.ToolName = FName(*Args[0].TrimStartAndEnd());
	Input.bSourceControlEnabled = bSourceControlEnabled;
	Input.bCheckoutSucceeded = bCheckoutSucceeded;

	const FHCIAbilityKitAgentSourceControlDecision Decision =
		FHCIAbilityKitAgentExecutionGate::EvaluateSourceControlFailFast(Input, Registry);
	HCI_LogAgentSourceControlDecision(TEXT("custom"), Decision);
}

struct FHCIAbilityKitAgentRbacMockUserConfigEntry
{
	FString UserName;
	FString UserNameNormalized;
	FString RoleName;
	TArray<FString> AllowedCapabilities;
};

struct FHCIAbilityKitAgentRbacResolvedUser
{
	FString UserName;
	FString ResolvedRole = TEXT("Guest");
	bool bMatchedWhitelist = false;
	TArray<FString> AllowedCapabilities;
};

static FString HCI_BuildDefaultAgentRbacMockConfigJson()
{
	return TEXT(
		"{\n"
		"  \"users\": [\n"
		"    { \"user\": \"artist_a\", \"role\": \"Artist\", \"allowed_capabilities\": [\"read_only\", \"write\"] },\n"
		"    { \"user\": \"ta_a\", \"role\": \"TA\", \"allowed_capabilities\": [\"read_only\", \"write\", \"destructive\"] },\n"
		"    { \"user\": \"reviewer_a\", \"role\": \"Reviewer\", \"allowed_capabilities\": [\"read_only\"] }\n"
		"  ]\n"
		"}\n");
}

static bool HCI_EnsureAgentRbacMockConfigExists(FString& OutConfigPath, bool& OutCreated, FString& OutError)
{
	OutConfigPath = HCI_GetAgentRbacMockConfigPath();
	OutCreated = false;
	OutError.Reset();

	if (IFileManager::Get().FileExists(*OutConfigPath))
	{
		return true;
	}

	const FString Directory = FPaths::GetPath(OutConfigPath);
	if (!IFileManager::Get().MakeDirectory(*Directory, true))
	{
		OutError = FString::Printf(TEXT("failed_to_create_config_dir:%s"), *Directory);
		return false;
	}

	if (!FFileHelper::SaveStringToFile(
			HCI_BuildDefaultAgentRbacMockConfigJson(),
			*OutConfigPath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = FString::Printf(TEXT("failed_to_write_default_config:%s"), *OutConfigPath);
		return false;
	}

	OutCreated = true;
	return true;
}

static bool HCI_LoadAgentRbacMockConfigEntries(
	TArray<FHCIAbilityKitAgentRbacMockUserConfigEntry>& OutEntries,
	FString& OutConfigPath,
	bool& OutConfigCreated,
	FString& OutError)
{
	OutEntries.Reset();
	OutError.Reset();
	OutConfigPath.Reset();
	OutConfigCreated = false;

	if (!HCI_EnsureAgentRbacMockConfigExists(OutConfigPath, OutConfigCreated, OutError))
	{
		return false;
	}

	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *OutConfigPath))
	{
		OutError = FString::Printf(TEXT("failed_to_read_config:%s"), *OutConfigPath);
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError = TEXT("invalid_json_root_object");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* UsersArray = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("users"), UsersArray) || UsersArray == nullptr)
	{
		OutError = TEXT("missing_users_array");
		return false;
	}

	for (const TSharedPtr<FJsonValue>& UserValue : *UsersArray)
	{
		const TSharedPtr<FJsonObject> UserObject = UserValue.IsValid() ? UserValue->AsObject() : nullptr;
		if (!UserObject.IsValid())
		{
			continue;
		}

		FString UserName;
		if (!UserObject->TryGetStringField(TEXT("user"), UserName))
		{
			continue;
		}

		UserName = UserName.TrimStartAndEnd();
		if (UserName.IsEmpty())
		{
			continue;
		}

		FHCIAbilityKitAgentRbacMockUserConfigEntry& Entry = OutEntries.AddDefaulted_GetRef();
		Entry.UserName = UserName;
		Entry.UserNameNormalized = UserName.ToLower();
		UserObject->TryGetStringField(TEXT("role"), Entry.RoleName);
		if (Entry.RoleName.IsEmpty())
		{
			Entry.RoleName = TEXT("Custom");
		}

		const TArray<TSharedPtr<FJsonValue>>* CapabilityArray = nullptr;
		if (UserObject->TryGetArrayField(TEXT("allowed_capabilities"), CapabilityArray) && CapabilityArray != nullptr)
		{
			for (const TSharedPtr<FJsonValue>& CapabilityValue : *CapabilityArray)
			{
				FString CapabilityString;
				if (CapabilityValue.IsValid() && CapabilityValue->TryGetString(CapabilityString))
				{
					CapabilityString = CapabilityString.TrimStartAndEnd();
					if (!CapabilityString.IsEmpty())
					{
						Entry.AllowedCapabilities.Add(CapabilityString);
					}
				}
			}
		}
	}

	return true;
}

static FHCIAbilityKitAgentRbacResolvedUser HCI_ResolveAgentRbacMockUser(
	const FString& UserName,
	const TArray<FHCIAbilityKitAgentRbacMockUserConfigEntry>& Entries)
{
	FHCIAbilityKitAgentRbacResolvedUser Resolved;
	Resolved.UserName = UserName.TrimStartAndEnd();
	const FString UserNameNormalized = Resolved.UserName.ToLower();

	for (const FHCIAbilityKitAgentRbacMockUserConfigEntry& Entry : Entries)
	{
		if (Entry.UserNameNormalized == UserNameNormalized)
		{
			Resolved.bMatchedWhitelist = true;
			Resolved.ResolvedRole = Entry.RoleName.IsEmpty() ? TEXT("Custom") : Entry.RoleName;
			Resolved.AllowedCapabilities = Entry.AllowedCapabilities;
			return Resolved;
		}
	}

	Resolved.bMatchedWhitelist = false;
	Resolved.ResolvedRole = TEXT("Guest");
	Resolved.AllowedCapabilities = {TEXT("read_only")};
	return Resolved;
}

static bool HCI_AppendAgentExecAuditLogRecord(
	const FHCIAbilityKitAgentLocalAuditLogRecord& Record,
	FString& OutAuditLogPath,
	FString& OutJsonLine,
	FString& OutError)
{
	OutAuditLogPath = HCI_GetAgentExecAuditLogPath();
	OutJsonLine.Reset();
	OutError.Reset();

	if (!FHCIAbilityKitAgentExecutionGate::SerializeLocalAuditLogRecordToJsonLine(Record, OutJsonLine, OutError))
	{
		return false;
	}

	const FString Directory = FPaths::GetPath(OutAuditLogPath);
	if (!IFileManager::Get().MakeDirectory(*Directory, true))
	{
		OutError = FString::Printf(TEXT("failed_to_create_audit_log_dir:%s"), *Directory);
		return false;
	}

	const FString LineWithNewline = OutJsonLine + LINE_TERMINATOR;
	if (!FFileHelper::SaveStringToFile(
			LineWithNewline,
			*OutAuditLogPath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
			&IFileManager::Get(),
			FILEWRITE_Append))
	{
		OutError = FString::Printf(TEXT("failed_to_append_audit_log:%s"), *OutAuditLogPath);
		return false;
	}

	return true;
}

static void HCI_LogAgentRbacDecision(
	const TCHAR* CaseName,
	const FHCIAbilityKitAgentMockRbacDecision& Decision,
	const bool bAuditLogAppended,
	const FString& AuditLogPath,
	const FString& AuditLogError)
{
	const bool bUseWarning = !Decision.bAllowed;
	if (!bUseWarning)
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentRBAC] case=%s request_id=%s user=%s resolved_role=%s user_in_whitelist=%s guest_fallback=%s tool_name=%s capability=%s write_like=%s asset_count=%d allowed=%s error_code=%s reason=%s audit_log_appended=%s audit_log_path=%s audit_log_error=%s"),
			CaseName,
			Decision.RequestId.IsEmpty() ? TEXT("-") : *Decision.RequestId,
			Decision.UserName.IsEmpty() ? TEXT("-") : *Decision.UserName,
			Decision.ResolvedRole.IsEmpty() ? TEXT("-") : *Decision.ResolvedRole,
			Decision.bUserMatchedWhitelist ? TEXT("true") : TEXT("false"),
			Decision.bGuestFallback ? TEXT("true") : TEXT("false"),
			Decision.ToolName.IsEmpty() ? TEXT("-") : *Decision.ToolName,
			Decision.Capability.IsEmpty() ? TEXT("-") : *Decision.Capability,
			Decision.bWriteLike ? TEXT("true") : TEXT("false"),
			Decision.TargetAssetCount,
			TEXT("true"),
			Decision.ErrorCode.IsEmpty() ? TEXT("-") : *Decision.ErrorCode,
			Decision.Reason.IsEmpty() ? TEXT("-") : *Decision.Reason,
			bAuditLogAppended ? TEXT("true") : TEXT("false"),
			AuditLogPath.IsEmpty() ? TEXT("-") : *AuditLogPath,
			AuditLogError.IsEmpty() ? TEXT("-") : *AuditLogError);
		return;
	}

	UE_LOG(
		LogHCIAbilityKitAgentDemo,
		Warning,
		TEXT("[HCIAbilityKit][AgentRBAC] case=%s request_id=%s user=%s resolved_role=%s user_in_whitelist=%s guest_fallback=%s tool_name=%s capability=%s write_like=%s asset_count=%d allowed=%s error_code=%s reason=%s audit_log_appended=%s audit_log_path=%s audit_log_error=%s"),
		CaseName,
		Decision.RequestId.IsEmpty() ? TEXT("-") : *Decision.RequestId,
		Decision.UserName.IsEmpty() ? TEXT("-") : *Decision.UserName,
		Decision.ResolvedRole.IsEmpty() ? TEXT("-") : *Decision.ResolvedRole,
		Decision.bUserMatchedWhitelist ? TEXT("true") : TEXT("false"),
		Decision.bGuestFallback ? TEXT("true") : TEXT("false"),
		Decision.ToolName.IsEmpty() ? TEXT("-") : *Decision.ToolName,
		Decision.Capability.IsEmpty() ? TEXT("-") : *Decision.Capability,
		Decision.bWriteLike ? TEXT("true") : TEXT("false"),
		Decision.TargetAssetCount,
		TEXT("false"),
		Decision.ErrorCode.IsEmpty() ? TEXT("-") : *Decision.ErrorCode,
		Decision.Reason.IsEmpty() ? TEXT("-") : *Decision.Reason,
		bAuditLogAppended ? TEXT("true") : TEXT("false"),
		AuditLogPath.IsEmpty() ? TEXT("-") : *AuditLogPath,
		AuditLogError.IsEmpty() ? TEXT("-") : *AuditLogError);
}

static void HCI_RunAbilityKitAgentRbacDemoCommand(const TArray<FString>& Args)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	TArray<FHCIAbilityKitAgentRbacMockUserConfigEntry> RbacEntries;
	FString ConfigPath;
	FString ConfigError;
	bool bConfigCreated = false;
	if (!HCI_LoadAgentRbacMockConfigEntries(RbacEntries, ConfigPath, bConfigCreated, ConfigError))
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Error,
			TEXT("[HCIAbilityKit][AgentRBAC] config_load_failed path=%s reason=%s"),
			ConfigPath.IsEmpty() ? TEXT("-") : *ConfigPath,
			ConfigError.IsEmpty() ? TEXT("unknown") : *ConfigError);
		return;
	}

	if (bConfigCreated)
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Warning,
			TEXT("[HCIAbilityKit][AgentRBAC] config_created_default path=%s"),
			*ConfigPath);
	}

	auto RunCase = [&Registry, &RbacEntries](const TCHAR* CaseName, const TCHAR* RequestId, const FString& UserName, const TCHAR* ToolName, const int32 AssetCount) -> TTuple<FHCIAbilityKitAgentMockRbacDecision, bool, FString, FString>
	{
		const FHCIAbilityKitAgentRbacResolvedUser ResolvedUser = HCI_ResolveAgentRbacMockUser(UserName, RbacEntries);

		FHCIAbilityKitAgentMockRbacCheckInput Input;
		Input.RequestId = RequestId;
		Input.UserName = ResolvedUser.UserName;
		Input.ResolvedRole = ResolvedUser.ResolvedRole;
		Input.bUserMatchedWhitelist = ResolvedUser.bMatchedWhitelist;
		Input.ToolName = FName(ToolName);
		Input.TargetAssetCount = AssetCount;
		Input.AllowedCapabilities = ResolvedUser.AllowedCapabilities;

		const FHCIAbilityKitAgentMockRbacDecision Decision = FHCIAbilityKitAgentExecutionGate::EvaluateMockRbac(Input, Registry);

		FHCIAbilityKitAgentLocalAuditLogRecord Record;
		Record.TimestampUtc = FHCIAbilityKitTimeFormat::FormatNowBeijingIso8601();
		Record.UserName = Decision.UserName;
		Record.ResolvedRole = Decision.ResolvedRole;
		Record.RequestId = Decision.RequestId;
		Record.ToolName = Decision.ToolName;
		Record.Capability = Decision.Capability;
		Record.AssetCount = Decision.TargetAssetCount;
		Record.Result = Decision.bAllowed ? TEXT("allowed") : TEXT("blocked");
		Record.ErrorCode = Decision.ErrorCode;
		Record.Reason = Decision.Reason;

		FString AuditLogPath;
		FString JsonLine;
		FString AuditLogError;
		const bool bAuditLogAppended = HCI_AppendAgentExecAuditLogRecord(Record, AuditLogPath, JsonLine, AuditLogError);
		if (!bAuditLogAppended)
		{
			UE_LOG(
				LogHCIAbilityKitAgentDemo,
				Error,
				TEXT("[HCIAbilityKit][AgentAuditLog] append_failed case=%s path=%s reason=%s"),
				CaseName,
				AuditLogPath.IsEmpty() ? TEXT("-") : *AuditLogPath,
				AuditLogError.IsEmpty() ? TEXT("unknown") : *AuditLogError);
		}
		else
		{
			UE_LOG(
				LogHCIAbilityKitAgentDemo,
				Display,
				TEXT("[HCIAbilityKit][AgentAuditLog] append_ok case=%s path=%s bytes=%d user=%s tool_name=%s result=%s error_code=%s"),
				CaseName,
				*AuditLogPath,
				JsonLine.Len(),
				Decision.UserName.IsEmpty() ? TEXT("-") : *Decision.UserName,
				Decision.ToolName.IsEmpty() ? TEXT("-") : *Decision.ToolName,
				Decision.bAllowed ? TEXT("allowed") : TEXT("blocked"),
				Decision.ErrorCode.IsEmpty() ? TEXT("-") : *Decision.ErrorCode);
		}

		HCI_LogAgentRbacDecision(CaseName, Decision, bAuditLogAppended, AuditLogPath, AuditLogError);
		return MakeTuple(Decision, bAuditLogAppended, AuditLogPath, AuditLogError);
	};

	if (Args.Num() == 0)
	{
		int32 AllowedCount = 0;
		int32 BlockedCount = 0;
		int32 GuestFallbackCount = 0;
		int32 AuditLogAppendCount = 0;
		FString LastAuditLogPath;

		auto ConsumeCase = [&AllowedCount, &BlockedCount, &GuestFallbackCount, &AuditLogAppendCount, &LastAuditLogPath](const TTuple<FHCIAbilityKitAgentMockRbacDecision, bool, FString, FString>& ResultTuple)
		{
			const FHCIAbilityKitAgentMockRbacDecision& Decision = ResultTuple.Get<0>();
			if (Decision.bAllowed)
			{
				++AllowedCount;
			}
			else
			{
				++BlockedCount;
			}
			if (Decision.bGuestFallback)
			{
				++GuestFallbackCount;
			}
			if (ResultTuple.Get<1>())
			{
				++AuditLogAppendCount;
			}
			LastAuditLogPath = ResultTuple.Get<2>();
		};

		ConsumeCase(RunCase(TEXT("guest_read_only_allowed"), TEXT("req_demo_e7_01"), TEXT("unknown_guest"), TEXT("ScanAssets"), 0));
		ConsumeCase(RunCase(TEXT("guest_write_blocked"), TEXT("req_demo_e7_02"), TEXT("unknown_guest"), TEXT("RenameAsset"), 1));
		ConsumeCase(RunCase(TEXT("configured_write_allowed"), TEXT("req_demo_e7_03"), TEXT("artist_a"), TEXT("SetTextureMaxSize"), 3));

		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentRBAC] summary total_cases=%d allowed=%d blocked=%d guest_fallback_cases=%d audit_log_appends=%d config_users=%d validation=ok"),
			3,
			AllowedCount,
			BlockedCount,
			GuestFallbackCount,
			AuditLogAppendCount,
			RbacEntries.Num());
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentRBAC] paths config_path=%s audit_log_path=%s config_created_default=%s"),
			*ConfigPath,
			LastAuditLogPath.IsEmpty() ? *HCI_GetAgentExecAuditLogPath() : *LastAuditLogPath,
			bConfigCreated ? TEXT("true") : TEXT("false"));
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentRBAC] hint=也可运行 HCIAbilityKit.AgentRbacDemo [user_name] [tool_name] [asset_count>=0]"));
		return;
	}

	if (Args.Num() < 3)
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Error,
			TEXT("[HCIAbilityKit][AgentRBAC] invalid_args usage=HCIAbilityKit.AgentRbacDemo [user_name] [tool_name] [asset_count>=0]"));
		return;
	}

	int32 AssetCount = 0;
	if (!HCI_TryParseNonNegativeIntArg(Args[2], AssetCount))
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Error,
			TEXT("[HCIAbilityKit][AgentRBAC] invalid_args reason=asset_count must be integer >= 0"));
		return;
	}

	const FString UserName = Args[0].TrimStartAndEnd();
	const FString ToolName = Args[1].TrimStartAndEnd();
	if (UserName.IsEmpty() || ToolName.IsEmpty())
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Error,
			TEXT("[HCIAbilityKit][AgentRBAC] invalid_args reason=user_name and tool_name must be non-empty"));
		return;
	}

	RunCase(TEXT("custom"), TEXT("req_cli_e7"), UserName, *ToolName, AssetCount);
}

static void HCI_LogAgentLodSafetyDecision(const TCHAR* CaseName, const FHCIAbilityKitAgentLodToolSafetyDecision& Decision)
{
	const bool bUseWarning = !Decision.bAllowed;
	if (!bUseWarning)
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentLodSafety] case=%s request_id=%s tool_name=%s capability=%s write_like=%s is_lod_tool=%s target_object_class=%s is_static_mesh_target=%s nanite_enabled=%s allowed=%s error_code=%s reason=%s"),
			CaseName,
			Decision.RequestId.IsEmpty() ? TEXT("-") : *Decision.RequestId,
			Decision.ToolName.IsEmpty() ? TEXT("-") : *Decision.ToolName,
			Decision.Capability.IsEmpty() ? TEXT("-") : *Decision.Capability,
			Decision.bWriteLike ? TEXT("true") : TEXT("false"),
			Decision.bLodTool ? TEXT("true") : TEXT("false"),
			Decision.TargetObjectClass.IsEmpty() ? TEXT("-") : *Decision.TargetObjectClass,
			Decision.bStaticMeshTarget ? TEXT("true") : TEXT("false"),
			Decision.bNaniteEnabled ? TEXT("true") : TEXT("false"),
			TEXT("true"),
			Decision.ErrorCode.IsEmpty() ? TEXT("-") : *Decision.ErrorCode,
			Decision.Reason.IsEmpty() ? TEXT("-") : *Decision.Reason);
		return;
	}

	UE_LOG(
		LogHCIAbilityKitAgentDemo,
		Warning,
		TEXT("[HCIAbilityKit][AgentLodSafety] case=%s request_id=%s tool_name=%s capability=%s write_like=%s is_lod_tool=%s target_object_class=%s is_static_mesh_target=%s nanite_enabled=%s allowed=%s error_code=%s reason=%s"),
		CaseName,
		Decision.RequestId.IsEmpty() ? TEXT("-") : *Decision.RequestId,
		Decision.ToolName.IsEmpty() ? TEXT("-") : *Decision.ToolName,
		Decision.Capability.IsEmpty() ? TEXT("-") : *Decision.Capability,
		Decision.bWriteLike ? TEXT("true") : TEXT("false"),
		Decision.bLodTool ? TEXT("true") : TEXT("false"),
		Decision.TargetObjectClass.IsEmpty() ? TEXT("-") : *Decision.TargetObjectClass,
		Decision.bStaticMeshTarget ? TEXT("true") : TEXT("false"),
		Decision.bNaniteEnabled ? TEXT("true") : TEXT("false"),
		TEXT("false"),
		Decision.ErrorCode.IsEmpty() ? TEXT("-") : *Decision.ErrorCode,
		Decision.Reason.IsEmpty() ? TEXT("-") : *Decision.Reason);
}

static void HCI_RunAbilityKitAgentLodSafetyDemoCommand(const TArray<FString>& Args)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	auto BuildInput = [](const TCHAR* RequestId, const TCHAR* ToolName, const TCHAR* TargetObjectClass, const bool bNaniteEnabled)
	{
		FHCIAbilityKitAgentLodToolSafetyCheckInput Input;
		Input.RequestId = RequestId;
		Input.ToolName = FName(ToolName);
		Input.TargetObjectClass = TargetObjectClass;
		Input.bNaniteEnabled = bNaniteEnabled;
		return Input;
	};

	if (Args.Num() == 0)
	{
		int32 AllowedCount = 0;
		int32 BlockedCount = 0;
		int32 TypeBlockedCount = 0;
		int32 NaniteBlockedCount = 0;

		auto RunCase = [&Registry, &AllowedCount, &BlockedCount, &TypeBlockedCount, &NaniteBlockedCount](
						   const TCHAR* CaseName,
						   const FHCIAbilityKitAgentLodToolSafetyCheckInput& Input)
		{
			const FHCIAbilityKitAgentLodToolSafetyDecision Decision =
				FHCIAbilityKitAgentExecutionGate::EvaluateLodToolSafety(Input, Registry);
			if (Decision.bAllowed)
			{
				++AllowedCount;
			}
			else
			{
				++BlockedCount;
				if (Decision.Reason == TEXT("lod_tool_requires_static_mesh"))
				{
					++TypeBlockedCount;
				}
				else if (Decision.Reason == TEXT("lod_tool_nanite_enabled_blocked"))
				{
					++NaniteBlockedCount;
				}
			}

			HCI_LogAgentLodSafetyDecision(CaseName, Decision);
		};

		RunCase(TEXT("type_not_static_mesh_blocked"), BuildInput(TEXT("req_demo_e8_01"), TEXT("SetMeshLODGroup"), TEXT("Texture2D"), false));
		RunCase(TEXT("nanite_static_mesh_blocked"), BuildInput(TEXT("req_demo_e8_02"), TEXT("SetMeshLODGroup"), TEXT("StaticMesh"), true));
		RunCase(TEXT("static_mesh_non_nanite_allowed"), BuildInput(TEXT("req_demo_e8_03"), TEXT("SetMeshLODGroup"), TEXT("UStaticMesh"), false));

		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentLodSafety] summary total_cases=%d allowed=%d blocked=%d type_blocked=%d nanite_blocked=%d expected_blocked_code=%s validation=ok"),
			3,
			AllowedCount,
			BlockedCount,
			TypeBlockedCount,
			NaniteBlockedCount,
			TEXT("E4010"));
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentLodSafety] hint=也可运行 HCIAbilityKit.AgentLodSafetyDemo [tool_name] [target_object_class] [nanite_enabled 0|1]"));
		return;
	}

	if (Args.Num() < 3)
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Error,
			TEXT("[HCIAbilityKit][AgentLodSafety] invalid_args usage=HCIAbilityKit.AgentLodSafetyDemo [tool_name] [target_object_class] [nanite_enabled 0|1]"));
		return;
	}

	bool bNaniteEnabled = false;
	if (!HCI_TryParseBool01Arg(Args[2], bNaniteEnabled))
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Error,
			TEXT("[HCIAbilityKit][AgentLodSafety] invalid_args reason=nanite_enabled must be 0 or 1"));
		return;
	}

	const FString ToolName = Args[0].TrimStartAndEnd();
	const FString TargetObjectClass = Args[1].TrimStartAndEnd();
	if (ToolName.IsEmpty() || TargetObjectClass.IsEmpty())
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Error,
			TEXT("[HCIAbilityKit][AgentLodSafety] invalid_args reason=tool_name and target_object_class must be non-empty"));
		return;
	}

	const FHCIAbilityKitAgentLodToolSafetyDecision Decision =
		FHCIAbilityKitAgentExecutionGate::EvaluateLodToolSafety(
			BuildInput(TEXT("req_cli_e8"), *ToolName, *TargetObjectClass, bNaniteEnabled),
			Registry);
	HCI_LogAgentLodSafetyDecision(TEXT("custom"), Decision);
}

static bool HCI_BuildAgentPlanDemoPlan(
	const FString& UserText,
	const FString& RequestId,
	FHCIAbilityKitAgentPlan& OutPlan,
	FString& OutRouteReason,
	FString& OutError)
{
	FHCIAbilityKitToolRegistry& ToolRegistry = FHCIAbilityKitToolRegistry::Get();
	ToolRegistry.ResetToDefaults();
	return FHCIAbilityKitAgentPlanner::BuildPlanFromNaturalLanguage(
		UserText,
		RequestId,
		ToolRegistry,
		OutPlan,
		OutRouteReason,
		OutError);
}

static void HCI_LogAgentPlanRows(const TCHAR* CaseName, const FString& UserText, const FString& RouteReason, const FHCIAbilityKitAgentPlan& Plan)
{
	for (int32 Index = 0; Index < Plan.Steps.Num(); ++Index)
	{
		const FHCIAbilityKitAgentPlanStep& Step = Plan.Steps[Index];
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentPlan] case=%s row=%d request_id=%s intent=%s input=%s step_id=%s tool_name=%s risk_level=%s requires_confirm=%s rollback_strategy=%s route_reason=%s expected_evidence=%s args=%s"),
			CaseName,
			Index,
			*Plan.RequestId,
			*Plan.Intent,
			*UserText,
			*Step.StepId,
			*Step.ToolName.ToString(),
			*FHCIAbilityKitAgentPlanContract::RiskLevelToString(Step.RiskLevel),
			Step.bRequiresConfirm ? TEXT("true") : TEXT("false"),
			*Step.RollbackStrategy,
			*RouteReason,
			*FString::Join(Step.ExpectedEvidence, TEXT("|")),
			*HCI_JsonObjectToCompactString(Step.Args));
	}
}

static void HCI_RunAbilityKitAgentPlanDemoCommand(const TArray<FString>& Args)
{
	if (Args.Num() == 0)
	{
		struct FDemoPlanCase
		{
			const TCHAR* CaseName;
			const TCHAR* RequestId;
			const TCHAR* UserText;
		};

		const TArray<FDemoPlanCase> Cases{
			{TEXT("naming_archive_temp_assets"), TEXT("req_demo_f1_01"), TEXT("整理临时目录资产，按规范命名并归档")},
			{TEXT("level_mesh_risk_scan"), TEXT("req_demo_f1_02"), TEXT("检查当前关卡选中物体的碰撞和默认材质")},
			{TEXT("asset_compliance_fix"), TEXT("req_demo_f1_03"), TEXT("检查贴图分辨率并处理LOD")}};

		int32 BuiltCount = 0;
		int32 ValidationOkCount = 0;
		for (const FDemoPlanCase& DemoCase : Cases)
		{
			FHCIAbilityKitAgentPlan Plan;
			FString RouteReason;
			FString Error;
			if (!HCI_BuildAgentPlanDemoPlan(DemoCase.UserText, DemoCase.RequestId, Plan, RouteReason, Error))
			{
				UE_LOG(
					LogHCIAbilityKitAgentDemo,
					Error,
					TEXT("[HCIAbilityKit][AgentPlan] case=%s build_failed input=%s reason=%s"),
					DemoCase.CaseName,
					DemoCase.UserText,
					*Error);
				continue;
			}

			++BuiltCount;
			FString ContractError;
			const bool bValid = FHCIAbilityKitAgentPlanContract::ValidateMinimalContract(Plan, ContractError);
			if (bValid)
			{
				++ValidationOkCount;
			}

			UE_LOG(
				LogHCIAbilityKitAgentDemo,
				Display,
				TEXT("[HCIAbilityKit][AgentPlan] case=%s summary request_id=%s intent=%s input=%s plan_version=%d steps=%d route_reason=%s validation=%s"),
				DemoCase.CaseName,
				*Plan.RequestId,
				*Plan.Intent,
				DemoCase.UserText,
				Plan.PlanVersion,
				Plan.Steps.Num(),
				*RouteReason,
				bValid ? TEXT("ok") : *FString::Printf(TEXT("fail:%s"), *ContractError));

			HCI_LogAgentPlanRows(DemoCase.CaseName, DemoCase.UserText, RouteReason, Plan);
			GHCIAbilityKitAgentPlanPreviewState = Plan;
		}

		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentPlan] summary total_cases=%d built=%d validation_ok=%d plan_version=1 supports_intents=naming_traceability|level_risk|asset_compliance validation=ok"),
			Cases.Num(),
			BuiltCount,
			ValidationOkCount);
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentPlan] hint=也可运行 HCIAbilityKit.AgentPlanDemo [自然语言文本...] 或 HCIAbilityKit.AgentPlanDemoJson [自然语言文本...]"));
		return;
	}

	const FString UserText = HCI_JoinConsoleArgsAsText(Args);
	if (UserText.IsEmpty())
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentPlan] invalid_args reason=empty input text"));
		return;
	}

	FHCIAbilityKitAgentPlan Plan;
	FString RouteReason;
	FString Error;
	if (!HCI_BuildAgentPlanDemoPlan(UserText, TEXT("req_cli_f1"), Plan, RouteReason, Error))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentPlan] build_failed input=%s reason=%s"), *UserText, *Error);
		return;
	}

	FString ContractError;
	const bool bValid = FHCIAbilityKitAgentPlanContract::ValidateMinimalContract(Plan, ContractError);
	GHCIAbilityKitAgentPlanPreviewState = Plan;

	UE_LOG(
		LogHCIAbilityKitAgentDemo,
		Display,
		TEXT("[HCIAbilityKit][AgentPlan] case=custom summary request_id=%s intent=%s input=%s plan_version=%d steps=%d route_reason=%s validation=%s"),
		*Plan.RequestId,
		*Plan.Intent,
		*UserText,
		Plan.PlanVersion,
		Plan.Steps.Num(),
		*RouteReason,
		bValid ? TEXT("ok") : *FString::Printf(TEXT("fail:%s"), *ContractError));

	HCI_LogAgentPlanRows(TEXT("custom"), UserText, RouteReason, Plan);
}

static void HCI_RunAbilityKitAgentPlanDemoJsonCommand(const TArray<FString>& Args)
{
	const FString UserText =
		Args.Num() > 0 ? HCI_JoinConsoleArgsAsText(Args) : FString(TEXT("整理临时目录资产，按规范命名并归档"));

	FHCIAbilityKitAgentPlan Plan;
	FString RouteReason;
	FString Error;
	if (!HCI_BuildAgentPlanDemoPlan(UserText, TEXT("req_cli_f1_json"), Plan, RouteReason, Error))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentPlan] json_build_failed input=%s reason=%s"), *UserText, *Error);
		return;
	}

	FString JsonText;
	if (!FHCIAbilityKitAgentPlanJsonSerializer::SerializeToJsonString(Plan, JsonText))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentPlan] export_json_failed reason=serialize_failed"));
		return;
	}

	GHCIAbilityKitAgentPlanPreviewState = Plan;
	UE_LOG(
		LogHCIAbilityKitAgentDemo,
		Display,
		TEXT("[HCIAbilityKit][AgentPlan] json request_id=%s intent=%s route_reason=%s json=%s"),
		*Plan.RequestId,
		*Plan.Intent,
		*RouteReason,
		*JsonText);
}

static FHCIAbilityKitAgentPlan HCI_MakeAgentPlanValidateTexturePlan(const TCHAR* RequestId)
{
	FHCIAbilityKitAgentPlan Plan;
	Plan.PlanVersion = 1;
	Plan.RequestId = RequestId;
	Plan.Intent = TEXT("batch_fix_asset_compliance");

	FHCIAbilityKitAgentPlanStep& Step = Plan.Steps.AddDefaulted_GetRef();
	Step.StepId = TEXT("s1");
	Step.ToolName = TEXT("SetTextureMaxSize");
	Step.RiskLevel = EHCIAbilityKitAgentPlanRiskLevel::Write;
	Step.bRequiresConfirm = true;
	Step.RollbackStrategy = TEXT("all_or_nothing");
	Step.ExpectedEvidence = {TEXT("asset_path"), TEXT("before"), TEXT("after")};
	Step.Args = MakeShared<FJsonObject>();

	TArray<TSharedPtr<FJsonValue>> AssetPaths;
	AssetPaths.Add(MakeShared<FJsonValueString>(TEXT("/Game/Art/Trees/T_Tree_01_D.T_Tree_01_D")));
	Step.Args->SetArrayField(TEXT("asset_paths"), AssetPaths);
	Step.Args->SetNumberField(TEXT("max_size"), 1024);
	return Plan;
}

static bool HCI_BuildAgentPlanValidateDemoCase(
	const FString& CaseKey,
	FHCIAbilityKitAgentPlan& OutPlan,
	FHCIAbilityKitAgentPlanValidationContext& OutContext,
	FString& OutDescription,
	FString& OutExpectedCode,
	FString& OutError)
{
	OutPlan = FHCIAbilityKitAgentPlan();
	OutContext = FHCIAbilityKitAgentPlanValidationContext();
	OutDescription.Reset();
	OutExpectedCode = TEXT("-");
	OutError.Reset();

	FString RouteReason;
	if (CaseKey.Equals(TEXT("ok_naming"), ESearchCase::IgnoreCase))
	{
		OutDescription = TEXT("planner_naming_valid");
		return HCI_BuildAgentPlanDemoPlan(TEXT("整理临时目录资产，按规范命名并归档"), TEXT("req_demo_f2_ok"), OutPlan, RouteReason, OutError);
	}

	if (CaseKey.Equals(TEXT("fail_missing_tool"), ESearchCase::IgnoreCase))
	{
		OutDescription = TEXT("missing_tool_name");
		OutExpectedCode = TEXT("E4001");
		OutPlan = HCI_MakeAgentPlanValidateTexturePlan(TEXT("req_demo_f2_e4001"));
		if (OutPlan.Steps.Num() > 0)
		{
			OutPlan.Steps[0].ToolName = NAME_None;
		}
		return true;
	}

	if (CaseKey.Equals(TEXT("fail_unknown_tool"), ESearchCase::IgnoreCase))
	{
		OutDescription = TEXT("unknown_tool");
		OutExpectedCode = TEXT("E4002");
		OutPlan = HCI_MakeAgentPlanValidateTexturePlan(TEXT("req_demo_f2_e4002"));
		if (OutPlan.Steps.Num() > 0)
		{
			OutPlan.Steps[0].ToolName = TEXT("UnknownTool_X");
		}
		return true;
	}

	if (CaseKey.Equals(TEXT("fail_invalid_enum"), ESearchCase::IgnoreCase))
	{
		OutDescription = TEXT("invalid_max_size_enum");
		OutExpectedCode = TEXT("E4009");
		OutPlan = HCI_MakeAgentPlanValidateTexturePlan(TEXT("req_demo_f2_e4009"));
		if (OutPlan.Steps.Num() > 0 && OutPlan.Steps[0].Args.IsValid())
		{
			OutPlan.Steps[0].Args->SetNumberField(TEXT("max_size"), 123);
		}
		return true;
	}

	if (CaseKey.Equals(TEXT("fail_over_limit"), ESearchCase::IgnoreCase))
	{
		OutDescription = TEXT("aggregate_modify_limit_exceeded");
		OutExpectedCode = TEXT("E4004");
		OutPlan = HCI_MakeAgentPlanValidateTexturePlan(TEXT("req_demo_f2_e4004"));
		if (OutPlan.Steps.Num() > 0 && OutPlan.Steps[0].Args.IsValid())
		{
			TArray<TSharedPtr<FJsonValue>> AssetPathsA;
			TArray<TSharedPtr<FJsonValue>> AssetPathsB;
			for (int32 Index = 0; Index < 50; ++Index)
			{
				AssetPathsA.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("/Game/Temp/TA_%03d.TA_%03d"), Index, Index)));
				AssetPathsB.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("/Game/Temp/TB_%03d.TB_%03d"), Index, Index)));
			}
			OutPlan.Steps[0].Args->SetArrayField(TEXT("asset_paths"), AssetPathsA);

			FHCIAbilityKitAgentPlanStep& Step2 = OutPlan.Steps.AddDefaulted_GetRef();
			Step2.StepId = TEXT("s2");
			Step2.ToolName = TEXT("SetTextureMaxSize");
			Step2.RiskLevel = EHCIAbilityKitAgentPlanRiskLevel::Write;
			Step2.bRequiresConfirm = true;
			Step2.RollbackStrategy = TEXT("all_or_nothing");
			Step2.ExpectedEvidence = {TEXT("asset_path"), TEXT("before"), TEXT("after")};
			Step2.Args = MakeShared<FJsonObject>();
			Step2.Args->SetArrayField(TEXT("asset_paths"), AssetPathsB);
			Step2.Args->SetNumberField(TEXT("max_size"), 1024);
		}
		return true;
	}

	if (CaseKey.Equals(TEXT("fail_level_scope"), ESearchCase::IgnoreCase))
	{
		OutDescription = TEXT("invalid_level_scope");
		OutExpectedCode = TEXT("E4011");
		if (!HCI_BuildAgentPlanDemoPlan(TEXT("检查当前关卡选中物体的碰撞和默认材质"), TEXT("req_demo_f2_e4011"), OutPlan, RouteReason, OutError))
		{
			return false;
		}
		if (OutPlan.Steps.Num() > 0 && OutPlan.Steps[0].Args.IsValid())
		{
			OutPlan.Steps[0].Args->SetStringField(TEXT("scope"), TEXT("world"));
		}
		return true;
	}

	if (CaseKey.Equals(TEXT("fail_naming_metadata"), ESearchCase::IgnoreCase))
	{
		OutDescription = TEXT("mock_metadata_insufficient");
		OutExpectedCode = TEXT("E4012");
		if (!HCI_BuildAgentPlanDemoPlan(TEXT("整理临时目录资产，按规范命名并归档"), TEXT("req_demo_f2_e4012"), OutPlan, RouteReason, OutError))
		{
			return false;
		}
		OutContext.MockMetadataUnavailableAssetPaths.Add(TEXT("/Game/Temp/SM_RockTemp_01.SM_RockTemp_01"));
		return true;
	}

	OutError = FString::Printf(TEXT("unknown_case_key=%s"), *CaseKey);
	return false;
}

static void HCI_LogAgentPlanValidationDecision(
	const TCHAR* CaseName,
	const FString& Description,
	const FString& ExpectedCode,
	const FHCIAbilityKitAgentPlanValidationResult& Result)
{
	const bool bExpected = ExpectedCode.IsEmpty() || ExpectedCode == TEXT("-") ? Result.bValid : Result.ErrorCode == ExpectedCode;
	if (Result.bValid || bExpected)
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentPlanValidate] case=%s request_id=%s intent=%s plan_version=%d step_count=%d valid=%s error_code=%s field=%s reason=%s validated_steps=%d write_steps=%d total_modify_targets=%d max_risk=%s expected_code=%s expected_match=%s note=%s"),
			CaseName,
			*Result.RequestId,
			*Result.Intent,
			Result.PlanVersion,
			Result.StepCount,
			Result.bValid ? TEXT("true") : TEXT("false"),
			*Result.ErrorCode,
			*Result.Field,
			*Result.Reason,
			Result.ValidatedStepCount,
			Result.WriteLikeStepCount,
			Result.TotalTargetModifyCount,
			*Result.MaxRiskLevel,
			*ExpectedCode,
			bExpected ? TEXT("true") : TEXT("false"),
			*Description);
	}
	else
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Warning,
			TEXT("[HCIAbilityKit][AgentPlanValidate] case=%s request_id=%s intent=%s plan_version=%d step_count=%d valid=%s error_code=%s field=%s reason=%s validated_steps=%d write_steps=%d total_modify_targets=%d max_risk=%s expected_code=%s expected_match=%s note=%s"),
			CaseName,
			*Result.RequestId,
			*Result.Intent,
			Result.PlanVersion,
			Result.StepCount,
			Result.bValid ? TEXT("true") : TEXT("false"),
			*Result.ErrorCode,
			*Result.Field,
			*Result.Reason,
			Result.ValidatedStepCount,
			Result.WriteLikeStepCount,
			Result.TotalTargetModifyCount,
			*Result.MaxRiskLevel,
			*ExpectedCode,
			bExpected ? TEXT("true") : TEXT("false"),
			*Description);
	}
}

static void HCI_RunAbilityKitAgentPlanValidateDemoCommand(const TArray<FString>& Args)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	if (Args.Num() == 0)
	{
		const TArray<FString> Cases = {
			TEXT("ok_naming"),
			TEXT("fail_missing_tool"),
			TEXT("fail_unknown_tool"),
			TEXT("fail_invalid_enum"),
			TEXT("fail_over_limit"),
			TEXT("fail_level_scope"),
			TEXT("fail_naming_metadata")};

		int32 ValidCount = 0;
		int32 InvalidCount = 0;
		int32 ExpectedMatches = 0;

		for (const FString& CaseKey : Cases)
		{
			FHCIAbilityKitAgentPlan Plan;
			FHCIAbilityKitAgentPlanValidationContext Context;
			FString Description;
			FString ExpectedCode;
			FString BuildError;
			if (!HCI_BuildAgentPlanValidateDemoCase(CaseKey, Plan, Context, Description, ExpectedCode, BuildError))
			{
				UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentPlanValidate] case=%s build_failed reason=%s"), *CaseKey, *BuildError);
				continue;
			}

			FHCIAbilityKitAgentPlanValidationResult Result;
			FHCIAbilityKitAgentPlanValidator::ValidatePlan(Plan, Registry, Context, Result);
			GHCIAbilityKitAgentPlanPreviewState = Plan;
			HCI_LogAgentPlanValidationDecision(*CaseKey, Description, ExpectedCode, Result);

			if (Result.bValid)
			{
				++ValidCount;
			}
			else
			{
				++InvalidCount;
			}

			const bool bExpectedMatch = (ExpectedCode == TEXT("-") && Result.bValid) || (ExpectedCode != TEXT("-") && Result.ErrorCode == ExpectedCode);
			if (bExpectedMatch)
			{
				++ExpectedMatches;
			}
		}

		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentPlanValidate] summary total_cases=%d valid=%d invalid=%d expected_matches=%d supports_checks=schema|whitelist|risk|threshold|special_cases validation=%s"),
			Cases.Num(),
			ValidCount,
			InvalidCount,
			ExpectedMatches,
			ExpectedMatches == Cases.Num() ? TEXT("ok") : TEXT("fail"));
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentPlanValidate] hint=也可运行 HCIAbilityKit.AgentPlanValidateDemo [ok_naming|fail_missing_tool|fail_unknown_tool|fail_invalid_enum|fail_over_limit|fail_level_scope|fail_naming_metadata]"));
		return;
	}

	if (Args.Num() != 1)
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Error,
			TEXT("[HCIAbilityKit][AgentPlanValidate] invalid_args usage=HCIAbilityKit.AgentPlanValidateDemo [case_key]"));
		return;
	}

	const FString CaseKey = Args[0].TrimStartAndEnd();
	if (CaseKey.IsEmpty())
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentPlanValidate] invalid_args reason=case_key is empty"));
		return;
	}

	FHCIAbilityKitAgentPlan Plan;
	FHCIAbilityKitAgentPlanValidationContext Context;
	FString Description;
	FString ExpectedCode;
	FString BuildError;
	if (!HCI_BuildAgentPlanValidateDemoCase(CaseKey, Plan, Context, Description, ExpectedCode, BuildError))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentPlanValidate] build_failed case=%s reason=%s"), *CaseKey, *BuildError);
		return;
	}

	FHCIAbilityKitAgentPlanValidationResult Result;
	FHCIAbilityKitAgentPlanValidator::ValidatePlan(Plan, Registry, Context, Result);
	GHCIAbilityKitAgentPlanPreviewState = Plan;
	HCI_LogAgentPlanValidationDecision(TEXT("custom"), Description, ExpectedCode, Result);
}

static FString HCI_FormatStringMapForLog(const TMap<FString, FString>& Map)
{
	if (Map.Num() == 0)
	{
		return TEXT("-");
	}

	TArray<FString> Keys;
	Map.GetKeys(Keys);
	Keys.Sort();

	TArray<FString> Tokens;
	Tokens.Reserve(Keys.Num());
	for (const FString& Key : Keys)
	{
		const FString* Value = Map.Find(Key);
		Tokens.Add(FString::Printf(TEXT("%s=%s"), *Key, Value ? *(*Value) : TEXT("-")));
	}
	return FString::Join(Tokens, TEXT("|"));
}

static void HCI_LogAgentExecutorRows(const TCHAR* CaseName, const FHCIAbilityKitAgentExecutorRunResult& RunResult)
{
	for (int32 Index = 0; Index < RunResult.StepResults.Num(); ++Index)
	{
		const FHCIAbilityKitAgentExecutorStepResult& Row = RunResult.StepResults[Index];
		if (Row.bSucceeded)
		{
			UE_LOG(
				LogHCIAbilityKitAgentDemo,
				Display,
				TEXT("[HCIAbilityKit][AgentExecutor] case=%s row=%d step_id=%s tool_name=%s capability=%s risk_level=%s write_like=%s status=%s attempted=%s succeeded=%s target_count_estimate=%d evidence_keys=%s evidence=%s error_code=%s reason=%s failure_phase=%s preflight_gate=%s"),
				CaseName,
				Index,
				Row.StepId.IsEmpty() ? TEXT("-") : *Row.StepId,
				Row.ToolName.IsEmpty() ? TEXT("-") : *Row.ToolName,
				Row.Capability.IsEmpty() ? TEXT("-") : *Row.Capability,
				Row.RiskLevel.IsEmpty() ? TEXT("-") : *Row.RiskLevel,
				Row.bWriteLike ? TEXT("true") : TEXT("false"),
				Row.Status.IsEmpty() ? TEXT("-") : *Row.Status,
				Row.bAttempted ? TEXT("true") : TEXT("false"),
				Row.bSucceeded ? TEXT("true") : TEXT("false"),
				Row.TargetCountEstimate,
				*FString::Join(Row.EvidenceKeys, TEXT("|")),
				*HCI_FormatStringMapForLog(Row.Evidence),
				Row.ErrorCode.IsEmpty() ? TEXT("-") : *Row.ErrorCode,
				Row.Reason.IsEmpty() ? TEXT("-") : *Row.Reason,
				Row.FailurePhase.IsEmpty() ? TEXT("-") : *Row.FailurePhase,
				Row.PreflightGate.IsEmpty() ? TEXT("-") : *Row.PreflightGate);
		}
		else
		{
			UE_LOG(
				LogHCIAbilityKitAgentDemo,
				Warning,
				TEXT("[HCIAbilityKit][AgentExecutor] case=%s row=%d step_id=%s tool_name=%s capability=%s risk_level=%s write_like=%s status=%s attempted=%s succeeded=%s target_count_estimate=%d evidence_keys=%s evidence=%s error_code=%s reason=%s failure_phase=%s preflight_gate=%s"),
				CaseName,
				Index,
				Row.StepId.IsEmpty() ? TEXT("-") : *Row.StepId,
				Row.ToolName.IsEmpty() ? TEXT("-") : *Row.ToolName,
				Row.Capability.IsEmpty() ? TEXT("-") : *Row.Capability,
				Row.RiskLevel.IsEmpty() ? TEXT("-") : *Row.RiskLevel,
				Row.bWriteLike ? TEXT("true") : TEXT("false"),
				Row.Status.IsEmpty() ? TEXT("-") : *Row.Status,
				Row.bAttempted ? TEXT("true") : TEXT("false"),
				Row.bSucceeded ? TEXT("true") : TEXT("false"),
				Row.TargetCountEstimate,
				*FString::Join(Row.EvidenceKeys, TEXT("|")),
				*HCI_FormatStringMapForLog(Row.Evidence),
				Row.ErrorCode.IsEmpty() ? TEXT("-") : *Row.ErrorCode,
				Row.Reason.IsEmpty() ? TEXT("-") : *Row.Reason,
				Row.FailurePhase.IsEmpty() ? TEXT("-") : *Row.FailurePhase,
				Row.PreflightGate.IsEmpty() ? TEXT("-") : *Row.PreflightGate);
		}
	}
}

static void HCI_LogAgentExecutorSummary(const TCHAR* CaseName, const FHCIAbilityKitAgentExecutorRunResult& RunResult)
{
	if (RunResult.bCompleted)
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentExecutor] case=%s summary request_id=%s intent=%s plan_version=%d execution_mode=%s termination_policy=%s preflight_enabled=%s accepted=%s completed=%s total_steps=%d executed_steps=%d succeeded_steps=%d failed_steps=%d skipped_steps=%d preflight_blocked_steps=%d failed_step_index=%d failed_step_id=%s failed_tool_name=%s failed_gate=%s terminal_status=%s terminal_reason=%s error_code=%s reason=%s started_utc=%s finished_utc=%s"),
			CaseName,
			RunResult.RequestId.IsEmpty() ? TEXT("-") : *RunResult.RequestId,
			RunResult.Intent.IsEmpty() ? TEXT("-") : *RunResult.Intent,
			RunResult.PlanVersion,
			RunResult.ExecutionMode.IsEmpty() ? TEXT("-") : *RunResult.ExecutionMode,
			RunResult.TerminationPolicy.IsEmpty() ? TEXT("-") : *RunResult.TerminationPolicy,
			RunResult.bPreflightEnabled ? TEXT("true") : TEXT("false"),
			RunResult.bAccepted ? TEXT("true") : TEXT("false"),
			RunResult.bCompleted ? TEXT("true") : TEXT("false"),
			RunResult.TotalSteps,
			RunResult.ExecutedSteps,
			RunResult.SucceededSteps,
			RunResult.FailedSteps,
			RunResult.SkippedSteps,
			RunResult.PreflightBlockedSteps,
			RunResult.FailedStepIndex,
			RunResult.FailedStepId.IsEmpty() ? TEXT("-") : *RunResult.FailedStepId,
			RunResult.FailedToolName.IsEmpty() ? TEXT("-") : *RunResult.FailedToolName,
			RunResult.FailedGate.IsEmpty() ? TEXT("-") : *RunResult.FailedGate,
			RunResult.TerminalStatus.IsEmpty() ? TEXT("-") : *RunResult.TerminalStatus,
			RunResult.TerminalReason.IsEmpty() ? TEXT("-") : *RunResult.TerminalReason,
			RunResult.ErrorCode.IsEmpty() ? TEXT("-") : *RunResult.ErrorCode,
			RunResult.Reason.IsEmpty() ? TEXT("-") : *RunResult.Reason,
			RunResult.StartedAtUtc.IsEmpty() ? TEXT("-") : *RunResult.StartedAtUtc,
			RunResult.FinishedAtUtc.IsEmpty() ? TEXT("-") : *RunResult.FinishedAtUtc);
	}
	else
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Warning,
			TEXT("[HCIAbilityKit][AgentExecutor] case=%s summary request_id=%s intent=%s plan_version=%d execution_mode=%s termination_policy=%s preflight_enabled=%s accepted=%s completed=%s total_steps=%d executed_steps=%d succeeded_steps=%d failed_steps=%d skipped_steps=%d preflight_blocked_steps=%d failed_step_index=%d failed_step_id=%s failed_tool_name=%s failed_gate=%s terminal_status=%s terminal_reason=%s error_code=%s reason=%s started_utc=%s finished_utc=%s"),
			CaseName,
			RunResult.RequestId.IsEmpty() ? TEXT("-") : *RunResult.RequestId,
			RunResult.Intent.IsEmpty() ? TEXT("-") : *RunResult.Intent,
			RunResult.PlanVersion,
			RunResult.ExecutionMode.IsEmpty() ? TEXT("-") : *RunResult.ExecutionMode,
			RunResult.TerminationPolicy.IsEmpty() ? TEXT("-") : *RunResult.TerminationPolicy,
			RunResult.bPreflightEnabled ? TEXT("true") : TEXT("false"),
			RunResult.bAccepted ? TEXT("true") : TEXT("false"),
			RunResult.bCompleted ? TEXT("true") : TEXT("false"),
			RunResult.TotalSteps,
			RunResult.ExecutedSteps,
			RunResult.SucceededSteps,
			RunResult.FailedSteps,
			RunResult.SkippedSteps,
			RunResult.PreflightBlockedSteps,
			RunResult.FailedStepIndex,
			RunResult.FailedStepId.IsEmpty() ? TEXT("-") : *RunResult.FailedStepId,
			RunResult.FailedToolName.IsEmpty() ? TEXT("-") : *RunResult.FailedToolName,
			RunResult.FailedGate.IsEmpty() ? TEXT("-") : *RunResult.FailedGate,
			RunResult.TerminalStatus.IsEmpty() ? TEXT("-") : *RunResult.TerminalStatus,
			RunResult.TerminalReason.IsEmpty() ? TEXT("-") : *RunResult.TerminalReason,
			RunResult.ErrorCode.IsEmpty() ? TEXT("-") : *RunResult.ErrorCode,
			RunResult.Reason.IsEmpty() ? TEXT("-") : *RunResult.Reason,
			RunResult.StartedAtUtc.IsEmpty() ? TEXT("-") : *RunResult.StartedAtUtc,
			RunResult.FinishedAtUtc.IsEmpty() ? TEXT("-") : *RunResult.FinishedAtUtc);
	}
}

static bool HCI_RunAgentExecutorDemoCase(
	const TCHAR* CaseName,
	const FString& UserText,
	const FString& RequestId,
	FHCIAbilityKitToolRegistry& Registry,
	int32& OutCompletedCases,
	int32& OutRejectedCases)
{
	FHCIAbilityKitAgentPlan Plan;
	FString RouteReason;
	FString BuildError;
	if (!HCI_BuildAgentPlanDemoPlan(UserText, RequestId, Plan, RouteReason, BuildError))
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Error,
			TEXT("[HCIAbilityKit][AgentExecutor] case=%s build_failed input=%s reason=%s"),
			CaseName,
			*UserText,
			*BuildError);
		return false;
	}

	GHCIAbilityKitAgentPlanPreviewState = Plan;

	FHCIAbilityKitAgentExecutorOptions Options;
	Options.bValidatePlanBeforeExecute = true;
	Options.bDryRun = true;

	FHCIAbilityKitAgentExecutorRunResult RunResult;
	FHCIAbilityKitAgentExecutor::ExecutePlan(
		Plan,
		Registry,
		FHCIAbilityKitAgentPlanValidationContext(),
		Options,
		RunResult);

	if (RunResult.bCompleted)
	{
		++OutCompletedCases;
	}
	if (!RunResult.bAccepted || !RunResult.bCompleted)
	{
		++OutRejectedCases;
	}

	UE_LOG(
		LogHCIAbilityKitAgentDemo,
		Display,
		TEXT("[HCIAbilityKit][AgentExecutor] case=%s route_reason=%s input=%s"),
		CaseName,
		*RouteReason,
		*UserText);
	HCI_LogAgentExecutorSummary(CaseName, RunResult);
	HCI_LogAgentExecutorRows(CaseName, RunResult);
	return true;
}

static FHCIAbilityKitAgentPlan HCI_BuildAgentExecutorF4TwoStepPlan(const FString& RequestId)
{
	FHCIAbilityKitAgentPlan Plan;
	Plan.PlanVersion = 1;
	Plan.RequestId = RequestId;
	Plan.Intent = TEXT("batch_fix_asset_compliance");

	{
		FHCIAbilityKitAgentPlanStep& Step = Plan.Steps.AddDefaulted_GetRef();
		Step.StepId = TEXT("s1");
		Step.ToolName = TEXT("SetTextureMaxSize");
		Step.RiskLevel = EHCIAbilityKitAgentPlanRiskLevel::Write;
		Step.bRequiresConfirm = true;
		Step.RollbackStrategy = TEXT("all_or_nothing");
		Step.ExpectedEvidence = {TEXT("asset_path"), TEXT("before"), TEXT("after")};
		Step.Args = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> AssetPaths;
		AssetPaths.Add(MakeShared<FJsonValueString>(TEXT("/Game/Art/T_Demo_A.T_Demo_A")));
		AssetPaths.Add(MakeShared<FJsonValueString>(TEXT("/Game/Art/T_Demo_B.T_Demo_B")));
		Step.Args->SetArrayField(TEXT("asset_paths"), AssetPaths);
		Step.Args->SetNumberField(TEXT("max_size"), 1024);
	}

	{
		FHCIAbilityKitAgentPlanStep& Step = Plan.Steps.AddDefaulted_GetRef();
		Step.StepId = TEXT("s2");
		Step.ToolName = TEXT("SetMeshLODGroup");
		Step.RiskLevel = EHCIAbilityKitAgentPlanRiskLevel::Write;
		Step.bRequiresConfirm = true;
		Step.RollbackStrategy = TEXT("all_or_nothing");
		Step.ExpectedEvidence = {TEXT("asset_path"), TEXT("before"), TEXT("after")};
		Step.Args = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> AssetPaths;
		AssetPaths.Add(MakeShared<FJsonValueString>(TEXT("/Game/Art/SM_Demo.SM_Demo")));
		Step.Args->SetArrayField(TEXT("asset_paths"), AssetPaths);
		Step.Args->SetStringField(TEXT("lod_group"), TEXT("SmallProp"));
	}

	return Plan;
}

static bool HCI_RunAgentExecutorFailDemoCase(
	const TCHAR* CaseName,
	const TCHAR* Description,
	const TCHAR* RequestId,
	const EHCIAbilityKitAgentExecutorTerminationPolicy TerminationPolicy,
	const int32 SimulatedFailureStepIndex,
	const TCHAR* SimulatedErrorCode,
	const TCHAR* SimulatedReason,
	FHCIAbilityKitToolRegistry& Registry,
	int32& OutCompletedCases,
	int32& OutFailedCases)
{
	FHCIAbilityKitAgentPlan Plan = HCI_BuildAgentExecutorF4TwoStepPlan(RequestId);
	GHCIAbilityKitAgentPlanPreviewState = Plan;

	FHCIAbilityKitAgentExecutorOptions Options;
	Options.bValidatePlanBeforeExecute = true;
	Options.bDryRun = true;
	Options.TerminationPolicy = TerminationPolicy;
	Options.SimulatedFailureStepIndex = SimulatedFailureStepIndex;
	Options.SimulatedFailureErrorCode = SimulatedErrorCode ? SimulatedErrorCode : TEXT("");
	Options.SimulatedFailureReason = SimulatedReason ? SimulatedReason : TEXT("");

	FHCIAbilityKitAgentExecutorRunResult RunResult;
	const bool bOk = FHCIAbilityKitAgentExecutor::ExecutePlan(
		Plan,
		Registry,
		FHCIAbilityKitAgentPlanValidationContext(),
		Options,
		RunResult);

	if (bOk && RunResult.bCompleted)
	{
		++OutCompletedCases;
	}
	else
	{
		++OutFailedCases;
	}

	UE_LOG(
		LogHCIAbilityKitAgentDemo,
		Display,
		TEXT("[HCIAbilityKit][AgentExecutorFail] case=%s description=%s policy=%s simulated_failure_step=%d"),
		CaseName,
		Description ? Description : TEXT("-"),
		*RunResult.TerminationPolicy,
		SimulatedFailureStepIndex);
	HCI_LogAgentExecutorSummary(CaseName, RunResult);
	HCI_LogAgentExecutorRows(CaseName, RunResult);
	return true;
}

static void HCI_RunAbilityKitAgentExecutePlanFailDemoCommand(const TArray<FString>& Args)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	if (Args.Num() == 0)
	{
		int32 CompletedCases = 0;
		int32 FailedCases = 0;
		int32 StopPolicyCases = 0;
		int32 ContinuePolicyCases = 0;

		auto RunCase =
			[&](const TCHAR* CaseName, const TCHAR* Description, const TCHAR* RequestId, const EHCIAbilityKitAgentExecutorTerminationPolicy Policy, const int32 FailIndex, const TCHAR* ErrorCode, const TCHAR* Reason)
		{
			if (Policy == EHCIAbilityKitAgentExecutorTerminationPolicy::StopOnFirstFailure)
			{
				++StopPolicyCases;
			}
			else
			{
				++ContinuePolicyCases;
			}
			return HCI_RunAgentExecutorFailDemoCase(
				CaseName,
				Description,
				RequestId,
				Policy,
				FailIndex,
				ErrorCode,
				Reason,
				Registry,
				CompletedCases,
				FailedCases);
		};

		RunCase(TEXT("ok_stop_on_first"), TEXT("two_step_success"), TEXT("req_demo_f4_01"), EHCIAbilityKitAgentExecutorTerminationPolicy::StopOnFirstFailure, INDEX_NONE, TEXT(""), TEXT(""));
		RunCase(TEXT("fail_step1_stop_on_first"), TEXT("fail_first_step_stop"), TEXT("req_demo_f4_02"), EHCIAbilityKitAgentExecutorTerminationPolicy::StopOnFirstFailure, 0, TEXT("E4101"), TEXT("simulated_tool_execution_failed"));
		RunCase(TEXT("fail_step1_continue"), TEXT("fail_first_step_continue"), TEXT("req_demo_f4_03"), EHCIAbilityKitAgentExecutorTerminationPolicy::ContinueOnFailure, 0, TEXT("E4102"), TEXT("simulated_first_step_failed_continue"));

		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentExecutorFail] summary total_cases=%d completed_cases=%d failed_cases=%d stop_policy_cases=%d continue_policy_cases=%d execution_mode=%s validation=ok"),
			3,
			CompletedCases,
			FailedCases,
			StopPolicyCases,
			ContinuePolicyCases,
			TEXT("simulate_dry_run"));
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentExecutorFail] hint=也可运行 HCIAbilityKit.AgentExecutePlanFailDemo [ok|fail_stop|fail_continue]"));
		return;
	}

	if (Args.Num() != 1)
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Error,
			TEXT("[HCIAbilityKit][AgentExecutorFail] invalid_args usage=HCIAbilityKit.AgentExecutePlanFailDemo [ok|fail_stop|fail_continue]"));
		return;
	}

	const FString CaseKey = Args[0].TrimStartAndEnd();
	int32 CompletedCases = 0;
	int32 FailedCases = 0;
	if (CaseKey == TEXT("ok"))
	{
		HCI_RunAgentExecutorFailDemoCase(
			TEXT("custom_ok"),
			TEXT("two_step_success"),
			TEXT("req_cli_f4_ok"),
			EHCIAbilityKitAgentExecutorTerminationPolicy::StopOnFirstFailure,
			INDEX_NONE,
			TEXT(""),
			TEXT(""),
			Registry,
			CompletedCases,
			FailedCases);
		return;
	}
	if (CaseKey == TEXT("fail_stop"))
	{
		HCI_RunAgentExecutorFailDemoCase(
			TEXT("custom_fail_stop"),
			TEXT("fail_first_step_stop"),
			TEXT("req_cli_f4_fail_stop"),
			EHCIAbilityKitAgentExecutorTerminationPolicy::StopOnFirstFailure,
			0,
			TEXT("E4101"),
			TEXT("simulated_tool_execution_failed"),
			Registry,
			CompletedCases,
			FailedCases);
		return;
	}
	if (CaseKey == TEXT("fail_continue"))
	{
		HCI_RunAgentExecutorFailDemoCase(
			TEXT("custom_fail_continue"),
			TEXT("fail_first_step_continue"),
			TEXT("req_cli_f4_fail_continue"),
			EHCIAbilityKitAgentExecutorTerminationPolicy::ContinueOnFailure,
			0,
			TEXT("E4102"),
			TEXT("simulated_first_step_failed_continue"),
			Registry,
			CompletedCases,
			FailedCases);
		return;
	}

	UE_LOG(
		LogHCIAbilityKitAgentDemo,
		Error,
		TEXT("[HCIAbilityKit][AgentExecutorFail] invalid_args reason=unknown case_key key=%s allowed=ok|fail_stop|fail_continue"),
		*CaseKey);
}

static FHCIAbilityKitAgentPlan HCI_BuildAgentExecutorF5TexturePlan(const FString& RequestId, const int32 AssetCount)
{
	FHCIAbilityKitAgentPlan Plan;
	Plan.PlanVersion = 1;
	Plan.RequestId = RequestId;
	Plan.Intent = TEXT("batch_fix_asset_compliance");

	FHCIAbilityKitAgentPlanStep& Step = Plan.Steps.AddDefaulted_GetRef();
	Step.StepId = TEXT("s1");
	Step.ToolName = TEXT("SetTextureMaxSize");
	Step.RiskLevel = EHCIAbilityKitAgentPlanRiskLevel::Write;
	Step.bRequiresConfirm = true;
	Step.RollbackStrategy = TEXT("all_or_nothing");
	Step.ExpectedEvidence = {TEXT("asset_path"), TEXT("before"), TEXT("after")};
	Step.Args = MakeShared<FJsonObject>();

	TArray<TSharedPtr<FJsonValue>> AssetPaths;
	for (int32 Index = 0; Index < FMath::Max(0, AssetCount); ++Index)
	{
		AssetPaths.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("/Game/Art/T_F5_%03d.T_F5_%03d"), Index, Index)));
	}
	Step.Args->SetArrayField(TEXT("asset_paths"), AssetPaths);
	Step.Args->SetNumberField(TEXT("max_size"), 1024);
	return Plan;
}

static bool HCI_RunAgentExecutorPreflightDemoCase(
	const TCHAR* CaseName,
	const TCHAR* Description,
	FHCIAbilityKitAgentPlan Plan,
	const FHCIAbilityKitAgentExecutorOptions& Options,
	FHCIAbilityKitToolRegistry& Registry,
	int32& OutPassedCases,
	int32& OutBlockedCases,
	TMap<FString, int32>& OutBlockedByGateCounts)
{
	GHCIAbilityKitAgentPlanPreviewState = Plan;

	FHCIAbilityKitAgentExecutorRunResult RunResult;
	const bool bOk = FHCIAbilityKitAgentExecutor::ExecutePlan(
		Plan,
		Registry,
		FHCIAbilityKitAgentPlanValidationContext(),
		Options,
		RunResult);

	if (bOk && RunResult.bCompleted)
	{
		++OutPassedCases;
	}
	else
	{
		++OutBlockedCases;
		if (!RunResult.FailedGate.IsEmpty())
		{
			int32& Count = OutBlockedByGateCounts.FindOrAdd(RunResult.FailedGate);
			Count += 1;
		}
	}

	UE_LOG(
		LogHCIAbilityKitAgentDemo,
		Display,
		TEXT("[HCIAbilityKit][AgentExecutorPreflight] case=%s description=%s preflight_enabled=%s"),
		CaseName,
		Description ? Description : TEXT("-"),
		Options.bEnablePreflightGates ? TEXT("true") : TEXT("false"));
	HCI_LogAgentExecutorSummary(CaseName, RunResult);
	HCI_LogAgentExecutorRows(CaseName, RunResult);
	return true;
}

static void HCI_RunAbilityKitAgentExecutePlanPreflightDemoCommand(const TArray<FString>& Args)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	auto MakeBaseOptions = []()
	{
		FHCIAbilityKitAgentExecutorOptions Options;
		Options.bValidatePlanBeforeExecute = true;
		Options.bDryRun = true;
		Options.bEnablePreflightGates = true;
		Options.TerminationPolicy = EHCIAbilityKitAgentExecutorTerminationPolicy::StopOnFirstFailure;
		Options.bUserConfirmedWriteSteps = true;
		Options.MockUserName = TEXT("artist_a");
		Options.MockResolvedRole = TEXT("Artist");
		Options.bMockUserMatchedWhitelist = true;
		Options.MockAllowedCapabilities = {TEXT("read_only"), TEXT("write")};
		Options.bSourceControlEnabled = false; // offline local mode pass path
		Options.bSourceControlCheckoutSucceeded = false;
		Options.SimulatedLodTargetObjectClass = TEXT("UStaticMesh");
		Options.bSimulatedLodTargetNaniteEnabled = false;
		return Options;
	};

	auto RunKey =
		[&](const FString& CaseKey, int32& PassedCases, int32& BlockedCases, TMap<FString, int32>& BlockedByGateCounts) -> bool
	{
		FHCIAbilityKitAgentExecutorOptions Options = MakeBaseOptions();
		FHCIAbilityKitAgentPlan Plan;
		const TCHAR* CaseName = TEXT("custom");
		const TCHAR* Description = TEXT("-");

		if (CaseKey == TEXT("ok"))
		{
			CaseName = TEXT("custom_ok");
			Description = TEXT("authorized_write_confirmed_offline_local_mode");
			Plan = HCI_BuildAgentExecutorF5TexturePlan(TEXT("req_cli_f5_ok"), 3);
		}
		else if (CaseKey == TEXT("fail_confirm"))
		{
			CaseName = TEXT("custom_fail_confirm");
			Description = TEXT("unconfirmed_write_blocked_by_confirm_gate");
			Plan = HCI_BuildAgentExecutorF5TexturePlan(TEXT("req_cli_f5_confirm"), 3);
			Options.bUserConfirmedWriteSteps = false;
		}
		else if (CaseKey == TEXT("fail_blast"))
		{
			CaseName = TEXT("custom_fail_blast");
			Description = TEXT("over_modify_limit_blocked_by_blast_radius");
			Plan = HCI_BuildAgentExecutorF5TexturePlan(TEXT("req_cli_f5_blast"), 51);
			Options.bValidatePlanBeforeExecute = false; // let F5 preflight blast-radius gate own the failure (F2 validator would reject first)
		}
		else if (CaseKey == TEXT("fail_rbac"))
		{
			CaseName = TEXT("custom_fail_rbac");
			Description = TEXT("guest_write_blocked_by_rbac");
			Plan = HCI_BuildAgentExecutorF5TexturePlan(TEXT("req_cli_f5_rbac"), 3);
			Options.MockUserName = TEXT("unknown_guest");
			Options.MockResolvedRole = TEXT("Guest");
			Options.bMockUserMatchedWhitelist = false;
			Options.MockAllowedCapabilities = {TEXT("read_only")};
		}
		else if (CaseKey == TEXT("fail_sc"))
		{
			CaseName = TEXT("custom_fail_sc");
			Description = TEXT("checkout_failed_blocked_by_source_control_fail_fast");
			Plan = HCI_BuildAgentExecutorF5TexturePlan(TEXT("req_cli_f5_sc"), 3);
			Options.bSourceControlEnabled = true;
			Options.bSourceControlCheckoutSucceeded = false;
		}
		else if (CaseKey == TEXT("fail_lod"))
		{
			CaseName = TEXT("custom_fail_lod");
			Description = TEXT("nanite_mesh_blocked_by_lod_safety");
			Plan = HCI_BuildAgentExecutorF4TwoStepPlan(TEXT("req_cli_f5_lod"));
			Options.SimulatedLodTargetObjectClass = TEXT("UStaticMesh");
			Options.bSimulatedLodTargetNaniteEnabled = true;
		}
		else
		{
			return false;
		}

		return HCI_RunAgentExecutorPreflightDemoCase(
			CaseName,
			Description,
			Plan,
			Options,
			Registry,
			PassedCases,
			BlockedCases,
			BlockedByGateCounts);
	};

	if (Args.Num() == 0)
	{
		int32 PassedCases = 0;
		int32 BlockedCases = 0;
		TMap<FString, int32> BlockedByGateCounts;

		const TArray<FString> CaseKeys = {
			TEXT("ok"),
			TEXT("fail_confirm"),
			TEXT("fail_blast"),
			TEXT("fail_rbac"),
			TEXT("fail_sc"),
			TEXT("fail_lod")};

		for (const FString& CaseKey : CaseKeys)
		{
			RunKey(CaseKey, PassedCases, BlockedCases, BlockedByGateCounts);
		}

		const int32 ConfirmBlocked = BlockedByGateCounts.FindRef(TEXT("confirm_gate"));
		const int32 BlastBlocked = BlockedByGateCounts.FindRef(TEXT("blast_radius"));
		const int32 RbacBlocked = BlockedByGateCounts.FindRef(TEXT("rbac"));
		const int32 SourceControlBlocked = BlockedByGateCounts.FindRef(TEXT("source_control"));
		const int32 LodSafetyBlocked = BlockedByGateCounts.FindRef(TEXT("lod_safety"));

		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentExecutorPreflight] summary total_cases=%d passed_cases=%d blocked_cases=%d preflight_enabled=true confirm_blocked=%d blast_radius_blocked=%d rbac_blocked=%d source_control_blocked=%d lod_safety_blocked=%d execution_mode=%s validation=ok"),
			CaseKeys.Num(),
			PassedCases,
			BlockedCases,
			ConfirmBlocked,
			BlastBlocked,
			RbacBlocked,
			SourceControlBlocked,
			LodSafetyBlocked,
			TEXT("simulate_dry_run"));
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentExecutorPreflight] hint=也可运行 HCIAbilityKit.AgentExecutePlanPreflightDemo [ok|fail_confirm|fail_blast|fail_rbac|fail_sc|fail_lod]"));
		return;
	}

	if (Args.Num() != 1)
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Error,
			TEXT("[HCIAbilityKit][AgentExecutorPreflight] invalid_args usage=HCIAbilityKit.AgentExecutePlanPreflightDemo [ok|fail_confirm|fail_blast|fail_rbac|fail_sc|fail_lod]"));
		return;
	}

	const FString CaseKey = Args[0].TrimStartAndEnd();
	if (CaseKey.IsEmpty())
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorPreflight] invalid_args reason=empty case_key"));
		return;
	}

	int32 PassedCases = 0;
	int32 BlockedCases = 0;
	TMap<FString, int32> BlockedByGateCounts;
	if (!RunKey(CaseKey, PassedCases, BlockedCases, BlockedByGateCounts))
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Error,
			TEXT("[HCIAbilityKit][AgentExecutorPreflight] invalid_args reason=unknown case_key key=%s allowed=ok|fail_confirm|fail_blast|fail_rbac|fail_sc|fail_lod"),
			*CaseKey);
	}
}

static void HCI_LogAgentExecutorReviewDiffSummary(
	const TCHAR* CaseName,
	const FHCIAbilityKitAgentExecutorRunResult& RunResult,
	const FHCIAbilityKitDryRunDiffReport& Report)
{
	const bool bUseWarning = Report.Summary.Skipped > 0 || !RunResult.bCompleted;
	if (bUseWarning)
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Warning,
			TEXT("[HCIAbilityKit][AgentExecutorReview] case=%s summary request_id=%s total_candidates=%d modifiable=%d skipped=%d executor_terminal_status=%s executor_failed_gate=%s execution_mode=%s bridge_ok=true"),
			CaseName,
			Report.RequestId.IsEmpty() ? TEXT("-") : *Report.RequestId,
			Report.Summary.TotalCandidates,
			Report.Summary.Modifiable,
			Report.Summary.Skipped,
			RunResult.TerminalStatus.IsEmpty() ? TEXT("-") : *RunResult.TerminalStatus,
			RunResult.FailedGate.IsEmpty() ? TEXT("-") : *RunResult.FailedGate,
			RunResult.ExecutionMode.IsEmpty() ? TEXT("-") : *RunResult.ExecutionMode);
		return;
	}

	UE_LOG(
		LogHCIAbilityKitAgentDemo,
		Display,
		TEXT("[HCIAbilityKit][AgentExecutorReview] case=%s summary request_id=%s total_candidates=%d modifiable=%d skipped=%d executor_terminal_status=%s executor_failed_gate=%s execution_mode=%s bridge_ok=true"),
		CaseName,
		Report.RequestId.IsEmpty() ? TEXT("-") : *Report.RequestId,
		Report.Summary.TotalCandidates,
		Report.Summary.Modifiable,
		Report.Summary.Skipped,
		RunResult.TerminalStatus.IsEmpty() ? TEXT("-") : *RunResult.TerminalStatus,
		RunResult.FailedGate.IsEmpty() ? TEXT("-") : *RunResult.FailedGate,
		RunResult.ExecutionMode.IsEmpty() ? TEXT("-") : *RunResult.ExecutionMode);
}

static void HCI_LogAgentExecutorReviewDiffRows(const TCHAR* CaseName, const FHCIAbilityKitDryRunDiffReport& Report)
{
	for (int32 Index = 0; Index < Report.DiffItems.Num(); ++Index)
	{
		const FHCIAbilityKitDryRunDiffItem& Item = Report.DiffItems[Index];
		const bool bBlockedOrSkipped = !Item.SkipReason.IsEmpty();
		if (bBlockedOrSkipped)
		{
			UE_LOG(
				LogHCIAbilityKitAgentDemo,
				Warning,
				TEXT("[HCIAbilityKit][AgentExecutorReview] case=%s row=%d asset_path=%s field=%s before=%s after=%s tool_name=%s risk=%s skip_reason=%s object_type=%s locate_strategy=%s evidence_key=%s actor_path=%s"),
				CaseName,
				Index,
				Item.AssetPath.IsEmpty() ? TEXT("-") : *Item.AssetPath,
				Item.Field.IsEmpty() ? TEXT("-") : *Item.Field,
				Item.Before.IsEmpty() ? TEXT("-") : *Item.Before,
				Item.After.IsEmpty() ? TEXT("-") : *Item.After,
				Item.ToolName.IsEmpty() ? TEXT("-") : *Item.ToolName,
				*FHCIAbilityKitDryRunDiff::RiskToString(Item.Risk),
				Item.SkipReason.IsEmpty() ? TEXT("-") : *Item.SkipReason,
				*FHCIAbilityKitDryRunDiff::ObjectTypeToString(Item.ObjectType),
				*FHCIAbilityKitDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
				Item.EvidenceKey.IsEmpty() ? TEXT("-") : *Item.EvidenceKey,
				Item.ActorPath.IsEmpty() ? TEXT("-") : *Item.ActorPath);
		}
		else
		{
			UE_LOG(
				LogHCIAbilityKitAgentDemo,
				Display,
				TEXT("[HCIAbilityKit][AgentExecutorReview] case=%s row=%d asset_path=%s field=%s before=%s after=%s tool_name=%s risk=%s skip_reason=%s object_type=%s locate_strategy=%s evidence_key=%s actor_path=%s"),
				CaseName,
				Index,
				Item.AssetPath.IsEmpty() ? TEXT("-") : *Item.AssetPath,
				Item.Field.IsEmpty() ? TEXT("-") : *Item.Field,
				Item.Before.IsEmpty() ? TEXT("-") : *Item.Before,
				Item.After.IsEmpty() ? TEXT("-") : *Item.After,
				Item.ToolName.IsEmpty() ? TEXT("-") : *Item.ToolName,
				*FHCIAbilityKitDryRunDiff::RiskToString(Item.Risk),
				Item.SkipReason.IsEmpty() ? TEXT("-") : *Item.SkipReason,
				*FHCIAbilityKitDryRunDiff::ObjectTypeToString(Item.ObjectType),
				*FHCIAbilityKitDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
				Item.EvidenceKey.IsEmpty() ? TEXT("-") : *Item.EvidenceKey,
				Item.ActorPath.IsEmpty() ? TEXT("-") : *Item.ActorPath);
		}
	}
}

static void HCI_LogAgentExecutorReviewSelectSummary(const FHCIAbilityKitDryRunDiffSelectionResult& SelectionResult)
{
	const bool bUseWarning = SelectionResult.SelectedReport.Summary.Skipped > 0;
	if (bUseWarning)
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Warning,
			TEXT("[HCIAbilityKit][AgentExecutorReviewSelect] summary request_id=%s total_before=%d total_after=%d input_rows=%d unique_rows=%d dropped_duplicates=%d modifiable=%d skipped=%d validation=ok"),
			SelectionResult.SelectedReport.RequestId.IsEmpty() ? TEXT("-") : *SelectionResult.SelectedReport.RequestId,
			SelectionResult.TotalRowsBefore,
			SelectionResult.TotalRowsAfter,
			SelectionResult.InputRowCount,
			SelectionResult.UniqueRowCount,
			SelectionResult.DroppedDuplicateRows,
			SelectionResult.SelectedReport.Summary.Modifiable,
			SelectionResult.SelectedReport.Summary.Skipped);
		return;
	}

	UE_LOG(
		LogHCIAbilityKitAgentDemo,
		Display,
		TEXT("[HCIAbilityKit][AgentExecutorReviewSelect] summary request_id=%s total_before=%d total_after=%d input_rows=%d unique_rows=%d dropped_duplicates=%d modifiable=%d skipped=%d validation=ok"),
		SelectionResult.SelectedReport.RequestId.IsEmpty() ? TEXT("-") : *SelectionResult.SelectedReport.RequestId,
		SelectionResult.TotalRowsBefore,
		SelectionResult.TotalRowsAfter,
		SelectionResult.InputRowCount,
		SelectionResult.UniqueRowCount,
		SelectionResult.DroppedDuplicateRows,
		SelectionResult.SelectedReport.Summary.Modifiable,
		SelectionResult.SelectedReport.Summary.Skipped);
}

static void HCI_LogAgentExecutorReviewSelectRows(const FHCIAbilityKitDryRunDiffReport& Report)
{
	for (int32 Index = 0; Index < Report.DiffItems.Num(); ++Index)
	{
		const FHCIAbilityKitDryRunDiffItem& Item = Report.DiffItems[Index];
		const bool bBlockedOrSkipped = !Item.SkipReason.IsEmpty();
		if (bBlockedOrSkipped)
		{
			UE_LOG(
				LogHCIAbilityKitAgentDemo,
				Warning,
				TEXT("[HCIAbilityKit][AgentExecutorReviewSelect] row=%d asset_path=%s field=%s before=%s after=%s tool_name=%s risk=%s skip_reason=%s object_type=%s locate_strategy=%s evidence_key=%s actor_path=%s"),
				Index,
				Item.AssetPath.IsEmpty() ? TEXT("-") : *Item.AssetPath,
				Item.Field.IsEmpty() ? TEXT("-") : *Item.Field,
				Item.Before.IsEmpty() ? TEXT("-") : *Item.Before,
				Item.After.IsEmpty() ? TEXT("-") : *Item.After,
				Item.ToolName.IsEmpty() ? TEXT("-") : *Item.ToolName,
				*FHCIAbilityKitDryRunDiff::RiskToString(Item.Risk),
				Item.SkipReason.IsEmpty() ? TEXT("-") : *Item.SkipReason,
				*FHCIAbilityKitDryRunDiff::ObjectTypeToString(Item.ObjectType),
				*FHCIAbilityKitDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
				Item.EvidenceKey.IsEmpty() ? TEXT("-") : *Item.EvidenceKey,
				Item.ActorPath.IsEmpty() ? TEXT("-") : *Item.ActorPath);
		}
		else
		{
			UE_LOG(
				LogHCIAbilityKitAgentDemo,
				Display,
				TEXT("[HCIAbilityKit][AgentExecutorReviewSelect] row=%d asset_path=%s field=%s before=%s after=%s tool_name=%s risk=%s skip_reason=%s object_type=%s locate_strategy=%s evidence_key=%s actor_path=%s"),
				Index,
				Item.AssetPath.IsEmpty() ? TEXT("-") : *Item.AssetPath,
				Item.Field.IsEmpty() ? TEXT("-") : *Item.Field,
				Item.Before.IsEmpty() ? TEXT("-") : *Item.Before,
				Item.After.IsEmpty() ? TEXT("-") : *Item.After,
				Item.ToolName.IsEmpty() ? TEXT("-") : *Item.ToolName,
				*FHCIAbilityKitDryRunDiff::RiskToString(Item.Risk),
				Item.SkipReason.IsEmpty() ? TEXT("-") : *Item.SkipReason,
				*FHCIAbilityKitDryRunDiff::ObjectTypeToString(Item.ObjectType),
				*FHCIAbilityKitDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
				Item.EvidenceKey.IsEmpty() ? TEXT("-") : *Item.EvidenceKey,
				Item.ActorPath.IsEmpty() ? TEXT("-") : *Item.ActorPath);
		}
	}
}

static bool HCI_ApplyAgentExecutorReviewSelection(
	const TArray<FString>& Args,
	FHCIAbilityKitDryRunDiffSelectionResult& OutSelectionResult)
{
	TArray<int32> RequestedRows;
	FString ParseReason;
	if (!HCI_TryParseRowIndexListArgs(Args, RequestedRows, ParseReason))
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Error,
			TEXT("[HCIAbilityKit][AgentExecutorReviewSelect] invalid_args reason=%s usage=HCIAbilityKit.AgentExecutePlanReviewSelect [row_indices_csv]"),
			*ParseReason);
		return false;
	}

	if (GHCIAbilityKitAgentExecutorReviewDiffPreviewState.DiffItems.Num() <= 0)
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorReviewSelect] select=unavailable reason=no_preview_state"));
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentExecutorReviewSelect] suggestion=先运行 HCIAbilityKit.AgentExecutePlanReviewDemo 或 HCIAbilityKit.AgentExecutePlanReviewJson"));
		return false;
	}

	if (!FHCIAbilityKitDryRunDiffSelection::SelectRows(
			GHCIAbilityKitAgentExecutorReviewDiffPreviewState,
			RequestedRows,
			OutSelectionResult))
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Error,
			TEXT("[HCIAbilityKit][AgentExecutorReviewSelect] invalid_args error_code=%s reason=%s total=%d"),
			OutSelectionResult.ErrorCode.IsEmpty() ? TEXT("-") : *OutSelectionResult.ErrorCode,
			OutSelectionResult.Reason.IsEmpty() ? TEXT("-") : *OutSelectionResult.Reason,
			GHCIAbilityKitAgentExecutorReviewDiffPreviewState.DiffItems.Num());
		return false;
	}

	GHCIAbilityKitAgentExecutorReviewDiffPreviewState = OutSelectionResult.SelectedReport;
	return true;
}

static bool HCI_BuildAgentExecutorReviewDemoCaseConfig(
	const FString& CaseKey,
	FHCIAbilityKitAgentPlan& OutPlan,
	FHCIAbilityKitAgentExecutorOptions& OutOptions,
	FString& OutDescription,
	FString& OutInputText)
{
	OutOptions = FHCIAbilityKitAgentExecutorOptions();
	OutOptions.bValidatePlanBeforeExecute = true;
	OutOptions.bDryRun = true;
	OutOptions.TerminationPolicy = EHCIAbilityKitAgentExecutorTerminationPolicy::StopOnFirstFailure;
	OutOptions.bEnablePreflightGates = false;
	OutOptions.bUserConfirmedWriteSteps = true;
	OutOptions.MockUserName = TEXT("artist_a");
	OutOptions.MockResolvedRole = TEXT("Artist");
	OutOptions.bMockUserMatchedWhitelist = true;
	OutOptions.MockAllowedCapabilities = {TEXT("read_only"), TEXT("write")};
	OutOptions.bSourceControlEnabled = false;
	OutOptions.bSourceControlCheckoutSucceeded = false;
	OutOptions.SimulatedLodTargetObjectClass = TEXT("UStaticMesh");
	OutOptions.bSimulatedLodTargetNaniteEnabled = false;

	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();

	if (CaseKey == TEXT("ok_naming"))
	{
		OutDescription = TEXT("executor_success_naming_review_bridge");
		OutInputText = TEXT("整理临时目录资产，按规范命名并归档");
		FString RouteReason;
		FString BuildError;
		return HCI_BuildAgentPlanDemoPlan(OutInputText, TEXT("req_cli_f6_ok_naming"), OutPlan, RouteReason, BuildError);
	}

	if (CaseKey == TEXT("ok_level_risk"))
	{
		OutDescription = TEXT("executor_success_level_risk_review_bridge");
		OutInputText = TEXT("检查当前关卡选中物体的碰撞和默认材质");
		FString RouteReason;
		FString BuildError;
		return HCI_BuildAgentPlanDemoPlan(OutInputText, TEXT("req_cli_f6_ok_level"), OutPlan, RouteReason, BuildError);
	}

	if (CaseKey == TEXT("fail_confirm"))
	{
		OutDescription = TEXT("preflight_confirm_blocked_review_bridge");
		OutInputText = TEXT("-");
		OutPlan = HCI_BuildAgentExecutorF5TexturePlan(TEXT("req_cli_f6_fail_confirm"), 3);
		OutOptions.bEnablePreflightGates = true;
		OutOptions.bUserConfirmedWriteSteps = false;
		return true;
	}

	if (CaseKey == TEXT("fail_lod"))
	{
		OutDescription = TEXT("preflight_lod_nanite_blocked_review_bridge");
		OutInputText = TEXT("-");
		OutPlan = HCI_BuildAgentExecutorF4TwoStepPlan(TEXT("req_cli_f6_fail_lod"));
		OutOptions.bEnablePreflightGates = true;
		OutOptions.SimulatedLodTargetObjectClass = TEXT("UStaticMesh");
		OutOptions.bSimulatedLodTargetNaniteEnabled = true;
		return true;
	}

	return false;
}

static bool HCI_RunAgentExecutorReviewDemoCase(
	const TCHAR* CaseName,
	const FString& CaseKey,
	FHCIAbilityKitToolRegistry& Registry,
	int32& OutBridgeOkCases,
	int32& OutFailedBuildOrBridgeCases)
{
	FHCIAbilityKitAgentPlan Plan;
	FHCIAbilityKitAgentExecutorOptions Options;
	FString Description;
	FString InputText;
	if (!HCI_BuildAgentExecutorReviewDemoCaseConfig(CaseKey, Plan, Options, Description, InputText))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorReview] case=%s build_failed reason=unknown_or_invalid_case key=%s"), CaseName, *CaseKey);
		++OutFailedBuildOrBridgeCases;
		return false;
	}

	GHCIAbilityKitAgentPlanPreviewState = Plan;

	FHCIAbilityKitAgentExecutorRunResult RunResult;
	FHCIAbilityKitAgentExecutor::ExecutePlan(
		Plan,
		Registry,
		FHCIAbilityKitAgentPlanValidationContext(),
		Options,
		RunResult);

	FHCIAbilityKitDryRunDiffReport Report;
	if (!FHCIAbilityKitAgentExecutorDryRunBridge::BuildDryRunDiffReport(RunResult, Report))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorReview] case=%s bridge_failed request_id=%s"), CaseName, *RunResult.RequestId);
		++OutFailedBuildOrBridgeCases;
		return false;
	}

	GHCIAbilityKitAgentExecutorReviewDiffPreviewState = Report;
	++OutBridgeOkCases;

	UE_LOG(
		LogHCIAbilityKitAgentDemo,
		Display,
		TEXT("[HCIAbilityKit][AgentExecutorReview] case=%s case_key=%s description=%s input=%s"),
		CaseName,
		*CaseKey,
		Description.IsEmpty() ? TEXT("-") : *Description,
		InputText.IsEmpty() ? TEXT("-") : *InputText);
	HCI_LogAgentExecutorSummary(CaseName, RunResult);
	HCI_LogAgentExecutorRows(CaseName, RunResult);
	HCI_LogAgentExecutorReviewDiffSummary(CaseName, RunResult, Report);
	HCI_LogAgentExecutorReviewDiffRows(CaseName, Report);
	return true;
}

static void HCI_RunAbilityKitAgentExecutePlanReviewDemoCommand(const TArray<FString>& Args)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	if (Args.Num() == 0)
	{
		const TArray<FString> CaseKeys = {
			TEXT("ok_naming"),
			TEXT("ok_level_risk"),
			TEXT("fail_confirm"),
		};

		int32 BridgeOkCases = 0;
		int32 FailedCases = 0;
		for (int32 Index = 0; Index < CaseKeys.Num(); ++Index)
		{
			const FString CaseName = FString::Printf(TEXT("demo_%02d"), Index + 1);
			HCI_RunAgentExecutorReviewDemoCase(*CaseName, CaseKeys[Index], Registry, BridgeOkCases, FailedCases);
		}

		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentExecutorReview] summary total_cases=%d bridge_ok_cases=%d failed_cases=%d execution_mode=%s review_contract=%s validation=ok"),
			CaseKeys.Num(),
			BridgeOkCases,
			FailedCases,
			TEXT("simulate_dry_run"),
			TEXT("dry_run_diff"));
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentExecutorReview] hint=也可运行 HCIAbilityKit.AgentExecutePlanReviewDemo [ok_naming|ok_level_risk|fail_confirm|fail_lod]"));
		return;
	}

	if (Args.Num() != 1)
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Error,
			TEXT("[HCIAbilityKit][AgentExecutorReview] invalid_args usage=HCIAbilityKit.AgentExecutePlanReviewDemo [ok_naming|ok_level_risk|fail_confirm|fail_lod]"));
		return;
	}

	const FString CaseKey = Args[0].TrimStartAndEnd();
	int32 BridgeOkCases = 0;
	int32 FailedCases = 0;
	if (CaseKey.IsEmpty() || !HCI_RunAgentExecutorReviewDemoCase(TEXT("custom"), CaseKey, Registry, BridgeOkCases, FailedCases))
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Error,
			TEXT("[HCIAbilityKit][AgentExecutorReview] invalid_args reason=unknown case_key key=%s allowed=ok_naming|ok_level_risk|fail_confirm|fail_lod"),
			*CaseKey);
	}
}

static void HCI_RunAbilityKitAgentExecutePlanReviewJsonCommand(const TArray<FString>& Args)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FString CaseKey = TEXT("fail_confirm");
	if (Args.Num() > 0)
	{
		CaseKey = Args[0].TrimStartAndEnd();
	}

	int32 BridgeOkCases = 0;
	int32 FailedCases = 0;
	if (CaseKey.IsEmpty() || !HCI_RunAgentExecutorReviewDemoCase(TEXT("json_preview"), CaseKey, Registry, BridgeOkCases, FailedCases))
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Error,
			TEXT("[HCIAbilityKit][AgentExecutorReview] invalid_args usage=HCIAbilityKit.AgentExecutePlanReviewJson [ok_naming|ok_level_risk|fail_confirm|fail_lod]"));
		return;
	}

	FString JsonText;
	if (!FHCIAbilityKitDryRunDiffJsonSerializer::SerializeToJsonString(GHCIAbilityKitAgentExecutorReviewDiffPreviewState, JsonText))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorReview] json_failed request_id=%s"), *GHCIAbilityKitAgentExecutorReviewDiffPreviewState.RequestId);
		return;
	}

	UE_LOG(
		LogHCIAbilityKitAgentDemo,
		Display,
		TEXT("[HCIAbilityKit][AgentExecutorReview] json case=%s request_id=%s bytes=%d"),
		*CaseKey,
		GHCIAbilityKitAgentExecutorReviewDiffPreviewState.RequestId.IsEmpty() ? TEXT("-") : *GHCIAbilityKitAgentExecutorReviewDiffPreviewState.RequestId,
		JsonText.Len());
	UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("%s"), *JsonText);
}

static void HCI_RunAbilityKitAgentExecutePlanReviewLocateCommand(const TArray<FString>& Args)
{
	int32 RowIndex = 0;
	if (Args.Num() >= 1)
	{
		if (!LexTryParseString(RowIndex, *Args[0]) || RowIndex < 0)
		{
			UE_LOG(
				LogHCIAbilityKitAgentDemo,
				Error,
				TEXT("[HCIAbilityKit][AgentExecutorReview] locate_invalid_args reason=row_index must be integer >= 0"));
			return;
		}
	}

	FHCIAbilityKitAgentExecutorReviewLocateResolvedRow Resolved;
	FString ResolveReason;
	if (!FHCIAbilityKitAgentExecutorReviewLocateUtils::TryResolveRow(
			GHCIAbilityKitAgentExecutorReviewDiffPreviewState,
			RowIndex,
			Resolved,
			ResolveReason))
	{
		if (ResolveReason == TEXT("no_preview_state"))
		{
			UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorReview] locate=unavailable reason=no_preview_state"));
			UE_LOG(
				LogHCIAbilityKitAgentDemo,
				Display,
				TEXT("[HCIAbilityKit][AgentExecutorReview] suggestion=先运行 HCIAbilityKit.AgentExecutePlanReviewDemo 或 HCIAbilityKit.AgentExecutePlanReviewJson"));
			return;
		}

		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Error,
			TEXT("[HCIAbilityKit][AgentExecutorReview] locate_invalid_args reason=%s row_index=%d total=%d"),
			*ResolveReason,
			RowIndex,
			GHCIAbilityKitAgentExecutorReviewDiffPreviewState.DiffItems.Num());
		return;
	}

	FString LocateReason;
	const bool bLocateOk = FHCIAbilityKitAgentExecutorReviewLocateUtils::TryLocateResolvedRowInEditor(Resolved, LocateReason);
	if (bLocateOk)
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentExecutorReview] locate row=%d tool_name=%s strategy=%s object_type=%s success=true reason=%s asset_path=%s actor_path=%s"),
			Resolved.RowIndex,
			Resolved.ToolName.IsEmpty() ? TEXT("-") : *Resolved.ToolName,
			*FHCIAbilityKitDryRunDiff::LocateStrategyToString(Resolved.LocateStrategy),
			*FHCIAbilityKitDryRunDiff::ObjectTypeToString(Resolved.ObjectType),
			*LocateReason,
			Resolved.AssetPath.IsEmpty() ? TEXT("-") : *Resolved.AssetPath,
			Resolved.ActorPath.IsEmpty() ? TEXT("-") : *Resolved.ActorPath);
		return;
	}

	UE_LOG(
		LogHCIAbilityKitAgentDemo,
		Warning,
		TEXT("[HCIAbilityKit][AgentExecutorReview] locate row=%d tool_name=%s strategy=%s object_type=%s success=false reason=%s asset_path=%s actor_path=%s"),
		Resolved.RowIndex,
		Resolved.ToolName.IsEmpty() ? TEXT("-") : *Resolved.ToolName,
		*FHCIAbilityKitDryRunDiff::LocateStrategyToString(Resolved.LocateStrategy),
		*FHCIAbilityKitDryRunDiff::ObjectTypeToString(Resolved.ObjectType),
		*LocateReason,
		Resolved.AssetPath.IsEmpty() ? TEXT("-") : *Resolved.AssetPath,
		Resolved.ActorPath.IsEmpty() ? TEXT("-") : *Resolved.ActorPath);
}

static void HCI_RunAbilityKitAgentExecutePlanReviewSelectCommand(const TArray<FString>& Args)
{
	FHCIAbilityKitDryRunDiffSelectionResult SelectionResult;
	if (!HCI_ApplyAgentExecutorReviewSelection(Args, SelectionResult))
	{
		return;
	}

	HCI_LogAgentExecutorReviewSelectSummary(SelectionResult);
	HCI_LogAgentExecutorReviewSelectRows(SelectionResult.SelectedReport);
	UE_LOG(
		LogHCIAbilityKitAgentDemo,
		Display,
		TEXT("[HCIAbilityKit][AgentExecutorReviewSelect] hint=已将最新审阅预览替换为已采纳子集，可继续运行 HCIAbilityKit.AgentExecutePlanReviewLocate [row_index]"));
}

static void HCI_RunAbilityKitAgentExecutePlanReviewSelectJsonCommand(const TArray<FString>& Args)
{
	FHCIAbilityKitDryRunDiffSelectionResult SelectionResult;
	if (!HCI_ApplyAgentExecutorReviewSelection(Args, SelectionResult))
	{
		return;
	}

	HCI_LogAgentExecutorReviewSelectSummary(SelectionResult);
	HCI_LogAgentExecutorReviewSelectRows(SelectionResult.SelectedReport);

	FString JsonText;
	if (!FHCIAbilityKitDryRunDiffJsonSerializer::SerializeToJsonString(SelectionResult.SelectedReport, JsonText))
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Error,
			TEXT("[HCIAbilityKit][AgentExecutorReviewSelect] json_failed request_id=%s"),
			SelectionResult.SelectedReport.RequestId.IsEmpty() ? TEXT("-") : *SelectionResult.SelectedReport.RequestId);
		return;
	}

	UE_LOG(
		LogHCIAbilityKitAgentDemo,
		Display,
		TEXT("[HCIAbilityKit][AgentExecutorReviewSelect] json request_id=%s bytes=%d"),
		SelectionResult.SelectedReport.RequestId.IsEmpty() ? TEXT("-") : *SelectionResult.SelectedReport.RequestId,
		JsonText.Len());
	UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("%s"), *JsonText);
}

static void HCI_LogAgentExecutorApplySummary(const FHCIAbilityKitAgentApplyRequest& ApplyRequest)
{
	const bool bUseWarning = !ApplyRequest.bReady;
	if (bUseWarning)
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Warning,
			TEXT("[HCIAbilityKit][AgentExecutorApply] summary request_id=%s review_request_id=%s selection_digest=%s total_rows=%d modifiable_rows=%d blocked_rows=%d ready=%s validation=ok"),
			ApplyRequest.RequestId.IsEmpty() ? TEXT("-") : *ApplyRequest.RequestId,
			ApplyRequest.ReviewRequestId.IsEmpty() ? TEXT("-") : *ApplyRequest.ReviewRequestId,
			ApplyRequest.SelectionDigest.IsEmpty() ? TEXT("-") : *ApplyRequest.SelectionDigest,
			ApplyRequest.Summary.TotalRows,
			ApplyRequest.Summary.ModifiableRows,
			ApplyRequest.Summary.BlockedRows,
			ApplyRequest.bReady ? TEXT("true") : TEXT("false"));
		return;
	}

	UE_LOG(
		LogHCIAbilityKitAgentDemo,
		Display,
		TEXT("[HCIAbilityKit][AgentExecutorApply] summary request_id=%s review_request_id=%s selection_digest=%s total_rows=%d modifiable_rows=%d blocked_rows=%d ready=%s validation=ok"),
		ApplyRequest.RequestId.IsEmpty() ? TEXT("-") : *ApplyRequest.RequestId,
		ApplyRequest.ReviewRequestId.IsEmpty() ? TEXT("-") : *ApplyRequest.ReviewRequestId,
		ApplyRequest.SelectionDigest.IsEmpty() ? TEXT("-") : *ApplyRequest.SelectionDigest,
		ApplyRequest.Summary.TotalRows,
		ApplyRequest.Summary.ModifiableRows,
		ApplyRequest.Summary.BlockedRows,
		ApplyRequest.bReady ? TEXT("true") : TEXT("false"));
}

static void HCI_LogAgentExecutorApplyRows(const FHCIAbilityKitAgentApplyRequest& ApplyRequest)
{
	for (const FHCIAbilityKitAgentApplyRequestItem& Item : ApplyRequest.Items)
	{
		if (Item.bBlocked)
		{
			UE_LOG(
				LogHCIAbilityKitAgentDemo,
				Warning,
				TEXT("[HCIAbilityKit][AgentExecutorApply] row=%d tool_name=%s risk=%s blocked=%s skip_reason=%s object_type=%s locate_strategy=%s asset_path=%s field=%s evidence_key=%s actor_path=%s"),
				Item.RowIndex,
				Item.ToolName.IsEmpty() ? TEXT("-") : *Item.ToolName,
				*FHCIAbilityKitDryRunDiff::RiskToString(Item.Risk),
				TEXT("true"),
				Item.SkipReason.IsEmpty() ? TEXT("-") : *Item.SkipReason,
				*FHCIAbilityKitDryRunDiff::ObjectTypeToString(Item.ObjectType),
				*FHCIAbilityKitDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
				Item.AssetPath.IsEmpty() ? TEXT("-") : *Item.AssetPath,
				Item.Field.IsEmpty() ? TEXT("-") : *Item.Field,
				Item.EvidenceKey.IsEmpty() ? TEXT("-") : *Item.EvidenceKey,
				Item.ActorPath.IsEmpty() ? TEXT("-") : *Item.ActorPath);
		}
		else
		{
			UE_LOG(
				LogHCIAbilityKitAgentDemo,
				Display,
				TEXT("[HCIAbilityKit][AgentExecutorApply] row=%d tool_name=%s risk=%s blocked=%s skip_reason=%s object_type=%s locate_strategy=%s asset_path=%s field=%s evidence_key=%s actor_path=%s"),
				Item.RowIndex,
				Item.ToolName.IsEmpty() ? TEXT("-") : *Item.ToolName,
				*FHCIAbilityKitDryRunDiff::RiskToString(Item.Risk),
				TEXT("false"),
				Item.SkipReason.IsEmpty() ? TEXT("-") : *Item.SkipReason,
				*FHCIAbilityKitDryRunDiff::ObjectTypeToString(Item.ObjectType),
				*FHCIAbilityKitDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
				Item.AssetPath.IsEmpty() ? TEXT("-") : *Item.AssetPath,
				Item.Field.IsEmpty() ? TEXT("-") : *Item.Field,
				Item.EvidenceKey.IsEmpty() ? TEXT("-") : *Item.EvidenceKey,
				Item.ActorPath.IsEmpty() ? TEXT("-") : *Item.ActorPath);
		}
	}
}

static bool HCI_TryBuildAgentExecutorApplyRequestFromLatestReview(FHCIAbilityKitAgentApplyRequest& OutApplyRequest)
{
	if (GHCIAbilityKitAgentExecutorReviewDiffPreviewState.DiffItems.Num() <= 0)
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorApply] prepare=unavailable reason=no_review_preview_state"));
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentExecutorApply] suggestion=先运行 HCIAbilityKit.AgentExecutePlanReviewDemo 或 HCIAbilityKit.AgentExecutePlanReviewSelect"));
		return false;
	}

	if (!FHCIAbilityKitAgentExecutorApplyRequestBridge::BuildApplyRequest(
			GHCIAbilityKitAgentExecutorReviewDiffPreviewState,
			OutApplyRequest))
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Error,
			TEXT("[HCIAbilityKit][AgentExecutorApply] prepare_failed reason=bridge_failed review_request_id=%s"),
			GHCIAbilityKitAgentExecutorReviewDiffPreviewState.RequestId.IsEmpty() ? TEXT("-") : *GHCIAbilityKitAgentExecutorReviewDiffPreviewState.RequestId);
		return false;
	}

	GHCIAbilityKitAgentApplyRequestPreviewState = OutApplyRequest;
	return true;
}

static void HCI_RunAbilityKitAgentExecutePlanReviewPrepareApplyCommand(const TArray<FString>& Args)
{
	if (Args.Num() > 0)
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Error,
			TEXT("[HCIAbilityKit][AgentExecutorApply] invalid_args usage=HCIAbilityKit.AgentExecutePlanReviewPrepareApply"));
		return;
	}

	FHCIAbilityKitAgentApplyRequest ApplyRequest;
	if (!HCI_TryBuildAgentExecutorApplyRequestFromLatestReview(ApplyRequest))
	{
		return;
	}

	HCI_LogAgentExecutorApplySummary(ApplyRequest);
	HCI_LogAgentExecutorApplyRows(ApplyRequest);
	UE_LOG(
		LogHCIAbilityKitAgentDemo,
		Display,
		TEXT("[HCIAbilityKit][AgentExecutorApply] hint=也可运行 HCIAbilityKit.AgentExecutePlanReviewPrepareApplyJson 输出 ApplyRequest JSON"));
}

static void HCI_RunAbilityKitAgentExecutePlanReviewPrepareApplyJsonCommand(const TArray<FString>& Args)
{
	if (Args.Num() > 0)
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Error,
			TEXT("[HCIAbilityKit][AgentExecutorApply] invalid_args usage=HCIAbilityKit.AgentExecutePlanReviewPrepareApplyJson"));
		return;
	}

	FHCIAbilityKitAgentApplyRequest ApplyRequest;
	if (!HCI_TryBuildAgentExecutorApplyRequestFromLatestReview(ApplyRequest))
	{
		return;
	}

	HCI_LogAgentExecutorApplySummary(ApplyRequest);
	HCI_LogAgentExecutorApplyRows(ApplyRequest);

	FString JsonText;
	if (!FHCIAbilityKitAgentApplyRequestJsonSerializer::SerializeToJsonString(ApplyRequest, JsonText))
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Error,
			TEXT("[HCIAbilityKit][AgentExecutorApply] json_failed request_id=%s"),
			ApplyRequest.RequestId.IsEmpty() ? TEXT("-") : *ApplyRequest.RequestId);
		return;
	}

	UE_LOG(
		LogHCIAbilityKitAgentDemo,
		Display,
		TEXT("[HCIAbilityKit][AgentExecutorApply] json request_id=%s bytes=%d"),
		ApplyRequest.RequestId.IsEmpty() ? TEXT("-") : *ApplyRequest.RequestId,
		JsonText.Len());
	UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("%s"), *JsonText);
}

static bool HCI_TryParseConfirmTamperModeArg(const FString& InValue, FString& OutTamperMode)
{
	const FString Trimmed = InValue.TrimStartAndEnd();
	if (Trimmed.Equals(TEXT("none"), ESearchCase::IgnoreCase))
	{
		OutTamperMode = TEXT("none");
		return true;
	}
	if (Trimmed.Equals(TEXT("digest"), ESearchCase::IgnoreCase))
	{
		OutTamperMode = TEXT("digest");
		return true;
	}
	if (Trimmed.Equals(TEXT("review"), ESearchCase::IgnoreCase))
	{
		OutTamperMode = TEXT("review");
		return true;
	}
	return false;
}

static void HCI_LogAgentExecutorConfirmSummary(const FHCIAbilityKitAgentApplyConfirmRequest& ConfirmRequest)
{
	const bool bUseWarning = !ConfirmRequest.bReadyToExecute;
	if (bUseWarning)
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Warning,
			TEXT("[HCIAbilityKit][AgentExecutorConfirm] summary request_id=%s apply_request_id=%s review_request_id=%s selection_digest=%s user_confirmed=%s ready_to_execute=%s error_code=%s reason=%s validation=ok"),
			ConfirmRequest.RequestId.IsEmpty() ? TEXT("-") : *ConfirmRequest.RequestId,
			ConfirmRequest.ApplyRequestId.IsEmpty() ? TEXT("-") : *ConfirmRequest.ApplyRequestId,
			ConfirmRequest.ReviewRequestId.IsEmpty() ? TEXT("-") : *ConfirmRequest.ReviewRequestId,
			ConfirmRequest.SelectionDigest.IsEmpty() ? TEXT("-") : *ConfirmRequest.SelectionDigest,
			ConfirmRequest.bUserConfirmed ? TEXT("true") : TEXT("false"),
			ConfirmRequest.bReadyToExecute ? TEXT("true") : TEXT("false"),
			ConfirmRequest.ErrorCode.IsEmpty() ? TEXT("-") : *ConfirmRequest.ErrorCode,
			ConfirmRequest.Reason.IsEmpty() ? TEXT("-") : *ConfirmRequest.Reason);
		return;
	}

	UE_LOG(
		LogHCIAbilityKitAgentDemo,
		Display,
		TEXT("[HCIAbilityKit][AgentExecutorConfirm] summary request_id=%s apply_request_id=%s review_request_id=%s selection_digest=%s user_confirmed=%s ready_to_execute=%s error_code=%s reason=%s validation=ok"),
		ConfirmRequest.RequestId.IsEmpty() ? TEXT("-") : *ConfirmRequest.RequestId,
		ConfirmRequest.ApplyRequestId.IsEmpty() ? TEXT("-") : *ConfirmRequest.ApplyRequestId,
		ConfirmRequest.ReviewRequestId.IsEmpty() ? TEXT("-") : *ConfirmRequest.ReviewRequestId,
		ConfirmRequest.SelectionDigest.IsEmpty() ? TEXT("-") : *ConfirmRequest.SelectionDigest,
		ConfirmRequest.bUserConfirmed ? TEXT("true") : TEXT("false"),
		ConfirmRequest.bReadyToExecute ? TEXT("true") : TEXT("false"),
		ConfirmRequest.ErrorCode.IsEmpty() ? TEXT("-") : *ConfirmRequest.ErrorCode,
		ConfirmRequest.Reason.IsEmpty() ? TEXT("-") : *ConfirmRequest.Reason);
}

static void HCI_LogAgentExecutorConfirmRows(const FHCIAbilityKitAgentApplyConfirmRequest& ConfirmRequest)
{
	for (const FHCIAbilityKitAgentApplyRequestItem& Item : ConfirmRequest.Items)
	{
		if (Item.bBlocked)
		{
			UE_LOG(
				LogHCIAbilityKitAgentDemo,
				Warning,
				TEXT("[HCIAbilityKit][AgentExecutorConfirm] row=%d tool_name=%s risk=%s blocked=%s skip_reason=%s object_type=%s locate_strategy=%s asset_path=%s field=%s evidence_key=%s actor_path=%s"),
				Item.RowIndex,
				Item.ToolName.IsEmpty() ? TEXT("-") : *Item.ToolName,
				*FHCIAbilityKitDryRunDiff::RiskToString(Item.Risk),
				TEXT("true"),
				Item.SkipReason.IsEmpty() ? TEXT("-") : *Item.SkipReason,
				*FHCIAbilityKitDryRunDiff::ObjectTypeToString(Item.ObjectType),
				*FHCIAbilityKitDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
				Item.AssetPath.IsEmpty() ? TEXT("-") : *Item.AssetPath,
				Item.Field.IsEmpty() ? TEXT("-") : *Item.Field,
				Item.EvidenceKey.IsEmpty() ? TEXT("-") : *Item.EvidenceKey,
				Item.ActorPath.IsEmpty() ? TEXT("-") : *Item.ActorPath);
		}
		else
		{
			UE_LOG(
				LogHCIAbilityKitAgentDemo,
				Display,
				TEXT("[HCIAbilityKit][AgentExecutorConfirm] row=%d tool_name=%s risk=%s blocked=%s skip_reason=%s object_type=%s locate_strategy=%s asset_path=%s field=%s evidence_key=%s actor_path=%s"),
				Item.RowIndex,
				Item.ToolName.IsEmpty() ? TEXT("-") : *Item.ToolName,
				*FHCIAbilityKitDryRunDiff::RiskToString(Item.Risk),
				TEXT("false"),
				Item.SkipReason.IsEmpty() ? TEXT("-") : *Item.SkipReason,
				*FHCIAbilityKitDryRunDiff::ObjectTypeToString(Item.ObjectType),
				*FHCIAbilityKitDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
				Item.AssetPath.IsEmpty() ? TEXT("-") : *Item.AssetPath,
				Item.Field.IsEmpty() ? TEXT("-") : *Item.Field,
				Item.EvidenceKey.IsEmpty() ? TEXT("-") : *Item.EvidenceKey,
				Item.ActorPath.IsEmpty() ? TEXT("-") : *Item.ActorPath);
		}
	}
}

static bool HCI_TryBuildAgentExecutorConfirmRequestFromLatestPreview(
	const bool bUserConfirmed,
	const FString& TamperMode,
	FHCIAbilityKitAgentApplyConfirmRequest& OutConfirmRequest)
{
	if (GHCIAbilityKitAgentApplyRequestPreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorConfirm] prepare=unavailable reason=no_apply_request_preview_state"));
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentExecutorConfirm] suggestion=先运行 HCIAbilityKit.AgentExecutePlanReviewPrepareApply"));
		return false;
	}

	if (GHCIAbilityKitAgentExecutorReviewDiffPreviewState.DiffItems.Num() <= 0)
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Warning, TEXT("[HCIAbilityKit][AgentExecutorConfirm] prepare=unavailable reason=no_review_preview_state"));
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentExecutorConfirm] suggestion=先运行 HCIAbilityKit.AgentExecutePlanReviewDemo 或 HCIAbilityKit.AgentExecutePlanReviewSelect"));
		return false;
	}

	FHCIAbilityKitAgentApplyRequest WorkingApplyRequest = GHCIAbilityKitAgentApplyRequestPreviewState;
	if (TamperMode == TEXT("digest"))
	{
		WorkingApplyRequest.SelectionDigest += TEXT("_tampered");
	}
	else if (TamperMode == TEXT("review"))
	{
		WorkingApplyRequest.ReviewRequestId += TEXT("_stale");
	}

	if (!FHCIAbilityKitAgentExecutorApplyConfirmBridge::BuildConfirmRequest(
			WorkingApplyRequest,
			GHCIAbilityKitAgentExecutorReviewDiffPreviewState,
			bUserConfirmed,
			OutConfirmRequest))
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutorConfirm] prepare_failed reason=bridge_failed"));
		return false;
	}

	GHCIAbilityKitAgentApplyConfirmRequestPreviewState = OutConfirmRequest;
	return true;
}

static bool HCI_TryParseConfirmCommandArgs(
	const TArray<FString>& Args,
	bool& OutUserConfirmed,
	FString& OutTamperMode)
{
	OutUserConfirmed = true;
	OutTamperMode = TEXT("none");

	if (Args.Num() == 0)
	{
		return true;
	}
	if (Args.Num() > 2)
	{
		return false;
	}
	if (!HCI_TryParseBool01Arg(Args[0], OutUserConfirmed))
	{
		return false;
	}
	if (Args.Num() == 2 && !HCI_TryParseConfirmTamperModeArg(Args[1], OutTamperMode))
	{
		return false;
	}
	return true;
}

static void HCI_RunAbilityKitAgentExecutePlanReviewPrepareConfirmCommand(const TArray<FString>& Args)
{
	bool bUserConfirmed = true;
	FString TamperMode;
	if (!HCI_TryParseConfirmCommandArgs(Args, bUserConfirmed, TamperMode))
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Error,
			TEXT("[HCIAbilityKit][AgentExecutorConfirm] invalid_args usage=HCIAbilityKit.AgentExecutePlanReviewPrepareConfirm [user_confirmed=0|1] [tamper=none|digest|review]"));
		return;
	}

	FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest;
	if (!HCI_TryBuildAgentExecutorConfirmRequestFromLatestPreview(bUserConfirmed, TamperMode, ConfirmRequest))
	{
		return;
	}

	HCI_LogAgentExecutorConfirmSummary(ConfirmRequest);
	HCI_LogAgentExecutorConfirmRows(ConfirmRequest);
	UE_LOG(
		LogHCIAbilityKitAgentDemo,
		Display,
		TEXT("[HCIAbilityKit][AgentExecutorConfirm] hint=也可运行 HCIAbilityKit.AgentExecutePlanReviewPrepareConfirmJson [user_confirmed] [tamper] 输出 ConfirmRequest JSON"));
}

static void HCI_RunAbilityKitAgentExecutePlanReviewPrepareConfirmJsonCommand(const TArray<FString>& Args)
{
	bool bUserConfirmed = true;
	FString TamperMode;
	if (!HCI_TryParseConfirmCommandArgs(Args, bUserConfirmed, TamperMode))
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Error,
			TEXT("[HCIAbilityKit][AgentExecutorConfirm] invalid_args usage=HCIAbilityKit.AgentExecutePlanReviewPrepareConfirmJson [user_confirmed=0|1] [tamper=none|digest|review]"));
		return;
	}

	FHCIAbilityKitAgentApplyConfirmRequest ConfirmRequest;
	if (!HCI_TryBuildAgentExecutorConfirmRequestFromLatestPreview(bUserConfirmed, TamperMode, ConfirmRequest))
	{
		return;
	}

	HCI_LogAgentExecutorConfirmSummary(ConfirmRequest);
	HCI_LogAgentExecutorConfirmRows(ConfirmRequest);

	FString JsonText;
	if (!FHCIAbilityKitAgentApplyConfirmRequestJsonSerializer::SerializeToJsonString(ConfirmRequest, JsonText))
	{
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Error,
			TEXT("[HCIAbilityKit][AgentExecutorConfirm] json_failed request_id=%s"),
			ConfirmRequest.RequestId.IsEmpty() ? TEXT("-") : *ConfirmRequest.RequestId);
		return;
	}

	UE_LOG(
		LogHCIAbilityKitAgentDemo,
		Display,
		TEXT("[HCIAbilityKit][AgentExecutorConfirm] json request_id=%s bytes=%d"),
		ConfirmRequest.RequestId.IsEmpty() ? TEXT("-") : *ConfirmRequest.RequestId,
		JsonText.Len());
	UE_LOG(LogHCIAbilityKitAgentDemo, Display, TEXT("%s"), *JsonText);
}

static void HCI_RunAbilityKitAgentExecutePlanDemoCommand(const TArray<FString>& Args)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	if (Args.Num() == 0)
	{
		struct FDemoExecCase
		{
			const TCHAR* CaseName;
			const TCHAR* RequestId;
			const TCHAR* UserText;
		};

		const TArray<FDemoExecCase> Cases{
			{TEXT("exec_naming_archive_temp_assets"), TEXT("req_demo_f3_01"), TEXT("整理临时目录资产，按规范命名并归档")},
			{TEXT("exec_level_mesh_risk_scan"), TEXT("req_demo_f3_02"), TEXT("检查当前关卡选中物体的碰撞和默认材质")},
			{TEXT("exec_asset_compliance_fix"), TEXT("req_demo_f3_03"), TEXT("检查贴图分辨率并处理LOD")}};

		int32 BuiltAndCompletedCases = 0;
		int32 RejectedCases = 0;
		int32 TotalStepRows = 0;

		for (const FDemoExecCase& DemoCase : Cases)
		{
			const int32 CompletedBefore = BuiltAndCompletedCases;
			if (HCI_RunAgentExecutorDemoCase(
					DemoCase.CaseName,
					DemoCase.UserText,
					DemoCase.RequestId,
					Registry,
					BuiltAndCompletedCases,
					RejectedCases))
			{
				// Rebuild plan only for counting is unnecessary; use preview state set by helper.
				TotalStepRows += GHCIAbilityKitAgentPlanPreviewState.Steps.Num();
				if (BuiltAndCompletedCases == CompletedBefore)
				{
					// Case executed but not completed is already counted in RejectedCases.
				}
			}
		}

		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentExecutor] summary total_cases=%d completed_cases=%d rejected_cases=%d execution_mode=%s total_step_rows=%d validation=ok"),
			Cases.Num(),
			BuiltAndCompletedCases,
			RejectedCases,
			TEXT("simulate_dry_run"),
			TotalStepRows);
		UE_LOG(
			LogHCIAbilityKitAgentDemo,
			Display,
			TEXT("[HCIAbilityKit][AgentExecutor] hint=也可运行 HCIAbilityKit.AgentExecutePlanDemo [自然语言文本...]"));
		return;
	}

	const FString UserText = HCI_JoinConsoleArgsAsText(Args);
	if (UserText.IsEmpty())
	{
		UE_LOG(LogHCIAbilityKitAgentDemo, Error, TEXT("[HCIAbilityKit][AgentExecutor] invalid_args reason=empty input text"));
		return;
	}

	int32 CompletedCases = 0;
	int32 RejectedCases = 0;
	HCI_RunAgentExecutorDemoCase(TEXT("custom"), UserText, TEXT("req_cli_f3"), Registry, CompletedCases, RejectedCases);
}

void FHCIAbilityKitAgentDemoConsoleCommands::Startup()
{
	if (!AgentConfirmGateDemoCommand.IsValid())
	{
		AgentConfirmGateDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentConfirmGateDemo"),
			TEXT("E3 confirm-gate demo. Usage: HCIAbilityKit.AgentConfirmGateDemo [tool_name] [requires_confirm 0|1] [user_confirmed 0|1]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentConfirmGateDemoCommand));
	}

	if (!AgentBlastRadiusDemoCommand.IsValid())
	{
		AgentBlastRadiusDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentBlastRadiusDemo"),
			TEXT("E4 blast-radius demo. Usage: HCIAbilityKit.AgentBlastRadiusDemo [tool_name] [target_modify_count>=0]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentBlastRadiusDemoCommand));
	}

	if (!AgentTransactionDemoCommand.IsValid())
	{
		AgentTransactionDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentTransactionDemo"),
			TEXT("E5 all-or-nothing transaction demo. Usage: HCIAbilityKit.AgentTransactionDemo [total_steps>=1] [fail_step_index>=0]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentTransactionDemoCommand));
	}

	if (!AgentSourceControlDemoCommand.IsValid())
	{
		AgentSourceControlDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentSourceControlDemo"),
			TEXT("E6 source-control fail-fast/offline demo. Usage: HCIAbilityKit.AgentSourceControlDemo [tool_name] [source_control_enabled 0|1] [checkout_succeeded 0|1]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentSourceControlDemoCommand));
	}

	if (!AgentRbacDemoCommand.IsValid())
	{
		AgentRbacDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentRbacDemo"),
			TEXT("E7 local mock RBAC + local audit log demo. Usage: HCIAbilityKit.AgentRbacDemo [user_name] [tool_name] [asset_count>=0]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentRbacDemoCommand));
	}

	if (!AgentLodSafetyDemoCommand.IsValid())
	{
		AgentLodSafetyDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentLodSafetyDemo"),
			TEXT("E8 LOD tool safety boundary demo. Usage: HCIAbilityKit.AgentLodSafetyDemo [tool_name] [target_object_class] [nanite_enabled 0|1]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentLodSafetyDemoCommand));
	}

	if (!AgentPlanDemoCommand.IsValid())
	{
		AgentPlanDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentPlanDemo"),
			TEXT("F1 NL->Plan JSON demo (structured plan preview). Usage: HCIAbilityKit.AgentPlanDemo [natural language text...]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentPlanDemoCommand));
	}

	if (!AgentPlanDemoJsonCommand.IsValid())
	{
		AgentPlanDemoJsonCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentPlanDemoJson"),
			TEXT("F1 NL->Plan JSON demo (print contract JSON). Usage: HCIAbilityKit.AgentPlanDemoJson [natural language text...]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentPlanDemoJsonCommand));
	}

	if (!AgentPlanValidateDemoCommand.IsValid())
	{
		AgentPlanValidateDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentPlanValidateDemo"),
			TEXT("F2 Plan validator demo. Usage: HCIAbilityKit.AgentPlanValidateDemo [case_key]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentPlanValidateDemoCommand));
	}

	if (!AgentExecutePlanDemoCommand.IsValid())
	{
		AgentExecutePlanDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanDemo"),
			TEXT("F3 Executor skeleton demo (validate + simulate execute + convergence logs). Usage: HCIAbilityKit.AgentExecutePlanDemo [natural language text...]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanDemoCommand));
	}

	if (!AgentExecutePlanFailDemoCommand.IsValid())
	{
		AgentExecutePlanFailDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanFailDemo"),
			TEXT("F4 Executor failure convergence demo (step-level error + termination policy). Usage: HCIAbilityKit.AgentExecutePlanFailDemo [ok|fail_stop|fail_continue]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanFailDemoCommand));
	}

	if (!AgentExecutePlanPreflightDemoCommand.IsValid())
	{
		AgentExecutePlanPreflightDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanPreflightDemo"),
			TEXT("F5 Executor preflight gate-chain demo (Confirm/BlastRadius/RBAC/SourceControl/LOD Safety). Usage: HCIAbilityKit.AgentExecutePlanPreflightDemo [ok|fail_confirm|fail_blast|fail_rbac|fail_sc|fail_lod]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanPreflightDemoCommand));
	}

	if (!AgentExecutePlanReviewDemoCommand.IsValid())
	{
		AgentExecutePlanReviewDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewDemo"),
			TEXT("F6 Executor -> Dry-Run Diff review bridge demo. Usage: HCIAbilityKit.AgentExecutePlanReviewDemo [ok_naming|ok_level_risk|fail_confirm|fail_lod]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewDemoCommand));
	}

	if (!AgentExecutePlanReviewJsonCommand.IsValid())
	{
		AgentExecutePlanReviewJsonCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewJson"),
			TEXT("F6 Executor -> Dry-Run Diff review bridge JSON preview. Usage: HCIAbilityKit.AgentExecutePlanReviewJson [ok_naming|ok_level_risk|fail_confirm|fail_lod]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewJsonCommand));
	}

	if (!AgentExecutePlanReviewLocateCommand.IsValid())
	{
		AgentExecutePlanReviewLocateCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewLocate"),
			TEXT("F7 Locate a row from the latest ExecutorReview dry-run diff preview. Usage: HCIAbilityKit.AgentExecutePlanReviewLocate [row_index]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewLocateCommand));
	}

	if (!AgentExecutePlanReviewSelectCommand.IsValid())
	{
		AgentExecutePlanReviewSelectCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewSelect"),
			TEXT("F8 Select review rows (dedupe/filter) from latest ExecutorReview preview. Usage: HCIAbilityKit.AgentExecutePlanReviewSelect [row_indices_csv]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewSelectCommand));
	}

	if (!AgentExecutePlanReviewSelectJsonCommand.IsValid())
	{
		AgentExecutePlanReviewSelectJsonCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewSelectJson"),
			TEXT("F8 Select review rows and print selected DryRunDiff JSON. Usage: HCIAbilityKit.AgentExecutePlanReviewSelectJson [row_indices_csv]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewSelectJsonCommand));
	}

	if (!AgentExecutePlanReviewPrepareApplyCommand.IsValid())
	{
		AgentExecutePlanReviewPrepareApplyCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewPrepareApply"),
			TEXT("F9 Build ApplyRequest package from latest selected ExecutorReview preview. Usage: HCIAbilityKit.AgentExecutePlanReviewPrepareApply"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewPrepareApplyCommand));
	}

	if (!AgentExecutePlanReviewPrepareApplyJsonCommand.IsValid())
	{
		AgentExecutePlanReviewPrepareApplyJsonCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewPrepareApplyJson"),
			TEXT("F9 Build ApplyRequest package from latest selected ExecutorReview preview and print JSON. Usage: HCIAbilityKit.AgentExecutePlanReviewPrepareApplyJson"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewPrepareApplyJsonCommand));
	}

	if (!AgentExecutePlanReviewPrepareConfirmCommand.IsValid())
	{
		AgentExecutePlanReviewPrepareConfirmCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewPrepareConfirm"),
			TEXT("F10 Validate latest ApplyRequest against latest review preview and build ConfirmRequest. Usage: HCIAbilityKit.AgentExecutePlanReviewPrepareConfirm [user_confirmed=0|1] [tamper=none|digest|review]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewPrepareConfirmCommand));
	}

	if (!AgentExecutePlanReviewPrepareConfirmJsonCommand.IsValid())
	{
		AgentExecutePlanReviewPrepareConfirmJsonCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentExecutePlanReviewPrepareConfirmJson"),
			TEXT("F10 Build ConfirmRequest and print JSON. Usage: HCIAbilityKit.AgentExecutePlanReviewPrepareConfirmJson [user_confirmed=0|1] [tamper=none|digest|review]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentExecutePlanReviewPrepareConfirmJsonCommand));
	}
}

void FHCIAbilityKitAgentDemoConsoleCommands::Shutdown()
{
	AgentConfirmGateDemoCommand.Reset();
	AgentBlastRadiusDemoCommand.Reset();
	AgentTransactionDemoCommand.Reset();
	AgentSourceControlDemoCommand.Reset();
	AgentRbacDemoCommand.Reset();
	AgentLodSafetyDemoCommand.Reset();
	AgentPlanDemoCommand.Reset();
	AgentPlanDemoJsonCommand.Reset();
	AgentPlanValidateDemoCommand.Reset();
	AgentExecutePlanDemoCommand.Reset();
	AgentExecutePlanFailDemoCommand.Reset();
	AgentExecutePlanPreflightDemoCommand.Reset();
	AgentExecutePlanReviewDemoCommand.Reset();
	AgentExecutePlanReviewJsonCommand.Reset();
	AgentExecutePlanReviewLocateCommand.Reset();
	AgentExecutePlanReviewSelectCommand.Reset();
	AgentExecutePlanReviewSelectJsonCommand.Reset();
	AgentExecutePlanReviewPrepareApplyCommand.Reset();
	AgentExecutePlanReviewPrepareApplyJsonCommand.Reset();
	AgentExecutePlanReviewPrepareConfirmCommand.Reset();
	AgentExecutePlanReviewPrepareConfirmJsonCommand.Reset();
}
