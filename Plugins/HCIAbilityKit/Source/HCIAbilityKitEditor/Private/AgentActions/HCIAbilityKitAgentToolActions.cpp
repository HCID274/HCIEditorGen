#include "AgentActions/HCIAbilityKitAgentToolActions.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "EditorAssetLibrary.h"
#include "Modules/ModuleManager.h"

namespace
{
static bool HCI_TrySplitObjectPath(
	const FString& ObjectPath,
	FString& OutPackagePath,
	FString& OutAssetName)
{
	OutPackagePath.Reset();
	OutAssetName.Reset();

	if (!ObjectPath.StartsWith(TEXT("/Game/")))
	{
		return false;
	}

	FString PackagePath = ObjectPath;
	const int32 DotIndex = PackagePath.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromStart);
	if (DotIndex != INDEX_NONE)
	{
		PackagePath = PackagePath.Left(DotIndex);
	}

	int32 LastSlash = INDEX_NONE;
	if (!PackagePath.FindLastChar(TEXT('/'), LastSlash) || LastSlash <= 0 || LastSlash + 1 >= PackagePath.Len())
	{
		return false;
	}

	OutPackagePath = PackagePath;
	OutAssetName = PackagePath.Mid(LastSlash + 1);
	return !OutAssetName.IsEmpty();
}

static FString HCI_ToObjectPath(const FString& PackagePath, const FString& AssetName)
{
	return FString::Printf(TEXT("%s.%s"), *PackagePath, *AssetName);
}

static FString HCI_GetDirectoryFromPackagePath(const FString& PackagePath)
{
	int32 LastSlash = INDEX_NONE;
	if (!PackagePath.FindLastChar(TEXT('/'), LastSlash) || LastSlash <= 0)
	{
		return FString();
	}
	return PackagePath.Left(LastSlash);
}

static bool HCI_TryReadRequiredStringArg(
	const TSharedPtr<FJsonObject>& Args,
	const TCHAR* Field,
	FString& OutValue)
{
	OutValue.Reset();
	if (!Args.IsValid())
	{
		return false;
	}
	if (!Args->TryGetStringField(Field, OutValue))
	{
		return false;
	}
	OutValue.TrimStartAndEndInline();
	return !OutValue.IsEmpty();
}

static bool HCI_ValidateSourceAssetExists(
	const FString& SourceAssetPath,
	FHCIAbilityKitAgentToolActionResult& OutResult)
{
	if (!SourceAssetPath.StartsWith(TEXT("/Game/")))
	{
		OutResult.bSucceeded = false;
		OutResult.ErrorCode = TEXT("E4009");
		OutResult.Reason = TEXT("asset_path_must_start_with_game");
		return false;
	}

	if (!UEditorAssetLibrary::DoesAssetExist(SourceAssetPath))
	{
		OutResult.bSucceeded = false;
		OutResult.ErrorCode = TEXT("E4201");
		OutResult.Reason = TEXT("asset_not_found");
		return false;
	}
	return true;
}

static int32 HCI_ComputePathKeywordScore(const FString& PathLower, const FString& KeywordLower)
{
	if (KeywordLower.IsEmpty())
	{
		return 0;
	}

	if (!PathLower.Contains(KeywordLower))
	{
		return 0;
	}

	int32 Score = 100;
	const FString SuffixToken = FString::Printf(TEXT("/%s"), *KeywordLower);
	if (PathLower.EndsWith(SuffixToken))
	{
		Score += 300;
	}
	if (PathLower.StartsWith(SuffixToken) || PathLower.StartsWith(FString::Printf(TEXT("/game/%s"), *KeywordLower)))
	{
		Score += 250;
	}
	if (PathLower.Contains(SuffixToken))
	{
		Score += 100;
	}
	Score += FMath::Min(50, KeywordLower.Len());
	return Score;
}

class FHCIAbilityKitScanAssetsToolAction final : public IHCIAbilityKitAgentToolAction
{
public:
	virtual FName GetToolName() const override
	{
		return TEXT("ScanAssets");
	}

	virtual bool DryRun(
		const FHCIAbilityKitAgentToolActionRequest& Request,
		FHCIAbilityKitAgentToolActionResult& OutResult) const override
	{
		return RunInternal(Request, OutResult);
	}

