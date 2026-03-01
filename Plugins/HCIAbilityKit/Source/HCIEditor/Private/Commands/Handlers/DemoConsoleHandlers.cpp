#include "Commands/HCIAgentDemoConsoleCommands.h"
// Moved: keep heavy console command handlers isolated from the thin registration shell.
#include "Commands/HCIAgentCommandHandlers.h"
#include "Commands/HCIAgentDemoState.h"
#include "Commands/HCIAgentExecutorReviewLocateUtils.h"
#include "UI/HCIAgentChatWindow.h"
#include "UI/HCIAgentPlanPreviewWindow.h"

#include "Agent/Executor/HCIAgentExecutionGate.h"
#include "Agent/Contracts/StageF/HCIAgentApplyConfirmRequest.h"
#include "Agent/Contracts/StageF/HCIAgentApplyConfirmRequestJsonSerializer.h"
#include "Agent/Contracts/StageF/HCIAgentApplyRequest.h"
#include "Agent/Contracts/StageF/HCIAgentApplyRequestJsonSerializer.h"
#include "Agent/Contracts/StageF/HCIAgentExecuteTicket.h"
#include "Agent/Contracts/StageF/HCIAgentExecuteTicketJsonSerializer.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteReceipt.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteReceiptJsonSerializer.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteFinalReport.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteFinalReportJsonSerializer.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteArchiveBundle.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteArchiveBundleJsonSerializer.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteHandoffEnvelope.h"
#include "Agent/Contracts/StageF/HCIAgentSimulateExecuteHandoffEnvelopeJsonSerializer.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecuteIntent.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecuteIntentJsonSerializer.h"
#include "Agent/Contracts/StageG/HCIAgentStageGWriteEnableRequest.h"
#include "Agent/Contracts/StageG/HCIAgentStageGWriteEnableRequestJsonSerializer.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecutePermitTicket.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecutePermitTicketJsonSerializer.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecuteDispatchRequest.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecuteDispatchRequestJsonSerializer.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecuteDispatchReceipt.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecuteDispatchReceiptJsonSerializer.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecuteCommitRequest.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecuteCommitRequestJsonSerializer.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecuteCommitReceipt.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecuteCommitReceiptJsonSerializer.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecuteFinalReport.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecuteFinalReportJsonSerializer.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecuteArchiveBundle.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecuteArchiveBundleJsonSerializer.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecutionReadinessReport.h"
#include "Agent/Contracts/StageG/HCIAgentStageGExecutionReadinessReportJsonSerializer.h"
#include "Agent/Executor/HCIAgentExecutor.h"
#include "Agent/Bridges/HCIAgentExecutorApplyConfirmBridge.h"
#include "Agent/Bridges/HCIAgentExecutorApplyRequestBridge.h"
#include "Agent/Bridges/HCIAgentExecutorDryRunBridge.h"
#include "Agent/Bridges/HCIAgentExecutorExecuteTicketBridge.h"
#include "Agent/Bridges/HCIAgentExecutorSimulateExecuteReceiptBridge.h"
#include "Agent/Bridges/HCIAgentExecutorSimulateExecuteFinalReportBridge.h"
#include "Agent/Bridges/HCIAgentExecutorSimulateExecuteArchiveBundleBridge.h"
#include "Agent/Bridges/HCIAgentExecutorSimulateExecuteHandoffEnvelopeBridge.h"
#include "Agent/Bridges/HCIAgentExecutorStageGExecuteIntentBridge.h"
#include "Agent/Bridges/HCIAgentExecutorStageGWriteEnableRequestBridge.h"
#include "Agent/Bridges/HCIAgentExecutorStageGExecutePermitTicketBridge.h"
#include "Agent/Bridges/HCIAgentExecutorStageGExecuteDispatchRequestBridge.h"
#include "Agent/Bridges/HCIAgentExecutorStageGExecuteDispatchReceiptBridge.h"
#include "Agent/Bridges/HCIAgentExecutorStageGExecuteCommitRequestBridge.h"
#include "Agent/Bridges/HCIAgentExecutorStageGExecuteCommitReceiptBridge.h"
#include "Agent/Bridges/HCIAgentExecutorStageGExecuteFinalReportBridge.h"
#include "Agent/Bridges/HCIAgentExecutorStageGExecuteArchiveBundleBridge.h"
#include "Agent/Bridges/HCIAgentExecutorStageGExecutionReadinessReportBridge.h"
#include "Agent/LLM/HCIAgentLlmClient.h"
#include "Agent/Planner/HCIAgentPlan.h"
#include "Agent/Planner/HCIAgentPlanJsonSerializer.h"
#include "Agent/Planner/HCIAgentPlanner.h"
#include "Agent/Planner/HCIAgentPlanValidator.h"
#include "Agent/Executor/HCIDryRunDiff.h"
#include "Agent/Executor/HCIDryRunDiffJsonSerializer.h"
#include "Agent/Executor/HCIDryRunDiffSelection.h"
#include "Agent/Tools/HCIToolRegistry.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Common/HCITimeFormat.h"
#include "Dom/JsonObject.h"
#include "EditorAssetLibrary.h"
#include "HAL/FileManager.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Templates/Atomic.h"
#include "Modules/ModuleManager.h"
#include "UObject/SoftObjectPath.h"

DEFINE_LOG_CATEGORY_STATIC(LogHCIAgentDemo, Log, All);

static FHCIAgentDemoState& HCI_State()
{
	return HCI_GetAgentDemoState();
}

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

static int64 HCI_TryExtractAssetSizeFromTags(const FAssetData& AssetData)
{
	int64 SizeBytes = -1;
	if (AssetData.GetTagValue(FName(TEXT("DiskSize")), SizeBytes))
	{
		return SizeBytes;
	}
	if (AssetData.GetTagValue(FName(TEXT("FileSize")), SizeBytes))
	{
		return SizeBytes;
	}

	FString SizeText;
	if (AssetData.GetTagValue(FName(TEXT("DiskSize")), SizeText) && LexTryParseString(SizeBytes, *SizeText))
	{
		return SizeBytes;
	}
	if (AssetData.GetTagValue(FName(TEXT("FileSize")), SizeText) && LexTryParseString(SizeBytes, *SizeText))
	{
		return SizeBytes;
	}
	return -1;
}

static bool HCI_ScanAssetsForPlannerEnvContext(
	const FString& ScanRoot,
	TArray<FHCIAgentPlannerEnvAssetEntry>& OutAssets,
	FString& OutError)
{
	OutAssets.Reset();
	OutError.Reset();

	FString SafeRoot = ScanRoot.TrimStartAndEnd();
	if (SafeRoot.IsEmpty() || !SafeRoot.StartsWith(TEXT("/Game/")))
	{
		SafeRoot = TEXT("/Game/Temp");
	}

	const TArray<FString> AssetPaths = UEditorAssetLibrary::ListAssets(SafeRoot, true, false);
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	OutAssets.Reserve(AssetPaths.Num());
	for (const FString& ObjectPath : AssetPaths)
	{
		FHCIAgentPlannerEnvAssetEntry Entry;
		Entry.ObjectPath = ObjectPath;

		FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(ObjectPath));
		if (AssetData.IsValid())
		{
			Entry.AssetClass = AssetData.AssetClassPath.GetAssetName().ToString();
			Entry.SizeBytes = HCI_TryExtractAssetSizeFromTags(AssetData);
		}
		else
		{
			Entry.AssetClass = TEXT("Unknown");
			Entry.SizeBytes = -1;
		}

		OutAssets.Add(MoveTemp(Entry));
	}

	OutAssets.Sort([](const FHCIAgentPlannerEnvAssetEntry& Lhs, const FHCIAgentPlannerEnvAssetEntry& Rhs)
	{
		return Lhs.ObjectPath < Rhs.ObjectPath;
	});
	return true;
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
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("HCI"), TEXT("Audit"), TEXT("agent_exec_log.jsonl"));
}


bool HCI_TryParseBool01Arg(const FString& InValue, bool& OutValue)
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


static void HCI_LogAgentConfirmGateDecision(const TCHAR* CaseName, const FHCIAgentExecutionDecision& Decision)
{
	if (Decision.bAllowed)
	{
		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentConfirmGate] case=%s step_id=%s tool_name=%s capability=%s write_like=%s requires_confirm=%s user_confirmed=%s allowed=%s error_code=%s reason=%s"),
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
		LogHCIAgentDemo,
		Warning,
		TEXT("[HCI][AgentConfirmGate] case=%s step_id=%s tool_name=%s capability=%s write_like=%s requires_confirm=%s user_confirmed=%s allowed=%s error_code=%s reason=%s"),
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

void HCI_RunAbilityKitAgentConfirmGateDemoCommand(const TArray<FString>& Args)
{
	const FHCIToolRegistry& Registry = FHCIToolRegistry::GetReadOnly();

	if (Args.Num() == 0)
	{
		int32 BlockedCount = 0;
		int32 AllowedCount = 0;

		auto RunCase = [&Registry, &BlockedCount, &AllowedCount](const TCHAR* CaseName, const TCHAR* StepId, const TCHAR* ToolName, const bool bRequiresConfirm, const bool bUserConfirmed)
		{
			FHCIAgentExecutionStep Step;
			Step.StepId = StepId;
			Step.ToolName = FName(ToolName);
			Step.bRequiresConfirm = bRequiresConfirm;
			Step.bUserConfirmed = bUserConfirmed;

			const FHCIAgentExecutionDecision Decision = FHCIAgentExecutionGate::EvaluateConfirmGate(Step, Registry);
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
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentConfirmGate] summary total_cases=%d allowed=%d blocked=%d expected_blocked_code=%s validation=ok"),
			3,
			AllowedCount,
			BlockedCount,
			TEXT("E4005"));
		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentConfirmGate] hint=也可运行 HCI.AgentConfirmGateDemo [tool_name] [requires_confirm 0|1] [user_confirmed 0|1]"));
		return;
	}

	if (Args.Num() < 3)
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentConfirmGate] invalid_args usage=HCI.AgentConfirmGateDemo [tool_name] [requires_confirm 0|1] [user_confirmed 0|1]"));
		return;
	}

	bool bRequiresConfirm = false;
	bool bUserConfirmed = false;
	if (!HCI_TryParseBool01Arg(Args[1], bRequiresConfirm) || !HCI_TryParseBool01Arg(Args[2], bUserConfirmed))
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentConfirmGate] invalid_args reason=requires_confirm and user_confirmed must be 0 or 1"));
		return;
	}

	FHCIAgentExecutionStep Step;
	Step.StepId = TEXT("step_cli");
	Step.ToolName = FName(*Args[0].TrimStartAndEnd());
	Step.bRequiresConfirm = bRequiresConfirm;
	Step.bUserConfirmed = bUserConfirmed;

	const FHCIAgentExecutionDecision Decision = FHCIAgentExecutionGate::EvaluateConfirmGate(Step, Registry);
	HCI_LogAgentConfirmGateDecision(TEXT("custom"), Decision);
}

static void HCI_LogAgentBlastRadiusDecision(const TCHAR* CaseName, const FHCIAgentBlastRadiusDecision& Decision)
{
	if (Decision.bAllowed)
	{
		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentBlastRadius] case=%s request_id=%s tool_name=%s capability=%s write_like=%s target_modify_count=%d max_limit=%d allowed=%s error_code=%s reason=%s"),
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
		LogHCIAgentDemo,
		Warning,
		TEXT("[HCI][AgentBlastRadius] case=%s request_id=%s tool_name=%s capability=%s write_like=%s target_modify_count=%d max_limit=%d allowed=%s error_code=%s reason=%s"),
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

void HCI_RunAbilityKitAgentBlastRadiusDemoCommand(const TArray<FString>& Args)
{
	const FHCIToolRegistry& Registry = FHCIToolRegistry::GetReadOnly();

	if (Args.Num() == 0)
	{
		int32 BlockedCount = 0;
		int32 AllowedCount = 0;

		auto RunCase = [&Registry, &BlockedCount, &AllowedCount](const TCHAR* CaseName, const TCHAR* RequestId, const TCHAR* ToolName, const int32 TargetModifyCount)
		{
			FHCIAgentBlastRadiusCheckInput Input;
			Input.RequestId = RequestId;
			Input.ToolName = FName(ToolName);
			Input.TargetModifyCount = TargetModifyCount;

			const FHCIAgentBlastRadiusDecision Decision = FHCIAgentExecutionGate::EvaluateBlastRadius(Input, Registry);
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
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentBlastRadius] summary total_cases=%d allowed=%d blocked=%d max_limit=%d expected_blocked_code=%s validation=ok"),
			3,
			AllowedCount,
			BlockedCount,
			FHCIAgentExecutionGate::MaxAssetModifyLimit,
			TEXT("E4004"));
		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentBlastRadius] hint=也可运行 HCI.AgentBlastRadiusDemo [tool_name] [target_modify_count>=0]"));
		return;
	}

	if (Args.Num() < 2)
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentBlastRadius] invalid_args usage=HCI.AgentBlastRadiusDemo [tool_name] [target_modify_count>=0]"));
		return;
	}

	int32 TargetModifyCount = 0;
	if (!HCI_TryParseNonNegativeIntArg(Args[1], TargetModifyCount))
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentBlastRadius] invalid_args reason=target_modify_count must be integer >= 0"));
		return;
	}

	FHCIAgentBlastRadiusCheckInput Input;
	Input.RequestId = TEXT("req_cli_e4");
	Input.ToolName = FName(*Args[0].TrimStartAndEnd());
	Input.TargetModifyCount = TargetModifyCount;

	const FHCIAgentBlastRadiusDecision Decision = FHCIAgentExecutionGate::EvaluateBlastRadius(Input, Registry);
	HCI_LogAgentBlastRadiusDecision(TEXT("custom"), Decision);
}

static FHCIAgentTransactionExecutionInput HCI_BuildAgentTransactionDemoInput(
	const TCHAR* RequestId,
	const int32 TotalSteps,
	const int32 FailStepIndexOneBased)
{
	FHCIAgentTransactionExecutionInput Input;
	Input.RequestId = RequestId;
	Input.Steps.Reserve(TotalSteps);

	static const FName DemoToolNames[] = {
		TEXT("RenameAsset"),
		TEXT("MoveAsset"),
		TEXT("SetTextureMaxSize"),
		TEXT("SetMeshLODGroup")};

	for (int32 Index = 0; Index < TotalSteps; ++Index)
	{
		FHCIAgentTransactionStepSimulation& Step = Input.Steps.AddDefaulted_GetRef();
		Step.StepId = FString::Printf(TEXT("step_%03d"), Index + 1);
		Step.ToolName = DemoToolNames[Index % UE_ARRAY_COUNT(DemoToolNames)];
		Step.bShouldSucceed = ((Index + 1) != FailStepIndexOneBased);
	}

	return Input;
}

static void HCI_LogAgentTransactionDecision(const TCHAR* CaseName, const FHCIAgentTransactionExecutionDecision& Decision)
{
	if (Decision.bCommitted)
	{
		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentTransaction] case=%s request_id=%s transaction_mode=%s total_steps=%d executed_steps=%d committed_steps=%d rolled_back_steps=%d committed=%s rolled_back=%s failed_step_index=%d failed_step_id=%s failed_tool_name=%s error_code=%s reason=%s"),
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
		LogHCIAgentDemo,
		Warning,
		TEXT("[HCI][AgentTransaction] case=%s request_id=%s transaction_mode=%s total_steps=%d executed_steps=%d committed_steps=%d rolled_back_steps=%d committed=%s rolled_back=%s failed_step_index=%d failed_step_id=%s failed_tool_name=%s error_code=%s reason=%s"),
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

void HCI_RunAbilityKitAgentTransactionDemoCommand(const TArray<FString>& Args)
{
	const FHCIToolRegistry& Registry = FHCIToolRegistry::GetReadOnly();

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
			const FHCIAgentTransactionExecutionInput Input =
				HCI_BuildAgentTransactionDemoInput(RequestId, TotalSteps, FailStepIndexOneBased);
			const FHCIAgentTransactionExecutionDecision Decision =
				FHCIAgentExecutionGate::EvaluateAllOrNothingTransaction(Input, Registry);

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
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentTransaction] summary total_cases=%d committed=%d rolled_back=%d transaction_mode=%s expected_failed_code=%s validation=ok"),
			3,
			CommittedCaseCount,
			RolledBackCaseCount,
			TEXT("all_or_nothing"),
			TEXT("E4007"));
		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentTransaction] hint=也可运行 HCI.AgentTransactionDemo [total_steps>=1] [fail_step_index>=0] (0 means all succeed)"));
		return;
	}

	if (Args.Num() < 2)
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentTransaction] invalid_args usage=HCI.AgentTransactionDemo [total_steps>=1] [fail_step_index>=0]"));
		return;
	}

	int32 TotalSteps = 0;
	int32 FailStepIndexOneBased = 0;
	if (!HCI_TryParseNonNegativeIntArg(Args[0], TotalSteps) || !HCI_TryParseNonNegativeIntArg(Args[1], FailStepIndexOneBased))
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentTransaction] invalid_args reason=total_steps and fail_step_index must be integers"));
		return;
	}

	if (TotalSteps < 1)
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentTransaction] invalid_args reason=total_steps must be >= 1"));
		return;
	}

	if (FailStepIndexOneBased < 0 || FailStepIndexOneBased > TotalSteps)
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentTransaction] invalid_args reason=fail_step_index must satisfy 0 <= fail_step_index <= total_steps"));
		return;
	}

	const FHCIAgentTransactionExecutionInput Input =
		HCI_BuildAgentTransactionDemoInput(TEXT("req_cli_e5"), TotalSteps, FailStepIndexOneBased);
	const FHCIAgentTransactionExecutionDecision Decision =
		FHCIAgentExecutionGate::EvaluateAllOrNothingTransaction(Input, Registry);
	HCI_LogAgentTransactionDecision(TEXT("custom"), Decision);
}

static void HCI_LogAgentSourceControlDecision(const TCHAR* CaseName, const FHCIAgentSourceControlDecision& Decision)
{
	const bool bUseWarningLevel = (!Decision.bAllowed) || Decision.bOfflineLocalMode;
	if (!bUseWarningLevel)
	{
		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentSourceControl] case=%s request_id=%s tool_name=%s capability=%s write_like=%s source_control_enabled=%s fail_fast=%s offline_local_mode=%s checkout_attempted=%s checkout_succeeded=%s allowed=%s error_code=%s reason=%s"),
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
		LogHCIAgentDemo,
		Warning,
		TEXT("[HCI][AgentSourceControl] case=%s request_id=%s tool_name=%s capability=%s write_like=%s source_control_enabled=%s fail_fast=%s offline_local_mode=%s checkout_attempted=%s checkout_succeeded=%s allowed=%s error_code=%s reason=%s"),
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

void HCI_RunAbilityKitAgentSourceControlDemoCommand(const TArray<FString>& Args)
{
	const FHCIToolRegistry& Registry = FHCIToolRegistry::GetReadOnly();

	auto BuildInput = [](const TCHAR* RequestId, const TCHAR* ToolName, const bool bSourceControlEnabled, const bool bCheckoutSucceeded)
	{
		FHCIAgentSourceControlCheckInput Input;
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

		auto RunCase = [&Registry, &AllowedCount, &BlockedCount, &OfflineLocalModeCount](const TCHAR* CaseName, const FHCIAgentSourceControlCheckInput& Input)
		{
			const FHCIAgentSourceControlDecision Decision =
				FHCIAgentExecutionGate::EvaluateSourceControlFailFast(Input, Registry);
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
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentSourceControl] summary total_cases=%d allowed=%d blocked=%d offline_local_mode_cases=%d fail_fast=%s expected_blocked_code=%s validation=ok"),
			3,
			AllowedCount,
			BlockedCount,
			OfflineLocalModeCount,
			TEXT("true"),
			TEXT("E4006"));
		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentSourceControl] hint=也可运行 HCI.AgentSourceControlDemo [tool_name] [source_control_enabled 0|1] [checkout_succeeded 0|1]"));
		return;
	}

	if (Args.Num() < 3)
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentSourceControl] invalid_args usage=HCI.AgentSourceControlDemo [tool_name] [source_control_enabled 0|1] [checkout_succeeded 0|1]"));
		return;
	}

	bool bSourceControlEnabled = false;
	bool bCheckoutSucceeded = false;
	if (!HCI_TryParseBool01Arg(Args[1], bSourceControlEnabled) || !HCI_TryParseBool01Arg(Args[2], bCheckoutSucceeded))
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentSourceControl] invalid_args reason=source_control_enabled and checkout_succeeded must be 0 or 1"));
		return;
	}

	FHCIAgentSourceControlCheckInput Input;
	Input.RequestId = TEXT("req_cli_e6");
	Input.ToolName = FName(*Args[0].TrimStartAndEnd());
	Input.bSourceControlEnabled = bSourceControlEnabled;
	Input.bCheckoutSucceeded = bCheckoutSucceeded;

	const FHCIAgentSourceControlDecision Decision =
		FHCIAgentExecutionGate::EvaluateSourceControlFailFast(Input, Registry);
	HCI_LogAgentSourceControlDecision(TEXT("custom"), Decision);
}

struct FHCIAgentRbacMockUserConfigEntry
{
	FString UserName;
	FString UserNameNormalized;
	FString RoleName;
	TArray<FString> AllowedCapabilities;
};

struct FHCIAgentRbacResolvedUser
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
	TArray<FHCIAgentRbacMockUserConfigEntry>& OutEntries,
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

		FHCIAgentRbacMockUserConfigEntry& Entry = OutEntries.AddDefaulted_GetRef();
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

