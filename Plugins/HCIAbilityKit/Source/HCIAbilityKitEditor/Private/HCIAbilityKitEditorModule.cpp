#include "HCIAbilityKitEditorModule.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Agent/HCIAbilityKitAgentExecutionGate.h"
#include "Agent/HCIAbilityKitDryRunDiff.h"
#include "Agent/HCIAbilityKitDryRunDiffJsonSerializer.h"
#include "Agent/HCIAbilityKitToolRegistry.h"
#include "Audit/HCIAbilityKitAuditPerfMetrics.h"
#include "Audit/HCIAbilityKitAuditScanAsyncController.h"
#include "Audit/HCIAbilityKitAuditReport.h"
#include "Audit/HCIAbilityKitAuditReportJsonSerializer.h"
#include "Audit/HCIAbilityKitAuditRuleRegistry.h"
#include "Audit/HCIAbilityKitAuditTagNames.h"
#include "ContentBrowserMenuContexts.h"
#include "Dom/JsonObject.h"
#include "Audit/HCIAbilityKitAuditScanService.h"
#include "Common/HCIAbilityKitTimeFormat.h"
#include "Containers/Ticker.h"
#include "Factories/HCIAbilityKitFactory.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/StreamableManager.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformProcess.h"
#include "HCIAbilityKitAsset.h"
#include "HCIAbilityKitErrorCodes.h"
#include "IPythonScriptPlugin.h"
#include "Misc/DelayedAutoRegister.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Search/HCIAbilityKitSearchIndexService.h"
#include "Search/HCIAbilityKitSearchQueryService.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Services/HCIAbilityKitParserService.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "UObject/Package.h"
#include "UObject/GarbageCollection.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogHCIAbilityKitSearchQuery, Log, All);
DEFINE_LOG_CATEGORY_STATIC(LogHCIAbilityKitAuditScan, Log, All);

static TObjectPtr<UHCIAbilityKitFactory> GHCIAbilityKitFactory;
static TUniquePtr<FAutoConsoleCommand> GHCIAbilityKitSearchCommand;
static TUniquePtr<FAutoConsoleCommand> GHCIAbilityKitAuditScanCommand;
static TUniquePtr<FAutoConsoleCommand> GHCIAbilityKitAuditExportJsonCommand;
static TUniquePtr<FAutoConsoleCommand> GHCIAbilityKitAuditScanAsyncCommand;
static TUniquePtr<FAutoConsoleCommand> GHCIAbilityKitAuditScanProgressCommand;
static TUniquePtr<FAutoConsoleCommand> GHCIAbilityKitAuditScanStopCommand;
static TUniquePtr<FAutoConsoleCommand> GHCIAbilityKitAuditScanRetryCommand;
static TUniquePtr<FAutoConsoleCommand> GHCIAbilityKitToolRegistryDumpCommand;
static TUniquePtr<FAutoConsoleCommand> GHCIAbilityKitDryRunDiffPreviewDemoCommand;
static TUniquePtr<FAutoConsoleCommand> GHCIAbilityKitDryRunDiffPreviewLocateCommand;
static TUniquePtr<FAutoConsoleCommand> GHCIAbilityKitDryRunDiffPreviewJsonCommand;
static TUniquePtr<FAutoConsoleCommand> GHCIAbilityKitAgentConfirmGateDemoCommand;
static TUniquePtr<FAutoConsoleCommand> GHCIAbilityKitAgentBlastRadiusDemoCommand;
static TUniquePtr<FAutoConsoleCommand> GHCIAbilityKitAgentTransactionDemoCommand;
static TUniquePtr<FAutoConsoleCommand> GHCIAbilityKitAgentSourceControlDemoCommand;
static TUniquePtr<FAutoConsoleCommand> GHCIAbilityKitAgentRbacDemoCommand;
static const TCHAR* GHCIAbilityKitPythonScriptPath = TEXT("SourceData/AbilityKits/Python/hci_abilitykit_hook.py");

struct FHCIAbilityKitAuditScanAsyncState
{
	FHCIAbilityKitAuditScanAsyncController Controller;
	int32 LastLogPercentBucket = -1;
	double StartTimeSeconds = 0.0;
	bool bDeepMeshCheckEnabled = false;
	int32 GcEveryNBatches = 0;
	int32 ProcessedBatchCount = 0;
	int32 GcRunCount = 0;
	int32 DeepMeshLoadAttemptCount = 0;
	int32 DeepMeshLoadSuccessCount = 0;
	int32 DeepMeshHandleReleaseCount = 0;
	int32 DeepTriangleResolvedCount = 0;
	int32 DeepMeshSignalsResolvedCount = 0;
	double LastBatchDurationMs = 0.0;
	double MaxBatchDurationMs = 0.0;
	double MaxBatchThroughputAssetsPerSec = 0.0;
	double LastGcDurationMs = 0.0;
	double MaxGcDurationMs = 0.0;
	double LastPerfLogTimeSeconds = 0.0;
	int32 LastPerfLogProcessedCount = 0;
	uint64 PeakUsedPhysicalBytes = 0;
	TArray<double> BatchThroughputSamplesAssetsPerSec;
	FHCIAbilityKitAuditScanSnapshot Snapshot;
	FTSTicker::FDelegateHandle TickHandle;
};

static FHCIAbilityKitAuditScanAsyncState GHCIAbilityKitAuditScanAsyncState;
static FHCIAbilityKitDryRunDiffReport GHCIAbilityKitDryRunDiffPreviewState;

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

static FString HCI_GetDefaultAuditReportOutputPath(const FString& RunId)
{
	const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("HCIAbilityKit"), TEXT("AuditReports"));
	return FPaths::Combine(Directory, FString::Printf(TEXT("%s.json"), *RunId));
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

static bool HCI_TryResolveAuditExportOutputPath(const TArray<FString>& Args, const FString& RunId, FString& OutPath, FString& OutError)
{
	FString CandidatePath;
	if (Args.Num() >= 1 && !Args[0].TrimStartAndEnd().IsEmpty())
	{
		CandidatePath = Args[0].TrimStartAndEnd();
		while (CandidatePath.EndsWith(TEXT(";")))
		{
			CandidatePath.LeftChopInline(1, EAllowShrinking::No);
			CandidatePath = CandidatePath.TrimStartAndEnd();
		}
		if (FPaths::IsRelative(CandidatePath))
		{
			CandidatePath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), CandidatePath));
		}
	}
	else
	{
		CandidatePath = HCI_GetDefaultAuditReportOutputPath(RunId);
	}

	const FString Normalized = FPaths::ConvertRelativePathToFull(CandidatePath);
	const FString Directory = FPaths::GetPath(Normalized);
	if (Directory.IsEmpty())
	{
		OutError = TEXT("output path directory is empty");
		return false;
	}

	if (!IFileManager::Get().MakeDirectory(*Directory, true))
	{
		OutError = FString::Printf(TEXT("failed to create output directory: %s"), *Directory);
		return false;
	}

	OutPath = Normalized;
	return true;
}

static bool HCI_TryMarkScanSkippedByState(const FAssetData& AssetData, FHCIAbilityKitAuditAssetRow& OutRow)
{
	const FString PackageName = AssetData.PackageName.ToString();

	if (UPackage* LoadedPackage = FindPackage(nullptr, *PackageName))
	{
		if (LoadedPackage->IsDirty())
		{
			OutRow.ScanState = TEXT("skipped_locked_or_dirty");
			OutRow.SkipReason = TEXT("package_dirty");
			OutRow.TriangleSource = TEXT("unavailable");
			return true;
		}
	}

	const FString PackageFilename = FPackageName::LongPackageNameToFilename(
		PackageName,
		FPackageName::GetAssetPackageExtension());
	if (!PackageFilename.IsEmpty() && IFileManager::Get().FileExists(*PackageFilename) && IFileManager::Get().IsReadOnly(*PackageFilename))
	{
		OutRow.ScanState = TEXT("skipped_locked_or_dirty");
		OutRow.SkipReason = TEXT("package_read_only");
		OutRow.TriangleSource = TEXT("unavailable");
		return true;
	}

	OutRow.ScanState = TEXT("ok");
	OutRow.SkipReason = TEXT("");
	return false;
}

