#include "AgentActions/HCIAbilityKitAgentToolActions.h"

#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "EditorAssetLibrary.h"
#include "Modules/ModuleManager.h"
#include "UObject/ObjectRedirector.h"

DEFINE_LOG_CATEGORY_STATIC(LogHCIAbilityKitAgentToolActions, Log, All);

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

static FString HCI_GetDirectoryLeafName(const FString& Path)
{
	int32 LastSlash = INDEX_NONE;
	if (!Path.FindLastChar(TEXT('/'), LastSlash) || LastSlash < 0 || LastSlash + 1 >= Path.Len())
	{
		return Path;
	}
	return Path.Mid(LastSlash + 1);
}

static FString HCI_NormalizeFuzzyToken(const FString& Text)
{
	FString Normalized = Text.ToLower();
	Normalized.ReplaceInline(TEXT("_"), TEXT(""), ESearchCase::CaseSensitive);
	Normalized.ReplaceInline(TEXT(" "), TEXT(""), ESearchCase::CaseSensitive);
	Normalized.ReplaceInline(TEXT("-"), TEXT(""), ESearchCase::CaseSensitive);
	return Normalized;
}

static void HCI_AddUniqueSearchKeyword(TArray<FString>& OutKeywords, const FString& Keyword)
{
	FString Trimmed = Keyword;
	Trimmed.TrimStartAndEndInline();
	if (Trimmed.IsEmpty())
	{
		return;
	}

	for (const FString& Existing : OutKeywords)
	{
		if (Existing.Equals(Trimmed, ESearchCase::IgnoreCase))
		{
			return;
		}
	}
	OutKeywords.Add(Trimmed);
}

static FString HCI_RemoveDirectorySemanticSuffix(const FString& InKeyword)
{
	FString Out = InKeyword;
	Out.ReplaceInline(TEXT("目录"), TEXT(""), ESearchCase::CaseSensitive);
	Out.ReplaceInline(TEXT("文件夹"), TEXT(""), ESearchCase::CaseSensitive);
	Out.ReplaceInline(TEXT("folder"), TEXT(""), ESearchCase::IgnoreCase);
	Out.ReplaceInline(TEXT("directory"), TEXT(""), ESearchCase::IgnoreCase);
	Out.TrimStartAndEndInline();
	return Out;
}

static TArray<FString> HCI_ExpandSearchKeywords(const FString& RawKeyword)
{
	TArray<FString> Expanded;
	HCI_AddUniqueSearchKeyword(Expanded, RawKeyword);

	const FString Stripped = HCI_RemoveDirectorySemanticSuffix(RawKeyword);
	HCI_AddUniqueSearchKeyword(Expanded, Stripped);

	const FString Normalized = HCI_NormalizeFuzzyToken(RawKeyword);
	if (Normalized.Contains(TEXT("临时")) || Normalized.Contains(TEXT("temp")) || Normalized.Contains(TEXT("tmp")))
	{
		HCI_AddUniqueSearchKeyword(Expanded, TEXT("Temp"));
	}
	if (Normalized.Contains(TEXT("艺术")) || Normalized.Contains(TEXT("美术")) || Normalized.Contains(TEXT("art")))
	{
		HCI_AddUniqueSearchKeyword(Expanded, TEXT("Art"));
	}
	if (Normalized.Contains(TEXT("关卡")) || Normalized.Contains(TEXT("场景")) || Normalized.Contains(TEXT("level")) || Normalized.Contains(TEXT("map")))
	{
		HCI_AddUniqueSearchKeyword(Expanded, TEXT("Maps"));
	}
	if (Normalized.Contains(TEXT("角色")) || Normalized.Contains(TEXT("character")))
	{
		HCI_AddUniqueSearchKeyword(Expanded, TEXT("Character"));
	}

	if (Expanded.Num() == 0)
	{
		HCI_AddUniqueSearchKeyword(Expanded, TEXT("Temp"));
	}
	return Expanded;
}

static bool HCI_TryResolveSemanticFallbackDirectory(const TArray<FString>& SearchKeywords, FString& OutDirectory)
{
	OutDirectory.Reset();
	for (const FString& Keyword : SearchKeywords)
	{
		const FString Normalized = HCI_NormalizeFuzzyToken(Keyword);
		if (Normalized.IsEmpty())
		{
			continue;
		}
		if (Normalized.Contains(TEXT("temp")) || Normalized.Contains(TEXT("tmp")) || Normalized.Contains(TEXT("临时")))
		{
			OutDirectory = TEXT("/Game/Temp");
			return true;
		}
		if (Normalized.Contains(TEXT("art")) || Normalized.Contains(TEXT("艺术")) || Normalized.Contains(TEXT("美术")))
		{
			OutDirectory = TEXT("/Game/Art");
			return true;
		}
		if (Normalized.Contains(TEXT("map")) || Normalized.Contains(TEXT("level")) || Normalized.Contains(TEXT("关卡")) || Normalized.Contains(TEXT("场景")))
		{
			OutDirectory = TEXT("/Game/Maps");
			return true;
		}
	}
	return false;
}

