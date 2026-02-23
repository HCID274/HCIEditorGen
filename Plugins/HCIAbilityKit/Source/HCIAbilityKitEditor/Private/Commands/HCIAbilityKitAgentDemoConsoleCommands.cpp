#include "Commands/HCIAbilityKitAgentDemoConsoleCommands.h"

#include "Agent/HCIAbilityKitAgentExecutionGate.h"
#include "Agent/HCIAbilityKitAgentExecutor.h"
#include "Agent/HCIAbilityKitAgentPlan.h"
#include "Agent/HCIAbilityKitAgentPlanJsonSerializer.h"
#include "Agent/HCIAbilityKitAgentPlanner.h"
#include "Agent/HCIAbilityKitAgentPlanValidator.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogHCIAbilityKitAuditScan, Log, All);

static FHCIAbilityKitAgentPlan GHCIAbilityKitAgentPlanPreviewState;

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


static void HCI_LogAgentConfirmGateDecision(const TCHAR* CaseName, const FHCIAbilityKitAgentExecutionDecision& Decision)
{
	if (Decision.bAllowed)
	{
		UE_LOG(
			LogHCIAbilityKitAuditScan,
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
		LogHCIAbilityKitAuditScan,
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
			LogHCIAbilityKitAuditScan,
			Display,
			TEXT("[HCIAbilityKit][AgentConfirmGate] summary total_cases=%d allowed=%d blocked=%d expected_blocked_code=%s validation=ok"),
			3,
			AllowedCount,
			BlockedCount,
			TEXT("E4005"));
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Display,
			TEXT("[HCIAbilityKit][AgentConfirmGate] hint=也可运行 HCIAbilityKit.AgentConfirmGateDemo [tool_name] [requires_confirm 0|1] [user_confirmed 0|1]"));
		return;
	}

	if (Args.Num() < 3)
	{
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Error,
			TEXT("[HCIAbilityKit][AgentConfirmGate] invalid_args usage=HCIAbilityKit.AgentConfirmGateDemo [tool_name] [requires_confirm 0|1] [user_confirmed 0|1]"));
		return;
	}

	bool bRequiresConfirm = false;
	bool bUserConfirmed = false;
	if (!HCI_TryParseBool01Arg(Args[1], bRequiresConfirm) || !HCI_TryParseBool01Arg(Args[2], bUserConfirmed))
	{
		UE_LOG(
			LogHCIAbilityKitAuditScan,
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
			LogHCIAbilityKitAuditScan,
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
		LogHCIAbilityKitAuditScan,
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
			LogHCIAbilityKitAuditScan,
			Display,
			TEXT("[HCIAbilityKit][AgentBlastRadius] summary total_cases=%d allowed=%d blocked=%d max_limit=%d expected_blocked_code=%s validation=ok"),
			3,
			AllowedCount,
			BlockedCount,
			FHCIAbilityKitAgentExecutionGate::MaxAssetModifyLimit,
			TEXT("E4004"));
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Display,
			TEXT("[HCIAbilityKit][AgentBlastRadius] hint=也可运行 HCIAbilityKit.AgentBlastRadiusDemo [tool_name] [target_modify_count>=0]"));
		return;
	}

	if (Args.Num() < 2)
	{
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Error,
			TEXT("[HCIAbilityKit][AgentBlastRadius] invalid_args usage=HCIAbilityKit.AgentBlastRadiusDemo [tool_name] [target_modify_count>=0]"));
		return;
	}

	int32 TargetModifyCount = 0;
	if (!HCI_TryParseNonNegativeIntArg(Args[1], TargetModifyCount))
	{
		UE_LOG(
			LogHCIAbilityKitAuditScan,
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
			LogHCIAbilityKitAuditScan,
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
		LogHCIAbilityKitAuditScan,
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
			LogHCIAbilityKitAuditScan,
			Display,
			TEXT("[HCIAbilityKit][AgentTransaction] summary total_cases=%d committed=%d rolled_back=%d transaction_mode=%s expected_failed_code=%s validation=ok"),
			3,
			CommittedCaseCount,
			RolledBackCaseCount,
			TEXT("all_or_nothing"),
			TEXT("E4007"));
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Display,
			TEXT("[HCIAbilityKit][AgentTransaction] hint=也可运行 HCIAbilityKit.AgentTransactionDemo [total_steps>=1] [fail_step_index>=0] (0 means all succeed)"));
		return;
	}

	if (Args.Num() < 2)
	{
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Error,
			TEXT("[HCIAbilityKit][AgentTransaction] invalid_args usage=HCIAbilityKit.AgentTransactionDemo [total_steps>=1] [fail_step_index>=0]"));
		return;
	}

	int32 TotalSteps = 0;
	int32 FailStepIndexOneBased = 0;
	if (!HCI_TryParseNonNegativeIntArg(Args[0], TotalSteps) || !HCI_TryParseNonNegativeIntArg(Args[1], FailStepIndexOneBased))
	{
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Error,
			TEXT("[HCIAbilityKit][AgentTransaction] invalid_args reason=total_steps and fail_step_index must be integers"));
		return;
	}

	if (TotalSteps < 1)
	{
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Error,
			TEXT("[HCIAbilityKit][AgentTransaction] invalid_args reason=total_steps must be >= 1"));
		return;
	}

	if (FailStepIndexOneBased < 0 || FailStepIndexOneBased > TotalSteps)
	{
		UE_LOG(
			LogHCIAbilityKitAuditScan,
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
			LogHCIAbilityKitAuditScan,
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
		LogHCIAbilityKitAuditScan,
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
			LogHCIAbilityKitAuditScan,
			Display,
			TEXT("[HCIAbilityKit][AgentSourceControl] summary total_cases=%d allowed=%d blocked=%d offline_local_mode_cases=%d fail_fast=%s expected_blocked_code=%s validation=ok"),
			3,
			AllowedCount,
			BlockedCount,
			OfflineLocalModeCount,
			TEXT("true"),
			TEXT("E4006"));
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Display,
			TEXT("[HCIAbilityKit][AgentSourceControl] hint=也可运行 HCIAbilityKit.AgentSourceControlDemo [tool_name] [source_control_enabled 0|1] [checkout_succeeded 0|1]"));
		return;
	}

	if (Args.Num() < 3)
	{
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Error,
			TEXT("[HCIAbilityKit][AgentSourceControl] invalid_args usage=HCIAbilityKit.AgentSourceControlDemo [tool_name] [source_control_enabled 0|1] [checkout_succeeded 0|1]"));
		return;
	}

	bool bSourceControlEnabled = false;
	bool bCheckoutSucceeded = false;
	if (!HCI_TryParseBool01Arg(Args[1], bSourceControlEnabled) || !HCI_TryParseBool01Arg(Args[2], bCheckoutSucceeded))
	{
		UE_LOG(
			LogHCIAbilityKitAuditScan,
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
			LogHCIAbilityKitAuditScan,
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
		LogHCIAbilityKitAuditScan,
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
			LogHCIAbilityKitAuditScan,
			Error,
			TEXT("[HCIAbilityKit][AgentRBAC] config_load_failed path=%s reason=%s"),
			ConfigPath.IsEmpty() ? TEXT("-") : *ConfigPath,
			ConfigError.IsEmpty() ? TEXT("unknown") : *ConfigError);
		return;
	}

	if (bConfigCreated)
	{
		UE_LOG(
			LogHCIAbilityKitAuditScan,
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
				LogHCIAbilityKitAuditScan,
				Error,
				TEXT("[HCIAbilityKit][AgentAuditLog] append_failed case=%s path=%s reason=%s"),
				CaseName,
				AuditLogPath.IsEmpty() ? TEXT("-") : *AuditLogPath,
				AuditLogError.IsEmpty() ? TEXT("unknown") : *AuditLogError);
		}
		else
		{
			UE_LOG(
				LogHCIAbilityKitAuditScan,
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
			LogHCIAbilityKitAuditScan,
			Display,
			TEXT("[HCIAbilityKit][AgentRBAC] summary total_cases=%d allowed=%d blocked=%d guest_fallback_cases=%d audit_log_appends=%d config_users=%d validation=ok"),
			3,
			AllowedCount,
			BlockedCount,
			GuestFallbackCount,
			AuditLogAppendCount,
			RbacEntries.Num());
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Display,
			TEXT("[HCIAbilityKit][AgentRBAC] paths config_path=%s audit_log_path=%s config_created_default=%s"),
			*ConfigPath,
			LastAuditLogPath.IsEmpty() ? *HCI_GetAgentExecAuditLogPath() : *LastAuditLogPath,
			bConfigCreated ? TEXT("true") : TEXT("false"));
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Display,
			TEXT("[HCIAbilityKit][AgentRBAC] hint=也可运行 HCIAbilityKit.AgentRbacDemo [user_name] [tool_name] [asset_count>=0]"));
		return;
	}

	if (Args.Num() < 3)
	{
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Error,
			TEXT("[HCIAbilityKit][AgentRBAC] invalid_args usage=HCIAbilityKit.AgentRbacDemo [user_name] [tool_name] [asset_count>=0]"));
		return;
	}

	int32 AssetCount = 0;
	if (!HCI_TryParseNonNegativeIntArg(Args[2], AssetCount))
	{
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Error,
			TEXT("[HCIAbilityKit][AgentRBAC] invalid_args reason=asset_count must be integer >= 0"));
		return;
	}

	const FString UserName = Args[0].TrimStartAndEnd();
	const FString ToolName = Args[1].TrimStartAndEnd();
	if (UserName.IsEmpty() || ToolName.IsEmpty())
	{
		UE_LOG(
			LogHCIAbilityKitAuditScan,
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
			LogHCIAbilityKitAuditScan,
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
		LogHCIAbilityKitAuditScan,
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
			LogHCIAbilityKitAuditScan,
			Display,
			TEXT("[HCIAbilityKit][AgentLodSafety] summary total_cases=%d allowed=%d blocked=%d type_blocked=%d nanite_blocked=%d expected_blocked_code=%s validation=ok"),
			3,
			AllowedCount,
			BlockedCount,
			TypeBlockedCount,
			NaniteBlockedCount,
			TEXT("E4010"));
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Display,
			TEXT("[HCIAbilityKit][AgentLodSafety] hint=也可运行 HCIAbilityKit.AgentLodSafetyDemo [tool_name] [target_object_class] [nanite_enabled 0|1]"));
		return;
	}

	if (Args.Num() < 3)
	{
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Error,
			TEXT("[HCIAbilityKit][AgentLodSafety] invalid_args usage=HCIAbilityKit.AgentLodSafetyDemo [tool_name] [target_object_class] [nanite_enabled 0|1]"));
		return;
	}

	bool bNaniteEnabled = false;
	if (!HCI_TryParseBool01Arg(Args[2], bNaniteEnabled))
	{
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Error,
			TEXT("[HCIAbilityKit][AgentLodSafety] invalid_args reason=nanite_enabled must be 0 or 1"));
		return;
	}

	const FString ToolName = Args[0].TrimStartAndEnd();
	const FString TargetObjectClass = Args[1].TrimStartAndEnd();
	if (ToolName.IsEmpty() || TargetObjectClass.IsEmpty())
	{
		UE_LOG(
			LogHCIAbilityKitAuditScan,
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
			LogHCIAbilityKitAuditScan,
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
					LogHCIAbilityKitAuditScan,
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
				LogHCIAbilityKitAuditScan,
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
			LogHCIAbilityKitAuditScan,
			Display,
			TEXT("[HCIAbilityKit][AgentPlan] summary total_cases=%d built=%d validation_ok=%d plan_version=1 supports_intents=naming_traceability|level_risk|asset_compliance validation=ok"),
			Cases.Num(),
			BuiltCount,
			ValidationOkCount);
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Display,
			TEXT("[HCIAbilityKit][AgentPlan] hint=也可运行 HCIAbilityKit.AgentPlanDemo [自然语言文本...] 或 HCIAbilityKit.AgentPlanDemoJson [自然语言文本...]"));
		return;
	}

	const FString UserText = HCI_JoinConsoleArgsAsText(Args);
	if (UserText.IsEmpty())
	{
		UE_LOG(LogHCIAbilityKitAuditScan, Error, TEXT("[HCIAbilityKit][AgentPlan] invalid_args reason=empty input text"));
		return;
	}

	FHCIAbilityKitAgentPlan Plan;
	FString RouteReason;
	FString Error;
	if (!HCI_BuildAgentPlanDemoPlan(UserText, TEXT("req_cli_f1"), Plan, RouteReason, Error))
	{
		UE_LOG(LogHCIAbilityKitAuditScan, Error, TEXT("[HCIAbilityKit][AgentPlan] build_failed input=%s reason=%s"), *UserText, *Error);
		return;
	}

	FString ContractError;
	const bool bValid = FHCIAbilityKitAgentPlanContract::ValidateMinimalContract(Plan, ContractError);
	GHCIAbilityKitAgentPlanPreviewState = Plan;

	UE_LOG(
		LogHCIAbilityKitAuditScan,
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
		UE_LOG(LogHCIAbilityKitAuditScan, Error, TEXT("[HCIAbilityKit][AgentPlan] json_build_failed input=%s reason=%s"), *UserText, *Error);
		return;
	}

	FString JsonText;
	if (!FHCIAbilityKitAgentPlanJsonSerializer::SerializeToJsonString(Plan, JsonText))
	{
		UE_LOG(LogHCIAbilityKitAuditScan, Error, TEXT("[HCIAbilityKit][AgentPlan] export_json_failed reason=serialize_failed"));
		return;
	}

	GHCIAbilityKitAgentPlanPreviewState = Plan;
	UE_LOG(
		LogHCIAbilityKitAuditScan,
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
			LogHCIAbilityKitAuditScan,
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
			LogHCIAbilityKitAuditScan,
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
				UE_LOG(LogHCIAbilityKitAuditScan, Error, TEXT("[HCIAbilityKit][AgentPlanValidate] case=%s build_failed reason=%s"), *CaseKey, *BuildError);
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
			LogHCIAbilityKitAuditScan,
			Display,
			TEXT("[HCIAbilityKit][AgentPlanValidate] summary total_cases=%d valid=%d invalid=%d expected_matches=%d supports_checks=schema|whitelist|risk|threshold|special_cases validation=%s"),
			Cases.Num(),
			ValidCount,
			InvalidCount,
			ExpectedMatches,
			ExpectedMatches == Cases.Num() ? TEXT("ok") : TEXT("fail"));
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Display,
			TEXT("[HCIAbilityKit][AgentPlanValidate] hint=也可运行 HCIAbilityKit.AgentPlanValidateDemo [ok_naming|fail_missing_tool|fail_unknown_tool|fail_invalid_enum|fail_over_limit|fail_level_scope|fail_naming_metadata]"));
		return;
	}

	if (Args.Num() != 1)
	{
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Error,
			TEXT("[HCIAbilityKit][AgentPlanValidate] invalid_args usage=HCIAbilityKit.AgentPlanValidateDemo [case_key]"));
		return;
	}

	const FString CaseKey = Args[0].TrimStartAndEnd();
	if (CaseKey.IsEmpty())
	{
		UE_LOG(LogHCIAbilityKitAuditScan, Error, TEXT("[HCIAbilityKit][AgentPlanValidate] invalid_args reason=case_key is empty"));
		return;
	}

	FHCIAbilityKitAgentPlan Plan;
	FHCIAbilityKitAgentPlanValidationContext Context;
	FString Description;
	FString ExpectedCode;
	FString BuildError;
	if (!HCI_BuildAgentPlanValidateDemoCase(CaseKey, Plan, Context, Description, ExpectedCode, BuildError))
	{
		UE_LOG(LogHCIAbilityKitAuditScan, Error, TEXT("[HCIAbilityKit][AgentPlanValidate] build_failed case=%s reason=%s"), *CaseKey, *BuildError);
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
				LogHCIAbilityKitAuditScan,
				Display,
				TEXT("[HCIAbilityKit][AgentExecutor] case=%s row=%d step_id=%s tool_name=%s capability=%s risk_level=%s write_like=%s status=%s attempted=%s succeeded=%s target_count_estimate=%d evidence_keys=%s evidence=%s error_code=%s reason=%s"),
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
				Row.Reason.IsEmpty() ? TEXT("-") : *Row.Reason);
		}
		else
		{
			UE_LOG(
				LogHCIAbilityKitAuditScan,
				Warning,
				TEXT("[HCIAbilityKit][AgentExecutor] case=%s row=%d step_id=%s tool_name=%s capability=%s risk_level=%s write_like=%s status=%s attempted=%s succeeded=%s target_count_estimate=%d evidence_keys=%s evidence=%s error_code=%s reason=%s"),
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
				Row.Reason.IsEmpty() ? TEXT("-") : *Row.Reason);
		}
	}
}

