#include "HCIAbilityKitEditorModule.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Audit/HCIAbilityKitAuditScanAsyncController.h"
#include "Audit/HCIAbilityKitAuditTagNames.h"
#include "ContentBrowserMenuContexts.h"
#include "Dom/JsonObject.h"
#include "Audit/HCIAbilityKitAuditScanService.h"
#include "Containers/Ticker.h"
#include "Factories/HCIAbilityKitFactory.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HCIAbilityKitAsset.h"
#include "HCIAbilityKitErrorCodes.h"
#include "IPythonScriptPlugin.h"
#include "Misc/DelayedAutoRegister.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Search/HCIAbilityKitSearchIndexService.h"
#include "Search/HCIAbilityKitSearchQueryService.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Services/HCIAbilityKitParserService.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"

DEFINE_LOG_CATEGORY_STATIC(LogHCIAbilityKitSearchQuery, Log, All);
DEFINE_LOG_CATEGORY_STATIC(LogHCIAbilityKitAuditScan, Log, All);

static TObjectPtr<UHCIAbilityKitFactory> GHCIAbilityKitFactory;
static TUniquePtr<FAutoConsoleCommand> GHCIAbilityKitSearchCommand;
static TUniquePtr<FAutoConsoleCommand> GHCIAbilityKitAuditScanCommand;
static TUniquePtr<FAutoConsoleCommand> GHCIAbilityKitAuditScanAsyncCommand;
static TUniquePtr<FAutoConsoleCommand> GHCIAbilityKitAuditScanProgressCommand;
static TUniquePtr<FAutoConsoleCommand> GHCIAbilityKitAuditScanStopCommand;
static TUniquePtr<FAutoConsoleCommand> GHCIAbilityKitAuditScanRetryCommand;
static const TCHAR* GHCIAbilityKitPythonScriptPath = TEXT("SourceData/AbilityKits/Python/hci_abilitykit_hook.py");

struct FHCIAbilityKitAuditScanAsyncState
{
	FHCIAbilityKitAuditScanAsyncController Controller;
	int32 LastLogPercentBucket = -1;
	double StartTimeSeconds = 0.0;
	FHCIAbilityKitAuditScanSnapshot Snapshot;
	FTSTicker::FDelegateHandle TickHandle;
};

static FHCIAbilityKitAuditScanAsyncState GHCIAbilityKitAuditScanAsyncState;

static FString HCI_ToPythonStringLiteral(const FString& Value)
{
	FString Escaped = Value;
	Escaped.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
	Escaped.ReplaceInline(TEXT("'"), TEXT("\\'"));
	return FString::Printf(TEXT("'%s'"), *Escaped);
}

static FString HCI_GetJsonStringOrDefault(
	const TSharedPtr<FJsonObject>& JsonObject,
	const FString& FieldName,
	const FString& DefaultValue)
{
	if (!JsonObject.IsValid())
	{
		return DefaultValue;
	}

	FString Parsed;
	return JsonObject->TryGetStringField(FieldName, Parsed) ? Parsed : DefaultValue;
}

static void HCI_DeleteTempFile(const FString& Filename)
{
	if (!Filename.IsEmpty())
	{
		IFileManager::Get().Delete(*Filename, false, true, true);
	}
}

static void HCI_ParseAuditRowFromAssetData(const FAssetData& AssetData, FHCIAbilityKitAuditAssetRow& OutRow)
{
	OutRow.AssetPath = AssetData.GetObjectPathString();
	OutRow.AssetName = AssetData.AssetName.ToString();

	AssetData.GetTagValue(HCIAbilityKitAuditTagNames::Id, OutRow.Id);
	AssetData.GetTagValue(HCIAbilityKitAuditTagNames::DisplayName, OutRow.DisplayName);
	AssetData.GetTagValue(HCIAbilityKitAuditTagNames::RepresentingMesh, OutRow.RepresentingMeshPath);

	FString DamageText;
	if (AssetData.GetTagValue(HCIAbilityKitAuditTagNames::Damage, DamageText))
	{
		LexTryParseString(OutRow.Damage, *DamageText);
	}
}

static void HCI_AccumulateAuditCoverage(const FHCIAbilityKitAuditAssetRow& Row, FHCIAbilityKitAuditScanStats& InOutStats)
{
	if (!Row.Id.IsEmpty())
	{
		++InOutStats.IdCoveredCount;
	}
	if (!Row.DisplayName.IsEmpty())
	{
		++InOutStats.DisplayNameCoveredCount;
	}
	if (!Row.RepresentingMeshPath.IsEmpty())
	{
		++InOutStats.RepresentingMeshCoveredCount;
	}
}