	virtual bool Execute(
		const FHCIAbilityKitAgentToolActionRequest& Request,
		FHCIAbilityKitAgentToolActionResult& OutResult) const override
	{
		return RunInternal(Request, OutResult);
	}

private:
	static bool RunInternal(
		const FHCIAbilityKitAgentToolActionRequest& Request,
		FHCIAbilityKitAgentToolActionResult& OutResult)
	{
		FString Directory = TEXT("/Game/Temp");
		if (Request.Args.IsValid())
		{
			Request.Args->TryGetStringField(TEXT("directory"), Directory);
			Request.Args->TryGetStringField(TEXT("target_root"), Directory);
		}
		if (Directory.IsEmpty() || !Directory.StartsWith(TEXT("/Game")))
		{
			Directory = TEXT("/Game/Temp");
		}

		const TArray<FString> AssetPaths = UEditorAssetLibrary::ListAssets(Directory, true, false);
		OutResult = FHCIAbilityKitAgentToolActionResult();
		OutResult.bSucceeded = true;
		OutResult.Reason = TEXT("scan_assets_ok");
		OutResult.EstimatedAffectedCount = AssetPaths.Num();
		OutResult.Evidence.Add(TEXT("scan_root"), Directory);
		OutResult.Evidence.Add(TEXT("asset_count"), FString::FromInt(AssetPaths.Num()));
		OutResult.Evidence.Add(TEXT("result"), FString::Printf(TEXT("scan_assets_ok count=%d"), AssetPaths.Num()));
		OutResult.Evidence.Add(TEXT("asset_path"), AssetPaths.Num() > 0 ? AssetPaths[0] : TEXT("-"));
		OutResult.Evidence.Add(TEXT("asset_paths"), FString::Join(AssetPaths, TEXT("|")));
		return true;
	}
};

class FHCIAbilityKitSearchPathToolAction final : public IHCIAbilityKitAgentToolAction
{
public:
	virtual FName GetToolName() const override
	{
		return TEXT("SearchPath");
	}

	virtual bool DryRun(
		const FHCIAbilityKitAgentToolActionRequest& Request,
		FHCIAbilityKitAgentToolActionResult& OutResult) const override
	{
		return RunInternal(Request, OutResult);
	}

	virtual bool Execute(
		const FHCIAbilityKitAgentToolActionRequest& Request,
		FHCIAbilityKitAgentToolActionResult& OutResult) const override
	{
		return RunInternal(Request, OutResult);
	}

private:
	struct FHCIPathCandidate
	{
		FString Path;
		int32 Score = 0;
	};

	static bool RunInternal(
		const FHCIAbilityKitAgentToolActionRequest& Request,
		FHCIAbilityKitAgentToolActionResult& OutResult)
	{
		FString Keyword;
		if (!HCI_TryReadRequiredStringArg(Request.Args, TEXT("keyword"), Keyword))
		{
			OutResult = FHCIAbilityKitAgentToolActionResult();
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4001");
			OutResult.Reason = TEXT("required_arg_missing");
			return false;
		}

		const FString KeywordLower = Keyword.ToLower();
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		IAssetRegistry& Registry = AssetRegistryModule.Get();

		TArray<FString> CachedPaths;
		Registry.GetAllCachedPaths(CachedPaths);

		TArray<FHCIPathCandidate> Candidates;
		Candidates.Reserve(CachedPaths.Num());
		for (const FString& Path : CachedPaths)
		{
			if (!Path.StartsWith(TEXT("/Game/")))
			{
				continue;
			}

			const int32 Score = HCI_ComputePathKeywordScore(Path.ToLower(), KeywordLower);
			if (Score <= 0)
			{
				continue;
			}

			FHCIPathCandidate& Candidate = Candidates.AddDefaulted_GetRef();
			Candidate.Path = Path;
			Candidate.Score = Score;
		}

		Candidates.Sort([](const FHCIPathCandidate& A, const FHCIPathCandidate& B)
		{
			if (A.Score != B.Score)
			{
				return A.Score > B.Score;
			}
			if (A.Path.Len() != B.Path.Len())
			{
				return A.Path.Len() < B.Path.Len();
			}
			return A.Path < B.Path;
		});

		TArray<FString> MatchedDirectories;
		MatchedDirectories.Reserve(3);
		for (const FHCIPathCandidate& Candidate : Candidates)
		{
			if (MatchedDirectories.Num() >= 3)
			{
				break;
			}
			MatchedDirectories.Add(Candidate.Path);
		}

		OutResult = FHCIAbilityKitAgentToolActionResult();
		OutResult.bSucceeded = true;
		OutResult.Reason = TEXT("search_path_ok");
		OutResult.EstimatedAffectedCount = MatchedDirectories.Num();
		OutResult.Evidence.Add(TEXT("keyword"), Keyword);
		OutResult.Evidence.Add(TEXT("matched_count"), FString::FromInt(MatchedDirectories.Num()));
		OutResult.Evidence.Add(TEXT("matched_directories"), FString::Join(MatchedDirectories, TEXT("|")));
		OutResult.Evidence.Add(TEXT("best_directory"), MatchedDirectories.Num() > 0 ? MatchedDirectories[0] : TEXT("-"));
		for (int32 Index = 0; Index < MatchedDirectories.Num(); ++Index)
		{
			OutResult.Evidence.Add(
				FString::Printf(TEXT("matched_directories[%d]"), Index),
				MatchedDirectories[Index]);
		}
		OutResult.Evidence.Add(TEXT("result"), TEXT("search_path_ok"));
		return true;
	}
};