static FString HCI_FormatStringArrayForLog(const TArray<FString>& Values)
{
	if (Values.Num() == 0)
	{
		return TEXT("-");
	}

	return FString::Join(Values, TEXT("|"));
}

static FString HCI_FormatIntArrayForLog(const TArray<int32>& Values)
{
	if (Values.Num() == 0)
	{
		return TEXT("-");
	}

	TArray<FString> Parts;
	Parts.Reserve(Values.Num());
	for (const int32 Value : Values)
	{
		Parts.Add(FString::FromInt(Value));
	}
	return FString::Join(Parts, TEXT("|"));
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

static void HCI_RunAbilityKitToolRegistryDumpCommand(const TArray<FString>& Args)
{
	const FString OptionalToolFilter = (Args.Num() >= 1) ? Args[0].TrimStartAndEnd() : FString();

	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FString ValidationError;
	if (!Registry.ValidateFrozenDefaults(ValidationError))
	{
		UE_LOG(LogHCIAbilityKitAuditScan, Error, TEXT("[HCIAbilityKit][ToolRegistry] invalid reason=%s"), *ValidationError);
		return;
	}

	TArray<FHCIAbilityKitToolDescriptor> Tools = Registry.GetAllTools();
	Tools.Sort([](const FHCIAbilityKitToolDescriptor& A, const FHCIAbilityKitToolDescriptor& B)
	{
		return A.ToolName.LexicalLess(B.ToolName);
	});

	int32 PrintedTools = 0;
	for (const FHCIAbilityKitToolDescriptor& Tool : Tools)
	{
		if (!OptionalToolFilter.IsEmpty() && !Tool.ToolName.ToString().Equals(OptionalToolFilter, ESearchCase::IgnoreCase))
		{
			continue;
		}

		++PrintedTools;
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Display,
			TEXT("[HCIAbilityKit][ToolRegistry] tool=%s capability=%s supports_dry_run=%s supports_undo=%s destructive=%s domain=%s arg_count=%d"),
			*Tool.ToolName.ToString(),
			*FHCIAbilityKitToolRegistry::CapabilityToString(Tool.Capability),
			Tool.bSupportsDryRun ? TEXT("true") : TEXT("false"),
			Tool.bSupportsUndo ? TEXT("true") : TEXT("false"),
			Tool.bDestructive ? TEXT("true") : TEXT("false"),
			Tool.Domain.IsEmpty() ? TEXT("-") : *Tool.Domain,
			Tool.ArgsSchema.Num());

		for (const FHCIAbilityKitToolArgSchema& Arg : Tool.ArgsSchema)
		{
			UE_LOG(
				LogHCIAbilityKitAuditScan,
				Display,
				TEXT("[HCIAbilityKit][ToolRegistry] arg tool=%s name=%s type=%s required=%s array_len=%d..%d str_len=%d..%d int_range=%d..%d str_enum=%s int_enum=%s regex=%s game_path=%s subset_enum=%s"),
				*Tool.ToolName.ToString(),
				*Arg.ArgName.ToString(),
				*FHCIAbilityKitToolRegistry::ArgValueTypeToString(Arg.ValueType),
				Arg.bRequired ? TEXT("true") : TEXT("false"),
				Arg.MinArrayLength,
				Arg.MaxArrayLength,
				Arg.MinStringLength,
				Arg.MaxStringLength,
				Arg.MinIntValue,
				Arg.MaxIntValue,
				*HCI_FormatStringArrayForLog(Arg.AllowedStringValues),
				*HCI_FormatIntArrayForLog(Arg.AllowedIntValues),
				Arg.RegexPattern.IsEmpty() ? TEXT("-") : *Arg.RegexPattern,
				Arg.bMustStartWithGamePath ? TEXT("true") : TEXT("false"),
				Arg.bStringArrayAllowsSubsetOfEnum ? TEXT("true") : TEXT("false"));
		}
	}

	UE_LOG(
		LogHCIAbilityKitAuditScan,
		Display,
		TEXT("[HCIAbilityKit][ToolRegistry] summary total_registered=%d printed=%d filter=%s validation=ok"),
		Tools.Num(),
		PrintedTools,
		OptionalToolFilter.IsEmpty() ? TEXT("-") : *OptionalToolFilter);

	if (!OptionalToolFilter.IsEmpty() && PrintedTools == 0)
	{
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Warning,
			TEXT("[HCIAbilityKit][ToolRegistry] filter_not_found tool=%s"),
			*OptionalToolFilter);
	}
}

static void HCI_LogDryRunDiffPreviewRows(const FHCIAbilityKitDryRunDiffReport& Report)
{
	for (int32 Index = 0; Index < Report.DiffItems.Num(); ++Index)
	{
		const FHCIAbilityKitDryRunDiffItem& Item = Report.DiffItems[Index];
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Display,
			TEXT("[HCIAbilityKit][DryRunDiff] row=%d asset_path=%s field=%s before=%s after=%s tool_name=%s risk=%s skip_reason=%s object_type=%s locate_strategy=%s evidence_key=%s actor_path=%s"),
			Index,
			*Item.AssetPath,
			*Item.Field,
			*Item.Before,
			*Item.After,
			*Item.ToolName,
			*FHCIAbilityKitDryRunDiff::RiskToString(Item.Risk),
			Item.SkipReason.IsEmpty() ? TEXT("-") : *Item.SkipReason,
			*FHCIAbilityKitDryRunDiff::ObjectTypeToString(Item.ObjectType),
			*FHCIAbilityKitDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
			Item.EvidenceKey.IsEmpty() ? TEXT("-") : *Item.EvidenceKey,
			Item.ActorPath.IsEmpty() ? TEXT("-") : *Item.ActorPath);
	}
}

static FString HCI_TryFindFirstAbilityAssetPath()
{
	FHCIAbilityKitAuditScanSnapshot Snapshot = FHCIAbilityKitAuditScanService::Get().ScanFromAssetRegistry();
	for (const FHCIAbilityKitAuditAssetRow& Row : Snapshot.Rows)
	{
		if (!Row.AssetPath.IsEmpty())
		{
			return Row.AssetPath;
		}
	}
	return TEXT("/Game/HCI/Data/TestSkill.TestSkill");
}