static void HCI_LogAuditRows(const TCHAR* Prefix, const TArray<FHCIAbilityKitAuditAssetRow>& Rows, const int32 LogTopN)
{
	const int32 CountToLog = FMath::Min(LogTopN, Rows.Num());
	for (int32 Index = 0; Index < CountToLog; ++Index)
	{
		const FHCIAbilityKitAuditAssetRow& Row = Rows[Index];
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Display,
			TEXT("%s row=%d asset=%s id=%s display_name=%s damage=%.2f representing_mesh=%s"),
			Prefix,
			Index,
			*Row.AssetPath,
			*Row.Id,
			*Row.DisplayName,
			Row.Damage,
			*Row.RepresentingMeshPath);
	}
}

static void HCI_StopAuditScanAsyncTicker()
{
	FHCIAbilityKitAuditScanAsyncState& State = GHCIAbilityKitAuditScanAsyncState;
	if (State.TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(State.TickHandle);
		State.TickHandle.Reset();
	}
}

static void HCI_ResetAuditScanAsyncState(const bool bClearRetryContext)
{
	HCI_StopAuditScanAsyncTicker();
	GHCIAbilityKitAuditScanAsyncState.Controller.Reset(bClearRetryContext);
	GHCIAbilityKitAuditScanAsyncState.LastLogPercentBucket = -1;
	GHCIAbilityKitAuditScanAsyncState.StartTimeSeconds = 0.0;
	GHCIAbilityKitAuditScanAsyncState.Snapshot = FHCIAbilityKitAuditScanSnapshot();
}

static bool HCI_ParseAuditScanAsyncArgs(
	const TArray<FString>& Args,
	int32& OutBatchSize,
	int32& OutLogTopN,
	FString& OutError)
{
	OutBatchSize = 256;
	OutLogTopN = 10;

	if (Args.Num() >= 1)
	{
		int32 ParsedBatchSize = 0;
		if (!LexTryParseString(ParsedBatchSize, *Args[0]) || ParsedBatchSize <= 0)
		{
			OutError = TEXT("batch_size must be an integer >= 1");
			return false;
		}
		OutBatchSize = ParsedBatchSize;
	}

	if (Args.Num() >= 2)
	{
		int32 ParsedTopN = 0;
		if (!LexTryParseString(ParsedTopN, *Args[1]) || ParsedTopN < 0)
		{
			OutError = TEXT("log_top_n must be an integer >= 0");
			return false;
		}
		OutLogTopN = ParsedTopN;
	}

	return true;
}

static void HCI_ConvergeAuditScanAsyncFailure(const FString& Reason)
{
	FHCIAbilityKitAuditScanAsyncState& State = GHCIAbilityKitAuditScanAsyncState;
	State.Controller.Fail(Reason);
	State.Snapshot.Stats.AssetCount = State.Snapshot.Rows.Num();
	State.Snapshot.Stats.UpdatedUtc = FDateTime::UtcNow();
	State.Snapshot.Stats.DurationMs = (FPlatformTime::Seconds() - State.StartTimeSeconds) * 1000.0;

	UE_LOG(
		LogHCIAbilityKitAuditScan,
		Error,
		TEXT("[HCIAbilityKit][AuditScanAsync] failed reason=%s processed=%d/%d"),
		*State.Controller.GetLastFailureReason(),
		State.Controller.GetNextIndex(),
		State.Controller.GetTotalCount());
	UE_LOG(
		LogHCIAbilityKitAuditScan,
		Warning,
		TEXT("[HCIAbilityKit][AuditScanAsync] suggestion=运行 HCIAbilityKit.AuditScanAsyncRetry 可按上次参数重试"));
}