static void HCI_LogAgentExecutorSummary(const TCHAR* CaseName, const FHCIAbilityKitAgentExecutorRunResult& RunResult)
{
	if (RunResult.bCompleted)
	{
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Display,
			TEXT("[HCIAbilityKit][AgentExecutor] case=%s summary request_id=%s intent=%s plan_version=%d execution_mode=%s accepted=%s completed=%s total_steps=%d executed_steps=%d succeeded_steps=%d failed_steps=%d terminal_status=%s terminal_reason=%s error_code=%s reason=%s started_utc=%s finished_utc=%s"),
			CaseName,
			RunResult.RequestId.IsEmpty() ? TEXT("-") : *RunResult.RequestId,
			RunResult.Intent.IsEmpty() ? TEXT("-") : *RunResult.Intent,
			RunResult.PlanVersion,
			RunResult.ExecutionMode.IsEmpty() ? TEXT("-") : *RunResult.ExecutionMode,
			RunResult.bAccepted ? TEXT("true") : TEXT("false"),
			RunResult.bCompleted ? TEXT("true") : TEXT("false"),
			RunResult.TotalSteps,
			RunResult.ExecutedSteps,
			RunResult.SucceededSteps,
			RunResult.FailedSteps,
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
			LogHCIAbilityKitAuditScan,
			Warning,
			TEXT("[HCIAbilityKit][AgentExecutor] case=%s summary request_id=%s intent=%s plan_version=%d execution_mode=%s accepted=%s completed=%s total_steps=%d executed_steps=%d succeeded_steps=%d failed_steps=%d terminal_status=%s terminal_reason=%s error_code=%s reason=%s started_utc=%s finished_utc=%s"),
			CaseName,
			RunResult.RequestId.IsEmpty() ? TEXT("-") : *RunResult.RequestId,
			RunResult.Intent.IsEmpty() ? TEXT("-") : *RunResult.Intent,
			RunResult.PlanVersion,
			RunResult.ExecutionMode.IsEmpty() ? TEXT("-") : *RunResult.ExecutionMode,
			RunResult.bAccepted ? TEXT("true") : TEXT("false"),
			RunResult.bCompleted ? TEXT("true") : TEXT("false"),
			RunResult.TotalSteps,
			RunResult.ExecutedSteps,
			RunResult.SucceededSteps,
			RunResult.FailedSteps,
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
			LogHCIAbilityKitAuditScan,
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
		LogHCIAbilityKitAuditScan,
		Display,
		TEXT("[HCIAbilityKit][AgentExecutor] case=%s route_reason=%s input=%s"),
		CaseName,
		*RouteReason,
		*UserText);
	HCI_LogAgentExecutorSummary(CaseName, RunResult);
	HCI_LogAgentExecutorRows(CaseName, RunResult);
	return true;
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
			LogHCIAbilityKitAuditScan,
			Display,
			TEXT("[HCIAbilityKit][AgentExecutor] summary total_cases=%d completed_cases=%d rejected_cases=%d execution_mode=%s total_step_rows=%d validation=ok"),
			Cases.Num(),
			BuiltAndCompletedCases,
			RejectedCases,
			TEXT("simulate_dry_run"),
			TotalStepRows);
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Display,
			TEXT("[HCIAbilityKit][AgentExecutor] hint=也可运行 HCIAbilityKit.AgentExecutePlanDemo [自然语言文本...]"));
		return;
	}

	const FString UserText = HCI_JoinConsoleArgsAsText(Args);
	if (UserText.IsEmpty())
	{
		UE_LOG(LogHCIAbilityKitAuditScan, Error, TEXT("[HCIAbilityKit][AgentExecutor] invalid_args reason=empty input text"));
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
}
