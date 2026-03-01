#include "HCIEditorModule.h"

#include "HCIRuntimeModule.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Agent/Executor/HCIAgentExecutionGate.h"
#include "Agent/Planner/HCIAgentPlan.h"
#include "Agent/Planner/HCIAgentPlanJsonSerializer.h"
#include "Agent/Planner/HCIAgentPlanner.h"
#include "Agent/Planner/HCIAgentPlanValidator.h"
#include "Agent/Executor/HCIDryRunDiff.h"
#include "Agent/Executor/HCIDryRunDiffJsonSerializer.h"
#include "Agent/Tools/HCIToolRegistry.h"
#include "Audit/HCIAuditPerfMetrics.h"
#include "Audit/HCIAuditScanAsyncController.h"
#include "Audit/HCIAuditReport.h"
#include "Audit/HCIAuditReportJsonSerializer.h"
#include "Audit/HCIAuditRuleRegistry.h"
#include "Audit/HCIAuditTagNames.h"
#include "Commands/HCIAgentDemoConsoleCommands.h"
#include "Dom/JsonObject.h"
#include "Audit/HCIAuditScanService.h"
#include "Common/HCITimeFormat.h"
#include "Containers/Ticker.h"
#include "Factories/HCIFactory.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/StreamableManager.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "EditorUndoClient.h"
#include "Framework/Notifications/NotificationManager.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformProcess.h"
#include "HCIAsset.h"
#include "HCIErrorCodes.h"
#include "IPythonScriptPlugin.h"
#include "Misc/DelayedAutoRegister.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Menu/HCIContentBrowserMenuRegistrar.h"
#include "Search/HCISearchIndexService.h"
#include "Search/HCISearchQueryService.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Services/HCIParserService.h"
#include "ToolMenus.h"
#include "UObject/Package.h"
#include "UObject/GarbageCollection.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Notifications/SNotificationList.h"

DEFINE_LOG_CATEGORY_STATIC(LogHCISearchQuery, Log, All);
DEFINE_LOG_CATEGORY_STATIC(LogHCIAuditScan, Log, All);

static TObjectPtr<UHCIFactory> GHCIFactory;
static TUniquePtr<FAutoConsoleCommand> GHCISearchCommand;
static TUniquePtr<FAutoConsoleCommand> GHCIAuditScanCommand;
static TUniquePtr<FAutoConsoleCommand> GHCIAuditExportJsonCommand;
static TUniquePtr<FAutoConsoleCommand> GHCIAuditScanAsyncCommand;
static TUniquePtr<FAutoConsoleCommand> GHCIAuditScanProgressCommand;
static TUniquePtr<FAutoConsoleCommand> GHCIAuditScanStopCommand;
static TUniquePtr<FAutoConsoleCommand> GHCIAuditScanRetryCommand;
static TUniquePtr<FAutoConsoleCommand> GHCIToolRegistryDumpCommand;
static TUniquePtr<FAutoConsoleCommand> GHCIDryRunDiffPreviewDemoCommand;
static TUniquePtr<FAutoConsoleCommand> GHCIDryRunDiffPreviewLocateCommand;
static TUniquePtr<FAutoConsoleCommand> GHCIDryRunDiffPreviewJsonCommand;
static const TCHAR* GHCIPythonScriptPath = TEXT("SourceData/AbilityKits/Python/hci_abilitykit_hook.py");

namespace
{
static const TCHAR* GHCIBusinessUndoContext = TEXT("HCI");

class FHCIBusinessUndoClient final : public FSelfRegisteringEditorUndoClient
{
public:
	virtual FString GetTransactionContext() const override
	{
		return GHCIBusinessUndoContext;
	}

	virtual bool MatchesContext(
		const FTransactionContext& InContext,
		const TArray<TPair<UObject*, FTransactionObjectEvent>>& TransactionObjectContexts) const override
	{
		if (!InContext.Context.Equals(GHCIBusinessUndoContext, ESearchCase::CaseSensitive))
		{
			return false;
		}

		// Cache title so PostUndo/PostRedo can show a meaningful toast (those callbacks don't carry context).
		LastMatchedTitle = InContext.Title;
		return true;
	}

	virtual void PostUndo(bool bSuccess) override
	{
		ShowToast(bSuccess, /*bRedo=*/false);
	}

	virtual void PostRedo(bool bSuccess) override
	{
		ShowToast(bSuccess, /*bRedo=*/true);
	}

private:
	void ShowToast(const bool bSuccess, const bool bRedo)
	{
		const FString TitleText = LastMatchedTitle.IsEmpty() ? TEXT("HCI") : LastMatchedTitle.ToString();
		const FString Prefix = bRedo ? TEXT("已重做") : TEXT("已撤销");
		const FString Status = bSuccess ? TEXT("") : TEXT("（失败）");
		const FString Message = FString::Printf(TEXT("%s：%s%s"), *Prefix, *TitleText, *Status);

		// Non-blocking toast; don't use modal dialogs in undo/redo callbacks.
		FNotificationInfo Info(FText::FromString(Message));
		Info.bFireAndForget = true;
		Info.ExpireDuration = 3.0f;
		Info.FadeOutDuration = 0.2f;
		Info.bUseSuccessFailIcons = true;
		const TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info);
		if (Item.IsValid())
		{
			Item->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
		}
	}

	mutable FText LastMatchedTitle;
};
} // namespace

static TUniquePtr<FHCIBusinessUndoClient> GHCIBusinessUndoClient;

struct FHCIAuditScanAsyncState
{
	FHCIAuditScanAsyncController Controller;
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
	FHCIAuditScanSnapshot Snapshot;
	FTSTicker::FDelegateHandle TickHandle;
};

static FHCIAuditScanAsyncState GHCIAuditScanAsyncState;
static FHCIDryRunDiffReport GHCIDryRunDiffPreviewState;
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
	const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("HCI"), TEXT("AuditReports"));
	return FPaths::Combine(Directory, FString::Printf(TEXT("%s.json"), *RunId));
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

static bool HCI_TryMarkScanSkippedByState(const FAssetData& AssetData, FHCIAuditAssetRow& OutRow)
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

	TArray<FString> Tokens;
	Tokens.Reserve(Values.Num());
	for (const int32 Value : Values)
	{
		Tokens.Add(FString::FromInt(Value));
	}
	return FString::Join(Tokens, TEXT("|"));
}