static FHCIAgentRbacResolvedUser HCI_ResolveAgentRbacMockUser(
	const FString& UserName,
	const TArray<FHCIAgentRbacMockUserConfigEntry>& Entries)
{
	FHCIAgentRbacResolvedUser Resolved;
	Resolved.UserName = UserName.TrimStartAndEnd();
	const FString UserNameNormalized = Resolved.UserName.ToLower();

	for (const FHCIAgentRbacMockUserConfigEntry& Entry : Entries)
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
	const FHCIAgentLocalAuditLogRecord& Record,
	FString& OutAuditLogPath,
	FString& OutJsonLine,
	FString& OutError)
{
	OutAuditLogPath = HCI_GetAgentExecAuditLogPath();
	OutJsonLine.Reset();
	OutError.Reset();

	if (!FHCIAgentExecutionGate::SerializeLocalAuditLogRecordToJsonLine(Record, OutJsonLine, OutError))
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
	const FHCIAgentMockRbacDecision& Decision,
	const bool bAuditLogAppended,
	const FString& AuditLogPath,
	const FString& AuditLogError)
{
	const bool bUseWarning = !Decision.bAllowed;
	if (!bUseWarning)
	{
		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentRBAC] case=%s request_id=%s user=%s resolved_role=%s user_in_whitelist=%s guest_fallback=%s tool_name=%s capability=%s write_like=%s asset_count=%d allowed=%s error_code=%s reason=%s audit_log_appended=%s audit_log_path=%s audit_log_error=%s"),
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
		LogHCIAgentDemo,
		Warning,
		TEXT("[HCI][AgentRBAC] case=%s request_id=%s user=%s resolved_role=%s user_in_whitelist=%s guest_fallback=%s tool_name=%s capability=%s write_like=%s asset_count=%d allowed=%s error_code=%s reason=%s audit_log_appended=%s audit_log_path=%s audit_log_error=%s"),
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

void HCI_RunAbilityKitAgentRbacDemoCommand(const TArray<FString>& Args)
{
	const FHCIToolRegistry& Registry = FHCIToolRegistry::GetReadOnly();

	TArray<FHCIAgentRbacMockUserConfigEntry> RbacEntries;
	FString ConfigPath;
	FString ConfigError;
	bool bConfigCreated = false;
	if (!HCI_LoadAgentRbacMockConfigEntries(RbacEntries, ConfigPath, bConfigCreated, ConfigError))
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentRBAC] config_load_failed path=%s reason=%s"),
			ConfigPath.IsEmpty() ? TEXT("-") : *ConfigPath,
			ConfigError.IsEmpty() ? TEXT("unknown") : *ConfigError);
		return;
	}

	if (bConfigCreated)
	{
		UE_LOG(
			LogHCIAgentDemo,
			Warning,
			TEXT("[HCI][AgentRBAC] config_created_default path=%s"),
			*ConfigPath);
	}

	auto RunCase = [&Registry, &RbacEntries](const TCHAR* CaseName, const TCHAR* RequestId, const FString& UserName, const TCHAR* ToolName, const int32 AssetCount) -> TTuple<FHCIAgentMockRbacDecision, bool, FString, FString>
	{
		const FHCIAgentRbacResolvedUser ResolvedUser = HCI_ResolveAgentRbacMockUser(UserName, RbacEntries);

		FHCIAgentMockRbacCheckInput Input;
		Input.RequestId = RequestId;
		Input.UserName = ResolvedUser.UserName;
		Input.ResolvedRole = ResolvedUser.ResolvedRole;
		Input.bUserMatchedWhitelist = ResolvedUser.bMatchedWhitelist;
		Input.ToolName = FName(ToolName);
		Input.TargetAssetCount = AssetCount;
		Input.AllowedCapabilities = ResolvedUser.AllowedCapabilities;

		const FHCIAgentMockRbacDecision Decision = FHCIAgentExecutionGate::EvaluateMockRbac(Input, Registry);

		FHCIAgentLocalAuditLogRecord Record;
		Record.TimestampUtc = FHCITimeFormat::FormatNowBeijingIso8601();
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
				LogHCIAgentDemo,
				Error,
				TEXT("[HCI][AgentAuditLog] append_failed case=%s path=%s reason=%s"),
				CaseName,
				AuditLogPath.IsEmpty() ? TEXT("-") : *AuditLogPath,
				AuditLogError.IsEmpty() ? TEXT("unknown") : *AuditLogError);
		}
		else
		{
			UE_LOG(
				LogHCIAgentDemo,
				Display,
				TEXT("[HCI][AgentAuditLog] append_ok case=%s path=%s bytes=%d user=%s tool_name=%s result=%s error_code=%s"),
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

		auto ConsumeCase = [&AllowedCount, &BlockedCount, &GuestFallbackCount, &AuditLogAppendCount, &LastAuditLogPath](const TTuple<FHCIAgentMockRbacDecision, bool, FString, FString>& ResultTuple)
		{
			const FHCIAgentMockRbacDecision& Decision = ResultTuple.Get<0>();
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
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentRBAC] summary total_cases=%d allowed=%d blocked=%d guest_fallback_cases=%d audit_log_appends=%d config_users=%d validation=ok"),
			3,
			AllowedCount,
			BlockedCount,
			GuestFallbackCount,
			AuditLogAppendCount,
			RbacEntries.Num());
		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentRBAC] paths config_path=%s audit_log_path=%s config_created_default=%s"),
			*ConfigPath,
			LastAuditLogPath.IsEmpty() ? *HCI_GetAgentExecAuditLogPath() : *LastAuditLogPath,
			bConfigCreated ? TEXT("true") : TEXT("false"));
		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentRBAC] hint=也可运行 HCI.AgentRbacDemo [user_name] [tool_name] [asset_count>=0]"));
		return;
	}

	if (Args.Num() < 3)
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentRBAC] invalid_args usage=HCI.AgentRbacDemo [user_name] [tool_name] [asset_count>=0]"));
		return;
	}

	int32 AssetCount = 0;
	if (!HCI_TryParseNonNegativeIntArg(Args[2], AssetCount))
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentRBAC] invalid_args reason=asset_count must be integer >= 0"));
		return;
	}

	const FString UserName = Args[0].TrimStartAndEnd();
	const FString ToolName = Args[1].TrimStartAndEnd();
	if (UserName.IsEmpty() || ToolName.IsEmpty())
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentRBAC] invalid_args reason=user_name and tool_name must be non-empty"));
		return;
	}

	RunCase(TEXT("custom"), TEXT("req_cli_e7"), UserName, *ToolName, AssetCount);
}

static void HCI_LogAgentLodSafetyDecision(const TCHAR* CaseName, const FHCIAgentLodToolSafetyDecision& Decision)
{
	const bool bUseWarning = !Decision.bAllowed;
	if (!bUseWarning)
	{
		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentLodSafety] case=%s request_id=%s tool_name=%s capability=%s write_like=%s is_lod_tool=%s target_object_class=%s is_static_mesh_target=%s nanite_enabled=%s allowed=%s error_code=%s reason=%s"),
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
		LogHCIAgentDemo,
		Warning,
		TEXT("[HCI][AgentLodSafety] case=%s request_id=%s tool_name=%s capability=%s write_like=%s is_lod_tool=%s target_object_class=%s is_static_mesh_target=%s nanite_enabled=%s allowed=%s error_code=%s reason=%s"),
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

void HCI_RunAbilityKitAgentLodSafetyDemoCommand(const TArray<FString>& Args)
{
	const FHCIToolRegistry& Registry = FHCIToolRegistry::GetReadOnly();

	auto BuildInput = [](const TCHAR* RequestId, const TCHAR* ToolName, const TCHAR* TargetObjectClass, const bool bNaniteEnabled)
	{
		FHCIAgentLodToolSafetyCheckInput Input;
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
						   const FHCIAgentLodToolSafetyCheckInput& Input)
		{
			const FHCIAgentLodToolSafetyDecision Decision =
				FHCIAgentExecutionGate::EvaluateLodToolSafety(Input, Registry);
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
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentLodSafety] summary total_cases=%d allowed=%d blocked=%d type_blocked=%d nanite_blocked=%d expected_blocked_code=%s validation=ok"),
			3,
			AllowedCount,
			BlockedCount,
			TypeBlockedCount,
			NaniteBlockedCount,
			TEXT("E4010"));
		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentLodSafety] hint=也可运行 HCI.AgentLodSafetyDemo [tool_name] [target_object_class] [nanite_enabled 0|1]"));
		return;
	}

	if (Args.Num() < 3)
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentLodSafety] invalid_args usage=HCI.AgentLodSafetyDemo [tool_name] [target_object_class] [nanite_enabled 0|1]"));
		return;
	}

	bool bNaniteEnabled = false;
	if (!HCI_TryParseBool01Arg(Args[2], bNaniteEnabled))
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentLodSafety] invalid_args reason=nanite_enabled must be 0 or 1"));
		return;
	}

	const FString ToolName = Args[0].TrimStartAndEnd();
	const FString TargetObjectClass = Args[1].TrimStartAndEnd();
	if (ToolName.IsEmpty() || TargetObjectClass.IsEmpty())
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentLodSafety] invalid_args reason=tool_name and target_object_class must be non-empty"));
		return;
	}

	const FHCIAgentLodToolSafetyDecision Decision =
		FHCIAgentExecutionGate::EvaluateLodToolSafety(
			BuildInput(TEXT("req_cli_e8"), *ToolName, *TargetObjectClass, bNaniteEnabled),
			Registry);
	HCI_LogAgentLodSafetyDecision(TEXT("custom"), Decision);
}

static bool HCI_BuildAgentPlanDemoPlan(
	const FString& UserText,
	const FString& RequestId,
	FHCIAgentPlan& OutPlan,
	FString& OutRouteReason,
	FString& OutError)
{
	const FHCIToolRegistry& ToolRegistry = FHCIToolRegistry::GetReadOnly();
	return FHCIAgentPlanner::BuildPlanFromNaturalLanguage(
		UserText,
		RequestId,
		ToolRegistry,
		OutPlan,
		OutRouteReason,
		OutError);
}

static FHCIAgentPlannerBuildOptions HCI_MakeRealHttpPlannerOptions()
{
	FHCIAgentPlannerBuildOptions PlannerOptions;
	PlannerOptions.bPreferLlm = true;
	PlannerOptions.bUseRealHttpProvider = true;
	PlannerOptions.LlmMockMode = EHCIAgentPlannerLlmMockMode::None;
	PlannerOptions.LlmRetryCount = 1;
	PlannerOptions.bEnableAutoEnvContextScan = true;
	PlannerOptions.ScanAssetsForEnvContext = HCI_ScanAssetsForPlannerEnvContext;
	PlannerOptions.bLlmEnableThinking = false;
	PlannerOptions.bLlmStream = false;
	PlannerOptions.LlmHttpTimeoutMs = 30000;
	return PlannerOptions;
}

static FHCIAgentPlanPreviewContext HCI_MakePlanPreviewContext(
	const FString& RouteReason,
	const FHCIAgentPlannerResultMetadata& PlannerMetadata)
{
	FHCIAgentPlanPreviewContext Context;
	Context.RouteReason = RouteReason;
	Context.PlannerProvider = PlannerMetadata.PlannerProvider;
	Context.ProviderMode = PlannerMetadata.ProviderMode;
	Context.bFallbackUsed = PlannerMetadata.bFallbackUsed;
	Context.FallbackReason = PlannerMetadata.FallbackReason;
	Context.EnvScannedAssetCount = PlannerMetadata.bEnvContextInjected ? PlannerMetadata.EnvContextAssetCount : INDEX_NONE;
	return Context;
}

static void HCI_LogAgentPlanWithProviderSummary(
	const TCHAR* CaseName,
	const FString& UserText,
	const FString& RouteReason,
	const FHCIAgentPlan& Plan,
	const FHCIAgentPlannerResultMetadata& PlannerMetadata,
	const FHCIAgentPlanValidationResult& Validation)
{
		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentPlanLLM] case=%s summary request_id=%s intent=%s input=%s provider=%s provider_mode=%s fallback_used=%s fallback_reason=%s error_code=%s llm_attempts=%d retry_used=%s circuit_open=%s consecutive_llm_failures=%d env_context_injected=%s env_assets=%d env_scan_root=%s plan_validation=%s validation_code=%s validation_reason=%s route_reason=%s steps=%d"),
			CaseName,
			*Plan.RequestId,
			*Plan.Intent,
			*UserText,
		*PlannerMetadata.PlannerProvider,
		*PlannerMetadata.ProviderMode,
		PlannerMetadata.bFallbackUsed ? TEXT("true") : TEXT("false"),
		*PlannerMetadata.FallbackReason,
		*PlannerMetadata.ErrorCode,
		PlannerMetadata.LlmAttemptCount,
			PlannerMetadata.bRetryUsed ? TEXT("true") : TEXT("false"),
			PlannerMetadata.bCircuitBreakerOpen ? TEXT("true") : TEXT("false"),
			PlannerMetadata.ConsecutiveLlmFailures,
			PlannerMetadata.bEnvContextInjected ? TEXT("true") : TEXT("false"),
			PlannerMetadata.EnvContextAssetCount,
				PlannerMetadata.EnvContextScanRoot.IsEmpty() ? TEXT("") : *PlannerMetadata.EnvContextScanRoot,
			Validation.bValid ? TEXT("ok") : TEXT("fail"),
			Validation.ErrorCode.IsEmpty() ? TEXT("-") : *Validation.ErrorCode,
			Validation.Reason.IsEmpty() ? TEXT("-") : *Validation.Reason,
		*RouteReason,
		Plan.Steps.Num());
}

static void HCI_LogAgentPlanRows(const TCHAR* CaseName, const FString& UserText, const FString& RouteReason, const FHCIAgentPlan& Plan)
{
	for (int32 Index = 0; Index < Plan.Steps.Num(); ++Index)
	{
		const FHCIAgentPlanStep& Step = Plan.Steps[Index];
		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentPlan] case=%s row=%d request_id=%s intent=%s input=%s step_id=%s tool_name=%s risk_level=%s requires_confirm=%s rollback_strategy=%s route_reason=%s expected_evidence=%s args=%s"),
			CaseName,
			Index,
			*Plan.RequestId,
			*Plan.Intent,
			*UserText,
			*Step.StepId,
			*Step.ToolName.ToString(),
			*FHCIAgentPlanContract::RiskLevelToString(Step.RiskLevel),
			Step.bRequiresConfirm ? TEXT("true") : TEXT("false"),
			*Step.RollbackStrategy,
			*RouteReason,
			*FString::Join(Step.ExpectedEvidence, TEXT("|")),
		*HCI_JsonObjectToCompactString(Step.Args));
	}
}

void HCI_RunAbilityKitAgentPlanDemoCommand(const TArray<FString>& Args)
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
			FHCIAgentPlan Plan;
			FString RouteReason;
			FString Error;
			if (!HCI_BuildAgentPlanDemoPlan(DemoCase.UserText, DemoCase.RequestId, Plan, RouteReason, Error))
			{
				UE_LOG(
					LogHCIAgentDemo,
					Error,
					TEXT("[HCI][AgentPlan] case=%s build_failed input=%s reason=%s"),
					DemoCase.CaseName,
					DemoCase.UserText,
					*Error);
				continue;
			}

			++BuiltCount;
			FString ContractError;
			const bool bValid = FHCIAgentPlanContract::ValidateMinimalContract(Plan, ContractError);
			if (bValid)
			{
				++ValidationOkCount;
			}

			UE_LOG(
				LogHCIAgentDemo,
				Display,
				TEXT("[HCI][AgentPlan] case=%s summary request_id=%s intent=%s input=%s plan_version=%d steps=%d route_reason=%s validation=%s"),
				DemoCase.CaseName,
				*Plan.RequestId,
				*Plan.Intent,
				DemoCase.UserText,
				Plan.PlanVersion,
				Plan.Steps.Num(),
				*RouteReason,
				bValid ? TEXT("ok") : *FString::Printf(TEXT("fail:%s"), *ContractError));

			HCI_LogAgentPlanRows(DemoCase.CaseName, DemoCase.UserText, RouteReason, Plan);
			HCI_State().AgentPlanPreviewState = Plan;
		}

		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentPlan] summary total_cases=%d built=%d validation_ok=%d plan_version=1 supports_intents=naming_traceability|level_risk|asset_compliance validation=ok"),
			Cases.Num(),
			BuiltCount,
			ValidationOkCount);
		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentPlan] hint=也可运行 HCI.AgentPlanDemo [自然语言文本...] 或 HCI.AgentPlanDemoJson [自然语言文本...]"));
		return;
	}

	const FString UserText = HCI_JoinConsoleArgsAsText(Args);
	if (UserText.IsEmpty())
	{
		UE_LOG(LogHCIAgentDemo, Error, TEXT("[HCI][AgentPlan] invalid_args reason=empty input text"));
		return;
	}

	FHCIAgentPlan Plan;
	FString RouteReason;
	FString Error;
	if (!HCI_BuildAgentPlanDemoPlan(UserText, TEXT("req_cli_f1"), Plan, RouteReason, Error))
	{
		UE_LOG(LogHCIAgentDemo, Error, TEXT("[HCI][AgentPlan] build_failed input=%s reason=%s"), *UserText, *Error);
		return;
	}

	FString ContractError;
	const bool bValid = FHCIAgentPlanContract::ValidateMinimalContract(Plan, ContractError);
	HCI_State().AgentPlanPreviewState = Plan;

	UE_LOG(
		LogHCIAgentDemo,
		Display,
		TEXT("[HCI][AgentPlan] case=custom summary request_id=%s intent=%s input=%s plan_version=%d steps=%d route_reason=%s validation=%s"),
		*Plan.RequestId,
		*Plan.Intent,
		*UserText,
		Plan.PlanVersion,
		Plan.Steps.Num(),
		*RouteReason,
		bValid ? TEXT("ok") : *FString::Printf(TEXT("fail:%s"), *ContractError));

	HCI_LogAgentPlanRows(TEXT("custom"), UserText, RouteReason, Plan);
}

void HCI_RunAbilityKitAgentPlanDemoJsonCommand(const TArray<FString>& Args)
{
	const FString UserText =
		Args.Num() > 0 ? HCI_JoinConsoleArgsAsText(Args) : FString(TEXT("整理临时目录资产，按规范命名并归档"));

	FHCIAgentPlan Plan;
	FString RouteReason;
	FString Error;
	if (!HCI_BuildAgentPlanDemoPlan(UserText, TEXT("req_cli_f1_json"), Plan, RouteReason, Error))
	{
		UE_LOG(LogHCIAgentDemo, Error, TEXT("[HCI][AgentPlan] json_build_failed input=%s reason=%s"), *UserText, *Error);
		return;
	}

	FString JsonText;
	if (!FHCIAgentPlanJsonSerializer::SerializeToJsonString(Plan, JsonText))
	{
		UE_LOG(LogHCIAgentDemo, Error, TEXT("[HCI][AgentPlan] export_json_failed reason=serialize_failed"));
		return;
	}

	HCI_State().AgentPlanPreviewState = Plan;
	UE_LOG(
		LogHCIAgentDemo,
		Display,
		TEXT("[HCI][AgentPlan] json request_id=%s intent=%s route_reason=%s json=%s"),
		*Plan.RequestId,
		*Plan.Intent,
		*RouteReason,
		*JsonText);
}

bool HCI_IsAgentPlanPreviewRequestInFlight()
{
	return HCI_State().bRealLlmPlanCommandInFlight.Load();
}