class FHCIAbilityKitRenameAssetToolAction final : public IHCIAbilityKitAgentToolAction
{
public:
	virtual FName GetToolName() const override
	{
		return TEXT("RenameAsset");
	}

	virtual bool DryRun(
		const FHCIAbilityKitAgentToolActionRequest& Request,
		FHCIAbilityKitAgentToolActionResult& OutResult) const override
	{
		FString SourceAssetPath;
		FString NewName;
		if (!HCI_TryReadRequiredStringArg(Request.Args, TEXT("asset_path"), SourceAssetPath) ||
			!HCI_TryReadRequiredStringArg(Request.Args, TEXT("new_name"), NewName))
		{
			OutResult = FHCIAbilityKitAgentToolActionResult();
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4001");
			OutResult.Reason = TEXT("required_arg_missing");
			return false;
		}

		OutResult = FHCIAbilityKitAgentToolActionResult();
		if (!HCI_ValidateSourceAssetExists(SourceAssetPath, OutResult))
		{
			return false;
		}

		FString SourcePackagePath;
		FString SourceAssetName;
		if (!HCI_TrySplitObjectPath(SourceAssetPath, SourcePackagePath, SourceAssetName))
		{
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4009");
			OutResult.Reason = TEXT("asset_path_invalid");
			return false;
		}

		const FString SourceDir = HCI_GetDirectoryFromPackagePath(SourcePackagePath);
		const FString DestinationPackagePath = FString::Printf(TEXT("%s/%s"), *SourceDir, *NewName);
		const FString DestinationAssetPath = HCI_ToObjectPath(DestinationPackagePath, NewName);

		OutResult.bSucceeded = true;
		OutResult.Reason = TEXT("rename_dry_run_ok");
		OutResult.EstimatedAffectedCount = 1;
		OutResult.Evidence.Add(TEXT("asset_path"), SourceAssetPath);
		OutResult.Evidence.Add(TEXT("before"), SourceAssetPath);
		OutResult.Evidence.Add(TEXT("after"), DestinationAssetPath);
		OutResult.Evidence.Add(TEXT("result"), TEXT("rename_dry_run_ok"));
		return true;
	}

	virtual bool Execute(
		const FHCIAbilityKitAgentToolActionRequest& Request,
		FHCIAbilityKitAgentToolActionResult& OutResult) const override
	{
		if (!DryRun(Request, OutResult))
		{
			return false;
		}

		const FString SourceAssetPath = OutResult.Evidence.FindRef(TEXT("before"));
		const FString DestinationAssetPath = OutResult.Evidence.FindRef(TEXT("after"));
		if (SourceAssetPath.IsEmpty() || DestinationAssetPath.IsEmpty())
		{
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4303");
			OutResult.Reason = TEXT("rename_execute_missing_paths");
			return false;
		}

		if (SourceAssetPath.Equals(DestinationAssetPath, ESearchCase::CaseSensitive))
		{
			OutResult.bSucceeded = true;
			OutResult.Reason = TEXT("rename_noop_same_path");
			OutResult.Evidence.Add(TEXT("result"), TEXT("rename_noop_same_path"));
			return true;
		}

			const int32 DotIndex = DestinationAssetPath.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromStart);
			const FString DestinationPackagePath = (DotIndex == INDEX_NONE) ? DestinationAssetPath : DestinationAssetPath.Left(DotIndex);
		const FString DestinationDir = HCI_GetDirectoryFromPackagePath(DestinationPackagePath);
		if (!DestinationDir.IsEmpty())
		{
			UEditorAssetLibrary::MakeDirectory(DestinationDir);
		}

		const bool bRenamed = UEditorAssetLibrary::RenameAsset(SourceAssetPath, DestinationAssetPath);
		OutResult.bSucceeded = bRenamed;
		OutResult.ErrorCode = bRenamed ? FString() : TEXT("E4203");
		OutResult.Reason = bRenamed ? TEXT("rename_execute_ok") : TEXT("rename_execute_failed");
		OutResult.Evidence.Add(TEXT("result"), bRenamed ? TEXT("rename_execute_ok") : TEXT("rename_execute_failed"));
		return bRenamed;
	}
};

class FHCIAbilityKitMoveAssetToolAction final : public IHCIAbilityKitAgentToolAction
{
public:
	virtual FName GetToolName() const override
	{
		return TEXT("MoveAsset");
	}

