#include "Commands/HCIAbilityKitAgentDemoConsoleCommands.h"

#include "Commands/HCIAbilityKitAgentCommandHandlers.h"
#include "Ingest/HCIAbilityKitIngestManifest.h"

#include "AssetToolsModule.h"
#include "Editor.h"
#include "HAL/FileManager.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "AssetImportTask.h"

DEFINE_LOG_CATEGORY_STATIC(LogHCIAbilityKitIngest, Log, All);

namespace
{
static bool HCI_TryLoadLatestManifest(FHCIAbilityKitIngestManifest& OutManifest, FString& OutError)
{
	OutManifest = FHCIAbilityKitIngestManifest();
	OutError.Reset();

	const FString IngestRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("HCIAbilityKit/Ingest"));
	FString ManifestPath;
	if (!FHCIAbilityKitIngestManifest::TryFindLatestManifestUnderRoot(IngestRoot, ManifestPath, OutError))
	{
		return false;
	}
	return FHCIAbilityKitIngestManifest::TryLoadFromFile(ManifestPath, OutManifest, OutError);
}

static bool HCI_IsValidGameContentPath(const FString& GamePath)
{
	const FString P = GamePath.TrimStartAndEnd();
	return P.StartsWith(TEXT("/Game/")) && !P.Contains(TEXT("..")) && !P.Contains(TEXT("\\"));
}
}

void HCI_RunAbilityKitIngestDumpLatestCommand(const TArray<FString>& Args)
{
	FHCIAbilityKitIngestManifest Manifest;
	FString Error;
	if (!HCI_TryLoadLatestManifest(Manifest, Error))
	{
		UE_LOG(
			LogHCIAbilityKitIngest,
			Warning,
			TEXT("[HCIAbilityKit][Ingest] dump_latest failed reason=%s"),
			Error.IsEmpty() ? TEXT("-") : *Error);
		return;
	}

	UE_LOG(
		LogHCIAbilityKitIngest,
		Display,
		TEXT("[HCIAbilityKit][Ingest] latest batch_id=%s source_app=%s preflight_ok=%s files=%d target_root=%s manifest=%s"),
		Manifest.BatchId.IsEmpty() ? TEXT("-") : *Manifest.BatchId,
		Manifest.SourceApp.IsEmpty() ? TEXT("unknown") : *Manifest.SourceApp,
		Manifest.bPreflightOk ? TEXT("true") : TEXT("false"),
		Manifest.Files.Num(),
		Manifest.SuggestedUnrealTargetRoot.IsEmpty() ? TEXT("-") : *Manifest.SuggestedUnrealTargetRoot,
		Manifest.ManifestPath.IsEmpty() ? TEXT("-") : *Manifest.ManifestPath);

	if (Manifest.PreflightWarnings.Num() > 0)
	{
		UE_LOG(LogHCIAbilityKitIngest, Display, TEXT("[HCIAbilityKit][Ingest] warnings=%d first=%s"),
			Manifest.PreflightWarnings.Num(),
			*Manifest.PreflightWarnings[0]);
	}
	if (Manifest.PreflightErrors.Num() > 0)
	{
		UE_LOG(LogHCIAbilityKitIngest, Display, TEXT("[HCIAbilityKit][Ingest] errors=%d first=%s"),
			Manifest.PreflightErrors.Num(),
			*Manifest.PreflightErrors[0]);
	}
}