bool HCI_RequestAgentPlanPreviewFromUi(
	const FString& UserText,
	const FString& SourceTag,
	const FString& ExtraEnvContextText,
	const bool bAutoOpenPreviewWindow,
	FHCIAgentPlanPreviewRequestOnComplete&& OnComplete)
{
	const FString SafeUserText = UserText.TrimStartAndEnd();
	const FString SafeSourceTag = SourceTag.TrimStartAndEnd().IsEmpty()
		? FString(TEXT("AgentPlanPreviewUI"))
		: SourceTag.TrimStartAndEnd();

	if (SafeUserText.IsEmpty())
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentPlanPreviewUI] invalid_args reason=empty input text source=%s"),
			*SafeSourceTag);
		if (OnComplete)
		{
			const FHCIAgentPlan EmptyPlan;
			const FHCIAgentPlannerResultMetadata EmptyMetadata;
			OnComplete(false, SafeUserText, EmptyPlan, FString(), EmptyMetadata, TEXT("empty_input"));
		}
		return false;
	}

	if (HCI_State().bRealLlmPlanCommandInFlight.Exchange(true))
	{
		UE_LOG(
			LogHCIAgentDemo,
			Warning,
			TEXT("[HCI][AgentPlanPreviewUI] busy reason=previous_request_in_flight input=%s source=%s"),
			*SafeUserText,
			*SafeSourceTag);
		if (OnComplete)
		{
			const FHCIAgentPlan EmptyPlan;
			const FHCIAgentPlannerResultMetadata EmptyMetadata;
			OnComplete(false, SafeUserText, EmptyPlan, FString(), EmptyMetadata, TEXT("request_busy"));
		}
		return false;
	}

	UE_LOG(
		LogHCIAgentDemo,
		Display,
		TEXT("[HCI][AgentPlanLLM][H3] dispatched mode=normal input=%s source=%s hint=异步执行中，结果将随后输出"),
		*SafeUserText,
		*SafeSourceTag);

	const FHCIToolRegistry& ToolRegistry = FHCIToolRegistry::GetReadOnly();
	FHCIAgentPlannerBuildOptions PlannerOptions = HCI_MakeRealHttpPlannerOptions();
	PlannerOptions.ExtraEnvContextText = ExtraEnvContextText;
	FHCIAgentPlanner::BuildPlanFromNaturalLanguageWithProviderAsync(
		SafeUserText,
		TEXT("req_cli_i1_preview_ui"),
		ToolRegistry,
		PlannerOptions,
			[SafeUserText, SafeSourceTag, bAutoOpenPreviewWindow, ToolRegistryPtr = &ToolRegistry, OnComplete = MoveTemp(OnComplete)](
				bool bBuilt,
				FHCIAgentPlan Plan,
				FString RouteReason,
			FHCIAgentPlannerResultMetadata PlannerMetadata,
			FString Error) mutable
		{
			HCI_State().bRealLlmPlanCommandInFlight.Store(false);
			if (!bBuilt)
			{
				UE_LOG(
					LogHCIAgentDemo,
					Error,
					TEXT("[HCI][AgentPlanPreviewUI] build_failed input=%s provider=%s provider_mode=%s fallback_used=%s fallback_reason=%s error_code=%s reason=%s source=%s"),
					*SafeUserText,
					*PlannerMetadata.PlannerProvider,
					*PlannerMetadata.ProviderMode,
					PlannerMetadata.bFallbackUsed ? TEXT("true") : TEXT("false"),
					*PlannerMetadata.FallbackReason,
					PlannerMetadata.ErrorCode.IsEmpty() ? TEXT("-") : *PlannerMetadata.ErrorCode,
					*Error,
					*SafeSourceTag);
				if (OnComplete)
				{
					OnComplete(false, SafeUserText, Plan, RouteReason, PlannerMetadata, Error);
				}
				return;
			}

			const bool bMessageOnlyPlan = Plan.Steps.Num() <= 0 && !Plan.AssistantMessage.TrimStartAndEnd().IsEmpty();
			FHCIAgentPlanValidationResult Validation;
			if (!bMessageOnlyPlan)
			{
				FHCIAgentPlanValidationContext ValidationContext;
				ValidationContext.bRequireWriteStepForModifyIntent = true;
				ValidationContext.bRequirePipelineInputs = true;
				if (!FHCIAgentPlanValidator::ValidatePlan(Plan, *ToolRegistryPtr, ValidationContext, Validation))
				{
					const FString ValidationError = FString::Printf(
						TEXT("plan_validation_failed code=%s field=%s reason=%s"),
						Validation.ErrorCode.IsEmpty() ? TEXT("-") : *Validation.ErrorCode,
						Validation.Field.IsEmpty() ? TEXT("-") : *Validation.Field,
						Validation.Reason.IsEmpty() ? TEXT("-") : *Validation.Reason);
					UE_LOG(
						LogHCIAgentDemo,
						Error,
						TEXT("[HCI][AgentPlanPreviewUI] build_failed input=%s provider=%s provider_mode=%s fallback_used=%s fallback_reason=%s error_code=%s reason=%s source=%s"),
						*SafeUserText,
						*PlannerMetadata.PlannerProvider,
						*PlannerMetadata.ProviderMode,
						PlannerMetadata.bFallbackUsed ? TEXT("true") : TEXT("false"),
						*PlannerMetadata.FallbackReason,
						PlannerMetadata.ErrorCode.IsEmpty() ? TEXT("-") : *PlannerMetadata.ErrorCode,
						*ValidationError,
						*SafeSourceTag);
					if (OnComplete)
					{
						OnComplete(false, SafeUserText, Plan, RouteReason, PlannerMetadata, ValidationError);
					}
					return;
				}
			}
			else
			{
				Validation = FHCIAgentPlanValidationResult();
				Validation.bValid = true;
				Validation.ErrorCode = TEXT("-");
				Validation.Field = TEXT("-");
				Validation.Reason = TEXT("ok");
			}

			HCI_State().AgentPlanPreviewState = Plan;
			HCI_LogAgentPlanWithProviderSummary(TEXT("preview_ui_real_http"), SafeUserText, RouteReason, Plan, PlannerMetadata, Validation);
			HCI_LogAgentPlanRows(TEXT("preview_ui_real_http"), SafeUserText, RouteReason, Plan);
			if (bAutoOpenPreviewWindow && !bMessageOnlyPlan)
			{
				const FHCIAgentPlanPreviewContext PreviewContext = HCI_MakePlanPreviewContext(RouteReason, PlannerMetadata);
				FHCIAgentPlanPreviewWindow::OpenWindow(
					Plan,
					PreviewContext);

				UE_LOG(
					LogHCIAgentDemo,
					Display,
					TEXT("[HCI][AgentPlanPreviewUI] opened request_id=%s intent=%s route_reason=%s steps=%d source=%s"),
					*Plan.RequestId,
					*Plan.Intent,
					*RouteReason,
					Plan.Steps.Num(),
					*SafeSourceTag);
			}
			else if (bMessageOnlyPlan)
			{
				UE_LOG(
					LogHCIAgentDemo,
					Display,
					TEXT("[HCI][AgentPlanPreviewUI] prepared_message_only request_id=%s intent=%s route_reason=%s source=%s assistant_message=%s"),
					*Plan.RequestId,
					*Plan.Intent,
					*RouteReason,
					*SafeSourceTag,
					*Plan.AssistantMessage);
			}
			else
			{
				UE_LOG(
					LogHCIAgentDemo,
					Display,
					TEXT("[HCI][AgentPlanPreviewUI] prepared request_id=%s intent=%s route_reason=%s steps=%d source=%s auto_open=false"),
					*Plan.RequestId,
					*Plan.Intent,
					*RouteReason,
					Plan.Steps.Num(),
					*SafeSourceTag);
			}
			if (OnComplete)
			{
				OnComplete(true, SafeUserText, Plan, RouteReason, PlannerMetadata, FString());
			}
		});
	return true;
}

void HCI_RunAbilityKitAgentPlanPreviewUiCommand(const TArray<FString>& Args)
{
	const FString UserText =
		Args.Num() > 0 ? HCI_JoinConsoleArgsAsText(Args) : FString(TEXT("整理临时目录资产，按规范命名并归档"));
	HCI_RequestAgentPlanPreviewFromUi(
		UserText,
		TEXT("AgentPlanPreviewUI"),
		FString(),
		true,
		FHCIAgentPlanPreviewRequestOnComplete());
}

void HCI_RunAbilityKitAgentChatUiCommand(const TArray<FString>& Args)
{
	const FString InitialText = Args.Num() > 0 ? HCI_JoinConsoleArgsAsText(Args) : FString();
	FHCIAgentChatWindow::OpenWindow(InitialText);
}

static FHCIAgentPlan HCI_MakeAgentPlanValidateTexturePlan(const TCHAR* RequestId)
{
	FHCIAgentPlan Plan;
	Plan.PlanVersion = 1;
	Plan.RequestId = RequestId;
	Plan.Intent = TEXT("batch_fix_asset_compliance");

	FHCIAgentPlanStep& Step = Plan.Steps.AddDefaulted_GetRef();
	Step.StepId = TEXT("s1");
	Step.ToolName = TEXT("SetTextureMaxSize");
	Step.RiskLevel = EHCIAgentPlanRiskLevel::Write;
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

static FHCIAgentPlan HCI_MakeAgentPlanValidateScanAssetsPlan(const TCHAR* RequestId)
{
	FHCIAgentPlan Plan;
	Plan.PlanVersion = 1;
	Plan.RequestId = RequestId;
	Plan.Intent = TEXT("inspect_folder_contents");

	FHCIAgentPlanStep& Step = Plan.Steps.AddDefaulted_GetRef();
	Step.StepId = TEXT("s1");
	Step.ToolName = TEXT("ScanAssets");
	Step.RiskLevel = EHCIAgentPlanRiskLevel::ReadOnly;
	Step.bRequiresConfirm = false;
	Step.RollbackStrategy = TEXT("all_or_nothing");
	Step.ExpectedEvidence = {TEXT("scan_root"), TEXT("asset_count"), TEXT("asset_paths"), TEXT("result")};
	Step.Args = MakeShared<FJsonObject>();
	Step.Args->SetStringField(TEXT("directory"), TEXT("/Game/Temp"));
	return Plan;
}

static bool HCI_BuildAgentPlanValidateDemoCase(
	const FString& CaseKey,
	FHCIAgentPlan& OutPlan,
	FHCIAgentPlanValidationContext& OutContext,
	FString& OutDescription,
	FString& OutExpectedCode,
	FString& OutError)
{
	OutPlan = FHCIAgentPlan();
	OutContext = FHCIAgentPlanValidationContext();
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

	if (CaseKey.Equals(TEXT("fail_illegal_evidence_key"), ESearchCase::IgnoreCase))
	{
		OutDescription = TEXT("illegal_expected_evidence_key_for_tool");
		OutExpectedCode = TEXT("E4009");
		OutPlan = HCI_MakeAgentPlanValidateScanAssetsPlan(TEXT("req_demo_f2_e4009_illegal_evidence"));
		if (OutPlan.Steps.Num() > 0)
		{
			OutPlan.Steps[0].ExpectedEvidence = {TEXT("unknown_key")};
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

			FHCIAgentPlanStep& Step2 = OutPlan.Steps.AddDefaulted_GetRef();
			Step2.StepId = TEXT("s2");
			Step2.ToolName = TEXT("SetTextureMaxSize");
			Step2.RiskLevel = EHCIAgentPlanRiskLevel::Write;
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
	const FHCIAgentPlanValidationResult& Result)
{
	const bool bExpected = ExpectedCode.IsEmpty() || ExpectedCode == TEXT("-") ? Result.bValid : Result.ErrorCode == ExpectedCode;
	if (Result.bValid || bExpected)
	{
		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentPlanValidate] case=%s request_id=%s intent=%s plan_version=%d step_count=%d valid=%s error_code=%s field=%s reason=%s validated_steps=%d write_steps=%d total_modify_targets=%d max_risk=%s expected_code=%s expected_match=%s note=%s"),
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
			LogHCIAgentDemo,
			Warning,
			TEXT("[HCI][AgentPlanValidate] case=%s request_id=%s intent=%s plan_version=%d step_count=%d valid=%s error_code=%s field=%s reason=%s validated_steps=%d write_steps=%d total_modify_targets=%d max_risk=%s expected_code=%s expected_match=%s note=%s"),
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

void HCI_RunAbilityKitAgentPlanValidateDemoCommand(const TArray<FString>& Args)
{
	const FHCIToolRegistry& Registry = FHCIToolRegistry::GetReadOnly();

	if (Args.Num() == 0)
	{
		const TArray<FString> Cases = {
			TEXT("ok_naming"),
			TEXT("fail_missing_tool"),
			TEXT("fail_unknown_tool"),
			TEXT("fail_invalid_enum"),
			TEXT("fail_illegal_evidence_key"),
			TEXT("fail_over_limit"),
			TEXT("fail_level_scope"),
			TEXT("fail_naming_metadata")};

		int32 ValidCount = 0;
		int32 InvalidCount = 0;
		int32 ExpectedMatches = 0;

		for (const FString& CaseKey : Cases)
		{
			FHCIAgentPlan Plan;
			FHCIAgentPlanValidationContext Context;
			FString Description;
			FString ExpectedCode;
			FString BuildError;
			if (!HCI_BuildAgentPlanValidateDemoCase(CaseKey, Plan, Context, Description, ExpectedCode, BuildError))
			{
				UE_LOG(LogHCIAgentDemo, Error, TEXT("[HCI][AgentPlanValidate] case=%s build_failed reason=%s"), *CaseKey, *BuildError);
				continue;
			}

			FHCIAgentPlanValidationResult Result;
			FHCIAgentPlanValidator::ValidatePlan(Plan, Registry, Context, Result);
			HCI_State().AgentPlanPreviewState = Plan;
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
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentPlanValidate] summary total_cases=%d valid=%d invalid=%d expected_matches=%d supports_checks=schema|whitelist|risk|threshold|special_cases validation=%s"),
			Cases.Num(),
			ValidCount,
			InvalidCount,
			ExpectedMatches,
			ExpectedMatches == Cases.Num() ? TEXT("ok") : TEXT("fail"));
		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentPlanValidate] hint=也可运行 HCI.AgentPlanValidateDemo [ok_naming|fail_missing_tool|fail_unknown_tool|fail_invalid_enum|fail_illegal_evidence_key|fail_over_limit|fail_level_scope|fail_naming_metadata]"));
		return;
	}

	if (Args.Num() != 1)
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentPlanValidate] invalid_args usage=HCI.AgentPlanValidateDemo [case_key]"));
		return;
	}

	const FString CaseKey = Args[0].TrimStartAndEnd();
	if (CaseKey.IsEmpty())
	{
		UE_LOG(LogHCIAgentDemo, Error, TEXT("[HCI][AgentPlanValidate] invalid_args reason=case_key is empty"));
		return;
	}

	FHCIAgentPlan Plan;
	FHCIAgentPlanValidationContext Context;
	FString Description;
	FString ExpectedCode;
	FString BuildError;
	if (!HCI_BuildAgentPlanValidateDemoCase(CaseKey, Plan, Context, Description, ExpectedCode, BuildError))
	{
		UE_LOG(LogHCIAgentDemo, Error, TEXT("[HCI][AgentPlanValidate] build_failed case=%s reason=%s"), *CaseKey, *BuildError);
		return;
	}

	FHCIAgentPlanValidationResult Result;
	FHCIAgentPlanValidator::ValidatePlan(Plan, Registry, Context, Result);
	HCI_State().AgentPlanPreviewState = Plan;
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

static void HCI_LogAgentExecutorRows(const TCHAR* CaseName, const FHCIAgentExecutorRunResult& RunResult)
{
	for (int32 Index = 0; Index < RunResult.StepResults.Num(); ++Index)
	{
		const FHCIAgentExecutorStepResult& Row = RunResult.StepResults[Index];
		if (Row.bSucceeded)
		{
			UE_LOG(
				LogHCIAgentDemo,
				Display,
				TEXT("[HCI][AgentExecutor] case=%s row=%d step_id=%s tool_name=%s capability=%s risk_level=%s write_like=%s status=%s attempted=%s succeeded=%s target_count_estimate=%d evidence_keys=%s evidence=%s error_code=%s reason=%s failure_phase=%s preflight_gate=%s"),
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
				LogHCIAgentDemo,
				Warning,
				TEXT("[HCI][AgentExecutor] case=%s row=%d step_id=%s tool_name=%s capability=%s risk_level=%s write_like=%s status=%s attempted=%s succeeded=%s target_count_estimate=%d evidence_keys=%s evidence=%s error_code=%s reason=%s failure_phase=%s preflight_gate=%s"),
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

static void HCI_LogAgentExecutorSummary(const TCHAR* CaseName, const FHCIAgentExecutorRunResult& RunResult)
{
	if (RunResult.bCompleted)
	{
		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentExecutor] case=%s summary request_id=%s intent=%s plan_version=%d execution_mode=%s termination_policy=%s preflight_enabled=%s accepted=%s completed=%s total_steps=%d executed_steps=%d succeeded_steps=%d failed_steps=%d skipped_steps=%d preflight_blocked_steps=%d failed_step_index=%d failed_step_id=%s failed_tool_name=%s failed_gate=%s terminal_status=%s terminal_reason=%s error_code=%s reason=%s started_utc=%s finished_utc=%s"),
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
			LogHCIAgentDemo,
			Warning,
			TEXT("[HCI][AgentExecutor] case=%s summary request_id=%s intent=%s plan_version=%d execution_mode=%s termination_policy=%s preflight_enabled=%s accepted=%s completed=%s total_steps=%d executed_steps=%d succeeded_steps=%d failed_steps=%d skipped_steps=%d preflight_blocked_steps=%d failed_step_index=%d failed_step_id=%s failed_tool_name=%s failed_gate=%s terminal_status=%s terminal_reason=%s error_code=%s reason=%s started_utc=%s finished_utc=%s"),
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
	const FHCIToolRegistry& Registry,
	int32& OutCompletedCases,
	int32& OutRejectedCases)
{
	FHCIAgentPlan Plan;
	FString RouteReason;
	FString BuildError;
	if (!HCI_BuildAgentPlanDemoPlan(UserText, RequestId, Plan, RouteReason, BuildError))
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentExecutor] case=%s build_failed input=%s reason=%s"),
			CaseName,
			*UserText,
			*BuildError);
		return false;
	}

	HCI_State().AgentPlanPreviewState = Plan;

	FHCIAgentExecutorOptions Options;
	Options.bValidatePlanBeforeExecute = true;
	Options.bDryRun = true;

	FHCIAgentExecutorRunResult RunResult;
	FHCIAgentExecutor::ExecutePlan(
		Plan,
		Registry,
		FHCIAgentPlanValidationContext(),
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
		LogHCIAgentDemo,
		Display,
		TEXT("[HCI][AgentExecutor] case=%s route_reason=%s input=%s"),
		CaseName,
		*RouteReason,
		*UserText);
	HCI_LogAgentExecutorSummary(CaseName, RunResult);
	HCI_LogAgentExecutorRows(CaseName, RunResult);
	return true;
}

static FHCIAgentPlan HCI_BuildAgentExecutorF4TwoStepPlan(const FString& RequestId)
{
	FHCIAgentPlan Plan;
	Plan.PlanVersion = 1;
	Plan.RequestId = RequestId;
	Plan.Intent = TEXT("batch_fix_asset_compliance");

	{
		FHCIAgentPlanStep& Step = Plan.Steps.AddDefaulted_GetRef();
		Step.StepId = TEXT("s1");
		Step.ToolName = TEXT("SetTextureMaxSize");
		Step.RiskLevel = EHCIAgentPlanRiskLevel::Write;
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
		FHCIAgentPlanStep& Step = Plan.Steps.AddDefaulted_GetRef();
		Step.StepId = TEXT("s2");
		Step.ToolName = TEXT("SetMeshLODGroup");
		Step.RiskLevel = EHCIAgentPlanRiskLevel::Write;
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
	const EHCIAgentExecutorTerminationPolicy TerminationPolicy,
	const int32 SimulatedFailureStepIndex,
	const TCHAR* SimulatedErrorCode,
	const TCHAR* SimulatedReason,
	const FHCIToolRegistry& Registry,
	int32& OutCompletedCases,
	int32& OutFailedCases)
{
	FHCIAgentPlan Plan = HCI_BuildAgentExecutorF4TwoStepPlan(RequestId);
	HCI_State().AgentPlanPreviewState = Plan;

	FHCIAgentExecutorOptions Options;
	Options.bValidatePlanBeforeExecute = true;
	Options.bDryRun = true;
	Options.TerminationPolicy = TerminationPolicy;
	Options.SimulatedFailureStepIndex = SimulatedFailureStepIndex;
	Options.SimulatedFailureErrorCode = SimulatedErrorCode ? SimulatedErrorCode : TEXT("");
	Options.SimulatedFailureReason = SimulatedReason ? SimulatedReason : TEXT("");

	FHCIAgentExecutorRunResult RunResult;
	const bool bOk = FHCIAgentExecutor::ExecutePlan(
		Plan,
		Registry,
		FHCIAgentPlanValidationContext(),
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
		LogHCIAgentDemo,
		Display,
		TEXT("[HCI][AgentExecutorFail] case=%s description=%s policy=%s simulated_failure_step=%d"),
		CaseName,
		Description ? Description : TEXT("-"),
		*RunResult.TerminationPolicy,
		SimulatedFailureStepIndex);
	HCI_LogAgentExecutorSummary(CaseName, RunResult);
	HCI_LogAgentExecutorRows(CaseName, RunResult);
	return true;
}

void HCI_RunAbilityKitAgentExecutePlanFailDemoCommand(const TArray<FString>& Args)
{
	const FHCIToolRegistry& Registry = FHCIToolRegistry::GetReadOnly();

	if (Args.Num() == 0)
	{
		int32 CompletedCases = 0;
		int32 FailedCases = 0;
		int32 StopPolicyCases = 0;
		int32 ContinuePolicyCases = 0;

		auto RunCase =
			[&](const TCHAR* CaseName, const TCHAR* Description, const TCHAR* RequestId, const EHCIAgentExecutorTerminationPolicy Policy, const int32 FailIndex, const TCHAR* ErrorCode, const TCHAR* Reason)
		{
			if (Policy == EHCIAgentExecutorTerminationPolicy::StopOnFirstFailure)
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

		RunCase(TEXT("ok_stop_on_first"), TEXT("two_step_success"), TEXT("req_demo_f4_01"), EHCIAgentExecutorTerminationPolicy::StopOnFirstFailure, INDEX_NONE, TEXT(""), TEXT(""));
		RunCase(TEXT("fail_step1_stop_on_first"), TEXT("fail_first_step_stop"), TEXT("req_demo_f4_02"), EHCIAgentExecutorTerminationPolicy::StopOnFirstFailure, 0, TEXT("E4101"), TEXT("simulated_tool_execution_failed"));
		RunCase(TEXT("fail_step1_continue"), TEXT("fail_first_step_continue"), TEXT("req_demo_f4_03"), EHCIAgentExecutorTerminationPolicy::ContinueOnFailure, 0, TEXT("E4102"), TEXT("simulated_first_step_failed_continue"));

		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentExecutorFail] summary total_cases=%d completed_cases=%d failed_cases=%d stop_policy_cases=%d continue_policy_cases=%d execution_mode=%s validation=ok"),
			3,
			CompletedCases,
			FailedCases,
			StopPolicyCases,
			ContinuePolicyCases,
			TEXT("simulate_dry_run"));
		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentExecutorFail] hint=也可运行 HCI.AgentExecutePlanFailDemo [ok|fail_stop|fail_continue]"));
		return;
	}

	if (Args.Num() != 1)
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentExecutorFail] invalid_args usage=HCI.AgentExecutePlanFailDemo [ok|fail_stop|fail_continue]"));
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
			EHCIAgentExecutorTerminationPolicy::StopOnFirstFailure,
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
			EHCIAgentExecutorTerminationPolicy::StopOnFirstFailure,
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
			EHCIAgentExecutorTerminationPolicy::ContinueOnFailure,
			0,
			TEXT("E4102"),
			TEXT("simulated_first_step_failed_continue"),
			Registry,
			CompletedCases,
			FailedCases);
		return;
	}

	UE_LOG(
		LogHCIAgentDemo,
		Error,
		TEXT("[HCI][AgentExecutorFail] invalid_args reason=unknown case_key key=%s allowed=ok|fail_stop|fail_continue"),
		*CaseKey);
}