static void HCI_BuildDryRunDiffPreviewDemo(FHCIAbilityKitDryRunDiffReport& OutReport)
{
	OutReport = FHCIAbilityKitDryRunDiffReport();
	OutReport.RequestId = FString::Printf(TEXT("req_dryrun_demo_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));

	const FString FirstAbilityAssetPath = HCI_TryFindFirstAbilityAssetPath();

	{
		FHCIAbilityKitDryRunDiffItem& Item = OutReport.DiffItems.Emplace_GetRef();
		Item.AssetPath = FirstAbilityAssetPath;
		Item.Field = TEXT("max_texture_size");
		Item.Before = TEXT("4096");
		Item.After = TEXT("1024");
		Item.ToolName = TEXT("SetTextureMaxSize");
		Item.Risk = EHCIAbilityKitDryRunRisk::Write;
		Item.ObjectType = EHCIAbilityKitDryRunObjectType::Asset;
		Item.EvidenceKey = TEXT("asset_path");
	}

	{
		FHCIAbilityKitDryRunDiffItem& Item = OutReport.DiffItems.Emplace_GetRef();
		Item.AssetPath = FirstAbilityAssetPath;
		Item.Field = TEXT("lod_group");
		Item.Before = TEXT("None");
		Item.After = TEXT("LevelArchitecture");
		Item.ToolName = TEXT("SetMeshLODGroup");
		Item.Risk = EHCIAbilityKitDryRunRisk::Write;
		Item.ObjectType = EHCIAbilityKitDryRunObjectType::Asset;
		Item.EvidenceKey = TEXT("triangle_count_lod0_actual");
	}

	{
		FHCIAbilityKitDryRunDiffItem& Item = OutReport.DiffItems.Emplace_GetRef();
		Item.AssetPath = TEXT("/Game/Maps/OpenWorld.OpenWorld:PersistentLevel.StaticMeshActor_1");
		Item.ActorPath = Item.AssetPath;
		Item.Field = TEXT("missing_collision");
		Item.Before = TEXT("true");
		Item.After = TEXT("would_add_collision");
		Item.ToolName = TEXT("ScanLevelMeshRisks");
		Item.Risk = EHCIAbilityKitDryRunRisk::ReadOnly;
		Item.ObjectType = EHCIAbilityKitDryRunObjectType::Actor;
		Item.EvidenceKey = TEXT("actor_path");
	}

	{
		FHCIAbilityKitDryRunDiffItem& Item = OutReport.DiffItems.Emplace_GetRef();
		Item.AssetPath = FirstAbilityAssetPath;
		Item.Field = TEXT("rename_target");
		Item.Before = TEXT("Temp_Ability_01");
		Item.After = TEXT("SM_Ability_01");
		Item.ToolName = TEXT("RenameAsset");
		Item.Risk = EHCIAbilityKitDryRunRisk::Write;
		Item.ObjectType = EHCIAbilityKitDryRunObjectType::Asset;
		Item.SkipReason = TEXT("dry_run_only_preview");
		Item.EvidenceKey = TEXT("asset_path");
	}

	FHCIAbilityKitDryRunDiff::NormalizeAndFinalize(OutReport);
}

static void HCI_RunAbilityKitDryRunDiffPreviewDemoCommand(const TArray<FString>& Args)
{
	HCI_BuildDryRunDiffPreviewDemo(GHCIAbilityKitDryRunDiffPreviewState);

	const FHCIAbilityKitDryRunDiffReport& Report = GHCIAbilityKitDryRunDiffPreviewState;
	UE_LOG(
		LogHCIAbilityKitAuditScan,
		Display,
		TEXT("[HCIAbilityKit][DryRunDiff] summary request_id=%s total_candidates=%d modifiable=%d skipped=%d"),
		*Report.RequestId,
		Report.Summary.TotalCandidates,
		Report.Summary.Modifiable,
		Report.Summary.Skipped);
	HCI_LogDryRunDiffPreviewRows(Report);
	UE_LOG(
		LogHCIAbilityKitAuditScan,
		Display,
		TEXT("[HCIAbilityKit][DryRunDiff] hint=运行 HCIAbilityKit.DryRunDiffLocate [row_index] 测试定位（actor->camera_focus, asset->sync_browser）"));
}

static void HCI_RunAbilityKitDryRunDiffPreviewJsonCommand(const TArray<FString>& Args)
{
	HCI_BuildDryRunDiffPreviewDemo(GHCIAbilityKitDryRunDiffPreviewState);

	FString JsonText;
	if (!FHCIAbilityKitDryRunDiffJsonSerializer::SerializeToJsonString(GHCIAbilityKitDryRunDiffPreviewState, JsonText))
	{
		UE_LOG(LogHCIAbilityKitAuditScan, Error, TEXT("[HCIAbilityKit][DryRunDiff] export_json_failed reason=serialize_failed"));
		return;
	}

	UE_LOG(LogHCIAbilityKitAuditScan, Display, TEXT("[HCIAbilityKit][DryRunDiff] json=%s"), *JsonText);
}

static bool HCI_TryLocateActorByPathCameraFocus(const FString& ActorPath, FString& OutReason)
{
	if (!GEditor)
	{
		OutReason = TEXT("g_editor_unavailable");
		return false;
	}

	UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
	if (!EditorWorld)
	{
		OutReason = TEXT("editor_world_unavailable");
		return false;
	}

	for (TActorIterator<AActor> It(EditorWorld); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}

		if (Actor->GetPathName().Equals(ActorPath, ESearchCase::CaseSensitive))
		{
			GEditor->MoveViewportCamerasToActor(*Actor, false);
			OutReason = TEXT("ok");
			return true;
		}
	}

	OutReason = TEXT("actor_not_found");
	return false;
}

static bool HCI_TryLocateAssetSyncBrowser(const FString& AssetPath, FString& OutReason)
{
	if (!GEditor)
	{
		OutReason = TEXT("g_editor_unavailable");
		return false;
	}

	FSoftObjectPath SoftPath(AssetPath);
	UObject* Object = SoftPath.ResolveObject();
	if (!Object)
	{
		Object = SoftPath.TryLoad();
	}

	if (!Object)
	{
		OutReason = TEXT("asset_load_failed");
		return false;
	}

	TArray<UObject*> Objects;
	Objects.Add(Object);
	GEditor->SyncBrowserToObjects(Objects);
	OutReason = TEXT("ok");
	return true;
}

static void HCI_RunAbilityKitDryRunDiffPreviewLocateCommand(const TArray<FString>& Args)
{
	if (GHCIAbilityKitDryRunDiffPreviewState.DiffItems.Num() == 0)
	{
		UE_LOG(LogHCIAbilityKitAuditScan, Warning, TEXT("[HCIAbilityKit][DryRunDiff] locate=unavailable reason=no_preview_state"));
		UE_LOG(LogHCIAbilityKitAuditScan, Display, TEXT("[HCIAbilityKit][DryRunDiff] suggestion=先运行 HCIAbilityKit.DryRunDiffPreviewDemo"));
		return;
	}

	int32 RowIndex = 0;
	if (Args.Num() >= 1)
	{
		if (!LexTryParseString(RowIndex, *Args[0]) || RowIndex < 0)
		{
			UE_LOG(LogHCIAbilityKitAuditScan, Error, TEXT("[HCIAbilityKit][DryRunDiff] locate_invalid_args reason=row_index must be integer >= 0"));
			return;
		}
	}

	if (!GHCIAbilityKitDryRunDiffPreviewState.DiffItems.IsValidIndex(RowIndex))
	{
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Error,
			TEXT("[HCIAbilityKit][DryRunDiff] locate_invalid_args reason=row_index out_of_range row_index=%d total=%d"),
			RowIndex,
			GHCIAbilityKitDryRunDiffPreviewState.DiffItems.Num());
		return;
	}

	const FHCIAbilityKitDryRunDiffItem& Item = GHCIAbilityKitDryRunDiffPreviewState.DiffItems[RowIndex];

	FString LocateReason;
	bool bLocateOk = false;
	if (Item.LocateStrategy == EHCIAbilityKitDryRunLocateStrategy::CameraFocus)
	{
		bLocateOk = HCI_TryLocateActorByPathCameraFocus(Item.ActorPath.IsEmpty() ? Item.AssetPath : Item.ActorPath, LocateReason);
	}
	else
	{
		bLocateOk = HCI_TryLocateAssetSyncBrowser(Item.AssetPath, LocateReason);
	}

	if (bLocateOk)
	{
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Display,
			TEXT("[HCIAbilityKit][DryRunDiff] locate row=%d strategy=%s object_type=%s success=%s reason=%s asset_path=%s actor_path=%s"),
			RowIndex,
			*FHCIAbilityKitDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
			*FHCIAbilityKitDryRunDiff::ObjectTypeToString(Item.ObjectType),
			TEXT("true"),
			*LocateReason,
			*Item.AssetPath,
			Item.ActorPath.IsEmpty() ? TEXT("-") : *Item.ActorPath);
	}
	else
	{
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Warning,
			TEXT("[HCIAbilityKit][DryRunDiff] locate row=%d strategy=%s object_type=%s success=%s reason=%s asset_path=%s actor_path=%s"),
			RowIndex,
			*FHCIAbilityKitDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
			*FHCIAbilityKitDryRunDiff::ObjectTypeToString(Item.ObjectType),
			TEXT("false"),
			*LocateReason,
			*Item.AssetPath,
			Item.ActorPath.IsEmpty() ? TEXT("-") : *Item.ActorPath);
	}
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