static int32 HCI_ComputePathKeywordScore(const FString& Path, const FString& Keyword)
{
	const FString KeywordNormalized = HCI_NormalizeFuzzyToken(Keyword);
	const FString PathNormalized = HCI_NormalizeFuzzyToken(Path);
	const FString Leaf = HCI_GetDirectoryLeafName(Path);
	const FString LeafNormalized = HCI_NormalizeFuzzyToken(Leaf);

	if (KeywordNormalized.IsEmpty())
	{
		return 0;
	}

	const bool bLeafExact = (LeafNormalized == KeywordNormalized);
	const bool bLeafContainsKeyword = LeafNormalized.Contains(KeywordNormalized);
	const bool bKeywordContainsLeaf = (LeafNormalized.Len() >= 3) && KeywordNormalized.Contains(LeafNormalized);
	const bool bPathContainsKeyword = PathNormalized.Contains(KeywordNormalized);
	if (!bLeafExact && !bLeafContainsKeyword && !bKeywordContainsLeaf && !bPathContainsKeyword)
	{
		return 0;
	}

	int32 Score = 0;
	if (bLeafExact)
	{
		Score += 1400;
	}
	if (LeafNormalized.StartsWith(KeywordNormalized))
	{
		Score += 700;
	}
	if (bLeafContainsKeyword)
	{
		Score += 550;
	}
	if (bKeywordContainsLeaf)
	{
		Score += 500;
	}
	if (bPathContainsKeyword)
	{
		Score += 350;
	}
	if (Path.EndsWith(Leaf))
	{
		Score += 250;
	}
	Score += FMath::Min(80, KeywordNormalized.Len() * 4);
	return Score;
}

static void HCI_FixupRedirectorReferencers(
	const FString& SourceAssetPath,
	FHCIAbilityKitAgentToolActionResult& OutResult)
{
	UObjectRedirector* Redirector = Cast<UObjectRedirector>(LoadObject<UObject>(nullptr, *SourceAssetPath));
	if (Redirector == nullptr)
	{
		OutResult.Evidence.Add(TEXT("redirector_fixup"), TEXT("no_redirector_found"));
		return;
	}

	TArray<UObjectRedirector*> Redirectors;
	Redirectors.Add(Redirector);

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	AssetToolsModule.Get().FixupReferencers(Redirectors, false, ERedirectFixupMode::DeleteFixedUpRedirectors);
	OutResult.Evidence.Add(TEXT("redirector_fixup"), TEXT("fixup_referencers_called"));
	OutResult.Evidence.Add(TEXT("redirector_count"), FString::FromInt(Redirectors.Num()));
}