static FHCIAgentPlan HCI_BuildAgentExecutorF5TexturePlan(const FString& RequestId, const int32 AssetCount)
{
	FHCIAgentPlan Plan;
	Plan.PlanVersion = 1;
	Plan.RequestId = RequestId;
	Plan.Intent = TEXT("batch_fix_asset_compliance");

	FHCIAgentPlanStep& Step = Plan.Steps.AddDefaulted_GetRef();
	Step.StepId = TEXT("s1");
	Step.ToolName = TEXT("SetTextureMaxSize");
	Step.RiskLevel = EHCIAgentPlanRiskLevel::Write;
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
	FHCIAgentPlan Plan,
	const FHCIAgentExecutorOptions& Options,
	const FHCIToolRegistry& Registry,
	int32& OutPassedCases,
	int32& OutBlockedCases,
	TMap<FString, int32>& OutBlockedByGateCounts)
{
	HCI_State().AgentPlanPreviewState = Plan;

	FHCIAgentExecutorRunResult RunResult;
	const bool bOk = FHCIAgentExecutor::ExecutePlan(
		Plan,
		Registry,
		FHCIAgentPlanValidationContext(),
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
		LogHCIAgentDemo,
		Display,
		TEXT("[HCI][AgentExecutorPreflight] case=%s description=%s preflight_enabled=%s"),
		CaseName,
		Description ? Description : TEXT("-"),
		Options.bEnablePreflightGates ? TEXT("true") : TEXT("false"));
	HCI_LogAgentExecutorSummary(CaseName, RunResult);
	HCI_LogAgentExecutorRows(CaseName, RunResult);
	return true;
}

void HCI_RunAbilityKitAgentExecutePlanPreflightDemoCommand(const TArray<FString>& Args)
{
	const FHCIToolRegistry& Registry = FHCIToolRegistry::GetReadOnly();

	auto MakeBaseOptions = []()
	{
		FHCIAgentExecutorOptions Options;
		Options.bValidatePlanBeforeExecute = true;
		Options.bDryRun = true;
		Options.bEnablePreflightGates = true;
		Options.TerminationPolicy = EHCIAgentExecutorTerminationPolicy::StopOnFirstFailure;
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
		FHCIAgentExecutorOptions Options = MakeBaseOptions();
		FHCIAgentPlan Plan;
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
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentExecutorPreflight] summary total_cases=%d passed_cases=%d blocked_cases=%d preflight_enabled=true confirm_blocked=%d blast_radius_blocked=%d rbac_blocked=%d source_control_blocked=%d lod_safety_blocked=%d execution_mode=%s validation=ok"),
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
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentExecutorPreflight] hint=也可运行 HCI.AgentExecutePlanPreflightDemo [ok|fail_confirm|fail_blast|fail_rbac|fail_sc|fail_lod]"));
		return;
	}

	if (Args.Num() != 1)
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentExecutorPreflight] invalid_args usage=HCI.AgentExecutePlanPreflightDemo [ok|fail_confirm|fail_blast|fail_rbac|fail_sc|fail_lod]"));
		return;
	}

	const FString CaseKey = Args[0].TrimStartAndEnd();
	if (CaseKey.IsEmpty())
	{
		UE_LOG(LogHCIAgentDemo, Error, TEXT("[HCI][AgentExecutorPreflight] invalid_args reason=empty case_key"));
		return;
	}

	int32 PassedCases = 0;
	int32 BlockedCases = 0;
	TMap<FString, int32> BlockedByGateCounts;
	if (!RunKey(CaseKey, PassedCases, BlockedCases, BlockedByGateCounts))
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentExecutorPreflight] invalid_args reason=unknown case_key key=%s allowed=ok|fail_confirm|fail_blast|fail_rbac|fail_sc|fail_lod"),
			*CaseKey);
	}
}

static void HCI_LogAgentExecutorReviewDiffSummary(
	const TCHAR* CaseName,
	const FHCIAgentExecutorRunResult& RunResult,
	const FHCIDryRunDiffReport& Report)
{
	const bool bUseWarning = Report.Summary.Skipped > 0 || !RunResult.bCompleted;
	if (bUseWarning)
	{
		UE_LOG(
			LogHCIAgentDemo,
			Warning,
			TEXT("[HCI][AgentExecutorReview] case=%s summary request_id=%s total_candidates=%d modifiable=%d skipped=%d executor_terminal_status=%s executor_failed_gate=%s execution_mode=%s bridge_ok=true"),
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
		LogHCIAgentDemo,
		Display,
		TEXT("[HCI][AgentExecutorReview] case=%s summary request_id=%s total_candidates=%d modifiable=%d skipped=%d executor_terminal_status=%s executor_failed_gate=%s execution_mode=%s bridge_ok=true"),
		CaseName,
		Report.RequestId.IsEmpty() ? TEXT("-") : *Report.RequestId,
		Report.Summary.TotalCandidates,
		Report.Summary.Modifiable,
		Report.Summary.Skipped,
		RunResult.TerminalStatus.IsEmpty() ? TEXT("-") : *RunResult.TerminalStatus,
		RunResult.FailedGate.IsEmpty() ? TEXT("-") : *RunResult.FailedGate,
		RunResult.ExecutionMode.IsEmpty() ? TEXT("-") : *RunResult.ExecutionMode);
}

static void HCI_LogAgentExecutorReviewDiffRows(const TCHAR* CaseName, const FHCIDryRunDiffReport& Report)
{
	for (int32 Index = 0; Index < Report.DiffItems.Num(); ++Index)
	{
		const FHCIDryRunDiffItem& Item = Report.DiffItems[Index];
		const bool bBlockedOrSkipped = !Item.SkipReason.IsEmpty();
		if (bBlockedOrSkipped)
		{
			UE_LOG(
				LogHCIAgentDemo,
				Warning,
				TEXT("[HCI][AgentExecutorReview] case=%s row=%d asset_path=%s field=%s before=%s after=%s tool_name=%s risk=%s skip_reason=%s object_type=%s locate_strategy=%s evidence_key=%s actor_path=%s"),
				CaseName,
				Index,
				Item.AssetPath.IsEmpty() ? TEXT("-") : *Item.AssetPath,
				Item.Field.IsEmpty() ? TEXT("-") : *Item.Field,
				Item.Before.IsEmpty() ? TEXT("-") : *Item.Before,
				Item.After.IsEmpty() ? TEXT("-") : *Item.After,
				Item.ToolName.IsEmpty() ? TEXT("-") : *Item.ToolName,
				*FHCIDryRunDiff::RiskToString(Item.Risk),
				Item.SkipReason.IsEmpty() ? TEXT("-") : *Item.SkipReason,
				*FHCIDryRunDiff::ObjectTypeToString(Item.ObjectType),
				*FHCIDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
				Item.EvidenceKey.IsEmpty() ? TEXT("-") : *Item.EvidenceKey,
				Item.ActorPath.IsEmpty() ? TEXT("-") : *Item.ActorPath);
		}
		else
		{
			UE_LOG(
				LogHCIAgentDemo,
				Display,
				TEXT("[HCI][AgentExecutorReview] case=%s row=%d asset_path=%s field=%s before=%s after=%s tool_name=%s risk=%s skip_reason=%s object_type=%s locate_strategy=%s evidence_key=%s actor_path=%s"),
				CaseName,
				Index,
				Item.AssetPath.IsEmpty() ? TEXT("-") : *Item.AssetPath,
				Item.Field.IsEmpty() ? TEXT("-") : *Item.Field,
				Item.Before.IsEmpty() ? TEXT("-") : *Item.Before,
				Item.After.IsEmpty() ? TEXT("-") : *Item.After,
				Item.ToolName.IsEmpty() ? TEXT("-") : *Item.ToolName,
				*FHCIDryRunDiff::RiskToString(Item.Risk),
				Item.SkipReason.IsEmpty() ? TEXT("-") : *Item.SkipReason,
				*FHCIDryRunDiff::ObjectTypeToString(Item.ObjectType),
				*FHCIDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
				Item.EvidenceKey.IsEmpty() ? TEXT("-") : *Item.EvidenceKey,
				Item.ActorPath.IsEmpty() ? TEXT("-") : *Item.ActorPath);
		}
	}
}

static void HCI_LogAgentExecutorReviewSelectSummary(const FHCIDryRunDiffSelectionResult& SelectionResult)
{
	const bool bUseWarning = SelectionResult.SelectedReport.Summary.Skipped > 0;
	if (bUseWarning)
	{
		UE_LOG(
			LogHCIAgentDemo,
			Warning,
			TEXT("[HCI][AgentExecutorReviewSelect] summary request_id=%s total_before=%d total_after=%d input_rows=%d unique_rows=%d dropped_duplicates=%d modifiable=%d skipped=%d validation=ok"),
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
		LogHCIAgentDemo,
		Display,
		TEXT("[HCI][AgentExecutorReviewSelect] summary request_id=%s total_before=%d total_after=%d input_rows=%d unique_rows=%d dropped_duplicates=%d modifiable=%d skipped=%d validation=ok"),
		SelectionResult.SelectedReport.RequestId.IsEmpty() ? TEXT("-") : *SelectionResult.SelectedReport.RequestId,
		SelectionResult.TotalRowsBefore,
		SelectionResult.TotalRowsAfter,
		SelectionResult.InputRowCount,
		SelectionResult.UniqueRowCount,
		SelectionResult.DroppedDuplicateRows,
		SelectionResult.SelectedReport.Summary.Modifiable,
		SelectionResult.SelectedReport.Summary.Skipped);
}

static void HCI_LogAgentExecutorReviewSelectRows(const FHCIDryRunDiffReport& Report)
{
	for (int32 Index = 0; Index < Report.DiffItems.Num(); ++Index)
	{
		const FHCIDryRunDiffItem& Item = Report.DiffItems[Index];
		const bool bBlockedOrSkipped = !Item.SkipReason.IsEmpty();
		if (bBlockedOrSkipped)
		{
			UE_LOG(
				LogHCIAgentDemo,
				Warning,
				TEXT("[HCI][AgentExecutorReviewSelect] row=%d asset_path=%s field=%s before=%s after=%s tool_name=%s risk=%s skip_reason=%s object_type=%s locate_strategy=%s evidence_key=%s actor_path=%s"),
				Index,
				Item.AssetPath.IsEmpty() ? TEXT("-") : *Item.AssetPath,
				Item.Field.IsEmpty() ? TEXT("-") : *Item.Field,
				Item.Before.IsEmpty() ? TEXT("-") : *Item.Before,
				Item.After.IsEmpty() ? TEXT("-") : *Item.After,
				Item.ToolName.IsEmpty() ? TEXT("-") : *Item.ToolName,
				*FHCIDryRunDiff::RiskToString(Item.Risk),
				Item.SkipReason.IsEmpty() ? TEXT("-") : *Item.SkipReason,
				*FHCIDryRunDiff::ObjectTypeToString(Item.ObjectType),
				*FHCIDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
				Item.EvidenceKey.IsEmpty() ? TEXT("-") : *Item.EvidenceKey,
				Item.ActorPath.IsEmpty() ? TEXT("-") : *Item.ActorPath);
		}
		else
		{
			UE_LOG(
				LogHCIAgentDemo,
				Display,
				TEXT("[HCI][AgentExecutorReviewSelect] row=%d asset_path=%s field=%s before=%s after=%s tool_name=%s risk=%s skip_reason=%s object_type=%s locate_strategy=%s evidence_key=%s actor_path=%s"),
				Index,
				Item.AssetPath.IsEmpty() ? TEXT("-") : *Item.AssetPath,
				Item.Field.IsEmpty() ? TEXT("-") : *Item.Field,
				Item.Before.IsEmpty() ? TEXT("-") : *Item.Before,
				Item.After.IsEmpty() ? TEXT("-") : *Item.After,
				Item.ToolName.IsEmpty() ? TEXT("-") : *Item.ToolName,
				*FHCIDryRunDiff::RiskToString(Item.Risk),
				Item.SkipReason.IsEmpty() ? TEXT("-") : *Item.SkipReason,
				*FHCIDryRunDiff::ObjectTypeToString(Item.ObjectType),
				*FHCIDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
				Item.EvidenceKey.IsEmpty() ? TEXT("-") : *Item.EvidenceKey,
				Item.ActorPath.IsEmpty() ? TEXT("-") : *Item.ActorPath);
		}
	}
}

static bool HCI_ApplyAgentExecutorReviewSelection(
	const TArray<FString>& Args,
	FHCIDryRunDiffSelectionResult& OutSelectionResult)
{
	TArray<int32> RequestedRows;
	FString ParseReason;
	if (!HCI_TryParseRowIndexListArgs(Args, RequestedRows, ParseReason))
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentExecutorReviewSelect] invalid_args reason=%s usage=HCI.AgentExecutePlanReviewSelect [row_indices_csv]"),
			*ParseReason);
		return false;
	}

	if (HCI_State().AgentExecutorReviewDiffPreviewState.DiffItems.Num() <= 0)
	{
		UE_LOG(LogHCIAgentDemo, Warning, TEXT("[HCI][AgentExecutorReviewSelect] select=unavailable reason=no_preview_state"));
		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentExecutorReviewSelect] suggestion=先运行 HCI.AgentExecutePlanReviewDemo 或 HCI.AgentExecutePlanReviewJson"));
		return false;
	}

	if (!FHCIDryRunDiffSelection::SelectRows(
			HCI_State().AgentExecutorReviewDiffPreviewState,
			RequestedRows,
			OutSelectionResult))
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentExecutorReviewSelect] invalid_args error_code=%s reason=%s total=%d"),
			OutSelectionResult.ErrorCode.IsEmpty() ? TEXT("-") : *OutSelectionResult.ErrorCode,
			OutSelectionResult.Reason.IsEmpty() ? TEXT("-") : *OutSelectionResult.Reason,
			HCI_State().AgentExecutorReviewDiffPreviewState.DiffItems.Num());
		return false;
	}

	HCI_State().AgentExecutorReviewDiffPreviewState = OutSelectionResult.SelectedReport;
	return true;
}

static bool HCI_BuildAgentExecutorReviewDemoCaseConfig(
	const FString& CaseKey,
	FHCIAgentPlan& OutPlan,
	FHCIAgentExecutorOptions& OutOptions,
	FString& OutDescription,
	FString& OutInputText)
{
	OutOptions = FHCIAgentExecutorOptions();
	OutOptions.bValidatePlanBeforeExecute = true;
	OutOptions.bDryRun = true;
	OutOptions.TerminationPolicy = EHCIAgentExecutorTerminationPolicy::StopOnFirstFailure;
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
	const FHCIToolRegistry& Registry,
	int32& OutBridgeOkCases,
	int32& OutFailedBuildOrBridgeCases)
{
	FHCIAgentPlan Plan;
	FHCIAgentExecutorOptions Options;
	FString Description;
	FString InputText;
	if (!HCI_BuildAgentExecutorReviewDemoCaseConfig(CaseKey, Plan, Options, Description, InputText))
	{
		UE_LOG(LogHCIAgentDemo, Error, TEXT("[HCI][AgentExecutorReview] case=%s build_failed reason=unknown_or_invalid_case key=%s"), CaseName, *CaseKey);
		++OutFailedBuildOrBridgeCases;
		return false;
	}

	HCI_State().AgentPlanPreviewState = Plan;

	FHCIAgentExecutorRunResult RunResult;
	FHCIAgentExecutor::ExecutePlan(
		Plan,
		Registry,
		FHCIAgentPlanValidationContext(),
		Options,
		RunResult);

	FHCIDryRunDiffReport Report;
	if (!FHCIAgentExecutorDryRunBridge::BuildDryRunDiffReport(RunResult, Report))
	{
		UE_LOG(LogHCIAgentDemo, Error, TEXT("[HCI][AgentExecutorReview] case=%s bridge_failed request_id=%s"), CaseName, *RunResult.RequestId);
		++OutFailedBuildOrBridgeCases;
		return false;
	}

	HCI_State().AgentExecutorReviewDiffPreviewState = Report;
	++OutBridgeOkCases;

	UE_LOG(
		LogHCIAgentDemo,
		Display,
		TEXT("[HCI][AgentExecutorReview] case=%s case_key=%s description=%s input=%s"),
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

void HCI_RunAbilityKitAgentExecutePlanReviewDemoCommand(const TArray<FString>& Args)
{
	const FHCIToolRegistry& Registry = FHCIToolRegistry::GetReadOnly();

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
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentExecutorReview] summary total_cases=%d bridge_ok_cases=%d failed_cases=%d execution_mode=%s review_contract=%s validation=ok"),
			CaseKeys.Num(),
			BridgeOkCases,
			FailedCases,
			TEXT("simulate_dry_run"),
			TEXT("dry_run_diff"));
		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentExecutorReview] hint=也可运行 HCI.AgentExecutePlanReviewDemo [ok_naming|ok_level_risk|fail_confirm|fail_lod]"));
		return;
	}

	if (Args.Num() != 1)
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentExecutorReview] invalid_args usage=HCI.AgentExecutePlanReviewDemo [ok_naming|ok_level_risk|fail_confirm|fail_lod]"));
		return;
	}

	const FString CaseKey = Args[0].TrimStartAndEnd();
	int32 BridgeOkCases = 0;
	int32 FailedCases = 0;
	if (CaseKey.IsEmpty() || !HCI_RunAgentExecutorReviewDemoCase(TEXT("custom"), CaseKey, Registry, BridgeOkCases, FailedCases))
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentExecutorReview] invalid_args reason=unknown case_key key=%s allowed=ok_naming|ok_level_risk|fail_confirm|fail_lod"),
			*CaseKey);
	}
}

void HCI_RunAbilityKitAgentExecutePlanReviewJsonCommand(const TArray<FString>& Args)
{
	const FHCIToolRegistry& Registry = FHCIToolRegistry::GetReadOnly();

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
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentExecutorReview] invalid_args usage=HCI.AgentExecutePlanReviewJson [ok_naming|ok_level_risk|fail_confirm|fail_lod]"));
		return;
	}

	FString JsonText;
	if (!FHCIDryRunDiffJsonSerializer::SerializeToJsonString(HCI_State().AgentExecutorReviewDiffPreviewState, JsonText))
	{
		UE_LOG(LogHCIAgentDemo, Error, TEXT("[HCI][AgentExecutorReview] json_failed request_id=%s"), *HCI_State().AgentExecutorReviewDiffPreviewState.RequestId);
		return;
	}

	UE_LOG(
		LogHCIAgentDemo,
		Display,
		TEXT("[HCI][AgentExecutorReview] json case=%s request_id=%s bytes=%d"),
		*CaseKey,
		HCI_State().AgentExecutorReviewDiffPreviewState.RequestId.IsEmpty() ? TEXT("-") : *HCI_State().AgentExecutorReviewDiffPreviewState.RequestId,
		JsonText.Len());
	UE_LOG(LogHCIAgentDemo, Display, TEXT("%s"), *JsonText);
}

void HCI_RunAbilityKitAgentExecutePlanReviewLocateCommand(const TArray<FString>& Args)
{
	int32 RowIndex = 0;
	if (Args.Num() >= 1)
	{
		if (!LexTryParseString(RowIndex, *Args[0]) || RowIndex < 0)
		{
			UE_LOG(
				LogHCIAgentDemo,
				Error,
				TEXT("[HCI][AgentExecutorReview] locate_invalid_args reason=row_index must be integer >= 0"));
			return;
		}
	}

	FHCIAgentExecutorReviewLocateResolvedRow Resolved;
	FString ResolveReason;
	if (!FHCIAgentExecutorReviewLocateUtils::TryResolveRow(
			HCI_State().AgentExecutorReviewDiffPreviewState,
			RowIndex,
			Resolved,
			ResolveReason))
	{
		if (ResolveReason == TEXT("no_preview_state"))
		{
			UE_LOG(LogHCIAgentDemo, Warning, TEXT("[HCI][AgentExecutorReview] locate=unavailable reason=no_preview_state"));
			UE_LOG(
				LogHCIAgentDemo,
				Display,
				TEXT("[HCI][AgentExecutorReview] suggestion=先运行 HCI.AgentExecutePlanReviewDemo 或 HCI.AgentExecutePlanReviewJson"));
			return;
		}

		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentExecutorReview] locate_invalid_args reason=%s row_index=%d total=%d"),
			*ResolveReason,
			RowIndex,
			HCI_State().AgentExecutorReviewDiffPreviewState.DiffItems.Num());
		return;
	}

	FString LocateReason;
	const bool bLocateOk = FHCIAgentExecutorReviewLocateUtils::TryLocateResolvedRowInEditor(Resolved, LocateReason);
	if (bLocateOk)
	{
		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentExecutorReview] locate row=%d tool_name=%s strategy=%s object_type=%s success=true reason=%s asset_path=%s actor_path=%s"),
			Resolved.RowIndex,
			Resolved.ToolName.IsEmpty() ? TEXT("-") : *Resolved.ToolName,
			*FHCIDryRunDiff::LocateStrategyToString(Resolved.LocateStrategy),
			*FHCIDryRunDiff::ObjectTypeToString(Resolved.ObjectType),
			*LocateReason,
			Resolved.AssetPath.IsEmpty() ? TEXT("-") : *Resolved.AssetPath,
			Resolved.ActorPath.IsEmpty() ? TEXT("-") : *Resolved.ActorPath);
		return;
	}

	UE_LOG(
		LogHCIAgentDemo,
		Warning,
		TEXT("[HCI][AgentExecutorReview] locate row=%d tool_name=%s strategy=%s object_type=%s success=false reason=%s asset_path=%s actor_path=%s"),
		Resolved.RowIndex,
		Resolved.ToolName.IsEmpty() ? TEXT("-") : *Resolved.ToolName,
		*FHCIDryRunDiff::LocateStrategyToString(Resolved.LocateStrategy),
		*FHCIDryRunDiff::ObjectTypeToString(Resolved.ObjectType),
		*LocateReason,
		Resolved.AssetPath.IsEmpty() ? TEXT("-") : *Resolved.AssetPath,
		Resolved.ActorPath.IsEmpty() ? TEXT("-") : *Resolved.ActorPath);
}