static void HCI_FinalizeAuditScanAsyncSuccess()
{
	FHCIAbilityKitAuditScanAsyncState& State = GHCIAbilityKitAuditScanAsyncState;
	State.Controller.Complete();
	State.Snapshot.Stats.AssetCount = State.Snapshot.Rows.Num();
	State.Snapshot.Stats.UpdatedUtc = FDateTime::UtcNow();
	State.Snapshot.Stats.DurationMs = (FPlatformTime::Seconds() - State.StartTimeSeconds) * 1000.0;
	UE_LOG(
		LogHCIAbilityKitAuditScan,
		Display,
		TEXT("[HCIAbilityKit][AuditScanAsync] %s"),
		*State.Snapshot.Stats.ToSummaryString());
	HCI_LogAuditRows(TEXT("[HCIAbilityKit][AuditScanAsync]"), State.Snapshot.Rows, State.Controller.GetLogTopN());

	HCI_ResetAuditScanAsyncState(true);
}

static bool HCI_TickAbilityKitAuditScanAsync(float DeltaSeconds)
{
	FHCIAbilityKitAuditScanAsyncState& State = GHCIAbilityKitAuditScanAsyncState;
	if (!State.Controller.IsRunning())
	{
		return false;
	}

	const FHCIAbilityKitAuditScanBatch Batch = State.Controller.DequeueBatch();
	const int32 TotalCount = State.Controller.GetTotalCount();
	if (!Batch.bValid)
	{
		HCI_FinalizeAuditScanAsyncSuccess();
		return false;
	}

	const TArray<FAssetData>& AssetDatas = State.Controller.GetAssetDatas();
	for (int32 Index = Batch.StartIndex; Index < Batch.EndIndex; ++Index)
	{
		if (!AssetDatas.IsValidIndex(Index))
		{
			HCI_ConvergeAuditScanAsyncFailure(TEXT("asset index out of range during scan tick"));
			return false;
		}

		FHCIAbilityKitAuditAssetRow& Row = State.Snapshot.Rows.Emplace_GetRef();
		HCI_ParseAuditRowFromAssetData(AssetDatas[Index], Row);
		HCI_AccumulateAuditCoverage(Row, State.Snapshot.Stats);
	}

	const int32 ProgressPercent = State.Controller.GetProgressPercent();
	const int32 ProgressBucket = ProgressPercent / 10;
	if (ProgressBucket > State.LastLogPercentBucket || State.Controller.GetNextIndex() >= TotalCount)
	{
		State.LastLogPercentBucket = ProgressBucket;
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Display,
			TEXT("[HCIAbilityKit][AuditScanAsync] progress=%d%% processed=%d/%d"),
			ProgressPercent,
			State.Controller.GetNextIndex(),
			TotalCount);
	}

	if (State.Controller.GetNextIndex() < TotalCount)
	{
		return true;
	}

	HCI_FinalizeAuditScanAsyncSuccess();
	return false;
}

static void HCI_RunAbilityKitSearchCommand(const TArray<FString>& Args)
{
	if (Args.Num() == 0)
	{
		UE_LOG(
			LogHCIAbilityKitSearchQuery,
			Warning,
			TEXT("[HCIAbilityKit][SearchQuery] usage: HCIAbilityKit.Search <query text> [topk]"));
		return;
	}

	int32 TopK = 5;
	int32 QueryTokenCount = Args.Num();
	if (Args.Num() >= 2)
	{
		int32 ParsedTopK = 0;
		if (LexTryParseString(ParsedTopK, *Args.Last()))
		{
			TopK = ParsedTopK;
			QueryTokenCount = Args.Num() - 1;
		}
	}

	TArray<FString> QueryTokens;
	QueryTokens.Reserve(QueryTokenCount);
	for (int32 Index = 0; Index < QueryTokenCount; ++Index)
	{
		QueryTokens.Add(Args[Index]);
	}
	const FString QueryText = FString::Join(QueryTokens, TEXT(" "));
	const FHCIAbilitySearchResult QueryResult = FHCIAbilityKitSearchQueryService::RunQuery(
		QueryText,
		TopK,
		FHCIAbilityKitSearchIndexService::Get().GetIndex());

	UE_LOG(
		LogHCIAbilityKitSearchQuery,
		Display,
		TEXT("[HCIAbilityKit][SearchQuery] raw=\"%s\" parsed=%s"),
		*QueryText,
		*QueryResult.ParsedQuery.ToSummaryString());

	UE_LOG(
		LogHCIAbilityKitSearchQuery,
		Display,
		TEXT("[HCIAbilityKit][SearchQuery] %s"),
		*QueryResult.ToSummaryString());

	if (QueryResult.Candidates.Num() > 0)
	{
		for (const FHCIAbilitySearchCandidate& Candidate : QueryResult.Candidates)
		{
			UE_LOG(
				LogHCIAbilityKitSearchQuery,
				Display,
				TEXT("[HCIAbilityKit][SearchQuery] hit id=%s score=%.2f"),
				*Candidate.Id,
				Candidate.Score);
		}
		return;
	}

	for (const FString& Suggestion : QueryResult.Suggestions)
	{
		UE_LOG(
			LogHCIAbilityKitSearchQuery,
			Warning,
			TEXT("[HCIAbilityKit][SearchQuery] suggestion=%s"),
			*Suggestion);
	}
}