static bool HCI_MoveAssetWithAssetTools(const FString& SourceAssetPath, const FString& DestinationAssetPath)
{
	UObject* SourceAsset = LoadObject<UObject>(nullptr, *SourceAssetPath);
	if (SourceAsset == nullptr)
	{
		return false;
	}

	FString DestinationPackagePath;
	FString DestinationAssetName;
	if (!HCI_TrySplitObjectPath(DestinationAssetPath, DestinationPackagePath, DestinationAssetName))
	{
		return false;
	}

	const FString DestinationDir = HCI_GetDirectoryFromPackagePath(DestinationPackagePath);
	if (DestinationDir.IsEmpty())
	{
		return false;
	}

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	TArray<FAssetRenameData> RenameRequests;
	RenameRequests.Emplace(SourceAsset, DestinationDir, DestinationAssetName);
	return AssetToolsModule.Get().RenameAssets(RenameRequests);
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
		FString MatchedKeyword;
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

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		IAssetRegistry& Registry = AssetRegistryModule.Get();

		if (!Registry.IsSearchAllAssets())
		{
			UE_LOG(
				LogHCIAbilityKitAgentToolActions,
				Display,
				TEXT("[HCIAbilityKit][SearchPath] search_all_assets=start keyword=\"%s\""),
				*Keyword);
			Registry.SearchAllAssets(true);
		}

		if (Registry.IsLoadingAssets())
		{
			Registry.WaitForCompletion();
		}

		TArray<FHCIPathCandidate> Candidates;
		int32 ScannedGamePathCount = 0;
		const FString KeywordNormalized = HCI_NormalizeFuzzyToken(Keyword);
		const TArray<FString> SearchKeywords = HCI_ExpandSearchKeywords(Keyword);
		const FString ExpandedKeywordsLog = FString::Join(SearchKeywords, TEXT("|"));
		Registry.EnumerateAllCachedPaths([&](FName PathName)
		{
			const FString Path = PathName.ToString();
			if (!Path.StartsWith(TEXT("/Game/")))
			{
				return true;
			}
			++ScannedGamePathCount;

			const FString LeafName = HCI_GetDirectoryLeafName(Path);
			const FString LeafNormalized = HCI_NormalizeFuzzyToken(LeafName);
			int32 BestScore = 0;
			FString BestKeyword;
			for (const FString& SearchKeyword : SearchKeywords)
			{
				const int32 Score = HCI_ComputePathKeywordScore(Path, SearchKeyword);
				if (Score > BestScore)
				{
					BestScore = Score;
					BestKeyword = SearchKeyword;
				}
			}
			if (BestScore <= 0)
			{
				return true;
			}

			FHCIPathCandidate& Candidate = Candidates.AddDefaulted_GetRef();
			Candidate.Path = Path;
			Candidate.Score = BestScore;
			Candidate.MatchedKeyword = BestKeyword;
			UE_LOG(
				LogHCIAbilityKitAgentToolActions,
				Display,
				TEXT("[HCIAbilityKit][SearchPath] keyword=\"%s\" normalized=\"%s\" matched_keyword=\"%s\" matched_path=\"%s\" leaf=\"%s\" leaf_normalized=\"%s\" score=%d"),
				*Keyword,
				*KeywordNormalized,
				*BestKeyword,
				*Path,
				*LeafName,
				*LeafNormalized,
				BestScore);
			return true;
		});

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

		FString SemanticFallbackDirectory;
		const bool bUsedSemanticFallback = (MatchedDirectories.Num() == 0) &&
			HCI_TryResolveSemanticFallbackDirectory(SearchKeywords, SemanticFallbackDirectory) &&
			!SemanticFallbackDirectory.IsEmpty();
		if (bUsedSemanticFallback)
		{
			MatchedDirectories.Add(SemanticFallbackDirectory);
			UE_LOG(
				LogHCIAbilityKitAgentToolActions,
				Display,
				TEXT("[HCIAbilityKit][SearchPath] semantic_fallback keyword=\"%s\" fallback_directory=\"%s\""),
				*Keyword,
				*SemanticFallbackDirectory);
		}

		OutResult = FHCIAbilityKitAgentToolActionResult();
		OutResult.bSucceeded = true;
		OutResult.Reason = TEXT("search_path_ok");
		OutResult.EstimatedAffectedCount = MatchedDirectories.Num();
		OutResult.Evidence.Add(TEXT("keyword"), Keyword);
		OutResult.Evidence.Add(TEXT("keyword_expanded"), ExpandedKeywordsLog);
		OutResult.Evidence.Add(TEXT("keyword_expanded_count"), FString::FromInt(SearchKeywords.Num()));
		OutResult.Evidence.Add(TEXT("matched_count"), FString::FromInt(MatchedDirectories.Num()));
		OutResult.Evidence.Add(TEXT("matched_directories"), FString::Join(MatchedDirectories, TEXT("|")));
		OutResult.Evidence.Add(TEXT("best_directory"), MatchedDirectories.Num() > 0 ? MatchedDirectories[0] : TEXT("-"));
		OutResult.Evidence.Add(TEXT("semantic_fallback_used"), bUsedSemanticFallback ? TEXT("true") : TEXT("false"));
		OutResult.Evidence.Add(TEXT("semantic_fallback_directory"), bUsedSemanticFallback ? SemanticFallbackDirectory : TEXT("-"));
		OutResult.Evidence.Add(TEXT("keyword_normalized"), KeywordNormalized);
		OutResult.Evidence.Add(TEXT("scanned_game_paths"), FString::FromInt(ScannedGamePathCount));
		for (int32 Index = 0; Index < MatchedDirectories.Num(); ++Index)
		{
			OutResult.Evidence.Add(
				FString::Printf(TEXT("matched_directories[%d]"), Index),
				MatchedDirectories[Index]);
		}
		OutResult.Evidence.Add(TEXT("result"), TEXT("search_path_ok"));
		UE_LOG(
			LogHCIAbilityKitAgentToolActions,
			Display,
			TEXT("[HCIAbilityKit][SearchPath] done keyword=\"%s\" normalized=\"%s\" expanded=\"%s\" scanned_game_paths=%d matched_count=%d best_directory=\"%s\""),
			*Keyword,
			*KeywordNormalized,
			*ExpandedKeywordsLog,
			ScannedGamePathCount,
			MatchedDirectories.Num(),
			MatchedDirectories.Num() > 0 ? *MatchedDirectories[0] : TEXT("-"));
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
		if (bRenamed)
		{
			HCI_FixupRedirectorReferencers(SourceAssetPath, OutResult);
		}
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

		const bool bMoved = HCI_MoveAssetWithAssetTools(SourceAssetPath, DestinationAssetPath);
		OutResult.bSucceeded = bMoved;
		OutResult.ErrorCode = bMoved ? FString() : TEXT("E4204");
		OutResult.Reason = bMoved ? TEXT("move_execute_ok") : TEXT("move_execute_failed");
		if (bMoved)
		{
			HCI_FixupRedirectorReferencers(SourceAssetPath, OutResult);
		}
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