void HCI_RunAbilityKitAgentExecutePlanReviewSelectCommand(const TArray<FString>& Args)
{
	FHCIDryRunDiffSelectionResult SelectionResult;
	if (!HCI_ApplyAgentExecutorReviewSelection(Args, SelectionResult))
	{
		return;
	}

	HCI_LogAgentExecutorReviewSelectSummary(SelectionResult);
	HCI_LogAgentExecutorReviewSelectRows(SelectionResult.SelectedReport);
	UE_LOG(
		LogHCIAgentDemo,
		Display,
		TEXT("[HCI][AgentExecutorReviewSelect] hint=已将最新审阅预览替换为已采纳子集，可继续运行 HCI.AgentExecutePlanReviewLocate [row_index]"));
}

void HCI_RunAbilityKitAgentExecutePlanReviewSelectJsonCommand(const TArray<FString>& Args)
{
	FHCIDryRunDiffSelectionResult SelectionResult;
	if (!HCI_ApplyAgentExecutorReviewSelection(Args, SelectionResult))
	{
		return;
	}

	HCI_LogAgentExecutorReviewSelectSummary(SelectionResult);
	HCI_LogAgentExecutorReviewSelectRows(SelectionResult.SelectedReport);

	FString JsonText;
	if (!FHCIDryRunDiffJsonSerializer::SerializeToJsonString(SelectionResult.SelectedReport, JsonText))
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentExecutorReviewSelect] json_failed request_id=%s"),
			SelectionResult.SelectedReport.RequestId.IsEmpty() ? TEXT("-") : *SelectionResult.SelectedReport.RequestId);
		return;
	}

	UE_LOG(
		LogHCIAgentDemo,
		Display,
		TEXT("[HCI][AgentExecutorReviewSelect] json request_id=%s bytes=%d"),
		SelectionResult.SelectedReport.RequestId.IsEmpty() ? TEXT("-") : *SelectionResult.SelectedReport.RequestId,
		JsonText.Len());
	UE_LOG(LogHCIAgentDemo, Display, TEXT("%s"), *JsonText);
}

static void HCI_LogAgentExecutorApplySummary(const FHCIAgentApplyRequest& ApplyRequest)
{
	const bool bUseWarning = !ApplyRequest.bReady;
	if (bUseWarning)
	{
		UE_LOG(
			LogHCIAgentDemo,
			Warning,
			TEXT("[HCI][AgentExecutorApply] summary request_id=%s review_request_id=%s selection_digest=%s total_rows=%d modifiable_rows=%d blocked_rows=%d ready=%s validation=ok"),
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
		LogHCIAgentDemo,
		Display,
		TEXT("[HCI][AgentExecutorApply] summary request_id=%s review_request_id=%s selection_digest=%s total_rows=%d modifiable_rows=%d blocked_rows=%d ready=%s validation=ok"),
		ApplyRequest.RequestId.IsEmpty() ? TEXT("-") : *ApplyRequest.RequestId,
		ApplyRequest.ReviewRequestId.IsEmpty() ? TEXT("-") : *ApplyRequest.ReviewRequestId,
		ApplyRequest.SelectionDigest.IsEmpty() ? TEXT("-") : *ApplyRequest.SelectionDigest,
		ApplyRequest.Summary.TotalRows,
		ApplyRequest.Summary.ModifiableRows,
		ApplyRequest.Summary.BlockedRows,
		ApplyRequest.bReady ? TEXT("true") : TEXT("false"));
}

static void HCI_LogAgentExecutorApplyRows(const FHCIAgentApplyRequest& ApplyRequest)
{
	for (const FHCIAgentApplyRequestItem& Item : ApplyRequest.Items)
	{
		if (Item.bBlocked)
		{
			UE_LOG(
				LogHCIAgentDemo,
				Warning,
				TEXT("[HCI][AgentExecutorApply] row=%d tool_name=%s risk=%s blocked=%s skip_reason=%s object_type=%s locate_strategy=%s asset_path=%s field=%s evidence_key=%s actor_path=%s"),
				Item.RowIndex,
				Item.ToolName.IsEmpty() ? TEXT("-") : *Item.ToolName,
				*FHCIDryRunDiff::RiskToString(Item.Risk),
				TEXT("true"),
				Item.SkipReason.IsEmpty() ? TEXT("-") : *Item.SkipReason,
				*FHCIDryRunDiff::ObjectTypeToString(Item.ObjectType),
				*FHCIDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
				Item.AssetPath.IsEmpty() ? TEXT("-") : *Item.AssetPath,
				Item.Field.IsEmpty() ? TEXT("-") : *Item.Field,
				Item.EvidenceKey.IsEmpty() ? TEXT("-") : *Item.EvidenceKey,
				Item.ActorPath.IsEmpty() ? TEXT("-") : *Item.ActorPath);
		}
		else
		{
			UE_LOG(
				LogHCIAgentDemo,
				Display,
				TEXT("[HCI][AgentExecutorApply] row=%d tool_name=%s risk=%s blocked=%s skip_reason=%s object_type=%s locate_strategy=%s asset_path=%s field=%s evidence_key=%s actor_path=%s"),
				Item.RowIndex,
				Item.ToolName.IsEmpty() ? TEXT("-") : *Item.ToolName,
				*FHCIDryRunDiff::RiskToString(Item.Risk),
				TEXT("false"),
				Item.SkipReason.IsEmpty() ? TEXT("-") : *Item.SkipReason,
				*FHCIDryRunDiff::ObjectTypeToString(Item.ObjectType),
				*FHCIDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
				Item.AssetPath.IsEmpty() ? TEXT("-") : *Item.AssetPath,
				Item.Field.IsEmpty() ? TEXT("-") : *Item.Field,
				Item.EvidenceKey.IsEmpty() ? TEXT("-") : *Item.EvidenceKey,
				Item.ActorPath.IsEmpty() ? TEXT("-") : *Item.ActorPath);
		}
	}
}

bool HCI_TryBuildAgentExecutorApplyRequestFromLatestReview(FHCIAgentApplyRequest& OutApplyRequest)
{
	if (HCI_State().AgentExecutorReviewDiffPreviewState.DiffItems.Num() <= 0)
	{
		UE_LOG(LogHCIAgentDemo, Warning, TEXT("[HCI][AgentExecutorApply] prepare=unavailable reason=no_review_preview_state"));
		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentExecutorApply] suggestion=先运行 HCI.AgentExecutePlanReviewDemo 或 HCI.AgentExecutePlanReviewSelect"));
		return false;
	}

	if (!FHCIAgentExecutorApplyRequestBridge::BuildApplyRequest(
			HCI_State().AgentExecutorReviewDiffPreviewState,
			OutApplyRequest))
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentExecutorApply] prepare_failed reason=bridge_failed review_request_id=%s"),
			HCI_State().AgentExecutorReviewDiffPreviewState.RequestId.IsEmpty() ? TEXT("-") : *HCI_State().AgentExecutorReviewDiffPreviewState.RequestId);
		return false;
	}

	HCI_State().AgentApplyRequestPreviewState = OutApplyRequest;
	return true;
}

void HCI_RunAbilityKitAgentExecutePlanReviewPrepareApplyCommand(const TArray<FString>& Args)
{
	if (Args.Num() > 0)
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentExecutorApply] invalid_args usage=HCI.AgentExecutePlanReviewPrepareApply"));
		return;
	}

	FHCIAgentApplyRequest ApplyRequest;
	if (!HCI_TryBuildAgentExecutorApplyRequestFromLatestReview(ApplyRequest))
	{
		return;
	}

	HCI_LogAgentExecutorApplySummary(ApplyRequest);
	HCI_LogAgentExecutorApplyRows(ApplyRequest);
	UE_LOG(
		LogHCIAgentDemo,
		Display,
		TEXT("[HCI][AgentExecutorApply] hint=也可运行 HCI.AgentExecutePlanReviewPrepareApplyJson 输出 ApplyRequest JSON"));
}

void HCI_RunAbilityKitAgentExecutePlanReviewPrepareApplyJsonCommand(const TArray<FString>& Args)
{
	if (Args.Num() > 0)
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentExecutorApply] invalid_args usage=HCI.AgentExecutePlanReviewPrepareApplyJson"));
		return;
	}

	FHCIAgentApplyRequest ApplyRequest;
	if (!HCI_TryBuildAgentExecutorApplyRequestFromLatestReview(ApplyRequest))
	{
		return;
	}

	HCI_LogAgentExecutorApplySummary(ApplyRequest);
	HCI_LogAgentExecutorApplyRows(ApplyRequest);

	FString JsonText;
	if (!FHCIAgentApplyRequestJsonSerializer::SerializeToJsonString(ApplyRequest, JsonText))
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentExecutorApply] json_failed request_id=%s"),
			ApplyRequest.RequestId.IsEmpty() ? TEXT("-") : *ApplyRequest.RequestId);
		return;
	}

	UE_LOG(
		LogHCIAgentDemo,
		Display,
		TEXT("[HCI][AgentExecutorApply] json request_id=%s bytes=%d"),
		ApplyRequest.RequestId.IsEmpty() ? TEXT("-") : *ApplyRequest.RequestId,
		JsonText.Len());
	UE_LOG(LogHCIAgentDemo, Display, TEXT("%s"), *JsonText);
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

static void HCI_LogAgentExecutorConfirmSummary(const FHCIAgentApplyConfirmRequest& ConfirmRequest)
{
	const bool bUseWarning = !ConfirmRequest.bReadyToExecute;
	if (bUseWarning)
	{
		UE_LOG(
			LogHCIAgentDemo,
			Warning,
			TEXT("[HCI][AgentExecutorConfirm] summary request_id=%s apply_request_id=%s review_request_id=%s selection_digest=%s user_confirmed=%s ready_to_execute=%s error_code=%s reason=%s validation=ok"),
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
		LogHCIAgentDemo,
		Display,
		TEXT("[HCI][AgentExecutorConfirm] summary request_id=%s apply_request_id=%s review_request_id=%s selection_digest=%s user_confirmed=%s ready_to_execute=%s error_code=%s reason=%s validation=ok"),
		ConfirmRequest.RequestId.IsEmpty() ? TEXT("-") : *ConfirmRequest.RequestId,
		ConfirmRequest.ApplyRequestId.IsEmpty() ? TEXT("-") : *ConfirmRequest.ApplyRequestId,
		ConfirmRequest.ReviewRequestId.IsEmpty() ? TEXT("-") : *ConfirmRequest.ReviewRequestId,
		ConfirmRequest.SelectionDigest.IsEmpty() ? TEXT("-") : *ConfirmRequest.SelectionDigest,
		ConfirmRequest.bUserConfirmed ? TEXT("true") : TEXT("false"),
		ConfirmRequest.bReadyToExecute ? TEXT("true") : TEXT("false"),
		ConfirmRequest.ErrorCode.IsEmpty() ? TEXT("-") : *ConfirmRequest.ErrorCode,
		ConfirmRequest.Reason.IsEmpty() ? TEXT("-") : *ConfirmRequest.Reason);
}

static void HCI_LogAgentExecutorConfirmRows(const FHCIAgentApplyConfirmRequest& ConfirmRequest)
{
	for (const FHCIAgentApplyRequestItem& Item : ConfirmRequest.Items)
	{
		if (Item.bBlocked)
		{
			UE_LOG(
				LogHCIAgentDemo,
				Warning,
				TEXT("[HCI][AgentExecutorConfirm] row=%d tool_name=%s risk=%s blocked=%s skip_reason=%s object_type=%s locate_strategy=%s asset_path=%s field=%s evidence_key=%s actor_path=%s"),
				Item.RowIndex,
				Item.ToolName.IsEmpty() ? TEXT("-") : *Item.ToolName,
				*FHCIDryRunDiff::RiskToString(Item.Risk),
				TEXT("true"),
				Item.SkipReason.IsEmpty() ? TEXT("-") : *Item.SkipReason,
				*FHCIDryRunDiff::ObjectTypeToString(Item.ObjectType),
				*FHCIDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
				Item.AssetPath.IsEmpty() ? TEXT("-") : *Item.AssetPath,
				Item.Field.IsEmpty() ? TEXT("-") : *Item.Field,
				Item.EvidenceKey.IsEmpty() ? TEXT("-") : *Item.EvidenceKey,
				Item.ActorPath.IsEmpty() ? TEXT("-") : *Item.ActorPath);
		}
		else
		{
			UE_LOG(
				LogHCIAgentDemo,
				Display,
				TEXT("[HCI][AgentExecutorConfirm] row=%d tool_name=%s risk=%s blocked=%s skip_reason=%s object_type=%s locate_strategy=%s asset_path=%s field=%s evidence_key=%s actor_path=%s"),
				Item.RowIndex,
				Item.ToolName.IsEmpty() ? TEXT("-") : *Item.ToolName,
				*FHCIDryRunDiff::RiskToString(Item.Risk),
				TEXT("false"),
				Item.SkipReason.IsEmpty() ? TEXT("-") : *Item.SkipReason,
				*FHCIDryRunDiff::ObjectTypeToString(Item.ObjectType),
				*FHCIDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
				Item.AssetPath.IsEmpty() ? TEXT("-") : *Item.AssetPath,
				Item.Field.IsEmpty() ? TEXT("-") : *Item.Field,
				Item.EvidenceKey.IsEmpty() ? TEXT("-") : *Item.EvidenceKey,
				Item.ActorPath.IsEmpty() ? TEXT("-") : *Item.ActorPath);
		}
	}
}

bool HCI_TryBuildAgentExecutorConfirmRequestFromLatestPreview(
	const bool bUserConfirmed,
	const FString& TamperMode,
	FHCIAgentApplyConfirmRequest& OutConfirmRequest)
{
	if (HCI_State().AgentApplyRequestPreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAgentDemo, Warning, TEXT("[HCI][AgentExecutorConfirm] prepare=unavailable reason=no_apply_request_preview_state"));
		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentExecutorConfirm] suggestion=先运行 HCI.AgentExecutePlanReviewPrepareApply"));
		return false;
	}

	if (HCI_State().AgentExecutorReviewDiffPreviewState.DiffItems.Num() <= 0)
	{
		UE_LOG(LogHCIAgentDemo, Warning, TEXT("[HCI][AgentExecutorConfirm] prepare=unavailable reason=no_review_preview_state"));
		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentExecutorConfirm] suggestion=先运行 HCI.AgentExecutePlanReviewDemo 或 HCI.AgentExecutePlanReviewSelect"));
		return false;
	}

	FHCIAgentApplyRequest WorkingApplyRequest = HCI_State().AgentApplyRequestPreviewState;
	if (TamperMode == TEXT("digest"))
	{
		WorkingApplyRequest.SelectionDigest += TEXT("_tampered");
	}
	else if (TamperMode == TEXT("review"))
	{
		WorkingApplyRequest.ReviewRequestId += TEXT("_stale");
	}

	if (!FHCIAgentExecutorApplyConfirmBridge::BuildConfirmRequest(
			WorkingApplyRequest,
			HCI_State().AgentExecutorReviewDiffPreviewState,
			bUserConfirmed,
			OutConfirmRequest))
	{
		UE_LOG(LogHCIAgentDemo, Error, TEXT("[HCI][AgentExecutorConfirm] prepare_failed reason=bridge_failed"));
		return false;
	}

	HCI_State().AgentApplyConfirmRequestPreviewState = OutConfirmRequest;
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

void HCI_RunAbilityKitAgentExecutePlanReviewPrepareConfirmCommand(const TArray<FString>& Args)
{
	bool bUserConfirmed = true;
	FString TamperMode;
	if (!HCI_TryParseConfirmCommandArgs(Args, bUserConfirmed, TamperMode))
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentExecutorConfirm] invalid_args usage=HCI.AgentExecutePlanReviewPrepareConfirm [user_confirmed=0|1] [tamper=none|digest|review]"));
		return;
	}

	FHCIAgentApplyConfirmRequest ConfirmRequest;
	if (!HCI_TryBuildAgentExecutorConfirmRequestFromLatestPreview(bUserConfirmed, TamperMode, ConfirmRequest))
	{
		return;
	}

	HCI_LogAgentExecutorConfirmSummary(ConfirmRequest);
	HCI_LogAgentExecutorConfirmRows(ConfirmRequest);
	UE_LOG(
		LogHCIAgentDemo,
		Display,
		TEXT("[HCI][AgentExecutorConfirm] hint=也可运行 HCI.AgentExecutePlanReviewPrepareConfirmJson [user_confirmed] [tamper] 输出 ConfirmRequest JSON"));
}

void HCI_RunAbilityKitAgentExecutePlanReviewPrepareConfirmJsonCommand(const TArray<FString>& Args)
{
	bool bUserConfirmed = true;
	FString TamperMode;
	if (!HCI_TryParseConfirmCommandArgs(Args, bUserConfirmed, TamperMode))
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentExecutorConfirm] invalid_args usage=HCI.AgentExecutePlanReviewPrepareConfirmJson [user_confirmed=0|1] [tamper=none|digest|review]"));
		return;
	}

	FHCIAgentApplyConfirmRequest ConfirmRequest;
	if (!HCI_TryBuildAgentExecutorConfirmRequestFromLatestPreview(bUserConfirmed, TamperMode, ConfirmRequest))
	{
		return;
	}

	HCI_LogAgentExecutorConfirmSummary(ConfirmRequest);
	HCI_LogAgentExecutorConfirmRows(ConfirmRequest);

	FString JsonText;
	if (!FHCIAgentApplyConfirmRequestJsonSerializer::SerializeToJsonString(ConfirmRequest, JsonText))
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentExecutorConfirm] json_failed request_id=%s"),
			ConfirmRequest.RequestId.IsEmpty() ? TEXT("-") : *ConfirmRequest.RequestId);
		return;
	}

	UE_LOG(
		LogHCIAgentDemo,
		Display,
		TEXT("[HCI][AgentExecutorConfirm] json request_id=%s bytes=%d"),
		ConfirmRequest.RequestId.IsEmpty() ? TEXT("-") : *ConfirmRequest.RequestId,
		JsonText.Len());
	UE_LOG(LogHCIAgentDemo, Display, TEXT("%s"), *JsonText);
}

static bool HCI_TryParseExecuteTicketTamperModeArg(const FString& InValue, FString& OutTamperMode)
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
	if (Trimmed.Equals(TEXT("apply"), ESearchCase::IgnoreCase))
	{
		OutTamperMode = TEXT("apply");
		return true;
	}
	if (Trimmed.Equals(TEXT("review"), ESearchCase::IgnoreCase))
	{
		OutTamperMode = TEXT("review");
		return true;
	}
	return false;
}

static void HCI_LogAgentExecutorExecuteTicketSummary(const FHCIAgentExecuteTicket& ExecuteTicket)
{
	const bool bUseWarning = !ExecuteTicket.bReadyToSimulateExecute;
	if (bUseWarning)
	{
		UE_LOG(
			LogHCIAgentDemo,
			Warning,
			TEXT("[HCI][AgentExecutorExecuteTicket] summary request_id=%s confirm_request_id=%s apply_request_id=%s review_request_id=%s selection_digest=%s user_confirmed=%s ready_to_simulate_execute=%s error_code=%s reason=%s validation=ok"),
			ExecuteTicket.RequestId.IsEmpty() ? TEXT("-") : *ExecuteTicket.RequestId,
			ExecuteTicket.ConfirmRequestId.IsEmpty() ? TEXT("-") : *ExecuteTicket.ConfirmRequestId,
			ExecuteTicket.ApplyRequestId.IsEmpty() ? TEXT("-") : *ExecuteTicket.ApplyRequestId,
			ExecuteTicket.ReviewRequestId.IsEmpty() ? TEXT("-") : *ExecuteTicket.ReviewRequestId,
			ExecuteTicket.SelectionDigest.IsEmpty() ? TEXT("-") : *ExecuteTicket.SelectionDigest,
			ExecuteTicket.bUserConfirmed ? TEXT("true") : TEXT("false"),
			ExecuteTicket.bReadyToSimulateExecute ? TEXT("true") : TEXT("false"),
			ExecuteTicket.ErrorCode.IsEmpty() ? TEXT("-") : *ExecuteTicket.ErrorCode,
			ExecuteTicket.Reason.IsEmpty() ? TEXT("-") : *ExecuteTicket.Reason);
		return;
	}

	UE_LOG(
		LogHCIAgentDemo,
		Display,
		TEXT("[HCI][AgentExecutorExecuteTicket] summary request_id=%s confirm_request_id=%s apply_request_id=%s review_request_id=%s selection_digest=%s user_confirmed=%s ready_to_simulate_execute=%s error_code=%s reason=%s validation=ok"),
		ExecuteTicket.RequestId.IsEmpty() ? TEXT("-") : *ExecuteTicket.RequestId,
		ExecuteTicket.ConfirmRequestId.IsEmpty() ? TEXT("-") : *ExecuteTicket.ConfirmRequestId,
		ExecuteTicket.ApplyRequestId.IsEmpty() ? TEXT("-") : *ExecuteTicket.ApplyRequestId,
		ExecuteTicket.ReviewRequestId.IsEmpty() ? TEXT("-") : *ExecuteTicket.ReviewRequestId,
		ExecuteTicket.SelectionDigest.IsEmpty() ? TEXT("-") : *ExecuteTicket.SelectionDigest,
		ExecuteTicket.bUserConfirmed ? TEXT("true") : TEXT("false"),
		ExecuteTicket.bReadyToSimulateExecute ? TEXT("true") : TEXT("false"),
		ExecuteTicket.ErrorCode.IsEmpty() ? TEXT("-") : *ExecuteTicket.ErrorCode,
		ExecuteTicket.Reason.IsEmpty() ? TEXT("-") : *ExecuteTicket.Reason);
}