static void HCI_RunAbilityKitAuditScanCommand(const TArray<FString>& Args)
{
	int32 LogTopN = 10;
	if (Args.Num() >= 1)
	{
		int32 ParsedTopN = 0;
		if (LexTryParseString(ParsedTopN, *Args[0]))
		{
			LogTopN = FMath::Max(0, ParsedTopN);
		}
	}

	const FHCIAbilityKitAuditScanSnapshot Snapshot = FHCIAbilityKitAuditScanService::Get().ScanFromAssetRegistry();
	UE_LOG(
		LogHCIAbilityKitAuditScan,
		Display,
		TEXT("[HCIAbilityKit][AuditScan] %s"),
		*Snapshot.Stats.ToSummaryString());

	if (Snapshot.Rows.Num() == 0)
	{
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Warning,
			TEXT("[HCIAbilityKit][AuditScan] suggestion=当前未发现 AbilityKit 资产，请先导入至少一个 .hciabilitykit 文件"));
		return;
	}

	HCI_LogAuditRows(TEXT("[HCIAbilityKit][AuditScan]"), Snapshot.Rows, LogTopN);
}

static void HCI_RunAbilityKitAuditScanAsyncCommand(const TArray<FString>& Args)
{
	FHCIAbilityKitAuditScanAsyncState& State = GHCIAbilityKitAuditScanAsyncState;
	if (State.Controller.IsRunning())
	{
		UE_LOG(LogHCIAbilityKitAuditScan, Warning, TEXT("[HCIAbilityKit][AuditScanAsync] scan is already running"));
		return;
	}

	int32 BatchSize = 256;
	int32 LogTopN = 10;
	FString ParseError;
	if (!HCI_ParseAuditScanAsyncArgs(Args, BatchSize, LogTopN, ParseError))
	{
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Error,
			TEXT("[HCIAbilityKit][AuditScanAsync] invalid_args reason=%s usage=HCIAbilityKit.AuditScanAsync [batch_size>=1] [log_top_n>=0]"),
			*ParseError);
		return;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	FARFilter Filter;
	Filter.ClassPaths.Add(UHCIAbilityKitAsset::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;

	TArray<FAssetData> AssetDatas;
	AssetRegistryModule.Get().GetAssets(Filter, AssetDatas);

	FString StartError;
	if (!State.Controller.Start(MoveTemp(AssetDatas), BatchSize, LogTopN, StartError))
	{
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Error,
			TEXT("[HCIAbilityKit][AuditScanAsync] start_failed reason=%s"),
			*StartError);
		return;
	}

	State.LastLogPercentBucket = -1;
	State.StartTimeSeconds = FPlatformTime::Seconds();
	State.Snapshot = FHCIAbilityKitAuditScanSnapshot();
	State.Snapshot.Stats.Source = TEXT("asset_registry_fassetdata_sliced");
	State.Snapshot.Rows.Reserve(State.Controller.GetTotalCount());

	UE_LOG(
		LogHCIAbilityKitAuditScan,
		Display,
		TEXT("[HCIAbilityKit][AuditScanAsync] start total=%d batch_size=%d"),
		State.Controller.GetTotalCount(),
		State.Controller.GetBatchSize());

	if (State.Controller.GetTotalCount() == 0)
	{
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Warning,
			TEXT("[HCIAbilityKit][AuditScanAsync] suggestion=当前未发现 AbilityKit 资产，请先导入至少一个 .hciabilitykit 文件"));
		HCI_ResetAuditScanAsyncState(true);
		return;
	}

	State.TickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateStatic(&HCI_TickAbilityKitAuditScanAsync),
		0.0f);
	if (!State.TickHandle.IsValid())
	{
		HCI_ConvergeAuditScanAsyncFailure(TEXT("failed to register ticker delegate"));
	}
}