	virtual bool DryRun(
		const FHCIAbilityKitAgentToolActionRequest& Request,
		FHCIAbilityKitAgentToolActionResult& OutResult) const override
	{
		FString SourceAssetPath;
		FString TargetPath;
		if (!HCI_TryReadRequiredStringArg(Request.Args, TEXT("asset_path"), SourceAssetPath) ||
			!HCI_TryReadRequiredStringArg(Request.Args, TEXT("target_path"), TargetPath))
		{
			OutResult = FHCIAbilityKitAgentToolActionResult();
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4001");
			OutResult.Reason = TEXT("required_arg_missing");
			return false;
		}

		OutResult = FHCIAbilityKitAgentToolActionResult();
		if (!HCI_ValidateSourceAssetExists(SourceAssetPath, OutResult))
		{
			return false;
		}

		FString SourcePackagePath;
		FString SourceAssetName;
		if (!HCI_TrySplitObjectPath(SourceAssetPath, SourcePackagePath, SourceAssetName))
		{
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4009");
			OutResult.Reason = TEXT("asset_path_invalid");
			return false;
		}

		FString DestinationAssetPath;
		if (TargetPath.Contains(TEXT(".")))
		{
			DestinationAssetPath = TargetPath;
		}
		else
		{
			FString TargetDir = TargetPath;
			if (TargetDir.EndsWith(TEXT("/")))
			{
				TargetDir.LeftChopInline(1, false);
			}
			DestinationAssetPath = FString::Printf(TEXT("%s/%s.%s"), *TargetDir, *SourceAssetName, *SourceAssetName);
		}

		if (!DestinationAssetPath.StartsWith(TEXT("/Game/")))
		{
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4009");
			OutResult.Reason = TEXT("target_path_must_start_with_game");
			return false;
		}

		OutResult.bSucceeded = true;
		OutResult.Reason = TEXT("move_dry_run_ok");
		OutResult.EstimatedAffectedCount = 1;
		OutResult.Evidence.Add(TEXT("asset_path"), SourceAssetPath);
		OutResult.Evidence.Add(TEXT("before"), SourceAssetPath);
		OutResult.Evidence.Add(TEXT("after"), DestinationAssetPath);
		OutResult.Evidence.Add(TEXT("result"), TEXT("move_dry_run_ok"));
		return true;
	}

	virtual bool Execute(
		const FHCIAbilityKitAgentToolActionRequest& Request,
		FHCIAbilityKitAgentToolActionResult& OutResult) const override
	{
		if (!DryRun(Request, OutResult))
		{
			return false;
		}

		const FString SourceAssetPath = OutResult.Evidence.FindRef(TEXT("before"));
		const FString DestinationAssetPath = OutResult.Evidence.FindRef(TEXT("after"));
		if (SourceAssetPath.IsEmpty() || DestinationAssetPath.IsEmpty())
		{
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4303");
			OutResult.Reason = TEXT("move_execute_missing_paths");
			return false;
		}

		const int32 DotIndex = DestinationAssetPath.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromStart);
		const FString DestinationPackagePath = (DotIndex == INDEX_NONE) ? DestinationAssetPath : DestinationAssetPath.Left(DotIndex);
		const FString DestinationDir = HCI_GetDirectoryFromPackagePath(DestinationPackagePath);
		if (!DestinationDir.IsEmpty())
		{
			UEditorAssetLibrary::MakeDirectory(DestinationDir);
		}

		const bool bMoved = UEditorAssetLibrary::RenameAsset(SourceAssetPath, DestinationAssetPath);
		OutResult.bSucceeded = bMoved;
		OutResult.ErrorCode = bMoved ? FString() : TEXT("E4204");
		OutResult.Reason = bMoved ? TEXT("move_execute_ok") : TEXT("move_execute_failed");
		OutResult.Evidence.Add(TEXT("result"), bMoved ? TEXT("move_execute_ok") : TEXT("move_execute_failed"));
		return bMoved;
	}
};
}

void HCIAbilityKitAgentToolActions::BuildStageIDraftActions(TMap<FName, TSharedPtr<IHCIAbilityKitAgentToolAction>>& OutActions)
{
	OutActions.Reset();
	OutActions.Add(TEXT("ScanAssets"), MakeShared<FHCIAbilityKitScanAssetsToolAction>());
	OutActions.Add(TEXT("SearchPath"), MakeShared<FHCIAbilityKitSearchPathToolAction>());
	OutActions.Add(TEXT("RenameAsset"), MakeShared<FHCIAbilityKitRenameAssetToolAction>());
	OutActions.Add(TEXT("MoveAsset"), MakeShared<FHCIAbilityKitMoveAssetToolAction>());
}