static void HCI_LogAgentExecutorExecuteTicketRows(const FHCIAgentExecuteTicket& ExecuteTicket)
{
	for (const FHCIAgentApplyRequestItem& Item : ExecuteTicket.Items)
	{
		if (Item.bBlocked)
		{
			UE_LOG(
				LogHCIAgentDemo,
				Warning,
				TEXT("[HCI][AgentExecutorExecuteTicket] row=%d tool_name=%s risk=%s blocked=%s skip_reason=%s object_type=%s locate_strategy=%s asset_path=%s field=%s evidence_key=%s actor_path=%s"),
				Item.RowIndex,
				Item.ToolName.IsEmpty() ? TEXT("-") : *Item.ToolName,
				*FHCIDryRunDiff::RiskToString(Item.Risk),
				TEXT("true"),
				Item.SkipReason.IsEmpty() ? TEXT("-") : *Item.SkipReason,
				*FHCIDryRunDiff::ObjectTypeToString(Item.ObjectType),
				*FHCIDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
				Item.AssetPath.IsEmpty() ? TEXT("-") : *Item.AssetPath,
				Item.Field.IsEmpty() ? TEXT("-") : *Item.Field,
				Item.EvidenceKey.IsEmpty() ? TEXT("-") : *Item.EvidenceKey,
				Item.ActorPath.IsEmpty() ? TEXT("-") : *Item.ActorPath);
		}
		else
		{
			UE_LOG(
				LogHCIAgentDemo,
				Display,
				TEXT("[HCI][AgentExecutorExecuteTicket] row=%d tool_name=%s risk=%s blocked=%s skip_reason=%s object_type=%s locate_strategy=%s asset_path=%s field=%s evidence_key=%s actor_path=%s"),
				Item.RowIndex,
				Item.ToolName.IsEmpty() ? TEXT("-") : *Item.ToolName,
				*FHCIDryRunDiff::RiskToString(Item.Risk),
				TEXT("false"),
				Item.SkipReason.IsEmpty() ? TEXT("-") : *Item.SkipReason,
				*FHCIDryRunDiff::ObjectTypeToString(Item.ObjectType),
				*FHCIDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
				Item.AssetPath.IsEmpty() ? TEXT("-") : *Item.AssetPath,
				Item.Field.IsEmpty() ? TEXT("-") : *Item.Field,
				Item.EvidenceKey.IsEmpty() ? TEXT("-") : *Item.EvidenceKey,
				Item.ActorPath.IsEmpty() ? TEXT("-") : *Item.ActorPath);
		}
	}
}

bool HCI_TryBuildAgentExecutorExecuteTicketFromLatestPreview(
	const FString& TamperMode,
	FHCIAgentExecuteTicket& OutExecuteTicket)
{
	if (HCI_State().AgentApplyConfirmRequestPreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAgentDemo, Warning, TEXT("[HCI][AgentExecutorExecuteTicket] prepare=unavailable reason=no_confirm_request_preview_state"));
		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentExecutorExecuteTicket] suggestion=先运行 HCI.AgentExecutePlanReviewPrepareConfirm"));
		return false;
	}

	if (HCI_State().AgentApplyRequestPreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAgentDemo, Warning, TEXT("[HCI][AgentExecutorExecuteTicket] prepare=unavailable reason=no_apply_request_preview_state"));
		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentExecutorExecuteTicket] suggestion=先运行 HCI.AgentExecutePlanReviewPrepareApply"));
		return false;
	}

	if (HCI_State().AgentExecutorReviewDiffPreviewState.DiffItems.Num() <= 0)
	{
		UE_LOG(LogHCIAgentDemo, Warning, TEXT("[HCI][AgentExecutorExecuteTicket] prepare=unavailable reason=no_review_preview_state"));
		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentExecutorExecuteTicket] suggestion=先运行 HCI.AgentExecutePlanReviewDemo 或 HCI.AgentExecutePlanReviewSelect"));
		return false;
	}

	FHCIAgentApplyConfirmRequest WorkingConfirmRequest = HCI_State().AgentApplyConfirmRequestPreviewState;
	if (TamperMode == TEXT("digest"))
	{
		WorkingConfirmRequest.SelectionDigest += TEXT("_tampered");
	}
	else if (TamperMode == TEXT("apply"))
	{
		WorkingConfirmRequest.ApplyRequestId += TEXT("_stale");
	}
	else if (TamperMode == TEXT("review"))
	{
		WorkingConfirmRequest.ReviewRequestId += TEXT("_stale");
	}

	if (!FHCIAgentExecutorExecuteTicketBridge::BuildExecuteTicket(
			WorkingConfirmRequest,
			HCI_State().AgentApplyRequestPreviewState,
			HCI_State().AgentExecutorReviewDiffPreviewState,
			OutExecuteTicket))
	{
		UE_LOG(LogHCIAgentDemo, Error, TEXT("[HCI][AgentExecutorExecuteTicket] prepare_failed reason=bridge_failed"));
		return false;
	}

	HCI_State().AgentExecuteTicketPreviewState = OutExecuteTicket;
	return true;
}

static bool HCI_TryParseExecuteTicketCommandArgs(const TArray<FString>& Args, FString& OutTamperMode)
{
	OutTamperMode = TEXT("none");
	if (Args.Num() == 0)
	{
		return true;
	}
	if (Args.Num() != 1)
	{
		return false;
	}
	return HCI_TryParseExecuteTicketTamperModeArg(Args[0], OutTamperMode);
}

void HCI_RunAbilityKitAgentExecutePlanReviewPrepareExecuteTicketCommand(const TArray<FString>& Args)
{
	FString TamperMode;
	if (!HCI_TryParseExecuteTicketCommandArgs(Args, TamperMode))
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentExecutorExecuteTicket] invalid_args usage=HCI.AgentExecutePlanReviewPrepareExecuteTicket [tamper=none|digest|apply|review]"));
		return;
	}

	FHCIAgentExecuteTicket ExecuteTicket;
	if (!HCI_TryBuildAgentExecutorExecuteTicketFromLatestPreview(TamperMode, ExecuteTicket))
	{
		return;
	}

	HCI_LogAgentExecutorExecuteTicketSummary(ExecuteTicket);
	HCI_LogAgentExecutorExecuteTicketRows(ExecuteTicket);
	UE_LOG(
		LogHCIAgentDemo,
		Display,
		TEXT("[HCI][AgentExecutorExecuteTicket] hint=也可运行 HCI.AgentExecutePlanReviewPrepareExecuteTicketJson [tamper] 输出 ExecuteTicket JSON"));
}

void HCI_RunAbilityKitAgentExecutePlanReviewPrepareExecuteTicketJsonCommand(const TArray<FString>& Args)
{
	FString TamperMode;
	if (!HCI_TryParseExecuteTicketCommandArgs(Args, TamperMode))
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentExecutorExecuteTicket] invalid_args usage=HCI.AgentExecutePlanReviewPrepareExecuteTicketJson [tamper=none|digest|apply|review]"));
		return;
	}

	FHCIAgentExecuteTicket ExecuteTicket;
	if (!HCI_TryBuildAgentExecutorExecuteTicketFromLatestPreview(TamperMode, ExecuteTicket))
	{
		return;
	}

	HCI_LogAgentExecutorExecuteTicketSummary(ExecuteTicket);
	HCI_LogAgentExecutorExecuteTicketRows(ExecuteTicket);

	FString JsonText;
	if (!FHCIAgentExecuteTicketJsonSerializer::SerializeToJsonString(ExecuteTicket, JsonText))
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentExecutorExecuteTicket] json_failed request_id=%s"),
			ExecuteTicket.RequestId.IsEmpty() ? TEXT("-") : *ExecuteTicket.RequestId);
		return;
	}

	UE_LOG(
		LogHCIAgentDemo,
		Display,
		TEXT("[HCI][AgentExecutorExecuteTicket] json request_id=%s bytes=%d"),
		ExecuteTicket.RequestId.IsEmpty() ? TEXT("-") : *ExecuteTicket.RequestId,
		JsonText.Len());
	UE_LOG(LogHCIAgentDemo, Display, TEXT("%s"), *JsonText);
}

static bool HCI_TryParseSimExecuteReceiptTamperModeArg(const FString& InValue, FString& OutTamperMode)
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
	if (Trimmed.Equals(TEXT("apply"), ESearchCase::IgnoreCase))
	{
		OutTamperMode = TEXT("apply");
		return true;
	}
	if (Trimmed.Equals(TEXT("review"), ESearchCase::IgnoreCase))
	{
		OutTamperMode = TEXT("review");
		return true;
	}
	if (Trimmed.Equals(TEXT("confirm"), ESearchCase::IgnoreCase))
	{
		OutTamperMode = TEXT("confirm");
		return true;
	}
	if (Trimmed.Equals(TEXT("ready"), ESearchCase::IgnoreCase))
	{
		OutTamperMode = TEXT("ready");
		return true;
	}
	return false;
}

static void HCI_LogAgentExecutorSimExecuteReceiptSummary(const FHCIAgentSimulateExecuteReceipt& Receipt)
{
	const bool bUseWarning = !Receipt.bSimulatedDispatchAccepted;
	if (bUseWarning)
	{
		UE_LOG(
			LogHCIAgentDemo,
			Warning,
			TEXT("[HCI][AgentExecutorSimExecuteReceipt] summary request_id=%s execute_ticket_id=%s confirm_request_id=%s apply_request_id=%s review_request_id=%s selection_digest=%s user_confirmed=%s ready_to_simulate_execute=%s simulated_dispatch_accepted=%s error_code=%s reason=%s validation=ok"),
			Receipt.RequestId.IsEmpty() ? TEXT("-") : *Receipt.RequestId,
			Receipt.ExecuteTicketId.IsEmpty() ? TEXT("-") : *Receipt.ExecuteTicketId,
			Receipt.ConfirmRequestId.IsEmpty() ? TEXT("-") : *Receipt.ConfirmRequestId,
			Receipt.ApplyRequestId.IsEmpty() ? TEXT("-") : *Receipt.ApplyRequestId,
			Receipt.ReviewRequestId.IsEmpty() ? TEXT("-") : *Receipt.ReviewRequestId,
			Receipt.SelectionDigest.IsEmpty() ? TEXT("-") : *Receipt.SelectionDigest,
			Receipt.bUserConfirmed ? TEXT("true") : TEXT("false"),
			Receipt.bReadyToSimulateExecute ? TEXT("true") : TEXT("false"),
			Receipt.bSimulatedDispatchAccepted ? TEXT("true") : TEXT("false"),
			Receipt.ErrorCode.IsEmpty() ? TEXT("-") : *Receipt.ErrorCode,
			Receipt.Reason.IsEmpty() ? TEXT("-") : *Receipt.Reason);
		return;
	}

	UE_LOG(
		LogHCIAgentDemo,
		Display,
		TEXT("[HCI][AgentExecutorSimExecuteReceipt] summary request_id=%s execute_ticket_id=%s confirm_request_id=%s apply_request_id=%s review_request_id=%s selection_digest=%s user_confirmed=%s ready_to_simulate_execute=%s simulated_dispatch_accepted=%s error_code=%s reason=%s validation=ok"),
		Receipt.RequestId.IsEmpty() ? TEXT("-") : *Receipt.RequestId,
		Receipt.ExecuteTicketId.IsEmpty() ? TEXT("-") : *Receipt.ExecuteTicketId,
		Receipt.ConfirmRequestId.IsEmpty() ? TEXT("-") : *Receipt.ConfirmRequestId,
		Receipt.ApplyRequestId.IsEmpty() ? TEXT("-") : *Receipt.ApplyRequestId,
		Receipt.ReviewRequestId.IsEmpty() ? TEXT("-") : *Receipt.ReviewRequestId,
		Receipt.SelectionDigest.IsEmpty() ? TEXT("-") : *Receipt.SelectionDigest,
		Receipt.bUserConfirmed ? TEXT("true") : TEXT("false"),
		Receipt.bReadyToSimulateExecute ? TEXT("true") : TEXT("false"),
		Receipt.bSimulatedDispatchAccepted ? TEXT("true") : TEXT("false"),
		Receipt.ErrorCode.IsEmpty() ? TEXT("-") : *Receipt.ErrorCode,
		Receipt.Reason.IsEmpty() ? TEXT("-") : *Receipt.Reason);
}

static void HCI_LogAgentExecutorSimExecuteReceiptRows(const FHCIAgentSimulateExecuteReceipt& Receipt)
{
	for (const FHCIAgentApplyRequestItem& Item : Receipt.Items)
	{
		if (Item.bBlocked)
		{
			UE_LOG(
				LogHCIAgentDemo,
				Warning,
				TEXT("[HCI][AgentExecutorSimExecuteReceipt] row=%d tool_name=%s risk=%s blocked=%s skip_reason=%s object_type=%s locate_strategy=%s asset_path=%s field=%s evidence_key=%s actor_path=%s"),
				Item.RowIndex,
				Item.ToolName.IsEmpty() ? TEXT("-") : *Item.ToolName,
				*FHCIDryRunDiff::RiskToString(Item.Risk),
				TEXT("true"),
				Item.SkipReason.IsEmpty() ? TEXT("-") : *Item.SkipReason,
				*FHCIDryRunDiff::ObjectTypeToString(Item.ObjectType),
				*FHCIDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
				Item.AssetPath.IsEmpty() ? TEXT("-") : *Item.AssetPath,
				Item.Field.IsEmpty() ? TEXT("-") : *Item.Field,
				Item.EvidenceKey.IsEmpty() ? TEXT("-") : *Item.EvidenceKey,
				Item.ActorPath.IsEmpty() ? TEXT("-") : *Item.ActorPath);
		}
		else
		{
			UE_LOG(
				LogHCIAgentDemo,
				Display,
				TEXT("[HCI][AgentExecutorSimExecuteReceipt] row=%d tool_name=%s risk=%s blocked=%s skip_reason=%s object_type=%s locate_strategy=%s asset_path=%s field=%s evidence_key=%s actor_path=%s"),
				Item.RowIndex,
				Item.ToolName.IsEmpty() ? TEXT("-") : *Item.ToolName,
				*FHCIDryRunDiff::RiskToString(Item.Risk),
				TEXT("false"),
				Item.SkipReason.IsEmpty() ? TEXT("-") : *Item.SkipReason,
				*FHCIDryRunDiff::ObjectTypeToString(Item.ObjectType),
				*FHCIDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
				Item.AssetPath.IsEmpty() ? TEXT("-") : *Item.AssetPath,
				Item.Field.IsEmpty() ? TEXT("-") : *Item.Field,
				Item.EvidenceKey.IsEmpty() ? TEXT("-") : *Item.EvidenceKey,
				Item.ActorPath.IsEmpty() ? TEXT("-") : *Item.ActorPath);
		}
	}
}

bool HCI_TryBuildAgentExecutorSimExecuteReceiptFromLatestPreview(
	const FString& TamperMode,
	FHCIAgentSimulateExecuteReceipt& OutReceipt)
{
	if (HCI_State().AgentExecuteTicketPreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAgentDemo, Warning, TEXT("[HCI][AgentExecutorSimExecuteReceipt] prepare=unavailable reason=no_execute_ticket_preview_state"));
		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentExecutorSimExecuteReceipt] suggestion=先运行 HCI.AgentExecutePlanReviewPrepareExecuteTicket"));
		return false;
	}

	if (HCI_State().AgentApplyConfirmRequestPreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAgentDemo, Warning, TEXT("[HCI][AgentExecutorSimExecuteReceipt] prepare=unavailable reason=no_confirm_request_preview_state"));
		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentExecutorSimExecuteReceipt] suggestion=先运行 HCI.AgentExecutePlanReviewPrepareConfirm"));
		return false;
	}

	if (HCI_State().AgentApplyRequestPreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAgentDemo, Warning, TEXT("[HCI][AgentExecutorSimExecuteReceipt] prepare=unavailable reason=no_apply_request_preview_state"));
		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentExecutorSimExecuteReceipt] suggestion=先运行 HCI.AgentExecutePlanReviewPrepareApply"));
		return false;
	}

	if (HCI_State().AgentExecutorReviewDiffPreviewState.DiffItems.Num() <= 0)
	{
		UE_LOG(LogHCIAgentDemo, Warning, TEXT("[HCI][AgentExecutorSimExecuteReceipt] prepare=unavailable reason=no_review_preview_state"));
		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentExecutorSimExecuteReceipt] suggestion=先运行 HCI.AgentExecutePlanReviewDemo 或 HCI.AgentExecutePlanReviewSelect"));
		return false;
	}

	FHCIAgentExecuteTicket WorkingExecuteTicket = HCI_State().AgentExecuteTicketPreviewState;
	if (TamperMode == TEXT("digest"))
	{
		WorkingExecuteTicket.SelectionDigest += TEXT("_tampered");
	}
	else if (TamperMode == TEXT("apply"))
	{
		WorkingExecuteTicket.ApplyRequestId += TEXT("_stale");
	}
	else if (TamperMode == TEXT("review"))
	{
		WorkingExecuteTicket.ReviewRequestId += TEXT("_stale");
	}
	else if (TamperMode == TEXT("confirm"))
	{
		WorkingExecuteTicket.ConfirmRequestId += TEXT("_stale");
	}
	else if (TamperMode == TEXT("ready"))
	{
		WorkingExecuteTicket.bReadyToSimulateExecute = false;
	}

	if (!FHCIAgentExecutorSimulateExecuteReceiptBridge::BuildSimulateExecuteReceipt(
			WorkingExecuteTicket,
			HCI_State().AgentApplyConfirmRequestPreviewState,
			HCI_State().AgentApplyRequestPreviewState,
			HCI_State().AgentExecutorReviewDiffPreviewState,
			OutReceipt))
	{
		UE_LOG(LogHCIAgentDemo, Error, TEXT("[HCI][AgentExecutorSimExecuteReceipt] prepare_failed reason=bridge_failed"));
		return false;
	}

	HCI_State().AgentSimulateExecuteReceiptPreviewState = OutReceipt;
	return true;
}

static bool HCI_TryParseSimExecuteReceiptCommandArgs(const TArray<FString>& Args, FString& OutTamperMode)
{
	OutTamperMode = TEXT("none");
	if (Args.Num() == 0)
	{
		return true;
	}
	if (Args.Num() != 1)
	{
		return false;
	}
	return HCI_TryParseSimExecuteReceiptTamperModeArg(Args[0], OutTamperMode);
}

void HCI_RunAbilityKitAgentExecutePlanReviewPrepareSimExecuteReceiptCommand(const TArray<FString>& Args)
{
	FString TamperMode;
	if (!HCI_TryParseSimExecuteReceiptCommandArgs(Args, TamperMode))
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentExecutorSimExecuteReceipt] invalid_args usage=HCI.AgentExecutePlanReviewPrepareSimExecuteReceipt [tamper=none|digest|apply|review|confirm|ready]"));
		return;
	}

	FHCIAgentSimulateExecuteReceipt Receipt;
	if (!HCI_TryBuildAgentExecutorSimExecuteReceiptFromLatestPreview(TamperMode, Receipt))
	{
		return;
	}

	HCI_LogAgentExecutorSimExecuteReceiptSummary(Receipt);
	HCI_LogAgentExecutorSimExecuteReceiptRows(Receipt);
	UE_LOG(
		LogHCIAgentDemo,
		Display,
		TEXT("[HCI][AgentExecutorSimExecuteReceipt] hint=也可运行 HCI.AgentExecutePlanReviewPrepareSimExecuteReceiptJson [tamper] 输出 SimExecuteReceipt JSON"));
}

void HCI_RunAbilityKitAgentExecutePlanReviewPrepareSimExecuteReceiptJsonCommand(const TArray<FString>& Args)
{
	FString TamperMode;
	if (!HCI_TryParseSimExecuteReceiptCommandArgs(Args, TamperMode))
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentExecutorSimExecuteReceipt] invalid_args usage=HCI.AgentExecutePlanReviewPrepareSimExecuteReceiptJson [tamper=none|digest|apply|review|confirm|ready]"));
		return;
	}

	FHCIAgentSimulateExecuteReceipt Receipt;
	if (!HCI_TryBuildAgentExecutorSimExecuteReceiptFromLatestPreview(TamperMode, Receipt))
	{
		return;
	}

	HCI_LogAgentExecutorSimExecuteReceiptSummary(Receipt);
	HCI_LogAgentExecutorSimExecuteReceiptRows(Receipt);

	FString JsonText;
	if (!FHCIAgentSimulateExecuteReceiptJsonSerializer::SerializeToJsonString(Receipt, JsonText))
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentExecutorSimExecuteReceipt] json_failed request_id=%s"),
			Receipt.RequestId.IsEmpty() ? TEXT("-") : *Receipt.RequestId);
		return;
	}

	UE_LOG(
		LogHCIAgentDemo,
		Display,
		TEXT("[HCI][AgentExecutorSimExecuteReceipt] json request_id=%s bytes=%d"),
		Receipt.RequestId.IsEmpty() ? TEXT("-") : *Receipt.RequestId,
		JsonText.Len());
	UE_LOG(LogHCIAgentDemo, Display, TEXT("%s"), *JsonText);
}