static void HCI_TryFillTriangleFromTagCached(
	const FAssetData& AssetData,
	IAssetRegistry* AssetRegistry,
	FHCIAbilityKitAuditAssetRow& OutRow)
{
	if (AssetRegistry == nullptr)
	{
		OutRow.TriangleSource = TEXT("unavailable");
		return;
	}

	FName SourceTagKey;
	if (HCIAbilityKitAuditTagNames::TryResolveTriangleCountFromTags(
			[&AssetData](const FName& TagName, FString& OutValue)
			{
				return AssetData.GetTagValue(TagName, OutValue);
			},
			OutRow.TriangleCountLod0Actual,
			SourceTagKey))
	{
		OutRow.TriangleSource = TEXT("tag_cached");
		OutRow.TriangleSourceTagKey = SourceTagKey.ToString();
		return;
	}

	if (OutRow.RepresentingMeshPath.IsEmpty())
	{
		OutRow.TriangleSource = TEXT("unavailable");
		return;
	}

	const FSoftObjectPath RepresentingMeshObjectPath(OutRow.RepresentingMeshPath);
	if (!RepresentingMeshObjectPath.IsValid())
	{
		OutRow.TriangleSource = TEXT("unavailable");
		return;
	}

	const FAssetData MeshAssetData = AssetRegistry->GetAssetByObjectPath(RepresentingMeshObjectPath);
	if (!MeshAssetData.IsValid())
	{
		OutRow.TriangleSource = TEXT("unavailable");
		return;
	}

	FName MeshLodTagKey;
	if (HCIAbilityKitAuditTagNames::TryResolveMeshLodCountFromTags(
			[&MeshAssetData](const FName& TagName, FString& OutValue)
			{
				return MeshAssetData.GetTagValue(TagName, OutValue);
			},
			OutRow.MeshLodCount,
			MeshLodTagKey))
	{
		OutRow.MeshLodCountTagKey = MeshLodTagKey.ToString();
	}

	FName MeshNaniteTagKey;
	if (HCIAbilityKitAuditTagNames::TryResolveMeshNaniteEnabledFromTags(
			[&MeshAssetData](const FName& TagName, FString& OutValue)
			{
				return MeshAssetData.GetTagValue(TagName, OutValue);
			},
			OutRow.bMeshNaniteEnabled,
			MeshNaniteTagKey))
	{
		OutRow.bMeshNaniteEnabledKnown = true;
		OutRow.MeshNaniteTagKey = MeshNaniteTagKey.ToString();
	}

	if (HCIAbilityKitAuditTagNames::TryResolveTriangleCountFromTags(
			[&MeshAssetData](const FName& TagName, FString& OutValue)
			{
				return MeshAssetData.GetTagValue(TagName, OutValue);
			},
			OutRow.TriangleCountLod0Actual,
			SourceTagKey))
	{
		OutRow.TriangleSource = TEXT("tag_cached");
		OutRow.TriangleSourceTagKey = SourceTagKey.ToString();
		return;
	}

	OutRow.TriangleSource = TEXT("unavailable");
}

struct FHCIAbilityKitAuditDeepMeshBatchStats
{
	int32 LoadAttemptCount = 0;
	int32 LoadSuccessCount = 0;
	int32 HandleReleaseCount = 0;
	int32 TriangleResolvedCount = 0;
	int32 MeshSignalsResolvedCount = 0;
};

static bool HCI_ShouldRunDeepMeshCheckForRow(const FHCIAbilityKitAuditAssetRow& Row)
{
	if (Row.ScanState != TEXT("ok"))
	{
		return false;
	}

	if (Row.RepresentingMeshPath.IsEmpty())
	{
		return false;
	}

	return Row.TriangleCountLod0Actual < 0 || Row.MeshLodCount < 0 || !Row.bMeshNaniteEnabledKnown;
}

static bool HCI_TryResolveAuditSignalsFromLoadedStaticMesh(UStaticMesh* StaticMesh, FHCIAbilityKitAuditAssetRow& OutRow)
{
	if (StaticMesh == nullptr)
	{
		return false;
	}

	bool bResolvedAny = false;
	bool bResolvedMeshSignals = false;

	if (OutRow.MeshLodCount < 0)
	{
		OutRow.MeshLodCount = StaticMesh->GetNumLODs();
		bResolvedAny = true;
		bResolvedMeshSignals = true;
	}

	if (!OutRow.bMeshNaniteEnabledKnown)
	{
		OutRow.bMeshNaniteEnabled = StaticMesh->NaniteSettings.bEnabled;
		OutRow.bMeshNaniteEnabledKnown = true;
		bResolvedAny = true;
		bResolvedMeshSignals = true;
	}

	if (OutRow.TriangleCountLod0Actual < 0 && OutRow.MeshLodCount > 0)
	{
		OutRow.TriangleCountLod0Actual = StaticMesh->GetNumTriangles(0);
		OutRow.TriangleSource = TEXT("mesh_loaded");
		OutRow.TriangleSourceTagKey.Reset();
		bResolvedAny = true;
	}

	if (bResolvedMeshSignals)
	{
		OutRow.MeshLodCountTagKey.Reset();
		OutRow.MeshNaniteTagKey.Reset();
	}

	return bResolvedAny;
}

static void HCI_TryEnrichAuditRowByDeepMeshLoad(
	FHCIAbilityKitAuditAssetRow& InOutRow,
	FHCIAbilityKitAuditDeepMeshBatchStats& InOutBatchStats)
{
	if (!HCI_ShouldRunDeepMeshCheckForRow(InOutRow))
	{
		return;
	}

	const FSoftObjectPath MeshObjectPath(InOutRow.RepresentingMeshPath);
	if (!MeshObjectPath.IsValid())
	{
		return;
	}

	++InOutBatchStats.LoadAttemptCount;

	static FStreamableManager GHCIAuditDeepScanStreamableManager;
	TSharedPtr<FStreamableHandle> LoadHandle = GHCIAuditDeepScanStreamableManager.RequestSyncLoad(MeshObjectPath, false);
	UObject* LoadedObject = LoadHandle.IsValid() ? LoadHandle->GetLoadedAsset() : MeshObjectPath.ResolveObject();

	if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(LoadedObject))
	{
		const bool bTriangleBeforeResolved = InOutRow.TriangleCountLod0Actual >= 0;
		const bool bMeshSignalsBeforeResolved = (InOutRow.MeshLodCount >= 0) && InOutRow.bMeshNaniteEnabledKnown;
		const bool bResolvedAny = HCI_TryResolveAuditSignalsFromLoadedStaticMesh(StaticMesh, InOutRow);
		if (bResolvedAny)
		{
			++InOutBatchStats.LoadSuccessCount;
			if (!bTriangleBeforeResolved && InOutRow.TriangleCountLod0Actual >= 0 && InOutRow.TriangleSource == TEXT("mesh_loaded"))
			{
				++InOutBatchStats.TriangleResolvedCount;
			}
			if (!bMeshSignalsBeforeResolved && InOutRow.MeshLodCount >= 0 && InOutRow.bMeshNaniteEnabledKnown)
			{
				++InOutBatchStats.MeshSignalsResolvedCount;
			}
		}
	}

	if (LoadHandle.IsValid())
	{
		LoadHandle->ReleaseHandle();
		LoadHandle.Reset();
		++InOutBatchStats.HandleReleaseCount;
	}
}