void HCI_RunAbilityKitIngestImportLatestCommand(const TArray<FString>& Args)
{
	FHCIAbilityKitIngestManifest Manifest;
	FString Error;
	if (!HCI_TryLoadLatestManifest(Manifest, Error))
	{
		UE_LOG(
			LogHCIAbilityKitIngest,
			Error,
			TEXT("[HCIAbilityKit][Ingest] import_latest failed reason=%s"),
			Error.IsEmpty() ? TEXT("-") : *Error);
		return;
	}

	if (!Manifest.bPreflightOk)
	{
		const FString Err = Manifest.PreflightErrors.Num() > 0 ? Manifest.PreflightErrors[0] : FString(TEXT("-"));
		UE_LOG(
			LogHCIAbilityKitIngest,
			Error,
			TEXT("[HCIAbilityKit][Ingest] import_latest rejected reason=preflight_not_ok batch_id=%s first_error=%s"),
			Manifest.BatchId.IsEmpty() ? TEXT("-") : *Manifest.BatchId,
			*Err);
		return;
	}

	if (!HCI_IsValidGameContentPath(Manifest.SuggestedUnrealTargetRoot))
	{
		UE_LOG(
			LogHCIAbilityKitIngest,
			Error,
			TEXT("[HCIAbilityKit][Ingest] import_latest rejected reason=invalid_target_root target_root=%s"),
			Manifest.SuggestedUnrealTargetRoot.IsEmpty() ? TEXT("-") : *Manifest.SuggestedUnrealTargetRoot);
		return;
	}

	const FString ConfirmText = FString::Printf(
		TEXT("即将导入外部投递批次。\n\nbatch_id: %s\nfiles: %d\ntarget_root: %s\n\n继续导入？\n\n备注：本操作会创建新资产（默认不覆盖已存在资产）。"),
		Manifest.BatchId.IsEmpty() ? TEXT("-") : *Manifest.BatchId,
		Manifest.Files.Num(),
		Manifest.SuggestedUnrealTargetRoot.IsEmpty() ? TEXT("-") : *Manifest.SuggestedUnrealTargetRoot);

	const EAppReturnType::Type Choice = FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(ConfirmText));
	if (Choice != EAppReturnType::Yes)
	{
		UE_LOG(
			LogHCIAbilityKitIngest,
			Warning,
			TEXT("[HCIAbilityKit][Ingest] import_latest cancelled_by_user batch_id=%s"),
			Manifest.BatchId.IsEmpty() ? TEXT("-") : *Manifest.BatchId);
		return;
	}

	TArray<UAssetImportTask*> Tasks;
	Tasks.Reserve(Manifest.Files.Num());

	for (const FHCIAbilityKitIngestManifestFile& File : Manifest.Files)
	{
		const FString AbsFilePath = FPaths::ConvertRelativePathToFull(FPaths::Combine(Manifest.BatchDirectory, File.RelativePath));
		if (!FPaths::FileExists(AbsFilePath))
		{
			UE_LOG(
				LogHCIAbilityKitIngest,
				Error,
				TEXT("[HCIAbilityKit][Ingest] import_latest failed reason=staged_file_missing rel=%s abs=%s"),
				File.RelativePath.IsEmpty() ? TEXT("-") : *File.RelativePath,
				*AbsFilePath);
			return;
		}

		UAssetImportTask* Task = NewObject<UAssetImportTask>(GetTransientPackage());
		Task->Filename = AbsFilePath;
		Task->DestinationPath = Manifest.SuggestedUnrealTargetRoot;
		Task->DestinationName = File.SuggestedAssetName;
		Task->bReplaceExisting = false;
		Task->bReplaceExistingSettings = false;
		Task->bAutomated = true;
		Task->bSave = true;
		Task->bAsync = false;

		Tasks.Add(Task);
	}

	if (Tasks.Num() <= 0)
	{
		UE_LOG(LogHCIAbilityKitIngest, Error, TEXT("[HCIAbilityKit][Ingest] import_latest failed reason=no_tasks"));
		return;
	}

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	AssetToolsModule.Get().ImportAssetTasks(Tasks);

	int32 TotalImported = 0;
	TArray<FString> SamplePaths;
	for (UAssetImportTask* Task : Tasks)
	{
		if (!Task)
		{
			continue;
		}
		TotalImported += Task->ImportedObjectPaths.Num();
		for (const FString& P : Task->ImportedObjectPaths)
		{
			if (SamplePaths.Num() < 10)
			{
				SamplePaths.Add(P);
			}
		}
	}

	const FString Sample = SamplePaths.Num() > 0 ? FString::Join(SamplePaths, TEXT(", ")) : FString(TEXT("-"));
	UE_LOG(
		LogHCIAbilityKitIngest,
		Display,
		TEXT("[HCIAbilityKit][Ingest] import_latest ok batch_id=%s tasks=%d imported_object_paths=%d sample=%s"),
		Manifest.BatchId.IsEmpty() ? TEXT("-") : *Manifest.BatchId,
		Tasks.Num(),
		TotalImported,
		*Sample);
}

void FHCIAbilityKitAgentDemoConsoleCommands::StartupIngestCommands()
{
	if (!IngestDumpLatestCommand.IsValid())
	{
		IngestDumpLatestCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.IngestDumpLatest"),
			TEXT("Stage N ingest: dump latest manifest summary. Usage: HCIAbilityKit.IngestDumpLatest"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitIngestDumpLatestCommand));
	}

	if (!IngestImportLatestCommand.IsValid())
	{
		IngestImportLatestCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.IngestImportLatest"),
			TEXT("Stage N ingest: import staged files from latest batch (requires confirmation). Usage: HCIAbilityKit.IngestImportLatest"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitIngestImportLatestCommand));
	}
}

void FHCIAbilityKitAgentDemoConsoleCommands::ShutdownIngestCommands()
{
	IngestImportLatestCommand.Reset();
	IngestDumpLatestCommand.Reset();
}