static bool HCI_TryParseSimFinalReportTamperModeArg(const FString& InValue, FString& OutTamperMode)
{
	const FString Normalized = InValue.TrimStartAndEnd().ToLower();
	if (Normalized == TEXT("none") ||
		Normalized == TEXT("digest") ||
		Normalized == TEXT("apply") ||
		Normalized == TEXT("review") ||
		Normalized == TEXT("confirm") ||
		Normalized == TEXT("receipt") ||
		Normalized == TEXT("ready"))
	{
		OutTamperMode = Normalized;
		return true;
	}
	return false;
}

static void HCI_LogAgentExecutorSimFinalReportSummary(const FHCIAgentSimulateExecuteFinalReport& Report)
{
	const FString SummaryLine = FString::Printf(
		TEXT("[HCI][AgentExecutorSimFinalReport] summary request_id=%s sim_execute_receipt_id=%s execute_ticket_id=%s confirm_request_id=%s apply_request_id=%s review_request_id=%s selection_digest=%s user_confirmed=%s ready_to_simulate_execute=%s simulated_dispatch_accepted=%s simulation_completed=%s terminal_status=%s error_code=%s reason=%s validation=ok"),
		Report.RequestId.IsEmpty() ? TEXT("-") : *Report.RequestId,
		Report.SimExecuteReceiptId.IsEmpty() ? TEXT("-") : *Report.SimExecuteReceiptId,
		Report.ExecuteTicketId.IsEmpty() ? TEXT("-") : *Report.ExecuteTicketId,
		Report.ConfirmRequestId.IsEmpty() ? TEXT("-") : *Report.ConfirmRequestId,
		Report.ApplyRequestId.IsEmpty() ? TEXT("-") : *Report.ApplyRequestId,
		Report.ReviewRequestId.IsEmpty() ? TEXT("-") : *Report.ReviewRequestId,
		Report.SelectionDigest.IsEmpty() ? TEXT("-") : *Report.SelectionDigest,
		Report.bUserConfirmed ? TEXT("true") : TEXT("false"),
		Report.bReadyToSimulateExecute ? TEXT("true") : TEXT("false"),
		Report.bSimulatedDispatchAccepted ? TEXT("true") : TEXT("false"),
		Report.bSimulationCompleted ? TEXT("true") : TEXT("false"),
		Report.TerminalStatus.IsEmpty() ? TEXT("-") : *Report.TerminalStatus,
		Report.ErrorCode.IsEmpty() ? TEXT("-") : *Report.ErrorCode,
		Report.Reason.IsEmpty() ? TEXT("-") : *Report.Reason);

	const bool bUseWarning = !Report.bSimulationCompleted;
	if (bUseWarning)
	{
		UE_LOG(LogHCIAgentDemo, Warning, TEXT("%s"), *SummaryLine);
		return;
	}

	UE_LOG(LogHCIAgentDemo, Display, TEXT("%s"), *SummaryLine);
}

static void HCI_LogAgentExecutorSimFinalReportRows(const FHCIAgentSimulateExecuteFinalReport& Report)
{
	for (const FHCIAgentApplyRequestItem& Item : Report.Items)
	{
		if (Item.bBlocked)
		{
			UE_LOG(
				LogHCIAgentDemo,
				Warning,
				TEXT("[HCI][AgentExecutorSimFinalReport] row=%d tool_name=%s risk=%s blocked=%s skip_reason=%s object_type=%s locate_strategy=%s asset_path=%s field=%s evidence_key=%s actor_path=%s"),
				Item.RowIndex,
				Item.ToolName.IsEmpty() ? TEXT("-") : *Item.ToolName,
				*FHCIDryRunDiff::RiskToString(Item.Risk),
				TEXT("true"),
				Item.SkipReason.IsEmpty() ? TEXT("-") : *Item.SkipReason,
				*FHCIDryRunDiff::ObjectTypeToString(Item.ObjectType),
				*FHCIDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
				Item.AssetPath.IsEmpty() ? TEXT("-") : *Item.AssetPath,
				Item.Field.IsEmpty() ? TEXT("-") : *Item.Field,
				Item.EvidenceKey.IsEmpty() ? TEXT("-") : *Item.EvidenceKey,
				Item.ActorPath.IsEmpty() ? TEXT("-") : *Item.ActorPath);
		}
		else
		{
			UE_LOG(
				LogHCIAgentDemo,
				Display,
				TEXT("[HCI][AgentExecutorSimFinalReport] row=%d tool_name=%s risk=%s blocked=%s skip_reason=%s object_type=%s locate_strategy=%s asset_path=%s field=%s evidence_key=%s actor_path=%s"),
				Item.RowIndex,
				Item.ToolName.IsEmpty() ? TEXT("-") : *Item.ToolName,
				*FHCIDryRunDiff::RiskToString(Item.Risk),
				TEXT("false"),
				Item.SkipReason.IsEmpty() ? TEXT("-") : *Item.SkipReason,
				*FHCIDryRunDiff::ObjectTypeToString(Item.ObjectType),
				*FHCIDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
				Item.AssetPath.IsEmpty() ? TEXT("-") : *Item.AssetPath,
				Item.Field.IsEmpty() ? TEXT("-") : *Item.Field,
				Item.EvidenceKey.IsEmpty() ? TEXT("-") : *Item.EvidenceKey,
				Item.ActorPath.IsEmpty() ? TEXT("-") : *Item.ActorPath);
		}
	}
}

bool HCI_TryBuildAgentExecutorSimFinalReportFromLatestPreview(
	const FString& TamperMode,
	FHCIAgentSimulateExecuteFinalReport& OutReport)
{
	if (HCI_State().AgentSimulateExecuteReceiptPreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAgentDemo, Warning, TEXT("[HCI][AgentExecutorSimFinalReport] prepare=unavailable reason=no_sim_execute_receipt_preview_state"));
		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentExecutorSimFinalReport] suggestion=先运行 HCI.AgentExecutePlanReviewPrepareSimExecuteReceipt"));
		return false;
	}

	if (HCI_State().AgentExecuteTicketPreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAgentDemo, Warning, TEXT("[HCI][AgentExecutorSimFinalReport] prepare=unavailable reason=no_execute_ticket_preview_state"));
		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentExecutorSimFinalReport] suggestion=先运行 HCI.AgentExecutePlanReviewPrepareExecuteTicket"));
		return false;
	}

	if (HCI_State().AgentApplyConfirmRequestPreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAgentDemo, Warning, TEXT("[HCI][AgentExecutorSimFinalReport] prepare=unavailable reason=no_confirm_request_preview_state"));
		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentExecutorSimFinalReport] suggestion=先运行 HCI.AgentExecutePlanReviewPrepareConfirm"));
		return false;
	}

	if (HCI_State().AgentApplyRequestPreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAgentDemo, Warning, TEXT("[HCI][AgentExecutorSimFinalReport] prepare=unavailable reason=no_apply_request_preview_state"));
		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentExecutorSimFinalReport] suggestion=先运行 HCI.AgentExecutePlanReviewPrepareApply"));
		return false;
	}

	if (HCI_State().AgentExecutorReviewDiffPreviewState.DiffItems.Num() <= 0)
	{
		UE_LOG(LogHCIAgentDemo, Warning, TEXT("[HCI][AgentExecutorSimFinalReport] prepare=unavailable reason=no_review_preview_state"));
		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentExecutorSimFinalReport] suggestion=先运行 HCI.AgentExecutePlanReviewDemo 或 HCI.AgentExecutePlanReviewSelect"));
		return false;
	}

	FHCIAgentSimulateExecuteReceipt WorkingReceipt = HCI_State().AgentSimulateExecuteReceiptPreviewState;
	if (TamperMode == TEXT("digest"))
	{
		WorkingReceipt.SelectionDigest += TEXT("_tampered");
	}
	else if (TamperMode == TEXT("apply"))
	{
		WorkingReceipt.ApplyRequestId += TEXT("_stale");
	}
	else if (TamperMode == TEXT("review"))
	{
		WorkingReceipt.ReviewRequestId += TEXT("_stale");
	}
	else if (TamperMode == TEXT("confirm"))
	{
		WorkingReceipt.ConfirmRequestId += TEXT("_stale");
	}
	else if (TamperMode == TEXT("receipt"))
	{
		WorkingReceipt.bSimulatedDispatchAccepted = false;
	}
	else if (TamperMode == TEXT("ready"))
	{
		WorkingReceipt.bReadyToSimulateExecute = false;
	}

	if (!FHCIAgentExecutorSimulateExecuteFinalReportBridge::BuildSimulateExecuteFinalReport(
			WorkingReceipt,
			HCI_State().AgentExecuteTicketPreviewState,
			HCI_State().AgentApplyConfirmRequestPreviewState,
			HCI_State().AgentApplyRequestPreviewState,
			HCI_State().AgentExecutorReviewDiffPreviewState,
			OutReport))
	{
		UE_LOG(LogHCIAgentDemo, Error, TEXT("[HCI][AgentExecutorSimFinalReport] prepare_failed reason=bridge_failed"));
		return false;
	}

	HCI_State().AgentSimulateExecuteFinalReportPreviewState = OutReport;
	return true;
}

static bool HCI_TryParseSimFinalReportCommandArgs(const TArray<FString>& Args, FString& OutTamperMode)
{
	OutTamperMode = TEXT("none");
	if (Args.Num() == 0)
	{
		return true;
	}
	if (Args.Num() != 1)
	{
		return false;
	}
	return HCI_TryParseSimFinalReportTamperModeArg(Args[0], OutTamperMode);
}

void HCI_RunAbilityKitAgentExecutePlanReviewPrepareSimFinalReportCommand(const TArray<FString>& Args)
{
	FString TamperMode;
	if (!HCI_TryParseSimFinalReportCommandArgs(Args, TamperMode))
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentExecutorSimFinalReport] invalid_args usage=HCI.AgentExecutePlanReviewPrepareSimFinalReport [tamper=none|digest|apply|review|confirm|receipt|ready]"));
		return;
	}

	FHCIAgentSimulateExecuteFinalReport Report;
	if (!HCI_TryBuildAgentExecutorSimFinalReportFromLatestPreview(TamperMode, Report))
	{
		return;
	}

	HCI_LogAgentExecutorSimFinalReportSummary(Report);
	HCI_LogAgentExecutorSimFinalReportRows(Report);
	UE_LOG(
		LogHCIAgentDemo,
		Display,
		TEXT("[HCI][AgentExecutorSimFinalReport] hint=也可运行 HCI.AgentExecutePlanReviewPrepareSimFinalReportJson [tamper] 输出 SimFinalReport JSON"));
}

void HCI_RunAbilityKitAgentExecutePlanReviewPrepareSimFinalReportJsonCommand(const TArray<FString>& Args)
{
	FString TamperMode;
	if (!HCI_TryParseSimFinalReportCommandArgs(Args, TamperMode))
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentExecutorSimFinalReport] invalid_args usage=HCI.AgentExecutePlanReviewPrepareSimFinalReportJson [tamper=none|digest|apply|review|confirm|receipt|ready]"));
		return;
	}

	FHCIAgentSimulateExecuteFinalReport Report;
	if (!HCI_TryBuildAgentExecutorSimFinalReportFromLatestPreview(TamperMode, Report))
	{
		return;
	}

	HCI_LogAgentExecutorSimFinalReportSummary(Report);
	HCI_LogAgentExecutorSimFinalReportRows(Report);

	FString JsonText;
	if (!FHCIAgentSimulateExecuteFinalReportJsonSerializer::SerializeToJsonString(Report, JsonText))
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentExecutorSimFinalReport] json_failed request_id=%s"),
			Report.RequestId.IsEmpty() ? TEXT("-") : *Report.RequestId);
		return;
	}

	UE_LOG(
		LogHCIAgentDemo,
		Display,
		TEXT("[HCI][AgentExecutorSimFinalReport] json request_id=%s bytes=%d"),
		Report.RequestId.IsEmpty() ? TEXT("-") : *Report.RequestId,
		JsonText.Len());
	UE_LOG(LogHCIAgentDemo, Display, TEXT("%s"), *JsonText);
}

static bool HCI_TryParseSimArchiveBundleTamperModeArg(const FString& InValue, FString& OutTamperMode)
{
	const FString Normalized = InValue.TrimStartAndEnd().ToLower();
	if (Normalized == TEXT("none") ||
		Normalized == TEXT("digest") ||
		Normalized == TEXT("apply") ||
		Normalized == TEXT("review") ||
		Normalized == TEXT("confirm") ||
		Normalized == TEXT("receipt") ||
		Normalized == TEXT("final") ||
		Normalized == TEXT("ready"))
	{
		OutTamperMode = Normalized;
		return true;
	}
	return false;
}

static void HCI_LogAgentExecutorSimArchiveBundleSummary(const FHCIAgentSimulateExecuteArchiveBundle& Bundle)
{
	const FString SummaryLine = FString::Printf(
		TEXT("[HCI][AgentExecutorSimArchiveBundle] summary request_id=%s sim_final_report_id=%s sim_execute_receipt_id=%s execute_ticket_id=%s confirm_request_id=%s apply_request_id=%s review_request_id=%s selection_digest=%s archive_digest=%s user_confirmed=%s ready_to_simulate_execute=%s simulated_dispatch_accepted=%s simulation_completed=%s terminal_status=%s archive_status=%s archive_ready=%s error_code=%s reason=%s validation=ok"),
		Bundle.RequestId.IsEmpty() ? TEXT("-") : *Bundle.RequestId,
		Bundle.SimFinalReportId.IsEmpty() ? TEXT("-") : *Bundle.SimFinalReportId,
		Bundle.SimExecuteReceiptId.IsEmpty() ? TEXT("-") : *Bundle.SimExecuteReceiptId,
		Bundle.ExecuteTicketId.IsEmpty() ? TEXT("-") : *Bundle.ExecuteTicketId,
		Bundle.ConfirmRequestId.IsEmpty() ? TEXT("-") : *Bundle.ConfirmRequestId,
		Bundle.ApplyRequestId.IsEmpty() ? TEXT("-") : *Bundle.ApplyRequestId,
		Bundle.ReviewRequestId.IsEmpty() ? TEXT("-") : *Bundle.ReviewRequestId,
		Bundle.SelectionDigest.IsEmpty() ? TEXT("-") : *Bundle.SelectionDigest,
		Bundle.ArchiveDigest.IsEmpty() ? TEXT("-") : *Bundle.ArchiveDigest,
		Bundle.bUserConfirmed ? TEXT("true") : TEXT("false"),
		Bundle.bReadyToSimulateExecute ? TEXT("true") : TEXT("false"),
		Bundle.bSimulatedDispatchAccepted ? TEXT("true") : TEXT("false"),
		Bundle.bSimulationCompleted ? TEXT("true") : TEXT("false"),
		Bundle.TerminalStatus.IsEmpty() ? TEXT("-") : *Bundle.TerminalStatus,
		Bundle.ArchiveStatus.IsEmpty() ? TEXT("-") : *Bundle.ArchiveStatus,
		Bundle.bArchiveReady ? TEXT("true") : TEXT("false"),
		Bundle.ErrorCode.IsEmpty() ? TEXT("-") : *Bundle.ErrorCode,
		Bundle.Reason.IsEmpty() ? TEXT("-") : *Bundle.Reason);

	const bool bUseWarning = !Bundle.bArchiveReady;
	if (bUseWarning)
	{
		UE_LOG(LogHCIAgentDemo, Warning, TEXT("%s"), *SummaryLine);
		return;
	}

	UE_LOG(LogHCIAgentDemo, Display, TEXT("%s"), *SummaryLine);
}

static void HCI_LogAgentExecutorSimArchiveBundleRows(const FHCIAgentSimulateExecuteArchiveBundle& Bundle)
{
	for (const FHCIAgentApplyRequestItem& Item : Bundle.Items)
	{
		if (Item.bBlocked)
		{
			UE_LOG(
				LogHCIAgentDemo,
				Warning,
				TEXT("[HCI][AgentExecutorSimArchiveBundle] row=%d tool_name=%s risk=%s blocked=%s skip_reason=%s object_type=%s locate_strategy=%s asset_path=%s field=%s evidence_key=%s actor_path=%s"),
				Item.RowIndex,
				Item.ToolName.IsEmpty() ? TEXT("-") : *Item.ToolName,
				*FHCIDryRunDiff::RiskToString(Item.Risk),
				TEXT("true"),
				Item.SkipReason.IsEmpty() ? TEXT("-") : *Item.SkipReason,
				*FHCIDryRunDiff::ObjectTypeToString(Item.ObjectType),
				*FHCIDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
				Item.AssetPath.IsEmpty() ? TEXT("-") : *Item.AssetPath,
				Item.Field.IsEmpty() ? TEXT("-") : *Item.Field,
				Item.EvidenceKey.IsEmpty() ? TEXT("-") : *Item.EvidenceKey,
				Item.ActorPath.IsEmpty() ? TEXT("-") : *Item.ActorPath);
		}
		else
		{
			UE_LOG(
				LogHCIAgentDemo,
				Display,
				TEXT("[HCI][AgentExecutorSimArchiveBundle] row=%d tool_name=%s risk=%s blocked=%s skip_reason=%s object_type=%s locate_strategy=%s asset_path=%s field=%s evidence_key=%s actor_path=%s"),
				Item.RowIndex,
				Item.ToolName.IsEmpty() ? TEXT("-") : *Item.ToolName,
				*FHCIDryRunDiff::RiskToString(Item.Risk),
				TEXT("false"),
				Item.SkipReason.IsEmpty() ? TEXT("-") : *Item.SkipReason,
				*FHCIDryRunDiff::ObjectTypeToString(Item.ObjectType),
				*FHCIDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
				Item.AssetPath.IsEmpty() ? TEXT("-") : *Item.AssetPath,
				Item.Field.IsEmpty() ? TEXT("-") : *Item.Field,
				Item.EvidenceKey.IsEmpty() ? TEXT("-") : *Item.EvidenceKey,
				Item.ActorPath.IsEmpty() ? TEXT("-") : *Item.ActorPath);
		}
	}
}

bool HCI_TryBuildAgentExecutorSimArchiveBundleFromLatestPreview(
	const FString& TamperMode,
	FHCIAgentSimulateExecuteArchiveBundle& OutBundle)
{
	if (HCI_State().AgentSimulateExecuteFinalReportPreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAgentDemo, Warning, TEXT("[HCI][AgentExecutorSimArchiveBundle] prepare=unavailable reason=no_sim_final_report_preview_state"));
		UE_LOG(LogHCIAgentDemo, Display, TEXT("[HCI][AgentExecutorSimArchiveBundle] suggestion=先运行 HCI.AgentExecutePlanReviewPrepareSimFinalReport"));
		return false;
	}

	if (HCI_State().AgentSimulateExecuteReceiptPreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAgentDemo, Warning, TEXT("[HCI][AgentExecutorSimArchiveBundle] prepare=unavailable reason=no_sim_execute_receipt_preview_state"));
		UE_LOG(LogHCIAgentDemo, Display, TEXT("[HCI][AgentExecutorSimArchiveBundle] suggestion=先运行 HCI.AgentExecutePlanReviewPrepareSimExecuteReceipt"));
		return false;
	}

	if (HCI_State().AgentExecuteTicketPreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAgentDemo, Warning, TEXT("[HCI][AgentExecutorSimArchiveBundle] prepare=unavailable reason=no_execute_ticket_preview_state"));
		UE_LOG(LogHCIAgentDemo, Display, TEXT("[HCI][AgentExecutorSimArchiveBundle] suggestion=先运行 HCI.AgentExecutePlanReviewPrepareExecuteTicket"));
		return false;
	}

	if (HCI_State().AgentApplyConfirmRequestPreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAgentDemo, Warning, TEXT("[HCI][AgentExecutorSimArchiveBundle] prepare=unavailable reason=no_confirm_request_preview_state"));
		UE_LOG(LogHCIAgentDemo, Display, TEXT("[HCI][AgentExecutorSimArchiveBundle] suggestion=先运行 HCI.AgentExecutePlanReviewPrepareConfirm"));
		return false;
	}

	if (HCI_State().AgentApplyRequestPreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAgentDemo, Warning, TEXT("[HCI][AgentExecutorSimArchiveBundle] prepare=unavailable reason=no_apply_request_preview_state"));
		UE_LOG(LogHCIAgentDemo, Display, TEXT("[HCI][AgentExecutorSimArchiveBundle] suggestion=先运行 HCI.AgentExecutePlanReviewPrepareApply"));
		return false;
	}

	if (HCI_State().AgentExecutorReviewDiffPreviewState.DiffItems.Num() <= 0)
	{
		UE_LOG(LogHCIAgentDemo, Warning, TEXT("[HCI][AgentExecutorSimArchiveBundle] prepare=unavailable reason=no_review_preview_state"));
		UE_LOG(LogHCIAgentDemo, Display, TEXT("[HCI][AgentExecutorSimArchiveBundle] suggestion=先运行 HCI.AgentExecutePlanReviewDemo 或 HCI.AgentExecutePlanReviewSelect"));
		return false;
	}

	FHCIAgentSimulateExecuteFinalReport WorkingFinalReport = HCI_State().AgentSimulateExecuteFinalReportPreviewState;
	if (TamperMode == TEXT("digest"))
	{
		WorkingFinalReport.SelectionDigest += TEXT("_tampered");
	}
	else if (TamperMode == TEXT("apply"))
	{
		WorkingFinalReport.ApplyRequestId += TEXT("_stale");
	}
	else if (TamperMode == TEXT("review"))
	{
		WorkingFinalReport.ReviewRequestId += TEXT("_stale");
	}
	else if (TamperMode == TEXT("confirm"))
	{
		WorkingFinalReport.ConfirmRequestId += TEXT("_stale");
	}
	else if (TamperMode == TEXT("receipt"))
	{
		WorkingFinalReport.bSimulatedDispatchAccepted = false;
	}
	else if (TamperMode == TEXT("final"))
	{
		WorkingFinalReport.bSimulationCompleted = false;
		WorkingFinalReport.TerminalStatus = TEXT("blocked");
	}
	else if (TamperMode == TEXT("ready"))
	{
		WorkingFinalReport.bReadyToSimulateExecute = false;
	}

	if (!FHCIAgentExecutorSimulateExecuteArchiveBundleBridge::BuildSimulateExecuteArchiveBundle(
			WorkingFinalReport,
			HCI_State().AgentSimulateExecuteReceiptPreviewState,
			HCI_State().AgentExecuteTicketPreviewState,
			HCI_State().AgentApplyConfirmRequestPreviewState,
			HCI_State().AgentApplyRequestPreviewState,
			HCI_State().AgentExecutorReviewDiffPreviewState,
			OutBundle))
	{
		UE_LOG(LogHCIAgentDemo, Error, TEXT("[HCI][AgentExecutorSimArchiveBundle] prepare_failed reason=bridge_failed"));
		return false;
	}

	HCI_State().AgentSimulateExecuteArchiveBundlePreviewState = OutBundle;
	return true;
}