static void HCI_ParseAuditRowFromAssetData(
	const FAssetData& AssetData,
	IAssetRegistry* AssetRegistry,
	FHCIAbilityKitAuditAssetRow& OutRow)
{
	OutRow.AssetPath = AssetData.GetObjectPathString();
	OutRow.AssetName = AssetData.AssetName.ToString();
	OutRow.AssetClass = AssetData.AssetClassPath.GetAssetName().ToString();

	AssetData.GetTagValue(HCIAbilityKitAuditTagNames::Id, OutRow.Id);
	AssetData.GetTagValue(HCIAbilityKitAuditTagNames::DisplayName, OutRow.DisplayName);
	AssetData.GetTagValue(HCIAbilityKitAuditTagNames::RepresentingMesh, OutRow.RepresentingMeshPath);

	FName TextureDimensionsTagKey;
	if (HCIAbilityKitAuditTagNames::TryResolveTextureDimensionsFromTags(
			[&AssetData](const FName& TagName, FString& OutValue)
			{
				return AssetData.GetTagValue(TagName, OutValue);
			},
			OutRow.TextureWidth,
			OutRow.TextureHeight,
			TextureDimensionsTagKey))
	{
		OutRow.TextureDimensionsTagKey = TextureDimensionsTagKey.ToString();
	}

	FName TriangleExpectedTagKey;
	HCIAbilityKitAuditTagNames::TryResolveTriangleExpectedFromTags(
		[&AssetData](const FName& TagName, FString& OutValue)
		{
			return AssetData.GetTagValue(TagName, OutValue);
		},
		OutRow.TriangleCountLod0ExpectedJson,
		TriangleExpectedTagKey);
	(void)TriangleExpectedTagKey;

	FString DamageText;
	if (AssetData.GetTagValue(HCIAbilityKitAuditTagNames::Damage, DamageText))
	{
		LexTryParseString(OutRow.Damage, *DamageText);
	}

	if (HCI_TryMarkScanSkippedByState(AssetData, OutRow))
	{
		return;
	}

	HCI_TryFillTriangleFromTagCached(AssetData, AssetRegistry, OutRow);
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
	if (Row.TriangleSource == TEXT("tag_cached") && Row.TriangleCountLod0Actual >= 0)
	{
		++InOutStats.TriangleTagCoveredCount;
	}
	if (Row.ScanState == TEXT("skipped_locked_or_dirty"))
	{
		++InOutStats.SkippedLockedOrDirtyCount;
	}
	if (Row.AuditIssues.Num() > 0)
	{
		++InOutStats.AssetsWithIssuesCount;
		for (const FHCIAbilityKitAuditIssue& Issue : Row.AuditIssues)
		{
			switch (Issue.Severity)
			{
			case EHCIAbilityKitAuditSeverity::Info:
				++InOutStats.InfoIssueCount;
				break;
			case EHCIAbilityKitAuditSeverity::Warn:
				++InOutStats.WarnIssueCount;
				break;
			case EHCIAbilityKitAuditSeverity::Error:
				++InOutStats.ErrorIssueCount;
				break;
			default:
				break;
			}
		}
	}
}

static void HCI_LogAuditRows(const TCHAR* Prefix, const TArray<FHCIAbilityKitAuditAssetRow>& Rows, const int32 LogTopN)
{
	const int32 CountToLog = FMath::Min(LogTopN, Rows.Num());
	for (int32 Index = 0; Index < CountToLog; ++Index)
	{
		const FHCIAbilityKitAuditAssetRow& Row = Rows[Index];
		const FHCIAbilityKitAuditIssue* FirstIssue = Row.AuditIssues.Num() > 0 ? &Row.AuditIssues[0] : nullptr;
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Display,
			TEXT("%s row=%d asset=%s class=%s id=%s display_name=%s damage=%.2f representing_mesh=%s triangle_count_lod0=%d triangle_expected_lod0=%d mesh_lods=%d mesh_nanite=%s tex_dims=%dx%d triangle_source=%s triangle_source_tag=%s scan_state=%s skip_reason=%s audit_issue_count=%d first_issue_rule=%s first_issue_severity=%d first_issue_severity_name=%s"),
			Prefix,
			Index,
			*Row.AssetPath,
			*Row.AssetClass,
			*Row.Id,
			*Row.DisplayName,
			Row.Damage,
			*Row.RepresentingMeshPath,
			Row.TriangleCountLod0Actual,
			Row.TriangleCountLod0ExpectedJson,
			Row.MeshLodCount,
			Row.bMeshNaniteEnabledKnown ? (Row.bMeshNaniteEnabled ? TEXT("true") : TEXT("false")) : TEXT("unknown"),
			Row.TextureWidth,
			Row.TextureHeight,
			*Row.TriangleSource,
			*Row.TriangleSourceTagKey,
			*Row.ScanState,
			*Row.SkipReason,
			Row.AuditIssues.Num(),
			FirstIssue ? *FirstIssue->RuleId.ToString() : TEXT(""),
			FirstIssue ? static_cast<int32>(FirstIssue->Severity) : -1,
			FirstIssue ? *FHCIAbilityKitAuditReportBuilder::SeverityToString(FirstIssue->Severity) : TEXT(""));
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
	GHCIAbilityKitAuditScanAsyncState.bDeepMeshCheckEnabled = false;
	GHCIAbilityKitAuditScanAsyncState.GcEveryNBatches = 0;
	GHCIAbilityKitAuditScanAsyncState.ProcessedBatchCount = 0;
	GHCIAbilityKitAuditScanAsyncState.GcRunCount = 0;
	GHCIAbilityKitAuditScanAsyncState.DeepMeshLoadAttemptCount = 0;
	GHCIAbilityKitAuditScanAsyncState.DeepMeshLoadSuccessCount = 0;
	GHCIAbilityKitAuditScanAsyncState.DeepMeshHandleReleaseCount = 0;
	GHCIAbilityKitAuditScanAsyncState.DeepTriangleResolvedCount = 0;
	GHCIAbilityKitAuditScanAsyncState.DeepMeshSignalsResolvedCount = 0;
	GHCIAbilityKitAuditScanAsyncState.LastBatchDurationMs = 0.0;
	GHCIAbilityKitAuditScanAsyncState.MaxBatchDurationMs = 0.0;
	GHCIAbilityKitAuditScanAsyncState.MaxBatchThroughputAssetsPerSec = 0.0;
	GHCIAbilityKitAuditScanAsyncState.LastGcDurationMs = 0.0;
	GHCIAbilityKitAuditScanAsyncState.MaxGcDurationMs = 0.0;
	GHCIAbilityKitAuditScanAsyncState.LastPerfLogTimeSeconds = 0.0;
	GHCIAbilityKitAuditScanAsyncState.LastPerfLogProcessedCount = 0;
	GHCIAbilityKitAuditScanAsyncState.PeakUsedPhysicalBytes = 0;
	GHCIAbilityKitAuditScanAsyncState.BatchThroughputSamplesAssetsPerSec.Reset();
	GHCIAbilityKitAuditScanAsyncState.Snapshot = FHCIAbilityKitAuditScanSnapshot();
}