static void HCI_RunAbilityKitAuditScanStopCommand(const TArray<FString>& Args)
{
	FHCIAbilityKitAuditScanAsyncState& State = GHCIAbilityKitAuditScanAsyncState;
	if (!State.Controller.IsRunning())
	{
		UE_LOG(LogHCIAbilityKitAuditScan, Display, TEXT("[HCIAbilityKit][AuditScanAsync] stop=ignored reason=not_running"));
		return;
	}

	FString CancelError;
	if (!State.Controller.Cancel(CancelError))
	{
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Warning,
			TEXT("[HCIAbilityKit][AuditScanAsync] stop=failed reason=%s"),
			*CancelError);
		return;
	}

	HCI_StopAuditScanAsyncTicker();
	State.Snapshot.Stats.AssetCount = State.Snapshot.Rows.Num();
	State.Snapshot.Stats.UpdatedUtc = FDateTime::UtcNow();
	State.Snapshot.Stats.DurationMs = (FPlatformTime::Seconds() - State.StartTimeSeconds) * 1000.0;
	UE_LOG(
		LogHCIAbilityKitAuditScan,
		Display,
		TEXT("[HCIAbilityKit][AuditScanAsync] interrupted processed=%d/%d can_retry=true"),
		State.Controller.GetNextIndex(),
		State.Controller.GetTotalCount());
}

static void HCI_RunAbilityKitAuditScanRetryCommand(const TArray<FString>& Args)
{
	FHCIAbilityKitAuditScanAsyncState& State = GHCIAbilityKitAuditScanAsyncState;
	if (State.Controller.IsRunning())
	{
		UE_LOG(LogHCIAbilityKitAuditScan, Warning, TEXT("[HCIAbilityKit][AuditScanAsync] retry=blocked reason=scan_is_running"));
		return;
	}

	FString RetryError;
	if (!State.Controller.Retry(RetryError))
	{
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Warning,
			TEXT("[HCIAbilityKit][AuditScanAsync] retry=unavailable reason=%s"),
			*RetryError);
		return;
	}

	State.LastLogPercentBucket = -1;
	State.StartTimeSeconds = FPlatformTime::Seconds();
	State.Snapshot = FHCIAbilityKitAuditScanSnapshot();
	State.Snapshot.Stats.Source = TEXT("asset_registry_fassetdata_sliced_retry");
	State.Snapshot.Rows.Reserve(State.Controller.GetTotalCount());

	UE_LOG(
		LogHCIAbilityKitAuditScan,
		Display,
		TEXT("[HCIAbilityKit][AuditScanAsync] retry start total=%d batch_size=%d"),
		State.Controller.GetTotalCount(),
		State.Controller.GetBatchSize());

	State.TickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateStatic(&HCI_TickAbilityKitAuditScanAsync),
		0.0f);
	if (!State.TickHandle.IsValid())
	{
		HCI_ConvergeAuditScanAsyncFailure(TEXT("failed to register ticker delegate in retry"));
	}
}

static void HCI_RunAbilityKitAuditScanProgressCommand(const TArray<FString>& Args)
{
	const FHCIAbilityKitAuditScanAsyncState& State = GHCIAbilityKitAuditScanAsyncState;
	if (State.Controller.IsRunning())
	{
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Display,
			TEXT("[HCIAbilityKit][AuditScanAsync] progress=%d%% processed=%d/%d"),
			State.Controller.GetProgressPercent(),
			State.Controller.GetNextIndex(),
			State.Controller.GetTotalCount());
		return;
	}

	switch (State.Controller.GetPhase())
	{
	case EHCIAbilityKitAuditScanAsyncPhase::Cancelled:
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Display,
			TEXT("[HCIAbilityKit][AuditScanAsync] progress=cancelled processed=%d/%d can_retry=%s"),
			State.Controller.GetNextIndex(),
			State.Controller.GetTotalCount(),
			State.Controller.CanRetry() ? TEXT("true") : TEXT("false"));
		return;
	case EHCIAbilityKitAuditScanAsyncPhase::Failed:
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Error,
			TEXT("[HCIAbilityKit][AuditScanAsync] progress=failed processed=%d/%d reason=%s can_retry=%s"),
			State.Controller.GetNextIndex(),
			State.Controller.GetTotalCount(),
			*State.Controller.GetLastFailureReason(),
			State.Controller.CanRetry() ? TEXT("true") : TEXT("false"));
		return;
	default:
		UE_LOG(LogHCIAbilityKitAuditScan, Display, TEXT("[HCIAbilityKit][AuditScanAsync] progress=idle"));
		return;
	}
}