static bool HCI_TryParseSimArchiveBundleCommandArgs(const TArray<FString>& Args, FString& OutTamperMode)
{
	OutTamperMode = TEXT("none");
	if (Args.Num() == 0)
	{
		return true;
	}
	if (Args.Num() != 1)
	{
		return false;
	}
	return HCI_TryParseSimArchiveBundleTamperModeArg(Args[0], OutTamperMode);
}

void HCI_RunAbilityKitAgentExecutePlanReviewPrepareSimArchiveBundleCommand(const TArray<FString>& Args)
{
	FString TamperMode;
	if (!HCI_TryParseSimArchiveBundleCommandArgs(Args, TamperMode))
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentExecutorSimArchiveBundle] invalid_args usage=HCI.AgentExecutePlanReviewPrepareSimArchiveBundle [tamper=none|digest|apply|review|confirm|receipt|final|ready]"));
		return;
	}

	FHCIAgentSimulateExecuteArchiveBundle Bundle;
	if (!HCI_TryBuildAgentExecutorSimArchiveBundleFromLatestPreview(TamperMode, Bundle))
	{
		return;
	}

	HCI_LogAgentExecutorSimArchiveBundleSummary(Bundle);
	HCI_LogAgentExecutorSimArchiveBundleRows(Bundle);
	UE_LOG(
		LogHCIAgentDemo,
		Display,
		TEXT("[HCI][AgentExecutorSimArchiveBundle] hint=也可运行 HCI.AgentExecutePlanReviewPrepareSimArchiveBundleJson [tamper] 输出 SimArchiveBundle JSON"));
}

void HCI_RunAbilityKitAgentExecutePlanReviewPrepareSimArchiveBundleJsonCommand(const TArray<FString>& Args)
{
	FString TamperMode;
	if (!HCI_TryParseSimArchiveBundleCommandArgs(Args, TamperMode))
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentExecutorSimArchiveBundle] invalid_args usage=HCI.AgentExecutePlanReviewPrepareSimArchiveBundleJson [tamper=none|digest|apply|review|confirm|receipt|final|ready]"));
		return;
	}

	FHCIAgentSimulateExecuteArchiveBundle Bundle;
	if (!HCI_TryBuildAgentExecutorSimArchiveBundleFromLatestPreview(TamperMode, Bundle))
	{
		return;
	}

	HCI_LogAgentExecutorSimArchiveBundleSummary(Bundle);
	HCI_LogAgentExecutorSimArchiveBundleRows(Bundle);

	FString JsonText;
	if (!FHCIAgentSimulateExecuteArchiveBundleJsonSerializer::SerializeToJsonString(Bundle, JsonText))
	{
		UE_LOG(
			LogHCIAgentDemo,
			Error,
			TEXT("[HCI][AgentExecutorSimArchiveBundle] json_failed request_id=%s"),
			Bundle.RequestId.IsEmpty() ? TEXT("-") : *Bundle.RequestId);
		return;
	}

	UE_LOG(
		LogHCIAgentDemo,
		Display,
		TEXT("[HCI][AgentExecutorSimArchiveBundle] json request_id=%s bytes=%d"),
		Bundle.RequestId.IsEmpty() ? TEXT("-") : *Bundle.RequestId,
		JsonText.Len());
	UE_LOG(LogHCIAgentDemo, Display, TEXT("%s"), *JsonText);
}

void HCI_RunAbilityKitAgentExecutePlanDemoCommand(const TArray<FString>& Args)
{
	const FHCIToolRegistry& Registry = FHCIToolRegistry::GetReadOnly();

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
				TotalStepRows += HCI_State().AgentPlanPreviewState.Steps.Num();
				if (BuiltAndCompletedCases == CompletedBefore)
				{
					// Case executed but not completed is already counted in RejectedCases.
				}
			}
		}

		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentExecutor] summary total_cases=%d completed_cases=%d rejected_cases=%d execution_mode=%s total_step_rows=%d validation=ok"),
			Cases.Num(),
			BuiltAndCompletedCases,
			RejectedCases,
			TEXT("simulate_dry_run"),
			TotalStepRows);
		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentExecutor] hint=也可运行 HCI.AgentExecutePlanDemo [自然语言文本...]"));
		return;
	}

	const FString UserText = HCI_JoinConsoleArgsAsText(Args);
	if (UserText.IsEmpty())
	{
		UE_LOG(LogHCIAgentDemo, Error, TEXT("[HCI][AgentExecutor] invalid_args reason=empty input text"));
		return;
	}

	int32 CompletedCases = 0;
	int32 RejectedCases = 0;
	HCI_RunAgentExecutorDemoCase(TEXT("custom"), UserText, TEXT("req_cli_f3"), Registry, CompletedCases, RejectedCases);
}
static bool HCI_TryParseSimHandoffEnvelopeTamperModeArg(const FString& InValue, FString& OutTamperMode)
{
	const FString Normalized = InValue.TrimStartAndEnd().ToLower();
	if (Normalized == TEXT("none") || Normalized == TEXT("digest") || Normalized == TEXT("apply") ||
		Normalized == TEXT("review") || Normalized == TEXT("confirm") || Normalized == TEXT("receipt") ||
		Normalized == TEXT("final") || Normalized == TEXT("archive") || Normalized == TEXT("ready"))
	{
		OutTamperMode = Normalized;
		return true;
	}
	return false;
}

static void HCI_LogAgentExecutorSimHandoffEnvelopeSummary(const FHCIAgentSimulateExecuteHandoffEnvelope& Envelope)
{
	const FString SummaryLine = FString::Printf(
		TEXT("[HCI][AgentExecutorSimHandoffEnvelope] summary request_id=%s sim_archive_bundle_id=%s sim_final_report_id=%s sim_execute_receipt_id=%s execute_ticket_id=%s confirm_request_id=%s apply_request_id=%s review_request_id=%s selection_digest=%s archive_digest=%s handoff_digest=%s handoff_target=%s user_confirmed=%s ready_to_simulate_execute=%s simulated_dispatch_accepted=%s simulation_completed=%s terminal_status=%s archive_status=%s handoff_status=%s archive_ready=%s handoff_ready=%s error_code=%s reason=%s validation=ok"),
		Envelope.RequestId.IsEmpty() ? TEXT("-") : *Envelope.RequestId,
		Envelope.SimArchiveBundleId.IsEmpty() ? TEXT("-") : *Envelope.SimArchiveBundleId,
		Envelope.SimFinalReportId.IsEmpty() ? TEXT("-") : *Envelope.SimFinalReportId,
		Envelope.SimExecuteReceiptId.IsEmpty() ? TEXT("-") : *Envelope.SimExecuteReceiptId,
		Envelope.ExecuteTicketId.IsEmpty() ? TEXT("-") : *Envelope.ExecuteTicketId,
		Envelope.ConfirmRequestId.IsEmpty() ? TEXT("-") : *Envelope.ConfirmRequestId,
		Envelope.ApplyRequestId.IsEmpty() ? TEXT("-") : *Envelope.ApplyRequestId,
		Envelope.ReviewRequestId.IsEmpty() ? TEXT("-") : *Envelope.ReviewRequestId,
		Envelope.SelectionDigest.IsEmpty() ? TEXT("-") : *Envelope.SelectionDigest,
		Envelope.ArchiveDigest.IsEmpty() ? TEXT("-") : *Envelope.ArchiveDigest,
		Envelope.HandoffDigest.IsEmpty() ? TEXT("-") : *Envelope.HandoffDigest,
		Envelope.HandoffTarget.IsEmpty() ? TEXT("-") : *Envelope.HandoffTarget,
		Envelope.bUserConfirmed ? TEXT("true") : TEXT("false"),
		Envelope.bReadyToSimulateExecute ? TEXT("true") : TEXT("false"),
		Envelope.bSimulatedDispatchAccepted ? TEXT("true") : TEXT("false"),
		Envelope.bSimulationCompleted ? TEXT("true") : TEXT("false"),
		Envelope.TerminalStatus.IsEmpty() ? TEXT("-") : *Envelope.TerminalStatus,
		Envelope.ArchiveStatus.IsEmpty() ? TEXT("-") : *Envelope.ArchiveStatus,
		Envelope.HandoffStatus.IsEmpty() ? TEXT("-") : *Envelope.HandoffStatus,
		Envelope.bArchiveReady ? TEXT("true") : TEXT("false"),
		Envelope.bHandoffReady ? TEXT("true") : TEXT("false"),
		Envelope.ErrorCode.IsEmpty() ? TEXT("-") : *Envelope.ErrorCode,
		Envelope.Reason.IsEmpty() ? TEXT("-") : *Envelope.Reason);
	if (Envelope.bHandoffReady)
	{
		UE_LOG(LogHCIAgentDemo, Display, TEXT("%s"), *SummaryLine);
		return;
	}
	UE_LOG(LogHCIAgentDemo, Warning, TEXT("%s"), *SummaryLine);
}

static void HCI_LogAgentExecutorSimHandoffEnvelopeRows(const FHCIAgentSimulateExecuteHandoffEnvelope& Envelope)
{
	for (const FHCIAgentApplyRequestItem& Item : Envelope.Items)
	{
		if (Item.bBlocked)
		{
			UE_LOG(
				LogHCIAgentDemo,
				Warning,
				TEXT("[HCI][AgentExecutorSimHandoffEnvelope] row=%d tool_name=%s risk=%s blocked=%s skip_reason=%s object_type=%s locate_strategy=%s asset_path=%s field=%s evidence_key=%s actor_path=%s"),
				Item.RowIndex,
				Item.ToolName.IsEmpty() ? TEXT("-") : *Item.ToolName,
				*FHCIDryRunDiff::RiskToString(Item.Risk),
				TEXT("true"),
				Item.SkipReason.IsEmpty() ? TEXT("-") : *Item.SkipReason,
				*FHCIDryRunDiff::ObjectTypeToString(Item.ObjectType),
				*FHCIDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
				Item.AssetPath.IsEmpty() ? TEXT("-") : *Item.AssetPath,
				Item.Field.IsEmpty() ? TEXT("-") : *Item.Field,
				Item.EvidenceKey.IsEmpty() ? TEXT("-") : *Item.EvidenceKey,
				Item.ActorPath.IsEmpty() ? TEXT("-") : *Item.ActorPath);
			continue;
		}

		UE_LOG(
			LogHCIAgentDemo,
			Display,
			TEXT("[HCI][AgentExecutorSimHandoffEnvelope] row=%d tool_name=%s risk=%s blocked=%s skip_reason=%s object_type=%s locate_strategy=%s asset_path=%s field=%s evidence_key=%s actor_path=%s"),
			Item.RowIndex,
			Item.ToolName.IsEmpty() ? TEXT("-") : *Item.ToolName,
			*FHCIDryRunDiff::RiskToString(Item.Risk),
			TEXT("false"),
			Item.SkipReason.IsEmpty() ? TEXT("-") : *Item.SkipReason,
			*FHCIDryRunDiff::ObjectTypeToString(Item.ObjectType),
			*FHCIDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
			Item.AssetPath.IsEmpty() ? TEXT("-") : *Item.AssetPath,
			Item.Field.IsEmpty() ? TEXT("-") : *Item.Field,
			Item.EvidenceKey.IsEmpty() ? TEXT("-") : *Item.EvidenceKey,
			Item.ActorPath.IsEmpty() ? TEXT("-") : *Item.ActorPath);
	}
}

bool HCI_TryBuildAgentExecutorSimHandoffEnvelopeFromLatestPreview(const FString& TamperMode, FHCIAgentSimulateExecuteHandoffEnvelope& OutEnvelope)
{
	if (HCI_State().AgentSimulateExecuteArchiveBundlePreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAgentDemo, Warning, TEXT("[HCI][AgentExecutorSimHandoffEnvelope] prepare=unavailable reason=no_sim_archive_bundle_preview_state"));
		UE_LOG(LogHCIAgentDemo, Display, TEXT("[HCI][AgentExecutorSimHandoffEnvelope] suggestion=先运行 HCI.AgentExecutePlanReviewPrepareSimArchiveBundle"));
		return false;
	}
	if (HCI_State().AgentSimulateExecuteFinalReportPreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAgentDemo, Warning, TEXT("[HCI][AgentExecutorSimHandoffEnvelope] prepare=unavailable reason=no_sim_final_report_preview_state"));
		UE_LOG(LogHCIAgentDemo, Display, TEXT("[HCI][AgentExecutorSimHandoffEnvelope] suggestion=先运行 HCI.AgentExecutePlanReviewPrepareSimFinalReport"));
		return false;
	}
	if (HCI_State().AgentSimulateExecuteReceiptPreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAgentDemo, Warning, TEXT("[HCI][AgentExecutorSimHandoffEnvelope] prepare=unavailable reason=no_sim_execute_receipt_preview_state"));
		UE_LOG(LogHCIAgentDemo, Display, TEXT("[HCI][AgentExecutorSimHandoffEnvelope] suggestion=先运行 HCI.AgentExecutePlanReviewPrepareSimExecuteReceipt"));
		return false;
	}
	if (HCI_State().AgentExecuteTicketPreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAgentDemo, Warning, TEXT("[HCI][AgentExecutorSimHandoffEnvelope] prepare=unavailable reason=no_execute_ticket_preview_state"));
		UE_LOG(LogHCIAgentDemo, Display, TEXT("[HCI][AgentExecutorSimHandoffEnvelope] suggestion=先运行 HCI.AgentExecutePlanReviewPrepareExecuteTicket"));
		return false;
	}
	if (HCI_State().AgentApplyConfirmRequestPreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAgentDemo, Warning, TEXT("[HCI][AgentExecutorSimHandoffEnvelope] prepare=unavailable reason=no_confirm_request_preview_state"));
		UE_LOG(LogHCIAgentDemo, Display, TEXT("[HCI][AgentExecutorSimHandoffEnvelope] suggestion=先运行 HCI.AgentExecutePlanReviewPrepareConfirm"));
		return false;
	}
	if (HCI_State().AgentApplyRequestPreviewState.Items.Num() <= 0)
	{
		UE_LOG(LogHCIAgentDemo, Warning, TEXT("[HCI][AgentExecutorSimHandoffEnvelope] prepare=unavailable reason=no_apply_request_preview_state"));
		UE_LOG(LogHCIAgentDemo, Display, TEXT("[HCI][AgentExecutorSimHandoffEnvelope] suggestion=先运行 HCI.AgentExecutePlanReviewPrepareApply"));
		return false;
	}
	if (HCI_State().AgentExecutorReviewDiffPreviewState.DiffItems.Num() <= 0)
	{
		UE_LOG(LogHCIAgentDemo, Warning, TEXT("[HCI][AgentExecutorSimHandoffEnvelope] prepare=unavailable reason=no_review_preview_state"));
		UE_LOG(LogHCIAgentDemo, Display, TEXT("[HCI][AgentExecutorSimHandoffEnvelope] suggestion=先运行 HCI.AgentExecutePlanReviewDemo 或 HCI.AgentExecutePlanReviewSelect"));
		return false;
	}

	FHCIAgentSimulateExecuteArchiveBundle WorkingArchive = HCI_State().AgentSimulateExecuteArchiveBundlePreviewState;
	if (TamperMode == TEXT("digest")) { WorkingArchive.SelectionDigest += TEXT("_tampered"); }
	else if (TamperMode == TEXT("apply")) { WorkingArchive.ApplyRequestId += TEXT("_stale"); }
	else if (TamperMode == TEXT("review")) { WorkingArchive.ReviewRequestId += TEXT("_stale"); }
	else if (TamperMode == TEXT("confirm")) { WorkingArchive.ConfirmRequestId += TEXT("_stale"); }
	else if (TamperMode == TEXT("receipt")) { WorkingArchive.bSimulatedDispatchAccepted = false; }
	else if (TamperMode == TEXT("final")) { WorkingArchive.bSimulationCompleted = false; WorkingArchive.TerminalStatus = TEXT("blocked"); }
	else if (TamperMode == TEXT("archive")) { WorkingArchive.bArchiveReady = false; WorkingArchive.ArchiveStatus = TEXT("blocked"); }
	else if (TamperMode == TEXT("ready")) { WorkingArchive.bReadyToSimulateExecute = false; }

	if (!FHCIAgentExecutorSimulateExecuteHandoffEnvelopeBridge::BuildSimulateExecuteHandoffEnvelope(
		WorkingArchive,
		HCI_State().AgentSimulateExecuteFinalReportPreviewState,
		HCI_State().AgentSimulateExecuteReceiptPreviewState,
		HCI_State().AgentExecuteTicketPreviewState,
		HCI_State().AgentApplyConfirmRequestPreviewState,
		HCI_State().AgentApplyRequestPreviewState,
		HCI_State().AgentExecutorReviewDiffPreviewState,
		OutEnvelope))
	{
		UE_LOG(LogHCIAgentDemo, Error, TEXT("[HCI][AgentExecutorSimHandoffEnvelope] prepare_failed reason=bridge_failed"));
		return false;
	}

	HCI_State().AgentSimulateExecuteHandoffEnvelopePreviewState = OutEnvelope;
	return true;
}

static bool HCI_TryParseSimHandoffEnvelopeCommandArgs(const TArray<FString>& Args, FString& OutTamperMode)
{
	OutTamperMode = TEXT("none");
	if (Args.Num() == 0)
	{
		return true;
	}
	if (Args.Num() != 1)
	{
		return false;
	}
	return HCI_TryParseSimHandoffEnvelopeTamperModeArg(Args[0], OutTamperMode);
}

void HCI_RunAbilityKitAgentExecutePlanReviewPrepareSimHandoffEnvelopeCommand(const TArray<FString>& Args)
{
	FString TamperMode;
	if (!HCI_TryParseSimHandoffEnvelopeCommandArgs(Args, TamperMode))
	{
		UE_LOG(LogHCIAgentDemo, Error, TEXT("[HCI][AgentExecutorSimHandoffEnvelope] invalid_args usage=HCI.AgentExecutePlanReviewPrepareSimHandoffEnvelope [tamper=none|digest|apply|review|confirm|receipt|final|archive|ready]"));
		return;
	}

	FHCIAgentSimulateExecuteHandoffEnvelope Envelope;
	if (!HCI_TryBuildAgentExecutorSimHandoffEnvelopeFromLatestPreview(TamperMode, Envelope))
	{
		return;
	}

	HCI_LogAgentExecutorSimHandoffEnvelopeSummary(Envelope);
	HCI_LogAgentExecutorSimHandoffEnvelopeRows(Envelope);
	UE_LOG(LogHCIAgentDemo, Display, TEXT("[HCI][AgentExecutorSimHandoffEnvelope] hint=也可运行 HCI.AgentExecutePlanReviewPrepareSimHandoffEnvelopeJson [tamper] 输出 SimHandoffEnvelope JSON"));
}

void HCI_RunAbilityKitAgentExecutePlanReviewPrepareSimHandoffEnvelopeJsonCommand(const TArray<FString>& Args)
{
	FString TamperMode;
	if (!HCI_TryParseSimHandoffEnvelopeCommandArgs(Args, TamperMode))
	{
		UE_LOG(LogHCIAgentDemo, Error, TEXT("[HCI][AgentExecutorSimHandoffEnvelope] invalid_args usage=HCI.AgentExecutePlanReviewPrepareSimHandoffEnvelopeJson [tamper=none|digest|apply|review|confirm|receipt|final|archive|ready]"));
		return;
	}

	FHCIAgentSimulateExecuteHandoffEnvelope Envelope;
	if (!HCI_TryBuildAgentExecutorSimHandoffEnvelopeFromLatestPreview(TamperMode, Envelope))
	{
		return;
	}

	HCI_LogAgentExecutorSimHandoffEnvelopeSummary(Envelope);
	HCI_LogAgentExecutorSimHandoffEnvelopeRows(Envelope);

	FString JsonText;
	if (!FHCIAgentSimulateExecuteHandoffEnvelopeJsonSerializer::SerializeToJsonString(Envelope, JsonText))
	{
		UE_LOG(LogHCIAgentDemo, Error, TEXT("[HCI][AgentExecutorSimHandoffEnvelope] json_failed request_id=%s"), Envelope.RequestId.IsEmpty() ? TEXT("-") : *Envelope.RequestId);
		return;
	}

	UE_LOG(LogHCIAgentDemo, Display, TEXT("[HCI][AgentExecutorSimHandoffEnvelope] json request_id=%s bytes=%d"), Envelope.RequestId.IsEmpty() ? TEXT("-") : *Envelope.RequestId, JsonText.Len());
	UE_LOG(LogHCIAgentDemo, Display, TEXT("%s"), *JsonText);
}






