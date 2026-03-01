#include "Ingest/HCIIngestManifest.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
static bool HCI_IsSafeRelativePath(const FString& Rel)
{
	const FString P = Rel.Replace(TEXT("\\"), TEXT("/"));
	if (P.IsEmpty())
	{
		return false;
	}
	if (P.StartsWith(TEXT("/")))
	{
		return false;
	}
	if (P.Contains(TEXT("..")))
	{
		return false;
	}
	return true;
}

static FString HCI_JoinForEnv(const TArray<FString>& Items, const int32 MaxCount)
{
	if (Items.Num() <= 0 || MaxCount <= 0)
	{
		return TEXT("-");
	}

	const int32 Emit = FMath::Min(MaxCount, Items.Num());
	TArray<FString> Slice;
	Slice.Reserve(Emit);
	for (int32 i = 0; i < Emit; ++i)
	{
		Slice.Add(Items[i]);
	}
	FString Out = FString::Join(Slice, TEXT(", "));
	if (Items.Num() > Emit)
	{
		Out += FString::Printf(TEXT(", ...(+%d)"), Items.Num() - Emit);
	}
	return Out;
}
}

FString FHCIIngestManifest::BuildEnvContextSnippet(const int32 MaxFileNames) const
{
	TArray<FString> Names;
	Names.Reserve(Files.Num());
	for (const FHCIIngestManifestFile& F : Files)
	{
		const FString Leaf = FPaths::GetCleanFilename(F.RelativePath);
		if (!Leaf.IsEmpty())
		{
			Names.Add(Leaf);
		}
	}
	Names.Sort();

	const FString FileList = HCI_JoinForEnv(Names, MaxFileNames);
	const FString WarnList = PreflightWarnings.Num() > 0 ? HCI_JoinForEnv(PreflightWarnings, 6) : TEXT("-");
	const FString ErrList = PreflightErrors.Num() > 0 ? HCI_JoinForEnv(PreflightErrors, 6) : TEXT("-");

	// Keep it "YAML-ish" because ENV_CONTEXT is currently rendered as plaintext.
	return FString::Printf(
		TEXT("ingest_batch:\n")
		TEXT("- batch_id: %s\n")
		TEXT("  source_app: %s\n")
		TEXT("  preflight_ok: %s\n")
		TEXT("  file_count: %d\n")
		TEXT("  suggested_unreal_target_root: %s\n")
		TEXT("  files: %s\n")
		TEXT("  warnings: %s\n")
		TEXT("  errors: %s\n"),
		BatchId.IsEmpty() ? TEXT("-") : *BatchId,
		SourceApp.IsEmpty() ? TEXT("unknown") : *SourceApp,
		bPreflightOk ? TEXT("true") : TEXT("false"),
		Files.Num(),
		SuggestedUnrealTargetRoot.IsEmpty() ? TEXT("-") : *SuggestedUnrealTargetRoot,
		*FileList,
		*WarnList,
		*ErrList);
}

bool FHCIIngestManifest::TryFindLatestManifestUnderRoot(
	const FString& IngestRootDir,
	FString& OutManifestPath,
	FString& OutError)
{
	OutManifestPath.Reset();
	OutError.Reset();

	const FString Root = IngestRootDir.TrimStartAndEnd();
	if (Root.IsEmpty())
	{
		OutError = TEXT("ingest_root_empty");
		return false;
	}

	TArray<FString> Candidates;
	IFileManager::Get().FindFilesRecursive(Candidates, *Root, TEXT("manifest.hci.json"), true, false);
	if (Candidates.Num() <= 0)
	{
		OutError = TEXT("manifest_not_found");
		return false;
	}

	Candidates.Sort([](const FString& A, const FString& B)
	{
		return IFileManager::Get().GetTimeStamp(*A) > IFileManager::Get().GetTimeStamp(*B);
	});

	OutManifestPath = Candidates[0];
	return true;
}