static bool HCI_ParsePythonResponse(
	const FString& ResponseFilename,
	const FString& ScriptPath,
	const FString& SourceFilename,
	const FString& PythonCommandResult,
	FHCIAbilityKitParsedData& InOutParsed,
	FHCIAbilityKitParseError& OutError)
{
	IFileManager& FileManager = IFileManager::Get();
	constexpr int32 MaxWaitAttempts = 50;
	for (int32 Attempt = 0; Attempt < MaxWaitAttempts && !FileManager.FileExists(*ResponseFilename); ++Attempt)
	{
		FPlatformProcess::Sleep(0.01f);
	}

	FString ResponseContent;
	if (!FFileHelper::LoadFileToString(ResponseContent, *ResponseFilename))
	{
		OutError.Code = HCIAbilityKitErrorCodes::PythonError;
		OutError.File = ScriptPath;
		OutError.Field = TEXT("python_response");
		OutError.Reason = TEXT("Python hook response file is missing");
		OutError.Hint = TEXT("Check Python script output and response path");
		OutError.Detail = PythonCommandResult.IsEmpty()
			? ResponseFilename
			: FString::Printf(TEXT("response_file=%s; command_result=%s"), *ResponseFilename, *PythonCommandResult);
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError.Code = HCIAbilityKitErrorCodes::PythonError;
		OutError.File = ScriptPath;
		OutError.Field = TEXT("python_response");
		OutError.Reason = TEXT("Python hook response is not valid JSON");
		OutError.Hint = TEXT("Return valid JSON with {ok, patch, error, audit}");
		OutError.Detail = ResponseContent;
		return false;
	}

	bool bPythonOk = false;
	if (!RootObject->TryGetBoolField(TEXT("ok"), bPythonOk))
	{
		OutError.Code = HCIAbilityKitErrorCodes::PythonError;
		OutError.File = ScriptPath;
		OutError.Field = TEXT("python_response.ok");
		OutError.Reason = TEXT("Python hook response missing required bool field: ok");
		OutError.Hint = TEXT("Set response.ok to true or false");
		return false;
	}

	if (!bPythonOk)
	{
		const TSharedPtr<FJsonObject>* ErrorObject = nullptr;
		if (RootObject->TryGetObjectField(TEXT("error"), ErrorObject) && ErrorObject && ErrorObject->IsValid())
		{
			OutError.Code = HCI_GetJsonStringOrDefault(*ErrorObject, TEXT("code"), HCIAbilityKitErrorCodes::PythonError);
			OutError.File = SourceFilename;
			OutError.Field = HCI_GetJsonStringOrDefault(*ErrorObject, TEXT("field"), TEXT("python_hook"));
			OutError.Reason = HCI_GetJsonStringOrDefault(*ErrorObject, TEXT("reason"), TEXT("Python hook returned failure"));
			OutError.Hint = HCI_GetJsonStringOrDefault(*ErrorObject, TEXT("hint"), TEXT("Inspect Python response error payload"));
			OutError.Detail = HCI_GetJsonStringOrDefault(*ErrorObject, TEXT("detail"), FString());
		}
		else
		{
			OutError.Code = HCIAbilityKitErrorCodes::PythonError;
			OutError.File = SourceFilename;
			OutError.Field = TEXT("python_hook");
			OutError.Reason = TEXT("Python hook returned failure without error payload");
			OutError.Hint = TEXT("Return response.error object when ok=false");
			OutError.Detail = ResponseContent;
		}
		return false;
	}

	const TSharedPtr<FJsonObject>* PatchObject = nullptr;
	if (RootObject->TryGetObjectField(TEXT("patch"), PatchObject) && PatchObject && PatchObject->IsValid())
	{
		FString DisplayNamePatch;
		if ((*PatchObject)->TryGetStringField(TEXT("display_name"), DisplayNamePatch))
		{
			InOutParsed.DisplayName = DisplayNamePatch;
		}

		FString RepresentingMeshPatch;
		if ((*PatchObject)->TryGetStringField(TEXT("representing_mesh"), RepresentingMeshPatch))
		{
			InOutParsed.RepresentingMeshPath = RepresentingMeshPatch;
		}
	}

	return true;
}

