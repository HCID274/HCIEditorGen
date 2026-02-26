#include "AgentActions/HCIAbilityKitAgentToolActions.h"

#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "EditorFramework/AssetImportData.h"
#include "EditorAssetLibrary.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PhysicsEngine/BodySetup.h"
#include "UObject/ObjectRedirector.h"
#include "UObject/UnrealType.h"

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

static void HCI_NormalizeAssetPathVariants(
	const FString& InPath,
	FString& OutAssetPath,
	FString& OutObjectPath)
{
	OutAssetPath = InPath;
	OutObjectPath = InPath;

	FString PackagePath;
	FString AssetName;
	if (HCI_TrySplitObjectPath(InPath, PackagePath, AssetName))
	{
		OutAssetPath = PackagePath;
		OutObjectPath = HCI_ToObjectPath(PackagePath, AssetName);
	}
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

static FString HCI_TrimTrailingSlash(const FString& InPath)
{
	FString Out = InPath;
	while (Out.EndsWith(TEXT("/")))
	{
		Out.LeftChopInline(1, false);
	}
	return Out;
}

static FString HCI_SanitizeIdentifier(const FString& InText)
{
	FString Out;
	Out.Reserve(InText.Len());
	bool bPrevUnderscore = false;
	for (TCHAR Ch : InText)
	{
		if (FChar::IsAlnum(Ch))
		{
			Out.AppendChar(Ch);
			bPrevUnderscore = false;
			continue;
		}

		if (!bPrevUnderscore)
		{
			Out.AppendChar(TEXT('_'));
			bPrevUnderscore = true;
		}
	}

	while (Out.StartsWith(TEXT("_")))
	{
		Out.RightChopInline(1, false);
	}
	while (Out.EndsWith(TEXT("_")))
	{
		Out.LeftChopInline(1, false);
	}

	if (Out.IsEmpty())
	{
		return FString();
	}

	if (FChar::IsDigit(Out[0]))
	{
		Out = FString::Printf(TEXT("A_%s"), *Out);
	}
	return Out;
}

static FString HCI_RemoveKnownPrefixToken(const FString& InName)
{
	const int32 UnderscoreIndex = InName.Find(TEXT("_"), ESearchCase::CaseSensitive, ESearchDir::FromStart);
	if (UnderscoreIndex <= 0 || UnderscoreIndex + 1 >= InName.Len())
	{
		return InName;
	}

	const FString PrefixToken = InName.Left(UnderscoreIndex).ToUpper();
	static const TSet<FString> KnownPrefixes = {
		TEXT("SM"),
		TEXT("SK"),
		TEXT("T"),
		TEXT("M"),
		TEXT("MI"),
		TEXT("BP"),
		TEXT("S"),
		TEXT("FX")};
	if (!KnownPrefixes.Contains(PrefixToken))
	{
		return InName;
	}

	return InName.Mid(UnderscoreIndex + 1);
}

static FString HCI_DeriveClassPrefix(const UObject* Asset)
{
	if (Asset == nullptr)
	{
		return TEXT("A");
	}
	if (Asset->IsA(UStaticMesh::StaticClass()))
	{
		return TEXT("SM");
	}
	if (Asset->IsA(UTexture2D::StaticClass()))
	{
		return TEXT("T");
	}

	const FString ClassName = Asset->GetClass()->GetName();
	if (ClassName.Contains(TEXT("MaterialInstance")))
	{
		return TEXT("MI");
	}
	if (ClassName.Contains(TEXT("Material")))
	{
		return TEXT("M");
	}
	if (ClassName.Contains(TEXT("SkeletalMesh")))
	{
		return TEXT("SK");
	}
	return TEXT("A");
}

static bool HCI_TryGetAssetImportData(UObject* Asset, UAssetImportData*& OutImportData)
{
	OutImportData = nullptr;
	if (Asset == nullptr)
	{
		return false;
	}

	const FObjectProperty* ImportDataProperty = FindFProperty<FObjectProperty>(Asset->GetClass(), TEXT("AssetImportData"));
	if (ImportDataProperty == nullptr)
	{
		return false;
	}

	UObject* ValueObject = ImportDataProperty->GetObjectPropertyValue_InContainer(Asset);
	OutImportData = Cast<UAssetImportData>(ValueObject);
	return OutImportData != nullptr;
}

static bool HCI_TryExtractImportSourceStem(UObject* Asset, FString& OutStem)
{
	OutStem.Reset();
	UAssetImportData* ImportData = nullptr;
	if (!HCI_TryGetAssetImportData(Asset, ImportData) || ImportData == nullptr)
	{
		return false;
	}

	TArray<FString> SourceFiles;
	ImportData->ExtractFilenames(SourceFiles);
	for (const FString& SourceFile : SourceFiles)
	{
		const FString Trimmed = SourceFile.TrimStartAndEnd();
		if (Trimmed.IsEmpty())
		{
			continue;
		}

		const FString Base = FPaths::GetBaseFilename(Trimmed);
		if (!Base.IsEmpty())
		{
			OutStem = Base;
			return true;
		}
	}
	return false;
}

static FString HCI_DeriveBaseNameFromCurrentName(const FString& CurrentAssetName, const FString& Prefix)
{
	FString Candidate = CurrentAssetName;
	const FString PrefixWithUnderscore = Prefix + TEXT("_");
	if (Candidate.StartsWith(PrefixWithUnderscore, ESearchCase::IgnoreCase))
	{
		Candidate.RightChopInline(PrefixWithUnderscore.Len(), false);
	}
	Candidate = HCI_RemoveKnownPrefixToken(Candidate);

	FString Sanitized = HCI_SanitizeIdentifier(Candidate);
	if (!Sanitized.IsEmpty())
	{
		return Sanitized;
	}
	return HCI_SanitizeIdentifier(CurrentAssetName);
}

static bool HCI_TryBuildNormalizedNameBase(
	UObject* Asset,
	const FString& MetadataSource,
	const FString& Prefix,
	const FString& CurrentAssetName,
	FString& OutBaseName,
	FString& OutFailureReason)
{
	OutBaseName.Reset();
	OutFailureReason.Reset();

	const bool bUseImportDataOnly = MetadataSource.Equals(TEXT("UAssetImportData"), ESearchCase::IgnoreCase);
	const bool bUseAssetUserDataOnly = MetadataSource.Equals(TEXT("AssetUserData"), ESearchCase::IgnoreCase);
	if (bUseAssetUserDataOnly)
	{
		OutFailureReason = TEXT("asset_user_data_metadata_not_supported");
		return false;
	}

	FString SourceStem;
	if (HCI_TryExtractImportSourceStem(Asset, SourceStem))
	{
		OutBaseName = HCI_SanitizeIdentifier(HCI_RemoveKnownPrefixToken(SourceStem));
		if (!OutBaseName.IsEmpty())
		{
			if (OutBaseName.StartsWith(Prefix + TEXT("_"), ESearchCase::IgnoreCase))
			{
				OutBaseName.RightChopInline(Prefix.Len() + 1, false);
				OutBaseName = HCI_SanitizeIdentifier(OutBaseName);
			}
			if (!OutBaseName.IsEmpty())
			{
				return true;
			}
		}
	}

	if (bUseImportDataOnly)
	{
		OutFailureReason = TEXT("import_metadata_missing");
		return false;
	}

	OutBaseName = HCI_DeriveBaseNameFromCurrentName(CurrentAssetName, Prefix);
	if (!OutBaseName.IsEmpty())
	{
		return true;
	}

	OutFailureReason = TEXT("name_sanitization_failed");
	return false;
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

static bool HCI_TryReadRequiredStringArrayArg(
	const TSharedPtr<FJsonObject>& Args,
	const TCHAR* Field,
	TArray<FString>& OutValue)
{
	OutValue.Reset();
	if (!Args.IsValid())
	{
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* JsonArray = nullptr;
	if (!Args->TryGetArrayField(Field, JsonArray))
	{
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Val : *JsonArray)
	{
		if (Val.IsValid() && Val->Type == EJson::String)
		{
			OutValue.Add(Val->AsString());
		}
	}
	return OutValue.Num() > 0;
}

static bool HCI_TryReadOptionalStringArrayArg(
	const TSharedPtr<FJsonObject>& Args,
	const TCHAR* Field,
	TArray<FString>& OutValue)
{
	OutValue.Reset();
	if (!Args.IsValid() || !Args->HasField(Field))
	{
		return true;
	}
	const TArray<TSharedPtr<FJsonValue>>* JsonArray = nullptr;
	if (!Args->TryGetArrayField(Field, JsonArray))
	{
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Val : *JsonArray)
	{
		if (!Val.IsValid() || Val->Type != EJson::String)
		{
			return false;
		}
		FString Item = Val->AsString();
		Item.TrimStartAndEndInline();
		if (!Item.IsEmpty())
		{
			OutValue.Add(MoveTemp(Item));
		}
	}
	return true;
}

static bool HCI_TryReadRequiredIntArg(
	const TSharedPtr<FJsonObject>& Args,
	const TCHAR* Field,
	int32& OutValue)
{
	if (!Args.IsValid())
	{
		return false;
	}
	return Args->TryGetNumberField(Field, OutValue);
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

class FHCIAbilityKitScanLevelMeshRisksToolAction final : public IHCIAbilityKitAgentToolAction
{
public:
	virtual FName GetToolName() const override
	{
		return TEXT("ScanLevelMeshRisks");
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
		FString Scope;
		TArray<FString> Checks;
		TArray<FString> RequestedActorNames;
		int32 MaxActorCount = 0;
		if (!HCI_TryReadRequiredStringArg(Request.Args, TEXT("scope"), Scope) ||
			!HCI_TryReadRequiredStringArrayArg(Request.Args, TEXT("checks"), Checks) ||
			!HCI_TryReadRequiredIntArg(Request.Args, TEXT("max_actor_count"), MaxActorCount))
		{
			OutResult = FHCIAbilityKitAgentToolActionResult();
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4001");
			OutResult.Reason = TEXT("required_arg_missing");
			return false;
		}
		if (!HCI_TryReadOptionalStringArrayArg(Request.Args, TEXT("actor_names"), RequestedActorNames))
		{
			OutResult = FHCIAbilityKitAgentToolActionResult();
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4002");
			OutResult.Reason = TEXT("invalid_actor_names");
			return false;
		}

		if (Scope != TEXT("selected") && Scope != TEXT("all"))
		{
			OutResult = FHCIAbilityKitAgentToolActionResult();
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4002");
			OutResult.Reason = TEXT("invalid_scope");
			return false;
		}

		UWorld* World = nullptr;
		if (GEditor)
		{
			World = GEditor->GetEditorWorldContext().World();
		}
		if (!World)
		{
			World = GWorld;
		}
		if (!World)
		{
			OutResult = FHCIAbilityKitAgentToolActionResult();
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4205");
			OutResult.Reason = TEXT("no_editor_world");
			return false;
		}

		TArray<AActor*> ActorsToScan;
		if (Scope == TEXT("selected"))
		{
			USelection* SelectedActors = GEditor ? GEditor->GetSelectedActors() : nullptr;
			if (SelectedActors)
			{
				for (FSelectionIterator It(*SelectedActors); It; ++It)
				{
					if (AActor* Actor = Cast<AActor>(*It))
					{
						ActorsToScan.Add(Actor);
					}
				}
			}

			if (ActorsToScan.Num() == 0)
			{
				OutResult = FHCIAbilityKitAgentToolActionResult();
				OutResult.bSucceeded = false;
				OutResult.ErrorCode = TEXT("no_actors_selected");
				OutResult.Reason = TEXT("no_actors_selected");
				OutResult.Evidence.Add(TEXT("scope"), Scope);
				OutResult.Evidence.Add(TEXT("selected_actor_count"), TEXT("0"));
				return false;
			}
		}
		else // "all"
		{
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				ActorsToScan.Add(*It);
			}
		}

		if (RequestedActorNames.Num() > 0)
		{
			TArray<AActor*> FilteredActors;
			FilteredActors.Reserve(ActorsToScan.Num());
			for (AActor* Actor : ActorsToScan)
			{
				if (!Actor)
				{
					continue;
				}

				const FString ActorLabel = Actor->GetActorLabel();
				const FString ActorName = Actor->GetName();
				const FString ActorPath = Actor->GetPathName();
				const bool bMatched = RequestedActorNames.ContainsByPredicate(
					[&ActorLabel, &ActorName, &ActorPath](const FString& RequestedName)
					{
						return ActorLabel.Equals(RequestedName, ESearchCase::IgnoreCase) ||
							ActorName.Equals(RequestedName, ESearchCase::IgnoreCase) ||
							ActorPath.Equals(RequestedName, ESearchCase::IgnoreCase);
					});
				if (bMatched)
				{
					FilteredActors.Add(Actor);
				}
			}
			ActorsToScan = MoveTemp(FilteredActors);
		}

		const bool bCheckMissingCollision = Checks.Contains(TEXT("missing_collision"));
		const bool bCheckDefaultMaterial = Checks.Contains(TEXT("default_material"));

		TArray<FString> RiskyActorNames;
		TArray<FString> RiskyActorPaths;
		TArray<FString> RiskIssueDetails;
		TArray<FString> MissingCollisionActors;
		TArray<FString> DefaultMaterialActors;
		int32 ScannedCount = 0;

		for (AActor* Actor : ActorsToScan)
		{
			if (MaxActorCount > 0 && ScannedCount >= MaxActorCount)
			{
				break;
			}

			TArray<UStaticMeshComponent*> MeshComponents;
			Actor->GetComponents<UStaticMeshComponent>(MeshComponents);
			if (MeshComponents.Num() == 0)
			{
				continue;
			}

			bool bHasValidStaticMeshComponent = false;
			for (UStaticMeshComponent* Comp : MeshComponents)
			{
				if (Comp && Comp->GetStaticMesh())
				{
					bHasValidStaticMeshComponent = true;
					break;
				}
			}
			if (!bHasValidStaticMeshComponent)
			{
				continue;
			}

			// Count only actors that actually have scanable static meshes.
			++ScannedCount;

			bool bHasRisk = false;
			bool bMissingCollisionForActor = false;
			bool bDefaultMaterialForActor = false;
			TArray<FString> ActorRiskTags;
			const FString ActorLabel = Actor->GetActorLabel();

			for (UStaticMeshComponent* SMC : MeshComponents)
			{
				if (!SMC)
				{
					continue;
				}

				UStaticMesh* SM = SMC->GetStaticMesh();
				if (!SM)
				{
					continue;
				}

				if (bCheckMissingCollision && !bMissingCollisionForActor)
				{
					const bool bComponentCollisionDisabled =
						(SMC->GetCollisionEnabled() == ECollisionEnabled::NoCollision) || !SMC->IsCollisionEnabled();

					bool bMeshCollisionInvalid = false;
					UBodySetup* BodySetup = SM->GetBodySetup();
					if (!BodySetup)
					{
						bMeshCollisionInvalid = true;
					}
					else
					{
						const bool bHasPrimitives = BodySetup->AggGeom.GetElementCount() > 0;
						const bool bIsComplexAsSimple =
							BodySetup->CollisionTraceFlag == ECollisionTraceFlag::CTF_UseComplexAsSimple;
						const bool bCollideAll = BodySetup->bMeshCollideAll;
						bMeshCollisionInvalid = !bHasPrimitives && !bIsComplexAsSimple && !bCollideAll;
					}

					if (bComponentCollisionDisabled || bMeshCollisionInvalid)
					{
						bHasRisk = true;
						bMissingCollisionForActor = true;
						ActorRiskTags.Add(TEXT("[MissingCollision]"));
					}
				}

				if (bCheckDefaultMaterial && !bDefaultMaterialForActor)
				{
					bool bHasDefaultMat = false;
					const int32 NumMaterials = SMC->GetNumMaterials();
					if (NumMaterials == 0)
					{
						bHasDefaultMat = true;
					}
					else
					{
						for (int32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
						{
							UMaterialInterface* Mat = SMC->GetMaterial(MaterialIndex);
							if (!Mat ||
								Mat->GetName().Contains(TEXT("Default")) ||
								Mat->GetName().Contains(TEXT("WorldGrid")) ||
								Mat->GetName().Contains(TEXT("BasicShape")) ||
								Mat->GetPathName().StartsWith(TEXT("/Engine/")))
							{
								bHasDefaultMat = true;
								break;
							}
						}
					}

					if (bHasDefaultMat)
					{
						bHasRisk = true;
						bDefaultMaterialForActor = true;
						ActorRiskTags.Add(TEXT("[DefaultMaterial]"));
					}
				}

				if ((!bCheckMissingCollision || bMissingCollisionForActor) &&
					(!bCheckDefaultMaterial || bDefaultMaterialForActor))
				{
					break;
				}
			}

			if (bHasRisk)
			{
				if (bMissingCollisionForActor)
				{
					MissingCollisionActors.Add(ActorLabel);
				}
				if (bDefaultMaterialForActor)
				{
					DefaultMaterialActors.Add(ActorLabel);
				}

				const FString RiskReason =
					ActorRiskTags.Num() > 0 ? FString::Join(ActorRiskTags, TEXT("")) : TEXT("-");
				RiskyActorNames.Add(FString::Printf(TEXT("%s %s"), *ActorLabel, *RiskReason));
				RiskyActorPaths.Add(Actor->GetPathName());
				RiskIssueDetails.Add(RiskReason);
			}
		}

		OutResult = FHCIAbilityKitAgentToolActionResult();
		OutResult.bSucceeded = true;
		OutResult.Reason = TEXT("scan_level_mesh_risks_ok");
		OutResult.EstimatedAffectedCount = RiskyActorNames.Num();

		OutResult.Evidence.Add(TEXT("scope"), Scope);
		OutResult.Evidence.Add(TEXT("requested_actor_count"), FString::FromInt(RequestedActorNames.Num()));
		OutResult.Evidence.Add(TEXT("candidate_actor_count"), FString::FromInt(ActorsToScan.Num()));
		OutResult.Evidence.Add(TEXT("scanned_count"), FString::FromInt(ScannedCount));
		OutResult.Evidence.Add(TEXT("risky_count"), FString::FromInt(RiskyActorNames.Num()));

		FString ReportSummary = FString::Printf(TEXT("Scanned %d actors in world '%s', found %d with risks."), ScannedCount, *World->GetName(), RiskyActorNames.Num());
		OutResult.Evidence.Add(TEXT("risk_summary"), ReportSummary);

		if (RequestedActorNames.Num() > 0)
		{
			OutResult.Evidence.Add(TEXT("actor_names"), FString::Join(RequestedActorNames, TEXT(" | ")));
		}

		FString AffectedActorsStr = RiskyActorNames.Num() > 0 ? FString::Join(RiskyActorNames, TEXT(" | ")) : TEXT("none");
		OutResult.Evidence.Add(TEXT("risky_actors"), AffectedActorsStr);
		OutResult.Evidence.Add(TEXT("actor_path"), RiskyActorPaths.Num() > 0 ? RiskyActorPaths[0] : TEXT("-"));
		OutResult.Evidence.Add(TEXT("issue"), RiskIssueDetails.Num() > 0 ? RiskIssueDetails[0] : TEXT("none"));
		OutResult.Evidence.Add(
			TEXT("evidence"),
			RiskIssueDetails.Num() > 0
				? FString::Printf(TEXT("risk_issues=%s"), *FString::Join(RiskIssueDetails, TEXT(" | ")))
				: TEXT("risk_issues=none"));

		if (MissingCollisionActors.Num() > 0)
		{
			OutResult.Evidence.Add(TEXT("missing_collision_actors"), FString::Join(MissingCollisionActors, TEXT(" | ")));
		}
		else
		{
			OutResult.Evidence.Add(TEXT("missing_collision_actors"), TEXT("none"));
		}

		if (DefaultMaterialActors.Num() > 0)
		{
			OutResult.Evidence.Add(TEXT("default_material_actors"), FString::Join(DefaultMaterialActors, TEXT(" | ")));
		}
		else
		{
			OutResult.Evidence.Add(TEXT("default_material_actors"), TEXT("none"));
		}

		OutResult.Evidence.Add(TEXT("result"), TEXT("scan_level_mesh_risks_ok"));
		return true;
	}
};

class FHCIAbilityKitSetTextureMaxSizeToolAction final : public IHCIAbilityKitAgentToolAction
{
public:
	virtual FName GetToolName() const override
	{
		return TEXT("SetTextureMaxSize");
	}

	virtual bool DryRun(
		const FHCIAbilityKitAgentToolActionRequest& Request,
		FHCIAbilityKitAgentToolActionResult& OutResult) const override
	{
		return RunInternal(Request, OutResult, true);
	}

	virtual bool Execute(
		const FHCIAbilityKitAgentToolActionRequest& Request,
		FHCIAbilityKitAgentToolActionResult& OutResult) const override
	{
		return RunInternal(Request, OutResult, false);
	}

private:
	static bool RunInternal(
		const FHCIAbilityKitAgentToolActionRequest& Request,
		FHCIAbilityKitAgentToolActionResult& OutResult,
		const bool bIsDryRun)
	{
		TArray<FString> AssetPaths;
		int32 MaxSize = 0;
		if (!HCI_TryReadRequiredStringArrayArg(Request.Args, TEXT("asset_paths"), AssetPaths) ||
			!HCI_TryReadRequiredIntArg(Request.Args, TEXT("max_size"), MaxSize))
		{
			OutResult = FHCIAbilityKitAgentToolActionResult();
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4001");
			OutResult.Reason = TEXT("required_arg_missing");
			return false;
		}

		TArray<FString> ModifiedAssets;
		TArray<FString> FailedAssets;

			for (const FString& Path : AssetPaths)
			{
				FString AssetPath;
				FString ObjectPath;
				HCI_NormalizeAssetPathVariants(Path, AssetPath, ObjectPath);

				if (!UEditorAssetLibrary::DoesAssetExist(AssetPath))
				{
					FailedAssets.Add(FString::Printf(TEXT("%s (not_found)"), *AssetPath));
					continue;
				}

				UObject* Asset = LoadObject<UObject>(nullptr, *ObjectPath);
				UTexture2D* Texture = Cast<UTexture2D>(Asset);
				if (!Texture)
				{
					FailedAssets.Add(FString::Printf(TEXT("%s (not_texture2d)"), *AssetPath));
					continue;
				}

			if (Texture->MaxTextureSize == MaxSize)
			{
				continue;
			}

				if (!bIsDryRun)
				{
					Texture->Modify();
					Texture->MaxTextureSize = MaxSize;
					Texture->PostEditChange();
					UEditorAssetLibrary::SaveAsset(AssetPath, false);
				}

				ModifiedAssets.Add(AssetPath);
			}

		OutResult = FHCIAbilityKitAgentToolActionResult();
		OutResult.bSucceeded = FailedAssets.Num() == 0 || ModifiedAssets.Num() > 0;
		OutResult.Reason = bIsDryRun ? TEXT("set_texture_max_size_dry_run_ok") : TEXT("set_texture_max_size_execute_ok");
		OutResult.EstimatedAffectedCount = ModifiedAssets.Num();
		
		OutResult.Evidence.Add(TEXT("target_max_size"), FString::FromInt(MaxSize));
		OutResult.Evidence.Add(TEXT("scanned_count"), FString::FromInt(AssetPaths.Num()));
		OutResult.Evidence.Add(TEXT("modified_count"), FString::FromInt(ModifiedAssets.Num()));
		OutResult.Evidence.Add(TEXT("failed_count"), FString::FromInt(FailedAssets.Num()));
		
		if (ModifiedAssets.Num() > 0)
		{
			OutResult.Evidence.Add(TEXT("modified_assets"), FString::Join(ModifiedAssets, TEXT(" | ")));
		}
		if (FailedAssets.Num() > 0)
		{
			OutResult.Evidence.Add(TEXT("failed_assets"), FString::Join(FailedAssets, TEXT(" | ")));
		}
		
		OutResult.Evidence.Add(TEXT("result"), OutResult.Reason);
		return OutResult.bSucceeded;
	}
};

class FHCIAbilityKitSetMeshLODGroupToolAction final : public IHCIAbilityKitAgentToolAction
{
public:
	virtual FName GetToolName() const override
	{
		return TEXT("SetMeshLODGroup");
	}

	virtual bool DryRun(
		const FHCIAbilityKitAgentToolActionRequest& Request,
		FHCIAbilityKitAgentToolActionResult& OutResult) const override
	{
		return RunInternal(Request, OutResult, true);
	}

	virtual bool Execute(
		const FHCIAbilityKitAgentToolActionRequest& Request,
		FHCIAbilityKitAgentToolActionResult& OutResult) const override
	{
		return RunInternal(Request, OutResult, false);
	}

	private:
	static bool RunInternal(
		const FHCIAbilityKitAgentToolActionRequest& Request,
		FHCIAbilityKitAgentToolActionResult& OutResult,
		const bool bIsDryRun)
	{
		TArray<FString> AssetPaths;
		FString LODGroup;
		if (!HCI_TryReadRequiredStringArrayArg(Request.Args, TEXT("asset_paths"), AssetPaths) ||
			!HCI_TryReadRequiredStringArg(Request.Args, TEXT("lod_group"), LODGroup))
		{
			OutResult = FHCIAbilityKitAgentToolActionResult();
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4001");
			OutResult.Reason = TEXT("required_arg_missing");
			return false;
		}

		TArray<FString> ModifiedAssets;
		TArray<FString> FailedAssets;
		bool bNaniteBlocked = false;

		const FName LODGroupName(*LODGroup);
		struct FHCIAbilityKitPendingLodUpdate
		{
			TObjectPtr<UStaticMesh> Mesh = nullptr;
			FString AssetPath;
		};
		TArray<FHCIAbilityKitPendingLodUpdate> PendingUpdates;

		for (const FString& Path : AssetPaths)
		{
			FString AssetPath;
			FString ObjectPath;
			HCI_NormalizeAssetPathVariants(Path, AssetPath, ObjectPath);

			if (!UEditorAssetLibrary::DoesAssetExist(AssetPath))
			{
				FailedAssets.Add(FString::Printf(TEXT("%s (not_found)"), *AssetPath));
				continue;
			}

			UObject* Asset = LoadObject<UObject>(nullptr, *ObjectPath);
			UStaticMesh* Mesh = Cast<UStaticMesh>(Asset);
			if (!Mesh)
			{
				FailedAssets.Add(FString::Printf(TEXT("%s (not_staticmesh)"), *AssetPath));
				continue;
			}

			if (Mesh->NaniteSettings.bEnabled)
			{
				bNaniteBlocked = true;
				FailedAssets.Add(FString::Printf(TEXT("%s (nanite_enabled_blocked)"), *AssetPath));
				continue;
			}

			if (Mesh->LODGroup == LODGroupName)
			{
				continue;
			}

			FHCIAbilityKitPendingLodUpdate& Update = PendingUpdates.AddDefaulted_GetRef();
			Update.Mesh = Mesh;
			Update.AssetPath = AssetPath;
		}

		if (bNaniteBlocked)
		{
			OutResult = FHCIAbilityKitAgentToolActionResult();
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4010");
			OutResult.Reason = TEXT("lod_tool_nanite_enabled_blocked");
			OutResult.EstimatedAffectedCount = 0;
			OutResult.Evidence.Add(TEXT("target_lod_group"), LODGroup);
			OutResult.Evidence.Add(TEXT("scanned_count"), FString::FromInt(AssetPaths.Num()));
			OutResult.Evidence.Add(TEXT("modified_count"), TEXT("0"));
			OutResult.Evidence.Add(TEXT("failed_count"), FString::FromInt(FailedAssets.Num()));
			OutResult.Evidence.Add(TEXT("failed_assets"), FString::Join(FailedAssets, TEXT(" | ")));
			OutResult.Evidence.Add(TEXT("result"), TEXT("lod_tool_nanite_enabled_blocked"));
			return false;
		}

		for (const FHCIAbilityKitPendingLodUpdate& Update : PendingUpdates)
		{
			if (!Update.Mesh)
			{
				FailedAssets.Add(TEXT("<invalid_mesh_ptr> (null_mesh_object)"));
				continue;
			}

			if (!bIsDryRun)
			{
				Update.Mesh->Modify();
				Update.Mesh->LODGroup = LODGroupName;
				Update.Mesh->PostEditChange();
				UEditorAssetLibrary::SaveAsset(Update.AssetPath, false);
			}

			ModifiedAssets.Add(Update.AssetPath);
		}

		OutResult = FHCIAbilityKitAgentToolActionResult();
		OutResult.bSucceeded = FailedAssets.Num() == 0 || ModifiedAssets.Num() > 0;
		OutResult.Reason = bIsDryRun ? TEXT("set_mesh_lod_group_dry_run_ok") : TEXT("set_mesh_lod_group_execute_ok");
		OutResult.EstimatedAffectedCount = ModifiedAssets.Num();
		
		OutResult.Evidence.Add(TEXT("target_lod_group"), LODGroup);
		OutResult.Evidence.Add(TEXT("scanned_count"), FString::FromInt(AssetPaths.Num()));
		OutResult.Evidence.Add(TEXT("modified_count"), FString::FromInt(ModifiedAssets.Num()));
		OutResult.Evidence.Add(TEXT("failed_count"), FString::FromInt(FailedAssets.Num()));
		
		if (ModifiedAssets.Num() > 0)
		{
			OutResult.Evidence.Add(TEXT("modified_assets"), FString::Join(ModifiedAssets, TEXT(" | ")));
		}
		if (FailedAssets.Num() > 0)
		{
			OutResult.Evidence.Add(TEXT("failed_assets"), FString::Join(FailedAssets, TEXT(" | ")));
		}
		
		OutResult.Evidence.Add(TEXT("result"), OutResult.Reason);
		return OutResult.bSucceeded;
	}
};

class FHCIAbilityKitNormalizeAssetNamingByMetadataToolAction final : public IHCIAbilityKitAgentToolAction
{
public:
	virtual FName GetToolName() const override
	{
		return TEXT("NormalizeAssetNamingByMetadata");
	}

	virtual bool DryRun(
		const FHCIAbilityKitAgentToolActionRequest& Request,
		FHCIAbilityKitAgentToolActionResult& OutResult) const override
	{
		return RunInternal(Request, OutResult, true);
	}

	virtual bool Execute(
		const FHCIAbilityKitAgentToolActionRequest& Request,
		FHCIAbilityKitAgentToolActionResult& OutResult) const override
	{
		return RunInternal(Request, OutResult, false);
	}

private:
	struct FHCINamingProposal
	{
		FString SourceAssetPath;
		FString SourceObjectPath;
		FString SourceAssetName;
		FString SourceDirectory;
		FString DestinationAssetPath;
		FString DestinationObjectPath;
		FString DestinationAssetName;
		FString DestinationDirectory;
		bool bNeedsRename = false;
		bool bNeedsMove = false;
	};

	static bool RunInternal(
		const FHCIAbilityKitAgentToolActionRequest& Request,
		FHCIAbilityKitAgentToolActionResult& OutResult,
		const bool bIsDryRun)
	{
		TArray<FString> AssetPaths;
		FString MetadataSource;
		FString PrefixMode;
		FString TargetRoot;
		if (!HCI_TryReadRequiredStringArrayArg(Request.Args, TEXT("asset_paths"), AssetPaths) ||
			!HCI_TryReadRequiredStringArg(Request.Args, TEXT("metadata_source"), MetadataSource) ||
			!HCI_TryReadRequiredStringArg(Request.Args, TEXT("prefix_mode"), PrefixMode) ||
			!HCI_TryReadRequiredStringArg(Request.Args, TEXT("target_root"), TargetRoot))
		{
			OutResult = FHCIAbilityKitAgentToolActionResult();
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4001");
			OutResult.Reason = TEXT("required_arg_missing");
			return false;
		}

		if (!PrefixMode.Equals(TEXT("auto_by_asset_class"), ESearchCase::CaseSensitive))
		{
			OutResult = FHCIAbilityKitAgentToolActionResult();
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4011");
			OutResult.Reason = TEXT("prefix_mode_not_supported");
			return false;
		}

		TargetRoot = HCI_TrimTrailingSlash(TargetRoot);
		if (TargetRoot.IsEmpty() || !TargetRoot.StartsWith(TEXT("/Game/")))
		{
			OutResult = FHCIAbilityKitAgentToolActionResult();
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4009");
			OutResult.Reason = TEXT("target_root_must_start_with_game");
			return false;
		}

		TArray<FHCINamingProposal> Proposals;
		TSet<FString> ReservedDestinationObjects;
		TArray<FString> ProposedRenameRows;
		TArray<FString> ProposedMoveRows;
		TArray<FString> FailedRows;

		for (const FString& RawPath : AssetPaths)
		{
			FString SourceAssetPath;
			FString SourceObjectPath;
			HCI_NormalizeAssetPathVariants(RawPath, SourceAssetPath, SourceObjectPath);

			if (!UEditorAssetLibrary::DoesAssetExist(SourceAssetPath))
			{
				FailedRows.Add(FString::Printf(TEXT("%s (not_found)"), *SourceAssetPath));
				continue;
			}

			FString SourcePackagePath;
			FString SourceAssetName;
			if (!HCI_TrySplitObjectPath(SourceObjectPath, SourcePackagePath, SourceAssetName))
			{
				FailedRows.Add(FString::Printf(TEXT("%s (invalid_object_path)"), *SourceObjectPath));
				continue;
			}

			UObject* Asset = LoadObject<UObject>(nullptr, *SourceObjectPath);
			if (Asset == nullptr)
			{
				FailedRows.Add(FString::Printf(TEXT("%s (load_failed)"), *SourceObjectPath));
				continue;
			}

			const FString Prefix = HCI_DeriveClassPrefix(Asset);
			FString BaseName;
			FString FailureReason;
			if (!HCI_TryBuildNormalizedNameBase(Asset, MetadataSource, Prefix, SourceAssetName, BaseName, FailureReason))
			{
				FailedRows.Add(FString::Printf(TEXT("%s (%s)"), *SourceObjectPath, *FailureReason));
				continue;
			}

			FString TargetAssetName = FString::Printf(TEXT("%s_%s"), *Prefix, *BaseName);
			TargetAssetName = HCI_SanitizeIdentifier(TargetAssetName);
			if (TargetAssetName.IsEmpty())
			{
				FailedRows.Add(FString::Printf(TEXT("%s (target_name_invalid)"), *SourceObjectPath));
				continue;
			}

			const FString DestinationAssetPath = FString::Printf(TEXT("%s/%s"), *TargetRoot, *TargetAssetName);
			const FString DestinationObjectPath = HCI_ToObjectPath(DestinationAssetPath, TargetAssetName);
			if (DestinationObjectPath.Equals(SourceObjectPath, ESearchCase::CaseSensitive))
			{
				continue;
			}

			if (ReservedDestinationObjects.Contains(DestinationObjectPath))
			{
				FailedRows.Add(FString::Printf(TEXT("%s (duplicate_destination)"), *SourceObjectPath));
				continue;
			}

			const bool bDestinationExists = UEditorAssetLibrary::DoesAssetExist(DestinationAssetPath);
			if (bDestinationExists && !DestinationAssetPath.Equals(SourceAssetPath, ESearchCase::CaseSensitive))
			{
				FailedRows.Add(FString::Printf(TEXT("%s (destination_exists:%s)"), *SourceObjectPath, *DestinationObjectPath));
				continue;
			}

			FHCINamingProposal& Proposal = Proposals.AddDefaulted_GetRef();
			Proposal.SourceAssetPath = SourceAssetPath;
			Proposal.SourceObjectPath = SourceObjectPath;
			Proposal.SourceAssetName = SourceAssetName;
			Proposal.SourceDirectory = HCI_GetDirectoryFromPackagePath(SourcePackagePath);
			Proposal.DestinationAssetPath = DestinationAssetPath;
			Proposal.DestinationObjectPath = DestinationObjectPath;
			Proposal.DestinationAssetName = TargetAssetName;
			Proposal.DestinationDirectory = TargetRoot;
			Proposal.bNeedsRename = !SourceAssetName.Equals(TargetAssetName, ESearchCase::CaseSensitive);
			Proposal.bNeedsMove = !Proposal.SourceDirectory.Equals(TargetRoot, ESearchCase::CaseSensitive);
			ReservedDestinationObjects.Add(DestinationObjectPath);

			if (Proposal.bNeedsRename)
			{
				ProposedRenameRows.Add(FString::Printf(TEXT("%s -> %s"), *Proposal.SourceAssetName, *Proposal.DestinationAssetName));
			}
			if (Proposal.bNeedsMove)
			{
				ProposedMoveRows.Add(FString::Printf(TEXT("%s -> %s"), *Proposal.SourceDirectory, *Proposal.DestinationDirectory));
			}
		}

		int32 AppliedCount = 0;
		if (!bIsDryRun && Proposals.Num() > 0)
		{
			UEditorAssetLibrary::MakeDirectory(TargetRoot);

			for (const FHCINamingProposal& Proposal : Proposals)
			{
				if (!HCI_MoveAssetWithAssetTools(Proposal.SourceObjectPath, Proposal.DestinationObjectPath))
				{
					FailedRows.Add(FString::Printf(TEXT("%s (rename_move_failed)"), *Proposal.SourceObjectPath));
					continue;
				}
				++AppliedCount;
			}
		}

		OutResult = FHCIAbilityKitAgentToolActionResult();
		const int32 AffectedCount = bIsDryRun ? Proposals.Num() : AppliedCount;
		const bool bHasSuccess = AffectedCount > 0;
		const bool bAllFailed = !bHasSuccess && FailedRows.Num() > 0;
		OutResult.bSucceeded = !bAllFailed;
		OutResult.ErrorCode = bAllFailed ? TEXT("E4012") : FString();
		OutResult.Reason = bIsDryRun
			? (bAllFailed ? TEXT("normalize_asset_naming_by_metadata_dry_run_failed") : TEXT("normalize_asset_naming_by_metadata_dry_run_ok"))
			: (bAllFailed ? TEXT("normalize_asset_naming_by_metadata_execute_failed") : TEXT("normalize_asset_naming_by_metadata_execute_ok"));
		OutResult.EstimatedAffectedCount = AffectedCount;

		OutResult.Evidence.Add(TEXT("proposed_renames"), ProposedRenameRows.Num() > 0 ? FString::Join(ProposedRenameRows, TEXT(" | ")) : TEXT("none"));
		OutResult.Evidence.Add(TEXT("proposed_moves"), ProposedMoveRows.Num() > 0 ? FString::Join(ProposedMoveRows, TEXT(" | ")) : TEXT("none"));
		OutResult.Evidence.Add(TEXT("affected_count"), FString::FromInt(AffectedCount));
		OutResult.Evidence.Add(TEXT("result"), OutResult.Reason);

		if (FailedRows.Num() > 0)
		{
			OutResult.Evidence.Add(TEXT("failed_assets"), FString::Join(FailedRows, TEXT(" | ")));
		}

		return OutResult.bSucceeded;
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
	OutActions.Add(TEXT("ScanLevelMeshRisks"), MakeShared<FHCIAbilityKitScanLevelMeshRisksToolAction>());
	OutActions.Add(TEXT("SetTextureMaxSize"), MakeShared<FHCIAbilityKitSetTextureMaxSizeToolAction>());
	OutActions.Add(TEXT("SetMeshLODGroup"), MakeShared<FHCIAbilityKitSetMeshLODGroupToolAction>());
	OutActions.Add(TEXT("NormalizeAssetNamingByMetadata"), MakeShared<FHCIAbilityKitNormalizeAssetNamingByMetadataToolAction>());
	OutActions.Add(TEXT("RenameAsset"), MakeShared<FHCIAbilityKitRenameAssetToolAction>());
	OutActions.Add(TEXT("MoveAsset"), MakeShared<FHCIAbilityKitMoveAssetToolAction>());
}