static void HCI_RunAbilityKitToolRegistryDumpCommand(const TArray<FString>& Args)
{
	const FString OptionalToolFilter = (Args.Num() >= 1) ? Args[0].TrimStartAndEnd() : FString();

	const FHCIToolRegistry& Registry = FHCIToolRegistry::GetReadOnly();

	FString ValidationError;
	if (!Registry.ValidateFrozenDefaults(ValidationError))
	{
		UE_LOG(LogHCIAuditScan, Error, TEXT("[HCI][ToolRegistry] invalid reason=%s"), *ValidationError);
		return;
	}

	TArray<FHCIToolDescriptor> Tools = Registry.GetAllTools();
	Tools.Sort([](const FHCIToolDescriptor& A, const FHCIToolDescriptor& B)
	{
		return A.ToolName.LexicalLess(B.ToolName);
	});

	int32 PrintedTools = 0;
	for (const FHCIToolDescriptor& Tool : Tools)
	{
		if (!OptionalToolFilter.IsEmpty() && !Tool.ToolName.ToString().Equals(OptionalToolFilter, ESearchCase::IgnoreCase))
		{
			continue;
		}

		++PrintedTools;
		UE_LOG(
			LogHCIAuditScan,
			Display,
			TEXT("[HCI][ToolRegistry] tool=%s capability=%s supports_dry_run=%s supports_undo=%s destructive=%s domain=%s arg_count=%d"),
			*Tool.ToolName.ToString(),
			*FHCIToolRegistry::CapabilityToString(Tool.Capability),
			Tool.bSupportsDryRun ? TEXT("true") : TEXT("false"),
			Tool.bSupportsUndo ? TEXT("true") : TEXT("false"),
			Tool.bDestructive ? TEXT("true") : TEXT("false"),
			Tool.Domain.IsEmpty() ? TEXT("-") : *Tool.Domain,
			Tool.ArgsSchema.Num());

		for (const FHCIToolArgSchema& Arg : Tool.ArgsSchema)
		{
			UE_LOG(
				LogHCIAuditScan,
				Display,
				TEXT("[HCI][ToolRegistry] arg tool=%s name=%s type=%s required=%s array_len=%d..%d str_len=%d..%d int_range=%d..%d str_enum=%s int_enum=%s regex=%s game_path=%s subset_enum=%s"),
				*Tool.ToolName.ToString(),
				*Arg.ArgName.ToString(),
				*FHCIToolRegistry::ArgValueTypeToString(Arg.ValueType),
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
		LogHCIAuditScan,
		Display,
		TEXT("[HCI][ToolRegistry] summary total_registered=%d printed=%d filter=%s validation=ok"),
		Tools.Num(),
		PrintedTools,
		OptionalToolFilter.IsEmpty() ? TEXT("-") : *OptionalToolFilter);

	if (!OptionalToolFilter.IsEmpty() && PrintedTools == 0)
	{
		UE_LOG(
			LogHCIAuditScan,
			Warning,
			TEXT("[HCI][ToolRegistry] filter_not_found tool=%s"),
			*OptionalToolFilter);
	}
}

static void HCI_LogDryRunDiffPreviewRows(const FHCIDryRunDiffReport& Report)
{
	for (int32 Index = 0; Index < Report.DiffItems.Num(); ++Index)
	{
		const FHCIDryRunDiffItem& Item = Report.DiffItems[Index];
		UE_LOG(
			LogHCIAuditScan,
			Display,
			TEXT("[HCI][DryRunDiff] row=%d asset_path=%s field=%s before=%s after=%s tool_name=%s risk=%s skip_reason=%s object_type=%s locate_strategy=%s evidence_key=%s actor_path=%s"),
			Index,
			*Item.AssetPath,
			*Item.Field,
			*Item.Before,
			*Item.After,
			*Item.ToolName,
			*FHCIDryRunDiff::RiskToString(Item.Risk),
			Item.SkipReason.IsEmpty() ? TEXT("-") : *Item.SkipReason,
			*FHCIDryRunDiff::ObjectTypeToString(Item.ObjectType),
			*FHCIDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
			Item.EvidenceKey.IsEmpty() ? TEXT("-") : *Item.EvidenceKey,
			Item.ActorPath.IsEmpty() ? TEXT("-") : *Item.ActorPath);
	}
}

static FString HCI_TryFindFirstAbilityAssetPath()
{
	FHCIAuditScanSnapshot Snapshot = FHCIAuditScanService::Get().ScanFromAssetRegistry();
	for (const FHCIAuditAssetRow& Row : Snapshot.Rows)
	{
		if (!Row.AssetPath.IsEmpty())
		{
			return Row.AssetPath;
		}
	}
	return TEXT("/Game/HCI/Data/TestSkill.TestSkill");
}

static void HCI_BuildDryRunDiffPreviewDemo(FHCIDryRunDiffReport& OutReport)
{
	OutReport = FHCIDryRunDiffReport();
	OutReport.RequestId = FString::Printf(TEXT("req_dryrun_demo_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));

	const FString FirstAbilityAssetPath = HCI_TryFindFirstAbilityAssetPath();

	{
		FHCIDryRunDiffItem& Item = OutReport.DiffItems.Emplace_GetRef();
		Item.AssetPath = FirstAbilityAssetPath;
		Item.Field = TEXT("max_texture_size");
		Item.Before = TEXT("4096");
		Item.After = TEXT("1024");
		Item.ToolName = TEXT("SetTextureMaxSize");
		Item.Risk = EHCIDryRunRisk::Write;
		Item.ObjectType = EHCIDryRunObjectType::Asset;
		Item.EvidenceKey = TEXT("asset_path");
	}

	{
		FHCIDryRunDiffItem& Item = OutReport.DiffItems.Emplace_GetRef();
		Item.AssetPath = FirstAbilityAssetPath;
		Item.Field = TEXT("lod_group");
		Item.Before = TEXT("None");
		Item.After = TEXT("LevelArchitecture");
		Item.ToolName = TEXT("SetMeshLODGroup");
		Item.Risk = EHCIDryRunRisk::Write;
		Item.ObjectType = EHCIDryRunObjectType::Asset;
		Item.EvidenceKey = TEXT("triangle_count_lod0_actual");
	}

	{
		FHCIDryRunDiffItem& Item = OutReport.DiffItems.Emplace_GetRef();
		Item.AssetPath = TEXT("/Game/Maps/OpenWorld.OpenWorld:PersistentLevel.StaticMeshActor_1");
		Item.ActorPath = Item.AssetPath;
		Item.Field = TEXT("missing_collision");
		Item.Before = TEXT("true");
		Item.After = TEXT("would_add_collision");
		Item.ToolName = TEXT("ScanLevelMeshRisks");
		Item.Risk = EHCIDryRunRisk::ReadOnly;
		Item.ObjectType = EHCIDryRunObjectType::Actor;
		Item.EvidenceKey = TEXT("actor_path");
	}

	{
		FHCIDryRunDiffItem& Item = OutReport.DiffItems.Emplace_GetRef();
		Item.AssetPath = FirstAbilityAssetPath;
		Item.Field = TEXT("rename_target");
		Item.Before = TEXT("Temp_Ability_01");
		Item.After = TEXT("SM_Ability_01");
		Item.ToolName = TEXT("RenameAsset");
		Item.Risk = EHCIDryRunRisk::Write;
		Item.ObjectType = EHCIDryRunObjectType::Asset;
		Item.SkipReason = TEXT("dry_run_only_preview");
		Item.EvidenceKey = TEXT("asset_path");
	}

	FHCIDryRunDiff::NormalizeAndFinalize(OutReport);
}

static void HCI_RunAbilityKitDryRunDiffPreviewDemoCommand(const TArray<FString>& Args)
{
	HCI_BuildDryRunDiffPreviewDemo(GHCIDryRunDiffPreviewState);

	const FHCIDryRunDiffReport& Report = GHCIDryRunDiffPreviewState;
	UE_LOG(
		LogHCIAuditScan,
		Display,
		TEXT("[HCI][DryRunDiff] summary request_id=%s total_candidates=%d modifiable=%d skipped=%d"),
		*Report.RequestId,
		Report.Summary.TotalCandidates,
		Report.Summary.Modifiable,
		Report.Summary.Skipped);
	HCI_LogDryRunDiffPreviewRows(Report);
	UE_LOG(
		LogHCIAuditScan,
		Display,
		TEXT("[HCI][DryRunDiff] hint=运行 HCI.DryRunDiffLocate [row_index] 测试定位（actor->camera_focus, asset->sync_browser）"));
}

static void HCI_RunAbilityKitDryRunDiffPreviewJsonCommand(const TArray<FString>& Args)
{
	HCI_BuildDryRunDiffPreviewDemo(GHCIDryRunDiffPreviewState);

	FString JsonText;
	if (!FHCIDryRunDiffJsonSerializer::SerializeToJsonString(GHCIDryRunDiffPreviewState, JsonText))
	{
		UE_LOG(LogHCIAuditScan, Error, TEXT("[HCI][DryRunDiff] export_json_failed reason=serialize_failed"));
		return;
	}

	UE_LOG(LogHCIAuditScan, Display, TEXT("[HCI][DryRunDiff] json=%s"), *JsonText);
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
	if (GHCIDryRunDiffPreviewState.DiffItems.Num() == 0)
	{
		UE_LOG(LogHCIAuditScan, Warning, TEXT("[HCI][DryRunDiff] locate=unavailable reason=no_preview_state"));
		UE_LOG(LogHCIAuditScan, Display, TEXT("[HCI][DryRunDiff] suggestion=先运行 HCI.DryRunDiffPreviewDemo"));
		return;
	}

	int32 RowIndex = 0;
	if (Args.Num() >= 1)
	{
		if (!LexTryParseString(RowIndex, *Args[0]) || RowIndex < 0)
		{
			UE_LOG(LogHCIAuditScan, Error, TEXT("[HCI][DryRunDiff] locate_invalid_args reason=row_index must be integer >= 0"));
			return;
		}
	}

	if (!GHCIDryRunDiffPreviewState.DiffItems.IsValidIndex(RowIndex))
	{
		UE_LOG(
			LogHCIAuditScan,
			Error,
			TEXT("[HCI][DryRunDiff] locate_invalid_args reason=row_index out_of_range row_index=%d total=%d"),
			RowIndex,
			GHCIDryRunDiffPreviewState.DiffItems.Num());
		return;
	}

	const FHCIDryRunDiffItem& Item = GHCIDryRunDiffPreviewState.DiffItems[RowIndex];

	FString LocateReason;
	bool bLocateOk = false;
	if (Item.LocateStrategy == EHCIDryRunLocateStrategy::CameraFocus)
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
			LogHCIAuditScan,
			Display,
			TEXT("[HCI][DryRunDiff] locate row=%d strategy=%s object_type=%s success=%s reason=%s asset_path=%s actor_path=%s"),
			RowIndex,
			*FHCIDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
			*FHCIDryRunDiff::ObjectTypeToString(Item.ObjectType),
			TEXT("true"),
			*LocateReason,
			*Item.AssetPath,
			Item.ActorPath.IsEmpty() ? TEXT("-") : *Item.ActorPath);
	}
	else
	{
		UE_LOG(
			LogHCIAuditScan,
			Warning,
			TEXT("[HCI][DryRunDiff] locate row=%d strategy=%s object_type=%s success=%s reason=%s asset_path=%s actor_path=%s"),
			RowIndex,
			*FHCIDryRunDiff::LocateStrategyToString(Item.LocateStrategy),
			*FHCIDryRunDiff::ObjectTypeToString(Item.ObjectType),
			TEXT("false"),
			*LocateReason,
			*Item.AssetPath,
			Item.ActorPath.IsEmpty() ? TEXT("-") : *Item.ActorPath);
	}
}

static void HCI_TryFillTriangleFromTagCached(
	const FAssetData& AssetData,
	IAssetRegistry* AssetRegistry,
	FHCIAuditAssetRow& OutRow)
{
	if (AssetRegistry == nullptr)
	{
		OutRow.TriangleSource = TEXT("unavailable");
		return;
	}

	FName SourceTagKey;
	if (HCIAuditTagNames::TryResolveTriangleCountFromTags(
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
	if (HCIAuditTagNames::TryResolveMeshLodCountFromTags(
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
	if (HCIAuditTagNames::TryResolveMeshNaniteEnabledFromTags(
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

	if (HCIAuditTagNames::TryResolveTriangleCountFromTags(
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

struct FHCIAuditDeepMeshBatchStats
{
	int32 LoadAttemptCount = 0;
	int32 LoadSuccessCount = 0;
	int32 HandleReleaseCount = 0;
	int32 TriangleResolvedCount = 0;
	int32 MeshSignalsResolvedCount = 0;
};

static bool HCI_ShouldRunDeepMeshCheckForRow(const FHCIAuditAssetRow& Row)
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

static bool HCI_TryResolveAuditSignalsFromLoadedStaticMesh(UStaticMesh* StaticMesh, FHCIAuditAssetRow& OutRow)
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
	FHCIAuditAssetRow& InOutRow,
	FHCIAuditDeepMeshBatchStats& InOutBatchStats)
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
	FHCIAuditAssetRow& OutRow)
{
	OutRow.AssetPath = AssetData.GetObjectPathString();
	OutRow.AssetName = AssetData.AssetName.ToString();
	OutRow.AssetClass = AssetData.AssetClassPath.GetAssetName().ToString();

	AssetData.GetTagValue(HCIAuditTagNames::Id, OutRow.Id);
	AssetData.GetTagValue(HCIAuditTagNames::DisplayName, OutRow.DisplayName);
	AssetData.GetTagValue(HCIAuditTagNames::RepresentingMesh, OutRow.RepresentingMeshPath);

	FName TextureDimensionsTagKey;
	if (HCIAuditTagNames::TryResolveTextureDimensionsFromTags(
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
	HCIAuditTagNames::TryResolveTriangleExpectedFromTags(
		[&AssetData](const FName& TagName, FString& OutValue)
		{
			return AssetData.GetTagValue(TagName, OutValue);
		},
		OutRow.TriangleCountLod0ExpectedJson,
		TriangleExpectedTagKey);
	(void)TriangleExpectedTagKey;

	FString DamageText;
	if (AssetData.GetTagValue(HCIAuditTagNames::Damage, DamageText))
	{
		LexTryParseString(OutRow.Damage, *DamageText);
	}

	if (HCI_TryMarkScanSkippedByState(AssetData, OutRow))
	{
		return;
	}

	HCI_TryFillTriangleFromTagCached(AssetData, AssetRegistry, OutRow);
}

static void HCI_AccumulateAuditCoverage(const FHCIAuditAssetRow& Row, FHCIAuditScanStats& InOutStats)
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
		for (const FHCIAuditIssue& Issue : Row.AuditIssues)
		{
			switch (Issue.Severity)
			{
			case EHCIAuditSeverity::Info:
				++InOutStats.InfoIssueCount;
				break;
			case EHCIAuditSeverity::Warn:
				++InOutStats.WarnIssueCount;
				break;
			case EHCIAuditSeverity::Error:
				++InOutStats.ErrorIssueCount;
				break;
			default:
				break;
			}
		}
	}
}

static void HCI_LogAuditRows(const TCHAR* Prefix, const TArray<FHCIAuditAssetRow>& Rows, const int32 LogTopN)
{
	const int32 CountToLog = FMath::Min(LogTopN, Rows.Num());
	for (int32 Index = 0; Index < CountToLog; ++Index)
	{
		const FHCIAuditAssetRow& Row = Rows[Index];
		const FHCIAuditIssue* FirstIssue = Row.AuditIssues.Num() > 0 ? &Row.AuditIssues[0] : nullptr;
		UE_LOG(
			LogHCIAuditScan,
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
			FirstIssue ? *FHCIAuditReportBuilder::SeverityToString(FirstIssue->Severity) : TEXT(""));
	}
}

static void HCI_StopAuditScanAsyncTicker()
{
	FHCIAuditScanAsyncState& State = GHCIAuditScanAsyncState;
	if (State.TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(State.TickHandle);
		State.TickHandle.Reset();
	}
}

static void HCI_ResetAuditScanAsyncState(const bool bClearRetryContext)
{
	HCI_StopAuditScanAsyncTicker();
	GHCIAuditScanAsyncState.Controller.Reset(bClearRetryContext);
	GHCIAuditScanAsyncState.LastLogPercentBucket = -1;
	GHCIAuditScanAsyncState.StartTimeSeconds = 0.0;
	GHCIAuditScanAsyncState.bDeepMeshCheckEnabled = false;
	GHCIAuditScanAsyncState.GcEveryNBatches = 0;
	GHCIAuditScanAsyncState.ProcessedBatchCount = 0;
	GHCIAuditScanAsyncState.GcRunCount = 0;
	GHCIAuditScanAsyncState.DeepMeshLoadAttemptCount = 0;
	GHCIAuditScanAsyncState.DeepMeshLoadSuccessCount = 0;
	GHCIAuditScanAsyncState.DeepMeshHandleReleaseCount = 0;
	GHCIAuditScanAsyncState.DeepTriangleResolvedCount = 0;
	GHCIAuditScanAsyncState.DeepMeshSignalsResolvedCount = 0;
	GHCIAuditScanAsyncState.LastBatchDurationMs = 0.0;
	GHCIAuditScanAsyncState.MaxBatchDurationMs = 0.0;
	GHCIAuditScanAsyncState.MaxBatchThroughputAssetsPerSec = 0.0;
	GHCIAuditScanAsyncState.LastGcDurationMs = 0.0;
	GHCIAuditScanAsyncState.MaxGcDurationMs = 0.0;
	GHCIAuditScanAsyncState.LastPerfLogTimeSeconds = 0.0;
	GHCIAuditScanAsyncState.LastPerfLogProcessedCount = 0;
	GHCIAuditScanAsyncState.PeakUsedPhysicalBytes = 0;
	GHCIAuditScanAsyncState.BatchThroughputSamplesAssetsPerSec.Reset();
	GHCIAuditScanAsyncState.Snapshot = FHCIAuditScanSnapshot();
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
	FHCIAuditScanAsyncState& State = GHCIAuditScanAsyncState;
	State.Controller.Fail(Reason);
	State.Snapshot.Stats.AssetCount = State.Snapshot.Rows.Num();
	State.Snapshot.Stats.UpdatedUtc = FDateTime::UtcNow();
	State.Snapshot.Stats.DurationMs = (FPlatformTime::Seconds() - State.StartTimeSeconds) * 1000.0;

	UE_LOG(
		LogHCIAuditScan,
		Error,
		TEXT("[HCI][AuditScanAsync] failed reason=%s processed=%d/%d"),
		*State.Controller.GetLastFailureReason(),
		State.Controller.GetNextIndex(),
		State.Controller.GetTotalCount());
	UE_LOG(
		LogHCIAuditScan,
		Warning,
		TEXT("[HCI][AuditScanAsync] suggestion=运行 HCI.AuditScanAsyncRetry 可按上次参数重试"));
}

static void HCI_LogAuditScanAsyncPerfProgress(const int32 ProgressPercent)
{
	FHCIAuditScanAsyncState& State = GHCIAuditScanAsyncState;
	const int32 TotalCount = State.Controller.GetTotalCount();
	const int32 ProcessedCount = State.Controller.GetNextIndex();
	const double NowSeconds = FPlatformTime::Seconds();
	const double ElapsedMs = (NowSeconds - State.StartTimeSeconds) * 1000.0;
	const double AvgAssetsPerSec = FHCIAuditPerfMetrics::AssetsPerSecond(ProcessedCount, ElapsedMs);

	double WindowAssetsPerSec = AvgAssetsPerSec;
	if (State.LastPerfLogTimeSeconds > 0.0)
	{
		const double WindowMs = (NowSeconds - State.LastPerfLogTimeSeconds) * 1000.0;
		const int32 WindowProcessed = FMath::Max(0, ProcessedCount - State.LastPerfLogProcessedCount);
		WindowAssetsPerSec = FHCIAuditPerfMetrics::AssetsPerSecond(WindowProcessed, WindowMs);
	}

	const int32 RemainingCount = FMath::Max(0, TotalCount - ProcessedCount);
	const double EtaSeconds = AvgAssetsPerSec > UE_SMALL_NUMBER
		? (static_cast<double>(RemainingCount) / AvgAssetsPerSec)
		: -1.0;

	const FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();
	const double UsedPhysicalMiB = static_cast<double>(MemoryStats.UsedPhysical) / (1024.0 * 1024.0);
	const double PeakUsedPhysicalMiB = static_cast<double>(State.PeakUsedPhysicalBytes) / (1024.0 * 1024.0);

	UE_LOG(
		LogHCIAuditScan,
		Display,
		TEXT("[HCI][AuditScanAsync][Perf] progress=%d%% processed=%d/%d elapsed_ms=%.2f avg_assets_per_sec=%.2f window_assets_per_sec=%.2f eta_s=%.2f used_physical_mib=%.1f peak_used_physical_mib=%.1f gc_runs=%d"),
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
	FHCIAuditScanAsyncState& State = GHCIAuditScanAsyncState;
	const int32 AssetCount = State.Snapshot.Rows.Num();
	const double DurationMs = State.Snapshot.Stats.DurationMs;
	const double AvgAssetsPerSec = FHCIAuditPerfMetrics::AssetsPerSecond(AssetCount, DurationMs);
	const double P50BatchAssetsPerSec = FHCIAuditPerfMetrics::PercentileNearestRank(State.BatchThroughputSamplesAssetsPerSec, 50.0);
	const double P95BatchAssetsPerSec = FHCIAuditPerfMetrics::PercentileNearestRank(State.BatchThroughputSamplesAssetsPerSec, 95.0);
	const double PeakUsedPhysicalMiB = static_cast<double>(State.PeakUsedPhysicalBytes) / (1024.0 * 1024.0);

	UE_LOG(
		LogHCIAuditScan,
		Display,
		TEXT("[HCI][AuditScanAsync][PerfSummary] assets=%d batches=%d duration_ms=%.2f avg_assets_per_sec=%.2f p50_batch_assets_per_sec=%.2f p95_batch_assets_per_sec=%.2f max_batch_assets_per_sec=%.2f peak_used_physical_mib=%.1f"),
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
	FHCIAuditScanAsyncState& State = GHCIAuditScanAsyncState;
	State.Controller.Complete();
	State.Snapshot.Stats.AssetCount = State.Snapshot.Rows.Num();
	State.Snapshot.Stats.UpdatedUtc = FDateTime::UtcNow();
	State.Snapshot.Stats.DurationMs = (FPlatformTime::Seconds() - State.StartTimeSeconds) * 1000.0;
	UE_LOG(
		LogHCIAuditScan,
		Display,
		TEXT("[HCI][AuditScanAsync] %s"),
		*State.Snapshot.Stats.ToSummaryString());
	HCI_LogAuditScanAsyncPerfFinalSummary();
	if (State.bDeepMeshCheckEnabled)
	{
		const double PeakUsedPhysicalMiB = static_cast<double>(State.PeakUsedPhysicalBytes) / (1024.0 * 1024.0);
		UE_LOG(
			LogHCIAuditScan,
			Display,
			TEXT("[HCI][AuditScanAsync][Deep] load_attempts=%d load_success=%d handle_releases=%d triangle_resolved=%d mesh_signals_resolved=%d batches=%d gc_every_n_batches=%d gc_runs=%d max_batch_ms=%.2f max_gc_ms=%.2f peak_used_physical_mib=%.1f"),
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
	HCI_LogAuditRows(TEXT("[HCI][AuditScanAsync]"), State.Snapshot.Rows, State.Controller.GetLogTopN());

	HCI_ResetAuditScanAsyncState(true);
}

static bool HCI_TickAbilityKitAuditScanAsync(float DeltaSeconds)
{
	FHCIAuditScanAsyncState& State = GHCIAuditScanAsyncState;
	if (!State.Controller.IsRunning())
	{
		return false;
	}

	const FHCIAuditScanBatch Batch = State.Controller.DequeueBatch();
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
	FHCIAuditDeepMeshBatchStats DeepBatchStats;
	for (int32 Index = Batch.StartIndex; Index < Batch.EndIndex; ++Index)
	{
		if (!AssetDatas.IsValidIndex(Index))
		{
			HCI_ConvergeAuditScanAsyncFailure(TEXT("asset index out of range during scan tick"));
			return false;
		}

		FHCIAuditAssetRow& Row = State.Snapshot.Rows.Emplace_GetRef();
		HCI_ParseAuditRowFromAssetData(AssetDatas[Index], AssetRegistry, Row);
		if (State.bDeepMeshCheckEnabled)
		{
			HCI_TryEnrichAuditRowByDeepMeshLoad(Row, DeepBatchStats);
		}
		const FHCIAuditContext Context{Row};
		FHCIAuditRuleRegistry::Get().Evaluate(Context, Row.AuditIssues);
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
	const double BatchAssetsPerSec = FHCIAuditPerfMetrics::AssetsPerSecond(BatchAssetCount, State.LastBatchDurationMs);
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
			LogHCIAuditScan,
			Display,
			TEXT("[HCI][AuditScanAsync] progress=%d%% processed=%d/%d"),
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
			LogHCISearchQuery,
			Warning,
			TEXT("[HCI][SearchQuery] usage: HCI.Search <query text> [topk]"));
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
	const FHCIAbilitySearchResult QueryResult = FHCISearchQueryService::RunQuery(
		QueryText,
		TopK,
		FHCISearchIndexService::Get().GetIndex());

	UE_LOG(
		LogHCISearchQuery,
		Display,
		TEXT("[HCI][SearchQuery] raw=\"%s\" parsed=%s"),
		*QueryText,
		*QueryResult.ParsedQuery.ToSummaryString());

	UE_LOG(
		LogHCISearchQuery,
		Display,
		TEXT("[HCI][SearchQuery] %s"),
		*QueryResult.ToSummaryString());

	if (QueryResult.Candidates.Num() > 0)
	{
		for (const FHCIAbilitySearchCandidate& Candidate : QueryResult.Candidates)
		{
			UE_LOG(
				LogHCISearchQuery,
				Display,
				TEXT("[HCI][SearchQuery] hit id=%s score=%.2f"),
				*Candidate.Id,
				Candidate.Score);
		}
		return;
	}

	for (const FString& Suggestion : QueryResult.Suggestions)
	{
		UE_LOG(
			LogHCISearchQuery,
			Warning,
			TEXT("[HCI][SearchQuery] suggestion=%s"),
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

	const FHCIAuditScanSnapshot Snapshot = FHCIAuditScanService::Get().ScanFromAssetRegistry();
	UE_LOG(
		LogHCIAuditScan,
		Display,
		TEXT("[HCI][AuditScan] %s"),
		*Snapshot.Stats.ToSummaryString());

	if (Snapshot.Rows.Num() == 0)
	{
		UE_LOG(
			LogHCIAuditScan,
			Warning,
			TEXT("[HCI][AuditScan] suggestion=当前未发现 AbilityKit 资产，请先导入至少一个 .hciabilitykit 文件"));
		return;
	}

	HCI_LogAuditRows(TEXT("[HCI][AuditScan]"), Snapshot.Rows, LogTopN);
}

static void HCI_RunAbilityKitAuditExportJsonCommand(const TArray<FString>& Args)
{
	const FHCIAuditScanSnapshot Snapshot = FHCIAuditScanService::Get().ScanFromAssetRegistry();
	const FHCIAuditReport Report = FHCIAuditReportBuilder::BuildFromSnapshot(Snapshot);

	FString OutputPath;
	FString ResolveError;
	if (!HCI_TryResolveAuditExportOutputPath(Args, Report.RunId, OutputPath, ResolveError))
	{
		UE_LOG(
			LogHCIAuditScan,
			Error,
			TEXT("[HCI][AuditExportJson] failed reason=%s"),
			*ResolveError);
		return;
	}

	FString JsonText;
	if (!FHCIAuditReportJsonSerializer::SerializeToJsonString(Report, JsonText))
	{
		UE_LOG(
			LogHCIAuditScan,
			Error,
			TEXT("[HCI][AuditExportJson] failed reason=serialize_to_json_failed"));
		return;
	}

	if (!FFileHelper::SaveStringToFile(JsonText, *OutputPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(
			LogHCIAuditScan,
			Error,
			TEXT("[HCI][AuditExportJson] failed reason=save_to_file_failed path=%s"),
			*OutputPath);
		return;
	}

	UE_LOG(
		LogHCIAuditScan,
		Display,
		TEXT("[HCI][AuditExportJson] success path=%s run_id=%s results=%d issue_assets=%d source=%s"),
		*OutputPath,
		*Report.RunId,
		Report.Results.Num(),
		Snapshot.Stats.AssetsWithIssuesCount,
		*Report.Source);

	if (Snapshot.Rows.Num() == 0)
	{
		UE_LOG(
			LogHCIAuditScan,
			Warning,
			TEXT("[HCI][AuditExportJson] suggestion=当前未发现 AbilityKit 资产，请先导入至少一个 .hciabilitykit 文件"));
	}
}

static void HCI_RunAbilityKitAuditScanAsyncCommand(const TArray<FString>& Args)
{
	FHCIAuditScanAsyncState& State = GHCIAuditScanAsyncState;
	if (State.Controller.IsRunning())
	{
		UE_LOG(LogHCIAuditScan, Warning, TEXT("[HCI][AuditScanAsync] scan is already running"));
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
			LogHCIAuditScan,
			Error,
			TEXT("[HCI][AuditScanAsync] invalid_args reason=%s usage=HCI.AuditScanAsync [batch_size>=1] [log_top_n>=0] [deep_mesh_check=0|1] [gc_every_n_batches>=0]"),
			*ParseError);
		return;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	FARFilter Filter;
	Filter.ClassPaths.Add(UHCIAsset::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;

	TArray<FAssetData> AssetDatas;
	AssetRegistryModule.Get().GetAssets(Filter, AssetDatas);

	FString StartError;
	if (!State.Controller.Start(MoveTemp(AssetDatas), BatchSize, LogTopN, bDeepMeshCheckEnabled, GcEveryNBatches, StartError))
	{
		UE_LOG(
			LogHCIAuditScan,
			Error,
			TEXT("[HCI][AuditScanAsync] start_failed reason=%s"),
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
	State.Snapshot = FHCIAuditScanSnapshot();
	State.Snapshot.Stats.Source = bDeepMeshCheckEnabled
		? (GcEveryNBatches > 0 ? TEXT("asset_registry_fassetdata_sliced_deepmesh_gc") : TEXT("asset_registry_fassetdata_sliced_deepmesh"))
		: TEXT("asset_registry_fassetdata_sliced");
	State.Snapshot.Rows.Reserve(State.Controller.GetTotalCount());
	State.BatchThroughputSamplesAssetsPerSec.Reserve(FMath::Max(16, State.Controller.GetTotalCount() / FMath::Max(1, State.Controller.GetBatchSize()) + 1));

	UE_LOG(
		LogHCIAuditScan,
		Display,
		TEXT("[HCI][AuditScanAsync] start total=%d batch_size=%d deep_mesh_check=%s gc_every_n_batches=%d"),
		State.Controller.GetTotalCount(),
		State.Controller.GetBatchSize(),
		State.bDeepMeshCheckEnabled ? TEXT("true") : TEXT("false"),
		State.GcEveryNBatches);

	if (State.Controller.GetTotalCount() == 0)
	{
		UE_LOG(
			LogHCIAuditScan,
			Warning,
			TEXT("[HCI][AuditScanAsync] suggestion=当前未发现 AbilityKit 资产，请先导入至少一个 .hciabilitykit 文件"));
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
	FHCIAuditScanAsyncState& State = GHCIAuditScanAsyncState;
	if (!State.Controller.IsRunning())
	{
		UE_LOG(LogHCIAuditScan, Display, TEXT("[HCI][AuditScanAsync] stop=ignored reason=not_running"));
		return;
	}

	FString CancelError;
	if (!State.Controller.Cancel(CancelError))
	{
		UE_LOG(
			LogHCIAuditScan,
			Warning,
			TEXT("[HCI][AuditScanAsync] stop=failed reason=%s"),
			*CancelError);
		return;
	}

	HCI_StopAuditScanAsyncTicker();
	State.Snapshot.Stats.AssetCount = State.Snapshot.Rows.Num();
	State.Snapshot.Stats.UpdatedUtc = FDateTime::UtcNow();
	State.Snapshot.Stats.DurationMs = (FPlatformTime::Seconds() - State.StartTimeSeconds) * 1000.0;
	UE_LOG(
		LogHCIAuditScan,
		Display,
		TEXT("[HCI][AuditScanAsync] interrupted processed=%d/%d can_retry=true"),
		State.Controller.GetNextIndex(),
		State.Controller.GetTotalCount());
}

static void HCI_RunAbilityKitAuditScanRetryCommand(const TArray<FString>& Args)
{
	FHCIAuditScanAsyncState& State = GHCIAuditScanAsyncState;
	if (State.Controller.IsRunning())
	{
		UE_LOG(LogHCIAuditScan, Warning, TEXT("[HCI][AuditScanAsync] retry=blocked reason=scan_is_running"));
		return;
	}

	FString RetryError;
	if (!State.Controller.Retry(RetryError))
	{
		UE_LOG(
			LogHCIAuditScan,
			Warning,
			TEXT("[HCI][AuditScanAsync] retry=unavailable reason=%s"),
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
	State.Snapshot = FHCIAuditScanSnapshot();
	State.Snapshot.Stats.Source = State.bDeepMeshCheckEnabled
		? (State.GcEveryNBatches > 0 ? TEXT("asset_registry_fassetdata_sliced_deepmesh_gc_retry") : TEXT("asset_registry_fassetdata_sliced_deepmesh_retry"))
		: TEXT("asset_registry_fassetdata_sliced_retry");
	State.Snapshot.Rows.Reserve(State.Controller.GetTotalCount());
	State.BatchThroughputSamplesAssetsPerSec.Reserve(FMath::Max(16, State.Controller.GetTotalCount() / FMath::Max(1, State.Controller.GetBatchSize()) + 1));

	UE_LOG(
		LogHCIAuditScan,
		Display,
		TEXT("[HCI][AuditScanAsync] retry start total=%d batch_size=%d deep_mesh_check=%s gc_every_n_batches=%d"),
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
	const FHCIAuditScanAsyncState& State = GHCIAuditScanAsyncState;
	if (State.Controller.IsRunning())
	{
		UE_LOG(
			LogHCIAuditScan,
			Display,
			TEXT("[HCI][AuditScanAsync] progress=%d%% processed=%d/%d"),
			State.Controller.GetProgressPercent(),
			State.Controller.GetNextIndex(),
			State.Controller.GetTotalCount());
		return;
	}

	switch (State.Controller.GetPhase())
	{
	case EHCIAuditScanAsyncPhase::Cancelled:
		UE_LOG(
			LogHCIAuditScan,
			Display,
			TEXT("[HCI][AuditScanAsync] progress=cancelled processed=%d/%d can_retry=%s"),
			State.Controller.GetNextIndex(),
			State.Controller.GetTotalCount(),
			State.Controller.CanRetry() ? TEXT("true") : TEXT("false"));
		return;
	case EHCIAuditScanAsyncPhase::Failed:
		UE_LOG(
			LogHCIAuditScan,
			Error,
			TEXT("[HCI][AuditScanAsync] progress=failed processed=%d/%d reason=%s can_retry=%s"),
			State.Controller.GetNextIndex(),
			State.Controller.GetTotalCount(),
			*State.Controller.GetLastFailureReason(),
			State.Controller.CanRetry() ? TEXT("true") : TEXT("false"));
		return;
	default:
		UE_LOG(LogHCIAuditScan, Display, TEXT("[HCI][AuditScanAsync] progress=idle"));
		return;
	}
}

static bool HCI_ParsePythonResponse(
	const FString& ResponseFilename,
	const FString& ScriptPath,
	const FString& SourceFilename,
	const FString& PythonCommandResult,
	FHCIParsedData& InOutParsed,
	FHCIParseError& OutError)
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
		OutError.Code = HCIErrorCodes::PythonError;
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
		OutError.Code = HCIErrorCodes::PythonError;
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
		OutError.Code = HCIErrorCodes::PythonError;
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
			OutError.Code = HCI_GetJsonStringOrDefault(*ErrorObject, TEXT("code"), HCIErrorCodes::PythonError);
			OutError.File = SourceFilename;
			OutError.Field = HCI_GetJsonStringOrDefault(*ErrorObject, TEXT("field"), TEXT("python_hook"));
			OutError.Reason = HCI_GetJsonStringOrDefault(*ErrorObject, TEXT("reason"), TEXT("Python hook returned failure"));
			OutError.Hint = HCI_GetJsonStringOrDefault(*ErrorObject, TEXT("hint"), TEXT("Inspect Python response error payload"));
			OutError.Detail = HCI_GetJsonStringOrDefault(*ErrorObject, TEXT("detail"), FString());
		}
		else
		{
			OutError.Code = HCIErrorCodes::PythonError;
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

static bool HCI_RunPythonHook(const FString& SourceFilename, FHCIParsedData& InOutParsed, FHCIParseError& OutError)
{
	const FString ScriptPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), GHCIPythonScriptPath);
	if (!FPaths::FileExists(ScriptPath))
	{
		OutError.Code = HCIErrorCodes::PythonScriptMissing;
		OutError.File = ScriptPath;
		OutError.Field = TEXT("python_hook");
		OutError.Reason = TEXT("Python hook script not found");
		OutError.Hint = TEXT("Restore script file in project directory");
		return false;
	}

	IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
	if (!PythonPlugin || !PythonPlugin->IsPythonAvailable())
	{
		OutError.Code = HCIErrorCodes::PythonPluginDisabled;
		OutError.File = ScriptPath;
		OutError.Field = TEXT("python_plugin");
		OutError.Reason = TEXT("PythonScriptPlugin is unavailable");
		OutError.Hint = TEXT("Enable PythonScriptPlugin in project settings");
		return false;
	}

	const FString AbsoluteSourceFilename = FPaths::ConvertRelativePathToFull(SourceFilename);
	const FString ResponseDir = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("HCI"), TEXT("Python")));
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
		OutError.Code = HCIErrorCodes::PythonError;
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

void FHCIEditorModule::StartupModule()
{
	// Planner Router DI: Editor-side decides and registers the real router instance.
	{
		FHCIRuntimeModule& RuntimeModule = FHCIRuntimeModule::Get();
		RuntimeModule.RegisterPlannerRouter(RuntimeModule.CreateDefaultPlannerRouter());
	}

	FHCISearchIndexService::Get().RebuildFromAssetRegistry();

	FHCIParserService::SetPythonHook(
		[](const FString& SourceFilename, FHCIParsedData& InOutParsed, FHCIParseError& OutError)
		{
			return HCI_RunPythonHook(SourceFilename, InOutParsed, OutError);
		});

	GHCIFactory = NewObject<UHCIFactory>(GetTransientPackage());
	GHCIFactory->AddToRoot();

	if (!ContentBrowserMenuRegistrar.IsValid())
	{
		ContentBrowserMenuRegistrar = MakeUnique<FHCIContentBrowserMenuRegistrar>();
	}
	ContentBrowserMenuRegistrar->Startup(GHCIFactory);

	if (!AgentDemoConsoleCommands.IsValid())
	{
		AgentDemoConsoleCommands = MakeUnique<FHCIAgentDemoConsoleCommands>();
	}
	AgentDemoConsoleCommands->Startup();

	if (!GHCISearchCommand.IsValid())
	{
		GHCISearchCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCI.Search"),
			TEXT("Search AbilityKit assets by natural language. Usage: HCI.Search <query text> [topk]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitSearchCommand));
	}

	if (!GHCIAuditScanCommand.IsValid())
	{
		GHCIAuditScanCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCI.AuditScan"),
			TEXT("Enumerate AbilityKit metadata via AssetRegistry only. Usage: HCI.AuditScan [log_top_n]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAuditScanCommand));
	}

	if (!GHCIAuditExportJsonCommand.IsValid())
	{
		GHCIAuditExportJsonCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCI.AuditExportJson"),
			TEXT("Run sync audit scan and export JSON report. Usage: HCI.AuditExportJson [output_json_path]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAuditExportJsonCommand));
	}

	if (!GHCIAuditScanAsyncCommand.IsValid())
	{
		GHCIAuditScanAsyncCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCI.AuditScanAsync"),
			TEXT("Slice-based non-blocking scan. Usage: HCI.AuditScanAsync [batch_size] [log_top_n] [deep_mesh_check=0|1] [gc_every_n_batches>=0]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAuditScanAsyncCommand));
	}

	if (!GHCIAuditScanProgressCommand.IsValid())
	{
		GHCIAuditScanProgressCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCI.AuditScanProgress"),
			TEXT("Print current progress of HCI.AuditScanAsync."),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAuditScanProgressCommand));
	}

	if (!GHCIAuditScanStopCommand.IsValid())
	{
		GHCIAuditScanStopCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCI.AuditScanAsyncStop"),
			TEXT("Interrupt currently running HCI.AuditScanAsync and keep retry context."),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAuditScanStopCommand));
	}

	if (!GHCIAuditScanRetryCommand.IsValid())
	{
		GHCIAuditScanRetryCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCI.AuditScanAsyncRetry"),
			TEXT("Retry last interrupted/failed HCI.AuditScanAsync with previous args."),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAuditScanRetryCommand));
	}

	if (!GHCIToolRegistryDumpCommand.IsValid())
	{
		GHCIToolRegistryDumpCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCI.ToolRegistryDump"),
			TEXT("Dump frozen Tool Registry capability declarations. Usage: HCI.ToolRegistryDump [tool_name]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitToolRegistryDumpCommand));
	}

	if (!GHCIDryRunDiffPreviewDemoCommand.IsValid())
	{
		GHCIDryRunDiffPreviewDemoCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCI.DryRunDiffPreviewDemo"),
			TEXT("Build and print a Dry-Run Diff preview list (E2 contract demo)."),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitDryRunDiffPreviewDemoCommand));
	}

	if (!GHCIDryRunDiffPreviewLocateCommand.IsValid())
	{
		GHCIDryRunDiffPreviewLocateCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCI.DryRunDiffLocate"),
			TEXT("Locate a Dry-Run Diff preview row. Usage: HCI.DryRunDiffLocate [row_index]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitDryRunDiffPreviewLocateCommand));
	}

	if (!GHCIDryRunDiffPreviewJsonCommand.IsValid())
	{
		GHCIDryRunDiffPreviewJsonCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCI.DryRunDiffPreviewJson"),
			TEXT("Serialize Dry-Run Diff preview demo to JSON and print it (E2 contract demo)."),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitDryRunDiffPreviewJsonCommand));
	}

	// Business-level Undo listener (toast on Ctrl+Z / Ctrl+Y) for transactions created with Context="HCI".
	// This must be non-blocking and should not depend on any user UI being open.
	if (!GHCIBusinessUndoClient.IsValid() && GEditor != nullptr)
	{
		GHCIBusinessUndoClient = MakeUnique<FHCIBusinessUndoClient>();
	}
}

void FHCIEditorModule::ShutdownModule()
{
	FHCIParserService::ClearPythonHook();

	if (ContentBrowserMenuRegistrar.IsValid())
	{
		ContentBrowserMenuRegistrar->Shutdown();
		ContentBrowserMenuRegistrar.Reset();
	}

	if (AgentDemoConsoleCommands.IsValid())
	{
		AgentDemoConsoleCommands->Shutdown();
		AgentDemoConsoleCommands.Reset();
	}

	if (GHCIFactory)
	{
		GHCIFactory->RemoveFromRoot();
		GHCIFactory = nullptr;
	}

	if (GHCIAuditScanAsyncState.Controller.IsRunning())
	{
		HCI_ResetAuditScanAsyncState(true);
	}
	else
	{
		HCI_StopAuditScanAsyncTicker();
	}

	GHCISearchCommand.Reset();
	GHCIAuditScanCommand.Reset();
	GHCIAuditExportJsonCommand.Reset();
	GHCIAuditScanAsyncCommand.Reset();
	GHCIAuditScanProgressCommand.Reset();
	GHCIAuditScanStopCommand.Reset();
	GHCIAuditScanRetryCommand.Reset();
	GHCIToolRegistryDumpCommand.Reset();
	GHCIDryRunDiffPreviewDemoCommand.Reset();
	GHCIDryRunDiffPreviewLocateCommand.Reset();
	GHCIDryRunDiffPreviewJsonCommand.Reset();
	GHCIBusinessUndoClient.Reset();
}

IMPLEMENT_MODULE(FHCIEditorModule, HCIEditor)