static bool HCI_RunPythonHook(const FString& SourceFilename, FHCIAbilityKitParsedData& InOutParsed, FHCIAbilityKitParseError& OutError)
{
	const FString ScriptPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), GHCIAbilityKitPythonScriptPath);
	if (!FPaths::FileExists(ScriptPath))
	{
		OutError.Code = HCIAbilityKitErrorCodes::PythonScriptMissing;
		OutError.File = ScriptPath;
		OutError.Field = TEXT("python_hook");
		OutError.Reason = TEXT("Python hook script not found");
		OutError.Hint = TEXT("Restore script file in project directory");
		return false;
	}

	IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
	if (!PythonPlugin || !PythonPlugin->IsPythonAvailable())
	{
		OutError.Code = HCIAbilityKitErrorCodes::PythonPluginDisabled;
		OutError.File = ScriptPath;
		OutError.Field = TEXT("python_plugin");
		OutError.Reason = TEXT("PythonScriptPlugin is unavailable");
		OutError.Hint = TEXT("Enable PythonScriptPlugin in project settings");
		return false;
	}

	const FString AbsoluteSourceFilename = FPaths::ConvertRelativePathToFull(SourceFilename);
	const FString ResponseDir = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("HCIAbilityKit"), TEXT("Python")));
	IFileManager::Get().MakeDirectory(*ResponseDir, true);
	const FString ResponseFilename = FPaths::Combine(
		ResponseDir,
		FString::Printf(TEXT("hook_response_%s.json"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));

	const FString PythonStatement = FString::Printf(
		TEXT("import runpy, sys; sys.argv=[%s, %s, %s]; runpy.run_path(%s, run_name='__main__')"),
		*HCI_ToPythonStringLiteral(ScriptPath),
		*HCI_ToPythonStringLiteral(AbsoluteSourceFilename),
		*HCI_ToPythonStringLiteral(ResponseFilename),
		*HCI_ToPythonStringLiteral(ScriptPath));

	FPythonCommandEx Command;
	Command.ExecutionMode = EPythonCommandExecutionMode::ExecuteStatement;
	Command.Command = PythonStatement;

	const bool bExecOk = PythonPlugin->ExecPythonCommandEx(Command);
	if (!bExecOk)
	{
		OutError.Code = HCIAbilityKitErrorCodes::PythonError;
		OutError.File = ScriptPath;
		OutError.Field = TEXT("python_execution");
		OutError.Reason = Command.CommandResult.IsEmpty() ? TEXT("Python hook execution failed") : Command.CommandResult;
		OutError.Hint = TEXT("Check Python script for syntax errors or runtime exceptions");
		HCI_DeleteTempFile(ResponseFilename);
		return false;
	}

	const bool bParseOk = HCI_ParsePythonResponse(
		ResponseFilename,
		ScriptPath,
		AbsoluteSourceFilename,
		Command.CommandResult,
		InOutParsed,
		OutError);
	HCI_DeleteTempFile(ResponseFilename);
	return bParseOk;
}

static bool HCI_CanReimportSelectedAbilityKits(const FToolMenuContext& MenuContext)
{
	const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(MenuContext);
	if (!Context || !GHCIAbilityKitFactory)
	{
		return false;
	}

	for (UHCIAbilityKitAsset* Asset : Context->LoadSelectedObjects<UHCIAbilityKitAsset>())
	{
		if (!Asset)
		{
			continue;
		}

		TArray<FString> SourceFiles;
		if (GHCIAbilityKitFactory->CanReimport(Asset, SourceFiles))
		{
			return true;
		}
	}

	return false;
}

static void HCI_ReimportSelectedAbilityKits(const FToolMenuContext& MenuContext)
{
	const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(MenuContext);
	if (!Context || !GHCIAbilityKitFactory)
	{
		return;
	}

	for (UHCIAbilityKitAsset* Asset : Context->LoadSelectedObjects<UHCIAbilityKitAsset>())
	{
		GHCIAbilityKitFactory->Reimport(Asset);
	}
}