static bool HCI_ParseAuditScanAsyncArgs(
	const TArray<FString>& Args,
	int32& OutBatchSize,
	int32& OutLogTopN,
	bool& OutDeepMeshCheckEnabled,
	int32& OutGcEveryNBatches,
	FString& OutError)
{
	OutBatchSize = 256;
	OutLogTopN = 10;
	OutDeepMeshCheckEnabled = false;
	OutGcEveryNBatches = 0;

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

	if (Args.Num() >= 3)
	{
		const FString Normalized = Args[2].TrimStartAndEnd();
		if (Normalized.Equals(TEXT("1")) || Normalized.Equals(TEXT("true"), ESearchCase::IgnoreCase))
		{
			OutDeepMeshCheckEnabled = true;
		}
		else if (Normalized.Equals(TEXT("0")) || Normalized.Equals(TEXT("false"), ESearchCase::IgnoreCase))
		{
			OutDeepMeshCheckEnabled = false;
		}
		else
		{
			OutError = TEXT("deep_mesh_check must be 0/1/false/true");
			return false;
		}
	}

	if (Args.Num() >= 4)
	{
		int32 ParsedGcEveryNBatches = 0;
		if (!LexTryParseString(ParsedGcEveryNBatches, *Args[3]) || ParsedGcEveryNBatches < 0)
		{
			OutError = TEXT("gc_every_n_batches must be an integer >= 0");
			return false;
		}
		OutGcEveryNBatches = ParsedGcEveryNBatches;
	}
	else if (OutDeepMeshCheckEnabled)
	{
		// D2 default: enable conservative GC throttling only in deep mesh mode.
		OutGcEveryNBatches = 16;
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

static void HCI_LogAuditScanAsyncPerfProgress(const int32 ProgressPercent)
{
	FHCIAbilityKitAuditScanAsyncState& State = GHCIAbilityKitAuditScanAsyncState;
	const int32 TotalCount = State.Controller.GetTotalCount();
	const int32 ProcessedCount = State.Controller.GetNextIndex();
	const double NowSeconds = FPlatformTime::Seconds();
	const double ElapsedMs = (NowSeconds - State.StartTimeSeconds) * 1000.0;
	const double AvgAssetsPerSec = FHCIAbilityKitAuditPerfMetrics::AssetsPerSecond(ProcessedCount, ElapsedMs);

	double WindowAssetsPerSec = AvgAssetsPerSec;
	if (State.LastPerfLogTimeSeconds > 0.0)
	{
		const double WindowMs = (NowSeconds - State.LastPerfLogTimeSeconds) * 1000.0;
		const int32 WindowProcessed = FMath::Max(0, ProcessedCount - State.LastPerfLogProcessedCount);
		WindowAssetsPerSec = FHCIAbilityKitAuditPerfMetrics::AssetsPerSecond(WindowProcessed, WindowMs);
	}

	const int32 RemainingCount = FMath::Max(0, TotalCount - ProcessedCount);
	const double EtaSeconds = AvgAssetsPerSec > UE_SMALL_NUMBER
		? (static_cast<double>(RemainingCount) / AvgAssetsPerSec)
		: -1.0;

	const FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();
	const double UsedPhysicalMiB = static_cast<double>(MemoryStats.UsedPhysical) / (1024.0 * 1024.0);
	const double PeakUsedPhysicalMiB = static_cast<double>(State.PeakUsedPhysicalBytes) / (1024.0 * 1024.0);

	UE_LOG(
		LogHCIAbilityKitAuditScan,
		Display,
		TEXT("[HCIAbilityKit][AuditScanAsync][Perf] progress=%d%% processed=%d/%d elapsed_ms=%.2f avg_assets_per_sec=%.2f window_assets_per_sec=%.2f eta_s=%.2f used_physical_mib=%.1f peak_used_physical_mib=%.1f gc_runs=%d"),
		ProgressPercent,
		ProcessedCount,
		TotalCount,
		ElapsedMs,
		AvgAssetsPerSec,
		WindowAssetsPerSec,
		EtaSeconds,
		UsedPhysicalMiB,
		PeakUsedPhysicalMiB,
		State.GcRunCount);

	State.LastPerfLogTimeSeconds = NowSeconds;
	State.LastPerfLogProcessedCount = ProcessedCount;
}

static void HCI_LogAuditScanAsyncPerfFinalSummary()
{
	FHCIAbilityKitAuditScanAsyncState& State = GHCIAbilityKitAuditScanAsyncState;
	const int32 AssetCount = State.Snapshot.Rows.Num();
	const double DurationMs = State.Snapshot.Stats.DurationMs;
	const double AvgAssetsPerSec = FHCIAbilityKitAuditPerfMetrics::AssetsPerSecond(AssetCount, DurationMs);
	const double P50BatchAssetsPerSec = FHCIAbilityKitAuditPerfMetrics::PercentileNearestRank(State.BatchThroughputSamplesAssetsPerSec, 50.0);
	const double P95BatchAssetsPerSec = FHCIAbilityKitAuditPerfMetrics::PercentileNearestRank(State.BatchThroughputSamplesAssetsPerSec, 95.0);
	const double PeakUsedPhysicalMiB = static_cast<double>(State.PeakUsedPhysicalBytes) / (1024.0 * 1024.0);

	UE_LOG(
		LogHCIAbilityKitAuditScan,
		Display,
		TEXT("[HCIAbilityKit][AuditScanAsync][PerfSummary] assets=%d batches=%d duration_ms=%.2f avg_assets_per_sec=%.2f p50_batch_assets_per_sec=%.2f p95_batch_assets_per_sec=%.2f max_batch_assets_per_sec=%.2f peak_used_physical_mib=%.1f"),
		AssetCount,
		State.ProcessedBatchCount,
		DurationMs,
		AvgAssetsPerSec,
		P50BatchAssetsPerSec,
		P95BatchAssetsPerSec,
		State.MaxBatchThroughputAssetsPerSec,
		PeakUsedPhysicalMiB);
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
	HCI_LogAuditScanAsyncPerfFinalSummary();
	if (State.bDeepMeshCheckEnabled)
	{
		const double PeakUsedPhysicalMiB = static_cast<double>(State.PeakUsedPhysicalBytes) / (1024.0 * 1024.0);
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Display,
			TEXT("[HCIAbilityKit][AuditScanAsync][Deep] load_attempts=%d load_success=%d handle_releases=%d triangle_resolved=%d mesh_signals_resolved=%d batches=%d gc_every_n_batches=%d gc_runs=%d max_batch_ms=%.2f max_gc_ms=%.2f peak_used_physical_mib=%.1f"),
			State.DeepMeshLoadAttemptCount,
			State.DeepMeshLoadSuccessCount,
			State.DeepMeshHandleReleaseCount,
			State.DeepTriangleResolvedCount,
			State.DeepMeshSignalsResolvedCount,
			State.ProcessedBatchCount,
			State.GcEveryNBatches,
			State.GcRunCount,
			State.MaxBatchDurationMs,
			State.MaxGcDurationMs,
			PeakUsedPhysicalMiB);
	}
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
	const double BatchStartSeconds = FPlatformTime::Seconds();
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry* AssetRegistry = &AssetRegistryModule.Get();
	FHCIAbilityKitAuditDeepMeshBatchStats DeepBatchStats;
	for (int32 Index = Batch.StartIndex; Index < Batch.EndIndex; ++Index)
	{
		if (!AssetDatas.IsValidIndex(Index))
		{
			HCI_ConvergeAuditScanAsyncFailure(TEXT("asset index out of range during scan tick"));
			return false;
		}

		FHCIAbilityKitAuditAssetRow& Row = State.Snapshot.Rows.Emplace_GetRef();
		HCI_ParseAuditRowFromAssetData(AssetDatas[Index], AssetRegistry, Row);
		if (State.bDeepMeshCheckEnabled)
		{
			HCI_TryEnrichAuditRowByDeepMeshLoad(Row, DeepBatchStats);
		}
		const FHCIAbilityKitAuditContext Context{Row};
		FHCIAbilityKitAuditRuleRegistry::Get().Evaluate(Context, Row.AuditIssues);
		HCI_AccumulateAuditCoverage(Row, State.Snapshot.Stats);
	}
	if (State.bDeepMeshCheckEnabled)
	{
		State.DeepMeshLoadAttemptCount += DeepBatchStats.LoadAttemptCount;
		State.DeepMeshLoadSuccessCount += DeepBatchStats.LoadSuccessCount;
		State.DeepMeshHandleReleaseCount += DeepBatchStats.HandleReleaseCount;
		State.DeepTriangleResolvedCount += DeepBatchStats.TriangleResolvedCount;
		State.DeepMeshSignalsResolvedCount += DeepBatchStats.MeshSignalsResolvedCount;
	}

	State.ProcessedBatchCount += 1;
	const int32 BatchAssetCount = Batch.EndIndex - Batch.StartIndex;
	State.LastBatchDurationMs = (FPlatformTime::Seconds() - BatchStartSeconds) * 1000.0;
	State.MaxBatchDurationMs = FMath::Max(State.MaxBatchDurationMs, State.LastBatchDurationMs);
	const double BatchAssetsPerSec = FHCIAbilityKitAuditPerfMetrics::AssetsPerSecond(BatchAssetCount, State.LastBatchDurationMs);
	if (BatchAssetsPerSec > 0.0)
	{
		State.BatchThroughputSamplesAssetsPerSec.Add(BatchAssetsPerSec);
		State.MaxBatchThroughputAssetsPerSec = FMath::Max(State.MaxBatchThroughputAssetsPerSec, BatchAssetsPerSec);
	}

	const FPlatformMemoryStats BatchMemoryStats = FPlatformMemory::GetStats();
	State.PeakUsedPhysicalBytes = FMath::Max<uint64>(State.PeakUsedPhysicalBytes, static_cast<uint64>(BatchMemoryStats.UsedPhysical));

	if (State.bDeepMeshCheckEnabled
		&& State.GcEveryNBatches > 0
		&& (State.ProcessedBatchCount % State.GcEveryNBatches) == 0)
	{
		const double GcStartSeconds = FPlatformTime::Seconds();
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		State.LastGcDurationMs = (FPlatformTime::Seconds() - GcStartSeconds) * 1000.0;
		State.MaxGcDurationMs = FMath::Max(State.MaxGcDurationMs, State.LastGcDurationMs);
		State.GcRunCount += 1;

		const FPlatformMemoryStats PostGcMemoryStats = FPlatformMemory::GetStats();
		State.PeakUsedPhysicalBytes = FMath::Max<uint64>(State.PeakUsedPhysicalBytes, static_cast<uint64>(PostGcMemoryStats.UsedPhysical));
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
		HCI_LogAuditScanAsyncPerfProgress(ProgressPercent);
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

static void HCI_RunAbilityKitAuditExportJsonCommand(const TArray<FString>& Args)
{
	const FHCIAbilityKitAuditScanSnapshot Snapshot = FHCIAbilityKitAuditScanService::Get().ScanFromAssetRegistry();
	const FHCIAbilityKitAuditReport Report = FHCIAbilityKitAuditReportBuilder::BuildFromSnapshot(Snapshot);

	FString OutputPath;
	FString ResolveError;
	if (!HCI_TryResolveAuditExportOutputPath(Args, Report.RunId, OutputPath, ResolveError))
	{
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Error,
			TEXT("[HCIAbilityKit][AuditExportJson] failed reason=%s"),
			*ResolveError);
		return;
	}

	FString JsonText;
	if (!FHCIAbilityKitAuditReportJsonSerializer::SerializeToJsonString(Report, JsonText))
	{
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Error,
			TEXT("[HCIAbilityKit][AuditExportJson] failed reason=serialize_to_json_failed"));
		return;
	}

	if (!FFileHelper::SaveStringToFile(JsonText, *OutputPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Error,
			TEXT("[HCIAbilityKit][AuditExportJson] failed reason=save_to_file_failed path=%s"),
			*OutputPath);
		return;
	}

	UE_LOG(
		LogHCIAbilityKitAuditScan,
		Display,
		TEXT("[HCIAbilityKit][AuditExportJson] success path=%s run_id=%s results=%d issue_assets=%d source=%s"),
		*OutputPath,
		*Report.RunId,
		Report.Results.Num(),
		Snapshot.Stats.AssetsWithIssuesCount,
		*Report.Source);

	if (Snapshot.Rows.Num() == 0)
	{
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Warning,
			TEXT("[HCIAbilityKit][AuditExportJson] suggestion=当前未发现 AbilityKit 资产，请先导入至少一个 .hciabilitykit 文件"));
	}
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
	bool bDeepMeshCheckEnabled = false;
	int32 GcEveryNBatches = 0;
	FString ParseError;
	if (!HCI_ParseAuditScanAsyncArgs(Args, BatchSize, LogTopN, bDeepMeshCheckEnabled, GcEveryNBatches, ParseError))
	{
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Error,
			TEXT("[HCIAbilityKit][AuditScanAsync] invalid_args reason=%s usage=HCIAbilityKit.AuditScanAsync [batch_size>=1] [log_top_n>=0] [deep_mesh_check=0|1] [gc_every_n_batches>=0]"),
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
	if (!State.Controller.Start(MoveTemp(AssetDatas), BatchSize, LogTopN, bDeepMeshCheckEnabled, GcEveryNBatches, StartError))
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
	State.bDeepMeshCheckEnabled = bDeepMeshCheckEnabled;
	State.GcEveryNBatches = GcEveryNBatches;
	State.ProcessedBatchCount = 0;
	State.GcRunCount = 0;
	State.DeepMeshLoadAttemptCount = 0;
	State.DeepMeshLoadSuccessCount = 0;
	State.DeepMeshHandleReleaseCount = 0;
	State.DeepTriangleResolvedCount = 0;
	State.DeepMeshSignalsResolvedCount = 0;
	State.LastBatchDurationMs = 0.0;
	State.MaxBatchDurationMs = 0.0;
	State.MaxBatchThroughputAssetsPerSec = 0.0;
	State.LastGcDurationMs = 0.0;
	State.MaxGcDurationMs = 0.0;
	State.LastPerfLogTimeSeconds = 0.0;
	State.LastPerfLogProcessedCount = 0;
	State.PeakUsedPhysicalBytes = 0;
	State.BatchThroughputSamplesAssetsPerSec.Reset();
	State.Snapshot = FHCIAbilityKitAuditScanSnapshot();
	State.Snapshot.Stats.Source = bDeepMeshCheckEnabled
		? (GcEveryNBatches > 0 ? TEXT("asset_registry_fassetdata_sliced_deepmesh_gc") : TEXT("asset_registry_fassetdata_sliced_deepmesh"))
		: TEXT("asset_registry_fassetdata_sliced");
	State.Snapshot.Rows.Reserve(State.Controller.GetTotalCount());
	State.BatchThroughputSamplesAssetsPerSec.Reserve(FMath::Max(16, State.Controller.GetTotalCount() / FMath::Max(1, State.Controller.GetBatchSize()) + 1));

	UE_LOG(
		LogHCIAbilityKitAuditScan,
		Display,
		TEXT("[HCIAbilityKit][AuditScanAsync] start total=%d batch_size=%d deep_mesh_check=%s gc_every_n_batches=%d"),
		State.Controller.GetTotalCount(),
		State.Controller.GetBatchSize(),
		State.bDeepMeshCheckEnabled ? TEXT("true") : TEXT("false"),
		State.GcEveryNBatches);

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
	State.bDeepMeshCheckEnabled = State.Controller.IsDeepMeshCheckEnabled();
	State.GcEveryNBatches = State.Controller.GetGcEveryNBatches();
	State.ProcessedBatchCount = 0;
	State.GcRunCount = 0;
	State.DeepMeshLoadAttemptCount = 0;
	State.DeepMeshLoadSuccessCount = 0;
	State.DeepMeshHandleReleaseCount = 0;
	State.DeepTriangleResolvedCount = 0;
	State.DeepMeshSignalsResolvedCount = 0;
	State.LastBatchDurationMs = 0.0;
	State.MaxBatchDurationMs = 0.0;
	State.MaxBatchThroughputAssetsPerSec = 0.0;
	State.LastGcDurationMs = 0.0;
	State.MaxGcDurationMs = 0.0;
	State.LastPerfLogTimeSeconds = 0.0;
	State.LastPerfLogProcessedCount = 0;
	State.PeakUsedPhysicalBytes = 0;
	State.BatchThroughputSamplesAssetsPerSec.Reset();
	State.Snapshot = FHCIAbilityKitAuditScanSnapshot();
	State.Snapshot.Stats.Source = State.bDeepMeshCheckEnabled
		? (State.GcEveryNBatches > 0 ? TEXT("asset_registry_fassetdata_sliced_deepmesh_gc_retry") : TEXT("asset_registry_fassetdata_sliced_deepmesh_retry"))
		: TEXT("asset_registry_fassetdata_sliced_retry");
	State.Snapshot.Rows.Reserve(State.Controller.GetTotalCount());
	State.BatchThroughputSamplesAssetsPerSec.Reserve(FMath::Max(16, State.Controller.GetTotalCount() / FMath::Max(1, State.Controller.GetBatchSize()) + 1));

	UE_LOG(
		LogHCIAbilityKitAuditScan,
		Display,
		TEXT("[HCIAbilityKit][AuditScanAsync] retry start total=%d batch_size=%d deep_mesh_check=%s gc_every_n_batches=%d"),
		State.Controller.GetTotalCount(),
		State.Controller.GetBatchSize(),
		State.bDeepMeshCheckEnabled ? TEXT("true") : TEXT("false"),
		State.GcEveryNBatches);

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

	if (!GHCIAbilityKitAuditExportJsonCommand.IsValid())
	{
		GHCIAbilityKitAuditExportJsonCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AuditExportJson"),
			TEXT("Run sync audit scan and export JSON report. Usage: HCIAbilityKit.AuditExportJson [output_json_path]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAuditExportJsonCommand));
	}

	if (!GHCIAbilityKitAuditScanAsyncCommand.IsValid())
	{
		GHCIAbilityKitAuditScanAsyncCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AuditScanAsync"),
			TEXT("Slice-based non-blocking scan. Usage: HCIAbilityKit.AuditScanAsync [batch_size] [log_top_n] [deep_mesh_check=0|1] [gc_every_n_batches>=0]"),
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

	if (!GHCIAbilityKitToolRegistryDumpCommand.IsValid())
	{
		GHCIAbilityKitToolRegistryDumpCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.ToolRegistryDump"),
			TEXT("Dump frozen Tool Registry capability declarations. Usage: HCIAbilityKit.ToolRegistryDump [tool_name]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitToolRegistryDumpCommand));
	}

	if (!GHCIAbilityKitDryRunDiffPreviewDemoCommand.IsValid())
	{
		GHCIAbilityKitDryRunDiffPreviewDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.DryRunDiffPreviewDemo"),
			TEXT("Build and print a Dry-Run Diff preview list (E2 contract demo)."),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitDryRunDiffPreviewDemoCommand));
	}

	if (!GHCIAbilityKitDryRunDiffPreviewLocateCommand.IsValid())
	{
		GHCIAbilityKitDryRunDiffPreviewLocateCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.DryRunDiffLocate"),
			TEXT("Locate a Dry-Run Diff preview row. Usage: HCIAbilityKit.DryRunDiffLocate [row_index]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitDryRunDiffPreviewLocateCommand));
	}

	if (!GHCIAbilityKitDryRunDiffPreviewJsonCommand.IsValid())
	{
		GHCIAbilityKitDryRunDiffPreviewJsonCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.DryRunDiffPreviewJson"),
			TEXT("Serialize Dry-Run Diff preview demo to JSON and print it (E2 contract demo)."),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitDryRunDiffPreviewJsonCommand));
	}

	if (!GHCIAbilityKitAgentConfirmGateDemoCommand.IsValid())
	{
		GHCIAbilityKitAgentConfirmGateDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentConfirmGateDemo"),
			TEXT("E3 confirm-gate demo. Usage: HCIAbilityKit.AgentConfirmGateDemo [tool_name] [requires_confirm 0|1] [user_confirmed 0|1]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentConfirmGateDemoCommand));
	}

	if (!GHCIAbilityKitAgentBlastRadiusDemoCommand.IsValid())
	{
		GHCIAbilityKitAgentBlastRadiusDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentBlastRadiusDemo"),
			TEXT("E4 blast-radius demo. Usage: HCIAbilityKit.AgentBlastRadiusDemo [tool_name] [target_modify_count>=0]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentBlastRadiusDemoCommand));
	}

	if (!GHCIAbilityKitAgentTransactionDemoCommand.IsValid())
	{
		GHCIAbilityKitAgentTransactionDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentTransactionDemo"),
			TEXT("E5 all-or-nothing transaction demo. Usage: HCIAbilityKit.AgentTransactionDemo [total_steps>=1] [fail_step_index>=0]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentTransactionDemoCommand));
	}

	if (!GHCIAbilityKitAgentSourceControlDemoCommand.IsValid())
	{
		GHCIAbilityKitAgentSourceControlDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentSourceControlDemo"),
			TEXT("E6 source-control fail-fast/offline demo. Usage: HCIAbilityKit.AgentSourceControlDemo [tool_name] [source_control_enabled 0|1] [checkout_succeeded 0|1]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentSourceControlDemoCommand));
	}

	if (!GHCIAbilityKitAgentRbacDemoCommand.IsValid())
	{
		GHCIAbilityKitAgentRbacDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AgentRbacDemo"),
			TEXT("E7 local mock RBAC + local audit log demo. Usage: HCIAbilityKit.AgentRbacDemo [user_name] [tool_name] [asset_count>=0]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAgentRbacDemoCommand));
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
	GHCIAbilityKitAuditExportJsonCommand.Reset();
	GHCIAbilityKitAuditScanAsyncCommand.Reset();
	GHCIAbilityKitAuditScanProgressCommand.Reset();
	GHCIAbilityKitAuditScanStopCommand.Reset();
	GHCIAbilityKitAuditScanRetryCommand.Reset();
	GHCIAbilityKitToolRegistryDumpCommand.Reset();
	GHCIAbilityKitDryRunDiffPreviewDemoCommand.Reset();
	GHCIAbilityKitDryRunDiffPreviewLocateCommand.Reset();
	GHCIAbilityKitDryRunDiffPreviewJsonCommand.Reset();
	GHCIAbilityKitAgentConfirmGateDemoCommand.Reset();
	GHCIAbilityKitAgentBlastRadiusDemoCommand.Reset();
	GHCIAbilityKitAgentTransactionDemoCommand.Reset();
	GHCIAbilityKitAgentSourceControlDemoCommand.Reset();
	GHCIAbilityKitAgentRbacDemoCommand.Reset();
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