bool FHCIIngestManifest::TryLoadFromFile(
	const FString& InManifestPath,
	FHCIIngestManifest& OutManifest,
	FString& OutError)
{
	OutManifest = FHCIIngestManifest();
	OutError.Reset();

	const FString Path = InManifestPath.TrimStartAndEnd();
	if (Path.IsEmpty() || !FPaths::FileExists(Path))
	{
		OutError = FString::Printf(TEXT("manifest_not_found path=%s"), *Path);
		return false;
	}

	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *Path))
	{
		OutError = FString::Printf(TEXT("manifest_read_failed path=%s"), *Path);
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = FString::Printf(TEXT("manifest_invalid_json path=%s"), *Path);
		return false;
	}

	OutManifest.ManifestPath = Path;
	OutManifest.BatchDirectory = FPaths::GetPath(Path);

	double SchemaNumber = 0.0;
	if (Root->TryGetNumberField(TEXT("schema_version"), SchemaNumber))
	{
		OutManifest.SchemaVersion = static_cast<int32>(SchemaNumber);
	}
	Root->TryGetStringField(TEXT("batch_id"), OutManifest.BatchId);
	Root->TryGetStringField(TEXT("source_app"), OutManifest.SourceApp);
	Root->TryGetStringField(TEXT("staged_at_utc"), OutManifest.StagedAtUtc);
	Root->TryGetStringField(TEXT("suggested_unreal_target_root"), OutManifest.SuggestedUnrealTargetRoot);

	const TSharedPtr<FJsonObject>* PreflightPtr = nullptr;
	if (Root->TryGetObjectField(TEXT("preflight"), PreflightPtr) && PreflightPtr != nullptr && PreflightPtr->IsValid())
	{
		const TSharedPtr<FJsonObject> Preflight = *PreflightPtr;
		Preflight->TryGetBoolField(TEXT("ok"), OutManifest.bPreflightOk);

		const TArray<TSharedPtr<FJsonValue>>* WarnArr = nullptr;
		if (Preflight->TryGetArrayField(TEXT("warnings"), WarnArr) && WarnArr != nullptr)
		{
			for (const TSharedPtr<FJsonValue>& V : *WarnArr)
			{
				const FString S = V.IsValid() ? V->AsString() : FString();
				if (!S.IsEmpty())
				{
					OutManifest.PreflightWarnings.Add(S);
				}
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* ErrArr = nullptr;
		if (Preflight->TryGetArrayField(TEXT("errors"), ErrArr) && ErrArr != nullptr)
		{
			for (const TSharedPtr<FJsonValue>& V : *ErrArr)
			{
				const FString S = V.IsValid() ? V->AsString() : FString();
				if (!S.IsEmpty())
				{
					OutManifest.PreflightErrors.Add(S);
				}
			}
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* FilesArr = nullptr;
	if (!Root->TryGetArrayField(TEXT("files"), FilesArr) || FilesArr == nullptr)
	{
		OutError = TEXT("manifest_missing_files");
		return false;
	}

	for (const TSharedPtr<FJsonValue>& V : *FilesArr)
	{
		const TSharedPtr<FJsonObject> Obj = V.IsValid() ? V->AsObject() : nullptr;
		if (!Obj.IsValid())
		{
			continue;
		}

		FHCIIngestManifestFile File;
		Obj->TryGetStringField(TEXT("relative_path"), File.RelativePath);
		Obj->TryGetStringField(TEXT("kind"), File.Kind);
		Obj->TryGetStringField(TEXT("sha1"), File.Sha1);
		Obj->TryGetStringField(TEXT("suggested_asset_name"), File.SuggestedAssetName);

		double Size = -1.0;
		if (Obj->TryGetNumberField(TEXT("size_bytes"), Size))
		{
			File.SizeBytes = static_cast<int64>(Size);
		}

		File.RelativePath.TrimStartAndEndInline();
		if (!HCI_IsSafeRelativePath(File.RelativePath))
		{
			// Skip unsafe paths.
			continue;
		}

		OutManifest.Files.Add(MoveTemp(File));
	}

	if (OutManifest.Files.Num() <= 0)
	{
		OutError = TEXT("manifest_files_empty");
		return false;
	}

	return true;
}