void FHCIAbilityKitEditorModule::StartupModule()
{
	FHCIAbilityKitSearchIndexService::Get().RebuildFromAssetRegistry();

	FHCIAbilityKitParserService::SetPythonHook(
		[](const FString& SourceFilename, FHCIAbilityKitParsedData& InOutParsed, FHCIAbilityKitParseError& OutError)
		{
			return HCI_RunPythonHook(SourceFilename, InOutParsed, OutError);
		});

	GHCIAbilityKitFactory = NewObject<UHCIAbilityKitFactory>(GetTransientPackage());
	GHCIAbilityKitFactory->AddToRoot();

	if (!GHCIAbilityKitSearchCommand.IsValid())
	{
		GHCIAbilityKitSearchCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.Search"),
			TEXT("Search AbilityKit assets by natural language. Usage: HCIAbilityKit.Search <query text> [topk]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitSearchCommand));
	}

	if (!GHCIAbilityKitAuditScanCommand.IsValid())
	{
		GHCIAbilityKitAuditScanCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AuditScan"),
			TEXT("Enumerate AbilityKit metadata via AssetRegistry only. Usage: HCIAbilityKit.AuditScan [log_top_n]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAuditScanCommand));
	}

	if (!GHCIAbilityKitAuditScanAsyncCommand.IsValid())
	{
		GHCIAbilityKitAuditScanAsyncCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AuditScanAsync"),
			TEXT("Slice-based non-blocking scan. Usage: HCIAbilityKit.AuditScanAsync [batch_size] [log_top_n]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAuditScanAsyncCommand));
	}

	if (!GHCIAbilityKitAuditScanProgressCommand.IsValid())
	{
		GHCIAbilityKitAuditScanProgressCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AuditScanProgress"),
			TEXT("Print current progress of HCIAbilityKit.AuditScanAsync."),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAuditScanProgressCommand));
	}

	if (!GHCIAbilityKitAuditScanStopCommand.IsValid())
	{
		GHCIAbilityKitAuditScanStopCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AuditScanAsyncStop"),
			TEXT("Interrupt currently running HCIAbilityKit.AuditScanAsync and keep retry context."),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAuditScanStopCommand));
	}

	if (!GHCIAbilityKitAuditScanRetryCommand.IsValid())
	{
		GHCIAbilityKitAuditScanRetryCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AuditScanAsyncRetry"),
			TEXT("Retry last interrupted/failed HCIAbilityKit.AuditScanAsync with previous args."),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAuditScanRetryCommand));
	}
}

void FHCIAbilityKitEditorModule::ShutdownModule()
{
	FHCIAbilityKitParserService::ClearPythonHook();

	if (GHCIAbilityKitFactory)
	{
		GHCIAbilityKitFactory->RemoveFromRoot();
		GHCIAbilityKitFactory = nullptr;
	}

	if (GHCIAbilityKitAuditScanAsyncState.Controller.IsRunning())
	{
		HCI_ResetAuditScanAsyncState(true);
	}
	else
	{
		HCI_StopAuditScanAsyncTicker();
	}

	GHCIAbilityKitSearchCommand.Reset();
	GHCIAbilityKitAuditScanCommand.Reset();
	GHCIAbilityKitAuditScanAsyncCommand.Reset();
	GHCIAbilityKitAuditScanProgressCommand.Reset();
	GHCIAbilityKitAuditScanStopCommand.Reset();
	GHCIAbilityKitAuditScanRetryCommand.Reset();
}

IMPLEMENT_MODULE(FHCIAbilityKitEditorModule, HCIAbilityKitEditor)

static FDelayedAutoRegisterHelper HCIAbilityKitMenuRegister(EDelayedRegisterRunPhase::EndOfEngineInit, []
{
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
	{
		FToolMenuOwnerScoped OwnerScoped(UE_MODULE_NAME);
		UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UHCIAbilityKitAsset::StaticClass());
		FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");

		Section.AddDynamicEntry("HCIAbilityKit_Reimport", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
		{
			const TAttribute<FText> Label = NSLOCTEXT("HCIAbilityKit", "Reimport", "Reimport");
			const TAttribute<FText> ToolTip = NSLOCTEXT("HCIAbilityKit", "Reimport_Tooltip", "Reimport from source .hciabilitykit file.");
			const FSlateIcon Icon(FAppStyle::GetAppStyleSetName(), "ContentBrowser.AssetActions.ReimportAsset");

			FToolUIAction UIAction;
			UIAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&HCI_ReimportSelectedAbilityKits);
			UIAction.CanExecuteAction = FToolMenuCanExecuteAction::CreateStatic(&HCI_CanReimportSelectedAbilityKits);
			InSection.AddMenuEntry("HCIAbilityKit_Reimport", Label, ToolTip, Icon, UIAction);
		}));
	}));
});
